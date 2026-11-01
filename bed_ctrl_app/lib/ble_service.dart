import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

enum BleLinkState { disconnected, connecting, connected }

/// Liaison Bluetooth LE avec le module BedCtrl : commandes du lit et
/// configuration. UUID partagés avec esp32/BedCtrl/globals.h.
///   - COMMAND (write)       : commande JSON du lit, ou {"cmd": "wifi_ap"|"restart"}
///   - CONFIG  (read/write)  : config du module en JSON
///   - STATUS  (notify)      : {"ok": bool, "message": "..."} après CONFIG / cmd
///
/// La connexion est maintenue : si le module redémarre ou s'éloigne, on
/// retente toutes les [_retryDelay] tant que [start] est actif.
class BleService {
  static final Guid serviceUuid = Guid('6e1d0001-8b4f-4c1a-9a3e-b3d1c0ffee01');
  static final Guid configUuid = Guid('6e1d0002-8b4f-4c1a-9a3e-b3d1c0ffee01');
  static final Guid statusUuid = Guid('6e1d0003-8b4f-4c1a-9a3e-b3d1c0ffee01');
  static final Guid commandUuid = Guid('6e1d0004-8b4f-4c1a-9a3e-b3d1c0ffee01');

  static const _retryDelay = Duration(seconds: 3);
  static const _statusTimeout = Duration(seconds: 5);

  BleService(this.device);

  final BluetoothDevice device;
  BluetoothCharacteristic? _command;
  BluetoothCharacteristic? _config;
  BluetoothCharacteristic? _status;

  BleLinkState state = BleLinkState.disconnected;
  String lastError = '';
  Function(BleLinkState)? onStateChanged;

  bool _wanted = false; // connexion souhaitée (start) ou non (stop)
  Timer? _retry;
  StreamSubscription<BluetoothConnectionState>? _connSub;

  // ── Scan des modules ───────────────────────────────────────────────────
  static Stream<List<ScanResult>> get scanResults => FlutterBluePlus.scanResults;
  static Stream<bool> get isScanning => FlutterBluePlus.isScanning;

  static Future<void> startScan() async {
    if (!await FlutterBluePlus.isSupported) {
      throw const BleException('Bluetooth LE non supporté');
    }
    if (FlutterBluePlus.adapterStateNow != BluetoothAdapterState.on) {
      await FlutterBluePlus.turnOn(); // Android : demande d'activation
    }
    await FlutterBluePlus.startScan(
      withServices: [serviceUuid],
      timeout: const Duration(seconds: 10),
    );
  }

  static Future<void> stopScan() => FlutterBluePlus.stopScan();

  // ── Connexion maintenue ────────────────────────────────────────────────
  void start() {
    _wanted = true;
    _connSub ??= device.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected &&
          state == BleLinkState.connected) {
        _clearCharacteristics();
        _setState(BleLinkState.disconnected);
        _scheduleRetry();
      }
    });
    _tryConnect();
  }

  Future<void> stop() async {
    _wanted = false;
    _retry?.cancel();
    await _connSub?.cancel();
    _connSub = null;
    _clearCharacteristics();
    await device.disconnect();
    _setState(BleLinkState.disconnected);
  }

  Future<void> _tryConnect() async {
    if (!_wanted || state != BleLinkState.disconnected) return;
    _setState(BleLinkState.connecting);
    try {
      await device.connect(timeout: const Duration(seconds: 10)); // MTU 512
      final services = await device.discoverServices();
      for (final s in services.where((s) => s.uuid == serviceUuid)) {
        for (final c in s.characteristics) {
          if (c.uuid == commandUuid) _command = c;
          if (c.uuid == configUuid) _config = c;
          if (c.uuid == statusUuid) _status = c;
        }
      }
      if (_command == null || _config == null || _status == null) {
        throw const BleException('service BedCtrl introuvable (firmware ?)');
      }
      await _status!.setNotifyValue(true);
      lastError = '';
      _setState(BleLinkState.connected);
    } catch (e) {
      lastError = e.toString();
      _clearCharacteristics();
      await device.disconnect();
      _setState(BleLinkState.disconnected);
      _scheduleRetry();
    }
  }

  void _scheduleRetry() {
    _retry?.cancel();
    if (_wanted) _retry = Timer(_retryDelay, _tryConnect);
  }

  void _clearCharacteristics() {
    _command = null;
    _config = null;
    _status = null;
  }

  void _setState(BleLinkState s) {
    state = s;
    onStateChanged?.call(s);
  }

  bool get isConnected => state == BleLinkState.connected;

  // ── Commandes du lit ───────────────────────────────────────────────────
  /// Envoie une commande ; ignorée si la liaison est coupée.
  Future<void> sendCommand(Map<String, dynamic> command) async {
    final c = _command;
    if (c == null) return;
    try {
      // Avec réponse : un "off" (relâchement) ne doit pas se perdre
      await c.write(utf8.encode(jsonEncode(command)));
    } catch (e) {
      debugPrint('BLE command failed: $e');
    }
  }

  // ── Configuration et commandes système ─────────────────────────────────
  Future<Map<String, dynamic>> readConfig() async {
    final raw = await _requireChar(_config).read();
    return jsonDecode(utf8.decode(raw)) as Map<String, dynamic>;
  }

  /// Envoie [values] au module, qui enregistre puis redémarre.
  /// Renvoie son message ; lève [BleException] si refusé.
  Future<String> writeConfig(Map<String, dynamic> values) =>
      _request(_config, values, allowLongWrite: true);

  /// Commande système : 'wifi_ap' (portail web) ou 'restart'.
  Future<String> sendSystem(String cmd) => _request(_command, {'cmd': cmd});

  Future<String> _request(
      BluetoothCharacteristic? target, Map<String, dynamic> payload,
      {bool allowLongWrite = false}) async {
    final c = _requireChar(target);
    // S'abonner avant d'écrire : la notification peut arriver très vite
    final reply = _requireChar(_status).onValueReceived.first.timeout(
        _statusTimeout,
        onTimeout: () => throw const BleException('pas de réponse du module'));
    await c.write(utf8.encode(jsonEncode(payload)),
        allowLongWrite: allowLongWrite);
    final status = jsonDecode(utf8.decode(await reply)) as Map<String, dynamic>;
    final message = status['message'] as String? ?? '';
    if (status['ok'] != true) {
      throw BleException(message);
    }
    return message;
  }

  BluetoothCharacteristic _requireChar(BluetoothCharacteristic? c) {
    if (c == null) throw const BleException('module non connecté');
    return c;
  }
}

class BleException implements Exception {
  const BleException(this.message);
  final String message;

  @override
  String toString() => message;
}

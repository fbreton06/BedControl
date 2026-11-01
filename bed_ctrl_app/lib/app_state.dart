import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'ble_service.dart';

class AppState extends ChangeNotifier {
  AppState() {
    _load();
  }

  // ── Module BedCtrl choisi (identifiant Bluetooth mémorisé) ────────────
  String deviceId = '';
  String deviceName = '';

  /// Seuil (en secondes) au-delà duquel un appui M1/M2 est considéré "long".
  int longPressSeconds = 2;

  /// Thème sombre (true) ou clair (false).
  bool isDark = false;

  // ── Liaison BLE ───────────────────────────────────────────────────────
  BleService? _ble;
  BleLinkState connectionState = BleLinkState.disconnected;
  String statusMessage = 'Aucun module choisi';

  // ── État des sélecteurs (aucun envoi) ─────────────────────────────────
  String beds = 'both'; // 'left' | 'both' | 'right'
  String parts = 'both'; // 'head' | 'both' | 'leg'

  bool get hasDevice => deviceId.isNotEmpty;
  bool get isConnected => connectionState == BleLinkState.connected;

  /// Liaison courante, pour l'écran de configuration du module.
  BleService? get ble => _ble;

  // ── Connexion ──────────────────────────────────────────────────────────
  Future<void> connect() async {
    await _ble?.stop();
    if (!hasDevice) {
      _ble = null;
      statusMessage = 'Aucun module choisi';
      notifyListeners();
      return;
    }
    final service = BleService(BluetoothDevice.fromId(deviceId));
    service.onStateChanged = (state) {
      connectionState = state;
      switch (state) {
        case BleLinkState.connected:
          statusMessage = 'Connecté à $deviceName';
          break;
        case BleLinkState.connecting:
          statusMessage = 'Connexion en cours...';
          break;
        case BleLinkState.disconnected:
          statusMessage = service.lastError.isEmpty
              ? 'Déconnecté'
              : 'Erreur : ${service.lastError}';
          break;
      }
      notifyListeners();
    };
    _ble = service;
    service.start();
  }

  Future<void> disconnect() async {
    await _ble?.stop();
    notifyListeners();
  }

  /// Module choisi dans la liste du scan : mémorisé puis connecté.
  Future<void> selectDevice(String id, String name) async {
    deviceId = id;
    deviceName = name;
    await _save();
    await connect();
  }

  /// Nom affiché après un changement de hostname (l'identifiant ne change pas).
  void renameDevice(String name) {
    deviceName = name;
    _save();
    notifyListeners();
  }

  // ── Sélecteurs d'état (pas d'envoi) ───────────────────────────────────
  void setBeds(String value) {
    beds = value;
    notifyListeners();
  }

  void setParts(String value) {
    parts = value;
    notifyListeners();
  }

  void setDarkMode(bool value) {
    isDark = value;
    _save();
    notifyListeners();
  }

  // ── Envoi d'une commande de bouton ────────────────────────────────────
  /// [source] : 'up','down','same','flat','light','m1','m2'
  /// [push]   : 'on'/'off' (maintien) ou 'short'/'long' (mémoire)
  void sendCommand(String source, String push) {
    _ble?.sendCommand({
      'source': source,
      'beds': beds,
      'parts': parts,
      'push': push,
    });
  }

  // ── Paramètres ──────────────────────────────────────────────────────────
  void updateSettings({required int longPress}) {
    longPressSeconds = longPress;
    _save();
    notifyListeners();
  }

  // ── Persistance (shared_preferences) ──────────────────────────────────
  Future<void> _load() async {
    final p = await SharedPreferences.getInstance();
    deviceId = p.getString('deviceId') ?? deviceId;
    deviceName = p.getString('deviceName') ?? deviceName;
    longPressSeconds = p.getInt('longPressSeconds') ?? longPressSeconds;
    isDark = p.getBool('isDark') ?? isDark;
    notifyListeners();
    // Connexion automatique dès qu'un module est mémorisé.
    if (hasDevice) {
      connect();
    }
  }

  Future<void> _save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString('deviceId', deviceId);
    await p.setString('deviceName', deviceName);
    await p.setInt('longPressSeconds', longPressSeconds);
    await p.setBool('isDark', isDark);
  }
}

import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'mqtt_service.dart';

class AppState extends ChangeNotifier {
  AppState() {
    _load();
  }

  // ── Configuration broker ──────────────────────────────────────────────
  String brokerHost = '';
  int brokerPort = 1883;
  String clientId = 'flutter_controller_01';
  String username = '';
  String password = '';

  /// Seuil (en secondes) au-delà duquel un appui M1/M2 est considéré "long".
  int longPressSeconds = 2;

  /// Thème sombre (true) ou clair (false).
  bool isDark = false;

  /// Topic sur lequel sont publiées les commandes du lit.
  static const String commandTopic = 'bedctrl';

  // ── Service MQTT ──────────────────────────────────────────────────────
  MqttService? _mqtt;
  AppMqttConnectionState connectionState = AppMqttConnectionState.disconnected;
  String statusMessage = 'Non connecté';

  // ── État des sélecteurs (aucun message MQTT) ──────────────────────────
  String beds = 'both'; // 'left' | 'both' | 'right'
  String parts = 'both'; // 'head' | 'both' | 'leg'

  // ── Création du service avec la config courante ───────────────────────
  MqttService _createService() {
    final service = MqttService(
      broker: brokerHost,
      port: brokerPort,
      clientId: clientId,
      username: username.isEmpty ? null : username,
      password: password.isEmpty ? null : password,
    );
    service.onConnectionStateChanged = (state) {
      connectionState = state;
      switch (state) {
        case AppMqttConnectionState.connected:
          statusMessage = 'Connecté à $brokerHost';
          break;
        case AppMqttConnectionState.connecting:
          statusMessage = 'Connexion en cours...';
          break;
        case AppMqttConnectionState.disconnected:
          statusMessage = 'Déconnecté';
          break;
        case AppMqttConnectionState.error:
          statusMessage = 'Erreur : ${service.lastError}';
          break;
      }
      notifyListeners();
    };
    return service;
  }

  // ── Connexion ──────────────────────────────────────────────────────────
  Future<void> connect() async {
    _mqtt?.disconnect();
    _mqtt = _createService();
    await _mqtt!.connect();
    notifyListeners();
  }

  void disconnect() {
    _mqtt?.disconnect();
    notifyListeners();
  }

  // ── Sélecteurs d'état (pas de publication) ────────────────────────────
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
    final payload = jsonEncode({
      'source': source,
      'beds': beds,
      'parts': parts,
      'push': push,
    });
    _mqtt?.publish(commandTopic, payload);
  }

  // ── Paramètres ──────────────────────────────────────────────────────────
  void updateBrokerSettings({
    required String host,
    required int port,
    required String id,
    required String user,
    required String pass,
    required int longPress,
  }) {
    brokerHost = host;
    brokerPort = port;
    clientId = id;
    username = user;
    password = pass;
    longPressSeconds = longPress;
    _save();
    // Déconnecter si connecté — l'utilisateur devra reconnecter
    if (_mqtt?.isConnected == true) {
      _mqtt!.disconnect();
    }
    notifyListeners();
  }

  bool get isConnected => _mqtt?.isConnected ?? false;

  // ── Persistance (shared_preferences) ──────────────────────────────────
  Future<void> _load() async {
    final p = await SharedPreferences.getInstance();
    brokerHost = p.getString('brokerHost') ?? brokerHost;
    brokerPort = p.getInt('brokerPort') ?? brokerPort;
    clientId = p.getString('clientId') ?? clientId;
    username = p.getString('username') ?? username;
    password = p.getString('password') ?? password;
    longPressSeconds = p.getInt('longPressSeconds') ?? longPressSeconds;
    isDark = p.getBool('isDark') ?? isDark;
    notifyListeners();
    // Connexion automatique dès qu'un broker est configuré.
    if (brokerHost.isNotEmpty) {
      connect();
    }
  }

  Future<void> _save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString('brokerHost', brokerHost);
    await p.setInt('brokerPort', brokerPort);
    await p.setString('clientId', clientId);
    await p.setString('username', username);
    await p.setString('password', password);
    await p.setInt('longPressSeconds', longPressSeconds);
    await p.setBool('isDark', isDark);
  }
}

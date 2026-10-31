import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

enum AppMqttConnectionState { disconnected, connecting, connected, error }

class MqttService {
  MqttServerClient? _client;

  final String broker;
  final int port;
  final String clientId;
  final String? username;
  final String? password;

  AppMqttConnectionState connectionState = AppMqttConnectionState.disconnected;
  String lastError = '';

  Function(AppMqttConnectionState)? onConnectionStateChanged;

  MqttService({
    required this.broker,
    this.port = 1883,
    required this.clientId,
    this.username,
    this.password,
  });

  Future<bool> connect() async {
    _client = MqttServerClient(broker, clientId);
    _client!.port = port;
    _client!.keepAlivePeriod = 30;
    _client!.logging(on: false);

    _client!.onConnected = _onConnected;
    _client!.onDisconnected = _onDisconnected;
    _client!.onAutoReconnect = _onAutoReconnect;

    _client!.autoReconnect = true;

    final connMessage = MqttConnectMessage()
        .withClientIdentifier(clientId)
        .withWillQos(MqttQos.atLeastOnce)
        .startClean();

    if (username != null && username!.isNotEmpty) {
      connMessage.authenticateAs(username!, password ?? '');
    }

    _client!.connectionMessage = connMessage;

    _setConnectionState(AppMqttConnectionState.connecting);

    try {
      await _client!.connect();
    } catch (e) {
      lastError = e.toString();
      _setConnectionState(AppMqttConnectionState.error);
      _client!.disconnect();
      return false;
    }

    if (_client!.connectionStatus!.state == MqttConnectionState.connected) {
      return true;
    } else {
      lastError = 'Connexion refusée : ${_client!.connectionStatus!.returnCode}';
      _setConnectionState(AppMqttConnectionState.error);
      return false;
    }
  }

  void disconnect() {
    _client?.disconnect();
  }

  /// Publie un message sur un topic
  bool publish(String topic, String message, {bool retain = false}) {
    if (_client == null ||
        _client!.connectionStatus!.state != MqttConnectionState.connected) {
      return false;
    }

    final builder = MqttClientPayloadBuilder();
    builder.addString(message);
    _client!.publishMessage(
      topic,
      MqttQos.atLeastOnce,
      builder.payload!,
      retain: retain,
    );
    return true;
  }

  void _onConnected() {
    _setConnectionState(AppMqttConnectionState.connected);
  }

  void _onDisconnected() {
    _setConnectionState(AppMqttConnectionState.disconnected);
  }

  void _onAutoReconnect() {
    _setConnectionState(AppMqttConnectionState.connecting);
  }

  void _setConnectionState(AppMqttConnectionState state) {
    connectionState = state;
    onConnectionStateChanged?.call(state);
  }

  bool get isConnected =>
      _client?.connectionStatus?.state == MqttConnectionState.connected;
}

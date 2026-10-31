import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late TextEditingController _hostCtrl;
  late TextEditingController _portCtrl;
  late TextEditingController _clientIdCtrl;
  late TextEditingController _userCtrl;
  late TextEditingController _passCtrl;
  late TextEditingController _longPressCtrl;
  bool _obscurePass = true;

  @override
  void initState() {
    super.initState();
    final state = context.read<AppState>();
    _hostCtrl = TextEditingController(text: state.brokerHost);
    _portCtrl = TextEditingController(text: state.brokerPort.toString());
    _clientIdCtrl = TextEditingController(text: state.clientId);
    _userCtrl = TextEditingController(text: state.username);
    _passCtrl = TextEditingController(text: state.password);
    _longPressCtrl =
        TextEditingController(text: state.longPressSeconds.toString());
  }

  @override
  void dispose() {
    _hostCtrl.dispose();
    _portCtrl.dispose();
    _clientIdCtrl.dispose();
    _userCtrl.dispose();
    _passCtrl.dispose();
    _longPressCtrl.dispose();
    super.dispose();
  }

  void _save() {
    final port = int.tryParse(_portCtrl.text) ?? 1883;
    final longPress = int.tryParse(_longPressCtrl.text) ?? 5;
    context.read<AppState>().updateBrokerSettings(
          host: _hostCtrl.text.trim(),
          port: port,
          id: _clientIdCtrl.text.trim(),
          user: _userCtrl.text.trim(),
          pass: _passCtrl.text,
          longPress: longPress,
        );
    Navigator.pop(context);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Configuration MQTT'),
        backgroundColor: Colors.black87,
        foregroundColor: Colors.white,
        actions: [
          IconButton(
            icon: const Icon(Icons.check),
            onPressed: _save,
            tooltip: 'Enregistrer',
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _section('Broker'),
          _field(_hostCtrl, 'Adresse IP / Hostname', Icons.router,
              hint: '192.168.1.100'),
          _field(_portCtrl, 'Port', Icons.numbers,
              hint: '1883', keyboardType: TextInputType.number),
          _field(_clientIdCtrl, 'Client ID', Icons.badge,
              hint: 'flutter_controller_01'),
          const SizedBox(height: 16),
          _section('Authentification (optionnel)'),
          _field(_userCtrl, 'Nom d\'utilisateur', Icons.person),
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: TextField(
              controller: _passCtrl,
              obscureText: _obscurePass,
              decoration: InputDecoration(
                labelText: 'Mot de passe',
                prefixIcon: const Icon(Icons.lock),
                suffixIcon: IconButton(
                  icon: Icon(_obscurePass
                      ? Icons.visibility
                      : Icons.visibility_off),
                  tooltip: _obscurePass ? 'Afficher' : 'Masquer',
                  onPressed: () =>
                      setState(() => _obscurePass = !_obscurePass),
                ),
                border: const OutlineInputBorder(),
              ),
            ),
          ),
          const SizedBox(height: 16),
          _section('Commandes'),
          _field(_longPressCtrl, 'Seuil appui long (secondes)', Icons.timer,
              hint: '2', keyboardType: TextInputType.number),
          const SizedBox(height: 16),
          _section('Apparence'),
          SwitchListTile(
            title: const Text('Thème sombre'),
            secondary: const Icon(Icons.dark_mode),
            value: context.watch<AppState>().isDark,
            onChanged: context.read<AppState>().setDarkMode,
          ),
          const SizedBox(height: 24),
          FilledButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save),
            label: const Text('Enregistrer et fermer'),
            style: FilledButton.styleFrom(
              backgroundColor: Colors.blue,
              minimumSize: const Size.fromHeight(48),
            ),
          ),
        ],
      ),
    );
  }

  Widget _section(String title) => Padding(
        padding: const EdgeInsets.only(bottom: 8, top: 8),
        child: Text(title,
            style: const TextStyle(
                fontSize: 12,
                fontWeight: FontWeight.w600,
                color: Colors.grey,
                letterSpacing: 1.1)),
      );

  Widget _field(
    TextEditingController ctrl,
    String label,
    IconData icon, {
    String hint = '',
    bool obscure = false,
    TextInputType keyboardType = TextInputType.text,
  }) =>
      Padding(
        padding: const EdgeInsets.only(bottom: 12),
        child: TextField(
          controller: ctrl,
          obscureText: obscure,
          keyboardType: keyboardType,
          decoration: InputDecoration(
            labelText: label,
            hintText: hint,
            prefixIcon: Icon(icon),
            border: const OutlineInputBorder(),
          ),
        ),
      );
}

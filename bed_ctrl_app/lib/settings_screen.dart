import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';
import 'device_scan_screen.dart';
import 'module_config_screen.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late TextEditingController _longPressCtrl;

  @override
  void initState() {
    super.initState();
    final state = context.read<AppState>();
    _longPressCtrl =
        TextEditingController(text: state.longPressSeconds.toString());
  }

  @override
  void dispose() {
    _longPressCtrl.dispose();
    super.dispose();
  }

  void _save() {
    final longPress = int.tryParse(_longPressCtrl.text) ?? 2;
    context.read<AppState>().updateSettings(longPress: longPress);
    Navigator.pop(context);
  }

  void _open(Widget screen) => Navigator.push(
        context,
        MaterialPageRoute(builder: (_) => screen),
      );

  @override
  Widget build(BuildContext context) {
    final state = context.watch<AppState>();
    return Scaffold(
      appBar: AppBar(
        title: const Text('Paramètres'),
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
          _section('Module BedCtrl (Bluetooth)'),
          ListTile(
            leading: const Icon(Icons.bluetooth),
            title: Text(state.hasDevice ? state.deviceName : 'Aucun module'),
            subtitle: Text(state.statusMessage),
            trailing: const Icon(Icons.chevron_right),
            onTap: () => _open(const DeviceScanScreen()),
          ),
          ListTile(
            leading: const Icon(Icons.tune),
            title: const Text('Configuration du module'),
            subtitle: const Text('WiFi, nom, lumière, portail web'),
            trailing: const Icon(Icons.chevron_right),
            enabled: state.isConnected,
            onTap: () => _open(const ModuleConfigScreen()),
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
            value: state.isDark,
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
    TextInputType keyboardType = TextInputType.text,
  }) =>
      Padding(
        padding: const EdgeInsets.only(bottom: 12),
        child: TextField(
          controller: ctrl,
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

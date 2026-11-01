import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';
import 'ble_service.dart';

/// Configuration du module BedCtrl via la liaison Bluetooth courante :
/// WiFi du portail web, hostname, réglages, mode AP et redémarrage.
class ModuleConfigScreen extends StatefulWidget {
  const ModuleConfigScreen({super.key});

  @override
  State<ModuleConfigScreen> createState() => _ModuleConfigScreenState();
}

class _ModuleConfigScreenState extends State<ModuleConfigScreen> {
  final _formKey = GlobalKey<FormState>();
  final _ssidCtrl = TextEditingController();
  final _wifiPassCtrl = TextEditingController();
  final _hostnameCtrl = TextEditingController();
  final _lightDelayCtrl = TextEditingController();
  final _wdtCtrl = TextEditingController();
  bool _obscureWifi = true;

  Map<String, dynamic>? _info; // config lue (version, mode WiFi, IP…)
  bool _busy = true;
  String? _error;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void dispose() {
    for (final c in [
      _ssidCtrl,
      _wifiPassCtrl,
      _hostnameCtrl,
      _lightDelayCtrl,
      _wdtCtrl,
    ]) {
      c.dispose();
    }
    super.dispose();
  }

  BleService? get _ble => context.read<AppState>().ble;

  // ── Actions ────────────────────────────────────────────────────────────
  Future<void> _load() async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final cfg = await _ble!.readConfig();
      _info = cfg;
      _ssidCtrl.text = cfg['wifi_ssid']?.toString() ?? '';
      _hostnameCtrl.text = cfg['hostname']?.toString() ?? '';
      _lightDelayCtrl.text = cfg['light_delay']?.toString() ?? '';
      _wdtCtrl.text = cfg['wdt_s']?.toString() ?? '';
    } catch (e) {
      _error = 'Lecture impossible : $e';
    }
    if (mounted) setState(() => _busy = false);
  }

  Future<void> _save() async {
    if (!_formKey.currentState!.validate()) return;
    final hostname = _hostnameCtrl.text.trim();
    final values = <String, dynamic>{
      if (_ssidCtrl.text.trim().isNotEmpty) 'wifi_ssid': _ssidCtrl.text.trim(),
      // Mot de passe : envoyé seulement s'il est saisi (sinon inchangé)
      if (_wifiPassCtrl.text.isNotEmpty) 'wifi_pass': _wifiPassCtrl.text,
      'hostname': hostname,
      'light_delay': int.parse(_lightDelayCtrl.text),
      'wdt_s': int.parse(_wdtCtrl.text),
    };
    final state = context.read<AppState>();
    await _run(() => _ble!.writeConfig(values), then: () {
      if (hostname.isNotEmpty) state.renameDevice(hostname);
    });
  }

  Future<void> _system(String cmd) =>
      _run(() => _ble!.sendSystem(cmd));

  /// Exécute [action], affiche le message du module et ferme l'écran.
  Future<void> _run(Future<String> Function() action,
      {VoidCallback? then}) async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      final message = await action();
      then?.call();
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text(message),
        behavior: SnackBarBehavior.floating,
        duration: const Duration(seconds: 6),
      ));
      Navigator.pop(context);
    } catch (e) {
      if (mounted) {
        setState(() {
          _busy = false;
          _error = 'Refusé : $e';
        });
      }
    }
  }

  Future<void> _confirm(String question, String cmd) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        content: Text(question),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: const Text('Annuler')),
          FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: const Text('Confirmer')),
        ],
      ),
    );
    if (ok == true) await _system(cmd);
  }

  // ── UI ─────────────────────────────────────────────────────────────────
  @override
  Widget build(BuildContext context) {
    final connected = context.watch<AppState>().isConnected;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Configuration du module'),
        backgroundColor: Colors.black87,
        foregroundColor: Colors.white,
      ),
      body: !connected
          ? const Center(child: Text('Module non connecté'))
          : _busy
              ? const Center(child: CircularProgressIndicator())
              : _buildForm(),
    );
  }

  Widget _buildForm() {
    final info = _info ?? const {};
    final mode = switch (info['wifi_mode']) {
      'sta' => 'connecté (${info['ip']})',
      'ap' => 'portail AP actif (${info['ip']})',
      _ => 'coupé',
    };
    return Form(
      key: _formKey,
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (_error != null)
            Padding(
              padding: const EdgeInsets.only(bottom: 12),
              child: Text(_error!, style: const TextStyle(color: Colors.red)),
            ),
          Text('Firmware ${info['version'] ?? '?'}  ·  WiFi $mode',
              style: const TextStyle(color: Colors.grey)),
          const SizedBox(height: 16),
          _section('WiFi (portail web et mise à jour)'),
          _field(_ssidCtrl, 'Nom du réseau (SSID)', Icons.wifi),
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: TextFormField(
              controller: _wifiPassCtrl,
              obscureText: _obscureWifi,
              decoration: InputDecoration(
                labelText: 'Mot de passe WiFi',
                hintText: 'inchangé si vide',
                prefixIcon: const Icon(Icons.lock),
                suffixIcon: IconButton(
                  icon: Icon(
                      _obscureWifi ? Icons.visibility : Icons.visibility_off),
                  tooltip: _obscureWifi ? 'Afficher' : 'Masquer',
                  onPressed: () => setState(() => _obscureWifi = !_obscureWifi),
                ),
                border: const OutlineInputBorder(),
              ),
            ),
          ),
          const SizedBox(height: 16),
          _section('Module'),
          _field(_hostnameCtrl, 'Nom du module (hostname)', Icons.badge,
              validator: (v) =>
                  (v == null || v.trim().isEmpty) ? 'Nom requis' : null),
          _field(_lightDelayCtrl, 'Délai allumage LEDs (cycles de 100 ms)',
              Icons.lightbulb,
              keyboardType: TextInputType.number,
              validator: _range(0, 600)),
          _field(_wdtCtrl, 'Watchdog (secondes)', Icons.timer,
              keyboardType: TextInputType.number, validator: _range(3, 300)),
          const SizedBox(height: 8),
          FilledButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save),
            label: const Text('Enregistrer et redémarrer le module'),
            style: FilledButton.styleFrom(
              backgroundColor: Colors.blue,
              minimumSize: const Size.fromHeight(48),
            ),
          ),
          const SizedBox(height: 24),
          _section('Maintenance'),
          OutlinedButton.icon(
            onPressed: info['wifi_mode'] == 'ap'
                ? null
                : () => _confirm(
                    'Activer le point d\'accès WiFi du module ?\n\n'
                        'Le portail web sera accessible sur le réseau '
                        '« BedCtrlConfig ». Le lit reste pilotable en Bluetooth.',
                    'wifi_ap'),
            icon: const Icon(Icons.wifi_tethering),
            label: const Text('Activer le portail web (mode AP)'),
            style: OutlinedButton.styleFrom(
                minimumSize: const Size.fromHeight(48)),
          ),
          const SizedBox(height: 12),
          OutlinedButton.icon(
            onPressed: () => _confirm('Redémarrer le module ?', 'restart'),
            icon: const Icon(Icons.restart_alt),
            label: const Text('Redémarrer le module'),
            style: OutlinedButton.styleFrom(
                minimumSize: const Size.fromHeight(48)),
          ),
        ],
      ),
    );
  }

  FormFieldValidator<String> _range(int min, int max) => (v) {
        final n = int.tryParse(v ?? '');
        return (n == null || n < min || n > max)
            ? 'Entre $min et $max'
            : null;
      };

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
    TextInputType keyboardType = TextInputType.text,
    FormFieldValidator<String>? validator,
  }) =>
      Padding(
        padding: const EdgeInsets.only(bottom: 12),
        child: TextFormField(
          controller: ctrl,
          keyboardType: keyboardType,
          validator: validator,
          decoration: InputDecoration(
            labelText: label,
            prefixIcon: Icon(icon),
            border: const OutlineInputBorder(),
          ),
        ),
      );
}

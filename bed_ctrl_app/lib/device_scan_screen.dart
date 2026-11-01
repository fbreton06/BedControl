import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';
import 'ble_service.dart';

/// Recherche des modules BedCtrl à portée et choix de celui à piloter.
class DeviceScanScreen extends StatefulWidget {
  const DeviceScanScreen({super.key});

  @override
  State<DeviceScanScreen> createState() => _DeviceScanScreenState();
}

class _DeviceScanScreenState extends State<DeviceScanScreen> {
  String? _error;

  @override
  void initState() {
    super.initState();
    _scan();
  }

  @override
  void dispose() {
    BleService.stopScan();
    super.dispose();
  }

  Future<void> _scan() async {
    setState(() => _error = null);
    try {
      await BleService.startScan();
    } catch (e) {
      if (mounted) setState(() => _error = 'Scan impossible : $e');
    }
  }

  Future<void> _select(ScanResult r) async {
    await BleService.stopScan();
    if (!mounted) return;
    await context.read<AppState>().selectDevice(r.device.remoteId.str, _name(r));
    if (mounted) Navigator.pop(context);
  }

  String _name(ScanResult r) =>
      r.device.advName.isNotEmpty ? r.device.advName : r.device.remoteId.str;

  @override
  Widget build(BuildContext context) {
    final currentId = context.watch<AppState>().deviceId;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Choisir le module'),
        backgroundColor: Colors.black87,
        foregroundColor: Colors.white,
      ),
      body: Column(
        children: [
          if (_error != null)
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 16, 16, 0),
              child: Text(_error!, style: const TextStyle(color: Colors.red)),
            ),
          Expanded(
            child: StreamBuilder<List<ScanResult>>(
              stream: BleService.scanResults,
              initialData: const [],
              builder: (_, snap) {
                final results = snap.data!;
                if (results.isEmpty) {
                  return const Center(child: Text('Aucun module trouvé'));
                }
                return ListView(
                  children: [
                    for (final r in results)
                      ListTile(
                        leading: const Icon(Icons.bluetooth),
                        title: Text(_name(r)),
                        subtitle:
                            Text('${r.device.remoteId.str} · ${r.rssi} dBm'),
                        trailing: r.device.remoteId.str == currentId
                            ? const Icon(Icons.check, color: Colors.green)
                            : const Icon(Icons.chevron_right),
                        onTap: () => _select(r),
                      ),
                  ],
                );
              },
            ),
          ),
          StreamBuilder<bool>(
            stream: BleService.isScanning,
            initialData: false,
            builder: (_, snap) => Padding(
              padding: const EdgeInsets.all(16),
              child: FilledButton.icon(
                onPressed: snap.data! ? null : _scan,
                icon: snap.data!
                    ? const SizedBox(
                        width: 18,
                        height: 18,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.search),
                label: Text(snap.data! ? 'Recherche…' : 'Rechercher'),
                style: FilledButton.styleFrom(
                  minimumSize: const Size.fromHeight(48),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

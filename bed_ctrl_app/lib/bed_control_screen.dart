import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'app_state.dart';
import 'mqtt_service.dart';
import 'settings_screen.dart';

class BedControlScreen extends StatelessWidget {
  const BedControlScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      body: SafeArea(
        child: SingleChildScrollView(
          padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: Column(
            children: [
              _Header(),
              SizedBox(height: 16),
              _BedsSelector(),
              SizedBox(height: 16),
              _BedImage(),
              SizedBox(height: 16),
              _PartsSelector(),
              SizedBox(height: 20),
              _UpDownRow(),
              SizedBox(height: 24),
              _PositionLabel(),
              SizedBox(height: 12),
              _PositionRow(),
              SizedBox(height: 20),
              _MemoryRow(),
              SizedBox(height: 12),
            ],
          ),
        ),
      ),
    );
  }
}

// ── En-tête : titre + voyant + paramètres ─────────────────────────────────────

class _Header extends StatelessWidget {
  const _Header();

  @override
  Widget build(BuildContext context) {
    return Consumer<AppState>(
      builder: (_, state, __) {
        final connected =
            state.connectionState == AppMqttConnectionState.connected;
        final connecting =
            state.connectionState == AppMqttConnectionState.connecting;

        final dark = Theme.of(context).brightness == Brightness.dark;
        final Color ledColor = connected
            ? Colors.green
            : connecting
                ? Colors.orange
                : Colors.red;

        return Row(
          children: [
            const SizedBox(width: 48), // équilibre le bouton paramètres à droite
            Expanded(
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(
                    'Bed Control',
                    style: TextStyle(
                      color: dark ? Colors.white : Colors.black87,
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                  const SizedBox(width: 12),
                  // Voyant connexion — tap pour (dé)connecter
                  Tooltip(
                    message: connected
                        ? 'Connecté — toucher pour déconnecter'
                        : connecting
                            ? 'Connexion...'
                            : 'Déconnecté — toucher pour connecter',
                    child: GestureDetector(
                      onTap: connecting
                          ? null
                          : connected
                              ? state.disconnect
                              : state.connect,
                      child: AnimatedContainer(
                        duration: const Duration(milliseconds: 300),
                        width: 20,
                        height: 20,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: ledColor,
                          boxShadow: [
                            BoxShadow(
                              color: ledColor.withValues(alpha: 0.5),
                              blurRadius: 6,
                              spreadRadius: 1,
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            ),
            IconButton(
              icon: Icon(Icons.settings,
                  color: dark ? Colors.white70 : Colors.black54),
              tooltip: 'Paramètres',
              onPressed: () => Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const SettingsScreen()),
              ),
            ),
          ],
        );
      },
    );
  }
}

// ── Sélecteur "beds" : LEFT / BOTH / RIGHT ────────────────────────────────────

class _BedsSelector extends StatelessWidget {
  const _BedsSelector();

  @override
  Widget build(BuildContext context) {
    return Consumer<AppState>(
      builder: (_, state, __) => _SegSelector(
        selected: state.beds,
        onChanged: state.setBeds,
        options: const {
          'left': 'LEFT',
          'both': 'BOTH',
          'right': 'RIGHT',
        },
      ),
    );
  }
}

// ── Sélecteur "parts" : HEAD / BOTH / LEG ─────────────────────────────────────

class _PartsSelector extends StatelessWidget {
  const _PartsSelector();

  @override
  Widget build(BuildContext context) {
    return Consumer<AppState>(
      builder: (_, state, __) => _SegSelector(
        selected: state.parts,
        onChanged: state.setParts,
        options: const {
          'head': 'HEAD',
          'both': 'BOTH',
          'leg': 'LEG',
        },
      ),
    );
  }
}

/// Commutateur segmenté générique à 3 positions (aucun MQTT, état seulement).
class _SegSelector extends StatelessWidget {
  final String selected;
  final ValueChanged<String> onChanged;
  final Map<String, String> options; // value -> label

  const _SegSelector({
    required this.selected,
    required this.onChanged,
    required this.options,
  });

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final unselectedBg = dark ? const Color(0xFF2A2A2A) : Colors.white;
    final unselectedFg = dark ? Colors.white70 : Colors.black87;
    final borderColor = dark ? Colors.white24 : const Color(0xFFBFC7D5);

    return SegmentedButton<String>(
      segments: options.entries
          .map((e) => ButtonSegment(value: e.key, label: Text(e.value)))
          .toList(),
      selected: {selected},
      showSelectedIcon: false,
      onSelectionChanged: (sel) => onChanged(sel.first),
      style: ButtonStyle(
        backgroundColor: WidgetStateProperty.resolveWith((states) =>
            states.contains(WidgetState.selected)
                ? const Color(0xFF4F8FE0)
                : unselectedBg),
        foregroundColor: WidgetStateProperty.resolveWith((states) =>
            states.contains(WidgetState.selected)
                ? Colors.white
                : unselectedFg),
        side: WidgetStateProperty.all(BorderSide(color: borderColor)),
      ),
    );
  }
}

// ── Image du lit ──────────────────────────────────────────────────────────────

class _BedImage extends StatelessWidget {
  const _BedImage();

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    return SizedBox(
      height: 150,
      child: Image.asset(
        'assets/images/bed.png',
        fit: BoxFit.contain,
        // Placeholder tant que l'asset n'est pas fourni
        errorBuilder: (_, __, ___) => Center(
          child: Icon(Icons.king_bed_outlined,
              size: 96, color: dark ? Colors.white24 : Colors.black26),
        ),
      ),
    );
  }
}

// ── Flèches up / down (boutons à maintien) ────────────────────────────────────

class _UpDownRow extends StatelessWidget {
  const _UpDownRow();

  @override
  Widget build(BuildContext context) {
    return Consumer<AppState>(
      builder: (_, state, __) {
        final enabled = state.isConnected;
        final dark = Theme.of(context).brightness == Brightness.dark;
        final circle = dark ? const Color(0xFF2A2A2A) : Colors.white;
        final iconColor = dark ? Colors.white70 : Colors.black54;
        return Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            _HoldButton(
              size: 60,
              color: circle,
              border: true,
              enabled: enabled,
              onPress: () => state.sendCommand('up', 'on'),
              onRelease: () => state.sendCommand('up', 'off'),
              child: Icon(Icons.keyboard_arrow_up, size: 32, color: iconColor),
            ),
            _HoldButton(
              size: 60,
              color: circle,
              border: true,
              enabled: enabled,
              onPress: () => state.sendCommand('down', 'on'),
              onRelease: () => state.sendCommand('down', 'off'),
              child: Icon(Icons.keyboard_arrow_down, size: 32, color: iconColor),
            ),
          ],
        );
      },
    );
  }
}

class _PositionLabel extends StatelessWidget {
  const _PositionLabel();

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    return Text(
      'POSITION',
      style: TextStyle(
        color: dark ? Colors.white54 : Colors.black45,
        fontSize: 12,
        fontWeight: FontWeight.w600,
        letterSpacing: 1.5,
      ),
    );
  }
}

// ── Same / Flat / Light (boutons à maintien) ──────────────────────────────────

class _PositionRow extends StatelessWidget {
  const _PositionRow();

  @override
  Widget build(BuildContext context) {
    return Consumer<AppState>(
      builder: (_, state, __) {
        final enabled = state.isConnected;
        return Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            _HoldButton(
              size: 64,
              color: const Color(0xFFADD8E6),
              enabled: enabled,
              label: 'Same',
              child: const Icon(Icons.bed, size: 26, color: Colors.white),
              onPress: () => state.sendCommand('same', 'on'),
              onRelease: () => state.sendCommand('same', 'off'),
            ),
            _HoldButton(
              size: 64,
              color: const Color(0xFFB9A7E0),
              enabled: enabled,
              label: 'Flat',
              child: const Icon(Icons.bed, size: 26, color: Colors.white),
              onPress: () => state.sendCommand('flat', 'on'),
              onRelease: () => state.sendCommand('flat', 'off'),
            ),
            _HoldButton(
              size: 64,
              color: const Color(0xFFF3B8C2),
              enabled: enabled,
              label: 'Light',
              child: const Icon(Icons.lightbulb, size: 26, color: Colors.white),
              onPress: () => state.sendCommand('light', 'on'),
              onRelease: () => state.sendCommand('light', 'off'),
            ),
          ],
        );
      },
    );
  }
}

// ── Mémoires M1 / M2 (appui long / court) ─────────────────────────────────────

class _MemoryRow extends StatelessWidget {
  const _MemoryRow();

  @override
  Widget build(BuildContext context) {
    return Consumer<AppState>(
      builder: (_, state, __) {
        final enabled = state.isConnected;
        final threshold = Duration(seconds: state.longPressSeconds);
        return Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            _MemoryButton(
              label: 'Memory 1',
              text: 'M1',
              enabled: enabled,
              threshold: threshold,
              onPush: (push) => state.sendCommand('m1', push),
            ),
            _MemoryButton(
              label: 'Memory 2',
              text: 'M2',
              enabled: enabled,
              threshold: threshold,
              onPush: (push) => state.sendCommand('m2', push),
            ),
          ],
        );
      },
    );
  }
}

// ── Widgets boutons réutilisables ─────────────────────────────────────────────

/// Bouton circulaire à maintien : onPress à l'appui, onRelease au relâchement.
class _HoldButton extends StatefulWidget {
  final double size;
  final Color color;
  final bool border;
  final bool enabled;
  final String? label;
  final Widget? child;
  final VoidCallback onPress;
  final VoidCallback onRelease;

  const _HoldButton({
    required this.size,
    required this.color,
    required this.enabled,
    required this.onPress,
    required this.onRelease,
    this.border = false,
    this.label,
    this.child,
  });

  @override
  State<_HoldButton> createState() => _HoldButtonState();
}

class _HoldButtonState extends State<_HoldButton> {
  bool _pressed = false;

  void _down() {
    if (!widget.enabled) return;
    setState(() => _pressed = true);
    widget.onPress();
  }

  void _up() {
    if (!_pressed) return;
    setState(() => _pressed = false);
    widget.onRelease();
  }

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final borderColor = dark ? Colors.white24 : const Color(0xFFBFC7D5);
    final labelColor = dark ? Colors.white70 : Colors.black54;
    final color =
        widget.enabled ? widget.color : widget.color.withValues(alpha: 0.4);
    return Listener(
      onPointerDown: widget.enabled ? (_) => _down() : null,
      onPointerUp: (_) => _up(),
      onPointerCancel: (_) => _up(),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          AnimatedScale(
            scale: _pressed ? 0.9 : 1.0,
            duration: const Duration(milliseconds: 80),
            child: Container(
              width: widget.size,
              height: widget.size,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                border:
                    widget.border ? Border.all(color: borderColor) : null,
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withValues(alpha: 0.08),
                    blurRadius: 4,
                    offset: const Offset(0, 2),
                  ),
                ],
              ),
              child: widget.child != null ? Center(child: widget.child) : null,
            ),
          ),
          if (widget.label != null) ...[
            const SizedBox(height: 6),
            Text(widget.label!,
                style: TextStyle(color: labelColor, fontSize: 12)),
          ],
        ],
      ),
    );
  }
}

/// Bouton mémoire : mesure la durée d'appui → 'long' (≥ seuil) ou 'short'.
class _MemoryButton extends StatefulWidget {
  final String label;
  final String text;
  final bool enabled;
  final Duration threshold;
  final ValueChanged<String> onPush;

  const _MemoryButton({
    required this.label,
    required this.text,
    required this.enabled,
    required this.threshold,
    required this.onPush,
  });

  @override
  State<_MemoryButton> createState() => _MemoryButtonState();
}

class _MemoryButtonState extends State<_MemoryButton> {
  bool _pressed = false;
  bool _longSent = false;
  Timer? _timer;

  void _down() {
    if (!widget.enabled) return;
    _longSent = false;
    setState(() => _pressed = true);
    // Dès que le seuil est atteint, on envoie 'long' sans attendre le relâchement.
    _timer = Timer(widget.threshold, () {
      _longSent = true;
      widget.onPush('long');
      if (mounted) {
        ScaffoldMessenger.of(context)
          ..hideCurrentSnackBar()
          ..showSnackBar(SnackBar(
            content: Text('Positions stored in ${widget.text}'),
            duration: const Duration(seconds: 1),
            behavior: SnackBarBehavior.floating,
          ));
      }
    });
  }

  void _up() {
    if (!_pressed) return;
    _timer?.cancel();
    setState(() => _pressed = false);
    // Relâchement avant le seuil → 'short'. Si 'long' déjà envoyé, ne rien renvoyer.
    if (!_longSent) widget.onPush('short');
  }

  void _cancel() {
    // Geste interrompu (ex. scroll) : on annule sans rien envoyer.
    if (!_pressed) return;
    _timer?.cancel();
    setState(() => _pressed = false);
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final labelColor = dark ? Colors.white70 : Colors.black54;
    const base = Color(0xFFF0A868);
    final color = widget.enabled ? base : base.withValues(alpha: 0.4);
    return Listener(
      onPointerDown: widget.enabled ? (_) => _down() : null,
      onPointerUp: (_) => _up(),
      onPointerCancel: (_) => _cancel(),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          AnimatedScale(
            scale: _pressed ? 0.9 : 1.0,
            duration: const Duration(milliseconds: 80),
            child: Container(
              width: 60,
              height: 60,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                boxShadow: [
                  BoxShadow(
                    color: Colors.black.withValues(alpha: 0.08),
                    blurRadius: 4,
                    offset: const Offset(0, 2),
                  ),
                ],
              ),
              child: Center(
                child: Text(
                  widget.text,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            ),
          ),
          const SizedBox(height: 6),
          Text(widget.label,
              style: TextStyle(color: labelColor, fontSize: 12)),
        ],
      ),
    );
  }
}

#!/usr/bin/env python3
"""
migrate_colours.py — Migra Colour(0xFF...) hardcodeados a MixCoachTheme tokens.

Uso:
    python scripts/migrate_colours.py                                # Dry-run (vista previa)
    python scripts/migrate_colours.py --apply                        # Aplica cambios
    python scripts/migrate_colours.py --apply --file ruta/archivo    # Solo un archivo
    python scripts/migrate_colours.py --show-unmapped                # Muestra colores sin mapeo
"""

import os
import re
import sys
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except: pass

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIR = os.path.join(PROJECT_ROOT, "Source")

# ─── Per-file colour maps ─────────────────────────────────────────────────
# Each map is specific to a file. The same hex can mean different things
# in different files (e.g. 0xFF3B82F6 = roleBass in MixMap, info elsewhere).
# Key = hex like "0xFFEF4444", Value = MixCoachTheme expression.

FILE_MAPS = {
    # ── MasterMeterPanel.cpp ─────────────────────────────────────────────
    "MasterMeterPanel.cpp": {
        "0xFFFFD93D": "MixCoachTheme::warning()",
        "0xFFFF5C74": "MixCoachTheme::error()",
        "0xFF4ADE80": "MixCoachTheme::success()",
        "0xFFFF5C74": "MixCoachTheme::truePeak()",
        "0xFFFFD93D": "MixCoachTheme::warning()",
        "0xFF4ADE80": "MixCoachTheme::success()",
        "0xFFFF5C74": "MixCoachTheme::error()",
        "0xFFFFD93D": "MixCoachTheme::warning()",
        "0xFF4ADE80": "MixCoachTheme::success()",
        "0xFF1A1A2A": "MixCoachTheme::bgDark()",
        "0xFFCCCCCC": "MixCoachTheme::textSecondary()",
        "0xFF2A2A4A": "MixCoachTheme::bgPanel()",
        "0xFFFFD93D": "MixCoachTheme::warning()",
        "0xFF888888": "MixCoachTheme::textMuted()",
    },

    # ── MasterMeterPanel.h ───────────────────────────────────────────────
    "MasterMeterPanel.h": {
        "0xFFFF5C74": "MixCoachTheme::error()",
        "0xFFFFD93D": "MixCoachTheme::warning()",
        "0xFF4ADE80": "MixCoachTheme::success()",
        "0xFF888888": "MixCoachTheme::textMuted()",
    },

    # ── DiagnosticBridge.cpp ──────────────────────────────────────────────
    "DiagnosticBridge.cpp": {
        "0xFF10B981": "MixCoachTheme::success()",
        "0xFFEF4444": "MixCoachTheme::error()",
        "0xFFF59E0B": "MixCoachTheme::warning()",
        "0xFFF97316": "MixCoachTheme::warning()",
    },

    # ── RefToggle.cpp ──────────────────────────────────────────────────────
    "RefToggle.cpp": {
        "0xFFFFD700": "MixCoachTheme::warning()",
        "0x66999999": "MixCoachTheme::textMuted()",
    },

    # ── ReferenceDrivenEngine.cpp ──────────────────────────────────────────
    "ReferenceDrivenEngine.cpp": {
        "0xFFFF5252": "MixCoachTheme::error()",
        "0xFFFFC107": "MixCoachTheme::warning()",
        "0xFF00B7FF": "MixCoachTheme::accentCyan()",
        "0xFF4CAF50": "MixCoachTheme::success()",
    },

    # ── ReferenceMatchPanel.cpp ────────────────────────────────────────────
    "ReferenceMatchPanel.cpp": {
        "0xFF6B5B95": "MixCoachTheme::specSub()",
        "0xFFE57373": "MixCoachTheme::specAir()",
        "0xFFFFB74D": "MixCoachTheme::specPres()",
        "0xFF4DB6AC": "MixCoachTheme::specLoMid()",
        "0xFF64B5F6": "MixCoachTheme::specBass()",
        "0xFFBA68C8": "MixCoachTheme::specSub()",
    },

    # ── StereoWidthMeter.cpp ──────────────────────────────────────────────
    "StereoWidthMeter.cpp": {
        "0xFF2A3344": "MixCoachTheme::bgSurface()",
        "0xFF6B7280": "MixCoachTheme::textMuted()",
        "0xFF22C55E": "MixCoachTheme::success()",
        "0xFF3B82F6": "MixCoachTheme::info()",
        "0xFFF97316": "MixCoachTheme::warning()",
    },

    # ── MeterPanel.cpp ─────────────────────────────────────────────────────
    "MeterPanel.cpp": {
        "0xFFCCD0D6": "MixCoachTheme::textSecondary()",
        "0xFFFF3333": "MixCoachTheme::error()",
        "0xFFFFCC00": "MixCoachTheme::warning()",
        "0xFF00CC66": "MixCoachTheme::meterGreen()",
        "0xFF0A0E14": "MixCoachTheme::bgDarker()",
        "0xFF2A3344": "MixCoachTheme::bgSurface()",
        "0xFF8899AA": "MixCoachTheme::textDim()",
        "0xFF55CCFF": "MixCoachTheme::accentCyan()",
        "0xFF2A4455": "MixCoachTheme::bgSurface()",
    },

    # ── MessengerListDrawing.cpp ──────────────────────────────────────────
    "MessengerListDrawing.cpp": {
        "0xFF8B8FA3": "textDim()",
        "0xFFF1F1F6": "textBright()",
        "0xFF2E2F3E": "MixCoachTheme::tooltipBorder()",
        "0xFF5C5F73": "textMuted()",
        "0xFFD1D1E0": "textBright()",
        "0xFF141420": "MixCoachTheme::bgCanvas()",
    },

    # ── MixMapComponent.cpp ───────────────────────────────────────────────
    "MixMapComponent.cpp": {
        "0xFF0E0F18": "MixCoachTheme::bgDarker()",
        "0xFF25262E": "MixCoachTheme::bgSurface()",
        "0xFFFFF6E0": "MixCoachTheme::textPrimary()",
        "0xFF94A3B8": "MixCoachTheme::textDim()",
        "0xFF10B981": "MixCoachTheme::success()",
        "0xFF3B82F6": "MixCoachTheme::info()",
        "0xFFF59E0B": "MixCoachTheme::warning()",
        "0xFF8B5CF6": "MixCoachTheme::roleDrums()",
        "0xFFEF4444": "MixCoachTheme::error()",
        "0xFFF97316": "MixCoachTheme::roleGuitars()",
        "0xFFEC4899": "MixCoachTheme::roleVocals()",
    },

    # ── Messenger/PluginEditor.cpp ────────────────────────────────────────
    "PluginEditor.cpp": {
        "0xFFEF4444": "MixCoachTheme::error()",
        "0xFFF97316": "MixCoachTheme::roleGuitars()",
        "0xFFEAB308": "MixCoachTheme::warning()",
        "0xFF22C55E": "MixCoachTheme::success()",
        "0xFF3B82F6": "MixCoachTheme::info()",
        "0xFFA78BFA": "MixCoachTheme::roleMelody()",
        "0xFFEC4899": "MixCoachTheme::roleVocals()",
        "0xFF14B8A6": "MixCoachTheme::roleFX()",
        "0xFFAAB4C0": "MixCoachTheme::textDim()",
        "0xFF25263A": "MixCoachTheme::bgDark()",
        "0xFFDC2626": "MixCoachTheme::error()",
    },

    # ── ColourPresetStrip.cpp ─────────────────────────────────────────────
    "ColourPresetStrip.cpp": {
        "0xFFE74C3C": "MixCoachTheme::error()",
        "0xFFE67E22": "MixCoachTheme::warning()",
        "0xFFF1C40F": "MixCoachTheme::warning()",
        "0xFF2ECC71": "MixCoachTheme::success()",
        "0xFF1ABC9C": "MixCoachTheme::roleFX()",
        "0xFF3498DB": "MixCoachTheme::info()",
        "0xFF9B59B6": "MixCoachTheme::accent()",
        "0xFFE91E63": "MixCoachTheme::roleVocals()",
        "0xFF2C2C3E": "MixCoachTheme::bgDark()",
    },

    # ── ColourSwatch.cpp ──────────────────────────────────────────────────
    "ColourSwatch.cpp": {
        "0xFF2C2C3E": "MixCoachTheme::bgDark()",
    },

    # ── Core/PluginEditor.cpp ─────────────────────────────────────────────
    "PluginEditor.cpp": {  # MixCoach core/PluginEditor.cpp
        "0xFF000000": "juce::Colours::black",
    },

    # ── SpectrographDrawing.cpp ──────────────────────────────────────────
    "SpectrographDrawing.cpp": {
        "0xFF0A0E1A": "MixCoachTheme::bgCanvas()",
        "0xFF1A2A44": "MixCoachTheme::bgSurface()",
        "0xFFCCDDFF": "MixCoachTheme::textBright()",
        "0xFF8899BB": "MixCoachTheme::textMuted()",
    },

    # ── SpectrographComponent.cpp ─────────────────────────────────────────
    "SpectrographComponent.cpp": {
        "0xFF1A2A44": "MixCoachTheme::bgSurface()",
    },

    # ── SpectrographProcessor.cpp ─────────────────────────────────────────
    "SpectrographProcessor.cpp": {
        "0xFF44BBFF": "MixCoachTheme::accentCyanBright()",
    },

    # ── CrestPanel.cpp ────────────────────────────────────────────────────
    "CrestPanel.cpp": {
        "0xFF1A2A44": "MixCoachTheme::bgSurface()",
        "0xFFA855F7": "MixCoachTheme::accentGlow()",
        "0xFFC084FC": "MixCoachTheme::accentGlow()",
        "0xFF0E1520": "MixCoachTheme::bgDark()",
        "0xFF06080E": "MixCoachTheme::bgDarker()",
        "0xFF0A0E18": "MixCoachTheme::bgDarker()",
    },

    # ── AnalogVUMeter.cpp ─────────────────────────────────────────────────
    "AnalogVUMeter.cpp": {
        "0xFFEED087": "MixCoachTheme::vuFace()",
        "0xFF111111": "MixCoachTheme::vuNeedle()",
        "0xFFD32F2F": "MixCoachTheme::vuRed()",
        "0xFF1A1A1A": "MixCoachTheme::vuText()",
        "0xFF0D0D0D": "MixCoachTheme::vuBg()",
        "0xFF151515": "MixCoachTheme::vuBorder()",
    },

    # ── StereoVUMeter.cpp ─────────────────────────────────────────────────
    "StereoVUMeter.cpp": {},  # Already uses MixCoachTheme::error()

    # ── StereoWidthMeter.cpp (additional) ─────────────────────────────────
    "StereoWidthMeter.cpp": {
        "0xFF2A3344": "MixCoachTheme::bgSurface()",
    },

    # ── VintageVUMeters.cpp ────────────────────────────────────────────────
    # Uses juce::Colours::white/black/transparentBlack — already fine
    "VintageVUMeters.cpp": {},
}

# Files to skip (tests, theme definitions, etc.)
SKIP_FILES = [
    "tests\\",
    "tests/",
    "MixCoachTheme.h",
    "Constants.h",
    "SlotRegistry.cpp",
    "CircularGauge.h",
    "pch.h",
]

# Full path patterns for disambiguation (when same filename exists in multiple dirs)
PATH_HINTS = {
    "MixCoach/core/PluginEditor.cpp": ["core/PluginEditor.cpp"],
    "Messenger/ui/PluginEditor.cpp": ["Messenger/ui/PluginEditor.cpp"],
    "MixCoach/core/PluginEditor.cpp": ["core/PluginEditor.cpp"],
}


def should_skip(filepath):
    rel = os.path.relpath(filepath, PROJECT_ROOT).replace("\\", "/")
    for pattern in SKIP_FILES:
        if pattern in rel:
            return True
    return False


def get_file_map(filepath):
    """Get the colour map for a specific file, using path hints if needed."""
    rel = os.path.relpath(filepath, PROJECT_ROOT).replace("\\", "/")
    filename = os.path.basename(filepath)

    # Check path-specific hints first
    for path_pattern, filenames in PATH_HINTS.items():
        for fn in filenames:
            if fn in rel:
                # Check if there's a map for this path
                map_key = os.path.basename(path_pattern)
                if map_key in FILE_MAPS:
                    # Only use if the file matches
                    if path_pattern.replace("\\", "/") in rel:
                        return FILE_MAPS.get(map_key, {})

    # Fall back to filename-based map
    return FILE_MAPS.get(filename, {})


def find_colour_matches(content):
    """Find all juce::Colour(0xFF...) patterns in content."""
    # Match: juce::Colour(0xFF......) — capture hex value
    pattern = r'juce::Colour\(0xFF([0-9A-Fa-f]{6})\)'
    matches = []
    for m in re.finditer(pattern, content):
        hex_val = "0xFF" + m.group(1).upper()
        matches.append((m.start(), hex_val, m.group(0)))
    return matches


def replace_in_file(filepath, dry_run=True):
    """Replace hardcoded colours in a single file. Returns list of changes."""
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    colour_map = get_file_map(filepath)
    if not colour_map:
        return []

    matches = find_colour_matches(content)
    if not matches:
        return []

    changes = []  # (start_pos, hex_val, token, full_match)
    seen = set()

    for start, hex_val, full_match in matches:
        if start in seen:
            continue
        seen.add(start)

        if hex_val in colour_map:
            token = colour_map[hex_val]
            changes.append((start, hex_val, token, full_match))

    if not changes:
        return []

    if not dry_run:
        new_content = content
        # Sort by position descending to avoid offset issues
        for start_pos, hex_val, token, full_match in sorted(changes, key=lambda x: -x[0]):
            new_content = new_content.replace(full_match, token, 1)

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)

    rel = os.path.relpath(filepath, PROJECT_ROOT)
    return [(rel, c[1], c[2]) for c in changes]


def find_unmapped_colours(filepath):
    """Find colours in a file that don't have a mapping yet."""
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    colour_map = get_file_map(filepath)
    matches = find_colour_matches(content)

    unmapped = []
    seen = set()
    for start, hex_val, _ in matches:
        if start in seen:
            continue
        seen.add(start)
        if hex_val not in colour_map:
            unmapped.append(hex_val)

    return unmapped


def get_source_files():
    """Get all .cpp and .h files in Source/"""
    files = []
    for root, dirs, fnames in os.walk(SOURCE_DIR):
        for f in fnames:
            if f.endswith(('.cpp', '.h')):
                files.append(os.path.join(root, f))
    return sorted(files)


def main():
    dry_run = "--apply" not in sys.argv
    show_unmapped = "--show-unmapped" in sys.argv

    specific_file = None
    for i, arg in enumerate(sys.argv):
        if arg == "--file" and i + 1 < len(sys.argv):
            specific_file = sys.argv[i + 1]

    if specific_file:
        fp = os.path.join(PROJECT_ROOT, specific_file)
        if not os.path.exists(fp):
            print(f"File not found: {specific_file}")
            sys.exit(1)
        files = [fp]
    else:
        files = get_source_files()

    # Filter skipped files
    files = [f for f in files if not should_skip(f)]

    # Show unmapped colours if requested
    if show_unmapped:
        print(f"\n{'=' * 60}")
        print(f"  COLOURS WITHOUT MAPPING")
        print(f"{'=' * 60}\n")
        for fp in files:
            unmapped = find_unmapped_colours(fp)
            if unmapped:
                rel = os.path.relpath(fp, PROJECT_ROOT)
                print(f"  {rel}:")
                for h in sorted(set(unmapped)):
                    print(f"    {h}")
                print()
        sys.exit(0)

    # Apply changes
    total_changes = 0
    file_results = {}

    for fp in files:
        changes = replace_in_file(fp, dry_run)
        if changes:
            rel = os.path.relpath(fp, PROJECT_ROOT)
            file_results[rel] = changes
            total_changes += len(changes)

    # Print results
    mode = "DRY RUN" if dry_run else "APPLIED"
    print(f"\n{'=' * 60}")
    print(f"  {mode}: {total_changes} colour replacements in {len(file_results)} files")
    print(f"{'=' * 60}\n")

    for rel in sorted(file_results.keys()):
        changes = file_results[rel]
        print(f"  {rel} ({len(changes)} changes):")
        for _, hex_val, token in changes[:10]:
            print(f"    {hex_val:14s} \u2192 {token}")
        if len(changes) > 10:
            print(f"    ... and {len(changes) - 10} more")
        print()

    print(f"{'=' * 60}")
    print(f"  Total: {total_changes} changes")
    if dry_run:
        print(f"  Run with --apply to apply changes")
    if not dry_run and total_changes > 0:
        print(f"  \u26a0\uFE0f  Run quick_validate.ps1 to verify build")
    print(f"{'=' * 60}\n")


if __name__ == "__main__":
    main()

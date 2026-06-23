#!/usr/bin/env python3
"""
migrate_colours_v2.py — Migra TODOS los colores hardcodeados restantes a MixCoachTheme.
Completa la migración iniciada por migrate_colours.py.

Uso:
    python scripts/migrate_colours_v2.py              # Dry-run
    python scripts/migrate_colours_v2.py --apply      # Aplica cambios
"""

import os
import re
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ─── Mapeo de colores por archivo ───────────────────────────────────────
# Formato: "0xFFHHHHHH": "MixCoachTheme::token()"

FILE_MAPS = {
    # ─── CrestPanel.cpp ─────────────────────────────────────────────
    "Source/MixCoach/UI/CrestPanel.cpp": {
        "0xFF05080D": "MixCoachTheme::bgCanvas()",
        "0xFF0A0E14": "MixCoachTheme::bgDarker()",
        "0xFF1A2233": "MixCoachTheme::bgPanel().darker(0.3f)",
        "0xFF2A3344": "MixCoachTheme::bgPanel().darker(0.15f)",
        "0xFF2A3A55": "MixCoachTheme::bgSurface().withAlpha(0.5f)",
        "0xFF3A4A5A": "MixCoachTheme::bgSurface().withAlpha(0.3f)",
        "0xFF5A6A7A": "MixCoachTheme::textMuted()",
        "0xFF6A7A8A": "MixCoachTheme::textDim()",
        "0xFF888888": "MixCoachTheme::textMuted()",
        "0xFF8899AA": "MixCoachTheme::textDim()",
        "0xFF8A9AAA": "MixCoachTheme::textDim()",
        "0xFFCCDDEE": "MixCoachTheme::textSecondary()",
        "0xFFE0E4E8": "MixCoachTheme::textBright()",
        "0xFFF0F4F8": "MixCoachTheme::textPrimary()",
        "0xFF44CC66": "MixCoachTheme::success()",
        "0xFF44BBFF": "MixCoachTheme::accentCyanBright()",
        "0xFFEE3333": "MixCoachTheme::error()",
        "0xFFFF8833": "MixCoachTheme::meterOrange()",
        "0xFFFFCC44": "MixCoachTheme::meterYellow()",
    },
    # ─── SpectrographDrawing.cpp ────────────────────────────────────
    "Source/MixCoach/UI/SpectrographDrawing.cpp": {
        "0xFF00CC66": "MixCoachTheme::success()",
        "0xFF10B981": "MixCoachTheme::success()",
        "0xFF44BBFF": "MixCoachTheme::accentCyanBright()",
        "0xFF77DDFF": "MixCoachTheme::accentCyanBright().withAlpha(0.7f)",
        "0xFF888888": "MixCoachTheme::textMuted()",
        "0xFFCCCCCC": "MixCoachTheme::textSecondary()",
        "0xFFCCD0D6": "MixCoachTheme::textDim()",
        "0xFFE0E8F0": "MixCoachTheme::textBright().withAlpha(0.7f)",
        "0xFFEF4444": "MixCoachTheme::error()",
        "0xFFF97316": "MixCoachTheme::meterOrange()",
        "0xFFFF5555": "MixCoachTheme::error()",
        "0xFFFFCC00": "MixCoachTheme::warning()",
        "0xFFFFCC44": "MixCoachTheme::meterYellow()",
        "0xFFFFD700": "MixCoachTheme::meterYellow()",
        "0xFFFFDD44": "MixCoachTheme::meterYellow()",
        "0xFFFFF0CC": "MixCoachTheme::vuFace()",
    },
    # ─── MessengerListDrawing.cpp ───────────────────────────────────
    "Source/MixCoach/UI/MessengerListDrawing.cpp": {
        "0xFF0E0F18": "MixCoachTheme::bgDarker()",
        "0xFF1A1A2E": "MixCoachTheme::tooltipBg()",
        "0xFF25262E": "MixCoachTheme::bgDark()",
        "0xFF808080": "MixCoachTheme::textMuted()",
        "0xFF94A3B8": "MixCoachTheme::textDim()",
        "0xFFFFF6E0": "MixCoachTheme::warning().withAlpha(0.15f)",
    },
    # ─── MeterPanel.cpp ─────────────────────────────────────────────
    "Source/MixCoach/UI/MeterPanel.cpp": {
        "0xFFFF8C42": "MixCoachTheme::meterOrange()",
        "0xFFFF6B35": "MixCoachTheme::meterOrange().brighter(0.1f)",
        "0xFF44BBFF": "MixCoachTheme::accentCyanBright()",
        "0xFF22AAEE": "MixCoachTheme::accentCyan()",
        "0xFF0088CC": "MixCoachTheme::accentCyanDim()",
        "0xFF005599": "MixCoachTheme::info().darker(0.3f)",
    },
    # ─── MixMapComponent.cpp ────────────────────────────────────────
    "Source/MixCoach/UI/MixMapComponent.cpp": {
        "0xFF6B7280": "MixCoachTheme::textDim()",
        "0xFF1A1B26": "MixCoachTheme::bgDark()",
    },
    # ─── StereoWidthMeter.cpp ───────────────────────────────────────
    "Source/MixCoach/UI/StereoWidthMeter.cpp": {
        "0xFFF97316": "MixCoachTheme::meterOrange()",
        "0xFF6B7280": "MixCoachTheme::textDim()",
        "0xFF3B82F6": "MixCoachTheme::info()",
        "0xFF22C55E": "MixCoachTheme::success()",
    },
    # ─── ReferenceDrivenEngine.cpp (Ley 6: mantener hex original) ───
    # Estos se dejaron intencionalmente como hex para no crear dep. engine→UI
    # Se reemplazan con constantes locales definidas en el mismo archivo.
    "Source/MixCoach/engine/ReferenceDrivenEngine.cpp": {
        "0xFFFF5252": "kMatchColours[0]",
        "0xFFFFC107": "kMatchColours[1]",
        "0xFF00B7FF": "kMatchColours[2]",
        "0xFF4CAF50": "kMatchColours[3]",
    },
    # ─── Messenger/PluginEditor.cpp ────────────────────────────────
    "Source/Messenger/ui/PluginEditor.cpp": {
        "0xFFAAB4C0": "MixCoachTheme::textDim()",
        "0xFF25263A": "MixCoachTheme::bgDark()",
        "0xFFDC2626": "MixCoachTheme::error()",
        "0xFFFF6B6B": "MixCoachTheme::error().withAlpha(0.7f)",
        "0xFFFCD34D": "MixCoachTheme::warning()",
        "0xFFF59E0B": "MixCoachTheme::warning()",
    },
}

# ─── Archivos que necesitan tokens en MixCoachTheme ──────────────────
NEEDS_THEME_TOKENS = [
    "Source/MixCoach/UI/CrestPanel.cpp",
    "Source/MixCoach/UI/SpectrographDrawing.cpp",
    "Source/MixCoach/UI/MessengerListDrawing.cpp",
    "Source/MixCoach/UI/MeterPanel.cpp",
    "Source/MixCoach/UI/MixMapComponent.cpp",
    "Source/MixCoach/UI/StereoWidthMeter.cpp",
    "Source/Messenger/ui/PluginEditor.cpp",
]


def add_colour_helper(rel_path: str, hex_val: str, replacement: str) -> bool:
    """
    Para colores que usan .brighter()/.darker()/.withAlpha(),
    no hay mucho que hacer - la expresión ya es válida.
    """
    return True  # All replacements are valid MixCoachTheme expressions


def process_file(rel_path: str, apply: bool) -> list:
    full_path = os.path.join(PROJECT_ROOT, rel_path)
    if not os.path.exists(full_path):
        print(f"  ⚠️  Archivo no encontrado: {rel_path}")
        return []

    with open(full_path, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = []
    for hex_val, replacement in FILE_MAPS.get(rel_path, {}).items():
        # Busca juce::Colour(0xFF...) y Colour(0xFF...)
        for pattern in [f"juce::Colour({hex_val})", f"Colour({hex_val})"]:
            count = content.count(pattern)
            if count > 0:
                changes.append((hex_val, pattern, replacement, count))
                if apply:
                    content = content.replace(pattern, replacement)

    if apply and changes:
        with open(full_path, 'w', encoding='utf-8') as f:
            f.write(content)

    return changes


def process_reference_engine(apply: bool) -> list:
    """Special handling for ReferenceDrivenEngine - add static colour array."""
    rel = "Source/MixCoach/engine/ReferenceDrivenEngine.cpp"
    full = os.path.join(PROJECT_ROOT, rel)
    if not os.path.exists(full):
        return []

    with open(full, 'r', encoding='utf-8') as f:
        content = f.read()

    # Check if kMatchColours already exists
    if "kMatchColours[0]" in content:
        return []  # Already done

    # Add the colour array after the last #include
    include_end = content.rfind('\n')
    for _ in range(5):
        idx = content.rfind('#include', 0, include_end)
        if idx >= 0:
            include_end = content.index('\n', idx)
    
    marker = content.index('namespace', content.index('namespace'))
    ns_end = content.index('{', marker) + 1

    colour_array = """
// Colores para getMatchStatusColour() - se mantienen como constantes locales
// para evitar dependencia engine→UI (Ley 6).
static const juce::Colour kMatchColours[4] = {
    juce::Colour(0xFFFF5252), // Red - OffTarget
    juce::Colour(0xFFFFC107), // Amber - NearTarget  
    juce::Colour(0xFF00B7FF), // Cyan - OnTarget
    juce::Colour(0xFF4CAF50), // Green - Success
};

"""

    if apply:
        content = content[:ns_end] + colour_array + content[ns_end:]
        with open(full, 'w', encoding='utf-8') as f:
            f.write(content)
        return [("Added kMatchColours[4] array", rel)]
    return [("Would add kMatchColours[4] array", rel)]


def fix_cmakelists(apply: bool) -> list:
    """Add missing source files to CMakeLists.txt."""
    rel = "CMakeLists.txt"
    full = os.path.join(PROJECT_ROOT, rel)
    
    with open(full, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = []
    
    # Check if already added
    if "ColourPresetStrip.cpp" not in content:
        # Find the target_sources for Messenger or add after juce_add_plugin
        # Add after the target_link_libraries section
        pattern = "target_include_directories(Messenger PRIVATE"
        if pattern in content:
            insert_before = f"{pattern}\n    ${{CMAKE_CURRENT_SOURCE_DIR}}/Source\n)\n\n"
            
            sources_block = """target_sources(Messenger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Messenger/ui/ColourPresetStrip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Messenger/ui/ColourSwatch.cpp
)

"""
            if apply:
                content = content.replace(insert_before, sources_block + insert_before)
                with open(full, 'w', encoding='utf-8') as f:
                    f.write(content)
            changes.append(("Added ColourPresetStrip + ColourSwatch to CMakeLists.txt", rel))
    
    return changes


import sys
try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except AttributeError:
    pass

def main():
    apply = '--apply' in sys.argv
    
    mode = "🔧 APPLYING" if apply else "🔍 DRY-RUN"
    print(f"\n{'='*60}")
    print(f"  {mode} — Migración v2 de colores a MixCoachTheme")
    print(f"{'='*60}\n")

    all_changes = []

    # 1. Process each file
    for rel_path in FILE_MAPS:
        changes = process_file(rel_path, apply)
        if changes:
            total = sum(c[3] for c in changes)
            print(f"\n  📁 {rel_path}:")
            for hex_val, pattern, replacement, count in changes:
                print(f"    {hex_val} → {replacement}  ({count}x)")
            all_changes.extend(changes)

    # 2. Special handling for ReferenceDrivenEngine
    ref_changes = process_reference_engine(apply)
    if ref_changes:
        print(f"\n  📁 ReferenceDrivenEngine.cpp:")
        for desc, _ in ref_changes:
            print(f"    {desc}")

    # 3. Fix CMakeLists.txt
    cmake_changes = fix_cmakelists(apply)
    if cmake_changes:
        print(f"\n  📁 CMakeLists.txt:")
        for desc, _ in cmake_changes:
            print(f"    {desc}")

    # Summary
    total_replacements = sum(c[3] for c in all_changes)
    total_files = len(set(c[1] if len(c) >= 2 else '' for c in all_changes))
    
    print(f"\n{'='*60}")
    print(f"  {'✅ APLICADO' if apply else '📋 VISTA PREVIA'}: {total_replacements} reemplazos en {total_files} archivos")
    print(f"{'='*60}\n")

    # Instructions for ReferenceDrivenEngine
    if not apply:
        print("  Para ReferenceDrivenEngine.cpp:")
        print("    Se agregará un array kMatchColours[4] con los colores originales")
        print("    y se reemplazarán las referencias a juce::Colour(0xFF...) por kMatchColours[i]")
        print()

    if not apply:
        print("  Ejecuta con --apply para aplicar cambios.\n")


if __name__ == '__main__':
    main()

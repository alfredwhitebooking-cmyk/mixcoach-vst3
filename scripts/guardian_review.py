#!/usr/bin/env python3
"""
👁️ Guardian de la Visión — Script de apoyo para revisiones de PR.

Uso:
    python scripts/guardian_review.py [--files file1 file2 ...] [--all]

Este script NO reemplaza al Guardian. Solo asiste con:
- Verificar que los archivos modificados existen
- Buscar referencias a patrones prohibidos (scores, términos técnicos)
- Resumir el diff para facilitar la revisión del Guardian

Requiere: Python 3.8+
"""

import argparse
import os
import re
import sys
from pathlib import Path
from typing import List, Optional

# Forzar UTF-8 en Windows (soporte para emojis y caracteres Unicode)
if sys.platform == "win32":
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# ─── Configuración ────────────────────────────────────────

PROJECT_ROOT = Path(__file__).resolve().parent.parent

EXCLUIR_PATRONES_EN = {
    "colores_hardcodeados": ["MixCoachTheme.h"],
}

PATRONES_PROHIBIDOS = {
    "score_numerico": {
        "patron": r'\bMixScore\b',
        "severidad": "🔴",
        "mensaje": "MixScore visible al usuario. Violación de 00_PROJECT_IDENTITY.md §6",
        "solo_cpp": True,
    },
    "termino_tecnico": {
        "patron": r'(isOptimal|slotIndex|severity|isCritical)\b',
        "severidad": "🟡",
        "mensaje": "Término técnico de engine visible en UI. Traducir a lenguaje musical.",
        "solo_cpp": False,
    },
    "heap_audio_thread": {
        "patron": r'\bnew\b',
        "severidad": "🔴",
        "mensaje": "Posible heap allocation. Verificar si está en processBlock() o audio thread.",
        "solo_cpp": False,
        "solo_archivos": ["processBlock", "prepareToPlay", "AudioAnalyzer."],
    },
    "cout_en_audio": {
        "patron": r'(std::cout|printf|MIXCOACH_LOG)\b',
        "severidad": "🟡",
        "mensaje": "Logging/file I/O. Verificar si está en audio thread.",
        "solo_cpp": False,
        "solo_archivos": ["processBlock", "prepareToPlay", "AudioAnalyzer."],
    },
    "llm_calcula_metricas": {
        "patron": r'(calcular|compute|analyze|calculate).*(FFT|LUFS|crest|RMS)',
        "severidad": "🔴",
        "mensaje": "Posible cálculo de métricas por LLM. Violación de 04_AI_RULES.md §2",
        "solo_cpp": False,
        "solo_archivos": ["AiCoachAdapter", "prompt", "LlmClient"],
    },
    "colores_hardcodeados": {
        "patron": r'Colour\(0x[0-9A-Fa-f]{6,10}\)',
        "severidad": "🟡",
        "mensaje": "Color hardcodeado. Usar MixCoachTheme en vez de Colour() directo (excepto en MixCoachTheme.h).",
        "solo_cpp": False,
        "excluir": ["MixCoachTheme.h"],
    },
    "nombres_tecnicos_ui": {
        "patron": r'(juce::String.*slotIndex|showSlot|setSlot)',
        "severidad": "🟡",
        "mensaje": "Término 'slot' en UI. El usuario no sabe qué es un slot.",
        "solo_cpp": False,
    },
    "cmake_missing_files": {
        "patron": r'^',
        "severidad": "🟡",
        "mensaje": "(check) Verificar si archivos nuevos están en CMakeLists.txt",
        "solo_cpp": False,
        "es_check": True,
    },
}

ARCHIVOS_CORE = [
    "SharedMemory.h", "SharedMemory.cpp",
    "SlotRegistry.h", "SlotRegistry.cpp",
    "SharedData.h", "SharedData.cpp",
    "AudioAnalyzer.h", "AudioAnalyzer.cpp",
    "CoachEngine.h", "CoachEngine.cpp",
    "Types.h", "Constants.h",
]


# ─── Funciones ────────────────────────────────────────────

def get_git_diff_files() -> List[str]:
    """Obtiene archivos modificados en el working tree usando git."""
    import subprocess
    result = subprocess.run(
        ["git", "diff", "--name-only", "HEAD"],
        capture_output=True, text=True, cwd=PROJECT_ROOT
    )
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        capture_output=True, text=True, cwd=PROJECT_ROOT
    )
    files = [f.strip() for f in result.stdout.split("\n") if f.strip()]
    files += [f.strip() for f in untracked.stdout.split("\n") if f.strip()]
    return files


def check_core_files(files: List[str]) -> List[str]:
    """Verifica si se están tocando archivos CORE."""
    warnings = []
    for f in files:
        name = Path(f).name
        if name in ARCHIVOS_CORE:
            warnings.append(f"🔴 CORE: {f} — requiere autorización explícita + tests")
        elif "Common/memory/" in f:
            warnings.append(f"🔴 CORE: {f} — IPC, requiere version check + tests de estrés")
    return warnings


def check_prohibited_patterns(file_path: Path) -> List[str]:
    """Busca patrones prohibidos en un archivo."""
    findings = []
    fname = file_path.name
    
    # Verificar exclusiones por nombre de archivo
    for patron_id, config in PATRONES_PROHIBIDOS.items():
        excluir = config.get("excluir", [])
        if fname in excluir:
            continue
        # Si el patrón tiene solo_archivos, verificar que estén en el path
        solo = config.get("solo_archivos", [])
        if solo:
            fpath_str = str(file_path.as_posix())
            if not any(s in fpath_str for s in solo):
                continue
        # Si es check manual (no regex), lo manejamos aparte
        if config.get("es_check"):
            continue
        
        try:
            content = file_path.read_text(encoding="utf-8", errors="replace")
            matches = re.findall(config["patron"], content, re.IGNORECASE)
            if matches:
                findings.append(
                    f"{config['severidad']} [{patron_id}] {file_path}: "
                    f"{len(matches)} ocurrencias. {config['mensaje']}"
                )
        except Exception as e:
            findings.append(f"⚠️ No se pudo leer {file_path}: {e}")
    return findings


def check_cmakelists(files: List[str]) -> List[str]:
    """Verifica que archivos nuevos estén en CMakeLists.txt."""
    findings = []
    cmake_path = PROJECT_ROOT / "CMakeLists.txt"
    if not cmake_path.exists():
        return []
    try:
        cmake_content = cmake_path.read_text(encoding="utf-8")
        for f in files:
            # Solo verificar .h y .cpp nuevos que no son tests
            if not f.endswith((".h", ".cpp")) or "tests/" in f:
                continue
            fname = Path(f).name
            if fname not in cmake_content:
                # Podría ser una eliminación, verificar si el archivo existe
                if (PROJECT_ROOT / f).exists():
                    findings.append(
                        f"🟡 [cmake_check] {f} — posible archivo nuevo no registrado en CMakeLists.txt"
                    )
    except Exception:
        pass
    return findings


def run_review(files: Optional[List[str]] = None) -> None:
    """Ejecuta la revisión completa del Guardian."""
    if files is None:
        files = get_git_diff_files()

    if not files:
        print("📭 No hay archivos modificados para revisar.")
        return

    print("=" * 60)
    print("  👁️ GUARDIAN DE LA VISIÓN — REVISIÓN AUTOMÁTICA")
    print("=" * 60)
    print()

    # 1. Verificar archivos CORE
    print("📋 1. Archivos CORE tocados:")
    core_warnings = check_core_files(files)
    if core_warnings:
        for w in core_warnings:
            print(f"   {w}")
    else:
        print("   ✅ Ningún archivo CORE tocado.")
    print()

    # 2. Buscar patrones prohibidos
    print("📋 2. Patrones prohibidos:")
    all_findings = []
    for f in files:
        fpath = PROJECT_ROOT / f
        if fpath.exists() and fpath.suffix in {".cpp", ".h", ".hpp", ".c", ".py", ".md"}:
            all_findings.extend(check_prohibited_patterns(fpath))

    # 3. Verificar CMakeLists.txt
    print("📋 3. Archivos en CMakeLists.txt:")
    cmake_findings = check_cmakelists(files)
    if cmake_findings:
        for w in cmake_findings:
            print(f"   {w}")
    else:
        print("   ✅ Todos los archivos parecen registrados.")
    print()

    # 4. Resumen de archivos
    print("📋 4. Resumen de archivos:")
    for f in sorted(files):
        fpath = PROJECT_ROOT / f
        if fpath.exists():
            size = fpath.stat().st_size
            print(f"   {'📝' if f.endswith(('.cpp','.h','.hpp')) else '📄'} {f} ({size} bytes)")
        else:
            print(f"   ❌ {f} (no encontrado)")
    print()

    # 5. Veredicto

    if all_findings:
        for finding in all_findings:
            print(f"   {finding}")
    else:
        print("   ✅ Sin patrones prohibidos detectados.")
    print()

    # 3. Resumen del cambio
    print("📋 3. Resumen de archivos:")
    for f in sorted(files):
        fpath = PROJECT_ROOT / f
        if fpath.exists():
            size = fpath.stat().st_size
            print(f"   {'📝' if f.endswith(('.cpp','.h','.hpp')) else '📄'} {f} ({size} bytes)")
        else:
            print(f"   ❌ {f} (no encontrado)")
    print()

    # 4. Veredicto
    has_critical = any("🔴" in w for w in core_warnings) or \
                   any("🔴" in finding for finding in all_findings)
    has_warnings = any("🟡" in w for w in core_warnings) or \
                   any("🟡" in finding for finding in all_findings)

    print("=" * 60)
    if has_critical:
        print("  🔴 VETADO — Se encontraron violaciones críticas")
        print("  Revisar y corregir antes de mergear.")
        sys.exit(1)
    elif has_warnings:
        print("  🟡 CONDICIONAL — Hay advertencias que revisar")
    else:
        print("  🟢 APROBADO — Sin violaciones detectadas")
    print("=" * 60)


# ─── CLI ──────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="👁️ Guardian de la Visión — Revisión de PR")
    parser.add_argument("--files", nargs="+", help="Archivos a revisar (por defecto: git diff)")
    parser.add_argument("--all", action="store_true", help="Revisar todos los archivos del proyecto")
    args = parser.parse_args()

    files = args.files
    if args.all:
        files = [str(p.relative_to(PROJECT_ROOT))
                 for p in PROJECT_ROOT.rglob("*")
                 if p.is_file() and p.suffix in {".cpp", ".h"}]

    run_review(files)

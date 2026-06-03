#!/usr/bin/env python3
"""
pattern_validator.py — Valida c\u00f3digo fuente contra APPROVED_PATTERNS.md

Uso:
  python scripts/pattern_validator.py              -> Valida todo
  python scripts/pattern_validator.py --verbose     -> Muestra detalles
  python scripts/pattern_validator.py --list-only   -> Solo lista patrones

Exit code: 0 = todos los checks pasan, 1 = alg\u00fan check fall\u00f3
"""

import os
import re
import sys
import fnmatch

# Forzar salida UTF-8 segura en Windows
try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except (AttributeError, ValueError):
    pass

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIR   = os.path.join(PROJECT_ROOT, "Source")

VERBOSE = "--verbose" in sys.argv
LIST_ONLY = "--list-only" in sys.argv

# Colores ANSI (vacios en Windows sin terminal compatible)
class C:
    GREEN  = "\033[92m" if os.name != "nt" else ""
    RED    = "\033[91m" if os.name != "nt" else ""
    YELLOW = "\033[93m" if os.name != "nt" else ""
    CYAN   = "\033[96m" if os.name != "nt" else ""
    GRAY   = "\033[90m" if os.name != "nt" else ""
    BOLD   = "\033[1m"  if os.name != "nt" else ""
    RESET  = "\033[0m"  if os.name != "nt" else ""

# ─── Helpers ────────────────────────────────────────────────────────────────
results = []

def check(name, passed, detail=""):
    results.append((name, passed, detail))
    icon = f"{C.GREEN}OK{C.RESET}" if passed else f"{C.RED}FAIL{C.RESET}"
    print(f"  [{icon}] {name}")
    if not passed and detail:
        for line in detail.strip().split("\n"):
            print(f"       {C.RED}{line}{C.RESET}")
    if VERBOSE and passed and detail:
        for line in detail.strip().split("\n"):
            print(f"       {C.GRAY}{line}{C.RESET}")

def get_files(pattern, base_dir=SOURCE_DIR):
    matches = []
    for root, dirs, files in os.walk(base_dir):
        for f in files:
            if fnmatch.fnmatch(f, pattern):
                matches.append(os.path.join(root, f))
    return sorted(matches)


def has_processblock(filepath):
    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
        return bool(re.search(r'\bprocessBlock\b', content))
    except Exception:
        return False

def is_in(path, segment):
    return f"/{segment}/" in path.replace("\\", "/")

def lines_within_processblock(filepath):
    """Generator que produce (nro_linea, texto) solo dentro de processBlock()."""
    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except Exception:
        return
    
    in_pb = False
    depth = 0
    entered = False  # True despues del primer { del cuerpo
    for i, line in enumerate(lines, 1):
        if re.search(r'\bprocessBlock\b', line):
            in_pb = True
            depth = 0
            entered = False
        if in_pb:
            depth += line.count('{') - line.count('}')
            if depth > 0:
                entered = True
            if depth <= 0 and entered:
                in_pb = False
                continue
            if entered:
                yield i, line

# ═══════════════════════════════════════════════════════════════════════════
#  1. PATRONES PROHIBIDOS
# ═══════════════════════════════════════════════════════════════════════════

def check_px1():
    """PX1: Heap allocation en audio thread."""
    cpp_files = [f for f in get_files("*.cpp") if has_processblock(f)]
    issues = []

    for fp in cpp_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
        except Exception:
            continue

        # `new` keyword (heap alloc) dentro de processBlock
        for lineno, line in lines_within_processblock(fp):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            # Match `new Tipo(` o `new Tipo[` - evita `newValue`, `renew`
            if re.search(r'\bnew\s+(?:[a-zA-Z_][a-zA-Z0-9_]*::)*[A-Z][a-zA-Z0-9_]*\s*[\(\[;]', stripped):
                # Excepciones: return new, delete, new (nothrow), ->new
                if not re.search(r'\b(return|delete|nothrow)\s+new|->\s*new', stripped):
                    issues.append(f"{rel}:{lineno}  Heap alloc 'new': {stripped[:80]}")

        # std::vector LOCAL (no miembro) dentro de processBlock
        pb_seen = False
        pb_depth = 0
        for i, line in enumerate(lines, 1):
            if re.search(r'\bprocessBlock\b', line):
                pb_seen = True
                pb_depth = 0
            if pb_seen:
                pb_depth += line.count('{') - line.count('}')
                if pb_depth <= 0:
                    pb_seen = False
                    continue
                if pb_depth > 0:
                    stripped = line.strip()
                    if re.search(r'\bstd::vector\s*<', stripped) and not stripped.startswith("//"):
                        # Miembro de clase: empieza con std::vector (columna 0+espacios)
                        is_member = bool(re.search(r'^\s*(mutable\s+)?std::vector', stripped))
                        if not is_member:
                            issues.append(f"{rel}:{i}  std::vector local en processBlock: {stripped[:80]}")

    detail = "\n".join(issues[:10])
    if len(issues) > 10:
        detail += f"\n      ... y {len(issues) - 10} mas"
    check("PX1: Heap alloc en audio thread", len(issues) == 0, detail)


def check_px2():
    """PX2: Logica de UI en archivos Engine/."""
    engine_files = [f for f in get_files("*.h") + get_files("*.cpp") if is_in(f, "engine")]
    issues = []

    for fp in engine_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
        except Exception:
            continue

        for inc in re.findall(r'#include\s+["<].*UI.*[">]', content):
            issues.append(f"{rel}  Include UI: {inc.strip()}")
        for ref in set(re.findall(r'\b(AnalyzersPanel|CoachChat|MixCoachTheme|VUMeter)\b', content)):
            issues.append(f"{rel}  Referencia UI: {ref}")

    check("PX2: UI en Engine/", len(issues) == 0, "\n".join(issues[:10]))


def check_px3():
    """PX3: Logica DSP en archivos UI/."""
    ui_files = [f for f in get_files("*.cpp") if is_in(f, "UI")]
    dsp_kw = [
        r'\bcomputeFFT\b', r'\bcomputeSpectrum\b', r'\banalyzePhase\b',
        r'\bcomputeRMS\b', r'\bcomputePeak\b', r'\bcomputeCorrelation\b',
        r'\bLoudnessMeter\b', r'\bcreateFileMapping\b', r'\bOpenFileMapping\b',
        r'\bsyncFromShared\b', r'\bforceFullSync\b',
    ]
    issues = []

    for fp in ui_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
        except Exception:
            continue
        for i, line in enumerate(lines, 1):
            s = line.strip()
            if s.startswith("//") or s.startswith("*"):
                continue
            if re.search(r'^\s*(public|private|protected|class|struct)\b', s):
                continue
            for kw in dsp_kw:
                if re.search(kw, s):
                    issues.append(f"{rel}:{i}  DSP en UI: {s[:80]}")
                    break

    check("PX3: Audio/DSP en UI/", len(issues) == 0, "\n".join(issues[:10]))


def check_px4():
    """PX4: Colores de bus hardcodeados."""
    cpp_files = get_files("*.cpp") + get_files("*.h")
    # Colores exactos de Constants.h (case-insensitive)
    bus_colors = [c.lower() for c in [
        "0xFF8B5CF6", "0xFF3B82F6", "0xFFF97316", "0xFF10B981",
        "0xFFEC4899", "0xFF14B8A6",
        "0xFF7C3AED", "0xFF00B4D8", "0xFF00E5FF",
    ]]
    issues = []

    for fp in cpp_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        if "Constants.h" in rel or "MixCoachTheme" in rel:
            continue
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
        except Exception:
            continue
        for i, line in enumerate(lines, 1):
            s = line.strip()
            if s.startswith("//") or s.startswith("*"):
                continue
            lower = s.lower()
            for color in bus_colors:
                if color in lower:
                    if not re.search(r'\b(const|kBusColourARGB|busColour)\b', lower):
                        issues.append(f"{rel}:{i}  Color hardcodeado: {color} en: {s[:80]}")
                        break

    check("PX4: Colores de bus hardcodeados", len(issues) == 0, "\n".join(issues[:10]))


def check_px5():
    """PX5: SharedSlotEntry sin incrementar version."""
    shm_h = os.path.join(SOURCE_DIR, "Common/memory/SharedMemory.h")
    if not os.path.exists(shm_h):
        check("PX5: SharedSlotEntry version", True, "Archivo no encontrado, skip")
        return
    try:
        with open(shm_h, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception:
        check("PX5: SharedSlotEntry version", True, "No se pudo leer")
        return

    entry = re.search(r'struct\s+SharedSlotEntry\b', content)
    version = re.search(r'kCurrentStructVersion\s*=\s*(\d+)', content)
    if entry and version:
        check("PX5: SharedSlotEntry version", True,
              f"kCurrentStructVersion = {version.group(1)} (OK)")
    else:
        check("PX5: SharedSlotEntry version", False,
              "No se encontro SharedSlotEntry o kCurrentStructVersion")


def check_px6():
    """PX6: Llamadas OS/DAW en audio thread."""
    cpp_files = [f for f in get_files("*.cpp") if has_processblock(f)]
    blocked = [
        r'\bjuce::File\b', r'\bFile::getSpecialLocation\b',
        r'\bLogHelper::writeToLog\b',
        r'\bjuce::Time::getMillisecondCounter\b',
    ]
    issues = []

    for fp in cpp_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        for lineno, line in lines_within_processblock(fp):
            s = line.strip()
            if s.startswith("//") or s.startswith("*"):
                continue
            for pat in blocked:
                if re.search(pat, s):
                    issues.append(f"{rel}:{lineno}  OS call: {s[:80]}")
                    break

    check("PX6: OS/DAW calls en audio thread", len(issues) == 0, "\n".join(issues[:10]))


def check_px7():
    """PX7: Mutex/Spinlock en audio thread."""
    cpp_files = [f for f in get_files("*.cpp") if has_processblock(f)]
    lock_pat = [
        r'\bacquireLock\b', r'\bstd::lock_guard\b', r'\bstd::mutex\b',
        r'\bstd::unique_lock\b', r'\bstd::scoped_lock\b',
    ]
    issues = []

    for fp in cpp_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        for lineno, line in lines_within_processblock(fp):
            s = line.strip()
            if s.startswith("//") or s.startswith("*"):
                continue
            # Ignorar declaraciones de miembros
            if re.search(r'^\s*(mutable\s+)?(std::mutex|SpinLock)\b', s):
                continue
            for pat in lock_pat:
                if re.search(pat, s):
                    issues.append(f"{rel}:{lineno}  Lock en audio thread: {s[:80]}")
                    break

    check("PX7: Locks en audio thread", len(issues) == 0, "\n".join(issues[:10]))


# ═══════════════════════════════════════════════════════════════════════════
#  2. PATRONES PERMITIDOS
# ═══════════════════════════════════════════════════════════════════════════

def check_p1():
    """P1: Lazy Initialization Segura."""
    proc_files = get_files("PluginProcessor*.cpp") + get_files("PluginProcessor*.h")
    findings = []

    for fp in proc_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
        except Exception:
            continue

        has_guard = bool(re.search(r'if\s*\([^)]*nullptr[^)]*\)\s*(?:return|continue)', content))
        has_try = bool(re.search(r'try\s*\{[^}]*catch\s*\(', content, re.DOTALL))
        has_shm_in_ctor = bool(re.search(r'SharedData::getInstance\(\)', content))

        guard_s = "OK" if has_guard else "MISSING"
        try_s = "OK" if has_try else "MISSING"
        ctor_s = "WARN" if has_shm_in_ctor else "OK"
        findings.append(f"{rel}: guard={guard_s} try/catch={try_s} ctor_SharedData={ctor_s}")

    passed = all("MISSING" not in f and "WARN" not in f for f in findings)
    check("P1: Lazy Initialization", passed, "\n".join(findings) if findings else "No files found")


def check_p6():
    """P6: Two-Phase Spinlock con _mm_pause()."""
    shm_files = get_files("SharedMemory*.cpp")
    issues = []

    for fp in shm_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
        except Exception:
            continue

        has_pause = bool(re.search(r'_mm_pause\(\)', content))
        has_sleep = bool(re.search(r'Sleep\s*\(\s*0\s*\)', content))

        if has_pause and has_sleep:
            issues.append(f"{rel}: Two-phase spinlock OK (_mm_pause + Sleep(0))")
        elif has_pause and not has_sleep:
            issues.append(f"{rel}: Solo _mm_pause(), falta Sleep(0)")
        elif has_sleep and not has_pause:
            issues.append(f"{rel}: Sleep(0) sin _mm_pause() (desaconsejado)")
        else:
            issues.append(f"{rel}: No se encontro spinlock")

    passed = any("OK" in i for i in issues) and not any("desaconsejado" in i for i in issues)
    check("P6: Two-Phase Spinlock", passed, "\n".join(issues))


def check_p8():
    """P8: Timer con Throttling (tickCounter_ % N dentro de timerCallback)."""
    editor_files = get_files("PluginEditor*.cpp") + get_files("*Editor*.cpp")
    findings = []

    for fp in editor_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
        except Exception:
            continue

        has_timer = bool(re.search(r'\btimerCallback\b', content))
        has_throttle = bool(re.search(r'tickCounter_\s*%\s*\d+', content))

        if has_timer:
            if has_throttle:
                findings.append(f"{rel}: Timer con throttling OK (tickCounter_ % N)")
            else:
                findings.append(f"{rel}: Timer sin throttling")

    passed = len(findings) > 0
    check("P8: Timer Throttling", passed, "\n".join(findings) if findings else "No Editor files found")


# ═══════════════════════════════════════════════════════════════════════════
#  3. PATRONES DESACONSEJADOS
# ═══════════════════════════════════════════════════════════════════════════

def check_d1():
    """D1: File I/O en processBlock()."""
    cpp_files = [f for f in get_files("*.cpp") if has_processblock(f)]
    io_pat = [
        r'\bjuce::File\s+\w+\s*[=;\(]', r'\.create\(\)', r'\.createDirectory\(\)',
        r'\.deleteFile\(\)', r'\.loadFileAsData\(\)', r'\.loadFileAsString\(\)',
        r'\bstd::ofstream\b', r'\bstd::ifstream\b', r'\bfopen\b', r'\bfwrite\b',
        r'\bsaveSlotToBackupFile\b', r'\bloadSlotsFromBackupFiles\b',
    ]
    issues = []

    for fp in cpp_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        for lineno, line in lines_within_processblock(fp):
            s = line.strip()
            if s.startswith("//") or s.startswith("*"):
                continue
            for pat in io_pat:
                if re.search(pat, s):
                    issues.append(f"{rel}:{lineno}  File I/O: {s[:80]}")
                    break

    check("D1: File I/O en processBlock", len(issues) == 0, "\n".join(issues[:10]))


def check_d2():
    """D2: Sleep(0) sin _mm_pause()."""
    cpp_files = get_files("*.cpp")
    issues = []

    for fp in cpp_files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
        except Exception:
            continue
        has_sleep = bool(re.search(r'Sleep\s*\(\s*0\s*\)', content))
        has_pause = bool(re.search(r'_mm_pause\(\)', content))
        if has_sleep and not has_pause:
            issues.append(f"{rel}: Sleep(0) sin _mm_pause()")

    check("D2: Sleep(0) sin _mm_pause()", len(issues) == 0, "\n".join(issues))


# ═══════════════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════════════

def run_all():
    sep = "=" * 60
    print(f"\n{C.BOLD}{C.CYAN}##{sep}{C.RESET}")
    print(f"{C.BOLD}{C.CYAN}  PATTERN VALIDATOR -- Verificando contra APPROVED_PATTERNS.md{C.RESET}")
    print(f"{C.BOLD}{C.CYAN}  Proyecto: {PROJECT_ROOT}{C.RESET}")
    print(f"{C.BOLD}{C.CYAN}##{sep}{C.RESET}\n")

    print(f"\n{C.BOLD}{C.RED}RED PATRONES PROHIBIDOS{C.RESET}")
    print(f"{C.GRAY}--- Estos patrones NUNCA deben aparecer en el codigo ---{C.RESET}")
    check_px1()
    check_px2()
    check_px3()
    check_px4()
    check_px5()
    check_px6()
    check_px7()

    print(f"\n{C.BOLD}{C.GREEN}GREEN PATRONES PERMITIDOS{C.RESET}")
    print(f"{C.GRAY}--- Verificando que los patrones aprobados se usan correctamente ---{C.RESET}")
    check_p1()
    check_p6()
    check_p8()

    print(f"\n{C.BOLD}{C.YELLOW}YELLOW PATRONES DESACONSEJADOS{C.RESET}")
    print(f"{C.GRAY}--- Estos patrones deberian evitarse ---{C.RESET}")
    check_d1()
    check_d2()

    total = len(results)
    passed = sum(1 for r in results if r[1])
    failed = total - passed

    print(f"\n{C.BOLD}{C.CYAN}##{sep}{C.RESET}")
    print(f"{C.BOLD}{C.CYAN}  RESUMEN{C.RESET}")
    print(f"{C.BOLD}{C.CYAN}##{sep}{C.RESET}")
    print(f"  Total checks: {total}")
    print(f"  {C.GREEN}Passed: {passed}{C.RESET}")
    print(f"  {C.RED if failed > 0 else ''}Failed: {failed}{C.RESET}")
    for name, ok, _ in results:
        if not ok:
            print(f"    {C.RED}* {name}{C.RESET}")

    print()
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    if LIST_ONLY:
        print("Patrones disponibles para validacion:")
        print("  RED PX1 - Heap alloc en audio thread")
        print("  RED PX2 - Logica UI en archivos Engine/")
        print("  RED PX3 - Logica Audio en archivos UI/")
        print("  RED PX4 - Colores de bus hardcodeados")
        print("  RED PX5 - SharedSlotEntry sin incrementar version")
        print("  RED PX6 - Llamadas OS/DAW en audio thread")
        print("  RED PX7 - Mutex/Spinlock en audio thread")
        print("  GREEN P1  - Lazy Initialization Segura")
        print("  GREEN P6  - Two-Phase Spinlock")
        print("  GREEN P8  - Timer Throttling")
        print("  YELLOW D1 - File I/O en processBlock")
        print("  YELLOW D2 - Sleep(0) sin _mm_pause()")
        sys.exit(0)
    sys.exit(run_all())

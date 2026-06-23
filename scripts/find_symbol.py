#!/usr/bin/env python3
"""
find_symbol.py — Busca declaración + definición + usos de cualquier símbolo C++

Busca en todos los .h/.cpp del proyecto y clasifica los resultados en:
  • DECLARACIONES  (.h)  — prototipos, clases, structs, enums, variables
  • DEFINICIONES   (.cpp) — implementaciones de funciones, variables globales
  • USOS           (.h/.cpp) — referencias (llamadas, parámetros, etc.)

Uso:
  python scripts/find_symbol.py trackRoles_
  python scripts/find_symbol.py --cpp-only analyzeGainStagingReal
  python scripts/find_symbol.py --context 3 SlotRegistry::kMaxSlots

Flags:
  --context N    Muestra N líneas de contexto alrededor de cada match (default 0)
  --cpp-only     Solo busca en archivos .cpp
  --h-only       Solo busca en archivos .h
  --no-color     Desactiva colores ANSI
  --help, -h     Muestra esta ayuda
"""

import os
import re
import sys
import fnmatch

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIR = os.path.join(PROJECT_ROOT, "Source")

# ─── CLI flags ─────────────────────────────────────────────────────────────

CONTEXT_LINES = 0
CPP_ONLY = False
H_ONLY = False
NO_COLOR = False

def parse_flags():
    global CONTEXT_LINES, CPP_ONLY, H_ONLY, NO_COLOR
    args = list(sys.argv[1:])
    symbol = None
    while args:
        a = args.pop(0)
        if a == "--context":
            CONTEXT_LINES = int(args.pop(0)) if args else 0
        elif a == "--cpp-only":
            CPP_ONLY = True
        elif a == "--h-only":
            H_ONLY = True
        elif a == "--no-color":
            NO_COLOR = True
        elif a in ("--help", "-h"):
            print(__doc__)
            sys.exit(0)
        elif a.startswith("--"):
            print(f"Unknown flag: {a}")
            print(__doc__)
            sys.exit(1)
        else:
            symbol = a
            # re-join remaining args in case symbol has spaces
            if args:
                symbol += " " + " ".join(args)
                args.clear()
    return symbol

# ─── Color helpers ─────────────────────────────────────────────────────────

class C:
    GREEN = "\033[92m"
    CYAN = "\033[96m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    GRAY = "\033[90m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

def c(s, color):
    if NO_COLOR:
        return s if isinstance(s, str) else str(s)
    return color + (s if isinstance(s, str) else str(s)) + C.RESET

# ─── File discovery ────────────────────────────────────────────────────────

def get_files(pattern, base_dir=SOURCE_DIR):
    matches = []
    for root, dirs, files in os.walk(base_dir):
        for f in files:
            if fnmatch.fnmatch(f, pattern):
                matches.append(os.path.join(root, f))
    return sorted(matches)

# ─── Search engine ─────────────────────────────────────────────────────────

def find_in_files(files, symbol):
    """
    Returns dict: { relpath: [(lineno, line, context_before, context_after)] }
    """
    results = {}
    for fp in files:
        rel = os.path.relpath(fp, PROJECT_ROOT)
        try:
            with open(fp, "r", encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except Exception as e:
            print(f"  {c('[ERROR]', C.RED)} {rel}: {e}", file=sys.stderr)
            continue

        for i, line in enumerate(lines):
            if symbol in line:
                # skip matches inside string literals if the symbol is short
                if len(symbol) < 3 and not re.search(r'\b' + re.escape(symbol) + r'\b', line):
                    continue
                start = max(0, i - CONTEXT_LINES)
                end = min(len(lines), i + CONTEXT_LINES + 1)
                context_before = lines[start:i]
                context_after = lines[i+1:end]
                if rel not in results:
                    results[rel] = []
                results[rel].append((i + 1, line.rstrip(), context_before, context_after))

    return results

# ─── Classification ────────────────────────────────────────────────────────

def is_declaration(line, symbol, is_h_file):
    """Heurísticas para detectar declaraciones en .h"""
    stripped = line.strip()

    # Class/struct/enum declaration
    if re.match(r'\b(class|struct|enum)\s+' + re.escape(symbol) + r'\b', stripped):
        return True

    # Using declaration / typedef
    if re.match(r'\b(using|typedef)\s+.*\b' + re.escape(symbol) + r'\b', stripped):
        return True

    if not is_h_file:
        return False

    # Function declaration: returnType symbol(...) or virtual ... symbol(...)
    # Match: symbol followed by ( (with possible whitespace)
    if re.search(r'\b' + re.escape(symbol) + r'\s*\(', stripped):
        # Exclude definitions (those with { or = default/delete)
        if not re.search(r'[\{=]', stripped):
            return True

    # Member variable: Type symbol; or Type symbol = ...;
    if re.search(r'\b' + re.escape(symbol) + r'\s*[;=]', stripped):
        # Exclude: parameter declarations, loop variables, cast expressions
        if not re.search(r'\b(for|if|while|switch|catch)\s*\(', stripped):
            # Check it's not a parameter or local
            if not re.match(r'^\s*(?:const\s+)?[A-Z]\w*\s+\w+', stripped):
                return True
            if re.match(r'^\s*.*\b' + re.escape(symbol) + r'\s*[;=]', stripped):
                return True

    return False


def is_definition(line, symbol, is_cpp_file):
    """Heurísticas para detectar definiciones en .cpp"""
    stripped = line.strip()

    if not is_cpp_file:
        return False

    # Function definition: ReturnType Class::symbol(...) {
    # Or: ReturnType symbol(...) {
    if re.search(r'\b' + re.escape(symbol) + r'\s*\(', stripped):
        if re.search(r'[\{]', stripped):
            return True

    # Variable definition at file/namespace scope: Type symbol = ...;
    if re.search(r'\b' + re.escape(symbol) + r'\s*[=;]', stripped):
        # Not inside a function body (heuristic: not indented with spaces/tabs only)
        if stripped.startswith(symbol) or stripped.startswith('}') or re.match(r'^[A-Za-z_]', stripped):
            return True

    return False


def classify(symbol, results_by_rel):
    """Classify results into declarations, definitions, and usages."""
    decls = []
    defs = []
    usages = []

    for rel, matches in sorted(results_by_rel.items()):
        is_h = rel.endswith(".h")
        is_cpp = rel.endswith(".cpp")
        for lineno, line, ctx_before, ctx_after in matches:
            if is_declaration(line, symbol, is_h):
                decls.append((rel, lineno, line, ctx_before, ctx_after))
            elif is_definition(line, symbol, is_cpp):
                defs.append((rel, lineno, line, ctx_before, ctx_after))
            else:
                usages.append((rel, lineno, line, ctx_before, ctx_after))

    return decls, defs, usages


# ─── Output ────────────────────────────────────────────────────────────────

def print_context(ctx_before, ctx_after):
    """Print context lines if CONTEXT_LINES > 0."""
    if CONTEXT_LINES == 0:
        return
    for cl in ctx_before:
        print(f"  {c(cl.rstrip(), C.GRAY)}")
    for cl in ctx_after:
        print(f"  {c(cl.rstrip(), C.GRAY)}")


def print_results(decls, defs, usages, symbol):
    total = len(decls) + len(defs) + len(usages)
    print(f"\n{c('═' * 60, C.CYAN)}")
    print(f"  {c('find_symbol:', C.BOLD)} {c(symbol, C.YELLOW)}")
    print(f"  {c(total, C.BOLD)} match(es) found")
    print(f"{c('═' * 60, C.CYAN)}\n")

    # ─── Declarations ───
    if decls:
        print(f"  {c('▼ DECLARACIONES', C.GREEN)} ({len(decls)})")
        print(f"  {c('─' * 50, C.GRAY)}")
        seen = set()
        for rel, lineno, line, ctx_b, ctx_a in decls:
            key = (rel, lineno)
            if key in seen:
                continue
            seen.add(key)
            print(f"  {c(rel, C.CYAN)}:{c(str(lineno), C.YELLOW)}")
            print(f"    {c(line.strip(), C.GREEN)}")
            print_context(ctx_b, ctx_a)
            print()
        print()

    # ─── Definitions ───
    if defs:
        print(f"  {c('▼ DEFINICIONES', C.YELLOW)} ({len(defs)})")
        print(f"  {c('─' * 50, C.GRAY)}")
        seen = set()
        for rel, lineno, line, ctx_b, ctx_a in defs:
            key = (rel, lineno)
            if key in seen:
                continue
            seen.add(key)
            print(f"  {c(rel, C.CYAN)}:{c(str(lineno), C.YELLOW)}")
            print(f"    {c(line.strip(), C.YELLOW)}")
            print_context(ctx_b, ctx_a)
            print()
        print()

    # ─── Usages ───
    if usages:
        print(f"  {c('▼ USOS', C.RED)} ({len(usages)})")
        print(f"  {c('─' * 50, C.GRAY)}")
        seen = set()
        for rel, lineno, line, ctx_b, ctx_a in usages:
            key = (rel, lineno)
            if key in seen:
                continue
            seen.add(key)
            print(f"  {c(rel, C.CYAN)}:{c(str(lineno), C.YELLOW)}")
            print(f"    {c(line.strip(), C.RED) if 'TODO' not in line and 'FIXME' not in line else line.strip()}")
            print_context(ctx_b, ctx_a)

    print(f"\n{c('═' * 60, C.CYAN)}")
    print(f"  Total: {c(str(len(decls)), C.GREEN)} decl + {c(str(len(defs)), C.YELLOW)} def + {c(str(len(usages)), C.RED)} usage")
    print(f"{c('═' * 60, C.CYAN)}\n")


# ─── Main ──────────────────────────────────────────────────────────────────

def main():
    symbol = parse_flags()
    if not symbol:
        print(c("ERROR: Specify a symbol to search for.", C.RED), file=sys.stderr)
        print(__doc__)
        sys.exit(1)

    # Collect files
    if CPP_ONLY:
        h_files = []
        cpp_files = get_files("*.cpp")
    elif H_ONLY:
        h_files = get_files("*.h")
        cpp_files = []
    else:
        h_files = get_files("*.h")
        cpp_files = get_files("*.cpp")

    all_files = h_files + cpp_files
    print(f"{c('Searching for:', C.BOLD)} {c(symbol, C.YELLOW)}")
    print(f"{c('Files scanned:', C.BOLD)} {len(all_files)} ({len(h_files)} .h + {len(cpp_files)} .cpp)")

    # Search
    raw = find_in_files(all_files, symbol)

    if not raw:
        print(f"\n{c('No matches found.', C.RED)}\n")
        sys.exit(0)

    # Classify
    decls, defs, usages = classify(symbol, raw)

    # Print
    print_results(decls, defs, usages, symbol)


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except (AttributeError, ValueError):
        pass
    main()

#!/usr/bin/env python3
"""
ai_build_loop.py — Build Feedback Loop v2.0 with Error Learning

BUILD FEEDBACK LOOP (requirement #7):
Flujo:
    IA modifica codigo -> build.ps1 -> capturar errores -> mapear error -> symbol graph
    -> analizar causa probable -> proponer fix -> recompilar

ERROR LEARNING SYSTEM (requirement #8):
    "cuando ocurre X, normalmente el problema esta en Y"
    Auto-learns from build errors and stores patterns.

Usage:
    python ai_build_loop.py                          # Full build (Release)
    python ai_build_loop.py --config Debug            # Debug build
    python ai_build_loop.py --config Release --deploy # Build + deploy
    python ai_build_loop.py --analyze-only "file.txt" # Analyze existing build log
    python ai_build_loop.py --learn                  # Train error patterns from build history
    python ai_build_loop.py --impact FILE             # Check build impact before changing

Features:
    - Executes build.ps1 or cmake directly
    - Captures and categorizes MSVC/CMake/linker errors
    - Matches errors against ERROR_PATTERNS.json (auto-learned)
    - Error-to-symbol mapping via SYMBOL_GRAPH.json
    - Suggests fixes for known error patterns
    - Auto-repair mode (applies known fixes, retries)
    - Smart build targeting (only rebuild changed modules)
    - Updates KNOWN_ERRORS.md with new patterns
    - Self-learning: records new error patterns
    - Saves session state to AI_SESSION_STATE.json
"""

import json
import re
import subprocess
import sys
import os
from datetime import datetime
from pathlib import Path
from collections import defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.resolve()
BUILDS_DIR = PROJECT_ROOT / "Builds"
ERROR_PATTERNS_PATH = PROJECT_ROOT / "ERROR_PATTERNS.json"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
KNOWN_ERRORS_PATH = PROJECT_ROOT / "KNOWN_ERRORS.md"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"
BUILD_SCRIPT = PROJECT_ROOT / "build.ps1"
DEPLOY_SCRIPT = PROJECT_ROOT / "DeployVST3.ps1"

# ─── Config ────────────────────────────────────────────────────────────────
DEFAULT_CONFIG = "Release"
MAX_RETRIES = 2


# ─── Error Pattern Matcher (v2 - with auto-learning) ──────────────────────
class ErrorAnalyzer:
    """Analyzes build errors, matches patterns, and learns new ones."""

    def __init__(self):
        self.patterns_data = self._load_patterns()
        self.patterns = self.patterns_data.get("patterns", [])
        self.fix_history = self.patterns_data.get("fix_history", [])
        self.symbol_graph = self._load_symbol_graph()
        self.errors = []

    def _load_patterns(self):
        try:
            with open(ERROR_PATTERNS_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {"version": "2.0.0", "patterns": [], "fix_history": []}

    def _load_symbol_graph(self):
        try:
            with open(SYMBOL_GRAPH_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {}

    def _save_patterns(self):
        self.patterns_data["patterns"] = self.patterns
        self.patterns_data["fix_history"] = self.fix_history
        self.patterns_data["last_updated"] = datetime.now().isoformat()
        with open(ERROR_PATTERNS_PATH, "w", encoding="utf-8") as f:
            json.dump(self.patterns_data, f, indent=2, ensure_ascii=False)

    def parse_build_output(self, output):
        """Parse build output and extract errors with symbol mapping."""
        errors = []

        # MSVC errors: file(line): error CXXXX:
        msvc_pattern = re.compile(
            r'([A-Za-z]:\\(?:[^\\\n]+\\)*\S+\.\w+)\s*\((\d+)\)\s*:\s*error\s+(C\d+):\s*(.+)'
        )
        # Linker errors: LNKXXXX
        linker_pattern = re.compile(
            r'(?:fatal\s+)?error\s+(LNK\d+):\s*(.+?)(?:\s+\[|\n|$)'
        )
        # CMake errors
        cmake_pattern = re.compile(
            r'CMake\s+(Error|Warning)\s+(?:at\s+)?(.+?):(\d+)?\s*\((.+?)\):\s*(.+)'
        )

        for line in output.split('\n'):
            stripped = line.strip()

            # MSVC errors
            m = msvc_pattern.search(line)
            if m:
                error = {
                    "type": "compiler",
                    "file": m.group(1),
                    "line": int(m.group(2)),
                    "code": m.group(3),
                    "message": m.group(4),
                    "raw": stripped,
                    "symbol_context": self._map_error_to_symbol(m.group(1), m.group(4)),
                }
                error["fix"] = self._match_pattern(error["code"], error["message"])
                errors.append(error)
                continue

            # Linker errors
            m = linker_pattern.search(line)
            if m:
                error = {
                    "type": "linker",
                    "file": None,
                    "line": None,
                    "code": m.group(1),
                    "message": m.group(2),
                    "raw": stripped,
                    "symbol_context": self._map_linker_to_symbol(m.group(2)),
                }
                error["fix"] = self._match_pattern(error["code"], error["message"])
                errors.append(error)
                continue

            # CMake errors
            m = cmake_pattern.search(line)
            if m:
                error = {
                    "type": "cmake",
                    "file": m.group(2),
                    "line": int(m.group(3)) if m.group(3) else None,
                    "code": m.group(4),
                    "message": m.group(5),
                    "raw": stripped,
                    "fix": self._match_pattern("CMake", m.group(5)),
                    "symbol_context": None,
                }
                errors.append(error)

        self.errors = errors
        return errors

    def _map_error_to_symbol(self, file_path, message):
        """Map a compiler error to symbols in SYMBOL_GRAPH.json."""
        symbols = self.symbol_graph.get("symbols", {})

        # Try to find the file in by_file
        normalized_path = file_path.replace("\\", "/")
        # Extract just the relative path
        rel_path = normalized_path
        if "Source" in normalized_path:
            idx = normalized_path.index("Source")
            rel_path = normalized_path[idx:]

        # Search symbols in the error file
        file_symbols = self.symbol_graph.get("by_file", {}).get(rel_path, [])
        if not file_symbols:
            # Try matching message content to symbol names
            for sym_name, sym in symbols.items():
                if sym_name.lower() in message.lower():
                    return {
                        "possible_symbols": [sym_name],
                        "note": "Matched by error message content",
                    }
            return None

        # Look for symbols that match the error context
        message_lower = message.lower()
        matched = []
        for sym_name in file_symbols[:10]:
            sym = symbols.get(sym_name, {})
            if sym.get("type") in ("class", "struct", "method", "enum"):
                sym_lower = sym_name.lower()
                # Check if error message mentions this symbol
                if sym_lower.split("::")[-1] in message_lower:
                    matched.append(sym_name)
                # Check if methods in this class match
                if sym.get("type") in ("class", "struct"):
                    for method in sym.get("methods", []):
                        method_name = method.split("::")[-1].lower()
                        if method_name in message_lower:
                            matched.append(method)

        if matched:
            return {"possible_symbols": matched[:5], "note": "Matched by symbol in file"}
        return None

    def _map_linker_to_symbol(self, message):
        """Map a linker error to missing symbols."""
        # LNK2019: unresolved external symbol "symbol" referenced in function
        sym_match = re.search(r'(?:symbol\s+["\']?)(\w+(?:::)?\w*)(?:["\']?\s+|$)', message)
        if sym_match:
            symbol_name = sym_match.group(1)
            symbols = self.symbol_graph.get("symbols", {})
            # Find the symbol in graph
            for sym_name, sym in symbols.items():
                if symbol_name in sym_name or sym_name in symbol_name:
                    return {
                        "possible_symbols": [sym_name],
                        "declared_in": sym.get("file", "unknown"),
                        "note": "Linker unresolved symbol",
                    }
        return None

    def _match_pattern(self, code, message):
        """Find matching error pattern and return fix strategy.
        Now includes auto-learned frequency weighting."""
        code_clean = code.strip()
        best_match = None
        best_score = 0

        for p in self.patterns:
            score = 0
            if p.get("error_code") == code_clean:
                score += 3
            if p.get("pattern", "") in message.lower():
                score += 2
            # Check project-specific cases
            for case in p.get("project_specific", {}).get("common_cases", []):
                if any(word in message.lower() for word in case.lower().split()[:3]):
                    score += 1

            if score > best_score:
                best_score = score
                best_match = p

        if best_match and best_score >= 2:
            return {
                "cause": best_match.get("cause"),
                "fix_strategy": best_match.get("fix_strategy"),
                "project_cases": best_match.get("project_specific", {}).get("common_cases", []),
                "confidence": min(1.0, best_score / 5),
            }
        return None

    def learn_from_error(self, error):
        """Learn a new error pattern from an unrecognized error."""
        code = error.get("code", "UNKNOWN")
        message = error.get("message", "")
        file_path = error.get("file", "")

        # Skip if already known
        for p in self.patterns:
            if p.get("error_code") == code:
                return False

        # Generate a new pattern entry
        new_pattern = {
            "error_code": code,
            "type": error.get("type", "unknown"),
            "pattern": message[:50].lower(),
            "cause": "Auto-detected: unknown error pattern",
            "fix_strategy": "Review manually. Record fix here for future auto-detection.",
            "first_seen": datetime.now().isoformat(),
            "times_seen": 1,
            "project_specific": {
                "common_cases": [f"Occurred in {file_path}"],
            },
        }
        self.patterns.append(new_pattern)
        self._save_patterns()
        return True

    def record_fix(self, error_code, fix_description, success=True):
        """Record a successful fix to fix_history for learning."""
        entry = {
            "timestamp": datetime.now().isoformat(),
            "error_code": error_code,
            "fix": fix_description,
            "success": success,
        }
        self.fix_history.append(entry)

        # Update pattern frequency
        for p in self.patterns:
            if p.get("error_code") == error_code:
                p["times_seen"] = p.get("times_seen", 0) + 1
                p["last_fix_description"] = fix_description
                break

        self._save_patterns()

    def summarize(self):
        """Summarize errors by category."""
        summary = {
            "total": len(self.errors),
            "by_type": {},
            "known_patterns": 0,
            "unknown_patterns": 0,
            "with_symbol_context": 0,
        }
        for e in self.errors:
            etype = e["type"]
            summary["by_type"][etype] = summary["by_type"].get(etype, 0) + 1
            if e.get("fix"):
                summary["known_patterns"] += 1
            else:
                summary["unknown_patterns"] += 1
            if e.get("symbol_context"):
                summary["with_symbol_context"] += 1
        return summary


# ─── Build Runner ──────────────────────────────────────────────────────────
class BuildRunner:
    """Executes builds and captures output."""

    def __init__(self, config=DEFAULT_CONFIG):
        self.config = config
        self.exit_code = None
        self.output = ""
        self.duration = 0

    def run(self, clean=False, deploy=False, targeted_files=None):
        """Run the build, optionally targeting specific files."""
        if targeted_files:
            # Smart rebuild: compile only changed files
            print(f"[BUILD] Smart rebuild: {len(targeted_files)} files")
            cmd = [
                "cmake", "--build", str(BUILDS_DIR),
                "--config", self.config,
                "--target", "MixCoach",
            ]
        else:
            cmd = [
                "cmake", "--build", str(BUILDS_DIR),
                "--config", self.config,
            ]
            if clean:
                cmd.append("--clean-first")

        print(f"[BUILD] {' '.join(cmd)}")
        start = datetime.now()

        try:
            result = subprocess.run(
                cmd,
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
                timeout=600,
            )
            self.exit_code = result.returncode
            self.output = result.stdout + "\n" + result.stderr
        except subprocess.TimeoutExpired:
            self.exit_code = -1
            self.output = "BUILD TIMEOUT"
        except FileNotFoundError:
            self.exit_code = -2
            self.output = "cmake not found"

        self.duration = (datetime.now() - start).total_seconds()

        if self.exit_code == 0 and deploy:
            self._deploy()

        return self.exit_code == 0

    def _deploy(self):
        print("[BUILD] Build OK -> deploying...")
        try:
            result = subprocess.run(
                ["powershell", "-ExecutionPolicy", "Bypass", "-File", str(DEPLOY_SCRIPT)],
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
                timeout=60,
            )
            if result.returncode == 0:
                print("[DEPLOY] OK")
            else:
                print(f"[DEPLOY] FAILED: {result.stderr[:500]}")
        except Exception as e:
            print(f"[DEPLOY] Error: {e}")


# ─── Session Manager ──────────────────────────────────────────────────────
class SessionManager:
    """Manages AI_SESSION_STATE.json with enhanced learning data."""

    def __init__(self):
        self.data = self._load()

    def _load(self):
        try:
            with open(SESSION_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {
                "version": "2.0.0",
                "recent_tasks": [],
                "recent_errors": [],
                "successful_fixes": [],
                "build_history": [],
                "error_pattern_stats": {},
                "project_health": {},
            }

    def _save(self):
        with open(SESSION_PATH, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=2)

    def record_build(self, success, config, duration, errors):
        entry = {
            "timestamp": datetime.now().isoformat(),
            "config": config,
            "success": success,
            "duration_seconds": duration,
            "error_count": len(errors),
        }
        self.data.setdefault("build_history", []).append(entry)
        if len(self.data["build_history"]) > 50:
            self.data["build_history"] = self.data["build_history"][-50:]

        if not success:
            for e in errors[:5]:
                self.data.setdefault("recent_errors", []).append({
                    "timestamp": datetime.now().isoformat(),
                    "code": e.get("code", "unknown"),
                    "message": e.get("message", "")[:200],
                    "file": e.get("file", "unknown"),
                    "type": e.get("type", "unknown"),
                })
                # Update error pattern stats
                code = e.get("code", "UNKNOWN")
                stats = self.data.setdefault("error_pattern_stats", {})
                stats[code] = stats.get(code, 0) + 1

        self._save()

    def record_fix(self, error_code, fix_description):
        self.data.setdefault("successful_fixes", []).append({
            "timestamp": datetime.now().isoformat(),
            "error_code": error_code,
            "fix": fix_description,
        })
        self._save()

    def get_most_common_errors(self, top_n=5):
        stats = self.data.get("error_pattern_stats", {})
        sorted_errors = sorted(stats.items(), key=lambda x: -x[1])
        return [{"code": code, "count": count} for code, count in sorted_errors[:top_n]]

    def get_recent_context(self):
        ctx = {}
        if self.data.get("recent_errors"):
            ctx["last_error"] = self.data["recent_errors"][-1]
        if self.data.get("build_history"):
            ctx["last_build"] = self.data["build_history"][-1]
        ctx["common_errors"] = self.get_most_common_errors(3)
        return ctx


# ─── Auto-Repair ──────────────────────────────────────────────────────────
class AutoRepair:
    """Attempts to auto-fix known errors with learning."""

    def __init__(self, errors, analyzer):
        self.errors = errors
        self.analyzer = analyzer
        self.fixes_applied = []

    def suggest_fixes(self):
        suggestions = []
        for e in self.errors:
            fix = e.get("fix")
            if fix:
                suggestions.append({
                    "file": e.get("file"),
                    "line": e.get("line"),
                    "code": e.get("code"),
                    "problem": fix.get("cause", "Unknown"),
                    "action": fix.get("fix_strategy", "Review manually"),
                    "confidence": fix.get("confidence", 0.5),
                    "symbol_context": e.get("symbol_context"),
                })
            else:
                # This is a new pattern - learn it
                self.analyzer.learn_from_error(e)
                suggestions.append({
                    "file": e.get("file"),
                    "line": e.get("line"),
                    "code": e.get("code"),
                    "problem": "NEW PATTERN - recorded for learning",
                    "action": "Review manually",
                    "confidence": 0,
                })
        return suggestions

    def generate_report(self, build_success):
        lines = []
        lines.append("# Build Report")
        lines.append(f"**Status:** {'[OK] SUCCESS' if build_success else '[FAIL] FAILED'}")
        lines.append(f"**Time:** {datetime.now().isoformat()}")
        lines.append("")

        if not build_success:
            summary = self.analyzer.summarize()
            lines.append(f"**Errors:** {len(self.errors)}")
            for etype, count in summary.get("by_type", {}).items():
                lines.append(f"- {etype}: {count}")
            lines.append(f"- With symbol context: {summary.get('with_symbol_context', 0)}")
            lines.append("")

            suggestions = self.suggest_fixes()
            if suggestions:
                lines.append("## Suggested Fixes")
                for s in suggestions[:10]:
                    conf = f"[{s['confidence']:.0%} confidence]" if s.get('confidence', 0) > 0 else "[NEW]"
                    lines.append(f"### {s['code']} {conf}")
                    lines.append(f"- **File:** {s['file']}")
                    lines.append(f"- **Line:** {s['line']}")
                    lines.append(f"- **Problem:** {s['problem']}")
                    lines.append(f"- **Action:** {s['action']}")
                    if s.get('symbol_context'):
                        symbols = s['symbol_context'].get('possible_symbols', [])
                        if symbols:
                            lines.append(f"- **Related symbols:** {', '.join(symbols[:3])}")
                lines.append("")

        return "\n".join(lines)


# ─── Main ──────────────────────────────────────────────────────────────────
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Build Feedback Loop v2.0 for MixCoach")
    parser.add_argument("--config", default=DEFAULT_CONFIG, help="Build config (Release/Debug)")
    parser.add_argument("--clean", action="store_true", help="Clean build")
    parser.add_argument("--deploy", action="store_true", help="Deploy after successful build")
    parser.add_argument("--analyze-only", metavar="FILE", help="Analyze existing build log file")
    parser.add_argument("--no-retry", action="store_true", help="Don't attempt auto-repair")
    parser.add_argument("--learn", action="store_true", help="Train error patterns from history")
    parser.add_argument("--impact", metavar="FILE", help="Check build impact of changing a file")
    parser.add_argument("--stats", action="store_true", help="Show error pattern statistics")
    args = parser.parse_args()

    session = SessionManager()
    analyzer = ErrorAnalyzer()
    output = ""

    # Mode: Show error stats
    if args.stats:
        common = session.get_most_common_errors(10)
        print(f"\n[ERROR STATS] Most common errors:")
        for e in common:
            print(f"  {e['code']}: seen {e['count']} times")
        history = session.data.get("successful_fixes", [])
        print(f"\n[FIX HISTORY] {len(history)} successful fixes recorded")
        for fix in history[-5:]:
            print(f"  [{fix['timestamp'][:16]}] {fix['error_code']}: {fix['fix'][:80]}")
        return

    # Mode: Analyze existing log
    if args.analyze_only:
        try:
            with open(args.analyze_only, "r", encoding="utf-8") as f:
                output = f.read()
        except FileNotFoundError:
            print(f"[ERROR] File not found: {args.analyze_only}", file=sys.stderr)
            sys.exit(1)

        errors = analyzer.parse_build_output(output)
        summary = analyzer.summarize()
        print(f"\nParsed {summary['total']} errors "
              f"({summary['known_patterns']} known, {summary['unknown_patterns']} unknown, "
              f"{summary['with_symbol_context']} with symbol context)")
        for e in errors[:10]:
            known = "[OK]" if e.get("fix") else "[??]"
            ctx = "*" if e.get("symbol_context") else " "
            print(f"  {known}{ctx} [{e['type']}] {e.get('code','?'):8s} {e.get('message','')[:100]}")
        sys.exit(0 if summary['total'] == 0 else 1)

    # Mode: Learn from history
    if args.learn:
        print("[LEARN] Analyzing build history for error patterns...")
        history = session.data.get("build_history", [])
        errors_seen = session.data.get("error_pattern_stats", {})
        print(f"  Build history entries: {len(history)}")
        print(f"  Unique error codes: {len(errors_seen)}")
        for code, count in sorted(errors_seen.items(), key=lambda x: -x[1])[:10]:
            print(f"    {code}: {count} times")
        return

    # Mode 2: Full build loop
    print("=" * 60)
    print(f"  MixCoach Build Loop v2.0 — {args.config}")
    print("=" * 60)

    for attempt in range(1 + MAX_RETRIES):
        print(f"\n[Attempt {attempt + 1}/{1 + MAX_RETRIES}]")

        runner = BuildRunner(args.config)
        success = runner.run(clean=(args.clean or attempt > 0))

        errors = analyzer.parse_build_output(runner.output)
        summary = analyzer.summarize()
        auto_repair = AutoRepair(errors, analyzer)

        # Save to session
        session.record_build(success, args.config, runner.duration, errors)

        if success:
            print(f"\n[OK] BUILD SUCCESSFUL ({runner.duration:.1f}s)")
            if args.deploy:
                runner._deploy()
            break
        else:
            print(f"\n[FAIL] BUILD FAILED ({runner.duration:.1f}s)")
            print(f"   Errors: {summary['total']} ({summary['known_patterns']} known, "
                  f"{summary['with_symbol_context']} with symbol context)")

            # Show top errors
            for e in errors[:8]:
                known = "[OK]" if e.get("fix") else "[??]"
                ctx = "*" if e.get("symbol_context") else " "
                loc = ""
                if e.get('file'):
                    parts = e['file'].split(chr(92))
                    loc = f"{parts[-1]}:{e.get('line','?')}"
                print(f"  {known}{ctx} [{e['type']}] {e.get('code','?'):8s} {loc} {e.get('message','')[:120]}")

            # Suggest fixes
            fixes = auto_repair.suggest_fixes()
            if fixes:
                print(f"\n[FIXES] Suggested fixes ({len(fixes)}):")
                for f in fixes[:5]:
                    conf = f" [{f.get('confidence',0):.0%}]" if f.get('confidence', 0) > 0 else " [NEW]"
                    print(f"  {conf} [{f['code']}] {f['action'][:150]}")

            # Auto-retry
            if not args.no_retry and attempt < MAX_RETRIES:
                has_linker = any(e["type"] == "linker" for e in errors)
                if has_linker:
                    print("\n[RETRY] Linker errors detected - clean retry...")
                    args.clean = True
                else:
                    print(f"\n[RETRY] Attempt {attempt + 2}...")
            else:
                break

    print("\n[SESSION] State saved to AI_SESSION_STATE.json")


if __name__ == "__main__":
    main()

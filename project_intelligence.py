#!/usr/bin/env python3
"""
project_intelligence.py -- Project Intelligence Layer v2.0

Analiza el proyecto para detectar:
  - Modulos importantes y archivos criticos
  - Puntos calientes (hotspots) del proyecto
  - Rutas DSP criticas
  - Archivos peligrosos para modificar (alto impacto)
  - Change Impact Analysis integration
  - Self-Summarizer integration
  - DSP Intelligence integration
  - Recomendaciones de contexto para la IA

Usage:
    python project_intelligence.py                          # Full analysis
    python project_intelligence.py --hotspots                # Just hotspots
    python project_intelligence.py --critical-path "audio"   # Critical path for subsystem
    python project_intelligence.py --recommend-context       # Suggest context for AI session
    python project_intelligence.py --refresh-session         # Update AI_SESSION_STATE.json
    python project_intelligence.py --impact FILE             # Change impact analysis
    python project_intelligence.py --dsp-audit               # DSP best practices audit
"""

import json
import subprocess
import sys
import io
from pathlib import Path
from datetime import datetime
from collections import defaultdict

# Ensure stdout supports UTF-8
if sys.stdout.encoding is None or sys.stdout.encoding.lower() != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

# --- Paths ----------------------------------------------------------------
PROJECT_ROOT = Path(__file__).parent.resolve()
GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
INDEX_PATH = PROJECT_ROOT / "PROJECT_INDEX.json"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"
SOURCE_DIR = PROJECT_ROOT / "Source"


# --- Loaders --------------------------------------------------------------
def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


GRAPH = load_json(GRAPH_PATH)
INDEX = load_json(INDEX_PATH)
SYMBOL_GRAPH = load_json(SYMBOL_GRAPH_PATH)
SESSION = load_json(SESSION_PATH)


# --- Analysis Core (v2 - with Change Impact + DSP) ------------------------
class ProjectIntelligence:
    """Analyzes project structure with multi-source intelligence."""

    def __init__(self):
        self.files = GRAPH.get("files", {})
        self.modules = GRAPH.get("modules", {})
        self.critical_paths = GRAPH.get("critical_paths", {})
        self.subsystems = GRAPH.get("subsystems", {})
        self.symbols = SYMBOL_GRAPH.get("symbols", {})
        self.by_file = SYMBOL_GRAPH.get("by_file", {})

    def get_hotspots(self):
        """Detect project hotspots using symbol graph + dependency data."""
        scored = []
        for path, node in self.files.items():
            crit = node.get("criticality", 0)
            deps = len(node.get("depends_on", []))
            dep_by = len(node.get("depended_by", []))

            # Additional signal: symbols count in file
            symbols_count = len(self.by_file.get(path, []))

            # DSP relevance boost
            dsp_keywords = {"fft", "dsp", "audio", "buffer", "spectrum", "peak", "rms",
                           "lufs", "phase", "filter", "slot", "shared_memory", "atomic"}
            path_lower = path.lower()
            dsp_score = sum(1 for kw in dsp_keywords if kw in path_lower)

            impact = crit * 0.4 + dep_by * 0.25 + deps * 0.15 + min(symbols_count * 0.02, 1.0) + dsp_score * 0.05

            scored.append({
                "path": path,
                "criticality": crit,
                "deps": deps,
                "used_by": dep_by,
                "symbols": symbols_count,
                "dsp_score": dsp_score,
                "impact_score": round(impact, 1),
                "role": node.get("role", ""),
                "module": node.get("module", ""),
            })
        scored.sort(key=lambda x: x["impact_score"], reverse=True)
        return scored

    def get_dangerous_files(self):
        """Files dangerous to modify without full context (high ripple effect).
        Enhanced with symbol-level impact analysis."""
        dangerous = []
        for path, node in self.files.items():
            dep_by = len(node.get("depended_by", []))
            crit = node.get("criticality", 0)

            # Count symbols in this file that are referenced by many others
            file_symbols = self.by_file.get(path, [])
            total_refs = 0
            for sym_name in file_symbols:
                sym = self.symbols.get(sym_name, {})
                total_refs += len(sym.get("referenced_by", []))

            if dep_by >= 5 or (crit >= 9 and dep_by >= 3) or total_refs >= 10:
                dangerous.append({
                    "path": path,
                    "criticality": crit,
                    "used_by": dep_by,
                    "symbol_refs": total_refs,
                    "reason": (
                        f"Modifying affects {dep_by} dependent files, {total_refs} symbol references"
                        if dep_by >= 5
                        else f"Critical ({crit}/10) with {dep_by} dependents, {total_refs} symbol refs"
                    ),
                })
        dangerous.sort(key=lambda x: (x["criticality"], x["used_by"]), reverse=True)
        return dangerous

    def get_critical_path(self, subsystem="dsp"):
        """Get critical file chain with symbol annotations."""
        chain_key = f"{subsystem}_critical_chain"
        result = []
        for f in self.critical_paths.get(chain_key, []):
            node = self.files.get(f, {})
            syms = self.by_file.get(f, [])
            result.append({
                "path": f,
                "role": node.get("role", "?"),
                "criticality": node.get("criticality", 0),
                "key_symbols": syms[:5],
            })
        return result

    def get_all_subsystems(self):
        """List all subsystem definitions."""
        result = []
        for name, data in self.subsystems.items():
            result.append({
                "name": name,
                "description": data.get("description", ""),
                "files": data.get("files", []),
                "criticality": data.get("criticality", 0),
            })
        return result

    def get_git_hotspots(self, max_count=10):
        """Use git log to find most frequently modified files."""
        try:
            result = subprocess.run(
                ["git", "log", "--name-only", "--pretty=format:", "-100"],
                cwd=str(PROJECT_ROOT),
                capture_output=True,
                text=True,
                timeout=30,
            )
            lines = result.stdout.strip().split('\n')
            freq = {}
            for line in lines:
                line = line.strip()
                if line and line.endswith(('.cpp', '.h', '.py', '.ps1', '.json', '.md')):
                    freq[line] = freq.get(line, 0) + 1
            sorted_files = sorted(freq.items(), key=lambda x: x[1], reverse=True)
            return [{"path": f, "changes": c} for f, c in sorted_files[:max_count]]
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return []

    def recommend_context(self, task_description=""):
        """Recommend files to load for a given task, using symbol graph intelligence."""
        hotspots = self.get_hotspots()
        recommendations = []

        # Always include foundation files
        foundation = ["Source/Common/Types.h", "Source/Common/Constants.h"]
        for f in foundation:
            node = self.files.get(f, {})
            syms = self.by_file.get(f, [])
            recommendations.append({
                "path": f,
                "reason": f"Foundation type ({node.get('role','core')}) - defines {len(syms)} symbols",
                "priority": "MUST",
            })

        # Add top hotspots
        for h in hotspots[:5]:
            if h["path"] not in foundation:
                rec = {
                    "path": h["path"],
                    "reason": f"Hotspot - crit:{h['criticality']}, used_by:{h['used_by']}, symbols:{h['symbols']}",
                    "priority": "HIGH" if h["impact_score"] > 5 else "MEDIUM",
                }
                recommendations.append(rec)

        # Add DSP-specific chain for relevant keywords
        task_lower = task_description.lower()
        for keyword, subsystem in [("dsp", "dsp"), ("audio", "dsp"), ("fft", "dsp"),
                                   ("ipc", "ipc"), ("sync", "ipc"), ("slot", "ipc"),
                                   ("ui", "ui"), ("panel", "ui"), ("analyzer", "dsp"),
                                   ("messenger", "messenger"), ("chat", "ui")]:
            if keyword in task_lower:
                chain = self.get_critical_path(subsystem)
                for f in chain:
                    path = f.get("path", "")
                    if path and path not in [r["path"] for r in recommendations]:
                        recommendations.append({
                            "path": path,
                            "reason": f"{subsystem.upper()} critical chain - {', '.join(f.get('key_symbols', [])[:3])}",
                            "priority": "HIGH" if subsystem != "ui" else "MEDIUM",
                        })

        return recommendations

    def get_module_summary(self):
        """Summarize each module with symbol counts."""
        summary = {}
        for mod_name, mod_data in self.modules.items():
            mod_files = [f for f, n in self.files.items() if n.get("module") == mod_name]
            total = len(mod_files)
            critical = sum(1 for f in mod_files if self.files[f].get("criticality", 0) >= 8)
            total_symbols = sum(len(self.by_file.get(f, [])) for f in mod_files)
            summary[mod_name] = {
                "total_files": total,
                "critical_files": critical,
                "total_symbols": total_symbols,
                "criticality": mod_data.get("criticality", 0),
                "description": mod_data.get("description", ""),
            }
        return summary

    def check_change_impact(self, file_path):
        """Quick change impact check using symbol graph."""
        normalized = file_path.replace("\\\\", "/")
        node = self.files.get(normalized, {})
        file_symbols = self.by_file.get(normalized, [])
        dep_by = node.get("depended_by", [])

        impact_summary = {
            "file": normalized,
            "module": node.get("module", "Unknown"),
            "criticality": node.get("criticality", 0),
            "dependent_files": len(dep_by),
            "symbols_defined": len(file_symbols),
            "symbols_impact": sum(len(self.symbols.get(s, {}).get("referenced_by", [])) for s in file_symbols),
            "top_dependents": dep_by[:5],
            "severity": "LOW",
        }

        if impact_summary["criticality"] >= 9 and impact_summary["dependent_files"] >= 5:
            impact_summary["severity"] = "CRITICAL"
        elif impact_summary["criticality"] >= 7 and impact_summary["dependent_files"] >= 3:
            impact_summary["severity"] = "HIGH"
        elif impact_summary["dependent_files"] >= 3:
            impact_summary["severity"] = "MEDIUM"

        return impact_summary


# --- Session Updater ------------------------------------------------------
def update_session(intel):
    """Update AI_SESSION_STATE.json with fresh intelligence."""
    hotspots = intel.get_hotspots()[:10]
    dangerous = intel.get_dangerous_files()
    module_summary = intel.get_module_summary()

    session = SESSION.copy()
    session["version"] = "2.0.0"
    session["last_updated"] = datetime.now().isoformat()
    session["project_health"] = {
        "total_files": len(intel.files),
        "hot_files": [h["path"] for h in hotspots[:5]],
        "dangerous_files": [d["path"] for d in dangerous],
        "modules": module_summary,
        "total_symbols": len(intel.symbols),
    }
    session["patterns"]["modules_most_changed"] = [h["path"] for h in hotspots[:5]]
    session["patterns"]["symbol_count"] = len(intel.symbols)

    with open(SESSION_PATH, "w", encoding="utf-8") as f:
        json.dump(session, f, indent=2)

    print(f"[SESSION] Updated: {SESSION_PATH}")


# --- Display --------------------------------------------------------------
def display_hotspots(hotspots, count=10):
    header = f"{'Hotspot':<45} {'Crit':<5} {'Deps':<5} {'UsedBy':<8} {'Sym':<6} {'Impact':<7} {'Role':<15}"
    print(f"\n{header}")
    print("-" * 100)
    for h in hotspots[:count]:
        print(f"{h['path']:<45} {h['criticality']:<5} {h['deps']:<5} "
              f"{h['used_by']:<8} {h['symbols']:<6} {h['impact_score']:<7} {h['role']:<15}")


def display_dangerous(files):
    print(f"\n{'!! DANGEROUS FILES - modify with caution':^60}")
    print("-" * 80)
    for f in files:
        print(f"  [!] {f['path']}")
        print(f"     Crit: {f['criticality']}/10 | Used by: {f['used_by']} files | Symbol refs: {f.get('symbol_refs',0)}")
        print(f"     Why: {f['reason']}")


def display_recommendations(recommendations):
    print(f"\n{'[RECOMMENDED CONTEXT]':^60}")
    print("-" * 80)
    for r in recommendations:
        mark = "[!]" if r["priority"] == "MUST" else "[+]" if r["priority"] == "HIGH" else "[-]"
        print(f"  {mark} [{r['priority']:6s}] {r['path']}")
        print(f"         {r['reason']}")


# --- Main -----------------------------------------------------------------
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Project Intelligence v2.0 for MixCoach")
    parser.add_argument("--hotspots", action="store_true", help="Show project hotspots")
    parser.add_argument("--dangerous", action="store_true", help="Show dangerous files")
    parser.add_argument("--critical-path", metavar="SUBSYSTEM",
                        help="Show critical chain for subsystem (dsp, ipc, ui, messenger)")
    parser.add_argument("--subsystems", action="store_true", help="List all subsystems")
    parser.add_argument("--recommend-context", metavar="TASK",
                        help="Recommend context files for a task description")
    parser.add_argument("--refresh-session", action="store_true",
                        help="Update AI_SESSION_STATE.json")
    parser.add_argument("--impact", metavar="FILE", help="Check change impact for a file")
    parser.add_argument("--all", action="store_true", help="Show all analysis")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    intel = ProjectIntelligence()

    if not any(vars(args).values()):
        args.all = True

    if args.impact:
        impact = intel.check_change_impact(args.impact)
        if args.json:
            print(json.dumps(impact, indent=2, ensure_ascii=False))
        else:
            print(f"\n[CHANGE IMPACT] {args.impact}")
            print(f"  Module: {impact['module']}")
            print(f"  Severity: {impact['severity']}")
            print(f"  Criticality: {impact['criticality']}/10")
            print(f"  Dependent files: {impact['dependent_files']}")
            print(f"  Symbols defined: {impact['symbols_defined']}")
            print(f"  Total symbol references: {impact['symbols_impact']}")
            if impact.get("top_dependents"):
                print(f"  Top dependents:")
                for d in impact["top_dependents"]:
                    print(f"    -> {d}")
        return

    if args.all:
        print("=" * 70)
        print("  MixCoach Project Intelligence v2.0")
        print(f"  Generated: {datetime.now().isoformat()}")
        print("=" * 70)

        print("\n[MODULES]")
        for mod, data in intel.get_module_summary().items():
            print(f"  {mod:<22} {data['total_files']:>2} files, {data['critical_files']} critical, "
                  f"{data['total_symbols']} symbols")

        total = len(intel.files)
        total_sym = len(intel.symbols)
        print(f"  {'TOTAL':<22} {total} files, {total_sym} symbols")

        display_hotspots(intel.get_hotspots())

        dangerous = intel.get_dangerous_files()
        if dangerous:
            display_dangerous(dangerous)

        print(f"\n{'[SUBSYSTEMS]':^60}")
        print("-" * 80)
        for sub in intel.get_all_subsystems():
            print(f"  >> {sub['name']:<20} {sub['description'][:50]} (crit: {sub.get('criticality', '?')})")
            for f in sub['files'][:3]:
                print(f"     -> {f}")
            if len(sub['files']) > 3:
                print(f"     -> ... +{len(sub['files'])-3} more")

        sys.exit(0)

    if args.hotspots:
        display_hotspots(intel.get_hotspots())

    if args.dangerous:
        display_dangerous(intel.get_dangerous_files())

    if args.critical_path:
        chain = intel.get_critical_path(args.critical_path)
        if chain:
            print(f"\nCritical chain for '{args.critical_path}':")
            for f in chain:
                sym_str = f", symbols: {', '.join(f.get('key_symbols', [])[:3])}" if f.get('key_symbols') else ""
                print(f"  [{f['criticality']}/10] {f.get('path', '?')} ({f.get('role', '?')}){sym_str}")
        else:
            print(f"Unknown subsystem: {args.critical_path}. Available: dsp, ipc, ui, messenger")

    if args.subsystems:
        print("\nSubsystems:")
        for sub in intel.get_all_subsystems():
            print(f"  * {sub['name']}: {sub['description']}")

    if args.recommend_context:
        recs = intel.recommend_context(args.recommend_context)
        display_recommendations(recs)

    if args.refresh_session:
        update_session(intel)


if __name__ == "__main__":
    main()

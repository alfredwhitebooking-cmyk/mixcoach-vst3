#!/usr/bin/env python3
"""
change_impact.py — Change Impact Analysis v1.0

CHANGE IMPACT ANALYSIS (requirement #6):
When a file is modified, this system detects:
  - Which modules could break
  - Which classes depend on it
  - Which callbacks use that symbol
  - Which parts need rebuild
  - Ripple effect scoring

Usage:
    python change_impact.py --file Source/Common/Types.h     # Analyze impact of changing this file
    python change_impact.py --diff "Types.h"                 # Analyze based on git diff
    python change_impact.py --check-build "file1 file2"      # Check rebuild requirements

Features:
    - Uses SYMBOL_GRAPH.json for symbol-level dependency analysis
    - Uses PROJECT_GRAPH.json for file-level dependency analysis
    - Uses PROJECT_INDEX.json for module-level impact
    - Ripple effect scoring (low, medium, high, critical)
    - Rebuild analysis (what needs recompilation)
    - Suggest incremental build strategy
"""

import json
import re
import sys
from pathlib import Path
from datetime import datetime
from collections import defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.resolve()
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
PROJECT_GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
PROJECT_INDEX_PATH = PROJECT_ROOT / "PROJECT_INDEX.json"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"


# ─── Loaders ───────────────────────────────────────────────────────────────
def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


# ═══════════════════════════════════════════════════════════════════════════
#  ChangeImpactAnalyzer
# ═══════════════════════════════════════════════════════════════════════════
class ChangeImpactAnalyzer:
    """Analyzes the impact of changing files or symbols in the project."""

    def __init__(self):
        self.symbol_graph = load_json(SYMBOL_GRAPH_PATH)
        self.project_graph = load_json(PROJECT_GRAPH_PATH)
        self.project_index = load_json(PROJECT_INDEX_PATH)

        self.symbols = self.symbol_graph.get("symbols", {})
        self.graph_files = self.project_graph.get("files", {})
        self.by_file = self.symbol_graph.get("by_file", {})
        self.relationships = self.symbol_graph.get("relationships", [])

    # ─── File Impact Analysis ──────────────────────────────────────────

    def analyze_file_impact(self, file_path):
        """Full impact analysis for a file change."""
        normalized = file_path.replace("\\", "/")

        # Get module info
        module = self._get_module(normalized)

        # Get file node from graph
        node = self.graph_files.get(normalized, {})

        # Get symbols defined in this file
        symbols_in_file = self.by_file.get(normalized, [])

        # Direct dependencies (what this file includes)
        depends_on = node.get("depends_on", [])

        # Direct dependents (what includes this file)
        depended_by = node.get("depended_by", [])

        # Symbol references (what symbols reference this file's symbols)
        affected_symbols = set()
        for sym in symbols_in_file:
            refs = self.symbols.get(sym, {}).get("referenced_by", [])
            affected_symbols.update(refs)

        # Map affected symbols back to files
        affected_files = set()
        for sym in affected_symbols:
            sym_file = self.symbols.get(sym, {}).get("file", "")
            if sym_file and sym_file != normalized:
                affected_files.add(sym_file)

        # Combine: direct dependents + symbol-affected files
        all_affected = set(depended_by) | affected_files

        # Calculate ripple effect
        ripple = self._calculate_ripple(normalized, all_affected)

        # Classify impact severity
        severity = self._classify_impact(normalized, node, all_affected, ripple)

        # Build impact details
        impacted_modules = self._get_impacted_modules(all_affected)
        rebuild_set = self._analyze_rebuild(normalized, all_affected)

        return {
            "file": normalized,
            "module": module,
            "severity": severity,
            "ripple_score": ripple["score"],
            "ripple_level": ripple["level"],
            "total_affected_files": len(all_affected),
            "direct_dependents": list(depended_by)[:10],
            "symbol_affected": list(affected_files)[:10],
            "all_affected": list(all_affected)[:20],
            "impacted_modules": impacted_modules,
            "rebuild": rebuild_set,
            "symbols_defined_here": symbols_in_file[:15],
            "metrics": {
                "criticality": node.get("criticality", 0),
                "depends_on_count": len(depends_on),
                "depended_by_count": len(depended_by),
                "symbols_count": len(symbols_in_file),
                "affected_symbol_count": len(affected_symbols),
            },
        }

    def _get_module(self, file_path):
        """Get module name for a file."""
        for module in self.project_index.get("modules", []):
            for f in module.get("files", []):
                if f.get("path", "").replace("\\", "/") == file_path:
                    return module.get("name", "Unknown")
        return "Unknown"

    def _calculate_ripple(self, file_path, affected_files):
        """Calculate ripple effect score and level."""
        node = self.graph_files.get(file_path, {})
        criticality = node.get("criticality", 0)
        depended_count = len(node.get("depended_by", []))
        affected_count = len(affected_files)

        # Score: criticality (0-10) + depended_by ratio + affected count
        base_score = criticality
        dependency_score = min(10, depended_count * 1.5)
        spread_score = min(10, affected_count * 0.8)

        score = (base_score * 0.4) + (dependency_score * 0.35) + (spread_score * 0.25)

        if score >= 8:
            level = "CRITICAL"
        elif score >= 5:
            level = "HIGH"
        elif score >= 3:
            level = "MEDIUM"
        else:
            level = "LOW"

        return {"score": round(score, 1), "level": level}

    def _classify_impact(self, file_path, node, affected_files, ripple):
        """Classify the overall impact severity."""
        criticality = node.get("criticality", 0)

        if criticality >= 9 and ripple["level"] in ("CRITICAL", "HIGH"):
            return "BREAKING"
        elif ripple["level"] == "CRITICAL":
            return "CRITICAL"
        elif ripple["level"] == "HIGH" or len(affected_files) >= 5:
            return "HIGH"
        elif ripple["level"] == "MEDIUM":
            return "MEDIUM"
        else:
            return "LOW"

    def _get_impacted_modules(self, affected_files):
        """Get which modules are impacted."""
        modules = defaultdict(int)
        for f in affected_files:
            for module in self.project_index.get("modules", []):
                for mf in module.get("files", []):
                    if mf.get("path", "").replace("\\\\", "/") == f:
                        modules[module["name"]] += 1
                        break
        return dict(sorted(modules.items(), key=lambda x: -x[1]))

    def _analyze_rebuild(self, file_path, affected_files):
        """Analyze rebuild requirements."""
        ext = Path(file_path).suffix
        is_header = ext == ".h"

        if is_header:
            # Header changes require recompilation of all dependents
            return {
                "requires_rebuild": True,
                "reason": "Header file change requires all dependent .cpp files to recompile",
                "files_to_rebuild": list(affected_files)[:15],
                "estimated_cpp_files": len([f for f in affected_files if f.endswith(".cpp")]),
                "strategy": "incremental" if len(affected_files) < 10 else "full",
            }
        else:
            # .cpp changes only require re-linking
            return {
                "requires_rebuild": True,
                "reason": "Source file change requires recompilation of this file and re-linking",
                "files_to_rebuild": [file_path],
                "estimated_cpp_files": 1,
                "strategy": "single_file",
            }

    # ─── Symbol Impact Analysis ────────────────────────────────────────

    def analyze_symbol_impact(self, symbol_name):
        """Analyze the impact of changing a specific symbol."""
        symbol = self.symbols.get(symbol_name)
        if not symbol:
            # Try partial match
            for sym_name, sym in self.symbols.items():
                if symbol_name.lower() in sym_name.lower():
                    symbol = sym
                    symbol_name = sym_name
                    break
        if not symbol:
            return {"error": f"Symbol not found: {symbol_name}"}

        file_path = symbol.get("file", "")
        references = self._find_references(symbol_name)

        return {
            "symbol": symbol_name,
            "type": symbol.get("type", "unknown"),
            "defined_in": file_path,
            "line": symbol.get("line", 0),
            "namespace": symbol.get("namespace", ""),
            "referenced_by": references[:20],
            "reference_count": len(references),
            "impact": self._symbol_impact_level(symbol, references),
            "details": self._symbol_details(symbol),
        }

    def _find_references(self, symbol_name):
        """Find all references to a symbol across the codebase."""
        refs = []
        # From symbol graph
        sym = self.symbols.get(symbol_name, {})
        for ref in sym.get("referenced_by", []):
            ref_sym = self.symbols.get(ref, {})
            refs.append({
                "symbol": ref,
                "type": ref_sym.get("type", "unknown"),
                "file": ref_sym.get("file", "unknown"),
                "line": ref_sym.get("line", 0),
            })

        # From inheritance relationships
        for rel in self.relationships:
            if rel.get("to") == symbol_name or rel.get("from") == symbol_name:
                refs.append({
                    "symbol": rel.get("to") if rel.get("from") == symbol_name else rel.get("from"),
                    "type": "inheritance",
                    "file": rel.get("file", "unknown"),
                    "line": 0,
                    "relationship": rel.get("type", "inherits"),
                })

        return refs

    def _symbol_impact_level(self, symbol, references):
        """Calculate impact level for a symbol change."""
        sym_type = symbol.get("type", "")
        ref_count = len(references)

        # Base class changes are most impactful
        if sym_type in ("class", "struct") and symbol.get("base_classes"):
            if ref_count > 5:
                return "CRITICAL"
            return "HIGH"

        # Methods with many references
        if sym_type == "method" and ref_count > 3:
            return "HIGH"

        # Callbacks and enums
        if sym_type == "callback" or sym_type == "enum":
            if ref_count > 2:
                return "HIGH"
            return "MEDIUM"

        # Light impact
        if ref_count == 0:
            return "LOW"
        return "MEDIUM"

    def _symbol_details(self, symbol):
        """Get detailed information about a symbol."""
        details = {}
        sym_type = symbol.get("type", "")

        if sym_type in ("class", "struct"):
            details["methods"] = len(symbol.get("methods", []))
            details["base_classes"] = symbol.get("base_classes", [])
            details["is_abstract"] = symbol.get("is_abstract", False)

        if sym_type == "method":
            details["return_type"] = symbol.get("return_type", "")
            details["params"] = symbol.get("params", "")
            details["virtual"] = symbol.get("virtual", False)
            details["override"] = symbol.get("override", False)
            details["pure_virtual"] = symbol.get("pure_virtual", False)

        if sym_type == "enum":
            details["values"] = len(symbol.get("values", []))
            details["scoped"] = symbol.get("scoped", False)

        if sym_type == "callback":
            details["signature"] = symbol.get("signature", "")

        return details

    # ─── Batch Impact Analysis ─────────────────────────────────────────

    def analyze_batch(self, file_paths):
        """Analyze impact of changing multiple files."""
        results = []
        combined_affected = set()
        combined_severities = []

        for f in file_paths:
            impact = self.analyze_file_impact(f)
            results.append(impact)
            combined_affected.update(impact.get("all_affected", []))
            combined_severities.append(impact["severity"])

        # Determine overall severity
        if "BREAKING" in combined_severities:
            overall = "BREAKING"
        elif "CRITICAL" in combined_severities:
            overall = "CRITICAL"
        elif "HIGH" in combined_severities:
            overall = "HIGH"
        elif "MEDIUM" in combined_severities:
            overall = "MEDIUM"
        else:
            overall = "LOW"

        # Combine rebuild analysis
        rebuild_summary = self._combine_rebuild(results)

        return {
            "files": file_paths,
            "overall_severity": overall,
            "total_affected": len(combined_affected),
            "all_affected": list(combined_affected)[:25],
            "per_file": results,
            "rebuild": rebuild_summary,
            "metrics": {
                "files_changed": len(file_paths),
                "unique_affected_files": len(combined_affected),
                "critical_files": sum(1 for r in results if r["severity"] in ("BREAKING", "CRITICAL")),
            },
        }

    def _combine_rebuild(self, results):
        """Combine rebuild analysis from multiple files."""
        all_rebuild_files = set()
        strategies = set()

        for r in results:
            rb = r.get("rebuild", {})
            strategies.add(rb.get("strategy", "unknown"))
            for f in rb.get("files_to_rebuild", []):
                all_rebuild_files.add(f)

        if "full" in strategies:
            strategy = "full"
        elif "incremental" in strategies:
            strategy = "incremental"
        else:
            strategy = "single_file"

        return {
            "requires_rebuild": True,
            "strategy": strategy,
            "files_to_rebuild": list(all_rebuild_files)[:20],
            "estimated_files": len(all_rebuild_files),
        }

    # ─── Impact Report ─────────────────────────────────────────────────

    def generate_report(self, file_path):
        """Generate a human-readable impact report."""
        impact = self.analyze_file_impact(file_path)

        lines = []
        lines.append("=" * 60)
        lines.append(f"  CHANGE IMPACT REPORT: {impact['file']}")
        lines.append("=" * 60)
        lines.append(f"  Module: {impact['module']}")
        lines.append(f"  Severity: {impact['severity']}")
        lines.append(f"  Ripple: {impact['ripple_level']} ({impact['ripple_score']}/10)")
        lines.append("")

        # Metrics
        lines.append("  [METRICS]")
        m = impact["metrics"]
        lines.append(f"    Criticality: {m['criticality']}/10")
        lines.append(f"    Dependencies: {m['depends_on_count']}")
        lines.append(f"    Dependents: {m['depended_by_count']}")
        lines.append(f"    Symbols defined: {m['symbols_count']}")
        lines.append(f"    Affected symbols: {m['affected_symbol_count']}")

        # Direct dependents
        if impact["direct_dependents"]:
            lines.append(f"\n  [DIRECT DEPENDENTS] ({len(impact['direct_dependents'])}):")
            for f in impact["direct_dependents"][:8]:
                lines.append(f"    -> {f}")

        # Impacted modules
        if impact["impacted_modules"]:
            lines.append(f"\n  [IMPACTED MODULES]:")
            for mod, count in impact["impacted_modules"].items():
                lines.append(f"    {mod}: {count} files affected")

        # Rebuild info
        rb = impact["rebuild"]
        lines.append(f"\n  [REBUILD]: {rb['strategy']}")
        lines.append(f"    Reason: {rb['reason']}")
        if rb.get("files_to_rebuild"):
            lines.append(f"    Files: {len(rb['files_to_rebuild'])}")

        # Risk warnings
        if impact["severity"] in ("BREAKING", "CRITICAL"):
            lines.append(f"\n  ⚠️  RISK: {impact['severity']} - Consider extensive testing and review")
        elif impact["severity"] == "HIGH":
            lines.append(f"\n  ⚠️  RISK: HIGH - Review dependent files before modifying")

        return "\n".join(lines)


# ─── Main ──────────────────────────────────────────────────────────────────
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Change Impact Analysis for MixCoach")
    parser.add_argument("--file", metavar="FILE", help="Analyze impact of changing a file")
    parser.add_argument("--symbol", metavar="SYMBOL", help="Analyze impact of changing a symbol")
    parser.add_argument("--batch", nargs="+", metavar="FILE", help="Analyze batch of file changes")
    parser.add_argument("--report", metavar="FILE", help="Generate human-readable impact report")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    if not any(vars(args).values()):
        parser.print_help()
        sys.exit(1)

    analyzer = ChangeImpactAnalyzer()

    if args.file:
        result = analyzer.analyze_file_impact(args.file)
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(json.dumps(result, indent=2, ensure_ascii=False))

    if args.symbol:
        result = analyzer.analyze_symbol_impact(args.symbol)
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"\n[SYMBOL IMPACT] {args.symbol}")
            if "error" in result:
                print(f"  ERROR: {result['error']}")
            else:
                print(f"  Type: {result['type']}")
                print(f"  Defined in: {result['defined_in']}:{result['line']}")
                print(f"  Impact: {result['impact']}")
                print(f"  References: {result['reference_count']}")
                for ref in result.get("referenced_by", [])[:5]:
                    print(f"    -> {ref['symbol']} ({ref['file']}:{ref['line']})")

    if args.batch:
        result = analyzer.analyze_batch(args.batch)
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"\n[BATCH IMPACT] {len(args.batch)} files")
            print(f"  Overall: {result['overall_severity']}")
            print(f"  Affected: {result['total_affected']} files")
            print(f"  Rebuild strategy: {result['rebuild']['strategy']}")
            for r in result.get("per_file", []):
                print(f"  [{r['severity']:8s}] {r['file']}")

    if args.report:
        report = analyzer.generate_report(args.report)
        print(report)


if __name__ == "__main__":
    main()

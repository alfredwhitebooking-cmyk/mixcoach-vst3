#!/usr/bin/env python3
"""
token_optimizer.py — Token Optimization System v2.0

SMART TOKEN OPTIMIZATION (requirement #12):
Multi-layer context compression for reducing token usage.

Features:
  - Multi-Layer Context Architecture (Layer 1-4 summaries)
  - Context compression (remove redundant docs, comments)
  - Priority-aware file selection with semantic ranking
  - Context caching (recent files tracked in AI_SESSION_STATE)
  - Token counting for estimation (C++ optimized)
  - Recommends which parts of a file to include/exclude
  - Deduplication of overlapping context
  - Semantic pruning based on relevance scoring
  - Batch summarization with token budget management

Usage:
    python token_optimizer.py --analyze-context "file1.cpp file2.h"
    python token_optimizer.py --optimize-order "query"
    python token_optimizer.py --summarize "Source/MixCoach/PluginEditor.cpp"
    python token_optimizer.py --cache-status
    python token_optimizer.py --layered-context "query" --budget 4000
    python token_optimizer.py --deduplicate "file1 file2 file3"
"""

import json
import re
import sys
from pathlib import Path
from datetime import datetime
from collections import defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.resolve()
GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"
SOURCE_DIR = PROJECT_ROOT / "Source"

# Token estimation constants for C++ code
TOKENS_PER_CHAR = 0.28
TOKENS_PER_LINE = 3.5

# Token budgets for each layer
LAYER_TOKENS = {
    "layer1_architecture": 500,
    "layer2_module": 300,
    "layer3_symbol_summary": 200,
    "layer4_code": None,  # Remaining budget
}

# ─── Loaders ───────────────────────────────────────────────────────────────
def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


GRAPH = load_json(GRAPH_PATH)
SYMBOL_GRAPH = load_json(SYMBOL_GRAPH_PATH)
SESSION = load_json(SESSION_PATH)


# ─── Token Estimator ──────────────────────────────────────────────────────
class TokenEstimator:
    """Estimate token counts for files and context with high accuracy."""

    @staticmethod
    def estimate_file(path):
        full_path = PROJECT_ROOT / path
        if not full_path.exists():
            return 0, 0, 0
        try:
            with open(full_path, "r", encoding="utf-8") as f:
                content = f.read()
            chars = len(content)
            lines = content.count('\n') + 1
            tokens = int(chars * TOKENS_PER_CHAR)
            return tokens, chars, lines
        except Exception:
            return 0, 0, 0

    @staticmethod
    def estimate_context(file_paths):
        total = 0
        details = []
        for path in file_paths:
            tokens, chars, lines = TokenEstimator.estimate_file(path)
            total += tokens
            details.append({
                "path": path,
                "tokens": tokens,
                "chars": chars,
                "lines": lines,
            })
        return total, details

    @staticmethod
    def format_size(tokens):
        if tokens > 100000:
            return f"{tokens/1000:.0f}K"
        elif tokens > 1000:
            return f"{tokens/1000:.1f}K"
        return str(tokens)


# ─── Multi-Layer Context Compressor (v2) ─────────────────────────────────
class ContextCompressor:
    """Multi-layer context optimization with semantic pruning."""

    def __init__(self):
        self.files = GRAPH.get("files", {})
        self.symbols = SYMBOL_GRAPH.get("symbols", {})
        self.by_file = SYMBOL_GRAPH.get("by_file", {})
        self.estimator = TokenEstimator()

    # ─── Layer 4: Code Compression ────────────────────────────────────

    def compress_file_content(self, path, max_tokens=4000):
        """Compress a file to fit within a token budget.
        Removes: comments, blank lines, debug code, redundant docs."""
        full_path = PROJECT_ROOT / path
        if not full_path.exists():
            return ""

        try:
            content = full_path.read_text(encoding="utf-8")
        except Exception:
            return ""

        token_count = int(len(content) * TOKENS_PER_CHAR)
        if token_count <= max_tokens:
            return content  # No compression needed

        lines = content.split("\n")
        compressed = []
        in_block_comment = False
        removed_lines = 0

        for line in lines:
            stripped = line.strip()

            # Skip block comments
            if "/*" in stripped:
                in_block_comment = True
                if "*/" in stripped:
                    in_block_comment = False
                removed_lines += 1
                continue
            if in_block_comment:
                if "*/" in stripped:
                    in_block_comment = False
                removed_lines += 1
                continue

            # Skip single-line comments that are documentation-only
            if stripped.startswith("// ") and not any(kw in stripped.lower()
               for kw in ["todo", "fixme", "hack", "warning", "note", "important"]):
                removed_lines += 1
                continue

            # Skip empty lines (but keep at most one consecutive)
            if not stripped:
                if compressed and compressed[-1] == "":
                    removed_lines += 1
                    continue

            compressed.append(line)

        # If still over budget, keep only class/function signatures
        compressed_str = "\n".join(compressed)
        new_token_count = int(len(compressed_str) * TOKENS_PER_CHAR)

        if new_token_count > max_tokens:
            # Aggressive compression: keep only signatures
            signatures = []
            for line in compressed:
                if re.match(r'^\s*(class|struct|enum|void|bool|int|float|double|\w+\s+\w+\s*\()', line):
                    signatures.append(line)
                elif re.match(r'^\s*(public|private|protected):', line):
                    signatures.append(line)
                elif re.match(r'^\s*};', line):
                    signatures.append(line)

            compressed_str = "\n".join(signatures)
            new_token_count = int(len(compressed_str) * TOKENS_PER_CHAR)

            if new_token_count > max_tokens:
                # Ultra compression: just the class/enum/struct names
                names = re.findall(r'(?:class|struct|enum)\s+(\w+)', "\n".join(compressed))
                compressed_str = f"// {path}\n// Classes: {', '.join(names[:15])}\n"

        return f"// [COMPRESSED from {token_count} to ~{new_token_count} tokens]\n\n{compressed_str}"

    # ─── Multi-Layer Context ───────────────────────────────────────────

    def get_layered_context(self, file_paths, query="", budget=8000):
        """Generate multi-layer context within a token budget.
        
        Layer 1: Architecture summary (~500 tokens)
        Layer 2: Module summaries (~300 tokens)
        Layer 3: Symbol summaries (~200 tokens each)
        Layer 4: Compressed code (remaining budget)
        """
        if not file_paths:
            file_paths = list(self.files.keys())

        # Score files by relevance to query
        scored_files = self.prioritize_files(file_paths, query)
        
        # Select top files that fit in budget
        budget_layer4 = budget - sum(LAYER_TOKENS.values()) if LAYER_TOKENS else budget

        # Layer 1: Architecture context
        layer1 = self._generate_layer1(file_paths)
        
        # Layer 2: Module summaries
        layer2 = self._generate_layer2(scored_files)
        
        # Layer 3: Symbol summaries
        layer3 = self._generate_layer3(scored_files)
        
        # Layer 4: Compressed code
        layer4 = []
        layer4_tokens = 0
        for sf in scored_files:
            if layer4_tokens >= budget_layer4:
                break
            path = sf["path"]
            file_budget = min(budget_layer4 - layer4_tokens, 4000)
            compressed = self.compress_file_content(path, max_tokens=file_budget)
            compressed_tokens = int(len(compressed) * TOKENS_PER_CHAR)
            layer4_tokens += compressed_tokens
            layer4.append({
                "path": path,
                "content": compressed,
                "estimated_tokens": compressed_tokens,
            })

        total_tokens = sum(LAYER_TOKENS.values()) + sum(l.get("estimated_tokens", 0) for l in layer4)

        return {
            "total_tokens": total_tokens,
            "budget": budget,
            "layer1_architecture": layer1,
            "layer2_modules": layer2,
            "layer3_symbols": layer3,
            "layer4_code": layer4,
            "over_budget": total_tokens > budget,
        }

    def _generate_layer1(self, file_paths):
        """Layer 1: Quick architecture context."""
        modules = defaultdict(int)
        for f in file_paths:
            node = self.files.get(f, {})
            modules[node.get("module", "Unknown")] += 1

        return {
            "files_count": len(file_paths),
            "modules_involved": dict(modules),
            "symbols_available": len(self.symbols),
            "estimated_tokens": 200,
        }

    def _generate_layer2(self, scored_files):
        """Layer 2: Module summaries for top files."""
        modules = defaultdict(list)
        for sf in scored_files[:10]:
            node = self.files.get(sf["path"], {})
            module = node.get("module", "Unknown")
            modules[module].append(sf["path"])

        result = []
        for module, files in sorted(modules.items(), key=lambda x: -len(x[1])):
            result.append({
                "module": module,
                "files_count": len(files),
                "files": files[:5],
                "critical_count": sum(1 for f in files if self.files.get(f, {}).get("criticality", 0) >= 8),
            })
        return {"modules": result, "estimated_tokens": 300}

    def _generate_layer3(self, scored_files):
        """Layer 3: Symbol summaries for top-scored files."""
        result = []
        for sf in scored_files[:8]:
            path = sf["path"]
            file_symbols = self.by_file.get(path, [])
            symbol_info = []
            for sym_name in file_symbols[:5]:
                sym = self.symbols.get(sym_name, {})
                sym_type = sym.get("type", "?")
                sym_line = sym.get("line", 0)
                symbol_info.append(f"{sym_type}:{sym_name.split('::')[-1]}(L{sym_line})")
            if symbol_info:
                result.append({
                    "path": path,
                    "symbols": symbol_info,
                    "criticality": self.files.get(path, {}).get("criticality", 0),
                })
        return {"file_summaries": result, "estimated_tokens": len(result) * 150}

    # ─── Priority Ranking ────────────────────────────────────────────

    def prioritize_files(self, file_paths, query=""):
        """Reorder files by relevance to query + criticality + token efficiency."""
        query_lower = query.lower()

        scored = []
        for path in file_paths:
            node = self.files.get(path, {})
            crit = node.get("criticality", 0)

            # Relevance from query matching
            relevance = 0.0
            if query_lower:
                role = node.get("role", "").lower()
                module = node.get("module", "").lower()
                path_lower = path.lower()
                for word in query_lower.split():
                    if word in path_lower or word in role or word in module:
                        relevance += 0.2

                # Symbol match boost
                file_symbols = self.by_file.get(path, [])
                for sym_name in file_symbols:
                    if any(word in sym_name.lower() for word in query_lower.split()):
                        relevance += 0.3
                        break

            tokens, _, _ = self.estimator.estimate_file(path)
            efficiency = (crit + relevance) / max(tokens, 1) * 100

            scored.append({
                "path": path,
                "score": crit + relevance,
                "criticality": crit,
                "relevance": relevance,
                "tokens": tokens,
                "efficiency": efficiency,
            })

        scored.sort(key=lambda x: x["score"], reverse=True)
        return scored

    # ─── Deduplication ──────────────────────────────────────────────

    def deduplicate_files(self, file_paths):
        """Detect and flag overlapping/duplicate context."""
        dup_groups = []
        checked = set()
        
        for i, f1 in enumerate(file_paths):
            if f1 in checked:
                continue
            related = [f1]
            # Check for header/source pairs
            stem = Path(f1).stem
            for j, f2 in enumerate(file_paths):
                if j != i and f2 not in checked:
                    if Path(f2).stem == stem:
                        related.append(f2)
            if len(related) > 1:
                dup_groups.append(related)
                checked.update(related)

        recommendations = []
        for group in dup_groups:
            h_files = [f for f in group if f.endswith(".h")]
            cpp_files = [f for f in group if f.endswith(".cpp")]
            if h_files and cpp_files:
                recommendations.append({
                    "group": group,
                    "suggestion": f"Header+source pair: include only {h_files[0]} (header contains interface)",
                    "tokens_saved": sum(self.estimator.estimate_file(f)[0] for f in cpp_files),
                })

        return {
            "duplicate_groups": dup_groups,
            "recommendations": recommendations,
            "total_tokens_saved": sum(r["tokens_saved"] for r in recommendations),
        }

    def summarize_file(self, path):
        """Generate a compact one-line summary."""
        node = self.files.get(path, {})
        if not node:
            return f"Unknown: {path}"

        role = node.get("role", "?")
        module = node.get("module", "?")
        file_symbols = self.by_file.get(path, [])
        types_str = ", ".join(s.split("::")[-1] for s in file_symbols[:4])
        if len(file_symbols) > 4:
            types_str += f" +{len(file_symbols)-4}"

        deps = len(node.get("depends_on", []))
        dep_by = len(node.get("depended_by", []))

        return f"[{module}] {path} ({role}) — {types_str} | deps:{deps} used_by:{dep_by}"

    def get_cache_status(self):
        """Show context cache from session."""
        recent_files = SESSION.get("recent_files", [])
        recent_tasks = SESSION.get("recent_tasks", [])

        print("Context Cache Status:")
        print(f"  Recent files ({len(recent_files)}):")
        for rf in recent_files[-5:]:
            print(f"    * {rf}")
        print(f"  Recent tasks ({len(recent_tasks)}):")
        for rt in recent_tasks[-3:]:
            print(f"    * {rt.get('description','?')[:80]}")

        total = 0
        for rf in recent_files:
            tokens, _, _ = self.estimator.estimate_file(rf)
            total += tokens
        print(f"  Estimated cached tokens: {TokenEstimator.format_size(total)}")
        
        # Show most expensive cached files
        expensive = sorted(
            [(rf, self.estimator.estimate_file(rf)) for rf in recent_files],
            key=lambda x: -x[1][0]
        )[:5]
        print(f"  Largest cached files:")
        for path, (tokens, chars, lines) in expensive:
            print(f"    {TokenEstimator.format_size(tokens):>6}  {path}")


# ─── Main ──────────────────────────────────────────────────────────────────
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Token Optimizer v2.0 for MixCoach")
    parser.add_argument("--analyze-context", nargs="+", metavar="FILE",
                        help="Estimate token cost for files")
    parser.add_argument("--optimize-order", metavar='"QUERY"',
                        help="Reorder/optimize files for a query")
    parser.add_argument("--summarize", metavar="FILE", help="Summarize a single file")
    parser.add_argument("--summarize-batch", nargs="+", metavar="FILE",
                        help="Summarize multiple files")
    parser.add_argument("--cache-status", action="store_true", help="Show cache status")
    parser.add_argument("--deduplicate", nargs="+", metavar="FILE",
                        help="Check for duplicate/overlapping context")
    parser.add_argument("--layered-context", metavar='"QUERY"',
                        help="Generate multi-layer context (smart token allocation)")
    parser.add_argument("--budget", type=int, default=8000,
                        help="Token budget for layered context (default: 8000)")
    parser.add_argument("--compress", metavar="FILE",
                        help="Compress a file to reduce tokens")
    parser.add_argument("--max-tokens", type=int, default=3000,
                        help="Maximum tokens for compression (default: 3000)")
    args = parser.parse_args()

    compressor = ContextCompressor()
    estimator = TokenEstimator()

    if args.analyze_context:
        tokens, details = estimator.estimate_context(args.analyze_context)
        print(f"\nToken estimate for {len(args.analyze_context)} files:")
        print("-" * 80)
        for d in sorted(details, key=lambda x: x["tokens"], reverse=True):
            marker = "[!]" if d["tokens"] > 2000 else "[+]" if d["tokens"] > 500 else "[-]"
            print(f"  {marker} {estimator.format_size(d['tokens']):>6}  {d['lines']:>4} lines  {d['path']}")
        print("-" * 80)
        print(f"  TOTAL: {estimator.format_size(tokens)} tokens ({tokens} est.)")

    if args.optimize_order:
        file_list = list(GRAPH.get("files", {}).keys())

        scored_files = compressor.prioritize_files(file_list, args.optimize_order)
        total = sum(s.get("tokens", 0) for s in scored_files)
        print(f"\nPriority order for: \"{args.optimize_order}\"")
        print(f"Total: {TokenEstimator.format_size(total)} tokens")
        print("-" * 80)
        print(f"{'Priority':<8} {'Score':<6} {'Tokens':<8} {'Effic.':<7} {'File':<40}")
        print("-" * 80)
        for i, s in enumerate(scored_files[:15]):
            prio = "[!]" if s["criticality"] >= 9 else "[+]" if s["criticality"] >= 7 else "[-]"
            print(f"  {prio} {i+1:<3}   {s['score']:<5.1f} {TokenEstimator.format_size(s['tokens']):>6}  "
                  f"{s['efficiency']:<6.1f} {s['path']:<40}")

    if args.summarize:
        print(compressor.summarize_file(args.summarize))

    if args.summarize_batch:
        print(f"\nFile summaries ({len(args.summarize_batch)}):")
        print("-" * 80)
        for f in args.summarize_batch:
            tokens, _, _ = estimator.estimate_file(f)
            summary = compressor.summarize_file(f)
            print(f"  {TokenEstimator.format_size(tokens):>6} tokens  {summary}")

    if args.cache_status:
        compressor.get_cache_status()

    if args.deduplicate:
        result = compressor.deduplicate_files(args.deduplicate)
        print(f"\n[DEDUPLICATION]")
        print(f"  Groups found: {len(result['duplicate_groups'])}")
        for g in result['duplicate_groups']:
            print(f"    Group: {', '.join(g)}")
        for r in result.get('recommendations', []):
            print(f"    -> {r['suggestion']} (saves {TokenEstimator.format_size(r['tokens_saved'])})")
        print(f"  Total tokens saved: {TokenEstimator.format_size(result['total_tokens_saved'])}")

    if args.layered_context:
        file_list = list(GRAPH.get("files", {}).keys())
        context = compressor.get_layered_context(file_list, args.layered_context, budget=args.budget)
        print(f"\n{'='*60}")
        print(f"  Layered Context for: '{args.layered_context}'")
        print(f"  Budget: {TokenEstimator.format_size(args.budget)} tokens")
        print(f"{'='*60}")
        print(f"  Total used: {TokenEstimator.format_size(context['total_tokens'])} tokens")
        l1 = context['layer1_architecture']
        print(f"\n  Layer 1 (Architecture):")
        print(f"    {l1['files_count']} files, {l1['modules_involved']}")
        l2 = context['layer2_modules']
        print(f"\n  Layer 2 (Modules):")
        for m in l2['modules'][:5]:
            print(f"    {m['module']}: {m['files_count']} files ({m['critical_count']} critical)")
        l3 = context['layer3_symbols']
        print(f"\n  Layer 3 (Symbols):")
        for fs in l3['file_summaries'][:5]:
            print(f"    [{fs['criticality']}/10] {fs['path']}: {', '.join(fs['symbols'][:4])}")
        print(f"\n  Layer 4 (Code): {len(context['layer4_code'])} files")
        for cf in context['layer4_code'][:5]:
            print(f"    {cf['path']} ({TokenEstimator.format_size(cf['estimated_tokens'])})")
        if context['over_budget']:
            print(f"\n  ⚠️  OVER BUDGET by {TokenEstimator.format_size(context['total_tokens'] - args.budget)}")

    if args.compress:
        compressed = compressor.compress_file_content(args.compress, max_tokens=args.max_tokens)
        print(f"\n[COMPRESSED] {args.compress}")
        print(f"  Max tokens: {args.max_tokens}")
        print(f"  Actual: ~{int(len(compressed) * TOKENS_PER_CHAR)} tokens")
        print(f"\n{compressed[:2000]}")
        print(f"\n  ... [{len(compressed)} chars total]")


if __name__ == "__main__":
    main()

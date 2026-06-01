#!/usr/bin/env python3
"""
self_summarizer.py — Self-Summarizing Project System v1.0

SELF-SUMMARIZING PROJECT SYSTEM (requirement #9):
Generates automatic summaries at multiple levels:
  - Module summaries
  - File summaries
  - Architecture summaries
  - DSP flow summaries
  - UI ↔ DSP communication summaries

Multi-Layer Context Architecture (requirement #10):
  Layer 1 → architecture summary
  Layer 2 → module summary
  Layer 3 → symbol summary
  Layer 4 → exact code

Usage:
    python self_summarizer.py --all                          # Generate all summaries
    python self_summarizer.py --module "Common"              # Summarize a module
    python self_summarizer.py --file Source/Common/Types.h   # Summarize a file
    python self_summarizer.py --architecture                  # Architecture summary
    python self_summarizer.py --dsp-flow                      # DSP flow summary
    python self_summarizer.py --layered-context "query"       # Multi-layer context for query
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
SOURCE_DIR = PROJECT_ROOT / "Source"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"


# ─── Loaders ───────────────────────────────────────────────────────────────
def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


# ═══════════════════════════════════════════════════════════════════════════
#  SelfSummarizer — Multi-Layer Summarization Engine
# ═══════════════════════════════════════════════════════════════════════════
class SelfSummarizer:
    """Generates intelligent summaries of the project at multiple layers."""

    def __init__(self):
        self.symbol_graph = load_json(SYMBOL_GRAPH_PATH)
        self.project_graph = load_json(PROJECT_GRAPH_PATH)
        self.project_index = load_json(PROJECT_INDEX_PATH)
        self.symbols = self.symbol_graph.get("symbols", {})
        self.graph_files = self.project_graph.get("files", {})
        self.modules_data = self.project_index.get("modules", [])

    # ═══════════════════════════════════════════════════════════════════
    #  LAYER 1: Architecture Summary
    # ═══════════════════════════════════════════════════════════════════

    def summarize_architecture(self):
        """Generate high-level architecture summary (Layer 1)."""
        modules = self._get_module_summaries()
        subsystems = self.project_graph.get("subsystems", {})

        # Build communication flow
        flows = []
        if "dsp" in subsystems:
            flows.append("Audio Signal: Input Buffer -> FFT Analysis -> Spectrum Data -> UI Render")
        if "ipc" in subsystems:
            flows.append("IPC: Messenger(per-track) -> Shared Memory -> MixCoach(Master)")
        if "ui" in subsystems:
            flows.append("UI: Render Loop -> Timer Callback -> Read Analyzer Data -> Repaint")
        if "messenger" in subsystems:
            flows.append("Telemetry: ProcessBlock -> Peak/RMS/LUFS -> Write Slot -> Sync to Shared Memory")

        # Critical paths
        critical_paths = self.project_graph.get("critical_paths", {})
        dsp_chain = [self.graph_files.get(f, {}).get("role", f.split("/")[-1])
                     for f in critical_paths.get("dsp_critical_chain", [])[:6]]
        ipc_chain = [self.graph_files.get(f, {}).get("role", f.split("/")[-1])
                     for f in critical_paths.get("ipc_critical_chain", [])[:6]]

        return {
            "layer": 1,
            "title": "MixCoach Architecture Overview",
            "version": "2.0",
            "plugins": [
                {"name": "MixCoach", "role": "Brain/Master", "installed_on": "Master channel"},
                {"name": "Messenger", "role": "Ears/Telemetry", "installed_on": "Per-track"},
            ],
            "modules": modules,
            "communication": {
                "ipc_mechanism": "Shared Memory (CreateFileMappingW) + Backup Files",
                "data_flow": [
                    "1. Messenger captures audio telemetry (peak, RMS, LUFS, FFT, phase)",
                    "2. Data written to shared memory slot + backup file",
                    "3. MixCoach reads from shared memory on timer callback",
                    "4. Analyzer processes and renders spectrum/meters",
                    "5. CoachEngine provides AI recommendations via chat",
                ],
                "audio_thread": "Lock-free atomic reads, no allocations",
                "ui_thread": "Timer-based polling (60fps), smooth coefficients",
            },
            "dsp_chain": " -> ".join(dsp_chain) if dsp_chain else "N/A",
            "ipc_chain": " -> ".join(ipc_chain) if ipc_chain else "N/A",
            "flows": flows,
            "file_count": len(self.graph_files),
            "symbol_count": len(self.symbols),
        }

    def _get_module_summaries(self):
        """Get summaries of all modules."""
        summaries = {}
        for module in self.modules_data:
            name = module.get("name", "Unknown")
            files = module.get("files", [])
            keywords = set()
            types = set()
            for f in files:
                keywords.update(f.get("keywords", []))
                types.add(f.get("type", ""))

            summaries[name] = {
                "file_count": len(files),
                "key_types": list(types - {""}),
                "top_keywords": list(keywords)[:10],
                "description": module.get("description", ""),
            }
        return summaries

    # ═══════════════════════════════════════════════════════════════════
    #  LAYER 2: Module Summary
    # ═══════════════════════════════════════════════════════════════════

    def summarize_module(self, module_name):
        """Generate detailed module summary (Layer 2)."""
        module_data = None
        for m in self.modules_data:
            if m.get("name", "").lower() == module_name.lower():
                module_data = m
                break

        if not module_data:
            return {"error": f"Module not found: {module_name}"}

        files = module_data.get("files", [])
        file_details = []
        for f in files:
            path = f.get("path", "")
            node = self.graph_files.get(path, {})
            symbols_in_file = self.symbol_graph.get("by_file", {}).get(path, [])

            file_details.append({
                "path": path,
                "role": node.get("role", "?"),
                "criticality": node.get("criticality", 0),
                "description": f.get("description", ""),
                "symbols": symbols_in_file[:8],
                "deps": len(node.get("depends_on", [])),
                "used_by": len(node.get("depended_by", [])),
            })

        # Sort by criticality
        file_details.sort(key=lambda x: -x["criticality"])

        return {
            "layer": 2,
            "name": module_data.get("name", ""),
            "description": module_data.get("description", ""),
            "files": file_details,
            "total_files": len(files),
            "critical_files": sum(1 for f in file_details if f["criticality"] >= 8),
            "key_patterns": self._detect_patterns(module_name, file_details),
        }

    def _detect_patterns(self, module_name, files):
        """Detect common patterns in a module."""
        patterns = []
        roles = defaultdict(int)
        for f in files:
            roles[f["role"]] += 1

        if roles.get("Header", 0) > 0 and roles.get("Source", 0) > 0:
            patterns.append("Header/Source pairs (matching .h/.cpp files)")
        if any(f.get("used_by", 0) >= 5 for f in files):
            patterns.append("High-ripple files (used by 5+ other files)")
        if any(f.get("criticality", 0) >= 9 for f in files):
            patterns.append("Contains critical shared types/definitions")

        return patterns

    # ═══════════════════════════════════════════════════════════════════
    #  LAYER 3: Symbol Summary
    # ═══════════════════════════════════════════════════════════════════

    def summarize_file(self, file_path):
        """Generate symbol-level summary for a file (Layer 3)."""
        normalized = file_path.replace("\\\\", "/")
        symbols_in_file = self.symbol_graph.get("by_file", {}).get(normalized, [])

        classes = []
        methods = []
        enums_list = []
        callbacks = []
        structs = []

        for sym_name in symbols_in_file:
            sym = self.symbols.get(sym_name, {})
            sym_type = sym.get("type", "")
            if sym_type in ("class", "struct"):
                entry = {
                    "name": sym_name,
                    "type": sym_type,
                    "line": sym.get("line", 0),
                    "base_classes": sym.get("base_classes", []),
                    "methods_count": len(sym.get("methods", [])),
                    "is_abstract": sym.get("is_abstract", False),
                }
                if sym_type == "class":
                    classes.append(entry)
                else:
                    structs.append(entry)
            elif sym_type == "method":
                methods.append({
                    "name": sym_name,
                    "return_type": sym.get("return_type", ""),
                    "params": sym.get("params", ""),
                    "class": sym.get("class", ""),
                    "virtual": sym.get("virtual", False),
                    "override": sym.get("override", False),
                })
            elif sym_type == "enum":
                enums_list.append({
                    "name": sym_name,
                    "values_count": len(sym.get("values", [])),
                })
            elif sym_type == "callback":
                callbacks.append({
                    "name": sym_name,
                    "signature": sym.get("signature", ""),
                })

        node = self.graph_files.get(normalized, {})
        role = node.get("role", "?")
        module = node.get("module", "?")
        criticality = node.get("criticality", 0)

        return {
            "layer": 3,
            "file": normalized,
            "module": module,
            "role": role,
            "criticality": criticality,
            "classes": classes,
            "structs": structs,
            "methods": methods[:20],
            "enums": enums_list,
            "callbacks": callbacks,
            "total_symbols": len(symbols_in_file),
        }

    # ═══════════════════════════════════════════════════════════════════
    #  DSP Flow Summary
    # ═══════════════════════════════════════════════════════════════════

    def summarize_dsp_flow(self):
        """Generate DSP-specific data flow summary."""
        critical_paths = self.project_graph.get("critical_paths", {})
        dsp_chain = critical_paths.get("dsp_critical_chain", [])

        flow_steps = []
        for i, f in enumerate(dsp_chain):
            node = self.graph_files.get(f, {})
            symbols = self.symbol_graph.get("by_file", {}).get(f, [])
            flow_steps.append({
                "step": i + 1,
                "file": f,
                "role": node.get("role", "?"),
                "key_symbols": symbols[:5],
                "criticality": node.get("criticality", 0),
            })

        return {
            "title": "DSP Data Flow",
            "realtime_path": True,
            "thread_safety": "Lock-free atomic operations on audio thread",
            "critical_rules": [
                "NO allocations on audio thread",
                "NO blocking operations on audio thread",
                "NO file I/O on audio thread",
                "UI reads are atomic and non-blocking",
                "FFT buffers must be pre-allocated",
            ],
            "flow_steps": flow_steps,
            "key_patterns": [
                "processBlock() -> FFT analysis -> SpectrumData -> IPC write",
                "Timer callback -> IPC read -> Smoothing -> UI repaint",
                "SlotRegistry: lock-free SPMC queue via atomic indices",
            ],
        }

    def summarize_ipc_flow(self):
        """Generate IPC communication flow summary."""
        critical_paths = self.project_graph.get("critical_paths", {})
        ipc_chain = critical_paths.get("ipc_critical_chain", [])

        return {
            "title": "IPC Communication Flow (Messenger ↔ MixCoach)",
            "mechanism": "Shared Memory (CreateFileMappingW) + Backup Files",
            "protocol": {
                "write": "Messenger writes to slot -> backup file -> signal ready",
                "read": "MixCoach polls slots -> reads if ready -> processes data",
                "recovery": "On SHM failure, reads from backup files",
            },
            "chain": ipc_chain,
            "critical_files": [
                "SharedMemory.h/cpp: CreateFileMapping, MapViewOfFile, synchronization",
                "SlotRegistry.h/cpp: Slot allocation, lock-free access, backup I/O",
                "SharedData.h/cpp: Data structures passed between plugins",
            ],
            "thread_safety": {
                "audio_thread": "Write-only, lock-free atomic",
                "message_thread": "Read-only via timer callback",
                "background_thread": "Backup file I/O protected by bgLock_",
            },
        }

    # ═══════════════════════════════════════════════════════════════════
    #  LAYER 4: Code Retrieval (Exact Code)
    # ═══════════════════════════════════════════════════════════════════

    def get_layered_context(self, query):
        """Get multi-layer context for a query, from summary to exact code."""
        query_lower = query.lower()

        # Layer 1: Architecture (always included)
        arch = self.summarize_architecture()

        # Layer 2: Identify relevant module
        target_module = self._identify_module(query_lower)
        module_summary = self.summarize_module(target_module) if target_module else None

        # Layer 3: Identify relevant files
        relevant_symbols = self._find_relevant_symbols(query_lower)
        file_summaries = []
        for sym in relevant_symbols[:10]:
            file_path = sym.get("file", "")
            if file_path and file_path not in [f.get("file") for f in file_summaries]:
                file_summaries.append(self.summarize_file(file_path))

        # Layer 4: Full code files (read from disk)
        code_files = []
        token_estimate = 0
        for fs in file_summaries[:5]:
            file_path = fs.get("file", "")
            if file_path:
                full_path = PROJECT_ROOT / file_path
                if full_path.exists():
                    try:
                        content = full_path.read_text(encoding="utf-8")
                        char_count = len(content)
                        est_tokens = int(char_count * 0.28)
                        token_estimate += est_tokens
                        code_files.append({
                            "path": file_path,
                            "content_length": char_count,
                            "estimated_tokens": est_tokens,
                            "content": content,
                        })
                    except Exception:
                        pass

        return {
            "query": query,
            "token_estimate": token_estimate,
            "layer1_architecture": arch,
            "layer2_module": module_summary,
            "layer3_file_summaries": file_summaries[:8],
            "layer4_code": code_files,
            "tree_structure": {
                "layer1": "Architecture overview (~500 tokens)",
                "layer2": f"Module summary (~200 tokens)" if module_summary else "Not needed",
                "layer3": f"File summaries (~100 tokens each, {len(file_summaries)} files)",
                "layer4": f"Full code (~{token_estimate} tokens, {len(code_files)} files)",
            },
        }

    def _identify_module(self, query_lower):
        """Identify the most relevant module for a query."""
        module_keywords = {
            "Common": ["types", "shared", "common", "ipc", "memory", "slot", "audio analysis",
                       "constants", "telemetry", "log"],
            "MixCoach Brain": ["dsp", "fft", "analyzer", "coach", "engine", "phase", "mentor",
                               "recommendation", "process"],
            "MixCoach UI": ["ui", "panel", "tab", "chat", "dashboard", "theme", "render",
                            "component", "visual"],
            "Messenger Ears": ["messenger", "telemetry", "collector", "ear", "track plugin"],
            "Build System": ["build", "cmake", "deploy", "compile", "vst3"],
        }

        max_score = 0
        best_module = None
        for module, keywords in module_keywords.items():
            score = sum(1 for kw in keywords if kw in query_lower)
            if score > max_score:
                max_score = score
                best_module = module

        return best_module

    def _find_relevant_symbols(self, query_lower):
        """Find symbols relevant to a query."""
        query_terms = set(re.findall(r'\b[a-z]+\b', query_lower))
        scored = []

        for sym_name, sym in self.symbols.items():
            sym_lower = sym_name.lower()
            score = 0
            for term in query_terms:
                if term in sym_lower:
                    score += 1
                if len(term) >= 3 and term in sym.get("file", "").lower():
                    score += 0.5
            if score > 0:
                scored.append((score, sym))

        scored.sort(key=lambda x: -x[0])
        return [s[1] for s in scored[:20]]

    # ═══════════════════════════════════════════════════════════════════
    #  Batch Summarization
    # ═══════════════════════════════════════════════════════════════════

    def generate_all_summaries(self):
        """Generate all summaries at once."""
        return {
            "architecture": self.summarize_architecture(),
            "dsp_flow": self.summarize_dsp_flow(),
            "ipc_flow": self.summarize_ipc_flow(),
            "modules": {
                m.get("name", "Unknown"): self.summarize_module(m.get("name", ""))
                for m in self.modules_data
            },
            "generated": datetime.now().isoformat(),
        }


# ═══════════════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════════════
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Self-Summarizer for MixCoach")
    parser.add_argument("--all", action="store_true", help="Generate all summaries")
    parser.add_argument("--architecture", action="store_true", help="Architecture summary")
    parser.add_argument("--module", metavar="NAME", help="Summarize a module")
    parser.add_argument("--file", metavar="FILE", help="Summarize a file")
    parser.add_argument("--dsp-flow", action="store_true", help="DSP flow summary")
    parser.add_argument("--ipc-flow", action="store_true", help="IPC flow summary")
    parser.add_argument("--layered-context", metavar='"QUERY"',
                        help="Multi-layer context for a query")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    summarizer = SelfSummarizer()

    if args.architecture or args.all:
        arch = summarizer.summarize_architecture()
        if args.json:
            print(json.dumps(arch, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  {arch['title']}")
            print(f"{'='*60}")
            print(f"  Plugins: {', '.join(p['role'] for p in arch['plugins'])}")
            print(f"  IPC: {arch['communication']['ipc_mechanism']}")
            print(f"  Modules: {', '.join(arch['modules'].keys())}")
            print(f"  Files: {arch['file_count']} | Symbols: {arch['symbol_count']}")
            print(f"\n  Data Flow:")
            for step in arch['communication']['data_flow']:
                print(f"    {step}")
            print(f"\n  DSP Chain: {arch['dsp_chain']}")
            print(f"  IPC Chain: {arch['ipc_chain']}")

    if args.dsp_flow or args.all:
        dsp = summarizer.summarize_dsp_flow()
        if args.json:
            print(json.dumps(dsp, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  {dsp['title']}")
            print(f"{'='*60}")
            print(f"  Realtime: {dsp['realtime_path']}")
            print(f"  Thread safety: {dsp['thread_safety']}")
            print(f"\n  Critical Rules:")
            for rule in dsp['critical_rules']:
                print(f"    ! {rule}")

    if args.ipc_flow or args.all:
        ipc = summarizer.summarize_ipc_flow()
        if args.json:
            print(json.dumps(ipc, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  {ipc['title']}")
            print(f"{'='*60}")
            print(f"  Mechanism: {ipc['mechanism']}")
            print(f"\n  Protocol:")
            for direction, desc in ipc.get('protocol', {}).items():
                print(f"    {direction}: {desc}")

    if args.module:
        module = summarizer.summarize_module(args.module)
        if args.json:
            print(json.dumps(module, indent=2, ensure_ascii=False))
        else:
            if "error" in module:
                print(f"ERROR: {module['error']}")
            else:
                print(f"\n{'='*60}")
                print(f"  Module: {module['name']}")
                print(f"{'='*60}")
                print(f"  {module['description']}")
                print(f"  Files: {module['total_files']} ({module['critical_files']} critical)")
                if module.get("key_patterns"):
                    print(f"\n  Patterns:")
                    for p in module["key_patterns"]:
                        print(f"    * {p}")
                print(f"\n  Top Files:")
                for f in module["files"][:8]:
                    print(f"    [{f['criticality']}/10] {f['path']}")
                    print(f"      {f['description']}")

    if args.file:
        fs = summarizer.summarize_file(args.file)
        if args.json:
            print(json.dumps(fs, indent=2, ensure_ascii=False))
        else:
            if "error" in fs:
                print(f"ERROR: {fs['error']}")
            else:
                print(f"\n{'='*60}")
                print(f"  File: {fs['file']}")
                print(f"{'='*60}")
                print(f"  Module: {fs['module']} | Role: {fs['role']} | Crit: {fs['criticality']}/10")
                if fs['classes']:
                    print(f"\n  Classes ({len(fs['classes'])}):")
                    for c in fs['classes']:
                        bases = f" -> {', '.join(c['base_classes'])}" if c['base_classes'] else ""
                        print(f"    {c['name']}{bases}")
                if fs['structs']:
                    print(f"\n  Structs ({len(fs['structs'])}):")
                    for s in fs['structs']:
                        print(f"    {s['name']}")
                if fs['enums']:
                    print(f"\n  Enums ({len(fs['enums'])}):")
                    for e in fs['enums']:
                        print(f"    {e['name']} ({e['values_count']} values)")
                if fs['callbacks']:
                    print(f"\n  Callbacks:")
                    for cb in fs['callbacks']:
                        print(f"    {cb['name']}: {cb['signature'][:60]}")
                print(f"\n  Methods ({len(fs['methods'])}):")
                for m in fs['methods'][:8]:
                    v = "V" if m['virtual'] else " "
                    o = "O" if m['override'] else " "
                    print(f"    [{v}{o}] {m['return_type']} {m['name'].split('::')[-1]}({m['params'][:30]})")

    if args.layered_context:
        context = summarizer.get_layered_context(args.layered_context)
        if args.json:
            print(json.dumps(context, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  Layered Context for: '{args.layered_context}'")
            print(f"{'='*60}")
            print(f"  Estimated tokens (full code): {context['token_estimate']}")
            print(f"\n  Tree Structure:")
            for layer, desc in context.get('tree_structure', {}).items():
                print(f"    {layer}: {desc}")
            print(f"\n  Layer 1 - Architecture:")
            print(f"    {context['layer1_architecture']['title']}")
            print(f"\n  Layer 3 - Relevant Files ({len(context['layer3_file_summaries'])}):")
            for fs in context['layer3_file_summaries'][:5]:
                print(f"    [{fs['criticality']}/10] {fs['file']}")
                if fs['classes']:
                    print(f"      Classes: {', '.join(c['name'] for c in fs['classes'][:3])}")
            print(f"\n  Layer 4 - Full Code ({len(context['layer4_code'])} files):")
            for cf in context['layer4_code']:
                print(f"    {cf['path']} ({cf['estimated_tokens']} tokens)")


if __name__ == "__main__":
    main()

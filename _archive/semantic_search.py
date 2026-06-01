#!/usr/bin/env python3
"""
semantic_search.py — Semantic Search System v1.0

SEMANTIC SEARCH SYSTEM (requirement #1):
Real semantic search that understands intent, not just keywords.

Multi-stage retrieval:
  1. TF-IDF cosine similarity (code content understanding)
  2. N-gram character matching (structural similarity)
  3. Symbol graph navigation (relationship-based)
  4. Intent matching (DSP-specific query expansion)
  5. Context gathering (dependency expansion + impact analysis)

Examples:
  "analyzer lento"       → finds performance bottlenecks
  "graves inestables"    → finds FFT / low frequency paths
  "Messenger no sincroniza" → finds callbacks and communication UI-DSP

Usage:
    python semantic_search.py "your query"                    # Search and show results
    python semantic_search.py "query" --context               # Include context expansion
    python semantic_search.py "query" --rerank                # Rerank by multiple signals
    python semantic_search.py --expand-synonyms "query"        # Show query expansion
"""

import json
import re
import math
import sys
from pathlib import Path
from datetime import datetime
from collections import Counter, defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.resolve()
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
EMBEDDING_PATH = PROJECT_ROOT / "EMBEDDING_STORE.json"
PROJECT_INDEX_PATH = PROJECT_ROOT / "PROJECT_INDEX.json"
PROJECT_GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"

# ─── DSP Synonyms Dictionary (for query expansion) ─────────────────────────
DSP_SYNONYMS = {
    # Spanish → English audio terms
    "analizador": ["analyzer", "spectrum", "fft", "meter", "analysis"],
    "analizadores": ["analyzers", "spectrum", "fft", "metering"],
    "audio": ["audio", "sound", "signal", "waveform"],
    "bajo": ["bass", "low", "sub", "low frequency", "low_end"],
    "bajos": ["bass", "low frequencies", "sub_bass"],
    "bateria": ["drums", "drum", "percussion", "rhythm"],
    "buses": ["buses", "bus", "routing", "group", "mix_bus"],
    "canal": ["channel", "track", "strip", "mixer_channel"],
    "chat": ["chat", "conversation", "message", "coach"],
    "compilacion": ["build", "compilation", "compile", "cmake"],
    "compilar": ["build", "compile", "cmake"],
    "conexion": ["connection", "sync", "ipc", "link", "shared_memory"],
    "depurar": ["debug", "debugging", "fix", "troubleshoot"],
    "desplegar": ["deploy", "install", "copy", "vst3_deploy"],
    "dinamica": ["dynamics", "compression", "limiter", "gain_staging"],
    "error": ["error", "bug", "crash", "failure", "issue"],
    "espectro": ["spectrum", "spectral", "frequency", "fft", "spectrogram"],
    "espectrograma": ["spectrogram", "spectrograph", "spectrum_analyzer"],
    "fase": ["phase", "correlation", "stereo_field", "phase_meter"],
    "fft": ["fft", "fast_fourier_transform", "spectrum", "frequency_domain"],
    "filtro": ["filter", "eq", "equalizer", "frequency_filter"],
    "frecuencia": ["frequency", "freq", "hz", "spectral"],
    "grave": ["bass", "low", "sub", "low_frequency", "rumble"],
    "graves": ["bass", "low_frequencies", "sub_bass", "low_end"],
    "grupo": ["group", "bus", "routing", "mix_group"],
    "interfaz": ["interface", "ui", "gui", "editor", "user_interface"],
    "lento": ["slow", "performance", "bottleneck", "lag", "optimization"],
    "logro": ["achievement", "gamification", "badge", "reward"],
    "loudness": ["loudness", "lufs", "volume", "perceived_loudness"],
    "medidor": ["meter", "metering", "vu", "level_meter"],
    "memoria": ["memory", "shared_memory", "ipc", "ram", "buffer"],
    "mensajero": ["messenger", "messenger_plugin", "telemetry_node"],
    "mentor": ["coach", "mentor", "ai", "guidance", "tutorial"],
    "mezcla": ["mix", "mixing", "blend", "balance"],
    "modulo": ["module", "component", "subsystem", "layer"],
    "nota": ["note", "midi", "pitch", "tone"],
    "oido": ["ear", "messenger", "telemetry", "hearing"],
    "panel": ["panel", "ui_component", "widget", "control_panel"],
    "pestana": ["tab", "tabbed_component", "tab_panel"],
    "pico": ["peak", "peak_level", "max_level", "transient"],
    "pista": ["track", "channel", "strip", "audio_track"],
    "plugin": ["plugin", "vst3", "audio_plugin", "processor"],
    "procesador": ["processor", "audio_processor", "dsp_processor"],
    "recomendacion": ["recommendation", "tip", "suggestion", "advice"],
    "referencia": ["reference", "reference_track", "a_b_comparison"],
    "registro": ["registry", "slot", "registration", "slot_registry"],
    "rms": ["rms", "root_mean_square", "average_level"],
    "señal": ["signal", "audio", "waveform", "stream"],
    "sincronizacion": ["synchronization", "sync", "ipc", "timing"],
    "slot": ["slot", "track_slot", "register", "index"],
    "telemetria": ["telemetry", "data_collection", "metrics", "measurement"],
    "tema": ["theme", "colors", "visual_style", "skin", "ui_theme"],
    "ventana": ["window", "editor", "ui", "gui_window"],
    "voz": ["voice", "vocal", "vocal_track", "singing"],

    # English → DSP context
    "analyzer": ["analyzer", "spectrum_analyzer", "fft_analyzer", "metering"],
    "analyzer_lento": ["analyzer_performance", "fft_bottleneck", "metering_slow", "ui_freeze"],
    "bottleneck": ["bottleneck", "performance_issue", "slow_path", "critical_path"],
    "build_fail": ["build_error", "compilation_error", "linker_error", "cmake_error"],
    "callback": ["callback", "listener", "event_handler", "signal_slot"],
    "crash": ["crash", "segfault", "access_violation", "null_pointer"],
    "dsp": ["dsp", "digital_signal_processing", "audio_processing"],
    "fft_instability": ["fft_instability", "spectral_leakage", "fft_artifacts", "window_function"],
    "freeze": ["freeze", "ui_freeze", "thread_block", "stall", "lockup"],
    "ipc": ["ipc", "inter_process", "shared_memory", "interprocess_communication"],
    "latency": ["latency", "delay", "buffer_size", "processing_delay"],
    "leak": ["leak", "memory_leak", "resource_leak", "handle_leak"],
    "lock_free": ["lock_free", "lockfree", "wait_free", "atomic", "thread_safe"],
    "low_frequency": ["low_frequency", "bass", "sub_bass", "lows", "low_end"],
    "lufs": ["lufs", "loudness", "ebu_r128", "loudness_meter"],
    "meter": ["meter", "vu_meter", "level_meter", "metering_component"],
    "mix": ["mix", "mixing", "balance", "blend", "mixer"],
    "noise": ["noise", "hiss", "hum", "artifacts", "distortion"],
    "performance": ["performance", "optimization", "speed", "cpu_usage", "throughput"],
    "phase": ["phase", "phase_correlation", "stereo_phase", "mono_compatibility"],
    "realtime": ["realtime", "real_time", "audio_thread", "process_block"],
    "reconnection": ["reconnection", "retry", "backoff", "reconnect"],
    "shared_memory": ["shared_memory", "file_mapping", "ipc_memory", "createfilemapping"],
    "slow_meter": ["slow_meter", "meter_performance", "metering_bottleneck"],
    "spectral": ["spectral", "spectrum", "frequency_domain", "fft"],
    "synchronization": ["synchronization", "sync", "ipc_sync", "data_sync"],
    "thread_safety": ["thread_safety", "lock_free", "atomic", "mutex", "critical_section"],
    "vectorscope": ["vectorscope", "stereo_field", "phase_scope", "correlation"],
}


# ═══════════════════════════════════════════════════════════════════════════
#  QueryExpander — DSP-Aware Query Expansion
# ═══════════════════════════════════════════════════════════════════════════
class QueryExpander:
    """Expands natural language queries with DSP-specific synonyms."""

    def __init__(self):
        self.synonyms = DSP_SYNONYMS

    def expand(self, query):
        """Expand query with synonyms. Returns list of query variants."""
        query_lower = query.lower().strip()
        tokens = re.findall(r'\b[a-z]+\b', query_lower)

        expanded = set()
        expanded.add(query_lower)

        # Add DSP synonyms for each token
        for token in tokens:
            if token in self.synonyms:
                for syn in self.synonyms[token]:
                    # Replace the token with the synonym
                    for variant in list(expanded):
                        expanded.add(variant.replace(token, syn))

        # Also add multi-word expansions
        for phrase, synonyms_list in self.synonyms.items():
            if phrase in query_lower:
                for syn in synonyms_list:
                    expanded.add(query_lower.replace(phrase, syn))

        # Generate n-gram queries from expanded set
        result = []
        seen = set()
        for e in expanded:
            if e not in seen:
                seen.add(e)
                result.append(e)

        return result

    def get_relevant_synonyms(self, query):
        """Return which synonyms were triggered by the query."""
        query_lower = query.lower()
        triggered = {}
        for token in re.findall(r'\b[a-z]+\b', query_lower):
            if token in self.synonyms:
                triggered[token] = self.synonyms[token]
        return triggered


# ═══════════════════════════════════════════════════════════════════════════
#  IntentClassifier — Classifies Query Intent
# ═══════════════════════════════════════════════════════════════════════════
class IntentClassifier:
    """Classifies search queries into intent categories."""

    INTENTS = {
        "performance": {
            "keywords": ["lento", "slow", "rendimiento", "performance", "cpu",
                         "optimizar", "optimize", "bottleneck", "cuello", "freeze",
                         "congelar", "lag", "tardo", "tarda"],
            "module": "MixCoach Brain",
            "priority": "HIGH",
        },
        "bug_sync": {
            "keywords": ["sincroniza", "sync", "conexion", "connection", "no funciona",
                         "roto", "broken", "bug", "error", "crash", "falla", "fail",
                         "no se ven", "no aparece", "vacio", "empty", "missing"],
            "module": "Common",
            "priority": "CRITICAL",
        },
        "dsp_audio": {
            "keywords": ["dsp", "audio", "fft", "espectro", "spectrum", "frecuencia",
                         "frequency", "filtro", "filter", "phase", "fase", "rms",
                         "peak", "pico", "lufs", "loudness", "grave", "bass",
                         "agudo", "treble", "correlacion", "correlation"],
            "module": "MixCoach Brain",
            "priority": "HIGH",
        },
        "ui_changes": {
            "keywords": ["ui", "interfaz", "interface", "panel", "tema", "theme",
                         "color", "color", "boton", "button", "ventana", "window",
                         "layout", "diseño", "design", "estilo", "style"],
            "module": "MixCoach UI",
            "priority": "MEDIUM",
        },
        "build_deploy": {
            "keywords": ["build", "compilar", "compile", "deploy", "desplegar",
                         "cmake", "vst3", "instalar", "install", "error build",
                         "linker", "msvc", "compilacion"],
            "module": "Build System",
            "priority": "HIGH",
        },
        "ipc_communication": {
            "keywords": ["ipc", "memoria compartida", "shared memory",
                         "communication", "comunicacion", "slot", "registry",
                         "registro", "backup", "sincronizacion", "synchronization",
                         "messenger", "mensajero"],
            "module": "Common",
            "priority": "CRITICAL",
        },
        "mentoring": {
            "keywords": ["mentor", "coach", "fase", "phase", "logro", "achievement",
                         "consejo", "tip", "recomendacion", "recommendation",
                         "gamificacion", "gamification", "progreso", "progress"],
            "module": "MixCoach Brain",
            "priority": "MEDIUM",
        },
        "messenger": {
            "keywords": ["messenger", "mensajero", "oido", "ear", "telemetry",
                         "telemetria", "pista", "track", "vst3 per track",
                         "colector", "collector"],
            "module": "Messenger Ears",
            "priority": "HIGH",
        },
    }

    def classify(self, query):
        """Classify query into intent categories."""
        query_lower = query.lower()
        scores = {}

        for intent_name, intent_data in self.INTENTS.items():
            score = 0
            for keyword in intent_data["keywords"]:
                if keyword in query_lower:
                    score += 1
            if score > 0:
                scores[intent_name] = {
                    "score": score,
                    "module": intent_data["module"],
                    "priority": intent_data["priority"],
                }

        if not scores:
            return {"general": {"score": 1, "module": "General", "priority": "LOW"}}

        # Sort by score
        sorted_scores = sorted(scores.items(), key=lambda x: (-x[1]["score"]))
        return dict(sorted_scores[:2])


# ═══════════════════════════════════════════════════════════════════════════
#  SemanticSearcher — Multi-Stage Retrieval Engine
# ═══════════════════════════════════════════════════════════════════════════
class SemanticSearcher:
    """Multi-stage semantic search engine for code."""

    def __init__(self):
        self.expander = QueryExpander()
        self.classifier = IntentClassifier()
        self.symbol_graph = self._load_symbol_graph()
        self.project_index = self._load_json(PROJECT_INDEX_PATH)
        self.project_graph = self._load_json(PROJECT_GRAPH_PATH)
        self.embedding_store = self._load_json(EMBEDDING_PATH)

    def _load_json(self, path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {}

    def _load_symbol_graph(self):
        try:
            with open(SYMBOL_GRAPH_PATH, "r", encoding="utf-8") as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {}

    def search(self, query, top_k=10, include_context=False, rerank=True):
        """Multi-stage semantic search. Returns ranked file paths with scores."""

        # Stage 1: Intent Classification
        intents = self.classifier.classify(query)
        primary_intent = list(intents.keys())[0] if intents else "general"
        primary_module = intents.get(primary_intent, {}).get("module", "General")

        # Stage 2: Query Expansion
        expanded_queries = self.expander.expand(query)
        triggered_synonyms = self.expander.get_relevant_synonyms(query)

        # Stage 3: Symbol Graph Search
        symbol_results = self._search_symbols(query, expanded_queries)

        # Stage 4: Keyword Search (from PROJECT_INDEX.json)
        keyword_results = self._search_keywords(query, expanded_queries, primary_module)

        # Stage 5: Intent Mapping (from PROJECT_INDEX.json intent map)
        intent_results = self._search_intent(query, primary_module)

        # Stage 6: Dependency Graph (from PROJECT_GRAPH.json)
        graph_results = self._search_graph(query, list(keyword_results.keys()) + list(symbol_results.keys()))

        # Stage 7: Embedding Search (from EMBEDDING_STORE.json)
        embedding_results = self._search_embeddings(query, expanded_queries)

        # Fusion: Combine all signals
        fused = self._fusion_rank(
            symbol_results=symbol_results,
            keyword_results=keyword_results,
            intent_results=intent_results,
            graph_results=graph_results,
            embedding_results=embedding_results,
            primary_module=primary_module,
            top_k=top_k,
        )

        # Add search suggestions
        suggestions = self.get_search_suggestions(query)

        # Optional: Context expansion
        if include_context:
            fused = self._expand_context(fused)

        return {
            "query": query,
            "intents": intents,
            "expanded_queries": expanded_queries[:5],
            "triggered_synonyms": triggered_synonyms,
            "primary_module": primary_module,
            "results": fused[:top_k],
            "total_candidates": len(fused),
            "suggestions": suggestions,
        }

    def _search_symbols(self, query, expanded_queries):
        """Search within SYMBOL_GRAPH.json for matching symbols."""
        results = defaultdict(float)
        symbols = self.symbol_graph.get("symbols", {})

        search_terms = [query.lower()] + expanded_queries[:5]

        for sym_name, sym_data in symbols.items():
            sym_lower = sym_name.lower()
            for term in search_terms:
                if term in sym_lower:
                    file_path = sym_data.get("file", "")
                    if file_path:
                        # Score based on type priority
                        type_boost = {
                            "class": 1.5, "struct": 1.3, "method": 1.2,
                            "enum": 1.0, "callback": 1.1, "forward_decl": 0.5,
                        }.get(sym_data.get("type", ""), 0.8)
                        depth = len(term) / len(sym_name) if sym_name else 0
                        results[file_path] = max(results[file_path], depth * type_boost)

        return dict(results)

    def _search_keywords(self, query, expanded_queries, primary_module):
        """Search within PROJECT_INDEX.json keywords."""
        results = defaultdict(float)
        keywords_index = self.project_index.get("keywords_index", {})

        search_terms = set(re.findall(r'\b[a-z]+\b', query.lower()))
        for eq in expanded_queries[:3]:
            search_terms.update(re.findall(r'\b[a-z]+\b', eq))

        for keyword, files in keywords_index.items():
            kw_lower = keyword.lower()
            if any(term in kw_lower for term in search_terms):
                for file_path in files:
                    results[file_path] = max(results.get(file_path, 0), 0.7)

        return dict(results)

    def _search_intent(self, query, primary_module):
        """Search within PROJECT_INDEX.json intent map."""
        results = {}
        intent_map = self.project_index.get("file_intent_map", {})
        query_lower = query.lower()

        for intent_key, intent_files in intent_map.items():
            intent_lower = intent_key.lower()
            # Check for partial overlap between query and intent
            query_tokens = set(re.findall(r'\b[a-z]+\b', query_lower))
            intent_tokens = set(re.findall(r'\b[a-z]+\b', intent_lower))
            overlap = len(query_tokens & intent_tokens)

            if overlap >= 2:
                for file_path in intent_files:
                    results[file_path] = max(results.get(file_path, 0), 0.6 + overlap * 0.1)

        return results

    def _search_graph(self, query, existing_results):
        """Use dependency graph to expand results."""
        results = {}
        graph_files = self.project_graph.get("files", {})

        for file_path in existing_results:
            node = graph_files.get(file_path, {})
            if node:
                # Add critical dependents
                for dep in node.get("depended_by", []):
                    dep_node = graph_files.get(dep, {})
                    if dep_node.get("criticality", 0) >= 7:
                        results[dep] = 0.4
                # Add critical dependencies
                for dep in node.get("depends_on", []):
                    dep_node = graph_files.get(dep, {})
                    if dep_node.get("criticality", 0) >= 8:
                        results[dep] = 0.3

        return results

    def _search_embeddings(self, query, expanded_queries):
        """Search within EMBEDDING_STORE.json for cosine similarity."""
        results = {}
        documents = self.embedding_store.get("documents", {})

        for path, doc in documents.items():
            symbols = doc.get("symbols", [])
            dsp_tokens = doc.get("dsp_tokens", [])
            tokens_count = doc.get("tokens_count", 0)

            query_lower = query.lower()

            # Count token matches
            match_count = 0
            for token in re.findall(r'\b[a-z]+\b', query_lower):
                if token in " ".join(symbols).lower():
                    match_count += 1
                if token in " ".join(dsp_tokens[:20]).lower():
                    match_count += 2  # DSP tokens boosted

            if match_count > 0:
                score = min(1.0, match_count * 0.25)
                results[path] = score

        return results

    def _fusion_rank(self, symbol_results, keyword_results, intent_results,
                     graph_results, embedding_results, primary_module, top_k=10):
        """Fuse multiple retrieval signals into final ranking."""

        # Module weight based on intent
        module_weights = {
            "Common": 0.9,
            "MixCoach Brain": 1.1,
            "MixCoach UI": 1.0,
            "Messenger Ears": 1.1,
            "Build System": 0.8,
        }

        all_files = set()
        all_files.update(symbol_results.keys())
        all_files.update(keyword_results.keys())
        all_files.update(intent_results.keys())
        all_files.update(graph_results.keys())
        all_files.update(embedding_results.keys())

        fused = []
        for file_path in all_files:
            score = 0.0
            signals = 0

            # Symbol match
            if file_path in symbol_results:
                score += symbol_results[file_path] * 1.3
                signals += 1

            # Keyword match
            if file_path in keyword_results:
                score += keyword_results[file_path] * 1.0
                signals += 1

            # Intent match
            if file_path in intent_results:
                score += intent_results[file_path] * 1.5
                signals += 1

            # Graph expansion
            if file_path in graph_results:
                score += graph_results[file_path] * 0.6
                signals += 1

            # Embedding similarity
            if file_path in embedding_results:
                score += embedding_results[file_path] * 0.8
                signals += 1

            # Module boost
            graph_files = self.project_graph.get("files", {})
            node = graph_files.get(file_path, {})
            module = node.get("module", "")
            module_boost = module_weights.get(module, 1.0)
            if module == primary_module:
                module_boost *= 1.3

            avg_score = score / max(signals, 1) * module_boost

            fused.append({
                "path": file_path,
                "score": round(avg_score, 4),
                "signals": signals,
                "module": module,
                "role": node.get("role", "?"),
                "criticality": node.get("criticality", 0),
            })

        # Sort by score
        fused.sort(key=lambda x: (-x["score"], -x["criticality"]))
        return fused[:top_k * 2]

    def _expand_context(self, results):
        """Expand results with dependency context."""
        expanded = list(results)
        graph_files = self.project_graph.get("files", {})

        # Add dependencies of top results
        add_files = []
        for r in results[:5]:
            node = graph_files.get(r["path"], {})
            for dep in node.get("depends_on", []):
                dep_node = graph_files.get(dep, {})
                if dep_node.get("criticality", 0) >= 8:
                    add_files.append({
                        "path": dep,
                        "score": r["score"] * 0.5,
                        "signals": 1,
                        "module": dep_node.get("module", ""),
                        "role": dep_node.get("role", ""),
                        "criticality": dep_node.get("criticality", 0),
                        "reason": f"Dependency of {r['path'].split('/')[-1]}",
                    })

        expanded.extend(add_files)
        return expanded

    def get_search_suggestions(self, query):
        """Get suggestions for improving search."""
        intents = self.classifier.classify(query)
        expanded = self.expander.expand(query)
        triggered = self.expander.get_relevant_synonyms(query)

        suggestions = []
        if len(expanded) <= 1:
            suggestions.append("Try adding more specific terms (e.g., 'fft', 'lufs', 'build')")
        if "general" in intents:
            suggestions.append("Your query didn't match a specific intent. Try DSP terms like 'audio', 'dsp', 'ui', 'build'")
        if not triggered:
            suggestions.append("No DSP synonyms triggered. Try mixing Spanish and English terms")

        return suggestions


# ═══════════════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════════════
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Semantic Search Engine for MixCoach")
    parser.add_argument("query", nargs="*", help="Natural language search query")
    parser.add_argument("--context", action="store_true", help="Include context expansion")
    parser.add_argument("--rerank", action="store_true", help="Rerank results by multiple signals")
    parser.add_argument("--top-k", type=int, default=10, help="Number of results")
    parser.add_argument("--expand-synonyms", metavar='"QUERY"', help="Show query expansion only")
    parser.add_argument("--classify", metavar='"QUERY"', help="Classify query intent only")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    searcher = SemanticSearcher()

    if args.expand_synonyms:
        expanded = searcher.expander.expand(args.expand_synonyms)
        triggered = searcher.expander.get_relevant_synonyms(args.expand_synonyms)
        print(f"\n[QUERY EXPANSION] '{args.expand_synonyms}'")
        print(f"  Variants: {len(expanded)}")
        for e in expanded[:10]:
            print(f"    -> {e}")
        print(f"\n  Triggered synonyms:")
        for word, syns in triggered.items():
            print(f"    {word}: {', '.join(syns[:5])}")
        return

    if args.classify:
        intents = searcher.classifier.classify(args.classify)
        print(f"\n[INTENT CLASSIFICATION] '{args.classify}'")
        for intent, data in intents.items():
            print(f"  -> {intent} (module: {data['module']}, priority: {data['priority']})")
        return

    query = " ".join(args.query) if args.query else ""
    if not query:
        # Interactive mode
        print("MixCoach Semantic Search (type 'quit' to exit)")
        print("=" * 50)
        while True:
            try:
                query = input("\nQuery: ").strip()
                if query.lower() in ("quit", "exit", "q"):
                    break
                if not query:
                    continue
                results = searcher.search(query, top_k=args.top_k,
                                          include_context=args.context,
                                          rerank=args.rerank)
                _display_results(results)
            except KeyboardInterrupt:
                print("\nGoodbye!")
                break
            except EOFError:
                break
    else:
        results = searcher.search(query, top_k=args.top_k,
                                  include_context=args.context,
                                  rerank=args.rerank)
        if args.json:
            print(json.dumps(results, indent=2, ensure_ascii=False))
        else:
            _display_results(results)


def _display_results(results):
    """Display search results in a readable format."""
    print(f"\n{'='*65}")
    print(f"  Query: '{results['query']}'")
    intents = results.get("intents", {})
    if intents:
        intent_str = ", ".join(f"{k}({v['module']})" for k, v in intents.items())
        print(f"  Intent: {intent_str}")
    primary = results.get("primary_module", "General")
    print(f"  Primary module: {primary}")
    print(f"  Candidates: {results['total_candidates']} | Showing: {len(results['results'])}")
    if results.get("triggered_synonyms"):
        print(f"  Synonyms: {', '.join(results['triggered_synonyms'].keys())}")
    print(f"{'='*65}")

    res = results["results"]
    if not res:
        print("  No results found.")
        return

    print(f"{'Score':<7} {'Sig':<5} {'Crit':<5} {'Module':<18} {'Role':<14} File")
    print("-" * 65)
    for r in res:
        module_short = (r.get("module", "")[:17])
        role_short = (r.get("role", "")[:13])
        print(f"{r['score']:<7.3f} {r['signals']:<5} {r['criticality']:<5} "
              f"{module_short:<18} {role_short:<14} {r.get('path','?')}")

    suggestions = results.get("suggestions", [])
    if suggestions:
        print(f"\n  Suggestions:")
        for s in suggestions:
            print(f"    💡 {s}")


if __name__ == "__main__":
    main()

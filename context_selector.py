#!/usr/bin/env python3
"""
context_selector.py — Smart Context Expansion v2.0 (AI-native)

Hybrid context retrieval engine that combines:
  - Semantic Search (query intent understanding)
  - Symbol Graph navigation (class/method dependencies)
  - AST Parsing results (structural relationships)
  - DSP Intelligence (domain-specific weighting)
  - Change Impact Analysis (precautionary context)
  - Multi-Layer Summarization (token-optimized output)

Usage:
    python context_selector.py "tu query en lenguaje natural"
    aider $(python context_selector.py "query")

Features:
    - Smart Context Expansion (query / main files / dependencies / headers / connected classes)
    - 5-pass scoring (intent_map, keywords, descriptions, semantic, symbols)
    - Module-penalty system (precise targeting)
    - Dependency graph + symbol graph integration
    - Token-aware output (max 5-8 files)
    - DSP-specific query expansion (synonyms)
    - Supports Spanish/English queries
"""

import json
import re
import sys
import os
from pathlib import Path
from collections import defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.resolve()
INDEX_PATH = PROJECT_ROOT / "PROJECT_INDEX.json"
GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"

# ─── Load Data ─────────────────────────────────────────────────────────────
def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return {}
    except json.JSONDecodeError:
        return {}

INDEX = load_json(INDEX_PATH)
GRAPH = load_json(GRAPH_PATH)
SYMBOL_GRAPH = load_json(SYMBOL_GRAPH_PATH)
SESSION = load_json(SESSION_PATH)

# ─── Constants ─────────────────────────────────────────────────────────────
MAX_FILES = 7  # maximum files to return

# Module criticality weights / prefer actionable modules
MODULE_WEIGHTS = {
    "Common": 0.9,
    "MixCoach (Brain)": 1.1,
    "MixCoach UI": 1.0,
    "Messenger (Ears)": 1.1,
    "Build System": 0.8,
    "Documentation & Config": 0.3,
}

# QUERY / MODULE priority mapping (extended with DSP-specific terms)
QUERY_MODULE_MAP = {
    # Audio / DSP
    "analyzer": ["MixCoach (Brain)"],
    "analizador": ["MixCoach (Brain)"],
    "audio": ["MixCoach (Brain)", "Common"],
    "dsp": ["MixCoach (Brain)", "Common"],
    "fft": ["MixCoach (Brain)", "Common"],
    "spectrum": ["MixCoach (Brain)", "Common"],
    "espectro": ["MixCoach (Brain)", "Common"],
    "rms": ["MixCoach (Brain)", "Common"],
    "peak": ["MixCoach (Brain)", "Common"],
    "pico": ["MixCoach (Brain)", "Common"],
    "lufs": ["MixCoach (Brain)", "Common"],
    "phase": ["MixCoach (Brain)"],
    "fase": ["MixCoach (Brain)"],
    "filter": ["MixCoach (Brain)", "Common"],
    "filtro": ["MixCoach (Brain)", "Common"],
    "grave": ["MixCoach (Brain)", "Common"],
    "bass": ["MixCoach (Brain)", "Common"],
    "latency": ["MixCoach (Brain)", "Common"],
    "buffer": ["MixCoach (Brain)", "Common"],
    "realtime": ["MixCoach (Brain)", "Common"],
    # Groups / Buses
    "group": ["MixCoach UI", "Common"],
    "grupo": ["MixCoach UI", "Common"],
    "bus": ["MixCoach UI", "Messenger (Ears)", "Common"],
    "selector": ["MixCoach UI"],
    "pista": ["MixCoach UI", "Messenger (Ears)"],
    "track": ["MixCoach UI", "Messenger (Ears)"],
    "mixer": ["MixCoach UI"],
    "routing": ["Messenger (Ears)", "MixCoach UI"],
    # Messenger / IPC
    "messenger": ["Messenger (Ears)", "Common"],
    "mensajero": ["Messenger (Ears)", "Common"],
    "oido": ["Messenger (Ears)"],
    "ear": ["Messenger (Ears)"],
    "telemetry": ["Messenger (Ears)", "Common"],
    "telemetria": ["Messenger (Ears)", "Common"],
    "slot": ["Common", "Messenger (Ears)"],
    "registry": ["Common"],
    "registro": ["Common"],
    "synchronization": ["Common", "MixCoach (Brain)", "Messenger (Ears)"],
    "sync": ["Common", "MixCoach (Brain)", "Messenger (Ears)"],
    "conexion": ["Common", "Messenger (Ears)"],
    "shared memory": ["Common"],
    "memoria compartida": ["Common"],
    "ipc": ["Common"],
    "lock_free": ["Common"],
    # UI
    "ui": ["MixCoach UI", "MixCoach (Brain)"],
    "interface": ["MixCoach UI"],
    "dashboard": ["MixCoach UI"],
    "tab": ["MixCoach UI"],
    "pestana": ["MixCoach UI"],
    "panel": ["MixCoach UI"],
    "chat": ["MixCoach UI"],
    "coach": ["MixCoach (Brain)", "MixCoach UI"],
    "theme": ["MixCoach UI"],
    "tema": ["MixCoach UI"],
    "analyzers": ["MixCoach UI", "MixCoach (Brain)"],
    "profesional": ["MixCoach UI"],
    # Build / Compile
    "build": ["Build System"],
    "compilacion": ["Build System"],
    "compile": ["Build System"],
    "compilar": ["Build System"],
    "deploy": ["Build System"],
    "desplegar": ["Build System"],
    "vst3": ["Build System", "MixCoach (Brain)", "Messenger (Ears)"],
    "error": ["Build System"],
    "linker": ["Build System"],
    "cmake": ["Build System"],
    # General
    "bug": ["MixCoach (Brain)", "Common", "Messenger (Ears)"],
    "fix": ["MixCoach (Brain)", "Common", "Messenger (Ears)"],
    "mejorar": ["MixCoach (Brain)", "Common", "MixCoach UI"],
    "improve": ["MixCoach (Brain)", "Common", "MixCoach UI"],
    "optimizar": ["MixCoach (Brain)", "Common"],
    "optimize": ["MixCoach (Brain)", "Common"],
    "performance": ["MixCoach (Brain)", "Common"],
    "rendimiento": ["MixCoach (Brain)", "Common"],
    "lento": ["MixCoach (Brain)", "Common"],
    "freeze": ["MixCoach UI", "Common"],
    # Coach / AI
    "recommendation": ["MixCoach (Brain)"],
    "recomendacion": ["MixCoach (Brain)"],
    "advice": ["MixCoach (Brain)"],
    "consejo": ["MixCoach (Brain)"],
    "ai": ["MixCoach (Brain)", "MixCoach UI"],
    "ia": ["MixCoach (Brain)", "MixCoach UI"],
    "mentor": ["MixCoach (Brain)"],
    # Coaching phases
    "fase": ["MixCoach (Brain)"],
    "phase": ["MixCoach (Brain)"],
    "logro": ["MixCoach (Brain)"],
    "achievement": ["MixCoach (Brain)"],
    "gamificacion": ["MixCoach (Brain)"],
    # Reference / Comparison
    "reference": ["MixCoach UI", "MixCoach (Brain)"],
    "referencia": ["MixCoach UI", "MixCoach (Brain)"],
    "comparar": ["MixCoach UI"],
    # DSP / Analysis specific
    "denormal": ["Common", "MixCoach (Brain)"],
    "nan": ["Common", "MixCoach (Brain)"],
    "smooth": ["MixCoach UI", "MixCoach (Brain)"],
    "suave": ["MixCoach UI"],
}

# DSP Synonyms Dictionary (for query expansion)
DSP_SYNONYMS = {
    # Spanish to English audio terms
    "analizador": ["analyzer", "spectrum", "fft", "meter", "analysis"],
    "analizadores": ["analyzers", "spectrum", "fft", "metering"],
    "audio": ["audio", "sound", "signal", "waveform"],
    "bajo": ["bass", "low", "sub", "low frequency", "low_end"],
    "bajos": ["bass", "low frequencies", "sub_bass"],
    "buses": ["buses", "bus", "routing", "group", "mix_bus"],
    "canal": ["channel", "track", "strip", "mixer_channel"],
    "compilacion": ["build", "compilation", "compile", "cmake"],
    "compilar": ["build", "compile", "cmake"],
    "conexion": ["connection", "sync", "ipc", "link", "shared_memory"],
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
    "sincronizacion": ["synchronization", "sync", "ipc", "timing"],
    "slot": ["slot", "track_slot", "register", "index"],
    "telemetria": ["telemetry", "data_collection", "metrics", "measurement"],
    "tema": ["theme", "colors", "visual_style", "skin", "ui_theme"],
    "voz": ["voice", "vocal", "vocal_track", "singing"],
    # English to DSP context
    "analyzer": ["analyzer", "spectrum_analyzer", "fft_analyzer", "metering"],
    "bottleneck": ["bottleneck", "performance_issue", "slow_path", "critical_path"],
    "crash": ["crash", "segfault", "access_violation", "null_pointer"],
    "dsp": ["dsp", "digital_signal_processing", "audio_processing"],
    "freeze": ["freeze", "ui_freeze", "thread_block", "stall", "lockup"],
    "ipc": ["ipc", "inter_process", "shared_memory", "interprocess_communication"],
    "latency": ["latency", "delay", "buffer_size", "processing_delay"],
    "lock_free": ["lock_free", "lockfree", "wait_free", "atomic", "thread_safe"],
    "lufs": ["lufs", "loudness", "ebu_r128", "loudness_meter"],
    "meter": ["meter", "vu_meter", "level_meter", "metering_component"],
    "mix": ["mix", "mixing", "balance", "blend", "mixer"],
    "performance": ["performance", "optimization", "speed", "cpu_usage", "throughput"],
    "phase": ["phase", "phase_correlation", "stereo_phase", "mono_compatibility"],
    "realtime": ["realtime", "real_time", "audio_thread", "process_block"],
    "spectral": ["spectral", "spectrum", "frequency_domain", "fft"],
    "synchronization": ["synchronization", "sync", "ipc_sync", "data_sync"],
    "vectorscope": ["vectorscope", "stereo_field", "phase_scope", "correlation"],
}

STOP_WORDS = {
    "el", "la", "los", "las", "un", "una", "unos", "unas", "de", "del", "en",
    "para", "por", "con", "sin", "al", "lo", "como", "mas", "pero", "que",
    "es", "son", "se", "su", "sus", "le", "les", "ya", "no", "si", "este",
    "esta", "esto", "eso", "esa", "ese", "entre", "todo", "todos", "cada",
    "muy", "hay", "era", "fue", "ser", "han", "tiene", "tienen", "hacer",
    "hace", "hacia", "donde", "cuando", "que", "como",
    "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for",
    "of", "with", "by", "from", "as", "is", "are", "was", "were", "be",
    "been", "being", "have", "has", "had", "do", "does", "did", "will",
    "would", "could", "should", "may", "might", "shall", "can", "need",
    "this", "that", "these", "those", "it", "we", "they", "not", "so",
    "just", "about", "above", "after", "again", "all", "also", "any",
    "because", "before", "below", "between", "both", "each", "few",
    "here", "how", "into", "like", "more", "most", "much", "must", "only",
    "other", "out", "over", "own", "same", "some", "such", "than",
    "then", "there", "too", "under", "until", "up", "very",
    "what", "when", "where", "while", "why",
}


# ─── Tokenization ──────────────────────────────────────────────────────────
def tokenize(text):
    """Tokenize, lowercase, remove stop words, expand DSP synonyms."""
    text = text.lower()
    text = text.replace("á", "a").replace("é", "e").replace("í", "i")
    text = text.replace("ó", "o").replace("ú", "u").replace("ü", "u")
    text = text.replace("ñ", "n")

    tokens = re.findall(r'\b[a-z]{2,}\b', text)
    result = [t for t in tokens if t not in STOP_WORDS]

    # Add DSP synonyms for matching
    for token in list(result):
        if token in DSP_SYNONYMS:
            for syn in DSP_SYNONYMS[token]:
                if syn not in result:
                    result.append(syn)

    return result


# ─── Dependency Expansion ─────────────────────────────────────────────────
def expand_dependencies(files):
    """Smart Context Expansion with symbol graph awareness."""
    expanded = set(files)
    graph_files = GRAPH.get("files", {})
    symbols = SYMBOL_GRAPH.get("symbols", {})
    by_file = SYMBOL_GRAPH.get("by_file", {})

    for f in files:
        node = graph_files.get(f)
        if node:
            # Add direct dependencies (what this file includes)
            for dep in node.get("depends_on", []):
                expanded.add(dep)
            # Add critical dependents (what includes this file)
            for dep in node.get("depended_by", []):
                dep_node = graph_files.get(dep, {})
                if dep_node.get("criticality", 0) >= 7:
                    expanded.add(dep)

        # Add symbol-based related files
        file_symbols = by_file.get(f, [])
        for sym_name in file_symbols:
            sym = symbols.get(sym_name, {})
            if sym.get("type") in ("class", "struct"):
                # Add files containing base classes
                for base in sym.get("base_classes", []):
                    base_sym = symbols.get(base, {})
                    base_file = base_sym.get("file", "")
                    if base_file and base_file != f:
                        expanded.add(base_file)

    # Limit to highest criticality files
    if len(expanded) > MAX_FILES + 2:
        sorted_files = sorted(
            expanded,
            key=lambda f: graph_files.get(f, {}).get("criticality", 0)
                         + (0.5 if f in files else 0),
            reverse=True
        )
        expanded = set(sorted_files[:MAX_FILES + 2])

    return list(expanded)


# ─── Semantic Score Extension ──────────────────────────────────────────────
def score_by_symbols(query_tokens, file_entry):
    """Symbol Graph scoring: check if query tokens match symbol names in file."""
    file_path = file_entry.get("path", "")
    by_file = SYMBOL_GRAPH.get("by_file", {})
    symbols = SYMBOL_GRAPH.get("symbols", {})

    file_symbols = by_file.get(file_path, [])
    if not file_symbols:
        return 0.0

    matches = 0
    for sym_name in file_symbols:
        sym_lower = sym_name.lower()
        for token in query_tokens:
            if token in sym_lower:
                matches += 1
                break
            # Check method names within the symbol
            sym = symbols.get(sym_name, {})
            if sym.get("type") in ("class", "struct"):
                for method in sym.get("methods", []):
                    method_name = method.split("::")[-1].lower()
                    if token == method_name:
                        matches += 1.5  # Method match is stronger

    return min(1.0, matches / max(len(query_tokens), 1) * 1.2)


def score_by_dsp_tokens(query_tokens, file_entry):
    """DSP-specific scoring: boost files with many DSP-related tokens."""
    file_path = file_entry.get("path", "")
    dsp_keywords = {"fft", "dsp", "audio", "buffer", "spectrum", "peak", "rms",
                    "lufs", "phase", "filter", "meter", "analyzer", "analysis",
                    "frequency", "amplitude", "signal", "processor", "juce",
                    "atomic", "lock_free", "thread", "realtime", "slot",
                    "registry", "shared_memory", "ipc", "backup"}

    keywords = [k.lower() for k in file_entry.get("keywords", [])]
    path_parts = file_path.lower().split("/")

    dsp_count = sum(1 for kw in keywords if kw in dsp_keywords)
    path_dsp = sum(1 for part in path_parts if part in dsp_keywords)

    return min(0.8, (dsp_count * 0.15 + path_dsp * 0.1))


# ─── Scoring ────────────────────────────────────────────────────────────────
def score_by_intent(query_tokens, file_entry):
    intent_map = INDEX.get("file_intent_map", {})
    file_path = file_entry.get("path", "")
    query_token_set = set(query_tokens)

    for intent, intent_files in intent_map.items():
        intent_tokens = set(tokenize(intent))
        overlap = query_token_set & intent_tokens
        if len(overlap) >= 2:
            for ifile in intent_files:
                if str(ifile) == file_path:
                    score = len(overlap) / max(len(intent_tokens), 1)
                    return min(1.0, score + 0.3)
    return 0.0


def score_by_keywords(query_tokens, file_entry):
    keywords = file_entry.get("keywords", [])
    if not keywords:
        return 0.0
    kw_lower = [k.lower() for k in keywords]
    matches = sum(1 for t in query_tokens if t in kw_lower)
    return matches / max(len(query_tokens), 1) * 0.8


def score_by_description(query_tokens, file_entry):
    desc = file_entry.get("description", "").lower()
    if not desc:
        return 0.0
    matches = sum(1 for t in query_tokens if t in desc)
    return matches / max(len(query_tokens), 1) * 0.6


def extract_files():
    files = []
    for module in INDEX.get("modules", []):
        for f in module.get("files", []):
            entry = {
                "path": f.get("path", ""),
                "description": f.get("description", ""),
                "keywords": f.get("keywords", []),
                "module": module.get("name", ""),
            }
            files.append(entry)
    return files


def score_files(query):
    """5-pass hybrid scoring: intent + keywords + description + symbols + DSP."""
    query_lower = query.lower()
    query_tokens = tokenize(query)

    all_files = extract_files()

    scored = []
    for f in all_files:
        path = f.get("path", "")
        module = f.get("module", "")

        s1 = score_by_intent(query_tokens, f)      # Intent map
        s2 = score_by_keywords(query_tokens, f)     # Keywords
        s3 = score_by_description(query_tokens, f)  # Description
        s4 = score_by_symbols(query_tokens, f)      # Symbol graph
        s5 = score_by_dsp_tokens(query_tokens, f)   # DSP awareness

        total = max(s1, s2, s3, s4 * 0.8, s5 * 0.6)

        # Module penalty/preference
        for q_word, modules in QUERY_MODULE_MAP.items():
            if q_word in query_lower:
                weight = MODULE_WEIGHTS.get(module, 1.0)
                if module in modules:
                    total *= 1.3
                else:
                    total *= 0.7

        if total > 0.1:
            scored.append((path, total))

    scored.sort(key=lambda x: x[1], reverse=True)

    # Get top files AND top symbol-matched files
    main_files = [s[0] for s in scored[:5]]

    # Add symbol-matched files that may not have scored high in keywords
    symbol_files = []
    for f in all_files:
        if score_by_symbols(query_tokens, f) > 0.3 and f["path"] not in main_files:
            symbol_files.append(f["path"])
    main_files.extend(symbol_files[:3])

    # Smart Context Expansion
    expanded = expand_dependencies(main_files)

    return expanded[:MAX_FILES]


# ─── Main ──────────────────────────────────────────────────────────────────
def main():
    if len(sys.argv) < 2:
        print("Usage: python context_selector.py \"tu consulta\"", file=sys.stderr)
        sys.exit(1)

    query = " ".join(sys.argv[1:])

    # Debug info to stderr
    tokens = tokenize(query)
    print(f"[DEBUG] Query: {query}", file=sys.stderr)
    print(f"[DEBUG] Tokens (+synonyms): {tokens}", file=sys.stderr)

    selected = score_files(query)

    print(f"[DEBUG] Selected files ({len(selected)}):", file=sys.stderr)
    graph_files = GRAPH.get("files", {})
    for f in selected:
        node = graph_files.get(f, {})
        crit = node.get("criticality", "?")
        deps = len(node.get("depends_on", []))
        dep_by = len(node.get("depended_by", []))
        print(f"  + {f} (crit:{crit}, deps:{deps}, used_by:{dep_by})", file=sys.stderr)

    # Output only file paths to stdout (for piping to aider)
    for f in selected:
        normalized = f.replace("\\\\", "/")
        print(normalized)


if __name__ == "__main__":
    main()

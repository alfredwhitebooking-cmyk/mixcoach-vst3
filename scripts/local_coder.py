#!/usr/bin/env python3
"""
local_coder.py — AI Local Coder v2.1 (Post-Refactor)

Routes tasks to the optimal local model (Qwen 7B / DeepSeek 16B)
and provides context from the restructured codebase.

Usage:
    python scripts/local_coder.py "your task"
    python scripts/local_coder.py --smart "task"       # Use context_selector.py
    python scripts/local_coder.py --files f1.h f2.cpp "task"
"""

import sys
import subprocess
import ollama
from pathlib import Path


# ─────────────────────────────────────────────────
#  CLI
# ─────────────────────────────────────────────────

def parse_args():
    smart = False
    files = []
    task = ""

    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == "--smart":
            smart = True
        elif a == "--files":
            while args and not args[0].startswith("--"):
                files.append(args.pop(0))
        elif a in ("--help", "-h"):
            print(__doc__)
            sys.exit(0)
        else:
            task = a
            if args:
                task += " " + " ".join(args)
                args.clear()

    if not task:
        print(__doc__)
        sys.exit(1)

    return smart, files, task


# ─────────────────────────────────────────────────
#  CORE
# ─────────────────────────────────────────────────

def run(smart_mode, explicit_files, task_original):
    task = task_original.lower()

    # -- paths (post-refactor) --

    root = Path.cwd()
    src = root / "Source"

    ct = src / "Common" / "types"
    ca = src / "Common" / "audio"
    cm = src / "Common" / "memory"

    mc = src / "MixCoach" / "core"
    me = src / "MixCoach" / "engine"
    ma = src / "MixCoach" / "audio"
    mu = src / "MixCoach" / "ui"

    ms_core = src / "Messenger" / "core"
    ms_ui = src / "Messenger" / "ui"
    ms_tel = src / "Messenger" / "telemetry"

    if not src.exists():
        print("ERROR: Source folder not found")
        sys.exit(1)

    context = ""
    files_to_read = []

    def add_file(path):
        if path.exists() and path not in files_to_read:
            files_to_read.append(path)

    def add_dir(d):
        if d.exists():
            for f in sorted(d.iterdir()):
                if f.suffix in (".h", ".cpp"):
                    add_file(f)

    # -- model router --

    model = "qwen2.5-coder:7b"

    deepseek_kw = [
        "fft", "dsp", "audio", "processblock", "thread",
        "realtime", "latency", "performance", "buffer",
        "analyzer", "analysis", "phase", "crash", "lock",
        "cpu", "optimization", "shared", "slot", "registry",
        "ipc", "telemetry",
    ]
    if any(w in task for w in deepseek_kw):
        model = "deepseek-coder-v2:latest"

    # -- file selection helpers --

    def sel_mixcoach():
        add_file(mc / "PluginProcessor.h")
        add_file(mc / "PluginProcessor.cpp")
        add_file(mc / "PluginEditor.h")
        add_file(mc / "PluginEditor.cpp")

    def sel_engine():
        add_file(me / "CoachEngine.h")
        add_file(me / "CoachEngine.cpp")
        add_file(me / "PhaseManager.h")
        add_file(me / "PhaseManager.cpp")

    def sel_audio():
        add_file(ma / "AudioAnalyzer.h")
        add_file(ma / "AudioAnalyzer.cpp")
        add_dir(ca)

    def sel_ui():
        add_dir(mu)

    def sel_messenger():
        add_file(ms_core / "PluginProcessor.h")
        add_file(ms_core / "PluginProcessor.cpp")
        add_file(ms_tel / "TelemetryCollector.h")
        add_file(ms_tel / "TelemetryCollector.cpp")
        add_file(ms_ui / "PluginEditor.h")
        add_file(ms_ui / "PluginEditor.cpp")

    def sel_common():
        add_dir(ct)
        add_dir(cm)
        add_dir(ca)

    def sel_fallback():
        sel_mixcoach()
        sel_engine()
        add_file(ms_core / "PluginProcessor.h")
        add_file(ms_core / "PluginProcessor.cpp")

    # -- route by keywords --

    if any(w in task for w in ["mixcoach", "coach", "brain", "master"]):
        sel_mixcoach()
        sel_engine()

    if any(w in task for w in ["messenger", "track", "sender", "telemetry", "oido", "ear"]):
        sel_messenger()

    if any(w in task for w in [
        "communicate", "communication", "shared", "send", "receive",
        "flow", "telemetry", "connection", "message", "sync",
        "data", "slot", "registry", "ipc",
    ]):
        sel_common()
        add_file(ms_core / "PluginProcessor.cpp")
        add_file(ms_tel / "TelemetryCollector.cpp")
        add_file(mc / "PluginProcessor.cpp")
        add_file(me / "CoachEngine.cpp")

    if any(w in task for w in ["fft", "analyzer", "analysis", "spectrum", "frequency", "masking", "phase", "dsp"]):
        sel_audio()
        add_file(mc / "PluginProcessor.cpp")
        add_file(ms_core / "PluginProcessor.cpp")
        add_file(ms_tel / "TelemetryCollector.cpp")

    if any(w in task for w in ["ui", "interface", "panel", "dashboard", "tab", "chat", "theme"]):
        sel_ui()

    if any(w in task for w in ["common", "types", "constants", "shared", "slot"]):
        sel_common()

    if not files_to_read:
        sel_fallback()

    # -- smart selection via context_selector.py --

    if smart_mode:
        try:
            result = subprocess.run(
                [sys.executable, str(root / "context_selector.py"), task_original],
                capture_output=True, text=True, timeout=30,
            )
            paths = [p.strip() for p in result.stdout.splitlines() if p.strip()]
            if paths:
                candidates = [root / p for p in paths]
                files_to_read = [f for f in candidates if f.exists()]
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        if not files_to_read:
            print("[WARN] context_selector returned no existing files; using fallback")
            sel_fallback()

    # -- explicit file list --

    for f in explicit_files:
        p = Path(f)
        if not p.is_absolute():
            p = root / p
        add_file(p)

    # -- read files --

    MAX_CHARS = 50000
    MAX_CTX = 200000

    for f in files_to_read:
        if len(context) > MAX_CTX:
            break
        try:
            content = f.read_text(encoding="utf-8", errors="ignore")
            if len(content) > MAX_CHARS:
                cut = content[:MAX_CHARS]
                lb = cut.rfind("}")
                content = cut[:lb + 1] if lb > 0 else cut
            context += (
                "\n\n" + "=" * 40 + "\n"
                f"FILE: {f.relative_to(root)}\n"
                + "=" * 40 + "\n"
                + content
            )
        except Exception as e:
            print(f"Error reading {f.name}: {e}")

    # -- prompts --

    system_prompt = """You are a senior JUCE DSP engineer.

This is a multi-plugin architecture:

Source/
  Common/           shared foundation
    types/            Types.h, Constants.h, TelemetryData.h, LogHelper.h
    audio/            AudioAnalysis.h/.cpp (FFT, RMS, phase utilities)
    memory/           SharedData.h, SharedMemory.h, SlotRegistry.h (IPC)

  MixCoach/         central AI coach plugin (VST3)
    core/             PluginProcessor.h/.cpp, PluginEditor.h/.cpp
    engine/           CoachEngine.h/.cpp, PhaseManager.h/.cpp
    audio/            AudioAnalyzer.h/.cpp
    ui/               Dashboard, chat, analyzers, track dashboard, etc.

  Messenger/        per-track telemetry plugin (VST3)
    core/             PluginProcessor.h/.cpp
    ui/               PluginEditor.h/.cpp
    telemetry/        TelemetryCollector.h/.cpp

KEY RULES:
- MESSENGER is inserted on individual tracks, sends telemetry via shared memory
- MIXCOACH is the central brain that receives telemetry and provides coaching
- COMMON provides IPC (shared memory), slot registry, and telemetry data types
- Never invent architecture — only reason from actual code
- Respect realtime safety: never allocate in processBlock(), never block audio thread
- Be precise and technical"""

    user_prompt = f"""TASK:
{task_original}

FILES:
{context}"""

    # -- run model --

    print("\n====================================")
    print("Model:", model)
    print("Files analyzed:", len(files_to_read))
    for f in files_to_read:
        print(f"  - {f.relative_to(root)}")
    print("====================================")

    try:
        response = ollama.chat(
            model=model,
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
        )
    except Exception as e:
        print(f"\n[ERROR] Failed to run model '{model}': {e}")
        print("Make sure Ollama is running:  ollama serve")
        sys.exit(1)

    # -- output --

    print("\n====================================")
    print("MODEL USED:", model)
    print("====================================\n")
    print(response["message"]["content"])


# ─────────────────────────────────────────────────
#  ENTRY POINT
# ─────────────────────────────────────────────────

if __name__ == "__main__":
    smart_mode, explicit_files, task_original = parse_args()
    run(smart_mode, explicit_files, task_original)

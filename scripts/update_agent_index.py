#!/usr/bin/env python3
"""
update_agent_index.py — Agent Index Updater v1.0

Orchestrates the semantic search pipeline:
  1. ast_parser.py --scan --update  → Build SYMBOL_GRAPH.json
  2. code_embedder.py --build-index → Build EMBEDDING_STORE.json

Usage:
    python scripts/update_agent_index.py           # Full update (scan + embed)
    python scripts/update_agent_index.py --quick    # Only re-embed changed files
    python scripts/update_agent_index.py --status   # Show index status
    python scripts/update_agent_index.py --search "query"  # Quick semantic search

Requires: Python 3.10+, no external dependencies.
"""

import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent.resolve()
ARCHIVE_DIR = PROJECT_ROOT / "_archive"
SCRIPTS_DIR = PROJECT_ROOT / "scripts"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
EMBEDDING_PATH = PROJECT_ROOT / "EMBEDDING_STORE.json"
SESSION_PATH = PROJECT_ROOT / "AI_SESSION_STATE.json"


def write_step(msg):
    print(f"\n{'=' * 60}")
    print(f"  {msg}")
    print(f"{'=' * 60}")


def write_ok(msg):
    print(f"  [OK] {msg}")


def write_err(msg):
    print(f"  [FAIL] {msg}", file=sys.stderr)


def run_script(script_name, args=None):
    """Run a Python script from _archive/ and return success."""
    script_path = ARCHIVE_DIR / script_name
    if not script_path.exists():
        write_err(f"Script not found: {script_path}")
        return False

    cmd = [sys.executable, str(script_path)]
    if args:
        cmd.extend(args)

    print(f"  Running: {' '.join(cmd)}")
    start = datetime.now()

    try:
        result = subprocess.run(
            cmd,
            cwd=str(PROJECT_ROOT),
            capture_output=True,
            text=True,
            timeout=120,
        )
        duration = (datetime.now() - start).total_seconds()

        if result.stdout:
            for line in result.stdout.strip().split("\n"):
                print(f"    {line}")

        if result.returncode != 0:
            write_err(f"Script failed (exit {result.returncode}, {duration:.1f}s)")
            if result.stderr:
                for line in result.stderr.strip().split("\n")[-5:]:
                    print(f"    [STDERR] {line}", file=sys.stderr)
            return False

        write_ok(f"Completed in {duration:.1f}s")
        return True
    except subprocess.TimeoutExpired:
        write_err("Script timed out (>120s)")
        return False
    except Exception as e:
        write_err(f"Script error: {e}")
        return False


def update_session_state(success, symbols=0, files=0, docs=0, vocab=0):
    """Update AI_SESSION_STATE.json with new agent state."""
    try:
        with open(SESSION_PATH, "r", encoding="utf-8") as f:
            session = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        session = {}

    # Update system components status
    session.setdefault("system_components", {})
    session["system_components"]["ast_parser"] = {
        "status": "active" if success else "error",
        "symbols": symbols,
        "files": files,
        "last_updated": datetime.now().isoformat(),
    }
    session["system_components"]["code_embedder"] = {
        "status": "active" if success else "error",
        "documents": docs,
        "vocabulary": vocab,
        "last_updated": datetime.now().isoformat(),
    }
    session["system_components"]["semantic_search"] = {
        "status": "active" if success else "error",
        "synonyms": 56,
        "last_updated": datetime.now().isoformat(),
    }

    # Mark all archive agents with context
    for agent in ["ast_parser", "code_embedder", "semantic_search",
                   "change_impact", "self_summarizer", "dsp_intelligence"]:
        session["system_components"].setdefault(agent, {})
        session["system_components"][agent]["location"] = "_archive/"

    # Update session metadata
    session["last_index_update"] = datetime.now().isoformat()
    session["index_status"] = "complete" if success else "failed"

    with open(SESSION_PATH, "w", encoding="utf-8") as f:
        json.dump(session, f, indent=2, ensure_ascii=False)
    write_ok(f"AI_SESSION_STATE.json updated")


def get_index_stats():
    """Get current index statistics."""
    stats = {"symbols": 0, "documents": 0, "vocabulary": 0}

    # Read symbol graph
    try:
        with open(SYMBOL_GRAPH_PATH, "r", encoding="utf-8") as f:
            graph = json.load(f)
        stats["symbols"] = graph.get("total_symbols", 0)
        stats["relationships"] = graph.get("total_relationships", 0)
        stats["files_parsed"] = len(graph.get("by_file", {}))
    except (FileNotFoundError, json.JSONDecodeError):
        pass

    # Read embedding store
    try:
        with open(EMBEDDING_PATH, "r", encoding="utf-8") as f:
            emb = json.load(f)
        stats["documents"] = emb.get("total_documents", 0)
        stats["vocabulary"] = emb.get("vocabulary_size", 0)
    except (FileNotFoundError, json.JSONDecodeError):
        pass

    return stats


def show_status():
    """Show current index status."""
    stats = get_index_stats()
    print(f"\n{'=' * 60}")
    print(f"  AGENT INDEX STATUS")
    print(f"  {'=' * 60}")
    print(f"\n  SYMBOL_GRAPH.json:")
    print(f"    Symbols:      {stats.get('symbols', 0)}")
    print(f"    Relationships: {stats.get('relationships', 0)}")
    print(f"\n  EMBEDDING_STORE.json:")
    print(f"    Documents:    {stats.get('documents', 0)}")
    print(f"    Vocabulary:   {stats.get('vocabulary', 0)}")

    # Check file existence
    for path, name in [(SYMBOL_GRAPH_PATH, "SYMBOL_GRAPH"),
                        (EMBEDDING_PATH, "EMBEDDING_STORE")]:
        if path.exists():
            size_kb = path.stat().st_size / 1024
            print(f"    File size:    {size_kb:.0f} KB")
        else:
            print(f"    File:         NOT FOUND")

    return stats


def quick_search(query):
    """Run a quick semantic search."""
    search_script = ARCHIVE_DIR / "semantic_search.py"
    if not search_script.exists():
        write_err("semantic_search.py not found")
        return

    cmd = [sys.executable, str(search_script), query, "--top-k", "5"]
    print(f"  Running: {' '.join(cmd)}\n")

    result = subprocess.run(
        cmd,
        cwd=str(PROJECT_ROOT),
        capture_output=True,
        text=True,
        timeout=30,
    )

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        # Filter out UnicodeEncodeErrors (cosmetic on Windows)
        for line in result.stderr.split("\n"):
            if "UnicodeEncodeError" not in line and line.strip():
                print(f"  [STDERR] {line}", file=sys.stderr)


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="MixCoach Agent Index Updater"
    )
    parser.add_argument(
        "--quick", action="store_true",
        help="Quick update (only re-embed changed files, skip full scan)"
    )
    parser.add_argument(
        "--status", action="store_true",
        help="Show current index status"
    )
    parser.add_argument(
        "--search", metavar='"QUERY"',
        help="Run a quick semantic search"
    )
    args = parser.parse_args()

    if args.status:
        show_status()
        return

    if args.search:
        quick_search(args.search)
        return

    # ─── Full update pipeline ──────────────────────────────────────
    print(f"\n  MixCoach Agent Index Updater v1.0")
    print(f"  {datetime.now().isoformat()}")
    print(f"  Project: {PROJECT_ROOT}")

    write_step("STEP 1/2: Building Symbol Graph")
    success = run_script("ast_parser.py", ["--scan", "--update"])

    if not success:
        write_err("Symbol graph generation failed")
        update_session_state(False)
        sys.exit(1)

    write_step("STEP 2/2: Building Embedding Index")
    success = run_script("code_embedder.py", ["--build-index"])

    # Get final stats
    stats = get_index_stats()

    # Update session state
    update_session_state(
        success=success,
        symbols=stats.get("symbols", 0),
        files=stats.get("files_parsed", 0),
        docs=stats.get("documents", 0),
        vocab=stats.get("vocabulary", 0),
    )

    # Summary
    write_step("SUMMARY")
    write_ok(f"Symbols:     {stats.get('symbols', 0)}")
    write_ok(f"Relationships: {stats.get('relationships', 0)}")
    write_ok(f"Documents:   {stats.get('documents', 0)}")
    write_ok(f"Vocabulary:  {stats.get('vocabulary', 0)}")

    if not success:
        sys.exit(1)

    print(f"\n  To search: python _archive/semantic_search.py \"your query\"")
    print(f"  To re-index: python scripts/update_agent_index.py")


if __name__ == "__main__":
    main()

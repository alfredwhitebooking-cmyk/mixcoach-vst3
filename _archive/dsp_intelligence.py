#!/usr/bin/env python3
"""
dsp_intelligence.py — DSP-Specific Intelligence v1.0

DSP-SPECIFIC INTELLIGENCE (requirement #14):
The system understands DSP concepts:
  - FFT, buffers, realtime threads
  - UI thread vs audio thread
  - Latency, repaint loops, lock-free communication
  - Analyzers, oversampling, spectral processing
  - Prioriza estabilidad realtime

Usage:
    python dsp_intelligence.py --audit                        # Audit project for DSP best practices
    python dsp_intelligence.py --explain "fft"                # Explain DSP concept in project context
    python dsp_intelligence.py --check-code "file.cpp"        # Check file for DSP safety issues
    python dsp_intelligence.py --realtime-check               # Check realtime safety across project
    python dsp_intelligence.py --suggest-fix "analyzer slow"  # Suggest DSP optimizations
"""

import json
import re
import sys
from pathlib import Path
from collections import defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.resolve()
SOURCE_DIR = PROJECT_ROOT / "Source"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
PROJECT_GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
PROJECT_INDEX_PATH = PROJECT_ROOT / "PROJECT_INDEX.json"

# ─── DSP Knowledge Base ────────────────────────────────────────────────────
DSP_KNOWLEDGE = {
    "fft": {
        "description": "Fast Fourier Transform — converts time-domain signal to frequency domain",
        "project_context": "Used in AudioAnalyzer for spectrum analysis. Forward FFT on audio buffer, magnitude spectrum extracted for visualization.",
        "realtime_safe": True,
        "allocation": "Pre-allocated buffers, no allocations in processBlock",
        "common_issues": [
            "Buffer sizes must be power of 2 (e.g., 1024, 2048)",
            "FFT windowing reduces spectral leakage (Hann window recommended)",
            "NaN/denormals from FFT of silent buffers",
            "Frequency bin resolution = sampleRate / fftSize",
        ],
        "best_practices": [
            "Pre-allocate FFT objects and buffers in prepareToPlay()",
            "Use juce::dsp::FFT (optimized) over custom implementation",
            "Apply window function before FFT",
            "Convert to dB scale for visualization (20*log10(magnitude))",
        ],
        "checklist": [
            "FFT objects created once, not per-block",
            "Window buffer pre-allocated",
            "No dynamic allocations in processBlock",
            "Denormal protection active",
            "Frequency bins correctly mapped to display",
        ],
    },
    "realtime_audio": {
        "description": "Audio processing on high-priority realtime thread",
        "project_context": "processBlock() in PluginProcessor is called from audio thread. Must never block, allocate, or wait.",
        "realtime_safe": True,
        "allocation": "ZERO allocations on audio thread. Pre-allocate everything in prepareToPlay()",
        "common_issues": [
            "malloc/new in audio thread causes glitches",
            "Mutex locking on audio thread (priority inversion)",
            "File I/O on audio thread (disk wait)",
            "Printf/logging on audio thread",
            "Unbounded loops or recursion",
        ],
        "best_practices": [
            "Use lock-free data structures for inter-thread communication",
            "Pre-allocate all buffers to maximum expected size",
            "Use atomic variables for flags shared with UI",
            "Never call juce::MessageManager or dispatch on audio thread",
            "Use FloatVectorOperations for optimized SIMD math",
        ],
        "checklist": [
            "No 'new' or 'malloc' in processBlock",
            "No 'delete' or 'free' in processBlock",
            "No file operations in processBlock",
            "No mutex locks in processBlock",
            "No MessageManager calls in processBlock",
            "All buffers pre-allocated",
            "No unbounded loops",
        ],
    },
    "lock_free": {
        "description": "Wait-free/lock-free thread communication for audio thread safety",
        "project_context": "SlotRegistry uses atomic read/write indices for SPMC queue. Shared memory uses CreateFileMappingW with atomic flags.",
        "realtime_safe": True,
        "allocation": "Pre-allocated shared memory regions, no heap allocations",
        "common_issues": [
            "False sharing (cache line contention between threads)",
            "ABA problem in lock-free structures",
            "Memory ordering (acquire/release semantics)",
            "Non-atomic operations on shared variables",
        ],
        "best_practices": [
            "Use std::atomic with appropriate memory ordering",
            "Pad shared variables to cache line size (64 bytes)",
            "Single producer, multiple consumer pattern for audio",
            "Use load(std::memory_order_acquire) and store(std::memory_order_release)",
        ],
    },
    "spectral_analysis": {
        "description": "Frequency-domain analysis for visualization and metering",
        "project_context": "AnalyzerPanel and ProfessionalAnalyzersComponent render spectrum data from FFT output",
        "realtime_safe": "Analysis on audio thread, rendering on UI thread",
        "common_issues": [
            "UI thread reading spectrum data while audio thread writes (use atomic swap)",
            "Too many FFT bins causing UI repaint slowness",
            "Decibel scale conversion and smoothing",
            "Averaging vs peak spectrum display",
        ],
    },
    "latency": {
        "description": "Processing delay through the audio chain",
        "project_context": "MixCoach is analysis-only (no processing), but Messenger adds analysis latency",
        "realtime_safe": True,
        "common_issues": [
            "FFT overlap introduces latency (hop size dependent)",
            "IPC read/write cycles introduce measurement delay",
            "LUFS measurement requires full integrated window (400ms)",
        ],
    },
    "denormals": {
        "description": "Denormalized floating-point numbers cause severe CPU slowdown on x86",
        "project_context": "Audio analysis can produce denormals with silent or very quiet input",
        "realtime_safe": True,
        "fixes": [
            "JUCE::FloatVectorOperations::disableDenormals()",
            "_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON)",
            "Add tiny epsilon (1e-30) to denominators",
            "Check for NaN/Inf after FFT operations",
            "safe_sqrt() and safe_atan2() as in AudioAnalysis.h",
        ],
    },
    "ipc_shared_memory": {
        "description": "Inter-process communication via memory-mapped files",
        "project_context": "CreateFileMappingW + MapViewOfFile between Messenger and MixCoach DLLs",
        "realtime_safe": "Writes are lock-free, reads are atomic",
        "common_issues": [
            "File mapping name collisions between DAW instances",
            "Backup file race conditions",
            "Memory-mapped file size mismatches",
            "View mapping failure when DLL is loaded in different process",
        ],
    },
}


# ═══════════════════════════════════════════════════════════════════════════
#  DSPIntelligence — DSP Audit and Analysis Engine
# ═══════════════════════════════════════════════════════════════════════════
class DSPIntelligence:
    """Analyzes project code for DSP best practices and safety."""

    def __init__(self):
        self.symbol_graph = self._load_json(SYMBOL_GRAPH_PATH)
        self.project_graph = self._load_json(PROJECT_GRAPH_PATH)
        self.project_index = self._load_json(PROJECT_INDEX_PATH)
        self.knowledge = DSP_KNOWLEDGE

    def _load_json(self, path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {}

    # ─── DSP Audit ────────────────────────────────────────────────────

    def audit_project(self):
        """Audit entire project for DSP best practices."""
        findings = []
        violations = []
        recommendations = []

        # Check patterns in source files
        source_files = []
        if SOURCE_DIR.exists():
            for ext in ("*.h", "*.cpp"):
                source_files.extend(SOURCE_DIR.rglob(ext))

        for file_path in source_files:
            relative = str(file_path.relative_to(PROJECT_ROOT))
            content = file_path.read_text(encoding="utf-8")

            file_findings = self._audit_file(content, relative)
            findings.extend(file_findings.get("findings", []))
            violations.extend(file_findings.get("violations", []))
            recommendations.extend(file_findings.get("recommendations", []))

        # Check for critical realtime paths
        rt_issues = self._check_realtime_audio(source_files)

        return {
            "total_files_audited": len(source_files),
            "total_findings": len(findings),
            "total_violations": len(violations),
            "critical_issues": len([v for v in violations if v.get("severity") == "CRITICAL"]),
            "findings": findings[:20],
            "violations": violations[:20],
            "realtime_issues": rt_issues[:10],
            "recommendations": recommendations[:15],
            "score": self._calculate_dsp_score(findings, violations),
        }

    def _audit_file(self, content, relative_path):
        """Audit a single file for DSP patterns."""
        findings = []
        violations = []
        recommendations = []

        is_process_block = "processBlock" in content
        is_prepare_to_play = "prepareToPlay" in content
        is_realtime = "audio" in relative_path.lower() or "dsp" in relative_path.lower()

        # Check for 'new' in processBlock (VIOLATION)
        if is_process_block and "new " in content:
            lines = content.split("\n")
            for i, line in enumerate(lines):
                if "processBlock" in line:
                    # Check next 30 lines for allocations
                    block_lines = lines[i:i+30]
                    for j, bl in enumerate(block_lines):
                        stripped = bl.strip()
                        if stripped.startswith("new ") or " = new " in stripped:
                            violations.append({
                                "file": relative_path,
                                "line": i + j + 1,
                                "issue": "Dynamic allocation in processBlock",
                                "severity": "CRITICAL",
                                "detail": "Use pre-allocated buffers from prepareToPlay()",
                            })
                    break

        # Check for mutex lock in audio thread files
        if is_process_block and ("lock(" in content or "mutex" in content):
            lines = content.split("\n")
            for i, line in enumerate(lines):
                if "lock(" in line or "try_lock" in line:
                    violations.append({
                        "file": relative_path,
                        "line": i + 1,
                        "issue": "Mutex lock in audio/realtime path",
                        "severity": "CRITICAL",
                        "detail": "Use lock-free data structures for audio thread",
                    })
                    break

        # Check for appropriate pre-allocations
        if is_prepare_to_play:
            findings.append({
                "file": relative_path,
                "type": "good_practice",
                "severity": "INFO",
                "detail": "Implements prepareToPlay() for pre-allocation",
            })

        # Check for denormal protection
        if "disableDenormals" in content or "FLUSH_ZERO" in content or "safe_sqrt" in content:
            findings.append({
                "file": relative_path,
                "type": "good_practice",
                "severity": "INFO",
                "detail": "Has denormal protection",
            })
        elif is_realtime and ("processBlock" in content or "float" in content):
            recommendations.append({
                "file": relative_path,
                "priority": "HIGH",
                "detail": "Add denormal protection (disableDenormals, safe_sqrt, safe_atan2)",
            })

        # Check for atomic variables
        if "std::atomic" in content:
            findings.append({
                "file": relative_path,
                "type": "good_practice",
                "severity": "INFO",
                "detail": "Uses std::atomic for thread-safe data sharing",
            })

        # Check for FloatVectorOperations
        if "FloatVectorOperations" in content:
            findings.append({
                "file": relative_path,
                "type": "good_practice",
                "severity": "INFO",
                "detail": "Uses SIMD-optimized FloatVectorOperations",
            })

        return {
            "findings": findings,
            "violations": violations,
            "recommendations": recommendations,
        }

    def _check_realtime_audio(self, source_files):
        """Specifically check realtime audio path safety."""
        issues = []
        for file_path in source_files:
            content = file_path.read_text(encoding="utf-8")
            relative = str(file_path.relative_to(PROJECT_ROOT))

            # Check for dangerous patterns on audio thread
            dangerous_patterns = [
                (r'\bnew\b', "Dynamic allocation"),
                (r'\bdelete\b', "Dynamic deallocation"),
                (r'\.wait\(', "Blocking wait"),
                (r'std::mutex', "Mutex (may cause priority inversion)"),
                (r'std::condition_variable', "Condition variable (blocking)"),
                (r'fopen\b|fwrite\b|fread\b', "File I/O"),
                (r'\bprintf\b|\bcout\b|\bcerr\b', "Console I/O"),
                (r'Sleep\b|sleep\b', "Sleep call"),
                (r'MessageManager::|dispatch_', "MessageManager call"),
            ]

            # Only check files that contain processBlock
            if "processBlock" in content:
                for pattern, desc in dangerous_patterns:
                    if re.search(pattern, content):
                        issues.append({
                            "file": relative,
                            "pattern": desc,
                            "recommendation": f"Remove '{desc}' from realtime audio path",
                        })

        return issues

    def _calculate_dsp_score(self, findings, violations):
        """Calculate DSP health score (0-100)."""
        base = 100
        critical_violations = sum(1 for v in violations if v.get("severity") == "CRITICAL")
        violations_count = len(violations)

        base -= critical_violations * 15
        base -= violations_count * 5

        # Bonus for good practices
        good_practices = sum(1 for f in findings if f.get("type") == "good_practice")
        base += min(good_practices * 2, 20)

        return max(0, min(100, base))

    # ─── Explain DSP Concept ──────────────────────────────────────────

    def explain_concept(self, concept):
        """Explain a DSP concept in the context of this project."""
        concept = concept.lower().strip()

        # Direct match
        if concept in self.knowledge:
            return self.knowledge[concept]

        # Partial match
        for key, value in self.knowledge.items():
            if concept in key or key in concept:
                return value

        # Search through text
        for key, value in self.knowledge.items():
            if any(concept in text.lower() for text in [value.get("description", ""),
                                                         value.get("project_context", "")]):
                return value

        return {
            "description": f"DSP concept '{concept}' not in knowledge base",
            "project_context": "Add to DSP_KNOWLEDGE dictionary for future queries",
        }

    # ─── Code Checking ────────────────────────────────────────────────

    def check_file(self, file_path):
        """Check a specific file for DSP safety issues."""
        full_path = PROJECT_ROOT / file_path
        if not full_path.exists():
            return {"error": f"File not found: {file_path}"}

        content = full_path.read_text(encoding="utf-8")
        relative = str(full_path.relative_to(PROJECT_ROOT))

        checks = {
            "denormal_protection": "disableDenormals" in content or "safe_sqrt" in content,
            "atomic_variables": "std::atomic" in content,
            "pre_allocated": "prepareToPlay" in content or "preallocate" in content.lower(),
            "lock_free": "atomic" in content or "lock_free" in content.lower(),
            "no_new_in_realtime": not ("processBlock" in content and "new " in content),
            "floatvector_ops": "FloatVectorOperations" in content,
            "juce_dsp": "juce_dsp" in content or "juce::dsp" in content,
        }

        issues = []
        if "processBlock" in content:
            lines = content.split("\n")
            for i, line in enumerate(lines):
                stripped = line.strip()
                if stripped.startswith("new ") and any(kw in content[:i] for kw in
                                                        ["processBlock", "process_block"]):
                    issues.append({
                        "line": i + 1,
                        "severity": "CRITICAL",
                        "message": "Allocation in audio thread",
                    })

        return {
            "file": relative,
            "has_audio_processing": "processBlock" in content,
            "checks": checks,
            "pass_rate": sum(1 for v in checks.values() if v) / max(len(checks), 1) * 100,
            "issues": issues[:10],
        }

    # ─── Suggest DSP Optimizations ───────────────────────────────────

    def suggest_optimizations(self, query=""):
        """Suggest DSP optimizations based on query or general analysis."""
        suggestions = []

        # General optimizations
        suggestions.extend([
            {
                "area": "FFT Performance",
                "suggestion": "Use power-of-2 FFT sizes and pre-allocate all FFT objects",
                "impact": "HIGH",
                "effort": "LOW",
                "files": ["Source/MixCoach/AudioAnalyzer.cpp"],
            },
            {
                "area": "Denormal Protection",
                "suggestion": "Add JUCE::FloatVectorOperations::disableDenormals() in prepareToPlay()",
                "impact": "MEDIUM",
                "effort": "LOW",
                "files": ["Source/MixCoach/PluginProcessor.cpp"],
            },
            {
                "area": "Atomic Writes",
                "suggestion": "Use memory_order_release for audio thread writes, memory_order_acquire for UI reads",
                "impact": "HIGH",
                "effort": "LOW",
                "files": ["Source/Common/SlotRegistry.cpp", "Source/Common/SharedMemory.cpp"],
            },
            {
                "area": "UI Smoothing",
                "suggestion": "Apply exponential smoothing to analyzer data for flicker-free rendering",
                "impact": "MEDIUM",
                "effort": "LOW",
                "files": ["Source/MixCoach/UI/AnalyzersPanelComponent.cpp"],
            },
            {
                "area": "FFT Windowing",
                "suggestion": "Apply Hann window before FFT for cleaner spectrum",
                "impact": "MEDIUM",
                "effort": "LOW",
                "files": ["Source/MixCoach/AudioAnalyzer.cpp"],
            },
            {
                "area": "Buffer Pre-allocation",
                "suggestion": "Ensure maximum buffer sizes are pre-allocated in prepareToPlay()",
                "impact": "HIGH",
                "effort": "MEDIUM",
                "files": ["Source/MixCoach/AudioAnalyzer.cpp"],
            },
        ])

        if query:
            query_lower = query.lower()
            filtered = [s for s in suggestions
                       if any(term in query_lower for term in s["area"].lower().split())]
            if filtered:
                return filtered

        return suggestions


# ═══════════════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════════════
def main():
    import argparse

    parser = argparse.ArgumentParser(description="DSP Intelligence for MixCoach")
    parser.add_argument("--audit", action="store_true", help="Audit project for DSP best practices")
    parser.add_argument("--explain", metavar="CONCEPT", help="Explain DSP concept")
    parser.add_argument("--check-code", metavar="FILE", help="Check file for DSP safety")
    parser.add_argument("--realtime-check", action="store_true", help="Check realtime safety")
    parser.add_argument("--suggest-fix", metavar="QUERY", help="Suggest DSP optimizations")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    dsp = DSPIntelligence()

    if args.audit:
        result = dsp.audit_project()
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print("  DSP AUDIT REPORT")
            print(f"{'='*60}")
            print(f"  Files audited: {result['total_files_audited']}")
            print(f"  DSP Score: {result['score']}/100")
            print(f"  Violations: {result['total_violations']} ({result['critical_issues']} critical)")
            print(f"\n  Top Violations:")
            for v in result["violations"][:5]:
                print(f"    [!{v.get('severity','')}] {v['file']}:{v.get('line','')} - {v['issue']}")
            print(f"\n  Recommendations:")
            for r in result["recommendations"][:5]:
                print(f"    [{r['priority']}] {r['file']}: {r['detail']}")
            if result.get("realtime_issues"):
                print(f"\n  Realtime Issues:")
                for ri in result["realtime_issues"][:5]:
                    print(f"    [!] {ri['file']}: {ri['pattern']}")
                    print(f"        -> {ri['recommendation']}")

    if args.explain:
        concept = dsp.explain_concept(args.explain)
        if args.json:
            print(json.dumps(concept, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  DSP Concept: {args.explain}")
            print(f"{'='*60}")
            print(f"  {concept.get('description', '')}")
            print(f"\n  Project Context:")
            print(f"  {concept.get('project_context', 'No project context available')}")
            if concept.get("common_issues"):
                print(f"\n  Common Issues:")
                for issue in concept["common_issues"]:
                    print(f"    [!] {issue}")
            if concept.get("best_practices"):
                print(f"\n  Best Practices:")
                for bp in concept["best_practices"]:
                    print(f"    [*] {bp}")
            if concept.get("realtime_safe"):
                print(f"\n  Realtime Safe: {'Yes' if concept.get('realtime_safe') == True else concept.get('realtime_safe', 'N/A')}")

    if args.check_code:
        result = dsp.check_file(args.check_code)
        if args.json:
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  DSP Check: {result['file']}")
            print(f"{'='*60}")
            print(f"  Has audio processing: {result.get('has_audio_processing', '?')}")
            print(f"  Pass rate: {result.get('pass_rate', 0):.0f}%")
            print(f"\n  Checks:")
            for check, passed in result.get("checks", {}).items():
                status = "OK" if passed else "FAIL"
                print(f"    [{status}] {check}")
            if result.get("issues"):
                print(f"\n  Issues:")
                for issue in result["issues"]:
                    print(f"    [!{issue['severity']}] Line {issue['line']}: {issue['message']}")

    if args.realtime_check:
        # Check all source files for realtime safety
        source_files = []
        if SOURCE_DIR.exists():
            for ext in ("*.h", "*.cpp"):
                source_files.extend(SOURCE_DIR.rglob(ext))

        issues = dsp._check_realtime_audio(source_files)
        print(f"\n{'='*60}")
        print("  REALTIME SAFETY CHECK")
        print(f"{'='*60}")
        if issues:
            print(f"  Found {len(issues)} potential issues:")
            for issue in issues[:10]:
                print(f"    [!] {issue['file']}")
                print(f"        {issue['pattern']} - {issue['recommendation']}")
        else:
            print("  No critical realtime issues detected.")

    if args.suggest_fix:
        suggestions = dsp.suggest_optimizations(args.suggest_fix)
        if args.json:
            print(json.dumps(suggestions, indent=2, ensure_ascii=False))
        else:
            print(f"\n{'='*60}")
            print(f"  DSP Optimization Suggestions")
            print(f"{'='*60}")
            for s in suggestions:
                print(f"\n  [{s['impact']}] {s['area']}")
                print(f"    {s['suggestion']}")
                print(f"    Effort: {s['effort']}")
                print(f"    Files: {', '.join(s['files'])}")


if __name__ == "__main__":
    main()

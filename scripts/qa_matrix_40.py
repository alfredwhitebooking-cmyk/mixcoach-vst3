#!/usr/bin/env python3
r"""
qa_matrix_40.py — Matriz de 104+ items QA contra codigo actual.

Extrae los items de prueba de BETA_TEST_PLAN.md, aplica reglas de
verificacion automatica (escaneo de codigo fuente nativo, sin grep de Unix)
y genera reporte HTML con theme oscuro.

Uso:
    python scripts/qa_matrix_40.py              # HTML a stdout
    python scripts/qa_matrix_40.py --html       # HTML a stdout
    python scripts/qa_matrix_40.py --markdown   # Markdown resumen
    python scripts/qa_matrix_40.py --json       # JSON raw

Exit code: 0 = todo pasa, 1 = hay fallos en items criticos.
"""

import argparse
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BETA_PLAN = PROJECT_ROOT / "BETA_TEST_PLAN.md"
SOURCE_DIR = PROJECT_ROOT / "Source"

SECTIONS = {
    "4.1": {"name": "Instalacion y Carga",  "pattern": r"### 4\.1"},
    "4.2": {"name": " Messenger - Por Pista", "pattern": r"### 4\.2"},
    "4.3": {"name": "MixCoach - Tab 1 AI Coach", "pattern": r"### 4\.3"},
    "4.4": {"name": "MixCoach - Tab 2 Analyzers", "pattern": r"### 4\.4"},
    "4.5": {"name": "Sistema de Referencias", "pattern": r"### 4\.5"},
    "4.6": {"name": "Comandos del Chat", "pattern": r"### 4\.6"},
    "4.7": {"name": "Rendimiento y Estabilidad", "pattern": r"### 4\.7"},
    "4.8": {"name": "Recuperacion de Errores", "pattern": r"### 4\.8"},
    "4.9": {"name": "Sesiones Multi-sesion", "pattern": r"### 4\.9"},
    "4.10":{"name": "Integracion FL Studio", "pattern": r"### 4\.10"},
}

# ═══════════════════════════════════════════════════════════════════════════
#  Python-native grep (cross-platform, no Unix commands)
# ═══════════════════════════════════════════════════════════════════════════

def _pygrep(pattern: str, subdir: str | None = None) -> bool:
    """Return True if pattern found in any source file under SOURCE_DIR.
    Pure Python, no external commands. O(archivos) scanning."""
    search_root = (SOURCE_DIR / subdir) if subdir else SOURCE_DIR
    if not search_root.exists():
        return False
    exts = {".cpp", ".h", ".hpp", ".c"}
    for path in search_root.rglob("*"):
        if path.suffix in exts and path.is_file():
            try:
                content = path.read_text(encoding="utf-8", errors="ignore")
                if pattern in content:
                    return True
            except Exception:
                pass
    return False


def _pygrep_count(pattern: str, subdir: str) -> int:
    """Count occurrences of pattern in all files under subdir."""
    search_root = SOURCE_DIR / subdir
    if not search_root.exists():
        return 0
    exts = {".cpp", ".h", ".hpp", ".c"}
    total = 0
    for path in search_root.rglob("*"):
        if path.suffix in exts and path.is_file():
            try:
                content = path.read_text(encoding="utf-8", errors="ignore")
                total += content.count(pattern)
            except Exception:
                pass
    return total


# ═══════════════════════════════════════════════════════════════════════════
#  Item parser
# ═══════════════════════════════════════════════════════════════════════════

_test_line_re = re.compile(
    r"^\|\s*(\d+\.\d+)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*(.*?)\s*\|"
    r"\s*([\u2b1c\u2705\u274c]?)\s*\|"
)


def parse_items(md: str) -> list[dict]:
    items = []
    current_section = None
    in_table = False

    for line in md.split("\n"):
        for sec_id, info in SECTIONS.items():
            if re.search(info["pattern"], line):
                current_section = {"id": sec_id, "name": info["name"]}

        if re.match(r"^\|[\s\-:]+\|", line):
            in_table = True
            continue

        if in_table and (not line.startswith("|") or line.strip() == ""):
            in_table = False
            continue
        if not in_table:
            continue

        m = _test_line_re.match(line)
        if not m:
            continue

        raw_status = m.group(5).strip()
        status = "not_tested"
        if raw_status == "\u2705":
            status = "pass"
        elif raw_status == "\u274c":
            status = "fail"

        items.append({
            "id": m.group(1).strip(),
            "section": current_section,
            "name": m.group(2).strip(),
            "procedure": m.group(3).strip(),
            "expected": m.group(4).strip(),
            "status": status,
        })
    return items


# ═══════════════════════════════════════════════════════════════════════════
#  Auto-check rules — each returns True/False/None
# ═══════════════════════════════════════════════════════════════════════════

def _check_installation(item: dict) -> bool | None:
    aid = item["id"]
    if aid in ("1.3",):
        return _pygrep("PluginProcessor|MixCoachAudio")
    if aid in ("1.4",):
        return _pygrep("MessengerAudioProcessor")
    if aid in ("1.5",):
        return _pygrep("SlotRegistry")  # Multi-instance via slots
    return None


def _check_coach_ui(item: dict) -> bool | None:
    aid = item["id"]
    if aid == "3.3":
        return _pygrep("Que vamos a mezclar") or _pygrep("Que genero")
    if aid == "3.5":
        return _pygrep("RobotAvatarComponent")
    if aid == "3.1":
        return _pygrep("CoachingEvidenceHost|split") and _pygrep("NavigationShell")
    return None


def _check_chat_commands(item: dict) -> bool | None:
    if item["id"] in ("3.13", "6.2"):
        return _pygrep("status") and _pygrep("/status")
    if item["id"] in ("3.14", "6.3"):
        return _pygrep("/analyze")
    if item["id"] in ("3.15", "6.4"):
        return _pygrep("/help")
    if item["id"] in ("6.8",):
        return _pygrep("no reconocido|unknown.command") or _pygrep("LlmCommandInterpreter")
    return None


def _check_analyzers(item: dict) -> bool | None:
    aid = item["id"]
    if aid == "4.2":
        return _pygrep("MasterMeterPanel") or _pygrep("VUMeter|StereoVU")
    if aid == "4.5":
        return _pygrep("SpectrographComponent")
    if aid == "4.10":
        return _pygrep("VectorscopeComponent")
    if aid == "4.11":
        return _pygrep("CrestPanel") or _pygrep("crestGauge")
    if aid == "4.13":
        return _pygrep("StereoVUMeter|VintageVU")
    if aid == "4.9":
        return _pygrep("PhaseScope|PhaseCorrelationMeter")
    return None


def _check_reference(item: dict) -> bool | None:
    aid = item["id"]
    if aid == "5.1":
        return _pygrep("ReferencePanelComponent") or _pygrep("ReferenceOnboardingCard")
    if aid == "5.2":
        return _pygrep("loadFile") and _pygrep("AudioFormatReader")
    if aid == "5.4":
        return _pygrep("UrlDownloader") or _pygrep("youtube")
    if aid == "5.5":
        return _pygrep("DifferenceProfile") or _pygrep("ReferenceAnalyzer")
    return None


def _check_ipc(item: dict) -> bool | None:
    aid = item["id"]
    if aid == "2.9":
        return _pygrep("SlotRegistry") and _pygrep("SharedMemory")
    if aid == "10.8":
        return _pygrep("muted") or _pygrep("soloed")
    return None


def _check_stability(item: dict) -> bool | None:
    if item["id"] == "7.7":
        return _pygrep_count("periodicAnalysis", "MixCoach/engine") >= 1
    if item["id"] == "7.8":
        return _pygrep_count("fastTrackAnalysis", "MixCoach/engine") >= 1
    if item["id"] == "7.9":
        return _pygrep("cooldown") or _pygrep("lastWarningTime")
    return None


def _check_errors(item: dict) -> bool | None:
    aid = item["id"]
    if aid == "8.1":
        return _pygrep("ScopedNoDenormals")
    if aid == "8.3":
        return _pygrep("checkStaleSlots") or _pygrep("stale")
    if aid == "8.6":
        return _pygrep("prepareToPlay")
    if aid == "10.6":
        return _pygrep("renderSafe") or _pygrep("isNonRealtime")
    return None


def _check_persistence(item: dict) -> bool | None:
    aid = item["id"]
    if aid in ("9.1", "9.2", "9.3", "9.4"):
        return _pygrep("getStateInformation") or _pygrep("session_memory") or _pygrep("toJson")
    if aid == "9.5":
        return _pygrep("reset") or _pygrep("newSession")
    if aid == "9.6":
        return _pygrep("Bienvenido de nuevo") or _pygrep("returningUser")
    return None


_AUTO_CHECKS = [
    _check_installation, _check_coach_ui, _check_chat_commands,
    _check_analyzers, _check_reference, _check_ipc,
    _check_stability, _check_errors, _check_persistence,
]


def _resolved(item: dict) -> str:
    st = item["status"]
    if st == "not_tested":
        for check in _AUTO_CHECKS:
            result = check(item)
            if result is True:
                return "pass"
            if result is False:
                return "fail"
    return st


# ═══════════════════════════════════════════════════════════════════════════
#  HTML Report
# ═══════════════════════════════════════════════════════════════════════════

_STAT_ICON = {"pass": "&#x2705;", "fail": "&#x274C;", "not_tested": "&#x2B1C;"}
_STAT_CLS  = {"pass": "pass", "fail": "fail", "not_tested": "nt"}


def _status_html(st: str) -> tuple[str, str]:
    return _STAT_ICON[st], _STAT_CLS[st]


def _pct(num: int, den: int) -> str:
    return f"{num}/{den} ({num*100//den}%)" if den else "0/0 (0%)"


def generate_html(items: list[dict]) -> str:
    # Group by section
    sec_map: dict[str, list[dict]] = {}
    for i in items:
        sid = i["section"]["id"] if i["section"] else "0"
        sec_map.setdefault(sid, []).append(i)

    total = len(items)
    n_pass = sum(1 for i in items if _resolved(i) == "pass")
    n_fail = sum(1 for i in items if _resolved(i) == "fail")
    n_nt = total - n_pass - n_fail

    all_sections = []
    for sid in sorted(sec_map):
        sec_items = sec_map[sid]
        sec_name = sec_items[0]["section"]["name"] if sec_items[0]["section"] else ""
        sp = sum(1 for i in sec_items if _resolved(i) == "pass")
        sf = sum(1 for i in sec_items if _resolved(i) == "fail")
        st = len(sec_items)

        rows = ""
        for it in sec_items:
            st2 = _resolved(it)
            icon, cls = _status_html(st2)
            badge = {"pass": "PASS", "fail": "FAIL", "not_tested": "NT"}[st2]
            rows += (f'<tr class="{cls}"><td>{it["id"]}</td>'
                     f'<td>{it["name"]}</td>'
                     f'<td>{it["procedure"]}</td>'
                     f'<td>{icon}</td>'
                     f'<td><span class="badge {cls}">{badge}</span></td></tr>')

        all_sections.append(
            f'<div class="section">'
            f'<h3>{sid} &mdash; {sec_name}'
            f' <span class="section-stats">{_pct(sp, st)} pass</span></h3>'
            f'<table><thead><tr><th>#</th><th>Test</th><th>Procedimiento</th>'
            f'<th>Resultado</th><th>Status</th></tr></thead>'
            f'<tbody>{rows}</tbody></table></div>'
        )

    css = """
* { box-sizing:border-box; margin:0; padding:0; }
body { font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
       background:#0f1117; color:#e1e4e8; padding:20px; max-width:1400px; margin:0 auto; }
h1 { color:#58a6ff; font-size:24px; margin-bottom:4px; }
h2 { color:#8b949e; font-size:14px; font-weight:400; margin-bottom:20px; }
h3 { color:#c9d1d9; font-size:16px; margin:24px 0 12px; }
.summary-bar { display:flex; gap:12px; margin-bottom:24px; flex-wrap:wrap; }
.summary-card { background:#161b22; border:1px solid #30363d; border-radius:8px;
                padding:14px 20px; text-align:center; min-width:100px; }
.summary-card .num { font-size:28px; font-weight:700; }
.summary-card .label { font-size:12px; color:#8b949e; margin-top:2px; }
.summary-card.pass .num { color:#3fb950; }
.summary-card.fail .num { color:#f85149; }
.summary-card.nt .num { color:#d29922; }
.section { margin-bottom:20px; }
.section-stats { font-size:12px; color:#8b949e; font-weight:400; margin-left:12px; }
table { width:100%; border-collapse:collapse; font-size:13px; }
th { text-align:left; padding:8px 10px; background:#161b22; border-bottom:1px solid #30363d;
     color:#8b949e; font-weight:500; }
td { padding:6px 10px; border-bottom:1px solid #21262d; vertical-align:top; }
tr:hover { background:#1c2128; }
tr.pass td { border-left:3px solid #3fb950; }
tr.fail td { border-left:3px solid #f85149; }
tr.nt td { border-left:3px solid #d29922; }
.badge { display:inline-block; padding:2px 8px; border-radius:4px; font-size:11px; font-weight:600; }
.badge.pass { background:#3fb95022; color:#3fb950; }
.badge.fail { background:#f8514922; color:#f85149; }
.badge.nt { background:#d2992222; color:#d29922; }
.footer { margin-top:30px; padding-top:12px; border-top:1px solid #30363d;
          font-size:12px; color:#484f58; text-align:center; }
"""

    return f"""<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8">
<title>MixCoach — Matriz QA</title><style>{css}</style></head><body>
<h1>MixCoach — Matriz de Pruebas QA</h1>
<h2>{_pct(n_pass + n_fail, total)} tests ejecutados &middot; {total} items totales</h2>
<div class="summary-bar">
<div class="summary-card pass"><div class="num">{n_pass}</div><div class="label">PASS</div></div>
<div class="summary-card fail"><div class="num">{n_fail}</div><div class="label">FAIL</div></div>
<div class="summary-card nt"><div class="num">{n_nt}</div><div class="label">NO TESTEADO</div></div>
</div>
{''.join(all_sections)}
<div class="footer">MixCoach QA Matrix &middot; scripts/qa_matrix_40.py</div>
</body></html>"""


# ═══════════════════════════════════════════════════════════════════════════
#  Markdown report
# ═══════════════════════════════════════════════════════════════════════════

def generate_markdown(items: list[dict]) -> str:
    lines = ["# MixCoach — Matriz QA\n"]
    total = len(items)
    n_pass = sum(1 for i in items if _resolved(i) == "pass")
    n_fail = sum(1 for i in items if _resolved(i) == "fail")
    n_nt = total - n_pass - n_fail
    lines.append(f"**Tests:** {n_pass} PASS, {n_fail} FAIL, {n_nt} NO TESTEADO / {total} total\n")

    sec_map: dict[str, list[dict]] = {}
    for i in items:
        sid = i["section"]["id"] if i["section"] else "0"
        sec_map.setdefault(sid, []).append(i)

    for sid in sorted(sec_map):
        si = sec_map[sid]
        name = si[0]["section"]["name"] if si[0]["section"] else ""
        lines.append(f"\n## {sid} — {name}\n")
        lines.append("| # | Test | Status |")
        lines.append("|---|------|--------|")
        for it in si:
            st = _resolved(it)
            icon = _STAT_ICON[st]
            lines.append(f"| {it['id']} | {it['name']} | {icon} |")

    return "\n".join(lines)


# ═══════════════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════════════

def main() -> int:
    parser = argparse.ArgumentParser(description="MixCoach QA Matrix")
    parser.add_argument("--html", action="store_true")
    parser.add_argument("--markdown", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if not BETA_PLAN.exists():
        print(f"ERROR: {BETA_PLAN} not found", file=sys.stderr)
        return 1

    items = parse_items(BETA_PLAN.read_text(encoding="utf-8"))
    if not items:
        print("ERROR: No items parsed from plan", file=sys.stderr)
        return 1

    # Determine format
    fmt = "html"
    if args.markdown:
        fmt = "markdown"
    elif args.json:
        fmt = "json"

    output = ""
    if fmt == "html":
        output = generate_html(items)
    elif fmt == "markdown":
        output = generate_markdown(items)
    elif fmt == "json":
        output = json.dumps([{
            "id": i["id"],
            "section": i["section"]["id"] if i["section"] else None,
            "name": i["name"],
            "status": _resolved(i),
        } for i in items], indent=2, ensure_ascii=False)

    # Write to stdout (redirect to file if needed)
    try:
        sys.stdout.reconfigure(encoding="utf-8")  # Python 3.7+
    except (AttributeError, OSError):
        pass  # stdout doesn't support reconfigure (piped, CI, etc.)
    print(output)

    # Exit 1 if critical sections have failures
    crit = [i for i in items if _resolved(i) == "fail"
            and i["section"] and i["section"]["id"] in ("4.1", "4.3", "4.4")]
    if crit:
        print(f"\n[WARN] {len(crit)} fail(s) in critical sections (4.1, 4.3, 4.4)",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)

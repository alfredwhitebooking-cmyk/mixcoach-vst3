#!/usr/bin/env python3
"""
validate_action_coverage.py — Validador de cobertura: cada accion debe tener
su seccion [CUANDO USAR ...] en buildCommandInstructions().

Lee el archivo fuente C++ y verifica:
  1. Que las 14 acciones enumeradas en ACCIONES DISPONIBLES tengan
     una seccion [CUANDO USAR ...] correspondiente.
  2. Que no haya secciones [CUANDO USAR ...] sin una accion asociada.

Uso:
    python scripts/validate_action_coverage.py

Exit code: 0 si todo cubierto, 1 si hay acciones sin cobertura.
"""

import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE_FILE = PROJECT_ROOT / "Source" / "MixCoach" / "engine" / "LlmCommandInterpreter.cpp"

# Mapa de acciones a secciones CUANDO USAR esperadas.
# switch_tab y return_to_coach comparten una seccion combinada.
ACTION_TO_CUANDO_USAR = {
    "reveal_panel":      "[CUANDO USAR reveal_panel]",
    "set_coach_state":   "[CUANDO USAR set_coach_state]",
    "switch_tab":        "[CUANDO USAR switch_tab + return_to_coach]",
    "return_to_coach":   "[CUANDO USAR switch_tab + return_to_coach]",
    "highlight_track":   "[CUANDO USAR highlight_track]",
    "celebrate":         "[CUANDO USAR celebrate]",
    "set_mode":          "[CUANDO USAR set_mode]",
    "advance_phase":     "[CUANDO USAR advance_phase]",
    "show_suggestions":  "[CUANDO USAR show_suggestions]",
    "show_report":       "[CUANDO USAR show_report]",
    "spectrum_highlight": "[CUANDO USAR spectrum_highlight]",
    "mixmap_highlight":  "[CUANDO USAR mixmap_highlight]",
    "avatar_emotion":    "[CUANDO USAR avatar_emotion]",
    "show_issue_card":   "[CUANDO USAR show_issue_card]",
}


def decode_cpp_string(content: str) -> str:
    """Decodifica escapes de una string literal C++ a texto plano."""
    content = content.replace('\\n', '\n')
    content = content.replace('\\t', '\t')
    content = content.replace('\\"', '"')
    content = content.replace('\\\\', '\\')
    return content


def extract_action_names(source: str) -> list[str]:
    """Extrae los nombres de accion del bloque ACCIONES DISPONIBLES.

    Busca patrones como:\n        1. action_name - descripcion
    dentro de lineas s += "..."
    """
    idx = source.find("ACCIONES DISPONIBLES:")
    if idx < 0:
        return []
    
    # Buscar el final de la seccion: el proximo header despues de la lista
    end_markers = ["EJEMPLOS DE RESPUESTAS", "CUANDO USAR CADA"]
    end_idx = len(source)
    for marker in end_markers:
        m = source.find(marker, idx + 1)
        if m >= 0 and m < end_idx:
            end_idx = m
    
    section = source[idx:end_idx]
    actions = []
    
    for line in section.split('\n'):
        line = line.strip()
        # Extraer el contenido entre comillas para lineas s += "..."
        # Luego buscar "N. action_name -" dentro del contenido
        m_str = re.search(r'"(.+)"', line)
        if m_str:
            content = decode_cpp_string(m_str.group(1)).strip()
            # Ahora buscar "N. action_name -" en el contenido decodificado
            m_action = re.search(r'(\d+)\.\s*(\w+)\s*[-]', content)
            if m_action:
                actions.append(m_action.group(2))
    
    return actions


def extract_cuando_usar_sections(source: str) -> list[str]:
    """Extrae los nombres de las secciones [CUANDO USAR ...]."""
    sections = []
    pattern = re.compile(r'\[CUANDO USAR ([^\]]+)\]')
    for match in pattern.finditer(source):
        sections.append(match.group(1).strip())
    return sections


def main() -> int:
    if not SOURCE_FILE.exists():
        print(f"ERROR: No se encuentra {SOURCE_FILE}")
        return 1
    
    source = SOURCE_FILE.read_text(encoding='utf-8')
    
    # Extraer acciones y secciones
    actions = extract_action_names(source)
    sections = extract_cuando_usar_sections(source)
    
    print()
    print("=" * 60)
    print("  Validador de cobertura: Acciones vs [CUANDO USAR]")
    print("=" * 60)
    
    print(f"\n[INFO] Acciones encontradas en ACCIONES DISPONIBLES:")
    for a in actions:
        print(f"  - {a}")
    print(f"  Total: {len(actions)}")
    
    print(f"\n[INFO] Secciones [CUANDO USAR ...] encontradas:")
    for s in sections:
        print(f"  - [CUANDO USAR {s}]")
    print(f"  Total: {len(sections)}")
    
    all_issues = []
    
    # 1. Cada accion debe tener su seccion CUANDO USAR
    print(f"\n{'-' * 60}")
    print("  [CHECK] Verificando cobertura de acciones...")
    
    for action in actions:
        if action not in ACTION_TO_CUANDO_USAR:
            all_issues.append(
                f"ACCION SIN MAPA: '{action}' - agregar entrada en ACTION_TO_CUANDO_USAR"
            )
            continue
        
        expected_section = ACTION_TO_CUANDO_USAR[action]
        section_name = expected_section.replace("[CUANDO USAR ", "").rstrip("]")
        
        found = any(s == section_name for s in sections)
        if not found:
            all_issues.append(
                f"ACCION SIN CUBRIR: '{action}' "
                f"(seccion esperada: {expected_section})"
            )
    
    # 2. Verificar que el conteo sea el esperado
    if len(actions) != 14:
        all_issues.append(
            f"Se esperaban 14 acciones, se encontraron {len(actions)}. "
            f"Si es intencional, actualizar ACTION_TO_CUANDO_USAR y este check."
        )
    
    # ─── Report ─────────────────────────────────────────────────────────────
    print(f"\n{'=' * 60}")
    if not all_issues:
        print("  [OK] TODAS LAS ACCIONES CUBIERTAS - Cada accion tiene su [CUANDO USAR]")
        print(f"{'=' * 60}\n")
        return 0
    else:
        print(f"  Se encontraron {len(all_issues)} issue(s):\n")
        for issue in all_issues:
            print(f"  [ISSUE] {issue}")
            print()
        print(f"{'=' * 60}\n")
        return 1


if __name__ == "__main__":
    sys.exit(main())

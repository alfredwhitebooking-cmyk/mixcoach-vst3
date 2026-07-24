#!/usr/bin/env python3
"""
validate_rules_consistency.py — Validador de consistencia entre
REGLAS DE USO y REGLAS DE ORO en buildCommandInstructions().

Lee el archivo fuente C++, extrae las reglas de ambas secciones
y verifica que no haya contradicciones ni inconsistencias.

Uso:
    python scripts/validate_rules_consistency.py

Exit code: 0 si todo consistente, 1 si hay issues.
"""

import re
import sys
from pathlib import Path

# ─── Configuración ──────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE_FILE = PROJECT_ROOT / "Source" / "MixCoach" / "engine" / "LlmCommandInterpreter.cpp"


def decode_cpp_string(content: str) -> str:
    """Decodifica escapes de una string literal C++ a texto plano."""
    # Orden importante: \\n -> newline, \\t -> tab, \\\" -> quote, \\\\ -> backslash
    content = content.replace('\\n', '\n')
    content = content.replace('\\t', '\t')
    content = content.replace('\\"', '"')
    content = content.replace('\\\\', '\\')
    return content


def extract_section(source: str, section_header: str, end_markers: list[str]) -> str:
    """Extrae el texto completo renderizado de una seccion.

    Concatena todas las lineas `s += "..."` entre el header y el primer end_marker,
    decodificando los escapes C++ a texto plano.
    """
    idx = source.find(section_header)
    if idx < 0:
        return ""
    
    end_idx = len(source)
    for marker in end_markers:
        m_idx = source.find(marker, idx + len(section_header))
        if m_idx >= 0 and m_idx < end_idx:
            end_idx = m_idx
    
    section = source[idx:end_idx]
    
    # Extraer todas las concatenaciones s += "..."
    # Usamos non-greedy (.+?) y anclamos al final con " antes de ;
    # Esto evita problemas con \\" (escaped quotes) dentro del string
    rendered = ""
    for line in section.split('\n'):
        line = line.strip()
        m = re.match(r's\s*\+=\s*"(.*)"\s*;', line)
        if m:
            raw = m.group(1)
            rendered += decode_cpp_string(raw)
    
    return rendered


def extract_rules_from_text(text: str) -> list[tuple[str, str]]:
    """Extrae reglas individuales del texto renderizado.

    Cada regla es una linea que comienza con numero, guion, o NOTA.
    Retorna lista de (texto_original, texto_normalizado).
    """
    rules = []
    for line in text.split('\n'):
        stripped = line.strip()
        if not stripped:
            continue
        # Capturar lineas de regla: empiezan con numero, guion, o NOTA
        if stripped[0].isdigit() or stripped.startswith('-') or stripped.startswith('NOTA') or stripped.startswith('('):
            norm = normalize_rule_text(stripped)
            if norm:
                rules.append((stripped, norm))
    return rules


def normalize_rule_text(text: str) -> str:
    """Normaliza el texto de una regla para comparacion."""
    text = re.sub(r'^\d+[\.\)]\s*', '', text)
    text = re.sub(r'^NOTA:\s*', '', text)
    text = re.sub(r'^\([^)]*\)\s*', '', text)
    text = ' '.join(text.split())
    return text.lower().strip()


# ─── Checks ────────────────────────────────────────────────────────────────

def check_max_command_rule(uso_text: str, oro_text: str) -> list[str]:
    """Verifica consistencia en la regla de maximo de comandos."""
    issues = []
    
    # Buscar "N. No pongas mas de X comandos" en REGLAS DE USO
    uso_match = re.search(r'(\d+)\.\s*[Nn]o pongas mas de (\d+) comandos', uso_text)
    oro_match = re.search(r'[Nn]o uses mas de (\d+) comandos', oro_text)
    
    if uso_match and oro_match:
        uso_num = uso_match.group(2)
        oro_num = oro_match.group(1)
        uso_exc = 'excepcion' in uso_text[uso_match.start():uso_match.end()+60]
        oro_exc = 'excepcion' in oro_text[oro_match.start():oro_match.end()+60]
        
        if uso_num != oro_num:
            issues.append(
                f"DISCREPANCIA: REGLAS DE USO dice max {uso_num} comandos, "
                f"REGLAS DE ORO dice max {oro_num} comandos"
            )
        if oro_exc and not uso_exc:
            issues.append(
                f"DISCREPANCIA: REGLAS DE ORO tiene excepcion '4 si incluye avatar_emotion' "
                f"pero REGLAS DE USO no la menciona"
            )
    
    return issues


def check_keyword_consistency(uso_text: str, oro_text: str) -> list[str]:
    """Verifica que temas clave cubiertos en REGLAS DE USO tambien esten en REGLAS DE ORO."""
    issues = []
    
    checks = [
        ("highlight_track + switch_tab + return_to_coach", 
         "highlight_track", "highlight_track"),
        ("avatar_emotion + tono del mensaje",
         "avatar_emotion DEBE coincidir", "avatar_emotion DEBE coincidir"),
        ("celebrate solo para logros genuinos",
         "celebrate solo para logros", "celebrate"),
        ("duda -> INCLUYE el comando",
         "Si tienes dudas entre incluir", "ante la duda"),
    ]
    
    for name, uso_kw, oro_kw in checks:
        has_uso = uso_kw in uso_text.lower()
        has_oro = oro_kw in oro_text.lower()
        if has_uso and not has_oro:
            issues.append(
                f"REGLA FALTANTE en REGLAS DE ORO: '{name}' existe en REGLAS DE USO "
                f"pero no tiene equivalente en REGLAS DE ORO"
            )
    
    return issues


def check_duplicates(rules: list[tuple[str, str]], section_name: str) -> list[str]:
    """Busca reglas duplicadas dentro de una misma seccion."""
    issues = []
    seen = {}
    for i, (orig, norm) in enumerate(rules):
        if norm in seen:
            issues.append(
                f"REGLA DUPLICADA en {section_name}:\n"
                f"  #{seen[norm]+1}: {orig}\n"
                f"  #{i+1}: {orig}"
            )
        seen[norm] = i
    return issues


def main() -> int:
    if not SOURCE_FILE.exists():
        print(f"ERROR: No se encuentra {SOURCE_FILE}")
        return 1
    
    source = SOURCE_FILE.read_text(encoding='utf-8')
    
    # Extraer texto renderizado de ambas secciones
    uso_text = extract_section(source, "REGLAS DE USO:", ["GUIA RAPIDA DE COMBINACIONES:", "return s;"])
    oro_text = extract_section(source, "REGLAS DE ORO:", ["return s;"])
    
    all_issues = []
    
    # Verificar que ambas secciones existen
    if not uso_text:
        all_issues.append("ERROR: No se encontro la seccion REGLAS DE USO")
    if not oro_text:
        all_issues.append("ERROR: No se encontro la seccion REGLAS DE ORO")
    
    if not uso_text or not oro_text:
        for issue in all_issues:
            print(f"  [ISSUE] {issue}")
        return 1
    
    # Extraer reglas individuales
    uso_rules = extract_rules_from_text(uso_text)
    oro_rules = extract_rules_from_text(oro_text)
    
    print()
    print("=" * 60)
    print("  Validador de consistencia: REGLAS DE USO vs REGLAS DE ORO")
    print("=" * 60)
    
    print(f"\n[INFO] REGLAS DE USO encontradas: {len(uso_rules)}")
    for i, (orig, _) in enumerate(uso_rules, 1):
        # Truncar si es muy larga
        display = orig if len(orig) < 100 else orig[:97] + "..."
        print(f"  {i}. {display}")
    
    print(f"\n[INFO] REGLAS DE ORO encontradas: {len(oro_rules)}")
    for i, (orig, _) in enumerate(oro_rules, 1):
        display = orig if len(orig) < 100 else orig[:97] + "..."
        print(f"  {i}. {display}")
    
    # 1. Verificar regla de max comandos
    print(f"\n{'-' * 60}")
    print("  [CHECK] Verificando regla de maximo de comandos...")
    all_issues.extend(check_max_command_rule(uso_text, oro_text))
    
    # 2. Verificar keywords consistentes
    print("  [CHECK] Verificando cobertura de temas clave...")
    all_issues.extend(check_keyword_consistency(uso_text, oro_text))
    
    # 3. Verificar duplicados
    print("  [CHECK] Buscando reglas duplicadas...")
    all_issues.extend(check_duplicates(uso_rules, "REGLAS DE USO"))
    all_issues.extend(check_duplicates(oro_rules, "REGLAS DE ORO"))
    
    # ─── Report ─────────────────────────────────────────────────────────────
    print(f"\n{'=' * 60}")
    if not all_issues:
        print("  [OK] TODO CONSISTENTE - No se encontraron problemas")
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

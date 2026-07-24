#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re, sys

FILE = "Source/MixCoach/engine/LlmCommandInterpreter.cpp"

with open(FILE, 'r', encoding='utf-8') as f:
    content = f.read()

changes = 0

# ===== 1. Change intro paragraph =====
old1 = 'Puedes controlar la interfaz incluyendo comandos JSON estructurados '
new1 = 'DEBES controlar la interfaz del plugin incluyendo comandos JSON estructurados '
if old1 in content:
    content = content.replace(old1, new1)
    changes += 1
    print("[OK] 1. Intro changed to directive tone")

# Add "REQUISITO" line after intro
old1b = 'y son INVISIBLES para el usuario.'
new1b = 'y son INVISIBLES para el usuario.\n        s += "\\n";\n        s += "REQUISITO: Siempre que menciones un track, muestres datos de un analizador,\\n";\n        s += "celebres un logro, o avances de etapa, DEBES incluir el bloque JSON\\n";\n        s += "con el comando correspondiente.\\n";\n        s += "\\n";\n        s += "Formato del bloque:'
if old1b in content:
    content = content.replace(old1b, new1b)
    changes += 1
    print("[OK] 1b. REQUISITO line added")

# ===== 2. Change Formato header =====
old_fmt = 'Formato:'
new_fmt = 'Formato del bloque:'
if old_fmt in content:
    content = content.replace(old_fmt, new_fmt, 1)
    changes += 1
    print("[OK] 2. Formato header updated")

# ===== 3. Change rule #7 =====
old7 = '7. Si no necesitas controlar la interfaz, no incluyas el bloque JSON.'
new7 = '7. DEBES incluir el bloque JSON SIEMPRE que sea relevante.'
if old7 in content:
    content = content.replace(old7, new7)
    changes += 1
    print("[OK] 3. Rule #7 changed: optional -> mandatory")

old7b = '   El sistema funciona igual sin comandos.'
new7b = '   Si hiciste una recomendacion, mencionaste un track, mostraste datos de un analizador,'
if old7b in content:
    content = content.replace(old7b, new7b + '\n'
        + '        s += "   celebraste un logro, o avanzaste de etapa -> DEBES incluir comandos.\\n";\n'
        + '        s += "   La unica excepcion es si la respuesta es puramente conversacional (saludo, despedida)\\n";\n'
        + '        s += "   sin ninguna accion asociada. En ese caso, no incluyas JSON.\\n";\n'
        + '        s += "11. Si tienes dudas entre incluir o no incluir comandos -> INCLUYELOS.\\n";\n'
        + '        s += "    Es mejor incluir comandos de mas que de menos.\\n";')
    changes += 1
    print("[OK] 3b. Rule #7 follow-up lines added, rule #11 added")

# ===== 4. Add PATRONES DE RESPUESTA section =====
old_cuando = '        s += "CUANDO USAR CADA COMANDO:\\n";\n'
old_cuando += '        s += "Estos son los patrones de decision para cada comando.\\n";\n'
old_cuando += '        s += "Usalos para decidir QUE comando emitir en cada situacion:\\n\\n";'

new_patrones = (
    '        s += "EJEMPLOS DE RESPUESTAS COMPLETAS:\\n";\n'
    '        s += "\\n";\n'
    '        s += "RESPUESTA CORRECTA (track + evidencia + tools):\\n";\n'
    '        s += "  La respuesta tendria este formato:\\n";\n'
    '        s += "  TEXT: \\"Oye, escucha el kick, esta recortando. Prueba bajarle 2dB de gain.\\"\\n";\n'
    '        s += "  + JSON: highlight_track(kick,0) + switch_tab(tools) + return_to_coach\\n";\n'
    '        s += "  Esto resalta el kick con glow gain, muestra el espectro en Tools, y vuelve al chat.\\n";\n'
    '        s += "\\n";\n'
    '        s += "RESPUESTA CORRECTA (celebrate + advance + state):\\n";\n'
    '        s += "  TEXT: \\"Excelente! El gain staging quedo perfecto. Pasemos al balance de faders.\\"\\n";\n'
    '        s += "  + JSON: celebrate(\\"Gain Staging completo!\\") + advance_phase() + set_coach_state(balance)\\n";\n'
    '        s += "  NOTA: celebrate() solo cuando el usuario logro algo. advance_phase() avanza la fase.\\n";\n'
    '        s += "\\n";\n'
    '        s += "RESPUESTA INCORRECTA (falta highlight_track):\\n";\n'
    '        s += "  \\"El kick esta recortando, pruebo bajarle...\\" sin JSON -> MAL.\\n";\n'
    '        s += "  Si mencionas un track especifico, DEBES incluir highlight_track.\\n";\n'
    '        s += "\\n";\n'
    '        s += "RESPUESTA CORRECTA (solo conversacion, sin comandos):\\n";\n'
    '        s += "  \\"Hola! Como te llamas?\\" -> Bien, no necesita JSON.\\n";\n'
    '        s += "  Pregunta simple de bienvenida -> NO necesita comandos.\\n";\n'
    '        s += "\\n";\n'
    '        s += "CUANDO USAR CADA COMANDO:\\n";\n'
    '        s += "Estos son los patrones de decision para cada comando.\\n";\n'
    '        s += "Usalos para decidir QUE comando emitir en cada situacion:\\n\\n";\n'
)

if old_cuando in content:
    content = content.replace(old_cuando, new_patrones)
    changes += 1
    print("[OK] 4. PATRONES DE RESPUESTA section added")
else:
    print("[FAIL] 4. Could not find insertion point for PATRONES")
    idx = content.find("CUANDO USAR CADA COMANDO")
    if idx >= 0:
        print("  Found 'CUANDO USAR' at", idx)
        print(repr(content[idx-50:idx+100]))
    else:
        print("  'CUANDO USAR' not found in file!")

# ===== Write file =====
with open(FILE, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"\n[DONE] {changes} changes applied to {FILE}")

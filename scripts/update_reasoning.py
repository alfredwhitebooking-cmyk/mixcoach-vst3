#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Update REASONING PROTOCOL step 6 in AiCoachAdapterPrompts.cpp"""

FILE = "Source/MixCoach/ai/AiCoachAdapterPrompts.cpp"

with open(FILE, 'r', encoding='utf-8') as f:
    content = f.read()

old_text = '        prompt += "  6. Que comando UI ayuda? Si mencionas un track -> highlight_track. Si muestras datos de un analizador -> switch_tab + return_to_coach. Si el usuario logro algo -> celebrate.\\n";'

new_text = """        prompt += "  6. QUE COMANDOS UI DEBES INCLUIR? Obligatorio: si mencionas un track -> highlight_track\\n";
        prompt += "    Si muestras datos de un analizador -> switch_tab(tools) + return_to_coach\\n";
        prompt += "    Si el usuario logro algo -> celebrate + advance_phase + set_coach_state\\n";
        prompt += "    Si avanzas de etapa -> set_coach_state(nueva_etapa) + advance_phase\\n";
        prompt += "    Si completaste el setup -> reveal_panel(proximo_panel)\\n";
        prompt += "    Si la sesion termina -> show_report + return_to_coach\\n";
        prompt += "    REGLA: Siempre que sea relevante, INCLUYE comandos. No hacerlo es un error.\\n";"""

if old_text in content:
    content = content.replace(old_text, new_text)
    with open(FILE, 'w', encoding='utf-8') as f:
        f.write(content)
    print("[OK] REASONING PROTOCOL step 6 updated")
else:
    print("[FAIL] Could not find old text")
    idx = content.find("Que comando UI ayuda")
    if idx >= 0:
        print(f"Found at offset {idx}")
        print(repr(content[idx-50:idx+150]))
    else:
        print("Text not found at all!")

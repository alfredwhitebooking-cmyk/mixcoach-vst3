#include "LlmCommandInterpreter.h"
#include "CommandTelemetry.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helper: findAfter — Busca un substring dentro de otro string usando strstr.
    //  Evita la ambigüedad de sobrecarga de juce::String::indexOf en MSVC.
    //  Retorna la posición de la primera ocurrencia después de startPos, o -1.
    // ═══════════════════════════════════════════════════════════════════════════
    static int findAfter(const juce::String& source, const char* search, int startPos)
    {
        if (source.isEmpty() || search == nullptr || *search == '\0')
            return -1;
        if (startPos >= source.length())
            return -1;

        const char* raw = source.toRawUTF8();
        const char* pos = raw + juce::jmax(0, startPos);
        const char* found = strstr(pos, search);
        if (found == nullptr)
            return -1;
        return static_cast<int>(found - raw);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  LlmCommandPrompt::buildCommandInstructions
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String LlmCommandPrompt::buildCommandInstructions()
    
    {
        juce::String s;

        s += "[UI COMMANDS - CONTROL DE INTERFAZ]\\n";
        s += "DEBES controlar la interfaz del plugin incluyendo comandos JSON estructurados ";
        s += "al final de tu respuesta. Los comandos van en un bloque de codigo JSON ";
        s += "y son INVISIBLES para el usuario.\\n";
        s += "\\n";
        s += "REQUISITO: Siempre que menciones un track, muestres datos de un analizador,\\n";
        s += "celebres un logro, o avances de etapa, DEBES incluir el bloque JSON\\n";
        s += "con el comando correspondiente.\\n";
        s += "\\n";
        s += "Formato del bloque:\\n";
        s += "```json\\n";
        s += "{\\n";
        s += "  \\\"ui\\\": [\\n";
        s += "    {\\\"action\\\": \\\"reveal_panel\\\", \\\"panel\\\": \\\"reference\\\"},\\n";
        s += "    {\\\"action\\\": \\\"celebrate\\\", \\\"message\\\": \\\"Excelente mejora!\\\"}\\n";
        s += "  ]\\n";
        s += "}\\n";
        s += "```\\n\\n";

        s += "ACCIONES DISPONIBLES:\\n\\n";

        s += "1. reveal_panel - Revela un panel por primera vez\\n";
        s += "   panel: \\\"reference\\\" | \\\"messengers\\\" | \\\"mixmap\\\" | \\\"tools\\\" | \\\"session\\\" | \\\"report\\\"\\n";
        s += "   Uso: Cuando necesitas que el usuario vea algo nuevo.\\n";
        s += "   Ej: El usuario termino setup -> reveal_panel(mixmap)\\n\\n";

        s += "2. set_coach_state - Cambia el estado visual del Coach Room\\n";
        s += "   state: \\\"welcome\\\" | \\\"intention\\\" | \\\"genre\\\" | \\\"reference\\\" ";
        s += "| \\\"messenger\\\" | \\\"mixmap\\\" | \\\"gain\\\" | \\\"balance\\\" ";
        s += "| \\\"eq\\\" | \\\"compression\\\" | \\\"space\\\" | \\\"automation\\\"\\n";
        s += "   Uso: Cuando avanzas a una nueva etapa de mezcla.\\n";
        s += "   Ej: \\\"Pasemos al balance de faders\\\" -> set_coach_state(balance)\\n\\n";

        s += "3. switch_tab - Cambia al tab especificado\\n";
        s += "   tab: \\\"coach\\\" | \\\"tools\\\" | \\\"session\\\"\\n";
        s += "   Uso: Cuando quieres mostrar evidencia al usuario.\\n";
        s += "   Ej: \\\"Mira el espectro...\\\" + switch_tab(tools)\\n\\n";

        s += "4. highlight_track - Resalta un track en la lista de Messengers\\n";
        s += "   track: nombre del track (e.j.: \\\"kick\\\", \\\"voz\\\")\\n";
        s += "   domain: 0=gain | 1=tonal | 2=dynamics | 3=spatial\\n";
        s += "   Uso: Cuando hablas de un problema en un track especifico.\\n";
        s += "   Ej: \\\"El kick esta clipping\\\" + highlight_track(kick, domain=0)\\n\\n";

        s += "5. celebrate - Dispara una animacion de celebracion\\n";
        s += "   message: texto corto de logro\\n";
        s += "   Uso: Cuando el usuario completa algo importante.\\n";
        s += "   Ej: celebrate(\\\"Gain Staging completado!\\\")\\n\\n";

        s += "6. set_mode - Cambia entre modo Mix y Master\\n";
        s += "   mix: true = Mix Mode | false = Master Mode\\n";
        s += "   Uso: Solo al inicio del setup.\\n\\n";

        s += "7. return_to_coach - Vuelve al chat desde Tools o Session\\n";
        s += "   Uso: Despues de mostrar evidencia, regresa al chat.\\n\\n";

        s += "8. advance_phase - Avanza a la siguiente fase\\n";
        s += "   Uso: Cuando el usuario completa una etapa tecnica.\\n\\n";

        s += "9. show_suggestions - Muestra sugerencias rapidas como chips\\n";
        s += "   suggestions: array de strings con las sugerencias\\n";
        s += "   Uso: Para dar opciones rapidas de respuesta.\\n\\n";

        s += "10. show_report - Muestra el overlay de reporte final\\n";
        s += "   Uso: Cuando la sesion se completa, para que el usuario vea su progreso.\\n\\n";

        s += "11. spectrum_highlight - Resalta una region de frecuencia en el analizador de espectro\\n";
        s += "    frequency: frecuencia central en Hz (ej: 60.0, 250.0, 3000.0)\\n";
        s += "    bandwidth: (opcional) ancho de banda en Hz (0=auto 1/3 de octava)\\n";
        s += "    label: (opcional) etiqueta descriptiva para el badge\\n";
        s += "    Uso: Cuando mencionas una frecuencia especifica y quieres senialarla\\n";
        s += "    visualmente en el espectro.\\n";
        s += "    Ej: \\\"El kick tiene pico en 60 Hz\\\" + spectrum_highlight(60, 20, \\\"60 Hz - Kick\\\")\\n\\n";

        s += "12. mixmap_highlight - Resalta un track o grupo de buses en el MixMap\\n";
        s += "    track: (opcional) nombre del track a resaltar\\n";
        s += "    bus: (opcional) nombre del bus a resaltar (\\\"Drums\\\", \\\"Bass\\\", \\\"Guitars\\\", etc.)\\n";
        s += "    Uso: Cuando hablas de un grupo entero de pistas o necesitas\\n";
        s += "    enfocar visualmente un bus en el mapa de mezcla.\\n";
        s += "    Ej: \\\"Las guitarras estan ocultando las voces\\\" + mixmap_highlight(\\\"\\\", \\\"Guitars\\\")\\n\\n";

        s += "13. avatar_emotion - Cambia la expresion emocional del avatar del Coach\\n";
        s += "    emotion: \\\"happy\\\" | \\\"serious\\\" | \\\"thinking\\\" | \\\"surprised\\\" | \\\"encouraging\\\" | \\\"neutral\\\"\\n";
        s += "    Uso: Para reforzar visualmente el tono emocional del mensaje.\\n";
        s += "    Ej: \\\"Excelente mejora!\\\" + avatar_emotion(happy)\\n";
        s += "    Ej: \\\"Cuidado, el kick esta recortando\\\" + avatar_emotion(serious)\\n\\n";

        s += "14. show_issue_card - Muestra una tarjeta de problema tecnico inline en el chat\\n";
        s += "    track: nombre del track afectado\\n";
        s += "    severity: \\\"info\\\" | \\\"warning\\\" | \\\"critical\\\"\\n";
        s += "    issue_type: tipo de issue: \\\"CLIP\\\" | \\\"EQ\\\" | \\\"DYN\\\" | \\\"PHASE\\\" | \\\"MASK\\\"\\n";
        s += "    description: texto descriptivo del problema y/o solucion\\n";
        s += "    Uso: Cuando detectas un problema tecnico claro que el usuario debe\\n";
        s += "    resolver, como clipping, enmascaramiento, o problemas de fase.\\n";
        s += "    La tarjeta es INLINE en el chat, no reemplaza nada.\\n";
        s += "    Ej: show_issue_card(\\\"Kick\\\", \\\"critical\\\", \\\"CLIP\\\", \\\"El kick esta recortando a 0 dBFS. Baja el fader 3-6 dB.\\\")\\n\\n";

        s += "15. select_analyzer - Navega a un sub-analiador especifico en el panel Tools\\n";
        s += "    analyzer: \\\"spectrum\\\" | \\\"vectorscope\\\" | \\\"crest\\\" | \\\"stereo\\\" | \\\"dna\\\"\\n";
        s += "    Uso: Cuando quieres que el usuario vea un analizador especifico dentro de Tools.\\n";
        s += "    Automaticamente cambia al tab Tools y activa/desactiva los toggles\\n";
        s += "    del sub-analiador solicitado.\\n";
        s += "    Ej: \\\"Mira el crest factor de la bateria...\\\" + switch_tab(tools) + select_analyzer(crest)\\n";
        s += "    Ej: \\\"Observa la imagen estereo...\\\" + switch_tab(tools) + select_analyzer(vectorscope)\\n";
        s += "    Ej: \\\"El espectro muestra un pico...\\\" + switch_tab(tools) + select_analyzer(spectrum)\\n";
        s += "    NOTA: Siempre combina select_analyzer con switch_tab(tools) y return_to_coach().\\n";
        s += "    Si solo usas switch_tab sin select_analyzer, se muestra el layout por defecto.\\n\\n";

        s += "EJEMPLOS DE RESPUESTAS COMPLETAS:\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (track + evidencia + tools):\\n";
        s += "  El text sample: \"Oye, escucha el kick, esta recortando.\"\\n";
        s += "  + JSON: highlight_track(kick,0) + switch_tab(tools) + return_to_coach\\n";
        s += "  Eso resalta el kick, muestra el espectro en Tools, y vuelve al chat.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (celebrate + advance + state):\\n";
        s += "  \"Excelente! Gain staging perfecto. Pasemos al balance de faders.\"\\n";
        s += "  + JSON: celebrate(\"Gain Staging completo!\") + advance_phase() + set_coach_state(balance)\\n";
        s += "  celebrate() solo cuando el usuario logro algo. advance_phase() avanza la fase.\\n";
        s += "\\n";
        s += "RESPUESTA INCORRECTA (falta highlight_track):\\n";
        s += "  \"El kick esta recortando...\" sin JSON -> MAL.\\n";
        s += "  Si mencionas un track especifico, DEBES incluir highlight_track.\\n";
        s += "\\n";
        s += "RESPUESTA INCORRECTA (highlight_track SOLO, faltan switch_tab + return_to_coach):\\n";
        s += "  \"Oye, el kick esta recortando, bajale 2dB al fader...\" + solo highlight_track -> MAL.\\n";
        s += "  Si mencionas un track Y das un consejo que requiere que el usuario vea datos,\\n";
        s += "  DEBES incluir TAMBIEN switch_tab(tools) para mostrar el analizador,\\n";
        s += "  Y return_to_coach() para volver al chat despues.\\n";
        s += "  El patron COMPLETO es: highlight_track + switch_tab + return_to_coach.\\n";
        s += "  highlight_track solo NO es suficiente cuando das consejos con datos.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (contraste - patron completo vs incompleto):\\n";
        s += "  INCOMPLETO: \"Bajale 2dB al kick, esta recortando.\"\\n";
        s += "    + JSON: highlight_track(kick,0) -> FALTA: switch_tab + return_to_coach\\n";
        s += "  COMPLETO: \"Bajale 2dB al kick, mira el medidor...\"\\n";
        s += "    + JSON: highlight_track(kick,0) + switch_tab(tools) + return_to_coach() -> CORRECTO\\n";
        s += "  Diferencia: el COMPLETO muestra evidencia (tools) y vuelve al chat.\\n";
        s += "  El INCOMPLETO solo resalta el track pero no da contexto visual.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (solo conversacion, sin comandos):\\n";
        s += "  \"Hola! Como te llamas?\" -> NO necesita JSON.\\n";
        s += "  Pregunta simple de bienvenida -> sin comandos.\\n";
        s += "\\n";

        s += "RESPUESTA CORRECTA (evidencia visual + espectro + emocion):\\n";
        s += "  \"El kick tiene un pico resonante a 60 Hz que esta ensuciando los sub-graves. Mira el espectro.\"\\n";
        s += "  + JSON: spectrum_highlight(60, 20, \"60 Hz - Kick\") + switch_tab(tools) + avatar_emotion(serious) + return_to_coach()\\n";
        s += "  Eso resalta la frecuencia en el espectro, cambia a Tools para que lo vea, pone cara seria al avatar, y vuelve al chat.\\n";
        s += "  El avatar refuerza visualmente la gravedad del problema.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (issue card + highlight + emocion - patron critico):\\n";
        s += "  \"Cuidado! El kick esta recortando a 0 dBFS. Baja el fader 3-6 dB y revisa el medidor.\"\\n";
        s += "  + JSON: show_issue_card(Kick, critical, CLIP, \"El kick esta recortando a 0 dBFS. Baja el fader 3-6 dB.\") + highlight_track(kick,0) + avatar_emotion(serious)\\n";
        s += "  La tarjeta muestra el problema tecnico con severidad critica, el highlight seniala el track, y el avatar refuerza la seriedad.\\n";
        s += "  PATRON COMPLETO para problemas tecnicos: show_issue_card + highlight_track + avatar_emotion.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (mixmap + emocion + coach state):\\n";
        s += "  \"Las guitarras estan ocultando las voces en el rango medio. Vamos a revisar el balance.\"\\n";
        s += "  + JSON: mixmap_highlight(\"\", Guitars) + avatar_emotion(thinking) + set_coach_state(balance)\\n";
        s += "  Resalta el bus de guitarras, el avatar muestra que esta pensando en el problema, y cambia a estado balance.\\n";
        s += "  El mixmap da contexto visual del grupo afectado.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (logro con emocion positiva + celebrate):\\n";
        s += "  \"Excelente! Has eliminado la resonancia del kick a 60 Hz. El sub-graves ahora esta limpio.\"\\n";
        s += "  + JSON: spectrum_highlight(60, 20, \"Resonancia eliminada\") + celebrate(\"Resonancia eliminada!\") + avatar_emotion(happy)\\n";
        s += "  Muestra la frecuencia que se corrigio como ya solucionada, celebra el logro, y el avatar sonrie.\\n";
        s += "  El spectrum_highlight con label actualizada funciona como \"antes y despues\" visual.\\n";
        s += "\\n";
        s += "RESPUESTA CORRECTA (multiple issue cards + emocion):\\n";
        s += "  \"Hay dos problemas que atender: el kick esta recortando y el snare necesita EQ.\"\\n";
        s += "  + JSON: show_issue_card(Kick, critical, CLIP, \"Kick recortando a 0 dBFS\") + show_issue_card(Snare, warning, EQ, \"Snare sin cuerpo a 200 Hz\") + avatar_emotion(serious)\n";
        s += "  Dos tarjetas de problema (maximo recomendado) con diferentes severidades y el avatar serio.\\n";
        s += "  El usuario ve ambos problemas a la vez y puede priorizar.\\n";
        s += "\\n";

s += "CUANDO USAR CADA COMANDO:\\n";
        s += "Estos son los patrones de decision para cada comando.\\n";
        s += "Usalos para decidir QUE comando emitir en cada situacion:\\n\\n";

        s += "[CUANDO USAR reveal_panel]\\n";
        s += "  - Acabas de recomendar cargar una referencia y el usuario aun no tiene una -> reveal_panel(reference)\\n";
        s += "  - Mencionaste un track especifico y los Messengers aun no se han revelado -> reveal_panel(messengers)\\n";
        s += "  - El usuario pregunto por el routing o conexiones entre pistas -> reveal_panel(mixmap)\\n";
        s += "  - Quieres mostrar evidencia de un analizador por primera vez -> reveal_panel(tools)\\n";
        s += "  - La sesion esta avanzada y el panel Session esta bloqueado pero relevante -> reveal_panel(session)\\n";
        s += "  - La sesion termino -> reveal_panel(report)\\n\\n";

        s += "[CUANDO USAR set_coach_state]\\n";
        s += "  - El usuario confirmo que termino el setup inicial -> set_coach_state(messenger)\\n";
        s += "  - Terminaron de organizar pistas y asignar roles -> set_coach_state(gain)\\n";
        s += "  - Completaron gain staging, pasan a balance de faders -> set_coach_state(balance)\\n";
        s += "  - Terminaron balance, pasan a ecualizacion -> set_coach_state(eq)\\n";
        s += "  - Terminaron EQ, pasan a compresion -> set_coach_state(compression)\\n";
        s += "  - Terminaron compresion, pasan a espacio/reverb -> set_coach_state(space)\\n";
        s += "  - Ultima etapa antes del master check -> set_coach_state(automation)\\n";
        s += "  NOTA: Los estados de setup (welcome/intention/genre/reference) los maneja el sistema.\\n";
        s += "  Solo usa set_coach_state para las etapas de mezcla (gain/balance/eq/compression/space/automation).\\n\\n";

        s += "[CUANDO USAR switch_tab + return_to_coach]\\n";
        s += "  - Mencionaste datos de un analizador (espectro, LUFS, correlacion) -> switch_tab(tools), luego return_to_coach()\\n";
        s += "  - Quieres que el usuario vea el session map o routing -> switch_tab(session), luego return_to_coach()\\n";
        s += "  - PATRON: SIEMPRE que uses switch_tab a algo que no es coach, programa un return_to_coach()\\n";
        s += "    para que el chat vuelva al foco despues de la evidencia.\\n";
        s += "  - Si solo hablas conceptualmente sin datos que mostrar -> NO uses switch_tab.\\n\\n";

        s += "[CUANDO USAR highlight_track]\\n";
        s += "  - Estas dando un consejo sobre UN track especifico: \\\"el kick esta clipping\\\" -> highlight_track(kick,0)\\n";
        s += "  - El dominio (domain) debe coincidir con el tipo de consejo:\\n";
        s += "      0 = gain (nivel, clipping, headroom, LUFS)\\n";
        s += "      1 = tonal (EQ, frecuencias, brillo, cuerpo, sub)\\n";
        s += "      2 = dynamics (compresion, transientes, crest factor)\\n";
        s += "      3 = spatial (panning, stereo, phase correlacion)\\n";
        s += "  - Si mencionas MULTIPLES tracks -> NO uses highlight_track (solo para UNO a la vez)\\n";
        s += "  - Si es un consejo general sin track especifico -> NO uses highlight_track\\n";
        s += "  - highlight_track funciona mejor combinado con switch_tab(tools) o reveal_panel(messengers)\\n\\n";

        s += "[CUANDO USAR celebrate]\\n";
        s += "  - El usuario confirmo que aplico un cambio y el audio mejoro -> celebrate(\\\"Eso! funciono!\\\")\\n";
        s += "  - Completaron una etapa entera (gain staging, balance, etc.) -> celebrate(\\\"Etapa completada!\\\")\\n";
        s += "  - El mix score subio significativamente (>=5%) -> celebrate(\\\"Sigue mejorando!\\\")\\n";
        s += "  - El match con la referencia mejoro -> celebrate(\\\"Te acercas a la referencia!\\\")\\n";
        s += "  - NO uses celebrate si solo diste un consejo (espera a que lo apliquen).\\n";
        s += "  - NO uses celebrate para cambios triviales (mute/solo, cambios de pan menores).\\n";
        s += "  - El mensaje debe ser corto (max 60 chars) y genuino.\\n\\n";

        s += "[CUANDO USAR advance_phase]\\n";
        s += "  - El usuario completo todos los objetivos de la fase actual -> advance_phase()\\n";
        s += "  - Generalmente va precedido de un cambio evidente: el usuario dice \\\"listo\\\" o confirma haber aplicado los cambios.\\n";
        s += "  - NO uses advance_phase en medio del setup inicial.\\n";
        s += "  - NO uses advance_phase si el usuario sigue ajustando cosas de la fase actual.\\n\\n";

        s += "[CUANDO USAR show_report]\\n";
        s += "  - La sesion esta completamente terminada y el usuario pidio ver resultados -> show_report()\\n";
        s += "  - Solo disponible en estados avanzados (Coaching, DeepAnalysis).\\n\\n";

        s += "[CUANDO USAR show_suggestions]\\n";
        s += "  - El usuario pregunto \\\"que hago ahora?\\\" sin direccion clara -> show_suggestions([...])\\n";
        s += "  - Hay multiples opciones viables y quieres que el usuario elija -> show_suggestions([...])\\n";
        s += "  - Maximo 4 sugerencias. Cada una de 1-5 palabras.\\n";
        s += "  - NO uses si ya hay una prioridad clara (usa la accion directa en vez).\\n\\n";

        s += "[CUANDO USAR set_mode]\\n";
        s += "  - Solo durante el setup inicial, cuando preguntas si quiere Mix o Master mode.\\n";
        s += "  - No uses despues de que el setup este completo.\\n\\n";

        s += "[CUANDO USAR spectrum_highlight]\\n";
        s += "  - Mencionaste una frecuencia especifica y quieres senialarla visualmente -> spectrum_highlight(freq, bw, label)\\n";
        s += "  - Estas diagnosticando un problema tonal, ej: \\\"el kick tiene un pico en 60 Hz\\\" -> spectrum_highlight(60, 20, \\\"60 Hz - Kick\\\")\\n";
        s += "  - Estas destacando una mejora, ej: \\\"la presencia vocal mejoro a 3 kHz\\\" -> spectrum_highlight(3000, 500, \\\"3 kHz - Presence\\\")\\n";
        s += "  - Cuando mencionas frecuencias en contexto de EQ o resonancias -> SIEMPRE incluye spectrum_highlight\\n";
        s += "  - spectrum_highlight funciona mejor combinado con switch_tab(tools) para mostrar el espectro\\n";
        s += "  - Si mencionas multiples frecuencias, usa spectrum_highlight para la mas importante\\n";
        s += "  - NO uses spectrum_highlight si no tienes una frecuencia especifica que senalar\\n\\n";

        s += "[CUANDO USAR mixmap_highlight]\\n";
        s += "  - Hablas de un grupo entero de pistas, ej: \\\"las guitarras estan ocultando las voces\\\" -> mixmap_highlight(\\\"\\\", \\\"Guitars\\\")\\n";
        s += "  - Quieres enfocar visualmente un bus entero en el MixMap -> mixmap_highlight(\\\"\\\", bus)\\n";
        s += "  - Estas comparando niveles entre buses, ej: \\\"los Drums estan mas fuertes que el Bass\\\" -> mixmap_highlight(\\\"\\\", \\\"Drums\\\") + mixmap_highlight(\\\"\\\", \\\"Bass\\\") en comandos separados\\n";
        s += "  - El usuario pregunto por el routing o estructura de grupos -> mixmap_highlight(track, bus)\\n";
        s += "  - mixmap_highlight puede usarse solo con track, solo con bus, o con ambos\\n";
        s += "  - Si usas solo track, resalta ese track. Si usas solo bus, resalta todo el grupo\\n";
        s += "  - NO uses mixmap_highlight para tracks individuales (usa highlight_track en su lugar)\\n";
        s += "  - NO uses mixmap_highlight si el MixMap no esta visible (usa reveal_panel(mixmap) primero)\\n\\n";

        s += "[CUANDO USAR avatar_emotion]\\n";
        s += "  - Diste una noticia positiva o el usuario logro algo -> avatar_emotion(happy)\\n";
        s += "  - Estas advirtiendo sobre un problema serio (clipping, fase, etc.) -> avatar_emotion(serious)\\n";
        s += "  - Estas analizando un problema y pensando en soluciones -> avatar_emotion(thinking)\\n";
        s += "  - El usuario hizo algo inesperado o sorprendente -> avatar_emotion(surprised)\\n";
        s += "  - Estas animando al usuario a seguir intentando -> avatar_emotion(encouraging)\\n";
        s += "  - Estado por defecto cuando no hay emocion particular -> avatar_emotion(neutral)\\n";
        s += "  - avatar_emotion DEBE coincidir con el tono del mensaje de texto\\n";
        s += "  - NO uses avatar_emotion en cada mensaje - solo cuando el tono emocional cambia significativamente\\n";
        s += "  - happy + celebrate es una combinacion potente para logros\\n";
        s += "  - serious + show_issue_card es efectivo para problemas criticos\\n\\n";

        s += "[CUANDO USAR show_issue_card]\\n";
        s += "  - Detectaste clipping en un track -> show_issue_card(track, \\\"critical\\\", \\\"CLIP\\\", descripcion)\\n";
        s += "  - Hay un problema de EQ claro, ej: \\\"el snare no tiene cuerpo a 200 Hz\\\" -> show_issue_card(track, \\\"warning\\\", \\\"EQ\\\", descripcion)\\n";
        s += "  - Hay enmascaramiento entre tracks, ej: \\\"el bass y el kick compiten en sub-graves\\\" -> show_issue_card(track, \\\"warning\\\", \\\"MASK\\\", descripcion)\\n";
        s += "  - Hay problemas de fase, ej: \\\"la voz tiene cancelacion de fase en stereo\\\" -> show_issue_card(track, \\\"warning\\\", \\\"PHASE\\\", descripcion)\\n";
        s += "  - Hay problemas de dinamica, ej: \\\"la compresion esta bombeando\\\" -> show_issue_card(track, \\\"info\\\", \\\"DYN\\\", descripcion)\\n";
        s += "  - La tarjeta es INLINE en el chat - no bloquea ni reemplaza nada\\n";
        s += "  - show_issue_card es ideal para dar contexto visual a un consejo tecnico\\n";
        s += "  - NO uses show_issue_card para issues no tecnicos (preferencias personales, dudas conceptuales)\\n";
        s += "  - NO uses mas de 2 issue cards por mensaje para no saturar al usuario\\n";
        s += "  - PATRON PODEROSO: show_issue_card + highlight_track + avatar_emotion(serious) para problemas criticos\\n\\n";

        s += "REGLAS DE USO:\\n";
        s += "1. Pon los comandos SIEMPRE al FINAL de tu respuesta, despues del texto.\\n";
        s += "2. No pongas mas de 3 comandos por respuesta (excepcion: 4 si incluye avatar_emotion).\\n";
        s += "3. highlight_track debe coincidir EXACTAMENTE con como el usuario nombro su track.\\n";
        s += "4. SIEMPRE que uses switch_tab, incluye return_to_coach despues en el mismo bloque.\\n";
        s += "5. celebrate solo para logros genuinos, no para cada interaccion.\\n";
        s += "6. Los estados de setup (welcome/intention/genre/reference) los maneja el sistema.\\n";
        s += "   NO uses set_coach_state con esos valores.\\n";
        s += "7. DEBES incluir el bloque JSON SIEMPRE que sea relevante.\\n";
        s += "   Si hiciste una recomendacion, mencionaste un track, mostraste datos de un analizador,\\n";
        s += "   celebraste un logro, o avanzaste de etapa -> DEBES incluir comandos.\\n";
        s += "   La unica excepcion es si la respuesta es puramente conversacional (saludo, despedida)\\n";
        s += "   sin ninguna accion asociada. En ese caso, no incluyas JSON.\\n";
        s += "8. No abuses de switch_tab - solo cambia cuando sea realmente util mostrar datos.\\n";
        s += "9. highlight_track + switch_tab + return_to_coach es el patron mas potente:\\n";
        s += "   muestras evidencia visual y vuelves al chat.\\n";
        s += "10. Nunca uses commands para interacciones de una sola palabra (\\\"si\\\", \\\"ok\\\").\\n";
        s += "11. Si tienes dudas entre incluir o no incluir comandos -> INCLUYELOS.\\n";
        s += "    Es mejor incluir comandos de mas que de menos.\\n";
        s += "12. EL PATRON highlight_track + switch_tab(tools) + return_to_coach() es OBLIGATORIO\\n";
        s += "    cuando recomiendas un cambio en un track y hay datos de analizador disponibles.\\n";
        s += "    highlight_track SOLO se considera una respuesta INCOMPLETA.\\n";
        s += "    La unica excepcion es si tools no esta disponible (aun no revelado).\\n";
        s += "    En ese caso, usa reveal_panel(tools) + return_to_coach en vez de switch_tab.\\n";
        s += "\\n";

        s += "GUIA RAPIDA DE COMBINACIONES:\\n";
        s += "Los patrones mas potentes, priorizados por impacto:\\n";
        s += "\\n";
        s += "1. [EVIDENCIA CLASICA] highlight_track + switch_tab(tools) + return_to_coach()\\n";
        s += "   -> Resalta un track, muestra datos del analizador, vuelve al chat.\\n";
        s += "\\n";
        s += "2. [PROBLEMA TONAL] spectrum_highlight(freq, bw, label) + switch_tab(tools) + avatar_emotion(serious) + return_to_coach()\\n";
        s += "   -> Seniala una frecuencia en el espectro, cambia a Tools, refuerza con emocion seria.\\n";
        s += "\\n";
        s += "3. [PROBLEMA CRITICO] show_issue_card(track, critical, CLIP, desc) + highlight_track(track,domain) + avatar_emotion(serious)\\n";
        s += "   -> Tarjeta de error + resalte de track + expresion seria. Maxima contundencia.\\n";
        s += "\\n";
        s += "4. [ANALISIS DE BUS] mixmap_highlight(\"\", bus) + avatar_emotion(thinking) + set_coach_state(balance)\\n";
        s += "   -> Resalta un grupo entero, avatar pensativo, avanza a la siguiente etapa.\\n";
        s += "\\n";
        s += "5. [ANTES/DESPUES] spectrum_highlight(freq, bw, label_actualizada) + celebrate(mensaje) + avatar_emotion(happy)\\n";
        s += "   -> Muestra la frecuencia corregida, celebra el logro, avatar feliz.\\n";
        s += "\\n";
        s += "6. [TRANSICION DE FASE] celebrate(mensaje) + advance_phase() + set_coach_state(siguiente_etapa)\\n";
        s += "   -> Celebra la etapa completada, avanza de fase, cambia al siguiente estado.\\n";
        s += "\\n";
        s += "7. [REVELAR + MOSTRAR] reveal_panel(panel) + highlight_track(track, domain) + switch_tab(tab)\\n";
        s += "   -> Revela un panel nuevo, resalta un track ahi, cambia de tab para mostrar.\\n";
        s += "\\n";
        s += "8. [MULTIPLES ISSUES] show_issue_card(track1, sev1, type1, desc1) + show_issue_card(track2, sev2, type2, desc2) + avatar_emotion(serious)\\n";
        s += "   -> Maximo 2 tarjetas de problema con diferentes severidades. Avatar serio.\\n";
        s += "\\n";
        s += "9. [DECISION RAPIDA] show_suggestions([opcion1, opcion2, opcion3]) + avatar_emotion(thinking)\\n";
        s += "   -> Ofrece opciones al usuario con avatar pensativo. Maximo 4 sugerencias.\\n";
        s += "\\n";
        s += "10. [CIERRE DE SESION] show_report() + avatar_emotion(happy) + reveal_panel(report)\\n";
        s += "    -> Muestra el reporte final, avatar feliz, revela el panel de reporte.\\n";
        s += "\\n";
        s += "REGLAS DE ORO:\\n";
        s += "- Cada respuesta DEBE tener al menos 1 comando si no es puramente conversacional.\\n";
        s += "- highlight_track + switch_tab + return_to_coach es el patron minimo para consejos con datos.\\n";
        s += "- show_issue_card + highlight_track + avatar_emotion(serious) para problemas criticos.\\n";
        s += "- spectrum_highlight + switch_tab(tools) para diagnosticos de frecuencia.\\n";
        s += "- avatar_emotion DEBE coincidir con el tono del mensaje.\\n";
        s += "- Si mencionas un track -> highlight_track. Si mencionas un bus -> mixmap_highlight.\\n";
        s += "- NO uses mas de 3 comandos por respuesta (excepcion: 4 si incluye avatar_emotion).\\n";
        s += "- ante la duda, INCLUYE el comando. Siempre es mejor incluir de mas que de menos.\\n";
        s += "\\n";

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  processResponse
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String LlmCommandInterpreter::processResponse(const juce::String& llmResponse)
    {
        if (llmResponse.isEmpty())
            return {};

        int jsonStart = findAfter(llmResponse, "```json", 0);
        if (jsonStart < 0)
            return llmResponse;

        int jsonEnd = findAfter(llmResponse, "```", jsonStart + 7);
        if (jsonEnd < 0) {
            LogHelper::writeToLog("[LlmCommandInterpreter] JSON block sin cierre");
            return llmResponse;
        }

        int contentStart = jsonStart + 7;
        juce::String jsonBlock = llmResponse.substring(contentStart, jsonEnd).trim();
        if (jsonBlock.isEmpty()) {
            LogHelper::writeToLog("[LlmCommandInterpreter] JSON block vacío");
            return llmResponse.substring(0, jsonStart).trim();
        }

        int uiStart = findAfter(jsonBlock, "\"ui\"", 0);
        if (uiStart < 0) {
            LogHelper::writeToLog("[LlmCommandInterpreter] No se encontró \"ui\" en JSON");
            return llmResponse.substring(0, jsonStart).trim();
        }

        juce::String uiSection = jsonBlock.substring(uiStart);
        std::vector<UiCommand> commands = parseCommands(uiSection);

        if (commands.empty()) {
            LogHelper::writeToLog("[LlmCommandInterpreter] No se parsearon comandos");
            return llmResponse.substring(0, jsonStart).trim();
        }

        int executed = 0;
        for (const auto& cmd : commands)
            if (executeCommand(cmd)) ++executed;

                // ═══ Gap #3: Registrar telemetría ══════════════════════════════
        if (telemetryCollector_) {
            CommandTelemetryEntry batchEntry;
            batchEntry.timestampUs = juce::Time::getMillisecondCounter() * 1000;
            batchEntry.actionType = -1; // Batch entry
            batchEntry.actionName = "batch";
            batchEntry.success = (executed > 0);
            batchEntry.paramSummary = juce::String(executed) + "/" + juce::String((int)commands.size()) + " executed";
            batchEntry.commandIndex = 0;
            batchEntry.batchSize = (int)commands.size();
            telemetryCollector_->recordCommand(batchEntry);
        }

        LogHelper::writeToLog("[LlmCommandInterpreter] Ejecutados " + juce::String(executed)
                              + "/" + juce::String((int)commands.size()) + " comandos");

        return llmResponse.substring(0, jsonStart).trim();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  parseCommands — Parsea comandos desde un string JSON
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<LlmCommandInterpreter::UiCommand>
    LlmCommandInterpreter::parseCommands(const juce::String& jsonSection)
    {
        std::vector<UiCommand> commands;
        if (jsonSection.isEmpty())
            return commands;

        int searchPos = 0;
        const int maxObjects = 10;

        for (int objCount = 0; objCount < maxObjects; ++objCount) {
            int braceStart = findAfter(jsonSection, "{", searchPos);
            if (braceStart < 0) break;

            int braceEnd = findAfter(jsonSection, "}", braceStart);
            if (braceEnd < 0) break;

            juce::String objStr = jsonSection.substring(braceStart, braceEnd + 1);

            // Parsear action
            int actionIdx = findAfter(objStr, "\"action\"", 0);
            if (actionIdx < 0) { searchPos = braceEnd + 1; continue; }

            int colonIdx = findAfter(objStr, ":", actionIdx + 8);
            if (colonIdx < 0) { searchPos = braceEnd + 1; continue; }

            // Buscar valor entre comillas después de ":"
            int qStart = colonIdx + 1;
            while (qStart < objStr.length() && objStr[qStart] == ' ') ++qStart;
            int valQuoteStart = findAfter(objStr, "\"", qStart);
            if (valQuoteStart < 0) { searchPos = braceEnd + 1; continue; }
            int valQuoteEnd = findAfter(objStr, "\"", valQuoteStart + 1);
            if (valQuoteEnd < 0) { searchPos = braceEnd + 1; continue; }

            juce::String actionStr = objStr.substring(valQuoteStart + 1, valQuoteEnd);
            UiCommand cmd;
            cmd.action = actionFromString(actionStr);

            // Extraer parámetros usando findAfter
            auto extractString = [&](const char* key, int keyLen, juce::String& out) {
                int keyIdx = findAfter(objStr, key, 0);
                if (keyIdx < 0) return;
                int cp = findAfter(objStr, ":", keyIdx + keyLen);
                if (cp < 0) return;
                int qs = findAfter(objStr, "\"", cp);
                if (qs < 0) return;
                int qe = findAfter(objStr, "\"", qs + 1);
                if (qe < 0) return;
                out = objStr.substring(qs + 1, qe);
            };

            auto extractInt = [&](const char* key, int keyLen) -> int {
                int keyIdx = findAfter(objStr, key, 0);
                if (keyIdx < 0) return -1;
                int cp = findAfter(objStr, ":", keyIdx + keyLen);
                if (cp < 0) return -1;
                int ns = cp + 1;
                while (ns < objStr.length() && objStr[ns] == ' ') ++ns;
                if (ns < objStr.length() && juce::CharacterFunctions::isDigit(objStr[ns])) {
                    // objStr[ns] returns juce_wchar (uint32_t). Using juce::String(juce_wchar) on
                    // MSVC may resolve to String(int) instead, producing ASCII code string ("48" for '0').
                    // Convert digit char to int directly.
                    return static_cast<int>(objStr[ns] - '0');
                }
                return -1;
            };

            auto extractBool = [&](const char* key, int keyLen) -> bool {
                int keyIdx = findAfter(objStr, key, 0);
                if (keyIdx < 0) return true;
                int cp = findAfter(objStr, ":", keyIdx + keyLen);
                if (cp < 0) return true;
                int vs = cp + 1;
                while (vs < objStr.length() && objStr[vs] == ' ') ++vs;
                return (objStr.substring(vs, vs + 4).trim().toLowerCase() == "true");
            };

            auto extractStringArray = [&](const char* key, int keyLen, std::vector<juce::String>& out) {
                int keyIdx = findAfter(objStr, key, 0);
                if (keyIdx < 0) return;
                int bs = findAfter(objStr, "[", keyIdx + keyLen);
                if (bs < 0) return;
                int be = findAfter(objStr, "]", bs);
                if (be <= bs) return;

                juce::String arrStr = objStr.substring(bs + 1, be);
                int sp = 0;
                while (true) {
                    int sq = findAfter(arrStr, "\"", sp);
                    if (sq < 0) break;
                    int eq = findAfter(arrStr, "\"", sq + 1);
                    if (eq < 0) break;
                    out.push_back(arrStr.substring(sq + 1, eq));
                    sp = eq + 1;
                }
            };

            // panel
            extractString("\"panel\"", 7, cmd.panel);

            // state
            extractString("\"state\"", 7, cmd.state);

            // tab
            extractString("\"tab\"", 5, cmd.tab);

            // track
            extractString("\"track\"", 7, cmd.track);

            // domain
            {
                int d = extractInt("\"domain\"", 8);
                if (d >= 0) cmd.domain = d;
            }

            // message
            extractString("\"message\"", 9, cmd.message);

            // mix
            cmd.isMixMode = extractBool("\"mix\"", 5);

            // suggestions
            extractStringArray("\"suggestions\"", 12, cmd.suggestions);

            // ═══ SelectAnalyzer: parsear analyzer parameter ═════════════
            extractString("\"analyzer\"", 10, cmd.analyzer);

            // ═══ Gap #2: Extraer parámetros de evidencia visual ═══════
            {
                int freqIdx = findAfter(objStr, "\"frequency\"", 0);
                if (freqIdx >= 0) {
                    int cp = findAfter(objStr, ":", freqIdx + 11);
                    if (cp >= 0) {
                        int ns = cp + 1;
                        while (ns < objStr.length() && objStr[ns] == ' ') ++ns;
                        // Parse float: read digits + dot + digits
                        juce::String freqStr;
                        while (ns < objStr.length() && (juce::CharacterFunctions::isDigit(objStr[ns]) || objStr[ns] == '.' || objStr[ns] == '-')) {
                            freqStr += juce::String::charToString(objStr[ns]);
                            ++ns;
                        }
                        if (freqStr.isNotEmpty())
                            cmd.frequencyHz = freqStr.getFloatValue();
                    }
                }
            }

            // bandwidth
            {
                int bwIdx = findAfter(objStr, "\"bandwidth\"", 0);
                if (bwIdx >= 0) {
                    int cp = findAfter(objStr, ":", bwIdx + 11);
                    if (cp >= 0) {
                        int ns = cp + 1;
                        while (ns < objStr.length() && objStr[ns] == ' ') ++ns;
                        juce::String bwStr;
                        while (ns < objStr.length() && (juce::CharacterFunctions::isDigit(objStr[ns]) || objStr[ns] == '.' || objStr[ns] == '-')) {
                            bwStr += juce::String::charToString(objStr[ns]);
                            ++ns;
                        }
                        if (bwStr.isNotEmpty())
                            cmd.bandwidthHz = bwStr.getFloatValue();
                    }
                }
            }

            // emotion
            extractString("\"emotion\"", 9, cmd.emotion);

            // severity
            extractString("\"severity\"", 10, cmd.severity);

            // issue_type
            extractString("\"issue_type\"", 12, cmd.issueType);

            // description
            extractString("\"description\"", 13, cmd.description);

            // bus
            extractString("\"bus\"", 5, cmd.bus);

            // label (reuse existing string member for SpectrumHighlight label)
            extractString("\"label\"", 7, cmd.label);

            commands.push_back(cmd);
            searchPos = braceEnd + 1;
        }

        LogHelper::writeToLog("[LlmCommandInterpreter] Parseados " + juce::String((int)commands.size()) + " comandos");
        return commands;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  executeCommand
    // ═══════════════════════════════════════════════════════════════════════════
    bool LlmCommandInterpreter::executeCommand(const UiCommand& cmd)
{
    // ═══ Gap #3: Lambda para registrar telemetría antes de cada return ═══
    auto recordCmd = [&](bool success) -> void {
        if (!telemetryCollector_) return;
        CommandTelemetryEntry e;
        e.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        e.actionType = static_cast<int>(cmd.action);
        e.actionName = actionTypeToString(static_cast<int>(cmd.action));
        e.success = success;
        if (cmd.panel.isNotEmpty()) e.paramSummary = "panel=" + cmd.panel;
        else if (cmd.state.isNotEmpty()) e.paramSummary = "state=" + cmd.state;
        else if (cmd.tab.isNotEmpty()) e.paramSummary = "tab=" + cmd.tab;
        else if (cmd.track.isNotEmpty()) e.paramSummary = "track=" + cmd.track;
        else if (cmd.message.isNotEmpty()) e.paramSummary = "msg=" + cmd.message.substring(0, 40);
        else if (cmd.frequencyHz > 0.0f) e.paramSummary = "freq=" + juce::String(cmd.frequencyHz, 0) + "Hz";
        else if (cmd.analyzer.isNotEmpty()) e.paramSummary = "analyzer=" + cmd.analyzer;
        else if (cmd.bus.isNotEmpty()) e.paramSummary = "bus=" + cmd.bus;
        else e.paramSummary = cmd.suggestions.empty() ? "(no params)" : ("suggestions=" + juce::String((int)cmd.suggestions.size()));
        telemetryCollector_->recordCommand(e);
    };

    switch (cmd.action) {
        case Action::RevealPanel:
            if (onRevealPanel && isValidPanel(cmd.panel)) {
                onRevealPanel(cmd.panel);
                recordCmd(true); return true;
            }
            break;
        case Action::SetCoachState:
            if (onSetCoachState && isValidState(cmd.state)) {
                onSetCoachState(cmd.state);
                recordCmd(true); return true;
            }
            break;
        case Action::SwitchTab:
            if (onSwitchTab && cmd.tab.isNotEmpty()) {
                onSwitchTab(cmd.tab);
                recordCmd(true); return true;
            }
            break;
        case Action::HighlightTrack:
            if (onHighlightTrack && cmd.track.isNotEmpty()) {
                onHighlightTrack(cmd.track, cmd.domain);
                recordCmd(true); return true;
            }
            break;
        case Action::Celebrate:
            if (onCelebrate && cmd.message.isNotEmpty()) {
                onCelebrate(cmd.message);
                recordCmd(true); return true;
            }
            break;
        case Action::SetMode:
            if (onSetMode) {
                onSetMode(cmd.isMixMode);
                recordCmd(true); return true;
            }
            break;
        case Action::ReturnToCoach:
            if (onReturnToCoach) {
                onReturnToCoach();
                recordCmd(true); return true;
            }
            break;
        case Action::AdvancePhase:
            if (onAdvancePhase) {
                onAdvancePhase();
                recordCmd(true); return true;
            }
            break;
        case Action::ShowReport:
            if (onShowReport) {
                onShowReport();
                recordCmd(true); return true;
            }
            break;
        case Action::ShowSuggestions:
            if (onShowSuggestions && !cmd.suggestions.empty()) {
                onShowSuggestions(cmd.suggestions);
                recordCmd(true); return true;
            }
            break;
        case Action::SpectrumHighlight:
            if (onSpectrumHighlight && cmd.frequencyHz > 0.0f) {
                onSpectrumHighlight(cmd.frequencyHz, cmd.bandwidthHz, cmd.label);
                recordCmd(true); return true;
            }
            break;
        case Action::MixmapHighlight:
            if (onMixmapHighlight && (cmd.track.isNotEmpty() || cmd.bus.isNotEmpty())) {
                onMixmapHighlight(cmd.track, cmd.bus);
                recordCmd(true); return true;
            }
            break;
        case Action::AvatarEmotion:
            if (onAvatarEmotion && cmd.emotion.isNotEmpty()) {
                onAvatarEmotion(cmd.emotion);
                recordCmd(true); return true;
            }
            break;
        case Action::ShowIssueCard:
            if (onShowIssueCard && cmd.issueType.isNotEmpty()) {
                onShowIssueCard(cmd.track, cmd.severity, cmd.issueType, cmd.description);
                recordCmd(true); return true;
            }
            break;
        case Action::SelectAnalyzer:
            if (onSelectAnalyzer && cmd.analyzer.isNotEmpty()) {
                onSelectAnalyzer(cmd.analyzer);
                recordCmd(true); return true;
            }
            break;
    }

    LogHelper::writeToLog("[LlmCommandInterpreter] Comando no ejecutado: action="
                          + juce::String(static_cast<int>(cmd.action)));
    recordCmd(false); return false;
}
bool LlmCommandInterpreter::hasAnyCallbacks() const noexcept
    {
        return onRevealPanel != nullptr
            || onSetCoachState != nullptr
            || onSwitchTab != nullptr
            || onHighlightTrack != nullptr
            || onCelebrate != nullptr
            || onSetMode != nullptr
            || onReturnToCoach != nullptr
            || onAdvancePhase != nullptr
            || onShowReport != nullptr
            || onShowSuggestions != nullptr
            || onSpectrumHighlight != nullptr
            || onMixmapHighlight != nullptr
            || onAvatarEmotion != nullptr
            || onShowIssueCard != nullptr
            || onSelectAnalyzer != nullptr;
    }

    LlmCommandInterpreter::Action LlmCommandInterpreter::actionFromString(const juce::String& str)
    {
        juce::String lower = str.trim().toLowerCase();

        if (lower == "reveal_panel")     return Action::RevealPanel;
        if (lower == "set_coach_state")  return Action::SetCoachState;
        if (lower == "switch_tab")       return Action::SwitchTab;
        if (lower == "highlight_track")  return Action::HighlightTrack;
        if (lower == "celebrate")        return Action::Celebrate;
        if (lower == "set_mode")         return Action::SetMode;
        if (lower == "return_to_coach")  return Action::ReturnToCoach;
        if (lower == "advance_phase")    return Action::AdvancePhase;
        if (lower == "show_report")      return Action::ShowReport;
        if (lower == "show_suggestions") return Action::ShowSuggestions;

        // ═══ Gap #2: Comandos de evidencia visual ═══════════════════════════
        if (lower == "spectrum_highlight") return Action::SpectrumHighlight;
        if (lower == "mixmap_highlight")  return Action::MixmapHighlight;
        if (lower == "avatar_emotion")    return Action::AvatarEmotion;
        if (lower == "show_issue_card")   return Action::ShowIssueCard;
        if (lower == "select_analyzer")   return Action::SelectAnalyzer;

        LogHelper::writeToLog("[LlmCommandInterpreter] Acción desconocida: " + str + " — saltando comando");
        return Action::ShowSuggestions;
    }

    bool LlmCommandInterpreter::isValidState(const juce::String& state)
    {
        juce::String lower = state.trim().toLowerCase();
        return lower == "welcome" || lower == "intention" || lower == "genre"
            || lower == "reference" || lower == "messenger" || lower == "mixmap"
            || lower == "gain" || lower == "balance" || lower == "eq"
            || lower == "compression" || lower == "space" || lower == "automation";
    }

    bool LlmCommandInterpreter::isValidPanel(const juce::String& panel)
    {
        juce::String lower = panel.trim().toLowerCase();
        return lower == "reference" || lower == "messengers" || lower == "mixmap"
            || lower == "tools" || lower == "session" || lower == "report";
    }

} // namespace mixcoach

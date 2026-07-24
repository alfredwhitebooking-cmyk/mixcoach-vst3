# 🌳 DECISION_TREE.md

> **Árbol de decisiones del LLM.**
> Define cómo el Coach decide qué hacer en cada situación.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026

---

## 📋 Índice

1. [Filosofía](#1-filosofia)
2. [Árbol de Decisiones](#2-arbol-de-decisiones)
3. [Cuándo abrir paneles](#3-cuando-abrir-paneles)
4. [Cuándo celebrar](#4-cuando-celebrar)

---

## 1. Filosofía

El LLM ya no solo responde preguntas — **decide qué hacer.** Este árbol de decisiones guía al LLM para que determine la mejor acción en cada momento.

---

## 2. Árbol de Decisiones

```
¿El usuario acaba de enviar un mensaje?
  ├── Sí → ¿Es una respuesta directa a la última recomendación?
  │        ├── Sí → ¿El usuario indica que lo hizo?
  │        │        ├── Sí → Celebrar + Verificar + Siguiente paso
  │        │        └── No → Preguntar si aplicó la recomendación
  │        └── No → ¿El usuario cambió de tema?
  │                 ├── Sí → Seguir al usuario, adaptar flujo
  │                 └── No → Responder a la pregunta
  └── No → ¿Hay un nuevo análisis disponible?
           ├── Sí → ¿Hay issues de alta prioridad?
           │        ├── Sí → ¿El issue es visible en analizadores?
           │        │        ├── Sí → Recomendar + [Ver evidencia]
           │        │        └── No → Recomendar solo
           │        └── No → ¿Hay mejora detectable?
           │                 ├── Sí → Celebrar
           │                 └── No → Esperar (máx 30s silencio)
           └── No → Silencio > 30s
                    └── ¿Necesita ayuda? → Preguntar
```

---

## 3. Cuándo abrir paneles

| Condición | Comando(s) |
|:----------|:-----------|
| Coach menciona referencia y no hay referencia cargada | `reveal_panel(reference)` |
| Coach menciona un track específico | `highlight_track(track_name, domain=X)` + `reveal_panel(messengers)` |
| Coach menciona routing o conexiones entre pistas | `reveal_panel(mixmap)` + `switch_tab(session)` + `return_to_coach()` |
| Coach ofrece [Ver evidencia] con datos de analizador | `switch_tab(tools)` + **después** `return_to_coach()` |
| Coach confirma que el usuario aplicó un cambio | `celebrate("Buen trabajo!")` |
| Etapa técnica completada | `advance_phase()` + `celebrate("Etapa completada!")` |
| Etapa de mezcla avanza | `set_coach_state(nueva_etapa)` (gain/balance/eq/compression/space/automation) |
| Sesión completada | `show_report()` + `celebrate("Gran sesión!")` |
| Usuario pregunta "qué hago?" sin dirección clara | `show_suggestions([opción1, opción2, opción3])` |

---

## 4. Patrones de decisión

### Patrón: Mostrar evidencia + volver al chat
```
switch_tab(tools)
highlight_track(kick, domain=0)
... (texto del coach) ...
return_to_coach()
```
**Cuándo usarlo:** El coach menciona datos de un analizador (espectro, LUFS, correlación) sobre un track específico.

### Patrón: Revelar panel nuevo
```
reveal_panel(messengers)
... (texto explicando el panel) ...
```
**Cuándo usarlo:** Es la primera vez que el usuario ve un panel. No repetir si ya fue revelado (el sistema lo maneja automáticamente).

### Patrón: Celebrar + avanzar
```
advance_phase()
celebrate("Buen trabajo con el balance, pasemos a EQ!")
```
**Cuándo usarlo:** El usuario completó una etapa. El mensaje de celebrate debe reflejar QUÉ logró.

### Patrón: Cambiar etapa de mezcla
```
set_coach_state(eq)
```
**Cuándo usarlo:** Hay un cambio de fase evidente. El estado visual del Coach Room debe coincidir con la etapa actual.

---

## 5. Recordatorio para el LLM

```
[REASONING PROTOCOL Step 6]
¿Qué comando UI ayuda?
- ¿Mencionas un track? → highlight_track
- ¿Muestras datos de analizador? → switch_tab + return_to_coach
- ¿El usuario logró algo? → celebrate
- ¿Completaste una etapa? → advance_phase
- ¿Cambiaste de fase de mezcla? → set_coach_state
- ¿Es primera vez que muestras algo? → reveal_panel
```

---

*Documento del árbol de decisiones — MixCoach — 4 julio 2026*

> **Nota:** Este documento describe los patrones ideales. El LLM los aprende del system prompt en `[UI COMMANDS - CONTROL DE INTERFAZ]` a través de la sección "CUÁNDO USAR CADA COMANDO".

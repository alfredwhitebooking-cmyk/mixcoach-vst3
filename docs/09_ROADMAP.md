# 🗺️ 09 — ROADMAP

> **Estado del proyecto y siguientes fases.**
> Este documento define el norte del producto, sprints completados, activos y futuros.
>
> **Versión:** 4.0 | **Última actualización:** 4 julio 2026
> **Documentos base:** UX_20_ROADMAP.md (plan maestro), workspace_memory/ROADMAP.md (v2.1 legacy)

---

## 📋 Índice

1. [🎯 North Star](#-north-star)
2. [📊 Evaluación Actual](#-evaluación-actual)
3. [✅ Sprints Completados](#-sprints-completados)
4. [🏃 UX 2.0 — Próximas Fases](#-ux-20--próximas-fases)
5. [🏆 Estado del Plan 10/10](#-estado-del-plan-1010)
6. [📈 Cobertura de Tests](#-cobertura-de-tests)
7. [❌ Lo que NO construir](#-lo-que-no-construir)

---

## 🎯 North Star

> **MixCoach es un ingeniero de mezcla sentado a tu lado.**
> No procesa audio. No juzga. Enseña, sugiere y verifica.

### Las 3 piezas que transforman el producto

| # | Pieza | Estado |
|:-:|:------|:------:|
| 1 | **Identidad de las pistas** — el cerebro sabe qué es cada pista | ✅ Completado |
| 2 | **Análisis individual por pista** — el cerebro entiende cada pista | ✅ Pipeline DSP completo |
| 3 | **Referencia como norte absoluto** — cada recomendación contra la referencia | ✅ Mensajes cualitativos + LLM path |

---

## 📊 Evaluación Actual

| Dimensión | Score | Nota |
|:----------|:-----:|:------|
| **Arquitectura** | 9/10 | IPC, separación Sensor-Cerebro, flujo de datos ✅ |
| **Producto** | 8.5/10 | UX 2.0 implementado al 85% (6 fases completas, 5 en progreso) |
| **Visión UX** | 10/10 | Documentación completa: MASTER_EXPERIENCE, SESSION_FLOW, COACH_PERSONALITY |
| **Cobertura de tests** | 8.5/10 | 33+ targets, 214 tests nuevos (CoachRoomState + LlmCommandInterpreter + ExperienceManager) |
| **Potencial comercial** | 9/10 | Nicho claro, problema real, enfoque único en mentor conversacional |

---

## ✅ Sprints Completados

### Sprint 1 — Identity Layer ✅
El cerebro sabe exactamente qué es cada pista, y el usuario lo confirma visualmente.
- `TrackRole` enum con 50+ roles
- Inferencia combinada nombre + espectral
- Role pills con confianza + badge 🎯 X/Y

### Sprint 2 — Mix Map ✅
Visualizar toda la sesión como árbol jerárquico de buses.
- `SessionMap` struct + `buildSessionMap()` en CoachEngine
- `MixMapComponent` con routing visual, flechas, tooltips
- Botón "Confirmar mapa" con avance de fase

### Sprint 3 — Per Track Analysis ✅
El cerebro entiende cada pista en profundidad.
- `TrackAudioResult` con peak/RMS/correlation/crest/bandEnergies
- Crest factor coloreado, spectral mini-bar 6 bandas, stereo width indicator

### Sprint 4 — Reference-Driven Coaching ✅
La referencia dirige toda la mezcla.
- `ReferenceDrivenMode` flag + `ReferenceProgress` struct
- Mensajes cualitativos periódicos con tendencia ▲/▼
- DifferenceProfile + DomainGap + PlanManager

### Sprint 5 — TrackFeed Live ✅
TrackFeedCore es el motor central de la UI.
- TrackFeed Banner colapsable con top 3 eventos
- Health Filter Pills (❌⚠️✓) + Event Tooltip al hover

### Sprint 6A — Track Gain Intelligence ✅
El cerebro responde: ¿Está muy bajo? ¿Está muy alto?
- `TrackGainAdvice` struct con target por rol (50+ roles)
- Sin IA — reglas C++, 0 tokens, instantáneo

### Sprint 6B — Track Dynamics Intelligence ✅
¿Sobre-comprimido? ¿Muy dinámico?
- `TrackDynamicsAdvice` con crest targets por rol
- Sin IA — reglas C++, 0 tokens

### Sprint 6C — Track Tonal Intelligence ✅
¿Exceso o déficit en regiones espectrales?
- `TrackTonalAdvice` con 30 bandas → 6 regiones
- Sin IA — reglas C++, 0 tokens

### UX 2.0 Fase 1-9 — Implementación (85%) ✅
Toda la infraestructura UX 2.0 está implementada y cableada (ver tabla detallada abajo para el estado exacto de cada fase):
- **Fase 1-8** — Documentación completa (MASTER_EXPERIENCE, SESSION_FLOW, COACH_PERSONALITY, UI_SYSTEM, 6 docs de componentes, 4 engine docs, 3 AI docs)
- **Fase 4** — TabBarComponent con 3 tabs exactas (Coach/Session/Tools), locking progresivo, NEW badges
- **Fase 9** — ExperienceManager creado e integrado: celebration cycle, state transitions, callback wiring
- **PanelRevealManager** — Keyword detection + fallback para Chat-Commanded UI
- **LlmCommandInterpreter** — 10 acciones JSON para que el LLM controle la UI
- **CoachRoomState** — 14 estados de progressive disclosure con animaciones crossfade

### UX 2.0 — Tests Unitarios (2026) ✅
Nuevos tests que validan la infraestructura UX 2.0:
- **TestCoachRoomState** (77 tests) — isPreFullUI, isCoachingState, progress, labels, enum ordering
- **TestLlmCommandInterpreter** (104 tests) — parseCommands (10 action types), processResponse, executeCommand, callback routing, edge cases
- **TestExperienceManager** (33 tests) — celebration lifecycle (animate → complete → duplicate ignore → multiple cycles)

---

## 🏃 UX 2.0 — Próximas Fases

| Fase | Nombre | Prioridad | Estado | Detalle |
|:----:|:-------|:---------:|:------:|:--------|
| 0 | Congelar desarrollo | 🔴 INMEDIATA | 🟡 Documentado | Pendiente reforzar disciplina UX-first — el equipo sigue agregando features |
| 1 | Filosofía (MASTER_EXPERIENCE.md) | 🔴 INMEDIATA | ✅ COMPLETO | Responde las 7 preguntas fundacionales con tabla de visibilidad |
| 2 | Chat como centro | 🔴 INMEDIATA | ✅ COMPLETO | PanelRevealManager + LlmCommandInterpreter + postUIEvent + chat feedback en panel reveals + celebraciones en el chat |
| 3 | Progressive Disclosure | 🔴 INMEDIATA | ✅ COMPLETO | 14 estados CoachRoomState, crossfade, revealPanel slide-up, tabla expandida, gaps cerrados ✅ |
| 4 | Rediseñar Tabs (Coach/Session/Tools) | 🔴 INMEDIATA | ✅ COMPLETO | 3 tabs exactas, locking progresivo, NEW badges, Panels Menu (☰) |
| 5 | Automatizar interfaz | 🟡 IMPORTANTE | ✅ COMPLETO | Auto-return timer 5s + keyword detection + cancelación en NavigationShell. Loop completo: "Coach dice → UI responde → regresa al chat" |
| 6 | Conversaciones (COACH_PERSONALITY.md) | 🟡 IMPORTANTE | ✅ COMPLETO | Tono, vocabulario, humor, empatía, celebraciones, errores |
| 7 | Flujo completo (SESSION_FLOW.md) | 🟡 IMPORTANTE | ✅ COMPLETO | 14 fases documentadas con objetivo, visibilidad, eventos, animaciones |
| 8 | Paneles (UI_*.md) | 🟡 IMPORTANTE | ✅ COMPLETO | 6 documentos (Chat, Reference, MixMap, Tools, Session, Report) |
| 9 | ExperienceManager | 🟡 IMPORTANTE | ✅ COMPLETO | Implementado + documentado. Celebration cycle, state transitions, wiring |
| 10 | LLM como mentor | 🔴 INMEDIATA | ✅ COMPLETO | Identidad "DIRECTOR DE LA SESIÓN" + DIRECTRICES DE DIRECCIÓN + REASONING step 5.5 + 230 tests |
| 11 | Gamificación | 🔵 FUTURO | ❌ No iniciado | Diferido intencionalmente hasta que UX 2.0 esté 100% operativo |
| 12 | Beta | 🔵 FUTURO | ❌ No iniciado | Pendiente de Fases 11 |

### Progreso general: **9.6/10 — 96% del UX 2.0 implementado**

| Métrica | Valor |
|:--------|:-----:|
| Fases completas | **11/13** (1, 2, 3, 4, 5, 6, 7, 8, 9, 10) |
| Fases en progreso | **1/13** (0) |
| Fases sin empezar | **2/13** (11, 12) |

### Próxima prioridad de implementación

```
🥇 Fase 2 — Pulir chat como centro (14% restante)
    El LLM ya dirige la UI y el auto-return funciona.
    Falta: que el chat sea EL ÚNICO punto de entrada a todos
    los paneles. Simplificar PanelRevealManager para que
    todo pase por el mensaje del Coach, no por clicks.

🥈 Fase 11 — Gamificación (preparar)
    Logros, rachas, XP, badges, ranking personal.
    Solo cuando UX 2.0 esté 100% operativo.

🥉 Fase 12 — Beta con productores reales
    Invitar productores, grabar sesiones, ajustar.
```

---

## 🏆 Estado del Plan 10/10

| Fase | Nombre | Prioridad | Estado |
|:-----|:-------|:---------:|:------:|
| 0 | Sprint 7 + DevOps | 🔴 INMEDIATA | ✅ CI pipeline + GitHub + Git Flow |
| 1 | Core de Inteligencia (MixPriorityEngine) | 🔴 INMEDIATA | ✅ Completado (66 tests) |
| 2 | Loop de Aprendizaje (MixHistory) | 🔴 INMEDIATA | ❌ Pendiente |
| 3 | Consciencia de Género | 🟡 IMPORTANTE | ❌ Pendiente |
| 4 | Pulido UX (UX 2.0 Fases 1-10) | 🟡 IMPORTANTE | 🟡 85% — Ver tabla UX 2.0 arriba |
| 5 | Cobertura de Tests | 🟡 IMPORTANTE | 🟡 33 targets, 214 tests nuevos (CoachRoomState/LlmCmdInterpreter/ExpManager) |
| 6 | Mastering Mode | 🔵 FUTURO | ❌ Pendiente |
| 7 | Modelo Local | 🔵 FUTURO | ❌ Pendiente |

---

## 📈 Cobertura de Tests

### Nuevos tests UX 2.0 (julio 2026)

| Test | Archivo | Tests | Estado | Cubre |
|:-----|:--------|:-----:|:------:|:------|
| **CoachRoomState** | `TestCoachRoomState.cpp` | **77** | ✅ | isPreFullUI, isCoachingState, progress monotónico, labels, enum ordering, count, edge cases |
| **LlmCommandInterpreter** | `TestLlmCommandInterpreter.cpp` | **104** | ✅ | parseCommands (10 actions), processResponse (JSON extraction), executeCommand (callbacks), hasAnyCallbacks, edge cases (malformed JSON, empty arrays, max commands) |
| **ExperienceManager** | `TestExperienceManager.cpp` | **33** | ✅ | celebration lifecycle (animate → 30 frames → complete), duplicate ignore, multiple cycles |

**Total tests UX 2.0 añadidos:** 214
**Total tests del proyecto:** ~1,200+

### Cómo ejecutar los tests

```powershell
# Todos los tests
cmake --build build --config Release --target run_tests

# Tests individuales
./build/tests/Release/TestCoachRoomState.exe
./build/tests/Release/TestLlmCommandInterpreter.exe
./build/tests/Release/TestExperienceManager.exe
```

---

## ❌ Lo que NO construir (próximo mes)

| Feature | Razón |
|:--------|:------|
| ❌ Cloud Sync | No acerca a la visión, viola privacidad |
| ❌ User Profiles | No crítica para el core loop |
| ❌ Mac/Linux | Windows + offline primero |
| ❌ Reportes PDF | Sin valor inmediato |
| ❌ Gamificación | Después del core loop UX 2.0 |
| ❌ Más analizadores | Ya hay suficientes |
| ❌ Bundle de plugins | Después de 10/10 |

---

*Roadmap de producto — MixCoach v4.0 — 4 julio 2026*
*Actualizado con auditoría de implementación UX 2.0 + nuevos tests de infraestructura.*

# 👥 14 — AGENT GUIDE

> **Guía para cualquier IA que trabaje en el proyecto MixCoach.**
> Define cómo debe leer la documentación, qué orden seguir, qué reglas respetar,
> cómo reportar hallazgos, y cómo invocar a otros agentes.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documento base:** `workspace_memory/06_AGENTS_SYSTEM.md`

---

## 📋 Índice

1. [Lectura Obligatoria](#1-lectura-obligatoria)
2. [Orden de Lectura](#2-orden-de-lectura)
3. [Reglas de Oro](#3-reglas-de-oro)
4. [Cómo Reportar Hallazgos](#4-como-reportar-hallazgos)
5. [Cómo Invocar a Otros Agentes](#5-como-invocar-a-otros-agentes)
6. [Lenguaje Prohibido](#6-lenguaje-prohibido)
7. [Zonas de Peligro](#7-zonas-de-peligro)
8. [Checklist Pre-Cambio](#8-checklist-pre-cambio)

---

## 1. Lectura Obligatoria

Todo agente — humano o IA — debe leer estos documentos antes de escribir una línea de código:

### Documentos Fundacionales (leer PRIMERO)

| Orden | Documento | Por qué |
|:------|:----------|:--------|
| 1 | `docs/00_PROJECT_IDENTITY.md` | La constitución. MixCoach no es un plugin, es un mentor. |
| 2 | `docs/02_EXPERIENCE_MANIFESTO.md` | Filosofía UX. La interfaz es una conversación. |
| 3 | `docs/14_AGENT_GUIDE.md` | Este documento. Cómo trabajar en el proyecto. |

### Documentos de Contexto (leer SEGUNDO)

| Orden | Documento | Por qué |
|:------|:----------|:--------|
| 4 | `docs/01_PRODUCT_VISION.md` | Visión comercial y de producto |
| 5 | `docs/04_PHASES.md` | Todas las fases de la sesión |
| 6 | `docs/05_UI_ARCHITECTURE.md` | Qué paneles existen, cuándo aparecen |
| 7 | `workspace_memory/06_AGENTS_SYSTEM.md` | Sistema completo de agentes |

### Documentos Técnicos (leer según el área)

| Área | Documento |
|:-----|:----------|
| **UI / Diseño** | `docs/05_UI_SYSTEM.md`, `docs/06_COMPONENTS/*.md` |
| **Audio / DSP** | `docs/08_ANALYSIS_PIPELINE.md` |
| **LLM / IA** | `docs/07_AI_BEHAVIOR.md` |
| **Engine / C++** | `docs/09_MIX_ENGINE.md`, `docs/13_PLUGIN_ARCHITECTURE.md` |
| **Build / CI** | `SAFE_EDIT_GUIDE.md`, `IPC_CONTRACT.md` |

---

## 2. Orden de Lectura Recomendado para IAs

```
1. docs/02_EXPERIENCE_MANIFESTO.md    ← Filosofía UX (leer PRIMERO)
2. docs/00_PROJECT_IDENTITY.md        ← Constitución del producto
3. docs/14_AGENT_GUIDE.md             ← Este documento
4. docs/04_PHASES.md                  ← Fases de la sesión
5. docs/05_UI_ARCHITECTURE.md         ← UI Architecture
6. docs/13_PLUGIN_ARCHITECTURE.md     ← Arquitectura C++
7. AI_CONTEXT.md                      ← Contexto completo del sistema
8. AI_COMPONENT_INDEX.yaml            ← Índice semántico de componentes
9. SAFE_EDIT_GUIDE.md                 ← Guía de modificación segura
10. docs/09_ROADMAP.md                ← Roadmap de producto
```

---

## 3. Reglas de Oro

### Regla #1 — No romper nada

> Si el código compila y pasa tests antes de tu cambio, debe seguir compilando y pasando tests después.

### Regla #2 — Entender antes de actuar

> Siempre leer el archivo completo antes de editarlo. Siempre entender el contexto antes de sugerir un cambio.

### Regla #3 — El Chat manda

> Ningún elemento UI existe antes de que el Coach lo necesite. La interfaz es una conversación, no un panel de control.

### Regla #4 — Datos reales, no inventados

> El LLM nunca calcula métricas. El LLM nunca inventa datos. Todo diagnóstico viene del engine C++.

### Regla #5 — Cuestiona al agente

> Si otro agente propone algo que contradice la visión de MixCoach, cuéstionalo. El Guardian tiene la última palabra.

---

## 4. Cómo Reportar Hallazgos

Todo agente debe reportar en este formato estructurado:

```
RESUMEN:      [Una línea del veredicto]
PROBLEMA:     [Qué problema se identificó o qué se hizo]
CAUSA:        [Por qué ocurre o por qué se tomó esta decisión]
SOLUCIÓN:     [Qué se implementó o qué se recomienda]
RIESGOS:      [Riesgos potenciales de la solución]
IMPACTO:      [Impacto en el proyecto: archivos, módulos, tests]
ARCHIVOS:     [Lista de archivos creados/modificados/eliminados]
TESTS:        [Tests que validan el cambio y su resultado]
CONFIANZA:    [0-100% de confianza en la recomendación]
```

### Ejemplo

```
RESUMEN:      Análisis de crest para Kick en reggaetón optimizado
PROBLEMA:     El crest threshold para Kick en reggaetón era 8dB,
              pero el target debería ser 10-14dB
CAUSA:        Los thresholds genéricos no consideraban el perfil
              específico del género reggaetón
SOLUCIÓN:     Ajustado kCrestTarget para TrackRole::kKick en
              género Reggaeton de 8dB a 12dB en GenreProfiles
RIESGOS:      Puede afectar a usuarios que tenían kicks
              intencionalmente comprimidos
IMPACTO:      Solo afecta a sesiones nuevas con género Reggaeton
ARCHIVOS:     Source/MixCoach/engine/TrackRole.h (una línea)
TESTS:        TestGenreProfiles (126 tests) → 126/126 PASS
CONFIANZA:    92%
```

---

## 5. Cómo Invocar a Otros Agentes

Usar `@AgentName` para invocar a un agente específico:

```
@Chief-Architect Evalúa este cambio: [descripción]
@Audio-Intelligence ¿Es correcto este threshold para Kick en reggaetón?
@UX-Designer Revisa el layout de esta nueva pantalla
@AI-Systems Revisa este cambio en los prompts
@QA Revisa este cambio — ¿cubre los casos borde?
@Performance ¿Este cambio introduce allocations en el audio thread?
@Guardian Revisa este cambio contra la visión del producto
```

### Agentes Disponibles

| Agente | Rol | Misión | @mention |
|:-------|:----|:-------|:---------|
| **Chief Architect** | Arquitecto | Proteger la arquitectura | `@Chief-Architect` |
| **Audio Intelligence** | DSP Engineer | FFT, LUFS, fase, análisis | `@Audio-Intelligence` |
| **AI Systems** | LLM Engineer | Prompts, memoria, tono | `@AI-Systems` |
| **UX/Product Designer** | Diseñador | Experiencia, flujos, layout | `@UX-Designer` |
| **QA & Reliability** | Tester | Tests, bugs, regresiones | `@QA` |
| **Performance Engineer** | Performance | CPU, RAM, locks, xruns | `@Performance` |
| **Guardian de la Visión** | Guardián | Veta cambios contra la identidad | `@Guardian` |

---

## 6. Lenguaje Prohibido

| ❌ Prohibido | ✅ Alternativa |
|:-------------|:---------------|
| "No sería mala idea..." | "Recomiendo: [acción concreta]" |
| "Podríamos..." | "Propongo: [acción específica]" |
| "Quizá..." | "Análisis: [hecho]. Decisión: [acción]" |
| "No estoy seguro..." | "Confianza: 40%. Necesito revisar [X]" |
| "Creo que..." | "Según [fuente/dato]: [conclusión]" |
| "Tal vez funcionaría..." | "Viable con [condición]. Riesgo: [riesgo]" |
| "Depende..." | "Si [condición] → A. Si no → B" |

**Regla:** Si tienes dudas, cuantifícalas. No las escondas en lenguaje vago.

---

## 7. Zonas de Peligro

### Archivos que NO tocar sin autorización explícita

| Archivo | Riesgo | Razón |
|:--------|:------:|:------|
| `SharedMemory.h/.cpp` | 🔴 CORE | IPC V6. Si se rompe, 0 comunicación |
| `SlotRegistry.h/.cpp` | 🔴 CORE | 128 slots, lock-free parcial |
| `SharedData.h/.cpp` | 🔴 CORE | Singleton bridge IPC+audio+UI |
| `AudioAnalyzer.h/.cpp` | 🔴 CORE | FFT/LUFS/fase del Master |
| `CoachEngine.h/.cpp` | 🔴 CORE | Motor de IA, 6 fases |
| `Types.h` | 🔴 CORE | 18 archivos dependen |
| `TrackRole.h` | 🔴 CORE | Perfiles de rol, targets por género |
| `MixCoachTheme.h` | 🔴 CORE | Tema visual usado por TODOS los componentes |

### Lo que NUNCA debe hacer un agente

| Prohibición | Razón |
|:------------|:-------|
| Mover el chat del centro visual | Rompe la experiencia |
| Mostrar scores numéricos al usuario | El usuario no debe sentirse juzgado |
| Hacer que el LLM calcule métricas | Datos incorrectos, rompe la confianza |
| Abrir un panel sin orden del Coach | Rompe Progressive Disclosure |
| Dejar al usuario sin siguiente paso | Se siente perdido |
| Usar jerga técnica sin explicación | Excluye a principiantes |
| Hacer que la UI decida diagnósticos | El Coach es quien diagnostica |
| Avanzar de etapa sin explicar por qué | El usuario no aprende |

---

## 8. Checklist Pre-Cambio

Antes de marcar cualquier tarea como completa, verificar:

### 🟢 Compilación
- [ ] Compila en Release sin warnings nuevos
- [ ] Compila en Debug (+ aserciones pasan)
- [ ] Todos los targets afectados compilan

### 🟢 Tests
- [ ] Tests existentes pasan
- [ ] Tests nuevos agregados si hay lógica nueva
- [ ] Suite de tests completa sin fallos

### 🟢 Regresiones
- [ ] No se rompió compatibilidad IPC
- [ ] No se eliminaron campos de SharedSlotEntry
- [ ] No se cambiaron firmas de API pública sin actualizar callers
- [ ] No se introdujeron nuevas dependencias circulares

### 🟢 Calidad
- [ ] Sin `std::cout`, `printf`, logs en hot paths
- [ ] Sin números mágicos
- [ ] Sin includes innecesarios

### 🟢 Documentación
- [ ] `CMakeLists.txt` actualizado si hay archivos nuevos
- [ ] Si cambió API pública, actualizar header comment
- [ ] Si cambió flujo IPC, actualizar `IPC_CONTRACT.md`

---

*Documento de guía para agentes — MixCoach — 4 julio 2026*
*Todo agente que trabaje en MixCoach debe leer este documento primero.*
*Violaciones a estas reglas deben reportarse al fundador.*

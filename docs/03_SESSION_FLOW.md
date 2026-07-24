# 🎯 03 — SESSION_FLOW.md

> **El viaje completo de una sesión de MixCoach, fase por fase.**
> Define objetivos, componentes visibles/ocultos, eventos, animaciones, mensajes del Coach, botones disponibles y condiciones para avanzar.
>
> **Versión:** 2.0 | **Última actualización:** 19 julio 2026
> **Documentos relacionados:**
> - `02_MASTER_EXPERIENCE.md` — Filosofía de experiencia
> - `04_COACH_PERSONALITY.md` — Tono y lenguaje del Coach
> - `05_UI_ARCHITECTURE.md` — Arquitectura UI con split-view
> - `COACHING_NARRATIVE.md` — Loop de coaching 7 pasos

---

## ═══ VISIÓN GENERAL ═══

MixCoach opera en **dos capas de fase** que corren en paralelo:

### Capa 1: SessionProgression (Alto Nivel)

```
Setup ──→ LoadReference ──→ DeepAnalysis ──→ GuidedCoaching ──→ Refinement ──→ Report ──→ Memory
```

### Capa 2: MentorPhase (Nivel de Mezcla)

```
Organización ──→ Gain Staging ──→ Balance ──→ EQ ──→ Compresión ──→ Espacio ──→ Master Check
```

### Capa 3: CoachingNarrativeDirector (NUEVO en v2.0)

Para CADA problema dentro de una MentorPhase, se ejecuta el loop narrativo:

```
DETECT → SHOW EVIDENCE → EXPLAIN → SHOW OPTIONS → WAIT USER → VERIFY → CELEBRATE → NEXT
```

Este loop se documenta en detalle en `docs/COACHING_NARRATIVE.md`.

---

## ═══ MATRIZ DE FASES HTML ↔ C++ (NUEVA) ═══

### Fases de Setup

| Prototipo HTML | CoachRoomState | SceneManager SceneId | Director Event | Estado |
|:---------------|:---------------|:---------------------|:---------------|:------:|
| Welcome | `Welcome` | `Welcome` | `UserNameEntered` → `ModeSelection` | ✅ |
| Mode Selection | `Intention` | `ModeSelection` | `ModeSelected` → `GenreSelection` | ✅ |
| Genre Selection | `Genre` | `GenreSelection` | `GenreSelected` → `ReferenceLoad` | ✅ |
| Reference Load | `ReferenceStage` | `ReferenceLoad` | `ReferenceLoaded` / `ReferenceSkipped` → `SetupComplete` | ✅ |
| Session Prep | `SessionPrep` | `SetupComplete` | `SessionPrepped` → `MixMapReview` | ✅ |
| Mix Map Review | `MixMapStage` | `MixMapReview` | `MixMapConfirmed` → `Coaching` | ✅ |

### Fases de Coaching

| Prototipo HTML | CoachRoomState | Panel en EvidenceHost | Analyzer contextual | Estado |
|:---------------|:---------------|:----------------------|:--------------------|:------:|
| Gain Staging | `GainStaging` | GainStagingPanel | VU (peak + RMS) | ✅ |
| Balance | `Balance` | GainStagingPanel | VU (balance relativo) | ✅ |
| EQ | `EQ` | EQPanel | Spectrum + highlight | ✅ |
| Compression | `Compression` | CompressionPanel | Crest gauge | ✅ |
| Space | `Space` | SpacePanel | Vectorscope mini | ✅ |
| *Automatización* | *(no implementado)* | — | — | ❌ Gap |
| Master Check | `MasterCheck` | MasterCheckPanel | Match Score ring | ✅ |
| Report | `Report` | EndOfSessionComponent | — | ✅ |

### Sub-fases (SetupStep / CoachEngineSetup)

Cada sub-fase es un paso en el diálogo interactivo de bienvenida.

#### 0.1 — NOT STARTED

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | No se ha enviado el saludo inicial |
| **Visible** | Solo Chat + Avatar. Sin input de texto aún? (no, el input siempre está visible) |
| **Oculto** | Todo: referencia, messengers, mixmap, tools, session, report |
| **Coach dice** | — (aún no habla) |
| **Condición para avanzar** | Se dispara `startSetupDialogue()` desde el timer del editor |

#### 0.2 — WAITING FOR NAME

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | El Coach pregunta el nombre del usuario |
| **Visible** | Chat + Avatar + Input de texto + Botón de enviar |
| **Oculto** | Botones Mix/Master, chips de género, referencia, messengers |
| **Estado UI** | `CoachRoomState::Welcome` — welcomeMode = true |
| **Coach dice** | "¡Hola! Bienvenido a MixCoach. ¿Cómo te llamas?" |
| **Botones/Sugerencias** | Ninguno (input libre) |
| **Eventos** | SetupStepChanged(NotStarted → WaitingForName) |
| **Condición para avanzar** | Usuario escribe su nombre → se guarda en `engineerName_` |

#### 0.3 — WAITING FOR MODE

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | El usuario elige Mix o Master |
| **Visible** | Chat + Avatar + Input |
| **Oculto** | Referencia, messengers, mixmap |
| **Estado UI** | `CoachRoomState::Intention` |
| **Transición** | SetupStepChanged(WaitingForName → WaitingForMode) |
| **Auto-advance** | robotNod(400) + Timer::callAfterDelay(400ms, showGenreCards) |
| **Coach dice** | "¿Qué vamos a hacer hoy? Mix o Master?" |
| **Botones/Sugerencias** | Chips: 🎛 **Mix** · 🎱 **Master** |
| **Condición para avanzar** | Usuario escribe "Mix" o "Master" → se bifurca el flujo |

#### 0.4 — WAITING FOR REFERENCE FIRST (V6, Mix Mode only)

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Cargar referencia ANTES del género (V6 flow) |
| **Visible** | Chat + Avatar + Reference Panel (DropZone) 🔓 *se revela aquí* |
| **Oculto** | Género, messengers, mixmap, tools |
| **Estado UI** | `CoachRoomState::ReferenceStage` |
| **Transición** | SetupStepChanged(WaitingForMode → WaitingForReferenceFirst) |
| **Animación** | Reference Panel aparece con crossfade + badge "NEW" |
| **Coach dice** | "¿Tienes una referencia de cómo quieres que suene? Arrastra un archivo WAV/MP3 o pega un enlace." |
| **Evento** | `revealPanel(PanelId::Reference)` |
| **Condición para avanzar** | Referencia cargada + analizada → `advanceFromReferenceFirst()` |

#### 0.5 — WAITING FOR GENRE

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Confirmar género inferido de la referencia o elegir género manualmente |
| **Visible** | Chat + Avatar + Reference Panel |
| **Oculto** | Messengers, mixmap, tools |
| **Estado UI** | `CoachRoomState::Genre` |
| **Transición** | SetupStepChanged(WaitingForReferenceFirst → WaitingForGenre) |
| **Coach dice** | Perfil de género enriquecido con frecuencias por rol (Kick → 40-60Hz, etc.) |
| **Botones/Sugerencias** | Grid de 16 géneros con filtro de búsqueda |
| **Condición para avanzar** | Usuario confirma o escribe otro género |

#### 0.6 — WAITING FOR CONFIRM

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Usuario confirma género + scan de pistas |
| **Visible** | Chat + Avatar + (Reference si aplica) |
| **Oculto** | Messengers, mixmap, tools |
| **Coach dice** | "Género: [Género]. Pistas detectadas: [N]. ¿Es correcto?" |
| **Botones/Sugerencias** | ✅ **Sí** · ✏️ **Cambiar género** |
| **Condición para avanzar** | Usuario confirma → `advanceFromGenreConfirm()` |

#### 0.7 — WAITING FOR REFERENCE (V5 fallback, Mix Mode)

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Preguntar si el usuario tiene referencia (V5: cuando no se cargó antes) |
| **Visible** | Chat + Avatar |
| **Oculto** | Reference Panel (aún no revelado) |
| **Coach dice** | "¿Tienes una canción de referencia para comparar?" |
| **Botones/Sugerencias** | ✅ **Sí** · ❌ **No aún** |
| **Condición para avanzar** | Si sí → revelar Reference Panel. Si no → avanzar a Preparar Sesión |

#### 0.8 — WAITING FOR SETUP INSTRUCTIONS

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Guiar al usuario a organizar su sesión: renombrar pistas, asignar roles |
| **Visible** | Chat + Avatar + SessionPrepChecklist (inline) |
| **Oculto** | MixMap, tools |
| **Estado UI** | `CoachRoomState::SessionPrep` |
| **Transición** | SetupStepChanged(WaitingForConfirm → WaitingForSetupInstructions) |
| **Coach dice** | "Antes de mezclar, necesito que organices tu sesión." |
| **Checklist** | 4 items: Insertar Messenger en cada pista, Renombrar pistas, Asignar colores, Crear buses |
| **Auto-avance** | robotNod() por cada item completado |
| **Condición para avanzar** | Botón "Verificar" clickeado → `onSessionPrepConfirmed` |
| **Post-confirmación** | Preguntas interactivas para pistas no identificadas vía QuickReplyBar |

#### 0.9 — COMPLETE

| Aspecto | Detalle |
|:--------|:--------|
| **Objetivo** | Setup completado. Transición a MixMap + Gain Staging |
| **Visible** | Chat + Avatar + MixMapComponent |
| **Oculto** | EvidenceHost, tools (aún no) |
| **Estado UI** | `CoachRoomState::MixMapStage` |
| **Transición** | SetupStepChanged(WaitingForSetupInstructions → Complete) |
| **Coach dice** | "✅ Sesión organizada! He mapeado todas tus pistas. Confirma el mapa para empezar." |
| **Animación** | Celebración: ✨ + setAvatarPoint() |
| **Condición para avanzar** | Botón "Confirmar mapa" → GainStaging |

---

## ═══ FASE 1-2: LOAD REFERENCE + DEEP ANALYSIS ═══

Ver docs v1 para detalles completos. En v2.0, el flujo de referencia usa:

- 5 stages de análisis en `ReferenceAnalysisProgressCard` (15% → 35% → 55% → 75% → 100%)
- Mensajes secuenciales post-análisis vía `onStageChanged` callback
- Animación SmoothValue de 3s de duración

---

## ═══ FASE 3: GUIDED COACHING — LOOP NARRATIVO ═══

### El Cambio Fundamental en v2.0

En lugar de tabs separadas (Coach vs Tools), durante el coaching el layout es **split-view**:

```
┌──────────────────────┬───────────────────────────────────────┐
│  CHAT (38%)          │  COACHING EVIDENCE HOST (62%)        │
│  (mensajes + avatar) │  (phase panel + evidence mini-view)  │
└──────────────────────┴───────────────────────────────────────┘
```

### CoachingNarrativeDirector — El Nuevo Orquestador

Para CADA problema detectado, el Director ejecuta este loop:

```cpp
1. DETECT (implícito, motor detecta)
   → Coach postea "He encontrado un problema"

2. SHOW EVIDENCE (typing 800ms + system msg)
   → Abre analyzer contextual + highlight frequency
   → "🔍 Abriendo medidores con alerta..."

3. EXPLAIN (typing 1200ms + coach msg)
   → "El Kick y el Bass están compitiendo en 60 Hz"

4. SHOW OPTIONS (typing 800ms + 3 tiers)
   → "Puedes resolverlo de varias maneras:"
   → 🎛 Nativo (Fruity Parametric EQ 2)
   → 🟢 Gratis (TDR Nova)
   → ⭐ Profesional (FabFilter Pro-Q 4)

5. WAIT USER
   → QuickReplyBar muestra las 3 opciones
   → Usuario elige una

6. VERIFY (1500ms + typing + system msg)
   → "Aplicando cambio... Escuchando... Verificando..."
   → "✅ El masking se redujo. Delta: -2.3 dB."

7. CELEBRATE (typing 1000ms + coach msg + XP)
   → "🎉 ¡Excelente trabajo! Has mejorado la claridad espectral."
   → "⚡ +45 XP · +12% progreso"

8. NEXT (system msg)
   → "Buscando el siguiente problema..."
   → Vuelve al paso 1 con el siguiente issue prioritario
```

### Mapeo Problema → Evidencia Visual (vía ProblemAnalyzerMap)

| Problema | Analyzer | Highlight | Mensaje del sistema |
|:---------|:---------|:----------|:--------------------|
| Clipping | VU / Peak | Pista en rojo | "🔴 Recorte digital en la pista" |
| Masking | Spectrum | 60 Hz (banda glow roja) | "🔊 Enmascaramiento entre instrumentos" |
| Tonal Excess | Spectrum | 2.5 kHz (banda glow roja) | "⬆ Exceso de energía espectral" |
| Crest bajo | Crest gauge | Valor objetivo | "📈 Compresión excesiva (crest bajo)" |
| Fase | Vectorscope | Correlación | "🔮 Problemas de correlación estéreo" |
| Estéreo | Vectorscope | Ancho estéreo | "🌊 Problemas de imagen estéreo" |
| Saturation | Spectrum | Armónicos | "🎲 Necesidad de saturación armónica" |
| Limiting | LUFS | Target | "🔊 Necesidad de limitación" |

### Timing Targets (del prototipo HTML, implementados en ChatMessageSequencer)

| Paso | Delay typing | Delay post-mensaje |
|:-----|:------------:|:------------------:|
| Detect | 800ms | 0ms |
| Evidence label | — | 300ms |
| Explain | 1200ms | 0ms |
| Options | 800ms | 0ms |
| Verify | 1500+1200+1800ms | escalonado |
| Celebrate | 1000ms | 0ms |
| XP feedback | — | 500ms |

---

## ═══ TABLA COMPLETA DE TRANSICIONES ═══

### CoachRoomState (con SceneManager)

```cpp
From → To                         Evento / Trigger
──────────────────────────────────────────────────────
Welcome → Intention               UserNameEntered
Intention → Genre                 ModeSelected
Genre → ReferenceStage            GenreSelected
ReferenceStage → SessionPrep      ReferenceAnalyzed o ReferenceSkipped
SessionPrep → MixMapStage         SessionPrepped
MixMapStage → GainStaging         MixMapConfirmed (setCoachRoomState directo)
GainStaging → Balance             CoachingStage cambia (PhaseManager)
Balance → EQ                      CoachingStage cambia
EQ → Compression                  CoachingStage cambia
Compression → Space               CoachingStage cambia
Space → MasterCheck               Fase final
MasterCheck → Report              Session end
```

---

## ═══ COMPORTAMIENTO DEL ROBOT POR FASE ═══

| Fase / Evento | Expresión Avatar | Animación |
|:--------------|:----------------:|:----------|
| Welcome (inicio) | `Neutral` | Float idle |
| Usuario escribe nombre | `Happy` | Nod(400) + Wave |
| Selecciona modo | `Thinking` | Nod(400) |
| Selecciona género | `Encouraging` | Nod(500) |
| Referencia cargada | `Thinking` | — |
| Análisis referencia completado | `Happy` | Nod(400) |
| Checklist completado | `Happy` | Nod(600) |
| MixMap confirmado → Coaching | `Happy` | Nod(600) |
| Problema detectado | `Thinking` | — |
| Evidencia mostrada | `Thinking` | — |
| Explicación | `Neutral` | Nod(600) |
| Esperando respuesta | `Neutral` | — |
| Verify exitoso | `Celebrating` | Celebrate bounce |
| XP otorgado | `Celebrating` | Nod(300) |
| Reporte mostrado | `Happy` | Wave |

---

*Documento de flujo de sesión — MixCoach UX 2.0 — 19 julio 2026*
*Basado en implementación: CoachingNarrativeDirector, ChatMessageSequencer, ProblemAnalyzerMap, SceneManager, PhaseManager.*

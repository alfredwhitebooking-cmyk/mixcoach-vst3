# 🏗️ 05 — UI ARCHITECTURE

> **La arquitectura de la interfaz de MixCoach.**
> Define qué paneles existen, cuándo aparecen, cuándo desaparecen,
> cómo se crean y destruyen, y cómo se relacionan entre sí.
>
> **Versión:** 2.0 | **Última actualización:** 19 julio 2026
> **Documentos relacionados:**
> - `02_EXPERIENCE_MANIFESTO.md` — Filosofía UX
> - `03_SESSION_FLOW.md` — Narrativa de sesión
> - `COACHING_NARRATIVE.md` — Loop de coaching 7 pasos
> - `05_UI_SYSTEM.md` — Sistema de diseño visual (colores, glassmorphism, tipografía)

---

## 📋 Índice

1. [Filosofía de Arquitectura UI](#1-filosofia-de-arquitectura-ui)
2. [Inventario de Paneles](#2-inventario-de-paneles)
3. [Split-View Coaching (Nuevo en v2.0)](#3-split-view-coaching)
4. [CoachingNarrativeDirector](#4-coachingnarrativedirector)
5. [ChatMessageSequencer](#5-chatmessagsequencer)
6. [BackgroundEffectsComponent](#6-backgroundeffectscomponent)
7. [Matriz de Visibilidad por Fase](#7-matriz-de-visibilidad-por-fase)
8. [Layout General](#8-layout-general)
9. [Transiciones y Animaciones](#9-transiciones-y-animaciones)
10. [Estados de cada Panel](#10-estados-de-cada-panel)
11. [Flujo de Datos UI](#11-flujo-de-datos-ui)
12. [Arquitectura de Componentes](#12-arquitectura-de-componentes)

---

## 1. Filosofía de Arquitectura UI

### Principios Rectores

1. **La Conversación es el Contenedor.** Todo panel vive dentro o junto al chat. No hay ventanas flotantes ni modales fuera de la jerarquía del chat.

2. **Los Paneles son Herramientas, No Ventanas.** Como si el Coach sacara herramientas de una caja. Aparecen cuando se necesitan, desaparecen cuando ya no.

3. **Lifecycle Dinámico.** Los paneles se crean cuando se revelan por primera vez y se destruyen cuando la sesión cambia de fase. No hay componentes "vivos pero ocultos".

4. **Split-View en Coaching (NUEVO).** Durante las fases de coaching (GainStaging → MasterCheck), el layout se divide en dos columnas: **chat (38%) + evidencia contextual (62%)**. El chat y la evidencia NUNCA se separan durante el coaching.

5. **Tab Tools = Modo Experto.** El tab Tools sigue existiendo para análisis profundos, pero NO es necesario para completar el flujo de coaching. La evidencia básica (VU, Spectrum, Crest, Vectorscope, Match Score) se muestra en el panel derecho del split-view.

### La Metáfora del Teatro

```
Coach → invoca Reference → usuario usa → Reference desaparece
Coach → invoca MixMap → usuario usa → MixMap desaparece
Coach → invoca Analyzer → usuario entiende → Analyzer desaparece
```

Cada actor (panel) entra cuando le toca. Nadie está en escena antes de tiempo.

---

## 2. Inventario de Paneles

### 2.1 Panel: Chat (Siempre visible)

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Coach` |
| **Componente** | `MixCoachPanel` (contiene `CoachChatComponent` + `ChatMessagesComponent`) |
| **Creación** | Al iniciar el plugin |
| **Destrucción** | Nunca |
| **Visibilidad** | Siempre 100% visible (en coaching: 38% ancho) |
| **Contenido** | Burbujas de chat, avatar, input de texto, sugerencias, QuickReplyBar |

### 2.2 Panel: Reference

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Reference` |
| **Componente** | `ReferencePanelComponent` + `DropZoneComponent` + `ReferenceOnboardingCard` |
| **Creación** | Cuando el Coach dice "referencia" por primera vez |
| **Destrucción** | Al finalizar la sesión o al cambiar de proyecto |
| **Estados** | `Hidden` → `DropZone` → `Analyzing` → `Ready` (colapsado) |

### 2.3 Panel: CoachingEvidenceHost (NUEVO)

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Evidence` (interno, no en tabBar) |
| **Componente** | `CoachingEvidenceHost` |
| **Creación** | Al entrar a primera fase de coaching (GainStaging) |
| **Destrucción** | Al salir de coaching (Report) |
| **Layout interno** | Header (fase + título) → Phase Panel (65%) + Evidence Panel (35%) → Dots de progreso |
| **Contenido** | GainStagingPanel / EQPanel / CompressionPanel / SpacePanel / MasterCheckPanel según la fase activa |

### 2.4 Panel: Phase Panels (GainStaging, EQ, Compression, Space, MasterCheck)

| Propiedad | Valor |
|:----------|:------|
| **Componentes** | `GainStagingPanel`, `EQPanel`, `CompressionPanel`, `SpacePanel`, `MasterCheckPanel` |
| **Hosteados por** | `CoachingEvidenceHost` (visible en coaching) |
| **Visibilidad** | Solo UN panel visible a la vez según `CoachRoomState` |

### 2.5 Panel: Evidence Panel (Mini-analyzer contextual)

| Propiedad | Valor |
|:----------|:------|
| **Componente** | `EvidencePanel` |
| **Visibilidad** | Siempre visible dentro de `CoachingEvidenceHost` en coaching |
| **Contenido** | Mini-VU / Mini-Spectrum / Mini-Crest / Mini-Vectorscope / Match Score según fase |
| **Highlight** | `ProblemAnalyzerMap` propaga `highlightFreq` al `SpectrographComponent` con línea roja pulsante |

### 2.6 Panel: Messenger List

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Messengers` |
| **Componente** | `MessengerListComponent` |
| **Creación** | Cuando el Coach dice "messenger" / "pista" por primera vez |

### 2.7 Panel: Mix Map

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::MixMap` |
| **Componente** | `MixMapComponent` |

### 2.8 Panel: Tools / Analyzers (Modo Experto)

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Tools` |
| **Componente** | `AnalyzersPanelComponent` (Spectrum, PhaseScope, Meters, VU) |
| **Comportamiento** | Tab independiente. Se abre automáticamente SOLO si el usuario solicita análisis profundo. Durante coaching normal, la evidencia se muestra en el panel derecho. |

### 2.9 Panel: Progress / Session

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Session` |
| **Componente** | `ProgressScreen` |

### 2.10 Panel: Report (Overlay)

| Propiedad | Valor |
|:----------|:------|
| **ID** | `PanelId::Report` |
| **Componente** | `EndOfSessionComponent` |

---

## 3. Split-View Coaching (Nuevo en v2.0)

### Layout durante Coaching

```
┌──────────────────────────────────────────────────────────────┐
│ [●─●─○─○─○─○─○]  FASE: GAIN STAGING         ⭐ 1,200 XP   │  ← PhaseProgressBar (siempre visible)
├──────────────────────┬───────────────────────────────────────┤
│   CHAT (38%)         │   COACHING EVIDENCE HOST (62%)         │
│                      │   ┌─────────────────────────────────┐ │
│  [Avatar] Coach msg  │   │ ◉ GAIN STAGING                  │ │
│  [Thinking...]       │   │ Ajusta niveles de cada pista    │ │
│  System: evidencia   │   ├────────────┬────────────────────┤ │
│                      │   │ PHASE PANEL│ EVIDENCE PANEL     │ │
│  Options:            │   │  (65%)     │   (35%)            │ │
│  🎛 Nativo           │   │            │                    │ │
│  🟢 Gratis           │   │ Gain Level  │ Mini-VU bars      │ │
│  ⭐ Profesional      │   │ sliders     │ (contextual según  │ │
│                      │   │             │  fase activa)      │ │
│  [✏️ Escribe...]     │   ├────────────┴────────────────────┤ │
│                      │   │ ●─●─○─○─○─○─○ (7 dots)         │ │
└──────────────────────┴───────────────────────────────────────┘
```

### Reglas del Split-View

1. **Siempre visible en coaching.** Desde GainStaging hasta MasterCheck, el split-view está activo.
2. **38/62 fijo.** El chat ocupa 380px (escalable) y el host de evidencia ocupa el resto.
3. **Sin cambio de tab.** No se requiere cambiar a Tools para ver evidencia.
4. **El Tab Tools sigue existiendo** como modo experto para análisis profundos (Spectrum completo, Vectorscope grande, etc.).

### Implementación

- `NavigationShell::resized()` detecta `isCoachingState()` y aplica el layout split-view.
- `CoachingEvidenceHost` gestiona los paneles de fase y evidencia.
- `EvidencePanel` cambia su contenido según `CoachRoomState`:
  - GainStaging → VU meters
  - EQ → Spectrum mini + highlight
  - Compression → Crest gauge
  - Space → Vectorscope mini
  - MasterCheck → Match Score

---

## 4. CoachingNarrativeDirector

### Responsabilidad

Único secuenciador del loop de coaching. Orquesta los 7 pasos narrativos para cada problema detectado:

```
DETECT → SHOW EVIDENCE → EXPLAIN → OPTIONS → VERIFY → CELEBRATE → NEXT
```

### Ciclo de Vida

```cpp
// 1. El motor detecta un problema (CoachEngine / MixPriorityEngine)
// 2. NavigationShell construye un CoachingProblem via buildFromProblemType()
// 3. Director.startProblem(problem) inicia el ciclo
// 4. El Director postea mensajes, abre analyzers, maneja QuickReplies
// 5. Al completar, dispara onCycleComplete()
// 6. NavigationShell avanza al siguiente problema o fase
```

### Integración

| Componente | Relación con Director |
|:-----------|:----------------------|
| `ChatMessageSequencer` | El Director encola pasos en el Sequencer para timing controlado |
| `CoachingEvidenceHost` | El Director configura la evidencia (highlightFreq, analyzer) |
| `QuickReplyBar` | El Director muestra/oculta QuickReplies en ShowOptions y WaitingForUser |
| `ProblemAnalyzerMap` | El Director consulta el mapping ProblemType→analyzer view |
| `PhaseProgressBar` | El Director dispara triggerXpBurst() al completar un ciclo |
| `SpectrographComponent` | El Director propaga highlightFreq con línea roja via setSpectrumHighlight() |
| `RobotAvatarComponent` | El Director setea expresiones (Thinking, Neutral, Celebrating) |

### Callbacks

| Callback | Disparo |
|:---------|:--------|
| `onCycleComplete` | Ciclo de un problema terminado → XP burst |
| `onTierSelected` | Usuario elige un tier de plugin |
| `onStartVerification` | Iniciar verificación del cambio aplicado |

---

## 5. ChatMessageSequencer

### Responsabilidad

Cola de mensajes con timing controlado para replicar el ritmo narrativo del prototipo HTML.

### SequencerStep

```cpp
struct SequencerStep {
    enum Type { TypingOn, TypingOff, CoachMessage, SystemMessage, Delay, Callback };
    Type type;
    juce::String text;
    int delayMs;
    std::function<void()> onComplete;
};
```

### Timing Targets (copiados del prototipo HTML)

| Paso | Delay antes | Duración mensaje |
|:-----|:-----------:|:----------------:|
| Detect (typing) | — | 800ms |
| Evidence label | 0ms | 300ms |
| Explain | 1000ms | 0ms |
| Options | 1200ms | 0ms |
| Verify | 800ms | 1500ms+1200ms+1800ms escalonado |
| Celebrate | 1000ms | 0ms |
| XP feedback | 500ms | system msg |

### API

```cpp
void enqueue(SequencerStep step);
void enqueueBatch(std::vector<SequencerStep> steps);
void cancel();
bool isBusy() const;
```

---

## 6. BackgroundEffectsComponent

### Responsabilidad

Renderiza partículas de fondo (25 dots flotantes con colores púrpura/cian/verde) + 3 glow orbs decorativos con animación float + noise texture overlay. Se pinta detrás de todo usando alpha compositing.

### Capas de renderizado (de atrás hacia adelante)

1. **Noise overlay** — ruido procedural regenerado cada 60 frames a alpha 0.02-0.03
2. **Glow orbs** — 3 orbes con animación float sinusoidal independiente, colores: púrpura, cian, verde
3. **Particles** — 25 dots con movimiento browniano, alpha 0.15-0.35, rebote en bordes

Visible durante todo el flujo, aporta profundidad visual sin distraer.

---

## 7. Matriz de Visibilidad por Fase

### CoachRoomState (Progressive Disclosure)

| State | Chat | EvidenceHost | Ref | Msgrs | Map | Tools | Progress | Report |
|:------|:----:|:------------:|:---:|:-----:|:---:|:-----:|:--------:|:------:|
| Welcome | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Intention | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Genre | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| ReferenceStage | ✅ | ❌ | 🔓 | ❌ | ❌ | ❌ | ❌ | ❌ |
| SessionPrep | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| MixMapStage | ✅ | ❌ | ✅ | ✅ | 🔓 | ❌ | ❌ | ❌ |
| GainStaging | ✅ | ✅ | ✅ | ✅ | ✅ | 🔒 | ❌ | ❌ |
| Balance | ✅ | ✅ | ✅ | ✅ | ✅ | 🔒 | ❌ | ❌ |
| EQ | ✅ | ✅ | ✅ | ✅ | ✅ | 🔓 | ❌ | ❌ |
| Compression | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Space | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| MasterCheck | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Report | ❌* | ❌ | ❌* | ❌* | ❌* | ❌* | ❌* | ✅ |

*\* Report es un overlay que reemplaza todo.*

### Matriz HTML ↔ C++ (NUEVA)

| Prototipo HTML | C++ CoachRoomState | Notas |
|:---------------|:-------------------|:------|
| Welcome | `Welcome` | ✅ Alineado |
| Mode Selection | `Intention` | ✅ Alineado |
| Genre Selection | `Genre` | ✅ Alineado |
| Reference Load | `ReferenceStage` | ✅ Alineado |
| Session Prep | `SessionPrep` | ✅ Alineado |
| Mix Map Review | `MixMapStage` | ✅ Alineado |
| Gain Staging | `GainStaging` | ✅ Alineado |
| Balance | `Balance` | ✅ Alineado |
| EQ | `EQ` | ✅ Alineado |
| Compression | `Compression` | ✅ Alineado |
| Space | `Space` | ✅ Alineado |
| *Automatización* | *(no implementado)* | ❌ Gap: pendiente en plan |
| Master Check | `MasterCheck` | ✅ Alineado |
| Report | `Report` | ✅ Alineado |

---

## 8. Layout General

### 8.1 Modo Setup (Welcome → MixMapStage)

```
┌──────────────────────────────────────────────────────────────┐
│ [PhaseProgressBar: dots + XP]                                │  ← Siempre visible
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  CHAT (100% width)                     │   │
│  │                                                       │   │
│  │  [Avatar] Coach bubble                                │   │
│  │  [Mode Selection Cards] o [Genre Grid]               │   │
│  │  [Reference DropZone] o [SessionPrep Checklist]       │   │
│  │                                                       │   │
│  │  [✏️ Escribe...]                              [➤]    │   │
│  └──────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

### 8.2 Modo Coaching (GainStaging → MasterCheck) — SPLIT-VIEW

```
┌──────────────────────────────────────────────────────────────┐
│ [PhaseProgressBar: ●─●─●─○─○─○─○]      GAIN STAGING  ⭐ XP │  ← Siempre visible
├──────────────────────┬───────────────────────────────────────┤
│  CHAT (38%)          │  COACHING EVIDENCE HOST (62%)          │
│                      │  ┌───────────────────────────────────┐ │
│  [Avatar]            │  │ ◉ GAIN STAGING                   │ │
│  ...                 │  │ Ajusta niveles de cada pista     │ │
│                      │  ├────────────┬──────────────────────┤ │
│  [Thinking...]       │  │ PHASE      │ EVIDENCE PANEL       │ │
│  Coach: "Encontré    │  │ PANEL      │ (contextual)         │ │
│   un problema..."    │  │ (65%)      │ (35%)                │ │
│                      │  │            │                      │ │
│  Options:            │  │ Sliders    │ Mini-VU              │ │
│  🎛 Nativo           │  │            │                      │ │
│  🟢 Gratis           │  ├────────────┴──────────────────────┤ │
│  ⭐ Profesional      │  │ ●─●─○─○─○─○─○                   │ │
│                      │  └───────────────────────────────────┘ │
└──────────────────────┴────────────────────────────────────────┘
```

### 8.3 Modo Tools (Modo Experto)

```
┌──────────────────────────────────────────────────────────────┐
│  ┌──────────────────┐  ┌──────────────────────────────────┐ │
│  │  CHAT             │  │  ANALYZERS (full)                │ │
│  │  (38%)            │  │  (62%)                           │ │
│  │                   │  │                                  │ │
│  │  ...              │  │  Spectrum  |  Phase Scope        │ │
│  │                   │  │  ─────────┼───────────           │ │
│  │  [Volver al       │  │  Meters   |  VU Meters          │ │
│  │   Coach]          │  │                                  │ │
│  └──────────────────┘  └──────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 8.4 Modo Reporte (Overlay)

```
┌──────────────────────────────────────────────────────────────┐
│  ┌────────────────────────────────────────────────────────┐ │
│  │                    REPORT OVERLAY                      │ │
│  │                                                        │ │
│  │  Score: 72/100  |  Correcciones: 12/18                 │ │
│  │  Has aprendido: EQ, Compression, Space                 │ │
│  │  Progreso: 42% → 72%                                  │ │
│  │                                                        │ │
│  │  [Exportar HTML]  [Nueva Sesión]                      │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  (Chat oculto detrás del overlay)                            │
└──────────────────────────────────────────────────────────────┘
```

---

## 9. Transiciones y Animaciones

### 9.1 Reglas de Animación

| Tipo | Duración | Easing | Propósito |
|:-----|:---------|:-------|:----------|
| Panel Reveal | 200ms | ease-out quad | Primera aparición de un panel |
| Crossfade entre modos | 150ms | ease-out | Cambio entre Coach / Tools / Session |
| Setup Fade (P1-P6) | 300ms | ease-out quad | Transición entre pantallas de setup |
| Colapso de bloque | 200ms | ease-out | Tarea completada → resumen |
| Highlight de track | 200ms | ease-out | Coach menciona un track |
| Clear highlights | 100ms | ease-out | Nuevo mensaje del Coach |
| Badge "NEW" | 500ms | ease-out | Panel recién desbloqueado |
| XP Burst | 1500ms | float-up + fade | Celebración de XP al completar fase |

### 9.2 Secuencia de Reveal de Panel

```cpp
1. t=0ms:    Panel invisible (alpha=0, translateY=20)
2. t=0-50ms: Badge "NEW" aparece con glow
3. t=0-200ms: Fade-in + slide-up (alpha 0→1, translateY 20→0)
4. t=200ms:  Panel completamente visible
5. t=2000ms: Badge "NEW" se desvanece lentamente
```

### 9.3 Secuencia de SetupFadeAnim

```cpp
// Duración total: ~300ms (18 frames a 60fps)
// Easing: ease-out quad (t*(2-t))
// kFadeInStart = 0.3 → pausa inicial de ~90ms con alpha=0
// Luego fade-in alpha 0→1 en los siguientes ~210ms
```

---

## 10. Estados de cada Panel

### Estados Comunes

| Estado | Qué se muestra | Texto |
|:-------|:---------------|:------|
| **Hidden** | Nada. El panel no existe. | — |
| **Locked** | Candado 🔒 + tooltip | "El Coach te guiará aquí cuando sea el momento." |
| **Loading** | Spinner + texto | "Analizando..." / "Cargando..." |
| **Empty** | Icono + hint | "Arrastra tu referencia aquí" |
| **Ready** | Contenido normal | — |
| **Error** | ⚠️ + mensaje + botón | "Algo salió mal. [Reintentar]" |

---

## 11. Flujo de Datos UI

### 11.1 Durante Setup (Welcome → MixMapStage)

```
CoachEngine → SceneManager → NavigationShell::processDirectorEvent()
                  ↓
         applyScene(sceneDef)
                  ↓
         setCoachRoomState() + postUIEvent()
```

### 11.2 Durante Coaching (GainStaging → MasterCheck)

```
MixPriorityEngine / CoachEngine
    → detecta problema
    → CoachingNarrativeDirector::startProblem(CoachingProblem)
        → ChatMessageSequencer::enqueueBatch(typing → delay → msg → ...)
        → ProblemAnalyzerMap::lookup(problemType)
        → navShell_.setSpectrumHighlight(freq, label)
        → evidencePanel_.setCoachRoomState(phase)
        → coachPanel_.showQuickReplies(options)
        → onOptionSelected() → onCorrectionConfirmed()
        → doCelebrateStep() → triggerXpBurst()
        → onCycleComplete()
    → NavigationShell avanza al siguiente problema
```

### 11.3 Direccionalidad

```
CoachEngine → ExperienceManager → NavigationShell
                  ↓                      ↓
          PanelRevealManager       SceneManager
                  ↓                      ↓
           Panel aparece/disparece     Layout cambia

CoachingNarrativeDirector (dueño del loop)
    → ChatMessageSequencer (timing)
    → ProblemAnalyzerMap (evidencia estructural)
    → QuickReplyBar (opciones)
    → PhaseProgressBar (XP)
```

### 11.4 Lo que la UI NUNCA hace

| ❌ Prohibido | Razón |
|:------------|:------|
| Decide qué fase viene | Eso lo hace SessionDirector + PhaseManager |
| Analiza audio | Eso lo hace AudioAnalyzer |
| Genera respuestas del Coach | Eso lo hace el LLM |
| Calcula métricas | Eso lo hace el engine C++ |
| Mueve faders | El usuario tiene el control físico |

---

## 12. Arquitectura de Componentes

### 12.1 Jerarquía de Componentes (v2.0)

```
PluginEditor
  └── NavigationShell
        ├── PhaseProgressBar (siempre visible, 28px)
        ├── BackgroundEffectsComponent (partículas + orbes + noise)
        ├── TabBarComponent (3 tabs: Coach, Session, Tools)
        │
        ├── WelcomeComponent (STATE 0, reemplazado por crossfade)
        │
        ├── MixCoachPanel (STATE 1+)
        │   ├── ChatMessagesComponent (burbujas de chat)
        │   ├── CoachChatComponent (input + avatar + quick replies)
        │   ├── CoachingEvidenceHost (panel derecho en coaching)
        │   │   ├── GainStagingPanel | EQPanel | CompressionPanel
        │   │   │   | SpacePanel | MasterCheckPanel
        │   │   └── EvidencePanel (mini analyzer contextual)
        │   ├── ReferencePanelComponent
        │   ├── MessengerListComponent
        │   └── MixMapComponent
        │
        ├── AnalyzersPanelComponent (tools: spectrum, phase, meters, VU)
        ├── ProgressScreen (timeline gamificado)
        └── EndOfSessionComponent (reporte overlay)
```

### 12.2 Managers de Orquestación (v2.0)

| Manager | Responsabilidad | Creado por |
|:--------|:---------------|:-----------|
| **CoachingNarrativeDirector** | Único secuenciador del loop 7 pasos | NavigationShell |
| **ChatMessageSequencer** | Cola de mensajes con delays controlados | Director |
| **ProblemAnalyzerMap** | Mapeo estructural ProblemType → analyzer view | Estático |
| **SceneManager** | Crea/destruye paneles según la escena actual | NavigationShell |
| **ExperienceManager** | Orquesta transiciones UI, animaciones, celebraciones | NavigationShell |
| **PanelRevealManager** | Detecta keywords en mensajes del Coach y revela paneles | NavigationShell |
| **LlmCommandInterpreter** | Parsea comandos JSON del LLM y ejecuta acciones UI | NavigationShell |

### 12.3 Flujo de Control Completo (v2.0)

```
SETUP FLOW (SceneManager controla):
  1. Usuario escribe nombre → onStart → processDirectorEvent(UserNameEntered)
  2. SceneManager.processEvent() → devuelve SceneDef → applyScene()
  3. applyScene() setea CoachRoomState + postea coachMessage
  4. Auto-advance: robotNod() + Timer::callAfterDelay(400ms) → próximo paso

COACHING FLOW (NarrativeDirector controla):
  1. LLM genera respuesta → NavigationShell detecta problema
  2. buildFromProblemType() → CoachingProblem
  3. Director.startProblem(problem) inicia loop 7 pasos
  4. Cada paso usa ChatMessageSequencer para timing + mensajes
  5. ProblemAnalyzerMap provee evidencia estructural
  6. QuickReplyBar para opciones 3 tiers
  7. CorrectionLearner para verificación
  8. Ciclo completo → triggerXpBurst() + onCycleComplete()

TOOLS FLOW (Modo experto, solo si usuario o LLM lo piden):
  1. commandInterpreter_.onSelectAnalyzer(analyzer)
  2. Si director activo → no cambia de tab (evidencia ya visible)
  3. Si director idle → switch a Tools tab + auto-return timer
```

---

*Documento de arquitectura UI — MixCoach v2.0 — 19 julio 2026*
*Este documento refleja la arquitectura actual con split-view coaching,*
*CoachingNarrativeDirector como secuenciador único, y ProblemAnalyzerMap como fuente de evidencia estructural.*

# 🏗️ 01 — ARCHITECTURE.md

> **El mapa del sistema. Flujo de datos, responsabilidades de cada capa, y reglas arquitectónicas.**
> Todos los agentes deben entender este documento antes de modificar cualquier componente.
>
> **Versión:** 1.0 | **Última actualización:** 26 junio 2026

---

## 📋 Índice

1. [Arquitectura General](#1-arquitectura-general)
2. [Flujo de Datos](#2-flujo-de-datos)
3. [Mesos: Messenger](#3-messenger)
4. [Shared Memory Layer](#4-shared-memory-layer)
5. [TrackFeed Layer](#5-trackfeed-layer)
6. [CoachEngine (El Cerebro)](#6-coachengine-el-cerebro)
7. [AI/LLM Layer](#7-aillm-layer)
8. [UI Layer](#8-ui-layer)
9. [Reglas Arquitectónicas](#9-reglas-arquitectónicas)
10. [Dependencias Prohibidas y Permitidas](#10-dependencias-prohibidas-y-permitidas)
11. [Archivos [CORE] — NO TOCAR](#11-archivos-core--no-tocar)
12. [Flujo por Fase de Mentoría](#12-flujo-por-fase-de-mentoría)

---

## 1. Arquitectura General

```
┌─────────────────────────────────────────────────────────────────────┐
│                        DAW (FL Studio)                              │
│                                                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐            ┌──────────┐  │
│  │Messenger │  │Messenger │  │Messenger │  ... 128   │ MixCoach │  │
│  │(Pista 1) │  │(Pista 2) │  │(Pista N) │            │(Master)  │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘            └────┬─────┘  │
│       │             │             │                       │        │
│       ▼             ▼             ▼                       ▼        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              Shared Memory Layer (IPC V6)                    │  │
│  │  ┌─────────────────────┐  ┌──────────────────────────────┐  │  │
│  │  │ SharedMemory V6     │  │ SharedAudioMemory V2         │  │  │
│  │  │ (identidad: nombre, │  │ (audio RAW estéreo,          │  │  │
│  │  │  color, bus, active)│  │  ring buffer lock-free)      │  │  │
│  │  └─────────────────────┘  └──────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                               │                                    │
│                               ▼                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              Background Worker (10-30Hz)                     │  │
│  │  ┌──────────────────────────────────────────────────────────┐│  │
│  │  │ • forceFullSync() — lee SharedMemory V6                 ││  │
│  │  │ • readSamples() — lee audio RAW de cada Messenger       ││  │
│  │  │ • Computa RMS/Peak real por pista                       ││  │
│  │  │ • Actualiza TrackAudioResult en SharedData cache        ││  │
│  │  │ • checkStaleSlots() — marca inactivos sin heartbeat     ││  │
│  │  └──────────────────────────────────────────────────────────┘│  │
│  └──────────────────────────────────────────────────────────────┘  │
│                               │                                    │
│                               ▼                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              TrackFeed Layer                                 │  │
│  │  Mantiene estado vivo de cada pista: health, eventos,        │  │
│  │  última actualización, crest, espectro, etc.                 │  │
│  │  Fuente: SharedData::getTrackAudioResult()                   │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                               │                                    │
│                               ▼                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              CoachEngine (Message Thread, ~8s)               │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │  │
│  │  │ Track       │  │ MixPriority │  │ MixScore           │  │  │
│  │  │ Intelligence│  │ Engine      │  │ (salud de mezcla)  │  │  │
│  │  │ (Gain/Dyn/  │  │ (prioriza   │  └────────────────────┘  │  │
│  │  │  Tonal)     │  │  issues)    │                           │  │
│  │  └─────────────┘  └──────────────┘                           │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │  │
│  │  │ Difference  │  │ Reference   │  │ Session           │  │  │
│  │  │ Profile     │  │ Driven      │  │ Progression       │  │  │
│  │  │             │  │ Engine      │  │ (fases y logros)  │  │  │
│  │  └─────────────┘  └──────────────┘  └────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                               │                                    │
│                               ▼                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              AI/LLM Layer (AiCoachAdapter)                   │  │
│  │  Recibe diagnóstico estructurado, genera respuesta           │  │
│  │  en lenguaje natural. NUNCA calcula métricas.                │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                               │                                    │
│                               ▼                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              UI Layer (60fps timer)                          │  │
│  │  • Dashboard — visión general, próximo paso, prioridades     │  │
│  │  • Chat — conversación con el coach                          │  │
│  │  • MixMap — árbol jerárquico de buses                        │  │
│  │  • Analyzers — FFT, LUFS, fase, estéreo                      │  │
│  │  • Reference — comparación con referencia                    │  │
│  │  • Report — resumen de sesión                                │  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Flujo de Datos (Vista Simplificada)

```
Messenger → Shared Memory → TrackFeed → CoachEngine → LLM → UI

Cada flecha respeta la dirección. NO hay caminos inversos.

PROHIBIDO:  UI → CoachEngine (la UI no decide)
PROHIBIDO:  LLM → SharedMemory (el LLM no escribe datos)
PROHIBIDO:  CoachEngine → Audio Thread (el engine no bloquea audio)
```

---

## 3. Messenger (Sensor por Pista)

**Rol:** Capturar audio RAW + identidad de pista. Pasar todo al MixCoach sin procesar.

**Responsabilidades:**
- Enviar audio RAW a `SharedAudioMemoryV2` (ring buffer lock-free)
- Mantener heartbeat (`SlotRegistry.setActive()`)
- Identidad: nombre, color, bus, tipo (desde UI del Messenger)

**Lo que NO hace:**
- ❌ No calcula FFT, LUFS, RMS, correlación, ni nada de DSP
- ❌ No decide buses ni roles (solo expone los que el usuario selecciona)
- ❌ No escribe archivos en el audio thread

**Regla crítica:** Messenger es 100% pas-through. Todo análisis ocurre en MixCoach.

**Archivos:** `Source/Messenger/core/PluginProcessor.h/.cpp`

---

## 4. Shared Memory Layer

**Rol:** Transportar datos entre procesos (Messenger → MixCoach) de forma segura y lock-free.

**Dos canales de IPC:**

| Canal | Contenido | Tamaño | Sincronización |
|:------|:----------|:-------|:---------------|
| `SharedMemory V6` | Identidad: 128 slots × (name, color, bus, active) | ~80 bytes/slot | Spinlock two-phase |
| `SharedAudioMemory V2` | Audio RAW estéreo: 128 slots × 4096 samples | ~4.2 MB | Lock-free (volatile + barriers) |

**Reglas IPC (inviolables):**
- `SharedSlotEntry` es POD-only. Nada de `std::string`, `std::vector`, punteros.
- Si se modifica `SharedSlotEntry`, INCREMENTAR `kCurrentStructVersion`.
- Los campos nuevos SIEMPRE al final del struct.
- NO reordenar, NO cambiar tipos, NO eliminar campos legacy.
- `CreateFileMappingW` puede lanzar SEH — constructor del plugin debe estar VACÍO.

**Archivos:** `Source/Common/memory/SharedMemory.h/.cpp`, `Source/Common/memory/SharedAudioMemoryV2.h/.cpp`, `Source/Common/memory/SlotRegistry.h/.cpp`

---

## 5. TrackFeed Layer

**Rol:** Mantener estado vivo de cada pista. Corazón de la UI de tracks.

**Responsabilidades:**
- Almacenar `TrackState` por slot (health, eventos recientes, crest, band energies, etc.)
- Generar eventos (gain, dynamics, tonal, phase) en cada ciclo
- Computar health consolidado de cada pista
- Exponer top eventos globales para UI

**Lo que NO hace:**
- ❌ No decide prioridades entre tracks (eso es del `MixPriorityEngine`)
- ❌ No genera mensajes al usuario
- ❌ No persiste datos entre sesiones

**Archivos:** `Source/MixCoach/engine/TrackFeedCore.h/.cpp`, `Source/MixCoach/engine/TrackState.h`

---

## 6. CoachEngine (El Cerebro)

**Rol:** Analizar, priorizar y generar diagnósticos. Es el corazón de la inteligencia de MixCoach.

**Subsistemas:**

| Subsistema | Función | Archivos |
|:-----------|:--------|:---------|
| **Track Intelligence** | Analiza gain, dinámica, tonal, fase por pista | `TrackGainAnalyzer`, `TrackDynamicsAnalyzer`, `TrackTonalAnalyzer`, `TrackPhaseAnalyzer` |
| **MixPriorityEngine** | Prioriza issues: severidad × rol × dominio × género | `MixPriorityEngine.h/.cpp` |
| **MixScore** | Score de salud general (solo interno, nunca visible) | `MixScore.h/.cpp` |
| **DifferenceProfile** | Comparación mix vs referencia | `DifferenceProfile.h/.cpp` |
| **ReferenceDrivenEngine** | Modo referencia activo | `ReferenceDrivenEngine.h/.cpp` |
| **CorrectionLearner** | Aprende de correcciones del usuario | `CorrectionLearner.h/.cpp` |
| **FeedbackCollector** | Ajusta thresholds según comportamiento | `FeedbackCollector.h/.cpp` |
| **SessionProgression** | Máquina de fases | `SessionProgression.h/.cpp` |
| **PlanManager** | Plan contra referencia | `PlanManager.h/.cpp` |

**Lo que NO hace:**
- ❌ No ejecuta la UI
- ❌ No genera prompts LLM directamente (usa `AiCoachAdapter`)
- ❌ No escribe a SharedMemory
- ❌ No procesa audio en tiempo real (solo lee datos cacheados)

**Regla crítica:** CoachEngine opera en el **message thread** (timer ~8s). Nunca en el audio thread.

---

## 7. AI/LLM Layer

**Rol:** Traducir diagnósticos técnicos en coaching comprensible.

**Responsabilidades:**
- Recibir `TrackAdvice[]` estructurado desde `CoachEngine`
- Generar respuesta en lenguaje natural al chat
- Adaptar tono según fase y experiencia del usuario
- Mantener memoria de sesión

**Lo que NUNCA hace:**
- ❌ Calcula métricas (FFT, LUFS, crest, etc.)
- ❌ Inventa datos que no vienen del engine
- ❌ Toma decisiones técnicas (priorización, diagnóstico)
- ❌ Escribe a `SharedData` o `SharedMemory`

**Archivos:** `Source/MixCoach/ai/AiCoachAdapter.h/.cpp`, `AiCoachAdapterPrompts.cpp`, `AiCoachAdapterSession.cpp`, `LlmClient.h/.cpp`

---

## 8. UI Layer

**Rol:** Comunicar, acompañar, guiar. Es la cara visible de MixCoach.

**Pantallas principales:**

| Pantalla | Componente | Función |
|:---------|:-----------|:--------|
| **Dashboard** | `DashboardScreen` | Visión general, próximo paso, prioridades |
| **Chat** | `MixCoachPanel` | Conversación con el coach |
| **MixMap** | `MixMapComponent` | Árbol jerárquico de buses y pistas |
| **Analyzers** | `AnalyzersPanelComponent` | FFT, LUFS, fase, estéreo |
| **Reference** | `ReferenceVisualPanel` | Comparación con referencia |
| **Progress** | `ProgressScreen` | Historial multi-sesión |
| **Report** | `EndOfSessionComponent` | Resumen de sesión |
| **Sidebar** | `SidebarComponent` | Navegación entre pantallas |

**Lo que NO hace:**
- ❌ No analiza audio (solo lee datos cacheados)
- ❌ No decide mentoría (solo muestra lo que el engine decidió)
- ❌ No bloquea el audio thread

**Regla:** Toda la UI se actualiza desde el **timer 60fps** del message thread. Nunca desde el audio thread.

---

## 9. Reglas Arquitectónicas

### 9.1 Direccionalidad

```
✅ PERMITIDO:
  UI → engine          (lectura de datos cacheados)
  UI → audio           (lectura de análisis)
  UI → SharedData      (lectura)
  engine → SharedData  (lectura)
  engine → AudioAnalyzer (lectura)
  Message Thread → Background Worker (datos fluyen hacia UI)

❌ PROHIBIDO:
  engine → UI              (el engine no llama a la UI)
  audio → UI               (el audio thread no toca la UI)
  SharedData → engine      (SharedData no llama al engine)
  AudioAnalyzer → engine   (el analyzer no llama al engine)
  Audio Thread → Heap/File I/O/Locks
```

### 9.2 Thread Safety

| Thread | Componentes | Frecuencia | Prohibido |
|:-------|:------------|:-----------|:----------|
| **Audio** | Messengers, AudioAnalyzer | Cada bloque (~2.9ms) | Heap, file I/O, locks, UI |
| **Background** | SharedData sync, RMS compute | ~10-30Hz | UI, locks bloqueantes |
| **Message** | UI draw, CoachEngine, LLM | 60fps | Análisis de audio pesado, I/O bloqueante |

### 9.3 Data Flow Timeline

```
t=0ms     AudioAnalyzer.processBlock() → FFT, LUFS, fase
t=0-16ms  SmoothValue.advance() → meters animados
t=100ms   Background Worker → RMS/Peak per-track
t=500ms   CoachEngine.periodicAnalysis() → diagnóstico
t=500ms+  AiCoachAdapter → respuesta al chat
t=8000ms  CoachEngine full cycle → actualización completa
```

---

## 10. Dependencias Prohibidas y Permitidas

### Include Graph

```
Messenger/
  └── Core/       → Common/memory/, Common/types/
  └── UI/         → Common/types/

MixCoach/
  ├── audio/      → Common/audio/, Common/types/
  ├── engine/     → audio/, Common/memory/, Common/types/, Common/audio/
  ├── ai/         → engine/, Common/types/
  ├── UI/         → engine/, audio/, Common/memory/, Common/types/
  └── core/       → everything (entry point)

Common/
  ├── memory/     → types/
  ├── audio/      → types/
  └── types/      → (standalone, solo constantes)
```

**Reglas de include:**
- `Common/types/` es standalone — no incluye nada del proyecto
- `Common/memory/` solo incluye `Common/types/`
- `Common/audio/` solo incluye `Common/types/`
- `engine/` puede incluir `audio/`, `memory/`, `types/`
- `UI/` puede incluir `engine/`, `audio/`, `memory/`, `types/`
- `core/` (PluginEditor/Processor) incluye todo

---

## 11. Archivos [CORE] — NO TOCAR

| Archivo | Riesgo | Razón | Test requerido |
|---------|:------:|-------|----------------|
| `SharedMemory.h/.cpp` | 🔴 CORE | IPC V6. Si se rompe, 0 comunicación | TestStress128Slots |
| `SlotRegistry.h/.cpp` | 🔴 CORE | 128 slots, lock-free parcial | TestSlotRegistry |
| `SharedData.h/.cpp` | 🔴 CORE | Singleton bridge IPC+audio+UI | TestIPCIntegration |
| `AudioAnalyzer.h/.cpp` | 🔴 CORE | FFT/LUFS/fase del Master | Compilar + deploy |
| `CoachEngine.h/.cpp` | 🔴 CORE | Motor de IA, 6 fases | TestCoachEngine |
| `Types.h` | 🔴 CORE | 18 archivos dependen | Múltiples |
| `Constants.h` | 🔴 CORE | Constantes globales | TestSpectralBands |
| `TrackRole.h` | 🔴 CORE | Perfiles de rol, targets por género | TestGenreProfiles |

---

## 12. Flujo por Fase de Mentoría

```
FASE 0: SETUP
  ├── MixCoach pregunta género y objetivo
  └── Usuario carga referencia (si tiene)

FASE 1: IDENTIDAD
  ├── Messenger registra slots
  ├── CoachEngine infiere roles (nombre + espectro)
  └── Usuario confirma roles en UI

FASE 2: MAPA DE MEZCLA
  ├── CoachEngine construye SessionMap
  ├── MixMapComponent renderiza árbol de buses
  └── Usuario confirma routing

FASE 3: COACHING ACTIVO
  ├── periodicAnalysis() cada ~8s
  │   ├── syncTrackFeedCore()
  │   ├── analyzeTrackGain()
  │   ├── analyzeTrackDynamics()
  │   ├── analyzeTrackTonal()
  │   └── MixPriorityEngine.prioritize()
  ├── AiCoachAdapter genera respuesta
  └── UI actualiza chat + dashboard

FASE 4: REFERENCIA
  ├── ReferenceDrivenEngine compara mix vs referencia
  ├── DifferenceProfile calcula gaps
  └── Coach guía hacia el target

FASE 5: REFINAMIENTO
  ├── Análisis de profundidad, impacto, movimiento
  ├── Sugerencias de espacio estéreo y FX
  └── Preparación para exportación

FASE 6: REPORTE
  ├── EndOfSessionComponent muestra resumen
  ├── MixScore + evolución multi-sesión
  └── Exportación opcional a HTML
```

---

*Documento arquitectónico — MixCoach — 26 junio 2026*
*Leer antes de modificar cualquier archivo fuera de UI/*
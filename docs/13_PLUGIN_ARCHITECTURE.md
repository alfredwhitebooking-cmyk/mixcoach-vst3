# 🏗️ 13 — PLUGIN ARCHITECTURE

> **Los dos plugins VST3 y su arquitectura C++.**
> Define componentes, build system, IPC, thread safety, y reglas arquitectónicas.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documento base:** `workspace_memory/01_ARCHITECTURE.md`, `workspace_memory/02_CODING_RULES.md`
> **Stack:** C++20, JUCE 8, CMake 3.22+, Visual Studio 17 2022

---

## 📋 Índice

1. [Los Dos Plugins](#1-los-dos-plugins)
2. [Arquitectura General](#2-arquitectura-general)
3. [Stack Tecnológico](#3-stack-tecnologico)
4. [Estructura de Directorios](#4-estructura-de-directorios)
5. [Flujo de Datos](#5-flujo-de-datos)
6. [IPC (Shared Memory)](#6-ipc-shared-memory)
7. [Thread Safety](#7-thread-safety)
8. [Build System](#8-build-system)
9. [Archivos CORE — No Tocar](#9-archivos-core)
10. [Reglas Arquitectónicas](#10-reglas-arquitectonicas)

---

## 1. Los Dos Plugins

| Plugin | Rol | Ubicación en DAW | Tamaño |
|:-------|:----|:-----------------|:-------|
| **Messenger** | Sensor pasivo. Pasa audio RAW + identidad | 1 por pista | 3.4 MB |
| **MixCoach** | Cerebro. Analiza TODO y mentoriza | Canal Master | 6.7 MB |

### Messenger (Sensor)

```
Archivo VST3: Messenger.vst3
Role: Capturar audio RAW + identidad de pista. 100% pas-through.
Lo que NO hace: No analiza, no procesa, no decide.
```

### MixCoach (Cerebro)

```
Archivo VST3: MixCoach.vst3
Role: Analizar, priorizar, mentorizar. Corazón de la inteligencia.
Lo que NO hace: No procesa audio en tiempo real, no mueve faders.
```

---

## 2. Arquitectura General

```
┌──────────────────────────────────────────────────────────────────────┐
│                          DAW (FL Studio)                             │
│                                                                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐         ┌──────────┐      │
│  │Messenger │  │Messenger │  │Messenger │  ...128 │ MixCoach │      │
│  │(Pista 1) │  │(Pista 2) │  │(Pista N) │         │(Master)  │      │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘         └────┬─────┘      │
│       │             │             │                    │             │
│       ▼             ▼             ▼                    ▼             │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │                   Shared Memory Layer (IPC V6)                │   │
│  │  ┌──────────────────────┐  ┌───────────────────────────────┐  │   │
│  │  │ SharedMemory V6      │  │ SharedAudioMemory V2          │  │   │
│  │  │ (identidad: nombre,  │  │ (audio RAW estéreo,           │  │   │
│  │  │  color, bus, active) │  │  ring buffer lock-free)       │  │   │
│  │  └──────────────────────┘  └───────────────────────────────┘  │   │
│  └───────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │                    CoachEngine (Cerebro)                      │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐  │   │
│  │  │ Track        │  │ MixPriority  │  │ MixScore           │  │   │
│  │  │ Intelligence │  │ Engine       │  │ (salud de mezcla)  │  │   │
│  │  │ (Gain/Dyn/   │  │ (prioriza    │  └────────────────────┘  │   │
│  │  │  Tonal)      │  │  issues)     │                           │   │
│  │  └──────────────┘  └──────────────┘                           │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐  │   │
│  │  │ Difference   │  │ Reference    │  │ Session            │  │   │
│  │  │ Profile      │  │ Driven       │  │ Progression        │  │   │
│  │  │              │  │ Engine       │  │ (fases y logros)   │  │   │
│  │  └──────────────┘  └──────────────┘  └────────────────────┘  │   │
│  └───────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │                   AI/LLM Layer (AiCoachAdapter)               │   │
│  │  Recibe diagnóstico, genera respuesta. NUNCA calcula métricas.│   │
│  └───────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │                    UI Layer (60fps timer)                     │   │
│  └───────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Stack Tecnológico

| Componente | Detalle |
|:-----------|:--------|
| **Lenguaje** | C++20 |
| **Framework** | JUCE 8 (como submodulo en `libs/JUCE`) |
| **Compilador** | Visual Studio 17 2022 (MSBuild, NO Ninja) |
| **Build System** | CMake 3.22+ |
| **Formato** | VST3 (bundle/directorio) |
| **DAW primario** | FL Studio (Windows) |
| **IPC** | CreateFileMappingW (memoria compartida inter-proceso) |

---

## 4. Estructura de Directorios

```
Source/
├── Messenger/              # Plugin Messenger (captura audio)
│   ├── core/               # PluginProcessor, SharedMemory
│   └── UI/                 # Interfaz del Messenger
├── MixCoach/               # Plugin principal
│   ├── core/               # PluginProcessor, PluginEditor
│   ├── audio/              # Análisis de audio DSP
│   │   ├── AudioAnalyzer.h/.cpp
│   │   └── ReferenceAnalyzer.h/.cpp
│   ├── engine/             # CoachEngine, analizadores, priorización
│   │   ├── CoachEngine.h/.cpp (+ 12 archivos de funciones)
│   │   ├── MixPriorityEngine.h/.cpp
│   │   ├── PhaseManager.h/.cpp
│   │   ├── PanelRevealManager.h/.cpp
│   │   ├── ExperienceManager.h/.cpp
│   │   ├── LlmCommandInterpreter.h/.cpp
│   │   └── ... (TrackGainAnalyzer, TrackDynamicsAnalyzer, etc.)
│   ├── ai/                 # LLM, prompts, adaptadores
│   │   ├── AiCoachAdapter.h/.cpp
│   │   ├── AiCoachAdapterPrompts.cpp
│   │   ├── AiCoachAdapterSession.cpp
│   │   └── LlmClient.h/.cpp
│   └── UI/                 # Toda la interfaz visual (22+ componentes)
│       ├── NavigationShell.h/.cpp
│       ├── DashboardScreen.h/.cpp
│       ├── SidebarComponent.h/.cpp (sidebar de 7 secciones)
│       ├── CoachChatComponent.h/.cpp
│       ├── ... (analyzers, meters, reference, progress, report)
│       └── MixCoachTheme.h (tema visual unificado)
└── Common/                 # Código compartido
    ├── types/              # Constantes, tipos, enums globales
    ├── memory/             # SharedMemory, SlotRegistry, IPC
    └── audio/              # AudioAnalysis, LoudnessAnalyzer
```

---

## 5. Flujo de Datos

### Direccionalidad

```
Messenger → Shared Memory → TrackFeed → CoachEngine → LLM → UI

Cada flecha respeta la dirección. NO hay caminos inversos.

PROHIBIDO:  UI → CoachEngine (la UI no decide)
PROHIBIDO:  LLM → SharedMemory (el LLM no escribe datos)
PROHIBIDO:  CoachEngine → Audio Thread (el engine no bloquea audio)
```

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

---

## 6. IPC (Shared Memory)

### Dos Canales de IPC

| Canal | Contenido | Tamaño | Sincronización |
|:------|:----------|:-------|:---------------|
| `SharedMemory V6` | Identidad: 128 slots × (name, color, bus, active) | ~80 bytes/slot | Spinlock two-phase |
| `SharedAudioMemory V2` | Audio RAW estéreo: 128 slots × 4096 samples | ~4.2 MB | Lock-free (volatile + barriers) |

### Reglas IPC (inviolables)

- `SharedSlotEntry` es POD-only. Nada de `std::string`, `std::vector`, punteros.
- Si se modifica `SharedSlotEntry`, INCREMENTAR `kCurrentStructVersion`.
- Los campos nuevos SIEMPRE al final del struct.
- NO reordenar, NO cambiar tipos, NO eliminar campos legacy.
- `CreateFileMappingW` puede lanzar SEH — constructor del plugin debe estar VACÍO.

---

## 7. Thread Safety

### Thread Affinity Map

| Thread | Componentes | Frecuencia | Prohibido |
|:-------|:------------|:-----------|:----------|
| **Audio** | Messengers, AudioAnalyzer | Cada bloque (~2.9ms) | Heap, file I/O, locks, UI |
| **Background** | SharedData sync, RMS compute | ~10-30Hz | UI, locks bloqueantes |
| **Message** | UI draw, CoachEngine, LLM | 60fps | Análisis de audio pesado, I/O bloqueante |

### Reglas de Thread Safety

| Regla | Descripción |
|:------|:------------|
| **El audio thread nunca espera** | Spinlocks con timeout máximo 100ns |
| **La UI nunca toca SharedMemory sin mutex** | Usar SharedData como bridge |
| **Dos hilos no escriben el mismo cache** | Cada hilo tiene su región de escritura |
| **Los objetos JUCE se crean en message thread** | Graphics, Font, Image |
| **Los timers nunca se crean desde audio thread** | startTimerHz() solo desde message thread |
| **std::atomic para flags cross-thread** | atomic<bool>, atomic<int> |

---

## 8. Build System

### CMake

```cmake
cmake_minimum_required(VERSION 3.22)
project(MixCoach VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

# JUCE como submodulo
set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE")

# Dos targets de plugin:
juce_add_plugin(MixCoach ...)     # Plugin principal
juce_add_plugin(Messenger ...)    # Plugin sensor

# Librería estática para engine (evita recompilar ~6000 líneas por test)
add_library(MixCoachEngine STATIC ${MIXCOACH_ENGINE_SOURCES})
```

### Scripts de Build

| Script | Propósito |
|:-------|:----------|
| `build.ps1` | Build completo Release |
| `build_fast.ps1` | Build incremental (solo cambios) |
| `DeployVST3.ps1` | Copiar .vst3 a FL Studio |
| `cleanup_system.ps1` | Limpiar builds y caches |

---

## 9. Archivos CORE — No Tocar

| Archivo | Riesgo | Razón |
|:--------|:------:|:------|
| `SharedMemory.h/.cpp` | 🔴 CORE | IPC V6. Si se rompe, 0 comunicación |
| `SlotRegistry.h/.cpp` | 🔴 CORE | 128 slots, lock-free parcial |
| `SharedData.h/.cpp` | 🔴 CORE | Singleton bridge IPC+audio+UI |
| `AudioAnalyzer.h/.cpp` | 🔴 CORE | FFT/LUFS/fase del Master |
| `CoachEngine.h/.cpp` | 🔴 CORE | Motor de IA, 6 fases |
| `Types.h` | 🔴 CORE | 18 archivos dependen |
| `Constants.h` | 🔴 CORE | Constantes globales |
| `TrackRole.h` | 🔴 CORE | Perfiles de rol, targets por género |

---

## 10. Reglas Arquitectónicas

### Prohibiciones Absolutas

| Prohibición | Alternativa |
|:------------|:------------|
| Variables globales no constantes | Pasar por constructor o SharedData |
| Singletons mutables | Dependency injection |
| `std::cout` / `printf` | Usar `MIXCOACH_LOG` |
| `using namespace std` | Namespace explícito |
| Cast a `any` | `std::variant` o herencia |
| Raw pointers compartidos | `std::unique_ptr`, `juce::WeakReference` |

### Responsabilidad de Cada Capa

| Capa | Responsabilidad | Lo que NO hace |
|:-----|:----------------|:---------------|
| **Messenger** | Captura audio RAW + identidad | No analiza, no procesa, no decide |
| **Shared Memory** | Transporta datos entre procesos | No transforma, no interpreta |
| **TrackFeed** | Mantiene estado vivo de cada pista | No decide prioridades |
| **CoachEngine** | Analiza, prioriza, genera diagnóstico | No ejecuta UI, no genera prompts LLM |
| **LLM** | Interpreta, explica, enseña | No calcula métricas, no mueve faders |
| **UI** | Comunica, acompaña, guía | No analiza audio, no decide mentoría |
| **Usuario** | **Siempre decide** | El Coach sugiere, el usuario aplica |

---

*Documento de arquitectura de plugins — MixCoach — 4 julio 2026*
*Leer antes de modificar cualquier archivo fuera de UI/*. El incumplimiento de estas reglas puede romper la compatibilidad IPC o causar xruns en audio.*

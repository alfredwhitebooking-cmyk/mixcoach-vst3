# 🎚️ MixCoach — Documentación Consolidada

> **Propósito:** Punto de entrada único para cualquier agente IA y desarrollador humano.
> **Consolidado de:** `AI_ONBOARDING.md` + `AI_CONTEXT.md` + `PROJECT_MAP.md` + `CODEX_STATE.md`
> **Versión:** 5.0 | **Última actualización:** 31 mayo 2026

---

### 🧭 Cómo navegar este documento (para IA)

```
AI_CONTEXT.md (ESTE ARCHIVO) → lectura completa recomendada
    ↓
workspace_memory/project_map.md → mapa arquitectónico con diagramas
    ↓
workspace_memory/component_map.md → qué archivo controla qué componente
    ↓
workspace_memory/project_rules.md → reglas de arquitectura detalladas
    ↓
workspace_memory/current_state.md → estado actual y próximos pasos
```

### 🔍 Para encontrar archivos por funcionalidad

| Si buscas... | Ve a... |
|:-------------|:---------|
| Estructuras de datos compartidas | `Common/types/Types.h` |
| Cómo se comunican los plugins | `Common/memory/SlotRegistry.h` + `SharedMemory.h` |
| Análisis FFT/RMS/fase | `Messenger/telemetry/TelemetryCollector.h` + `Common/audio/AudioAnalysis.h` |
| Lógica de mentoría IA | `MixCoach/engine/CoachEngine.h` + `PhaseManager.h` |
| UI de analizadores | `MixCoach/ui/AnalyzersPanelComponent.h` |
| Cómo se inicializa el plugin | `MixCoach/core/PluginProcessor.h` |
| Cómo compilar y validar | `build.ps1` + `scripts/validate.ps1` |
| Tests unitarios | `tests/Test*.cpp` |

---

## 📋 Tabla de Contenidos

1. [⚡ En 30 segundos](#-en-30-segundos)
2. [🎯 Arquitectura del Sistema](#-arquitectura-del-sistema)
3. [📁 Estructura de Directorios](#-estructura-de-directorios)
4. [🔄 Flujo de Datos](#-flujo-de-datos)
5. [🧠 Módulos del Sistema](#-módulos-del-sistema)
6. [🔧 Build System](#-build-system)
7. [📦 Deploy VST3](#-deploy-vst3)
8. [🧪 Tests & Validación](#-tests--validación)
9. [🤖 AI Development Environment](#-ai-development-environment)
10. [🔥 Hotspots & Archivos Peligrosos](#-hotspots--archivos-peligrosos)
11. [📐 Reglas de Edición](#-reglas-de-edición)
12. [🚫 Reglas CRÍTICAS](#-reglas-críticas)
13. [📐 Patrones de Código](#-patrones-de-código)
14. [🔍 Debug & Troubleshooting](#-debug--troubleshooting)
15. [🐛 Historial de Bugs y Errores Conocidos](#-historial-de-bugs-y-errores-conocidos)
16. [🚀 Próximos Pasos](#-próximos-pasos)
17. [📚 Documentación Adicional](#-documentación-adicional)

---

## ⚡ En 30 segundos

**MixCoach** es un sistema de mentoría para mezcla de audio, implementado como **2 plugins VST3** en JUCE 8 (C++20):

| Plugin | Rol | Ubicación |
|--------|-----|-----------|
| **Messenger** (Los Oídos) | Una instancia por pista. Analiza audio en tiempo real (Peak, RMS, FFT, LUFS, fase). CPU ultrabajo. | `Source/Messenger/` |
| **MixCoach** (El Cerebro) | Una instancia en el Master. Recibe datos de los Messengers, ejecuta mentoría, muestra analizadores. | `Source/MixCoach/` |
| **Common** (Compartido) | Código compartido entre ambos plugins: tipos, IPC, memoria compartida, audio primitives. | `Source/Common/` |

**Comunicación:** Windows `CreateFileMappingW` (shared memory) + backup files en `%LOCALAPPDATA%/MixCoach/SlotBackup/` como fallback garantizado.

**Stack:** C++20 · JUCE 8 · Visual Studio 17 2022 · CMake 3.22+ · Windows 10/11

**Entry points:**
- `build.ps1` → **Único** entry point para compilar
- `scripts/validate.ps1` → **Único** entry point para validación completa

---

## 🎯 Arquitectura del Sistema

```
build.ps1 (Entry point ÚNICO para compilar)
│
├── Capa 1: Contexto Inteligente
│   ├── context_selector.py   — 5-pass scoring (intent + keywords + symbols + DSP)
│   └── token_optimizer.py    — Compresión multi-capa con presupuesto de tokens
│
├── Capa 2: Análisis de Proyecto
│   └── project_intelligence.py — Hotspots, dangerous files, subsistemas
│
├── Capa 3: Build & Aprendizaje
│   ├── ai_build_loop.py      — Build loop con auto-repair + symbol mapping
│   ├── ERROR_PATTERNS.json   — Base de errores con auto-learning
│   └── Source/pch.h          — Precompiled Header (JUCE modules)
│
└── Capa 4: Deploy
    └── DeployVST3.ps1        — Deploy a C:\Program Files\Common Files\VST3\
```

### Visión General FL Studio

```
┌─────────────────────────────────────────────────────────────────────┐
│                         FL STUDIO (DAW Host)                        │
│                                                                     │
│  ┌──────────────────────┐          ┌──────────────────────────┐    │
│  │  MESSENGER (xN)      │          │     MIXCOACH (Master)    │    │
│  │  ┌────────────────┐  │  IPC     │  ┌────────────────────┐  │    │
│  │  │ PluginProcessor│──┼──────────┼─▶│ PluginProcessor    │  │    │
│  │  │ + Telemetry    │  │          │  │ + CoachEngine      │  │    │
│  │  │   Collector    │  │◀─────────┼──│ + PhaseManager     │  │    │
│  │  └────────┬───────┘  │  Backup  │  │ + AudioAnalyzer    │  │    │
│  │           │          │  Files   │  └────────┬───────────┘  │    │
│  │  ┌────────▼───────┐  │          │           │              │    │
│  │  │ UI: Editor     │  │          │  ┌────────▼───────────┐  │    │
│  │  │ (name, color,  │  │          │  │ UI: MainTabbedComp │  │    │
│  │  │  bus, VU meter)│  │          │  │  ├─ Chat + List    │  │    │
│  │  └────────────────┘  │          │  │  └─ Analyzers      │  │    │
│  └──────────────────────┘          │  └────────────────────┘  │    │
│                                     └──────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

### Plugins

| Plugin | Rol | Archivos clave |
|--------|-----|:---------------|
| **MixCoach** | Cerebro (canal master) | `Source/MixCoach/core/`, `engine/`, `ui/` |
| **Messenger** | Oídos (por pista) | `Source/Messenger/core/`, `ui/`, `telemetry/` |
| **Common** | Compartido | `Source/Common/types/`, `memory/`, `audio/` |

### Filosofía del Proyecto

- **80/20 rule:** Enfocar en las métricas y fases que más impacto tienen en la mezcla
- **Gamificación:** Fases progresivas + logros para motivar al ingeniero
- **"Engineer + AI" team dynamic:** El sistema mentoriza, no procesa — el ingeniero mantiene el control creativo

---

## 📁 Estructura de Directorios

```
MixCoach/                          ← Raíz del proyecto
├── build/                         ← ✅ ÚNICO directorio de build (CMake output)
│   ├── MixCoach_artefacts/        → MixCoach VST3 + Standalone
│   └── Messenger_artefacts/       → Messenger VST3 + Standalone
│
├── Source/                        ← Código fuente C++20
│   ├── Common/                    → Tipos, IPC, logging (compartido entre plugins)
│   │   ├── types/                 → Types.h, Constants.h, TelemetryData.h, LogHelper.h
│   │   ├── memory/                → SlotRegistry.h, SharedMemory.h, SharedData.h
│   │   └── audio/                 → AudioAnalysis.h
│   ├── MixCoach/                  → Plugin maestro (cerebro)
│   │   ├── core/                  → PluginProcessor.cpp/.h, PluginEditor.cpp/.h
│   │   ├── engine/                → CoachEngine.cpp/.h, PhaseManager.cpp/.h
│   │   ├── audio/                 → AudioAnalyzer.h
│   │   └── ui/                    → MainTabbedComponent, CoachChatComponent,
│   │                                AnalyzersPanelComponent, TrackDashboardComponent,
│   │                                VirtualBusesComponent, ReferencePanelComponent,
│   │                                ProfessionalAnalyzersComponent, MixCoachTheme
│   └── Messenger/                 → Plugin esclavo (por pista)
│       ├── core/                  → PluginProcessor.cpp/.h
│       ├── telemetry/             → TelemetryCollector (DSP: peak, RMS, FFT, LUFS, phase)
│       └── ui/                    → PluginEditor.cpp/.h
│
├── tests/                         → Tests unitarios C++ + Python
│   ├── TestSmoothValue.cpp        → 12 tests (SmoothValue)
│   ├── TestPhaseManager.cpp       → 80 tests (PhaseManager — fases, transiciones, logros)
│   ├── TestCoachEngine.cpp        → 41 tests (CoachEngine — comandos, tips, análisis)
│   ├── TestIPCIntegration.cpp     → 86 tests (IPC pipeline)
│   └── test_ms_calculation.py    → Test Python
│
├── scripts/                       → Scripts auxiliares
│   ├── validate.ps1               → Validación completa (build + tests + deploy)
│   └── deploy_vst3_postbuild.bat  → Post-build step (copia VST3 al sistema)
│
├── workspace_memory/              → Memoria de proyecto para agentes AI
│   ├── project_map.md             → Mapa arquitectónico detallado
│   ├── project_rules.md           → Reglas de desarrollo
│   ├── component_map.md           → Mapa de componentes UI
│   ├── current_state.md           → Estado actual del proyecto
│   └── ui_map.yaml                → Mapa de UI
│
├── Python AI Tools:
│   ├── context_selector.py        → Selección de contexto inteligente
│   ├── token_optimizer.py         → Optimización de tokens
│   ├── project_intelligence.py    → Análisis de proyecto
│   ├── ai_build_loop.py           → Build loop con auto-aprendizaje
│   └── agent_mode.ps1             → Orquestador simplificado
│
├── Data Files:
│   ├── ERROR_PATTERNS.json        → Base de errores (auto-learning)
│   ├── AI_SESSION_STATE.json      → Memoria de sesión
│   ├── EMBEDDING_STORE.json       → Embeddings semánticos
│   ├── PROJECT_GRAPH.json         → Grafo de dependencias
│   ├── PROJECT_INDEX.json         → Índice del proyecto (keywords + intents)
│   ├── SYMBOL_GRAPH.json          → Grafo de símbolos (126 símbolos)
│   └── KNOWN_ERRORS.md            → Errores conocidos documentados
│
├── build.ps1                      → ✅ Entry point ÚNICO para build + deploy
├── DeployVST3.ps1                 → Deploy manual a Common Files
├── CMakeLists.txt                 → Configuración CMake
├── AI_CONTEXT.md                  → ⬅️ ESTE ARCHIVO (documentación consolidada)
├── IPC_CONTRACT.md                → Contrato formal de comunicación Messenger↔MixCoach
├── README.md                      → README del proyecto
└── .gitignore
```

### Directorios obsoletos (NO USAR)

| Directorio | Motivo |
|:-----------|:-------|
| ~~`Builds/`~~ | ❌ Eliminado — duplicado de `build/` |
| ~~`_archive/`~~ | ❌ Código legacy, ignorado por git |
| ~~`__pycache__/`~~ | ❌ Caché Python |

---

## 🔄 Flujo de Datos

### IPC: Messenger → MixCoach

```
Messenger (DLL 1)                 MixCoach (DLL 2)
     │                                 │
     │  registerSlot()                 │
     ├──▶ saveSlotToBackupFile() ──────┤
     │    (escribe .dat en             │
     │     %LOCALAPPDATA%/)            │
     │                                 │
     │  processBlock()                 │  timer (~5s)
     │  ├──▶ updateSharedTelemetry()   │  ├──▶ loadSlotsFromBackupFiles()
     │  │    (shared memory +          │  │    (lee .dat ← GARANTIZADO)
     │  │     backup file)             │  │
     │  │                              │  ├──▶ syncFromShared()
     │  └──▶ SharedMemoryBlock.slots[]─┼──┤    (shared memory ← RÁPIDO)
     │       (CreateFileMapping)       │  │
     │                                 │  └──▶ forceFullSync()
     │                                     │    (ambos, prioriza backup)
```

### Pipeline de Audio del Messenger (por pista)

```
Audio In (Stereo)
    │
    ├──▶ Peak (max sample por canal)
    ├──▶ RMS (root mean square por bloque)
    ├──▶ FFT (512-point Hann window, cada 4 bloques)
    ├──▶ Correlación de fase (sumProduct/sqrt(sumSqL*sumSqR))
    ├──▶ Crest Factor (peak/rms ratio en dB)
    └──▶ LUFS (EBU R128)
         ├── Filtro K-weighting (HP 20Hz + Shelving +4dB @ 1.5kHz)
         ├── Momentary (400ms window)
         ├── Short-term (3s window)
         ├── Integrated (acumulativo)
         └── Loudness Range
              │
              ▼
    SharedMemoryBlock.slots[i]  +  Backup File (.dat)
```

### Arquitectura Threading

```
┌──────────┐    ┌──────────────────┐    ┌─────────────────────┐
│ Message  │    │  Timer (30fps)   │    │  Background Worker  │
│ Thread   │    │  (JUCE Timer)    │    │  (juce::Thread)     │
│          │    │                  │    │                     │
│ UI draws │    │ initSharedData() │    │ forceFullSync()     │
│ events   │    │ buildFullUI()    │    │ loadBackupFiles()   │
│          │    │ updateAnalyzers()│    │ healthCheck()       │
│          │    │ detectNewMsgr()  │    │ syncFromShared()    │
│          │    │                  │    │                     │
└──────────┘    └──────┬───────────┘    └──────────┬──────────┘
                       │                           │
                       └───────────┬───────────────┘
                                   │
                        ┌──────────▼──────────┐
                        │   bgLock_ (Mutex)   │
                        │  SlotRegistry (IPC) │
                        └─────────────────────┘
```

---

## 🧠 Módulos del Sistema

### Módulo 1: Common (Librería Compartida)

**Ubicación:** `Source/Common/` (organizado en `types/`, `memory/`, `audio/`)
**Propósito:** Código compartido entre todos los plugins (MixCoach + Messenger).

| Archivo | Responsabilidad |
|:--------|:----------------|
| `types/Types.h` | Define todas las estructuras de datos compartidas (`TrackTelemetry`, `SlotInfo`, `BusType`, `MentorMessage`, `MentorPhase`, `Achievement`) |
| `types/Constants.h` | Constantes del sistema: `kMaxTracks=64`, `kFFTSize=512`, colores de bus, niveles de referencia |
| `types/TelemetryData.h` | Buffers lock-free: `TelemetryBuffer` (circular 512 slots) y `AudioRingBuffer` (4096 samples) para IPC thread-safe |
| `types/LogHelper.h` | Logger por archivo para diagnóstico. Cada plugin escribe su propio log |
| `memory/SharedData.h` | **Singleton thread-safe** que actúa como puente entre `SlotRegistry` y `SharedMemoryManager` |
| `memory/SharedMemory.h` | IPC entre procesos via Windows `CreateFileMappingW`. Spinlock para acceso atómico |
| `memory/SlotRegistry.h` | **Corazón del IPC.** Registro de slots con modo local + shared. Backup files como mecanismo de comunicación GARANTIZADO |
| `audio/AudioAnalysis.h` | Análisis de audio: FFT (512-point Hann window), RMS, correlación de fase |

### Módulo 2: MixCoach — El Cerebro

**Ubicación:** `Source/MixCoach/` (organizado en `core/`, `engine/`, `audio/`, `ui/`)
**Propósito:** Plugin VST3 en el canal Master. Hub central de mentoría.

| Componente | Responsabilidad |
|:-----------|:----------------|
| **PluginProcessor** (`core/`) | `AudioProcessor` estándar JUCE. Inicialización lazy (no crashear en escaneo VST3). `ensureSharedData()` con retry + backoff exponencial. `ChangeBroadcaster` para notificar UI |
| **PluginEditor** (`core/`) | UI completa con **Background Worker Thread** para I/O pesada (backup files, health check, forceFullSync). Timer a 30fps para actualizaciones. Botón "Re-scan" |
| **AudioAnalyzer** (`audio/`) | Envuelve `AudioAnalysis` para análisis multicanal (master, left, right) |
| **CoachEngine** (`engine/`) | Motor de IA (sistema experto). Analiza por fase: GainStaging, Organización, Balance Tonal, Dinámica, Espacialidad. Comandos: `/next`, `/status`, `/analyze`, `/help`. Tips proactivos. |
| **PhaseManager** (`engine/`) | Gestor de fases progresivas con logros (gamificación). 6 fases: Welcome → GainStaging → Organisation → TonalBalance → Dynamics → Spatial. Cada fase requiere N pistas mínimas. |

**Componentes UI:**

| Componente | Propósito |
|:-----------|:----------|
| `MainTabbedComponent` | Contenedor con 2 tabs: Mentoría + Analizadores |
| `CoachChatComponent` | Chat con IA + lista de Messengers con barras suavizadas |
| `AnalyzersPanelComponent` | Panel profesional: LUFS, VU, espectrograma, vectorscopio, fase, crest histograma, selector de pistas |
| `TrackDashboardComponent` | Vista detallada de pista seleccionada |
| `VirtualBusesComponent` | Gestión de buses virtuales (Drums, Bass, Guitars, Keys, Vocals, FX) |
| `ReferencePanelComponent` | Pistas de referencia para comparación A/B |
| `ProfessionalAnalyzersComponent` | Analizadores avanzados detallados |
| `MixCoachTheme` | Tema visual profesional (colores, gradientes, helpers) |

### Módulo 3: Messenger — Los Oídos

**Ubicación:** `Source/Messenger/` (organizado en `core/`, `ui/`, `telemetry/`)
**Propósito:** Plugin VST3 en pistas individuales. Ultra-bajo CPU (<0.05%).

| Componente | Responsabilidad |
|:-----------|:----------------|
| **PluginProcessor** (`core/`) | `AudioProcessor` ligero. `ensureSlotRegistered()` lazy (no en constructor). Escribe a shared memory y backup files en `processBlock()` |
| **PluginEditor** (`ui/`) | UI compacta: nombre de pista, selector de color, combo de bus, VU meter, waveform |
| **TelemetryCollector** (`telemetry/`) | **DSP completo:** Peak (instantáneo), RMS (por bloque), FFT (512-point), Correlación de fase estéreo, Crest Factor, **LUFS (EBU R128)** con filtrado K-weighting |

---

## 🔧 Build System

### ⚠️ REGLA DE ORO: Usar SIEMPRE `build/` (minúscula)

Hay un ÚNICO directorio de build oficial: **`build/`** (minúscula). NO usar `Builds/` (mayúscula).

### Comandos

```powershell
# BUILD (entry point único):
cd C:\Proyectos\MixCoach
.\build.ps1                    # Compila Release + deploy automático
.\build.ps1 -Clean             # Limpia build anterior y recompila
.\build.ps1 -Debug             # Debug mode
.\build.ps1 -NoDeploy          # Solo compilar sin deploy

# O manualmente:
cd C:\Proyectos\MixCoach\build
cmake --build . --config Release --target MixCoach_VST3 --target MixCoach_Standalone --target Messenger_VST3

# Validación completa:
.\scripts\validate.ps1         # Build + deploy check + tests (11 checks, ~5s)

# Pre-commit hook (automático):
# validate.ps1 se ejecuta automáticamente antes de cada commit.
# Para saltar: git commit --no-verify
```

### Targets de Build

| Target | Tipo | Propósito |
|:-------|:-----|:----------|
| `MixCoach_VST3` | VST3 | Plugin master (mentoría) |
| `Messenger_VST3` | VST3 | Plugin por pista (telemetría) |
| `MixCoach_Standalone` | EXE | Standalone para pruebas fuera de DAW |
| `Messenger_Standalone` | EXE | Standalone Messenger |

### Ubicación de Artefactos

```
build/MixCoach_artefacts/Release/VST3/MixCoach.vst3/
build/Messenger_artefacts/Release/VST3/Messenger.vst3/
build/MixCoach_artefacts/Release/Standalone/MixCoach.exe
```

### ⚠️ Known Build Issues

1. **MSVC C1001**: Ninja + Release crashea en `juce_graphics_Harfbuzz.cpp`. Usar Visual Studio 17 2022 (MSBuild)
2. **Deploy bloqueado**: Si FL Studio tiene los VST3 abiertos, falla el copiado. Cerrar FL Studio primero
3. **PCH desactivado**: JUCE módulos no pueden estar en precompiled headers

---

## 📦 Deploy VST3

### Ruta de destino correcta (la ÚNICA que FL Studio lee)

```
C:\Program Files\Common Files\VST3\
  ├── MixCoach.vst3/
  │   └── Contents/
  │       ├── Resources/moduleinfo.json
  │       └── x86_64-win/MixCoach.vst3   ← DLL real (~4.2 MB)
  └── Messenger.vst3/
      └── Contents/
          ├── Resources/moduleinfo.json
          └── x86_64-win/Messenger.vst3  ← DLL real (~4.1 MB)
```

### ⚠️ ERROR COMÚN: Copiar a C:\Proyectos\Program Files\Common Files\VST3\

Los scripts PowerShell a veces resuelven mal las rutas relativas. **Siempre verificar** que la ruta completa sea exactamente:
```
C:\Program Files\Common Files\VST3\MixCoach.vst3
```
NO:
```
C:\Proyectos\Program Files\Common Files\VST3\MixCoach.vst3  ← ❌ INCORRECTO
```

### Deploy manual

```powershell
cd C:\Proyectos\MixCoach
.\DeployVST3.ps1
```

### Verificar deploy

```powershell
# Comparar build vs deployed
ls "build/MixCoach_artefacts/Release/VST3/MixCoach.vst3/Contents/x86_64-win/MixCoach.vst3"
ls "C:/Program Files/Common Files/VST3/MixCoach.vst3/Contents/x86_64-win/MixCoach.vst3"
# Deben tener el MISMO tamaño y timestamp
```

### Si FL Studio no ve el VST3 después del deploy

1. Cerrar FL Studio completamente
2. Reabrir FL Studio
3. **Options → Manage plugins**
4. Buscar "MixCoach" en la lista
5. Si está tachado/deshabilitado: clic derecho → **Verify installed plugins** (⚡ rayo)
6. Si no aparece: **Rescan → Quick scan**
7. Si sigue sin aparecer: borrar caché en `C:\Users\[user]\AppData\Roaming\FL Studio\Plugins\*`

---

## 🧪 Tests & Validación

### Suite de Tests (219 tests, todos PASS)

| Test | Archivo | Tests | Propósito |
|:-----|:--------|:-----|:----------|
| **SmoothValue** | `tests/TestSmoothValue.cpp` | 12 | Suavizado exponencial de valores |
| **PhaseManager** | `tests/TestPhaseManager.cpp` | 80 | Fases, transiciones, logros, minTracks |
| **CoachEngine** | `tests/TestCoachEngine.cpp` | 41 | Comandos, tips, análisis, clipping |
| **IPC Integration** | `tests/TestIPCIntegration.cpp` | 86 | Pipeline IPC completo |
| **Python** | `tests/test_ms_calculation.py` | 1 | Cálculo de metric tons (Python) |

### Validación completa

```powershell
.\scripts\validate.ps1
```

Ejecuta 11 checks: build VST3 → build tests → verificar artefactos → deploy → 4 tests C++ → 1 test Python → summary. ~5s en total.

### Pre-commit hook

El hook `pre-commit` en `.git/hooks/` ejecuta `validate.ps1` automáticamente. Para saltar: `git commit --no-verify`.

---

## 🤖 AI Development Environment

El proyecto incluye un sistema completo de herramientas Python para desarrollo asistido por IA.

### Pipeline de Herramientas

```
User Query
    ↓
agent_mode.ps1 ───→ context_selector.py (query → archivos)
    │                       ↓
    │               dependency expansion (PROJECT_GRAPH.json)
    │                       ↓
    ├──→ token_optimizer.py (estimar tokens, priorizar)
    ├──→ project_intelligence.py (hotspots, dangerous files)
    ├──→ aider (edición con contexto mínimo)
    ├──→ ai_build_loop.py (build → error analysis → auto-repair)
    └──→ AI_SESSION_STATE.json (memoria persistente)
```

### Herramientas

| Script | Propósito |
|:-------|:----------|
| `context_selector.py` | Smart Context Expansion v2.0: tokenización ES/EN, 3-pass scoring, module-penalty system, dependency expansion via PROJECT_GRAPH.json |
| `token_optimizer.py` | Token estimation (~0.28 tokens/char), priority-aware file ordering, file summarization con metadatos del grafo |
| `project_intelligence.py` | Hotspots detection, dangerous files, critical path analysis, git hotspots, session updater |
| `ai_build_loop.py` | Build feedback loop con error analysis y auto-repair. Timeout 10min. Matching contra ERROR_PATTERNS.json (17 patrones) |
| `agent_mode.ps1` | Orquestador completo: query → build → deploy. Flags: `-Build`, `-Deploy`, `-Repair`, `-Analyze`, `-NoCache` |

### Archivos de Datos

| Archivo | Propósito |
|:--------|:----------|
| `PROJECT_INDEX.json` | Índice del proyecto con módulos, keywords (60+), file_intent_map (20+ queries) |
| `PROJECT_GRAPH.json` | Grafo de dependencias con 47 archivos, critical paths para 4 subsistemas |
| `SYMBOL_GRAPH.json` | Grafo de símbolos (126 símbolos) |
| `ERROR_PATTERNS.json` | 17 patrones: 6 MSVC, 5 Linker, 4 CMake, 2 Runtime |
| `AI_SESSION_STATE.json` | Memoria de sesión persistente (build history, errores, fixes) |
| `EMBEDDING_STORE.json` | Embeddings semánticos para búsqueda |

---

## 🔥 Hotspots & Archivos Peligrosos

### Hotspots del Proyecto (Top 5)

| Hotspot | Crit | UsedBy | Impact | Rol |
|:--------|:-----|:-------|:-------|:----|
| `Source/Common/types/Types.h` | 10 | 18 | 10.4 | foundation |
| `Source/Common/memory/SharedData.h` | 9 | 10 | 8.1 | ipc |
| `Source/Common/memory/SlotRegistry.h` | 10 | 7 | 7.7 | ipc |
| `Source/MixCoach/core/PluginProcessor.h` | 10 | 2 | 7.0 | plugin |
| `Source/Messenger/core/PluginProcessor.h` | 10 | 2 | 6.6 | plugin |

### Archivos Peligrosos de Modificar

| Archivo | Crit | Used By | Riesgo |
|:--------|:-----|:--------|:-------|
| `Source/Common/types/Types.h` | 10/10 | 18 archivos | Modificar afecta a TODO el proyecto |
| `Source/Common/memory/SlotRegistry.h` | 10/10 | 7 archivos | IPC crítico |
| `Source/Common/memory/SharedData.h` | 9/10 | 10 archivos | Bridge IPC central |
| `Source/Common/types/Constants.h` | 9/10 | 6 archivos | Constantes globales |
| `Source/MixCoach/ui/MixCoachTheme.h` | 6/10 | 9 archivos | Tema visual, alto acoplamiento |

### Subsistemas

| Subsistema | Descripción | Archivos Clave |
|:-----------|:------------|:----------------|
| `dsp_pipeline` | FFT → RMS/Peak/LUFS → Spectral → Phase | AudioAnalysis.h, Constants.h, AudioAnalyzer.h |
| `ipc_layer` | Shared memory + backup files + slot registry | SharedMemory.h, SlotRegistry.h, SharedData.h |
| `messenger_sync` | Messenger → MixCoach data pipeline | PluginProcessor.h, TelemetryCollector.h, SlotRegistry.h |
| `analyzers_ui` | UI components for track/audio visualization | AnalyzersPanelComponent.h, ProfessionalAnalyzersComponent.h |

---

## 📐 Reglas de Edición

### Capas de Arquitectura

| Capa | Responsabilidad | Prohibido |
|:-----|:----------------|:-----------|
| **Core/** | Plugin lifecycle (PluginProcessor, Editor) | Lógica de negocio |
| **Engine/** | Mentoría (CoachEngine, PhaseManager) | Dependencias de UI |
| **Audio/** | Análisis de audio (AudioAnalyzer) | Lógica de UI o mentoría |
| **UI/** | Interfaz visual (componentes JUCE) | Lógica de audio |
| **Memory/** | IPC y persistencia (SlotRegistry) | Dependencias de audio o UI |

### Convenciones de Código

```cpp
// Naming
class PascalCase;                    // Clases
void camelCase();                    // Métodos
int memberVariable_;                 // Miembros: suffix _
static constexpr int kConst = 42;   // Constantes: kPrefix
enum class EnumType { Value1 };      // Enums: PascalCase + enum class

// Include order
#include "OwnHeader.h"               // 1. Header propio
#include "../OtherModule.h"           // 2. Módulos del proyecto
#include <juce_foo.h>                 // 3. JUCE
#include <string>                     // 4. STL

// Thread safety
// Audio thread: NO heap allocation
// UI thread: MessageManager::callAsync() para dispatchear
// bgLock_: tryEnter() en timer (no-bloqueante)
```

---

## 🚫 Reglas CRÍTICAS

1. **NUNCA:**
   - Hacer heap allocation en el audio thread (`processBlock()`)
   - Incluir lógica de UI en archivos de engine/audio
   - Incluir lógica de audio en archivos de UI
   - Hardcodear valores de color (usar `MixCoachTheme`)
   - Modificar `BusType` enum sin actualizar también: `busNames[]`, `kBusColourARGB`, `VirtualBusesComponent`, `SlotRegistry`
   - Modificar `SharedSlotEntry` sin incrementar `kCurrentStructVersion`

2. **Siempre:**
   - Leer el archivo ANTES de editarlo
   - Hacer cambios mínimos y enfocados
   - Usar los índices JSON actualizados (`PROJECT_INDEX.json`, `PROJECT_GRAPH.json`) como fuente de verdad para dependencias
   - Buscar usos existentes — si modificas un símbolo exportado, busca y actualiza todas las referencias

3. **⚠️ Advertencias críticas:**
   - **NO usar `do_build.bat`** — usa Ninja (produce VST3s vacíos o crashea C1001)
   - **NO usar `Builds/`** — el directorio oficial es `build/` (minúscula)
   - **Siempre cerrar FL Studio** antes de rebuildear/deployar
   - **Si modificas Types.h o Constants.h**, verifica TODOS los archivos que los incluyen
   - **Ruta de deploy correcta**: `C:\Program Files\Common Files\VST3\`
   - **NO usar rutas relativas** en scripts de deploy

---

## 📐 Patrones de Código

### Patrón 1: Nuevo Componente UI

```cpp
// Header: Source/MixCoach/ui/MiComponente.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

class MiComponente : public juce::Component {
public:
    MiComponente() {
        try {
            addAndMakeVisible(titleLabel_);
            titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            titleLabel_.setText("Mi Componente", juce::dontSendNotification);
        } catch (const std::exception& e) {
            juce::Logger::outputDebugString("[MiComponente] Exception: " + juce::String(e.what()));
        } catch (...) {
            juce::Logger::outputDebugString("[MiComponente] Unknown exception");
        }
    }

    void resized() override {
        auto area = getLocalBounds().reduced(4);
        titleLabel_.setBounds(area.removeFromTop(16));
    }

    void paint(juce::Graphics& g) override {
        MixCoachTheme::fillGlassPanel(g, getLocalBounds().toFloat(), 6.0f);
    }

private:
    juce::Label titleLabel_;
};

} // namespace mixcoach
```

### Patrón 2: Agregar un nuevo método al CoachEngine

```cpp
// En Source/MixCoach/engine/CoachEngine.h
class CoachEngine {
public:
    [[nodiscard]] float analyzeNuevaMetrica(int slotIndex) const;
};

// En Source/MixCoach/engine/CoachEngine.cpp
float CoachEngine::analyzeNuevaMetrica(int slotIndex) const {
    auto& registry = sharedData_.getSlotRegistry();
    auto info = registry.getSlotInfo(slotIndex);
    if (!info.active) return 0.0f;
    // ... lógica de análisis ...
    return resultado;
}
```

### Patrón 3: Leer telemetría de un Messenger

```cpp
void MiComponente::updateTelemetry(SlotRegistry& registry, int slotIndex) {
    if (slotIndex < 0) return;
    try {
        auto& telemetry = registry.getTelemetry(slotIndex);
        auto latest = telemetry.latest();
        peakLeft_ = latest.peakLeft;
        peakRight_ = latest.peakRight;
        rmsLeft_ = latest.rmsLeft;
        rmsRight_ = latest.rmsRight;
        correlation_ = latest.correlation;
        repaint();
    } catch (...) {
        // Fallback silencioso
    }
}
```

### Patrón 4: Agregar un nuevo analyzer en el timer

```cpp
// En Source/MixCoach/core/PluginEditor.cpp
void MixCoachAudioProcessorEditor::timerCallback() {
    tickCounter_++;

    // Operaciones pesadas: ~3fps (cada 10 ticks a 30fps)
    if (tickCounter_ % 10 == 0) {
        registry.syncFromShared();
        detectNewMessengers();
    }

    // Operaciones medias: ~10fps (cada 3 ticks)
    if (tickCounter_ % 3 == 0) {
        updateMessengers();
    }

    // TU NUEVO ANALYZER — agregar aquí según frecuencia necesaria
    if (tickCounter_ % 1 == 0) {
        updateMiAnalyzer();
    }

    // Operaciones ligeras: cada tick
    updateAnalyzers(registry);
    repaint();
}
```

### Patrón 5: Agregar un nuevo bus al sistema

```cpp
// PASO 1: Agregar a Common/types/Types.h
enum class BusType : int {
    None, Drums, Bass, Guitars, Keys, Vocals, FX,
    MiNuevoBus  // <-- agregar AQUÍ (antes de kBusCount)
};

// PASO 2: Agregar nombre y color en Common/types/Constants.h
inline constexpr const char* busNames[] = {
    "None", "Drums", "Bass", "Guitars", "Keys", "Vocals", "FX",
    "Mi Nuevo Bus"
};

// PASO 3: Actualizar VirtualBusesComponent
// PASO 4: Build y verificar switch() sin cubrir
```

---

## 🔍 Debug & Troubleshooting

### Árbol: "Plugin no se ve en FL Studio"

```
¿VST3 existe en C:\Program Files\Common Files\VST3\?
├── NO → .\build.ps1 (compila + deploy automático)
└── SÍ →
    ¿Bundle tiene DLL dentro?
    ├── NO → Build defectuoso (usar MSBuild, NO Ninja)
    └── SÍ →
        ⚡ PRUEBA RÁPIDA: Ejecutar el Standalone
        → .\build\MixCoach_artefacts\Release\Standalone\MixCoach.exe
        ├── ¿Standalone abre y se ve bien?
        │   └── SÍ → Options → Manage plugins → Verify / Rescan
        └── NO → Bug de inicialización
              → Revisar PluginProcessor (lazy init, ensureSharedData)
```

### Árbol: "Build falla"

```
¿LNK2019/LNK2001? → Falta .cpp en CMakeLists.txt target_sources()
¿C2664? → .toRawUTF8() para String→char*, static_cast<size_t>() para int→size_t
¿C2259? → Faltan métodos virtuales JUCE (getName, acceptsMidi, etc.)
¿C1001? → Estás usando Ninja. Usar Visual Studio 17 2022 (MSBuild)
¿MSB3073? → FL Studio abierto. Cerrar FL y rebuildear
```

### Comandos rápidos para debugging

```powershell
# Ver estado del código
cd C:\Proyectos\MixCoach && git status --short
cd C:\Proyectos\MixCoach && git diff HEAD

# Ver builds
ls -la build/MixCoach_artefacts/Release/VST3/MixCoach.vst3/Contents/x86_64-win/
ls -la build/Messenger_artefacts/Release/VST3/Messenger.vst3/Contents/x86_64-win/

# Ver deployed
ls -la "C:/Program Files/Common Files/VST3/MixCoach.vst3/Contents/x86_64-win/"
ls -la "C:/Program Files/Common Files/VST3/Messenger.vst3/Contents/x86_64-win/"

# Buscar en código fuente
grep -rn "funcName" Source/ --include="*.cpp" --include="*.h"
```

---

## 🐛 Historial de Bugs y Errores Conocidos

### Bugs Corregidos (AI Development Environment)

| # | Bug | Archivo | Fix |
|---|-----|:--------|:----|
| 1 | `score_files()` devolvía 0 archivos | `context_selector.py` | `extract_files()` que recorre `modules[].files[]` |
| 2 | `score_by_intent()` no normalizaba acentos | `context_selector.py` | Token-overlap matching con acentos normalizados |
| 3 | `normalize_text()` dead code | `context_selector.py` | Eliminada |
| 4 | Emojis Unicode → `UnicodeEncodeError` | `ai_build_loop.py` | Reemplazados con `[OK] [FAIL] [??]` |
| 5 | `IndentationError` línea 432 | `ai_build_loop.py` | Indentación correcta del cuerpo |
| 6 | `open()` sin `encoding="utf-8"` | 3 archivos Python | `encoding="utf-8"` en todos los `open()` |
| 7 | stderr mezclado con stdout en PS | `agent_mode.ps1` | `2>$null` para stdout, `2>&1 1>$null` separado |

### Errores de Compilación Conocidos

| Error | Causa | Solución |
|:------|:------|:---------|
| **C1001** | Ninja + Release en `juce_graphics_Harfbuzz.cpp` | Usar Visual Studio 17 2022 (MSBuild) |
| **LNK2019/LNK2001** | Falta .cpp en CMakeLists.txt `target_sources()` | Agregar el .cpp al target correspondiente |
| **C2664** | Conversión implícita incorrecta | `.toRawUTF8()` para String→char*, `static_cast<size_t>()` para int→size_t |
| **C2259** | Faltan métodos virtuales JUCE | Implementar `getName()`, `acceptsMidi()`, etc. |
| **MSB3073** | FL Studio abierto bloquea el deploy | Cerrar FL Studio y rebuildear |

---

## 🚀 Próximos Pasos

### Inmediatos (Probar en FL Studio)
1. Cerrar FL Studio completamente
2. Abrir FL Studio, cargar Messenger en pistas, MixCoach en Master
3. Presionar **"↳ Re-scan"** en la pestaña de analizadores
4. Verificar que los grupos/buses se ven correctamente

### Tareas Técnicas Pendientes
1. Probar el sistema de backup files en `%LOCALAPPDATA%\MixCoach\SlotBackup\`
2. Refactorizar `ensureSharedData()` (método extenso)
3. Performance de `forceFullSync()` en sesiones con +16 pistas
4. Roadmap de Mentoría:
   - Fase 0: Setting the Stage ✅
   - Fase 1: Gain Staging (estructura lista, lógica parcial)
   - Fase 2: Balancing (estructura lista, lógica parcial)
   - Fases 3-5: Tonal, Spatial, Mentoring final (estructura, poca lógica real)

### Prioridades por Tipo de Cambio

| Si quieres... | Prioriza este módulo |
|:--------------|:---------------------|
| Mejorar análisis de audio | `Messenger/telemetry/TelemetryCollector` + `Common/audio/AudioAnalysis` |
| Arreglar IPC / sincronización | `Common/memory/SlotRegistry` + `Common/memory/SharedMemory` |
| Cambiar UI / tema visual | `MixCoach/ui/` |
| Mejorar mentoría IA | `MixCoach/engine/CoachEngine` + `MixCoach/engine/PhaseManager` |
| Compilar / debug build | `Build System` (esta sección) |
| Agregar/quitar buses | `Common/types/Types.h` + `Common/types/Constants.h` |
| Mejorar fluidez visual | `CoachChatComponent` (coeficientes smooth) |
| Debug de conectividad | `MixCoach/core/PluginEditor` (health check) + `MixCoach/core/PluginProcessor` (ensureSharedData) |

---

## 📚 Documentación Adicional

| Archivo | Contenido |
|:--------|:----------|
| `IPC_CONTRACT.md` | Contrato formal de comunicación Messenger↔MixCoach |
| `README.md` | README del proyecto: filosofía, roadmap, stack |
| `KNOWN_ERRORS.md` | Historial completo de errores conocidos y fixes |
> **Nota:** Los archivos `PROJECT_MAP.md` y `CODEX_STATE.md` fueron consolidados en este documento (`AI_CONTEXT.md`). `AI_ONBOARDING.md` también fue consolidado aquí. Los 3 archivos ahora son redirects.

| Archivo | Contenido |
|:--------|:----------|
| `workspace_memory/project_map.md` | Mapa arquitectónico detallado con diagramas |
| `workspace_memory/component_map.md` | Mapeo componente → archivo → responsabilidad |
| `workspace_memory/ui_map.yaml` | Mapa de componentes UI |
| `workspace_memory/project_rules.md` | Reglas de arquitectura, naming, dependencias |
| `workspace_memory/current_state.md` | Estado actual, problemas conocidos, próximos pasos |
| `PROJECT_INDEX.json` | Índice de archivos con keywords para navegación IA |
| `PROJECT_GRAPH.json` | Grafo de dependencias entre archivos del proyecto |
| `scripts/validate.ps1` | Script de validación build+deploy+tests |
| `.git/hooks/pre-commit` | Pre-commit hook que ejecuta validate.ps1 |

---

*Documento consolidado a partir de AI_ONBOARDING.md, AI_CONTEXT.md, PROJECT_MAP.md y CODEX_STATE.md — 31 mayo 2026*

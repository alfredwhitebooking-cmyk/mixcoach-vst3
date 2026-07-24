# 📊 08 — ANALYSIS PIPELINE

> **Cómo se analiza el audio en MixCoach, de principio a fin.**
> Describe el pipeline desde que el audio sale del Messenger hasta que el LLM recibe un diagnóstico estructurado.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Implementación:** `Source/MixCoach/audio/`, `Source/MixCoach/engine/TrackFeedCore.cpp`

---

## 📋 Índice

1. [Visión General del Pipeline](#1-vision-general-del-pipeline)
2. [Capa 1: Captura (Messenger)](#2-capa-1-captura-messenger)
3. [Capa 2: Transporte (Shared Memory)](#3-capa-2-transporte-shared-memory)
4. [Capa 3: Sincronización (Background Worker)](#4-capa-3-sincronizacion-background-worker)
5. [Capa 4: TrackFeed — Estado Vivo](#5-capa-4-trackfeed)
6. [Capa 5: Análisis por Pista (Track Intelligence)](#6-capa-5-analisis-por-pista)
7. [Capa 6: Análisis Global (CoachEngine)](#7-capa-6-analisis-global)
8. [Capa 7: Priorización (MixPriorityEngine)](#8-capa-7-priorizacion)
9. [Capa 8: Interpretación (LLM)](#9-capa-8-interpretacion-llm)
10. [Timeline de Análisis](#10-timeline-de-analisis)

---

## 1. Visión General del Pipeline

```
Messenger → Shared Memory → Background Worker → TrackFeed → Track Intelligence → CoachEngine → MixPriorityEngine → LLM → UI
(Pista)     (IPC V6)        (10-30Hz)          (Estado)   (Gain/Dyn/Tonal/Phase) (Análisis)   (Priorización)     (Coach) (Chat)
```

### Flujo de Datos (Simplificado)

```
t=0ms     AudioAnalyzer.processBlock() → FFT, LUFS, fase
t=0-16ms  SmoothValue.advance() → meters animados
t=100ms   Background Worker → RMS/Peak per-track
t=500ms   CoachEngine.periodicAnalysis() → diagnóstico
t=500ms+  AiCoachAdapter → respuesta al chat
t=8000ms  CoachEngine full cycle → actualización completa
```

---

## 2. Capa 1: Captura (Messenger)

**Rol:** Capturar audio RAW + identidad de pista. 100% pas-through.

| Componente | Archivo | Responsabilidad |
|:-----------|:--------|:----------------|
| PluginProcessor | `Messenger/core/PluginProcessor.cpp` | Enviar audio RAW a SharedAudioMemoryV2 |
| Slot heartbeat | `SlotRegistry::setActive()` | Mantener heartbeat cada 100ms |
| UI de identidad | `Messenger/ui/PluginEditor.cpp` | Nombre, color, rol, bus |

**Lo que NO hace:**
- ❌ No calcula FFT, LUFS, RMS, correlación, ni nada de DSP
- ❌ No decide buses ni roles (solo expone lo que el usuario selecciona)
- ❌ No escribe archivos en el audio thread

**Regla crítica:** Messenger es 100% pas-through. Todo análisis ocurre en MixCoach.

---

## 3. Capa 2: Transporte (Shared Memory)

**Rol:** Transportar datos entre procesos de forma segura y lock-free.

### Dos Canales IPC

| Canal | Contenido | Tamaño | Sincronización |
|:------|:----------|:-------|:---------------|
| `SharedMemory V6` | Identidad: 128 slots × (name, color, bus, active) | ~80 bytes/slot | Spinlock two-phase |
| `SharedAudioMemory V2` | Audio RAW estéreo: 128 slots × 4096 samples | ~4.2 MB | Lock-free (volatile + barriers) |

### Reglas IPC

- `SharedSlotEntry` es POD-only. Nada de `std::string`, `std::vector`, punteros.
- Si se modifica `SharedSlotEntry`, INCREMENTAR `kCurrentStructVersion`.
- Los campos nuevos SIEMPRE al final del struct.
- NO reordenar, NO cambiar tipos, NO eliminar campos legacy.

---

## 4. Capa 3: Sincronización (Background Worker)

**Rol:** Leer datos de SharedMemory y actualizar el cache local (SharedData).

| Operación | Frecuencia | Descripción |
|:----------|:-----------|:------------|
| `forceFullSync()` | ~10-30Hz | Lee SharedMemory V6 |
| `readSamples()` | ~10-30Hz | Lee audio RAW de cada Messenger |
| RMS/Peak compute | ~10-30Hz | Computa RMS y Peak real por pista |
| `checkStaleSlots()` | ~10-30Hz | Marca inactivos sin heartbeat |
| Update TrackAudioResult | ~10-30Hz | Actualiza cache en SharedData |

---

## 5. Capa 4: TrackFeed (Estado Vivo)

**Rol:** Mantener estado vivo de cada pista entre ciclos de análisis.

| Componente | Archivo | Responsabilidad |
|:-----------|:--------|:----------------|
| TrackFeedCore | `engine/TrackFeedCore.h/.cpp` | Almacena TrackState por slot |
| TrackState | `engine/TrackState.h` | Health, eventos, crest, band energies |

**Responsabilidades:**
- Almacenar `TrackState` por slot (health, eventos recientes, crest, band energies)
- Generar eventos (gain, dynamics, tonal, phase) en cada ciclo
- Computar health consolidado de cada pista
- Exponer top eventos globales para UI

---

## 6. Capa 5: Análisis por Pista (Track Intelligence)

**Rol:** Analizar cada pista individualmente y generar `TrackAdvice`.

### Los 4 Analizadores

| Analizador | Archivo | Métrica Principal | Thresholds |
|:-----------|:--------|:------------------|:-----------|
| **TrackGainAnalyzer** | `engine/TrackGainAnalyzer.h/.cpp` | RMS, Peak relativo | Headroom: -18dB a -3dB |
| **TrackDynamicsAnalyzer** | `engine/TrackDynamicsAnalyzer.h/.cpp` | Crest factor | 8-14dB saludable |
| **TrackTonalAnalyzer** | `engine/TrackTonalAnalyzer.h/.cpp` | Band energies, spectral tilt | Por género |
| **TrackPhaseAnalyzer** | `engine/TrackPhaseAnalyzer.h/.cpp` | Correlación, ancho estéreo | 0.3 < correlation < 0.8 |

### Output: TrackAdvice

```cpp
struct TrackAdvice {
    int slotIndex;           // Qué pista
    TrackRole role;          // Rol inferido
    TrackDomain domain;      // gain / tonal / dynamics / spatial
    float severity;          // 0.0 - 1.0
    juce::String actionText; // "Subir 2dB en 60Hz"
    juce::String humanMessage; // "El kick pierde cuerpo en el rango fundamental"
    bool isOptimal;          // true si está dentro de target
};
```

---

## 7. Capa 6: Análisis Global (CoachEngine)

**Rol:** Consolidar análisis por pista, generar diagnóstico global, gestionar referencia.

### Subsistemas

| Subsistema | Archivo | Función |
|:-----------|:--------|:--------|
| `periodicAnalysis()` | `CoachEngine_Analysis.cpp` | Ciclo principal cada ~8s |
| `MixScore` | `engine/MixScore.h/.cpp` | Score de salud general (solo interno) |
| `DifferenceProfile` | `engine/DifferenceProfile.h/.cpp` | Comparación mix vs referencia |
| `ReferenceDrivenEngine` | `engine/ReferenceDrivenEngine.h/.cpp` | Modo referencia activo |
| `CorrectionLearner` | `engine/CorrectionLearner.h/.cpp` | Aprende de correcciones del usuario |
| `SessionProgression` | `engine/SessionProgression.h/.cpp` | Máquina de fases |

### Ciclo `periodicAnalysis()` (cada ~8s)

```
1. syncTrackFeedCore()        — Sincronizar estado de pistas
2. analyzeTrackGain()         — Analizar gain de cada pista
3. analyzeTrackDynamics()     — Analizar dinámica de cada pista
4. analyzeTrackTonal()        — Analizar balance tonal de cada pista
5. analyzeTrackPhase()        — Analizar fase/estéreo de cada pista
6. updateMixScore()           — Actualizar score global
7. collectAllIssues()         — Consolidar todos los TrackAdvice
8. MixPriorityEngine.prioritize() — Priorizar issues
9. updateUiAdvice()           — Actualizar UI con nuevos datos
```

---

## 8. Capa 7: Priorización (MixPriorityEngine)

**Rol:** Ordenar issues por severidad × rol × dominio × género.

| Factor | Peso | Descripción |
|:-------|:----:|:------------|
| Severidad | 0.5 | 0.0 - 1.0 (qué tan lejos del target) |
| Rol | 0.25 | Kick y voz tienen más peso que hihats |
| Dominio | 0.15 | Gain tiene prioridad sobre fase |
| Género | 0.10 | Targets específicos por género |

**Output:** Lista ordenada de `TrackAdvice` para el LLM y la UI.

---

## 9. Capa 8: Interpretación (LLM)

**Rol:** Traducir diagnósticos estructurados en coaching conversacional.

Ver documento completo: [`07_AI_BEHAVIOR.md`](./07_AI_BEHAVIOR.md)

---

## 10. Timeline de Análisis

```
t=0ms       Bloque de audio llega al AudioAnalyzer (cada ~2.9ms a 44.1kHz, 128 samples)
            ├── FFT 1024-point Hann (espectro)
            ├── Phase correlation (correlación estéreo)
            ├── Vectorscope data (Lissajous)
            └── Crest factor (pico/RMS)

t=0-16ms    UI se actualiza (60fps)
            ├── SmoothValue.advance() para todos los meters
            ├── Spectrogram FFT bins se copian al hilo de UI
            ├── Vectorscope phosphor trail se actualiza
            └── Phase correlation bar se anima

t=100ms     Background Worker (10-30Hz, en hilo secundario)
            ├── forceFullSync() — lee SharedMemory V6
            ├── readSamples() — lee audio RAW de cada Messenger
            ├── Computa RMS/Peak real por pista
            ├── Actualiza TrackAudioResult en SharedData cache
            └── checkStaleSlots() — marca inactivos sin heartbeat

t=500ms     CoachEngine.periodicAnalysis() (cada ~8s, en message thread)
            ├── syncTrackFeedCore()
            ├── analyzeTrackGain()
            ├── analyzeTrackDynamics()
            ├── analyzeTrackTonal()
            ├── analyzeTrackPhase()
            ├── updateMixScore()
            ├── collectAllIssues()
            └── MixPriorityEngine.prioritize()

t=500ms+    AiCoachAdapter (en message thread, ~500ms-2s)
            ├── buildPrompt() — construye el prompt con datos priorizados
            ├── LLM.generate() — llama al modelo
            └── processResponse() — extrae comandos UI + texto limpio

t=500ms+    UI se actualiza con nueva data
            ├── DashboardScreen → nuevas prioridades
            ├── ChatMessagesComponent → nueva burbuja del Coach
            ├── NavigationShell → auto-switch + auto-return si aplica
            └── PanelRevealManager → revela paneles según keywords
```

### Frecuencias Clave

| Operación | Frecuencia | Thread |
|:----------|:-----------|:-------|
| Audio block processing | Cada bloque (~2.9ms) | Audio thread |
| UI frame | 60fps (~16ms) | Message thread |
| SharedData sync | 10-30Hz (~33-100ms) | Background worker |
| CoachEngine analysis | ~8s | Message thread |
| LLM inference | ~500ms-2s | Message thread |
| Stale heartbeat timeout | 30s sin heartbeat | Background worker |

---

*Documento del pipeline de análisis — MixCoach — 4 julio 2026*
*Todo análisis de audio sigue este pipeline. No hay atajos.*

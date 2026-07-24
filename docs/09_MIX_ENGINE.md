# ⚙️ 09 — MIX ENGINE

> **El motor de mezcla de MixCoach.**
> Define cómo se genera el diagnóstico, cómo se priorizan los issues, cómo se computa el score,
> cómo funciona el reference-driven engine, y cómo aprende de las correcciones del usuario.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Documentos base:** `docs/07_ENGINE/` (CoachEngine.md, MixPriorityEngine.md, PhaseManager.md, ExperienceManager.md)
> **Implementación:** `Source/MixCoach/engine/`

---

## 📋 Índice

1. [Visión General](#1-vision-general)
2. [CoachEngine — El Cerebro](#2-coachengine)
3. [Track Intelligence — Los 4 Analizadores](#3-track-intelligence)
4. [MixPriorityEngine — Priorización](#4-mixpriorityengine)
5. [MixScore — Salud de Mezcla](#5-mixscore)
6. [ReferenceDrivenEngine — Modo Referencia](#6-referencedrivenengine)
7. [DifferenceProfile — Comparación](#7-differenceprofile)
8. [CorrectionLearner — Aprendizaje](#8-correctionlearner)
9. [SessionDirector (Futuro)](#9-sessiondirector-futuro)
10. [Diagrama de Flujo Completo](#10-diagrama-de-flujo-completo)

---

## 1. Visión General

El Mix Engine es el corazón de la inteligencia de MixCoach. Opera en el message thread (~8s) y **nunca** en el audio thread.

```
TrackFeed → Track Intelligence → CoachEngine → MixPriorityEngine → LLM → UI
  (estado)   (Gain/Dyn/Tonal)   (análisis)    (priorización)   (Coach) (Chat)
```

### Subsistemas

| Subsistema | Archivo | Función |
|:-----------|:--------|:--------|
| CoachEngine | `CoachEngine.h/.cpp` | Orquestador principal, ciclo de análisis |
| TrackGainAnalyzer | `engine/TrackGainAnalyzer.h` | Análisis de nivel por pista |
| TrackDynamicsAnalyzer | `engine/TrackDynamicsAnalyzer.h` | Análisis de compresión/crest |
| TrackTonalAnalyzer | `engine/TrackTonalAnalyzer.h` | Análisis de balance tonal |
| TrackPhaseAnalyzer | `engine/TrackPhaseAnalyzer.h` | Análisis de fase/estéreo |
| MixPriorityEngine | `engine/MixPriorityEngine.h/.cpp` | Priorización de issues |
| MixScore | `engine/MixScore.h/.cpp` | Score de salud general |
| DifferenceProfile | `engine/DifferenceProfile.h/.cpp` | Comparación vs referencia |
| ReferenceDrivenEngine | `engine/ReferenceDrivenEngine.h/.cpp` | Modo referencia activo |
| CorrectionLearner | `engine/CorrectionLearner.h/.cpp` | Aprendizaje de correcciones |
| SessionProgression | `engine/SessionProgression.h/.cpp` | Máquina de fases |
| PhaseManager | `engine/PhaseManager.h/.cpp` | Gestión de MentorPhase |
| ExperienceManager | `engine/ExperienceManager.h/.cpp` | Orquestación de UI |
| PanelRevealManager | `engine/PanelRevealManager.h/.cpp` | Revelación por keywords |
| LlmCommandInterpreter | `engine/LlmCommandInterpreter.h/.cpp` | Comandos UI desde LLM |

---

## 2. CoachEngine (El Cerebro)

**Rol:** Analizar, priorizar y generar diagnósticos.

### Ciclo `periodicAnalysis()` (cada ~8s)

```
1. syncTrackFeedCore()            ← Sincronizar estado de pistas
2. analyzeTrackGain()            ← Analizar gain de cada pista
3. analyzeTrackDynamics()        ← Analizar dinámica de cada pista
4. analyzeTrackTonal()           ← Analizar balance tonal
5. analyzeTrackPhase()           ← Analizar fase/estéreo
6. updateMixScore()              ← Actualizar score global
7. collectAllIssues()            ← Consolidar todos los TrackAdvice
8. MixPriorityEngine.prioritize()← Priorizar issues
9. updateUiAdvice()              ← Actualizar UI con nuevos datos
10. processCoachMessages()       ← Generar mensajes si hay cambios
```

### Lo que NO hace

- ❌ No ejecuta la UI
- ❌ No genera prompts LLM directamente (usa `AiCoachAdapter`)
- ❌ No escribe a SharedMemory
- ❌ No procesa audio en tiempo real (solo lee datos cacheados)

---

## 3. Track Intelligence (Los 4 Analizadores)

| Analizador | Archivo | Métrica | Rango Saludable |
|:-----------|:--------|:--------|:----------------|
| **Gain** | `TrackGainAnalyzer.h` | RMS, Peak relativo | -18dB a -3dB |
| **Dynamics** | `TrackDynamicsAnalyzer.h` | Crest factor | 8-14dB |
| **Tonal** | `TrackTonalAnalyzer.h` | Band energies, spectral tilt | Por género |
| **Phase** | `TrackPhaseAnalyzer.h` | Correlación, ancho estéreo | 0.3 < corr < 0.8 |

### Output: TrackAdvice

```cpp
struct TrackAdvice {
    int slotIndex;
    TrackRole role;
    TrackDomain domain;      // gain / tonal / dynamics / spatial
    float severity;          // 0.0 - 1.0
    juce::String actionText;  // "Subir 2dB en 60Hz"
    juce::String humanMessage;// "El kick pierde cuerpo en el rango fundamental"
    bool isOptimal;
};
```

---

## 4. MixPriorityEngine

**Rol:** Ordenar issues por severidad × rol × dominio × género.

### Fórmula de Prioridad

```
priority = severity × 0.5 + roleWeight × 0.25 + domainWeight × 0.15 + genreWeight × 0.10
```

| Factor | Peso | Descripción |
|:-------|:----:|:------------|
| Severidad | 0.5 | 0.0 - 1.0 (qué tan lejos del target) |
| Rol | 0.25 | Kick y voz tienen más peso |
| Dominio | 0.15 | Gain > Tonal > Dynamics > Spatial |
| Género | 0.10 | Targets específicos por género |

---

## 5. MixScore

**Rol:** Score de salud general de la mezcla. **Solo interno, nunca visible al usuario.**

### Dimensiones

| Dimensión | Rango | Descripción |
|:----------|:-----:|:------------|
| Gain | 0-100 | Niveles y headroom |
| Tonal | 0-100 | Balance espectral |
| Dynamics | 0-100 | Crest factor y compresión |
| Spatial | 0-100 | Fase y ancho estéreo |
| Reference | 0-100 | Match con referencia |

### Score General

```
MixScore = (gain * 0.25 + tonal * 0.25 + dynamics * 0.20 + spatial * 0.15 + reference * 0.15)
```

El score es **interno**. El Coach nunca dice "tu score es 72". Dice "tu mezcla está mejorando".

---

## 6. ReferenceDrivenEngine

**Rol:** Modo referencia activo. Compara la mezcla actual contra la referencia cargada.

| Operación | Descripción |
|:----------|:------------|
| `loadReference(path)` | Carga archivo de referencia |
| `analyzeReference()` | Analiza perfil espectral completo |
| `compareMixVsRef()` | Genera DifferenceProfile |
| `getMatchScore()` | Score de similitud (0-100) |

---

## 7. DifferenceProfile

**Rol:** Comparación detallada mix vs referencia por región espectral.

| Región | Rango | Uso |
|:-------|:------|:----|
| Sub | 20-60Hz | Low-end, peso |
| Bass | 60-250Hz | Cuerpo, fundamentales |
| LowMid | 250-500Hz | Calidez, nasalidad |
| HighMid | 500-2kHz | Presencia, claridad |
| Presence | 2-6kHz | Brillo, definición |
| Air | 6-20kHz | Aire, detalle |

---

## 8. CorrectionLearner

**Rol:** Aprende de las correcciones del usuario para ajustar recomendaciones futuras.

| Operación | Descripción |
|:----------|:------------|
| `recordCorrection(advice, applied)` | Guarda si el usuario aplicó o ignoró |
| `getEffectiveness(domain)` | % de correcciones aplicadas por dominio |
| `adjustThreshold(domain, feedback)` | Ajusta thresholds según comportamiento |
| `summarizeLearning()` | Resumen de lo aprendido esta sesión |

---

## 9. SessionDirector (Futuro)

**Rol:** Decidir qué debe pasar ahora. Capa por encima de ExperienceManager.

```
Estado actual:   ExperienceManager reacciona a eventos del engine
Visión futura:   SessionDirector decide qué escena sigue basado en contexto completo

SessionDirector decide:
├── ¿Qué fase sigue?
├── ¿Qué panel revelar?
├── ¿Qué anunciar al usuario?
└── ¿Qué animación lanzar?
```

No implementado aún. Ver `docs/03_SESSION_EXPERIENCE.md` para la visión completa.

---

## 10. Diagrama de Flujo Completo

```
t=0ms     Audio block → FFT, LUFS, phase (audio thread)
t=100ms   Background Worker → RMS/Peak per-track
t=500ms   CoachEngine.periodicAnalysis()
              ├── syncTrackFeedCore()
              ├── analyzeTrackGain()
              ├── analyzeTrackDynamics()
              ├── analyzeTrackTonal()
              ├── analyzeTrackPhase()
              ├── updateMixScore()
              ├── collectAllIssues()
              └── MixPriorityEngine.prioritize()
t=500ms+  AiCoachAdapter
              ├── buildPrompt()
              ├── LLM.generate()
              └── processResponse()
t=500ms+  UI update
              ├── DashboardScreen → new priorities
              ├── ChatMessagesComponent → new Coach message
              └── PanelRevealManager → reveal panels
```

---

*Documento del motor de mezcla — MixCoach — 4 julio 2026*
*Ver `docs/07_ENGINE/` para la especificación detallada de cada subsistema.*

# 🧠 CoachEngine.md

> **Motor de mentoría — El cerebro analítico de MixCoach.**
> Procesa datos de audio, genera diagnóstico, prioriza issues.
>
> **Versión:** 1.0 | **Última actualización:** 3 julio 2026
> **Archivo:** Source/MixCoach/engine/CoachEngine.h/.cpp

---

## 📋 Índice

1. [Propósito](#1-proposito)
2. [Responsabilidades](#2-responsabilidades)
3. [Lo que NO hace](#3-lo-que-no-hace)
4. [Arquitectura](#4-arquitectura)
5. [Dependencias](#5-dependencias)

---

## 1. Propósito

CoachEngine es el motor central de análisis y mentoría. Recibe datos de audio de los Messengers, los analiza contra perfiles esperados, genera diagnóstico estructurado, y alimenta al LLM para que genere recomendaciones en lenguaje natural.

---

## 2. Responsabilidades

- Análisis de audio maestro (FFT, LUFS, correlación, crest)
- Análisis por pista (gain, dynamics, tonal, phase)
- Priorización de issues (MixPriorityEngine)
- Comparación contra referencia (DifferenceProfile)
- Score de mezcla (MixScore)
- Progresión de sesión (SessionProgression)
- Ciclo de corrección (Recommend → Apply → Verify → Feedback)

---

## 3. Lo que NO hace

- ❌ No ejecuta UI
- ❌ No genera prompts LLM directamente
- ❌ No mueve faders
- ❌ No procesa audio (solo analiza)

---

## 4. Arquitectura

```
Messengers → SharedMemory → CoachEngine
                               │
                    ┌──────────┼──────────┐
                    ▼          ▼          ▼
            TrackGain    TrackDynamics   TrackTonal
            Analyzer     Analyzer        Analyzer
                    │          │          │
                    └──────────┼──────────┘
                               ▼
                       MixPriorityEngine
                               │
                               ▼
                       AiCoachAdapter (LLM)
                               │
                               ▼
                            Chat UI
```

---

## 5. Dependencias

| Dependencia | Tipo | Propósito |
|:------------|:-----|:----------|
| PhaseManager | engine | Gestión de fases de mezcla |
| MixPriorityEngine | engine | Priorización de issues |
| ReferenceDrivenEngine | engine | Modo referencia |
| DifferenceProfile | engine | Comparación vs referencia |
| SessionProgression | engine | Progreso de sesión |
| MixScore | engine | Score de mezcla |
| TrackGainAnalyzer | engine | Análisis de gain |
| TrackDynamicsAnalyzer | engine | Análisis dinámico |
| TrackTonalAnalyzer | engine | Análisis tonal |
| AudioAnalyzer | audio | FFT, LUFS, correlación |
| AiCoachAdapter | ai | Conexión con LLM |

---

*Documento del motor de mentoría — MixCoach — 3 julio 2026*

# 🏆 10 — PROGRESS SYSTEM

> **El sistema de progreso y gamificación de MixCoach.**
> Define logros, XP, rachas, milestones y cómo se celebra el aprendizaje del usuario.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Componente UI:** ProgressScreen
> **Fase del roadmap:** Fase 11 (Plan 10/10)

---

## 📋 Índice

1. [Filosofía de Gamificación](#1-filosofia-de-gamificacion)
2. [Categorías de Logros](#2-categorias-de-logros)
3. [Sistema de Rachas (Streaks)](#3-sistema-de-rachas)
4. [Progreso Multi-Sesión](#4-progreso-multi-sesion)
5. [Componente ProgressScreen](#5-componente-progressscreen)
6. [Integración con el Coach](#6-integracion-con-el-coach)

---

## 1. Filosofía de Gamificación

> **La gamificación no es para competir. Es para celebrar el aprendizaje.**

### Reglas de Diseño

1. ❌ **Nunca mostrar scores numéricos visibles** — el usuario no debe sentirse juzgado
2. ✅ **Los logros son cualitativos**, visibles en el chat (vía `postUIEvent`)
3. ✅ **Cada logro es una oportunidad de enseñanza**
4. ✅ **Las rachas recompensan consistencia**, no habilidad

### Principio Rector

El progreso nunca será un espacio independiente. Siempre será un **indicador permanente** en la barra superior.

Debe mostrar:
- Fase actual
- Etapas completadas
- Próxima etapa

El progreso debe generar **motivación.** Nunca ansiedad.

---

## 2. Categorías de Logros

### 🥇 Hitos de Sesión (Progression)

| Logro | Trigger | Mensaje en Chat |
|:------|:--------|:----------------|
| **Primer paso** | Completar Setup | "¡Setup completado! Primer hito desbloqueado." |
| **Mapa explorado** | Confirmar MixMap | "Mapa de sesión confirmado. Tu sesión ya tiene orden." |
| **Primer ajuste** | Aplicar primera corrección | "¡Primera corrección aplicada! El aprendizaje empieza." |
| **Mitad del camino** | Llegar a EQ (fase 4 de 7) | "Mitad del camino en la mezcla. Sigue así." |
| **Cruzando la meta** | Completar MasterCheck | "¡Master Check completado! Has recorrido todo el pipeline." |
| **Maestro del refino** | Entrar a Refinement (MixScore ≥ 70) | "Tu mezcla suena sólida. Hablemos de calidad artística." |
| **Sesión completa** | Generar reporte | "Sesión documentada. Revisa tu progreso en el reporte." |

### 🥇 Dominios Técnicos (Skill Trees)

| Logro | Trigger | Dominio |
|:------|:--------|:--------|
| **Control de ganancia** | 5 correcciones de gain aplicadas | Gain |
| **Ecualizador en mano** | 5 correcciones tonales aplicadas | Tonal |
| **Domador de dinámica** | 5 correcciones de dinámica aplicadas | Dynamics |
| **Arquitecto estéreo** | 5 correcciones espaciales aplicadas | Spatial |
| **Cazador de máscaras** | 5 correcciones de masking aplicadas | Masking |

### 🥇 Rachas (Streaks)

| Logro | Trigger |
|:------|:--------|
| **3 días seguidos** | 3 sesiones en días consecutivos |
| **7 días seguidos** | 7 sesiones en días consecutivos |
| **14 días seguidos** | 14 sesiones en días consecutivos |
| **30 días seguidos** | 30 sesiones en días consecutivos |
| **30 min seguidos** | 30 min sin pausa en una sesión |
| **2 horas seguidas** | 2h sin pausa en una sesión |

### 🥇 Hitos de Aprendizaje

| Logro | Trigger |
|:------|:--------|
| **¿Qué es un crest?** | Usuario pregunta por crest factor |
| **Oído entrenado** | Usuario identifica correctamente un problema antes que el Coach |
| **Sin miedo al HPF** | Usuario aplica su primer HPF |
| **Explorador** | Usuario abre Tools manualmente por primera vez |

---

## 3. Sistema de Rachas (Streaks)

### Persistencia

Requiere guardar fecha de última sesión en `AiCoachAdapter::autoSave()`:

```json
{
  "lastSessionDate": "2026-07-04",
  "currentStreak": 3,
  "longestStreak": 7,
  "totalSessions": 12,
  "milestones": ["Primera mezcla", "Score > 50", "Usó referencia"]
}
```

### Visualización

En ProgressScreen:

```
🔥 3 sesiones seguidas  |  Mejor racha: 7 días  |  Total: 12 sesiones
```

---

## 4. Progreso Multi-Sesión

### Datos Persistidos por Sesión

```json
{
  "engineerName": "Alex",
  "coachMode": "Mix",
  "genre": "Pop",
  "referenceName": "Afrobeat Moderno.wav",
  "sessionDate": "2026-07-04",
  "mixScore": 72,
  "phaseCompleted": "MasterCheck",
  "correctionsApplied": ["Kick_gain_+3dB", "Snare_comp_4:1"],
  "milestones": ["Primera mezcla", "Score > 50", "Usó referencia"]
}
```

### Visualización Histórica

```
📊 Progress History
Día 1  ████████░░░░  42%
Día 3  ████████████░  58%
Día 8  █████████████  89%  ← Hoy
```

---

## 5. Componente ProgressScreen

### Layout

```
┌──────────────────────────────────────────────────────────────┐
│  📈 PROGRESS        🔥 3 sesiones seguidas                  │
├──────────────────────────────────────────────────────────────┤
│  🚀 Session Progression                                      │
│  ● Setup → ● Reference → ● Analysis → ● Coaching →         │
│  🎯 Refine → ○ Report                                        │
├──────────────────────────────────────────────────────────────┤
│  📊 Progress History                                         │
│  Día 1  ████████░░░░  42%                                   │
│  Día 3  ████████████░  58%                                  │
│  Día 8  █████████████  89%  ← Hoy                          │
├──────────────────────────────────────────────────────────────┤
│  🏆 Milestones                                               │
│  [✓] Primera mezcla  [✓] Score > 50                         │
│  [ ] Score > 80      [✓] Usó referencia                     │
└──────────────────────────────────────────────────────────────┘
```

### Estados

| Estado | Descripción |
|:-------|:------------|
| **Locked** | Antes de DeepAnalysis — bloqueado con candado 🔒 |
| **Ready** | Desbloqueado, datos disponibles |
| **In Progress** | Coaching activo, progreso actualizándose |
| **Complete** | Sesión completada, timeline completo |

---

## 6. Integración con el Coach

### Disparadores de Logros

```
PhaseManager::advanceToNextPhase() → AchievementSystem::checkTriggers()
ExperienceManager::celebrate() → AchievementSystem::checkTriggers()
SessionProgression::onPhaseChanged() → AchievementSystem::checkTriggers()
AiCoachAdapter::autoSave() → GamificationSystem::saveProgress()
```

### Visualización en el Chat

Los logros se muestran via `postUIEvent()`:

```
🎉 ¡Primera corrección aplicada! El aprendizaje empieza.
🔥 3 sesiones seguidas — ¡sigue así!
```

### Lo que NO se muestra

- ❌ Scores numéricos visibles
- ❌ Rankings o comparaciones
- ❌ Barras de XP con números
- ❌ Niveles de "usuario principiante/intermedio/avanzado"

Todo es cualitativo. El Coach dice "vas por buen camino", no "tu XP es 450".

---

*Documento del sistema de progreso — MixCoach — 4 julio 2026*
*Fase 11 del Plan 10/10 — Pendiente de implementar.*

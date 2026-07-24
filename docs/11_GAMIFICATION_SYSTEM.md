# 🏆 11 — GAMIFICATION SYSTEM

> **Sistema completo de gamificación para MixCoach.**
> Logros, XP, badges, skill trees, rachas y celebración del aprendizaje.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Fase del roadmap:** Fase 11 (Plan 10/10)
> **Documentos base:** `docs/10_PROGRESS_SYSTEM.md`, `Source/MixCoach/UI/ProgressScreen.h/.cpp`

---

## 📋 Índice

1. [Filosofía](#1-filosofia)
2. [Arquitectura del Sistema](#2-arquitectura-del-sistema)
3. [Logros (Achievements)](#3-logros)
4. [Skill Trees (Árboles de Habilidad)](#4-skill-trees)
5. [XP System](#5-xp-system)
6. [Badges](#6-badges)
7. [Persistencia](#7-persistencia)
8. [Integración con ExperienceManager](#8-integracion-con-experiencemanager)
9. [Visualización en UI](#9-visualizacion-en-ui)
10. [Plan de Implementación](#10-plan-de-implementacion)

---

## 1. Filosofía

### Principios Rectores

| # | Principio | Implicación |
|:-:|:----------|:------------|
| 1 | **La gamificación celebra el aprendizaje, no la competencia** | No hay rankings, no hay comparaciones entre usuarios |
| 2 | **Los números son del motor, no del usuario** | XP es interno; el usuario ve cualidades ("¡Vas avanzando!") |
| 3 | **Cada logro tiene un mensaje del Coach** | No hay notificaciones genéricas; el Coach celebra con personalidad |
| 4 | **Consistencia > Talento** | Las rachas recompensan volver, no tener buen oído |
| 5 | **Nunca castigar** | No hay "rachas perdidas" ni "logros caducados". Solo "sigue así" |
| 6 | **Primero la experiencia, después el badge** | El logro se siente primero (chat), se ve después (UI) |

### Lo que NUNCA se muestra

| ❌ Nunca | ✅ Siempre |
|:---------|:-----------|
| "XP: 450" | "¡Vas por buen camino!" |
| "Nivel 3: Principiante" | "Cada sesión te fortalece" |
| "Racha perdida" | "Vuelve cuando quieras" |
| "Ranking #42" | "Tu progreso es único" |
| "Logro desbloqueado: 75%" | "¡Has logrado algo especial!" |

---

## 2. Arquitectura del Sistema

### Diagrama de Componentes

```
┌──────────────────────────────────────────────────────────────────┐
│                        GAMIFICATION SYSTEM                        │
│                                                                    │
│  ┌─────────────────────┐    ┌────────────────────────────────┐   │
│  │  AchievementSystem   │    │     ProgressScreen (UI)        │   │
│  │  ────────────────    │◄───│  ────────────────              │   │
│  │  • checkTriggers()   │    │  • drawAchievements()          │   │
│  │  • awardAchievement()│    │  • drawBadges()                │   │
│  │  • getUnlocked()     │    │  • drawSkillTree()             │   │
│  │  • getProgress()     │    │  • drawXPMeter()               │   │
│  └──────────┬──────────┘    └────────────────────────────────┘   │
│             │                                                      │
│  ┌──────────▼──────────┐    ┌────────────────────────────────┐   │
│  │   XPSystem           │    │   BadgeRenderer                │   │
│  │  ────────────────    │    │  ────────────────              │   │
│  │  • awardXP()         │    │  • getBadgeIcon()              │   │
│  │  • getLevel()        │    │  • getBadgeColor()             │   │
│  │  • getTotalXP()      │    │  • animateUnlock()             │   │
│  └──────────┬──────────┘    └────────────────────────────────┘   │
│             │                                                      │
│  ┌──────────▼──────────┐                                          │
│  │  GamificationState   │                                          │
│  │  ────────────────    │──→ AiCoachAdapter::autoSave()          │
│  │  • achievements[]    │                                          │
│  │  • totalXP           │                                          │
│  │  • skillTree[]       │                                          │
│  │  • badges[]          │                                          │
│  └─────────────────────┘                                          │
└──────────────────────────────────────────────────────────────────┘

           │
           ▼
┌──────────────────────────────────────────────────────────────────┐
│                        EVENT INPUTS                               │
│                                                                    │
│  ExperienceManager::celebrate()  →  AchievementSystem::onEvent() │
│  SessionProgression::onPhaseChanged()  →  checkTriggers()        │
│  CoachEngine::onCorrectionApplied()  →  checkTriggers()          │
│  MixScore::compute()  →  checkThresholds()                       │
│  AiCoachAdapter::autoSave()  →  GamificationState::save()        │
└──────────────────────────────────────────────────────────────────┘
```

### Flujo de Datos

```
Evento (engine/UI)
    ↓
AchievementSystem::onEvent(eventType, data)
    ↓
├── ¿Cumple criterios de algún logro?
│   ├── Sí → awardAchievement(achievementId)
│   │        ├── ExperienceManager::celebrate("¡Logro! ...")
│   │        ├── XPSystem::awardXP(achievementXP)
│   │        ├── BadgeRenderer::animateUnlock()
│   │        └── GamificationState::save()
│   └── No → solo actualizar progreso
│
├── ¿Cambió skill tree?
│   ├── Sí → actualizar barras de progreso
│   └── No → nada
│
└── ¿Cambió XP threshold?
    ├── Sí → ExperienceManager::celebrate("¡Nuevo nivel!")
    └── No → nada
```

---

## 3. Logros (Achievements)

### Categorías

| Categoría | ID Prefijo | Ejemplos | XP por logro |
|:----------|:-----------|:---------|:-------------|
| 🥇 **Progression** | `prog_` | Hitos de sesión | 50-200 |
| 🛠️ **Technical** | `tech_` | Correcciones por dominio | 25-100 |
| 🔥 **Streaks** | `strk_` | Días/grupo consecutivos | 50-500 |
| 📚 **Learning** | `learn_` | Descubrimientos | 25-75 |
| 🌟 **Mastery** | `mast_` | Logros compuestos | 200-1000 |

### Tabla Completa de Logros

#### 🥇 Progression (Hitos de Sesión)

| ID | Nombre | Trigger | XP | Mensaje del Coach |
|:---|:-------|:--------|:--:|:------------------|
| `prog_first_step` | 🐣 Primer Paso | Setup completado | 50 | "¡Setup completado! Primer hito desbloqueado. Bienvenido a bordo." |
| `prog_map_explored` | 🗺️ Mapa Explorado | MixMap confirmado | 75 | "Mapa de sesión confirmado. Tu sesión ya tiene orden — saber qué tienes es el primer paso para controlarlo." |
| `prog_first_fix` | 🔧 Primer Ajuste | 1ª corrección aplicada | 100 | "¡Primera corrección aplicada! El aprendizaje empieza con acción. Bien hecho." |
| `prog_halfway` | ⛰️ Mitad del Camino | Llegar a EQ (CoachRoomState::EQ) | 150 | "Mitad del camino. Has pasado de organización a acción. Sigue así." |
| `prog_finish_line` | 🏁 Cruzando la Meta | MasterCheck completado | 200 | "¡Master Check completado! Has recorrido todo el pipeline de mezcla. Impresionante." |
| `prog_refinement` | ✨ Maestro del Refino | Entrar a Refinement (MixScore ≥ 70) | 200 | "Tu mezcla suena sólida. Hablemos de calidad artística — esto es lo que separa lo bueno de lo excelente." |
| `prog_session_done` | 📋 Sesión Completa | Reporte generado | 250 | "Sesión documentada. Cada sesión te acerca más a dominar la mezcla." |
| `prog_first_session` | 🌟 Primera Sesión | 1ª sesión completada + guardada | 300 | "¡Tu primera sesión completa! Esto es solo el comienzo." |

#### 🛠️ Technical (Dominios Técnicos)

| ID | Nombre | Trigger | XP | Mensaje |
|:---|:-------|:--------|:--:|:--------|
| `tech_gain_5` | 🎚️ Control de Ganancia | 5 correcciones de gain | 100 | "5 ajustes de ganancia. El volumen es la base de toda buena mezcla." |
| `tech_gain_20` | 🎚️ Maestro del Gain | 20 correcciones de gain | 250 | "Gain staging dominado. Tus tracks respiran en el rango óptimo." |
| `tech_tonal_5` | 🎛️ Ecualizador en Mano | 5 correcciones tonales | 100 | "5 ajustes de EQ. Cada frecuencia que ajustas es un problema que resuelves." |
| `tech_tonal_20` | 🎛️ Cirujano Espectral | 20 correcciones tonales | 250 | "Dominas el espectro. Sabes dónde cortar y dónde realzar." |
| `tech_dyn_5` | 📊 Domador de Dinámica | 5 correcciones de dinámica | 100 | "5 ajustes de compresión. El rango dinámico empieza a ser tuyo." |
| `tech_dyn_20` | 📊 Rey del Crest | 20 correcciones de dinámica | 250 | "Control dinámico profesional. Tus tracks tienen pegada sin perder naturalidad." |
| `tech_spatial_5` | 🌌 Arquitecto Estéreo | 5 correcciones espaciales | 100 | "5 ajustes de imagen estéreo. Tu mezcla empieza a tener profundidad." |
| `tech_spatial_20` | 🌌 Ingeniero de Profundidad | 20 correcciones espaciales | 250 | "Manejas el espacio como un ingeniero senior. Panorama, phase, width — lo tienes." |
| `tech_ref_1` | 🎯 Alineado con Referencia | 1 corrección alineada con referencia | 150 | "Primer ajuste guiado por tu referencia. El norte está marcado." |
| `tech_ref_10` | 🎯 Cazador de Referencia | 10 correcciones alineadas | 400 | "Tu referencia y tu mezcla bailan juntas. Excelente trabajo de alineación." |

#### 🔥 Streaks (Rachas)

| ID | Nombre | Trigger | XP | Mensaje |
|:---|:-------|:--------|:--:|:--------|
| `strk_3_days` | 🔥 3 Días Seguidos | 3 sesiones en ≤36h | 150 | "¡3 sesiones seguidas! La consistencia es el superpoder del ingeniero." |
| `strk_7_days` | 🔥🔥 Semana Completa | 7 sesiones en ≤36h c/u | 350 | "¡Una semana completa! Ya eres parte de la rutina del estudio." |
| `strk_14_days` | 🔥🔥🔥 Dos Semanas | 14 sesiones consecutivas | 500 | "14 días. Esto ya no es racha, es disciplina de ingeniero." |
| `strk_30_days` | 🔥🔥🔥🔥 Mes de Éxito | 30 sesiones consecutivas | 1000 | "¡UN MES! Has usado MixCoach 30 días seguidos. Eres un verdadero ingeniero." |
| `strk_session_30min` | ⏱️ En Foco | 30 min sin pausa en sesión | 50 | "30 minutos seguidos en flow. El estado de concentración es real." |
| `strk_session_2h` | ⏱️⚠️ Flow Profundo | 2h sin pausa | 200 | "2 horas en flow. No te diste cuenta, pero tu mezcla mejoró." |

#### 📚 Learning (Hitos de Aprendizaje)

| ID | Nombre | Trigger | XP | Mensaje |
|:---|:-------|:--------|:--:|:--------|
| `learn_what_crest` | 💡 ¿Qué es un Crest? | Usuario pregunta por crest factor | 50 | "¡Buena pregunta! El crest factor te dice cuánta dinámica tiene un track. Es la diferencia entre aburrido y vibrante." |
| `learn_first_hpf` | 🔉 Sin Miedo al HPF | Usuario aplica su primer HPF | 75 | "¡Primer HPF! Los graves innecesarios ya no ensuciarán tu mezcla." |
| `learn_ear_trained` | 👂 Oído Entrenado | Usuario identifica problema antes que el Coach | 100 | "¡Identificaste el problema antes que yo! Eso es oído de ingeniero." |
| `learn_explorer` | 🧭 Explorador | Usuario abre Tools manualmente 1ª vez | 25 | "Curiosidad activa. Explorar las herramientas es como aprende un ingeniero." |
| `learn_ref_drop` | 📥 Cargó Referencia | Usuario carga 1ª referencia | 75 | "Referencia cargada. Tener un norte es el secreto de las grandes mezclas." |
| `learn_messenger_placed` | 📡 Primer Messenger | 1er Messenger insertado | 50 | "Primer Messenger colocado. Ahora el Coach puede ver tu sesión." |
| `learn_all_messengers` | 📡 Red Completa | Todos los tracks con Messenger | 150 | "Todos los tracks conectados. El Coach tiene visión completa de tu sesión." |

#### 🌟 Mastery (Logros Compuestos)

| ID | Nombre | Trigger | XP | Mensaje |
|:---|:-------|:--------|:--:|:--------|
| `mast_score_80` | 💎 Mezcla Profesional | MixScore ≥ 80 | 300 | "¡MixScore 80+! Tu mezcla suena a nivel profesional. Orgullo." |
| `mast_score_90` | 💎💎 Excelencia Sonora | MixScore ≥ 90 | 500 | "90+ puntos. Esto no es suerte, es talento y técnica combinados." |
| `mast_all_domains_70` | 🏆 Ingeniero Completo | Los 5 dominios ≥ 70 simultáneamente | 600 | "Gain, Tonal, Dynamics, Spatial, Reference — todos sólidos. Eres un ingeniero completo." |
| `mast_all_tech_5` | 🏆 Coleccionista Técnico | Los 4 tech_5 desbloqueados | 400 | "Has trabajado todos los aspectos de la mezcla. Versatilidad es tu nombre." |
| `mast_10_sessions` | 📚 Veterano | 10 sesiones completadas | 500 | "10 sesiones. Has invertido horas en tu oficio. Se nota." |
| `mast_50_sessions` | 📚📚 Legendario | 50 sesiones completadas | 1000 | "50 sesiones. Eres parte del 1% que realmente se toma el tiempo de aprender." |

---

## 4. Skill Trees (Árboles de Habilidad)

### Estructura

Cada dominio técnico tiene un árbol de 4 niveles. El progreso se mide por **correcciones aplicadas** en ese dominio.

```
GAIN STAGING (🎚️)
├── 🌱 Novato (1 corrección)    → "Has comenzado a controlar el gain"
├── 🌿 Aprendiz (5)             → "Sabes identificar problemas de volumen"
├── 🌳 Profesional (15)         → "El gain staging es intuitivo para ti"
└── 🌲 Maestro (30)             → "Nivel de ingeniero senior en gain"

TONAL (🎛️)
├── 🌱 Novato (1)
├── 🌿 Aprendiz (5)
├── 🌳 Profesional (15)
└── 🌲 Maestro (30)

DYNAMICS (📊)
├── 🌱 Novato (1)
├── 🌿 Aprendiz (5)
├── 🌳 Profesional (15)
└── 🌲 Maestro (30)

SPATIAL (🌌)
├── 🌱 Novato (1)
├── 🌿 Aprendiz (5)
├── 🌳 Profesional (15)
└── 🌲 Maestro (30)

REFERENCE (🎯)
├── 🌱 Novato (1)
├── 🌿 Aprendiz (5)
├── 🌳 Profesional (15)
└── 🌲 Maestro (30)
```

### Progreso Acumulativo

```
Skill Progress ─── Gain ───████░░░░ 40%  (12/30)
                  Tonal ──███░░░░░ 30%  (9/30)
                  Dyn ────██████░░ 60%  (18/30)
                  Spat ──██░░░░░░ 20%  (6/30)
                  Ref ───█░░░░░░░ 10%  (3/30)
```

---

## 5. XP System

### Propósito

El XP es **interno** — el usuario nunca ve el número. El XP determina:
1. **Cuándo el Coach celebra un hito** ("Llevas 450XP esta sesión — ¡récord!")
2. **Desbloqueo de mensajes especiales del Coach** (a más XP, más variación en celebraciones)
3. **Indicador de veteranía** (el Coach trata diferente a un usuario con 5000 XP que a uno con 100)

### Cálculo

```
TotalXP = Σ(logros completados) + Σ(bonus por rachas) + Σ(bonum por consistencia)

Donde:
  - Logro común:    25-100 XP
  - Logro intermedio: 150-300 XP
  - Logro maestro:   500-1000 XP
  - Racha 3 días:    +150 XP (bonus único)
  - Racha 7 días:    +350 XP
  - Racha 30 días:   +1000 XP
  - 10 sesiones:     +500 XP
```

### Thresholds de Celebración

| XP Total | El Coach dice... |
|:---------|:-----------------|
| 100 | "Has dado tus primeros pasos como ingeniero." |
| 500 | "Ya tienes experiencia. Tus decisiones lo reflejan." |
| 1000 | "1000 XP. La práctica está dando frutos." |
| 2500 | "Eres un ingeniero en formación sólida." |
| 5000 | "Medio camino a la maestría. Sigue así." |
| 10000 | "10,000 XP. Eres un verdadero ingeniero de mezcla." |

---

## 6. Badges

### Diseño

Cada badge es un **icono circular con color y emoji** que se muestra en:
1. **Chat** — cuando se desbloquea (animación de revelación)
2. **ProgressScreen** — en sección de logros
3. **Dashboard** — en el header como colección compacta

### Tabla de Badges

| Logro | Badge | Color | Forma |
|:------|:------|:------|:------|
| Primer Paso | 🐣 | `#A855F7` (purple) | Huevo |
| Mapa Explorado | 🗺️ | `#3B82F6` (blue) | Brújula |
| Primer Ajuste | 🔧 | `#10B981` (emerald) | Llave |
| Mitad del Camino | ⛰️ | `#F59E0B` (amber) | Montaña |
| Maestro del Refino | ✨ | `#EC4899` (pink) | Estrella |
| Control de Ganancia | 🎚️ | `#8B5CF6` (purple) | Fader |
| 7 Días Seguidos | 🔥🔥 | `#FF6B35` (orange) | Llama doble |
| Mezcla Profesional | 💎 | `#00B7FF` (cyan) | Diamante |
| Ingeniero Completo | 🏆 | `#FFD700` (gold) | Trofeo |

### Animación de Desbloqueo

```
1. Coach dice: "¡Has desbloqueado un nuevo logro!"
2. Badge aparece en el chat (icono + nombre, fade in 300ms)
3. ExperienceManager::celebrate() dispara glow animado
4. Badge aparece en ProgressScreen (sección de logros)
5. El badge queda permanentemente visible
```

---

## 7. Persistencia

### Estructura de Datos

```json
{
  "gamification": {
    "totalXP": 2450,
    "level": "apprentice",
    "achievements": {
      "prog_first_step": {
        "unlocked": true,
        "unlockedAt": "2026-07-04T10:30:00Z",
        "sessionNumber": 1
      },
      "tech_gain_5": {
        "unlocked": true,
        "unlockedAt": "2026-07-04T11:15:00Z",
        "sessionNumber": 1
      },
      "tech_gain_20": {
        "unlocked": false,
        "progress": 12,
        "target": 20
      }
    },
    "skillTrees": {
      "gain":    { "totalCorrections": 12, "level": 2 },
      "tonal":   { "totalCorrections": 8,  "level": 2 },
      "dynamics": { "totalCorrections": 3,  "level": 1 },
      "spatial":  { "totalCorrections": 1,  "level": 1 },
      "reference": { "totalCorrections": 5, "level": 1 }
    },
    "badges": [
      "prog_first_step",
      "prog_map_explored",
      "prog_first_fix",
      "tech_gain_5"
    ],
    "lastUpdated": "2026-07-04T11:20:00Z"
  }
}
```

### Integración con Persistencia Existente

```
AiCoachAdapter::autoSave()
    ↓
GamificationState::save(gamificationData)
    ↓
Se guarda junto con sessionHistory_
    ↓
AiCoachAdapter::loadSessionHistory()
    ↓
GamificationState::load() → ProgressScreen::updateFromGamification()
```

---

## 8. Integración con ExperienceManager

### Disparadores de Logros

```cpp
// ExperienceManager.cpp — secciones existentes que se extienden

void ExperienceManager::onCoachingStageChanged(oldStage, newStage) {
    // ... lógica existente de UI ...
    
    // 🆕 Nuevo: verificar logros de progresión
    if (newStage == CoachingStage::EQ)
        achievementSystem_.checkTrigger("prog_halfway");
}

void ExperienceManager::celebrate(const juce::String& text) {
    // ... lógica existente de animación ...
    
    // 🆕 Nuevo: verificar si hay logro asociado
    if (achievementSystem_.hasAchievementFor(text)) {
        auto& ach = achievementSystem_.getAchievementFor(text);
        if (!ach.unlocked) {
            achievementSystem_.awardAchievement(ach.id);
        }
    }
}
```

### Nuevos Callbacks en CoachEngine

```cpp
// CoachEngine — nuevos callbacks para gamificación

void CoachEngine::onCorrectionApplied(const TrackRecommendation& rec) {
    // ... lógica existente ...
    
    // 🆕 Nuevo: actualizar skill trees
    gamificationSystem_.onCorrectionApplied(rec.domain);
    
    // Verificar logros técnicos
    int count = gamificationSystem_.getDomainCorrectionCount(rec.domain);
    if (count == 5)  achievementSystem_.checkTrigger("tech_" + domainPrefix + "_5");
    if (count == 20) achievementSystem_.checkTrigger("tech_" + domainPrefix + "_20");
}
```

---

## 9. Visualización en UI

### ProgressScreen — Sección de Logros (nueva)

```
┌──────────────────────────────────────────────────────────────┐
│  🏆 LOGROS                                                     │
│                                                                │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐                          │
│  │ 🐣 │ │ 🗺️ │ │ 🔧 │ │ 🎚️ │ │ ⛰️ │  ← Badges desbloqueados │
│  │XP50│ │XP75│ │XP100│ │XP100│ │XP150│                          │
│  └────┘ └────┘ └────┘ └────┘ └────┘                          │
│                                                                │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐                                  │
│  │ 🔒 │ │ 🔒 │ │ 🔒 │ │ 🔒 │  ← Badges bloqueados             │
│  │🎚️20│ │📊5 │ │📊20│ │💎80│  (con progreso)                 │
│  └────┘ └────┘ └────┘ └────┘                                  │
│  🎚️ Gain Master: ████████░░ 12/20                             │
└──────────────────────────────────────────────────────────────┘
```

### Chat — Mensaje de Logro

```
┌──────────────────────────────────────────────────┐
│ 🤖 Coach                                          │
│                                                   │
│ 🎉 ¡Logro desbloqueado!                          │
│                                                   │
│ ┌──────────────────────────────────────┐         │
│ │        🔧                             │         │
│ │   PRIMER AJUSTE                      │         │
│ │   Has aplicado tu primera            │         │
│ │   corrección de mezcla.              │         │
│ │   ¡El aprendizaje empieza!           │         │
│ └──────────────────────────────────────┘         │
│                                                   │
│ ▶ Siguiente: 5 ajustes → "Control de Ganancia"  │
└──────────────────────────────────────────────────┘
```

### Dashboard — Colección Compacta

```
┌──────────────────────────────────────────────────┐
│  🏆 12 logros  |  🔥 3 racha  |  ✨ 2450 XP      │
│  [🐣][🗺️][🔧][🎚️][📊][⛰️][💎] +5 más...        │
└──────────────────────────────────────────────────┘
```

---

## 10. Plan de Implementación

### Fases

| Fase | Archivos | Esfuerzo | Dependencias |
|:-----|:---------|:--------:|:-------------|
| **1. AchievementSystem** | `Source/MixCoach/engine/AchievementSystem.h/.cpp` | 🟡 Media | Ninguna |
| **2. XPSystem** | `Source/MixCoach/engine/XPSystem.h/.cpp` | 🟢 Baja | AchievementSystem |
| **3. GamificationState** | `Source/MixCoach/engine/GamificationState.h/.cpp` | 🟡 Media | AchievementSystem, XPSystem |
| **4. Integración CoachEngine** | `CoachEngine.h/.cpp` (nuevos callbacks) | 🟢 Baja | AchievementSystem |
| **5. Integración ExperienceManager** | `ExperienceManager.cpp` (disparadores) | 🟢 Baja | AchievementSystem |
| **6. Actualizar ProgressScreen** | `ProgressScreen.h/.cpp` (badges, logros, skill trees) | 🔴 Alta | AchievementSystem, GamificationState |
| **7. Actualizar DashboardScreen** | `DashboardScreen.h/.cpp` (colección compacta) | 🟢 Baja | GamificationState |
| **8. Tests** | `tests/TestAchievementSystem.cpp` | 🟡 Media | AchievementSystem |

### Orden Recomendado

```
Semana 1:  AchievementSystem + tests        (Fase 1)
Semana 2:  XPSystem + GamificationState     (Fases 2-3)
Semana 3:  Integración engine + manager     (Fases 4-5)
Semana 4:  UI (ProgressScreen + Dashboard)  (Fases 6-7)
```

---

*Documento de diseño de gamificación — MixCoach — 4 julio 2026*
*Fase 11 del Plan 10/10 — Diseño completo listo para implementar.*

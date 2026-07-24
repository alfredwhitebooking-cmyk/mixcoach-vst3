# 💾 12 — MEMORY SYSTEM

> **Qué recuerda el Coach entre sesiones y cómo lo recuerda.**
> Define la persistencia de datos, el historial de sesiones, y cómo el Coach retoma el contexto.
>
> **Versión:** 1.0 | **Última actualización:** 4 julio 2026
> **Implementación:** `Source/MixCoach/ai/AiCoachAdapterSession.cpp`

---

## 📋 Índice

1. [Filosofía de Memoria](#1-filosofia-de-memoria)
2. [Datos que se Persisten](#2-datos-que-se-persisten)
3. [Datos que NO se Persisten](#3-datos-que-no-se-persisten)
4. [Flujo de Persistencia](#4-flujo-de-persistencia)
5. [Reapertura de Sesión](#5-reapertura-de-sesion)
6. [Estructura de Datos](#6-estructura-de-datos)
7. [Privacidad](#7-privacidad)

---

## 1. Filosofía de Memoria

> **El Coach recuerda quién eres y qué has aprendido. No recuerda cada palabra que dijiste.**

### Principios

1. **El Coach recuerda al usuario, no la conversación.** El historial de chat se resetea cada sesión.
2. **La memoria es privada.** Todo se almacena localmente. Nunca en la nube.
3. **La memoria es ligera.** Solo datos esenciales. No logs de audio.
4. **El usuario puede borrar su memoria.** No hay vendor lock-in emocional.

---

## 2. Datos que se Persisten

| Dato | Dónde se guarda | Uso |
|:-----|:----------------|:----|
| Nombre del usuario | Archivo JSON en AppData | Saludo personalizado |
| Modo preferido (Mix/Master) | Archivo JSON | Default en nueva sesión |
| Género favorito | Archivo JSON | Sugerencia rápida |
| Última referencia usada | Archivo JSON | Re-carga rápida |
| Historial de scores | Archivo JSON | Gráfico de progreso |
| Rachas (streaks) | Archivo JSON | Indicador motivacional |
| Milestones desbloqueados | Archivo JSON | Display en Progress |
| Nivel de experiencia | Archivo JSON | Adaptación de tono |

---

## 3. Datos que NO se Persisten

| Dato | Razón |
|:-----|:------|
| Audio de la sesión | Privacidad, espacio |
| Historial de chat completo | Contexto irrelevante entre sesiones |
| Configuración de la UI | Se resetea a default |
| Correcciones no aplicadas | No tienen sentido sin la sesión |
| Estado de paneles revelados | Se revelan de nuevo |

---

## 4. Flujo de Persistencia

### Al Finalizar la Sesión

```
Usuario cierra sesión o genera reporte
  → AiCoachAdapter::autoSave()
    → Construye SessionRecord con datos actuales
    → Escribe a archivo JSON en AppData
    → Actualiza streaks (fecha última sesión)
```

### Archivo de Memoria

```
Windows: %APPDATA%/MixCoach/memory/sessions.json
         %APPDATA%/MixCoach/memory/user_prefs.json
```

---

## 5. Reapertura de Sesión

### Flujo de Reapertura

```
Usuario abre MixCoach
  → AiCoachAdapter::loadSessionHistory()
    → Lee archivo JSON
    → Si hay sesiones previas:
      → Coach saluda por nombre
      → Muestra progreso histórico en Dashboard
      → Ofrece: "¿Quieres retomar donde lo dejaste?"
    → Si no hay sesiones previas:
      → Coach da bienvenida como nuevo usuario
```

### Coach Dice (Usuario Recurrente)

> *"¡Bienvenido de vuelta, Alex! Han pasado 3 días desde tu última sesión. Tu progreso general: 42% → 72%. ¿Quieres retomar con una nueva mezcla?"*

### Coach Dice (Usuario Nuevo)

> *"¡Hola! Soy MixCoach, tu ingeniero de mezcla. ¿Cómo te llamas?"*

---

## 6. Estructura de Datos

### SessionRecord

```json
{
  "engineerName": "Alex",
  "coachMode": "Mix",
  "genre": "Reggaeton",
  "referenceName": "Titi Me Pregunto.wav",
  "sessionDate": "2026-07-04",
  "durationMinutes": 47,
  "mixScore": 72,
  "scoreBreakdown": {
    "gain": 78,
    "tonal": 65,
    "dynamics": 81,
    "spatial": 70,
    "reference": 58
  },
  "phaseCompleted": "MasterCheck",
  "correctionsApplied": [
    "Kick_gain_+3dB",
    "Snare_comp_4:1",
    "808_HPF_80Hz",
    "Voz_EQ_-2dB_3kHz"
  ],
  "milestones": ["Primera mezcla", "Score > 50", "Usó referencia"]
}
```

### UserPreferences

```json
{
  "engineerName": "Alex",
  "preferredMode": "Mix",
  "experienceLevel": "intermediate",
  "favoriteGenres": ["Reggaeton", "Pop"],
  "lastSessionDate": "2026-07-04",
  "currentStreak": 3,
  "longestStreak": 7,
  "totalSessions": 12,
  "milestonesUnlocked": [
    "primera_mezcla",
    "score_50",
    "uso_referencia",
    "primer_ajuste"
  ]
}
```

---

## 7. Privacidad

### Reglas

1. **100% offline.** No hay servidores. No hay nube. No hay telemetría.
2. **Datos locales.** Todo se guarda en `%APPDATA%/MixCoach/`.
3. **Sin audio.** Nunca se persiste audio del usuario.
4. **Borrable.** El usuario puede borrar `sessions.json` y empezar de cero.
5. **Transparente.** El Coach nunca oculta qué datos recuerda.

---

*Documento del sistema de memoria — MixCoach — 4 julio 2026*
*Pendiente de implementar completamente en AiCoachAdapterSession.cpp.*

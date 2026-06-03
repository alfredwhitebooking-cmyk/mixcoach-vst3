# 🎯 PRODUCT_VISION.md — La Visión de MixCoach

> **El alma del proyecto. Léeme primero para entender QUÉ es MixCoach, QUÉ NO es, y HACIA DÓNDE va.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## ⚡ La Frase que lo Define

> **MixCoach no es un procesador de audio. Es un Mentor Profesional que te guía a través del proceso de mezcla, convirtiéndote en un ingeniero de audio más capaz, organizado y seguro de tus decisiones.**

Esta frase NO es decorativa. Es la **estrella del norte** del proyecto. Cada decisión de diseño, cada funcionalidad, cada línea de código debe responder: *"¿Esto acerca al usuario a ser un mejor ingeniero?"*

---

## 🧠 Los 3 Pilares del Producto

### 1. Mentor, no Juez 🧑‍🏫

| El Mentor... | El Juez... |
|-------------|------------|
| Sugiere, guía, enseña | Critica, califica, señala |
| Dice "prueba a subir 1dB en 60Hz" | Dice "tu mezcla está mal" |
| Explica POR QUÉ | Solo dice QUÉ está mal |
| Se adapta al nivel del ingeniero | Aplica el mismo estándar a todos |
| Celebra el progreso (logros) | Solo señala errores |

**En código:** Los mensajes de `CoachEngine` son TIPS y sugerencias, no juicios. El tono es profesional pero alentador. Los warnings existen (clipping, fase negativa) pero siempre con solución.

### 2. Enfoque 80/20 🎯

> El 80% del resultado viene del 20% de las acciones. MixCoach se enfoca en las métricas y fases que más impacto tienen en la mezcla.

**¿Qué significa en la práctica?**

- No necesitas un analizador de 2048 bandas — con 512 bins FFT es suficiente
- No necesitas 50 métricas — con Peak, RMS, LUFS, correlación y crest factor cubres el 90% de los problemas
- Las fases de mentoría cubren el flujo completo de mezcla, pero sin profundidad excesiva en cada una

**¿Qué NO haremos?**
- ❌ Analizador espectral de precisión de laboratorio
- ❌ Medición de distorsión armónica (THD)
- ❌ Análisis de fase en frecuencia (solo correlación global)
- ❌ Soporte para Surround / Dolby Atmos

### 3. Relación de Equipo: "Ingeniero + MixCoach" 🤝

```
TÚ (Ingeniero)          MixCoach (Mentor)
     │                        │
     │  Decide creativamente  │
     │───────────────────────▶│
     │                        │  Analiza, sugiere, alerta
     │                        │───────────────────────▶
     │◀───────────────────────│
     │  Toma la decisión final│
     │                        │
     ▼                        ▼
     ──── MEZCLA MEJOR ────
```

**MixCoach NUNCA toca el audio.** El ingeniero mantiene el control creativo total. MixCoach solo provee criterio, datos y consejo.

---

## 🎨 Identidad Visual

### Look & Feel

```
FONDOS:       Negro profundo (#0A0A0F) con paneles sutilmente elevados
MARCA:        Violeta (#7C3AED / #8B5CF6)
IA/ESPECTRO:  Cyan (#00B4D8 / #00E5FF)
TIPOGRAFÍA:   Sans-serif moderna, ALL CAPS para headers de sección
ESTILO:       Profesional, oscuro, denso (como iZotope Ozone / IK Multimedia)
```

**Referencias visuales (fuente de verdad):**
- `UI_REFERENCES/Messenger.png` — UI del plugin por pista
- `UI_REFERENCES/MixCoach_Tab1_AICoach.png` — Pestaña de chat + pistas
- `UI_REFERENCES/MixCoach_Tab2_Analyzers.png` — Pestaña de analizadores

### Paleta de Colores de Bus (inviolable)

| Bus | Color | Código |
|-----|:-----:|:------:|
| Drums | 🟣 Violeta | `#8B5CF6` |
| Bass | 🔵 Azul | `#3B82F6` |
| Guitars | 🟠 Naranja | `#F97316` |
| Keys/Synths | 🟢 Verde-teal | `#10B981` |
| Vocals | 🩷 Rosa | `#EC4899` |
| FX | 🫀 Teal | `#14B8A6` |

**Regla:** Estos colores NUNCA se hardcodean. Siempre se usan via `MixCoachTheme::busColour()` + `Constants.h::kBusColourARGB`.

---

## 🏗️ Arquitectura Conceptual

```
┌──────────────────────────────────────────────────────────────┐
│                      FL STUDIO (DAW)                         │
│                                                              │
│  PISTA 1          PISTA 2  ...  PISTA N         MASTER      │
│  ┌────────┐       ┌────────┐     ┌────────┐    ┌────────┐  │
│  │MESSENGER│      │MESSENGER│     │MESSENGER│   │MIXCOACH│  │
│  │ (oídos) │      │ (oídos) │     │ (oídos) │   │(cerebro)│  │
│  │ CPU <0.05│     │ CPU <0.05│    │ CPU <0.05│   │         │  │
│  └────┬───┘       └────┬───┘     └────┬───┘    └────────┘  │
│       │                │              │           ▲         │
│       └────────────────┴──────────────┘───────────┘         │
│                        │  IPC (SHM + backup)                │
│                        ▼                                    │
│                   DATOS EN TIEMPO REAL                       │
│              (Peak, RMS, FFT, LUFS, fase)                   │
└──────────────────────────────────────────────────────────────┘
```

### Los Dos Plugins

| Plugin | Metáfora | Rol | CPU Objetivo |
|--------|----------|-----|:------------:|
| **Messenger** | 👂 Los Oídos | Uno por pista. Escucha, mide, envía telemetría. NO toca el audio. | < 0.05% |
| **MixCoach** | 🧠 El Cerebro | Uno en el Master. Recibe datos, ejecuta mentoría, muestra analizadores. | < 2% (con 60+ tracks) |

### El Flujo de Datos

```
Messenger en cada pista:
  1. Audio entra → NO se modifica (passthrough)
  2. TelemetryCollector analiza: Peak, RMS, FFT, correlación, LUFS
  3. Datos viajan a MixCoach por:
     a) Shared Memory (CreateFileMappingW) → RÁPIDO (~1μs)
     b) Backup Files (%LOCALAPPDATA%) → GARANTIZADO (~5ms)

MixCoach en el Master:
  4. SlotRegistry recibe datos de N Messengers
  5. CoachEngine analiza según la fase activa
  6. UI muestra analizadores + chat con sugerencias
```

---

## 🗺️ Roadmap de Mentoría (Las 6 Fases)

```
FASE 0: WELCOME 🎉
    ● Mensaje de bienvenida
    ● Detección de pistas
    ● Setup de género musical
    → Avance: automático al detectar primera pista

FASE 1: GAIN STAGING 📊
    ● Detección de clipping (peak > -0.5dB)
    ● Headroom (-6dB target)
    ● Señal baja (peak < -30dB)
    ● Crest factor analysis
    → Objetivo: sin clipping, headroom saludable

FASE 2: ORGANISATION 📝
    ● Nombrar pistas descriptivamente
    ● Asignar colores por familia
    ● Agrupar en buses virtuales
    → Objetivo: mezcla organizada visualmente

FASE 3: TONAL BALANCE 🎛️
    ● Análisis espectral por bandas
    ● Detección de 6 desbalances (subs, graves, presencia, etc.)
    ● Enmascaramiento espectral entre pares
    → Objetivo: espectro balanceado

FASE 4: DYNAMICS ⚡
    ● Crest factor por pista
    ● Compresión excesiva vs insuficiente
    ● LUFS y loudness range
    → Objetivo: dinámica controlada

FASE 5: SPATIAL 🌌
    ● Correlación de fase
    ● Fase negativa
    ● Compatibilidad mono
    → Objetivo: mezcla con profundidad y ancho
```

**Filosofía de fases:** El usuario AVANZA cuando QUIERE (comando `/next`). No hay obligación. La gamificación (logros) motiva pero no fuerza.

---

## ✅ QUÉ ES MixCoach (Sí)

- ✅ Un mentor que analiza tu mezcla y da sugerencias
- ✅ Un sistema de analizadores visuales profesionales
- ✅ Un organizador de pistas (nombres, colores, buses virtuales)
- ✅ Un detector de problemas (clipping, fase, enmascaramiento)
- ✅ Un sistema de gamificación que motiva el aprendizaje
- ✅ Un plugin VST3 para Windows (FL Studio y DAWs compatibles)
- ✅ Una herramienta que funciona con 1 pista o con 100+

## ❌ QUÉ NO ES MixCoach (Nunca)

- ❌ **NO** es un procesador de audio (no ecualiza, no comprime, no limita)
- ❌ **NO** es un sustituto de oído entrenado (es una herramienta más)
- ❌ **NO** es iZotope Neutron / Ozone (no tiene procesamiento AI de audio)
- ❌ **NO** es FabFilter Pro-Q / Pro-C (no reemplaza tus plugins favoritos)
- ❌ **NO** es un medidor de laboratorio (precisiones redondeadas son aceptables)
- ❌ **NO** es multi-plataforma (solo Windows VST3 por ahora)
- ❌ **NO** funciona como standalone sin DAW
- ❌ **NO** altera el audio en ninguna circunstancia

---

## 👤 El Usuario

MixCoach está diseñado para:

| Perfil | ¿Qué valora? | MixCoach le ayuda a... |
|--------|-------------|----------------------|
| **Ingeniero principiante** | Aprender el proceso de mezcla | Entender el flujo completo, evitar errores comunes |
| **Ingeniero intermedio** | Validar decisiones, ahorrar tiempo | Detectar problemas rápido, mantener consistencia |
| **Ingeniero avanzado** | Segunda opinión, objetividad | Ver lo que el oído ya no escucha por fatiga |

**Lo que NO somos:** Para productores que solo quieren "masterizar" sin aprender. MixCoach requiere que el ingeniero participe activamente.

---

## 🧭 Hacia Dónde Vamos (Visión a Futuro)

### Corto plazo (próximos meses)
```
● Estabilidad con 100+ Messengers ✅ (logrado)
● UI alineada con imágenes de referencia
● CoachEngine con mensajes más contextuales y variados
● Exportación de informes de mezcla
● /commands avanzados (historial, estadísticas, export)
```

### Mediano plazo
```
● Soporte para arrastrar/soltar archivos de referencia
● Comparación A/B con pistas de referencia
● Análisis de espectro por bus
● Detección de enmascaramiento mejorada
```

### Largo plazo (visión)
```
● Perfiles de usuario que recuerdan preferencias
● Modo "rehearsal" donde MixCoach analiza sin el artista presente
● Integración con servicios de referencia (Spotify, etc.)
● Versión macOS (cuando JUCE lo permita)
```

---

## 🔗 Referencias Cruzadas

| Documento | Relación |
|-----------|----------|
| `HOW_TO_WORK_ON_THIS_PROJECT.md` | Todo cambio debe preservar la visión del producto |
| `APPROVED_PATTERNS.md` | PX3: Prohibido lógica de audio en UI (MixCoach solo analiza) |
| `DEFINITION_OF_DONE.md` | Criterio 1: "Preserva el objetivo del producto: mentor/analyzer, no audio processor" |
| `USER_PERSONA.md` | Detalla el perfil del usuario final |
| `AGENTS.md` | Entrada rápida que referencia esta visión |

---

*Documento de visión de producto — MixCoach Project*

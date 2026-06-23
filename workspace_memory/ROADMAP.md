# 🗺️ ROADMAP — MixCoach Product Sprint Plan

> **LEER PRIMERO.** Este documento define el norte del producto y los sprints completados/activos.
> Cualquier feature, UI o cambio debe contrastarse contra este roadmap.
>
> **Última actualización:** 22 junio 2026
> **Versión:** 2.0

---

## 📋 Índice

1. [🎯 North Star](#-north-star)
2. [📊 Evaluación Actual](#-evaluación-actual)
3. [✅ Sprints Completados](#-sprints-completados)
4. [🏃 Próximo Sprint](#-próximo-sprint)
5. [📌 Prioridad Siguiente Sesión](#-prioridad-siguiente-sesión)
6. [❌ Lo que NO construir](#-lo-que-no-construir)

---

## 🎯 North Star

> **MixCoach es un ingeniero de mezcla sentado a tu lado.**
> No procesa audio. No juzga. Enseña, sugiere y verifica.

### Las 3 piezas que transforman el producto

| # | Pieza | Estado |
|:-:|:------|:------:|
| 1 | **Identidad de las pistas** — el cerebro sabe qué es cada pista | ✅ Completado (UI de confirmación + badge 🎯) |
| 2 | **Análisis individual por pista** — el cerebro entiende cada pista | ✅ Pipeline DSP completo + Crest + FFT + Stereo Width visibles |
| 3 | **Referencia como norte absoluto** — cada recomendación se mide contra la referencia | ✅ Mensajes cualitativos automáticos + LLM path |

---

## 📊 Evaluación Actual

| Dimensión | Score | Nota |
|:----------|:-----:|:------|
| **Arquitectura** | 9/10 | IPC, separación Sensor-Cerebro, flujo de datos ✅ |
| **Producto** | 8/10 | Sprints 1-5 completados, TrackFeed vivo conectado |
| **Visión** | 10/10 | Clara, diferenciada, sin competencia directa |
| **Potencial comercial** | 9/10 | Nicho claro, problema real |

---

## ✅ Sprints Completados

### Sprint 1 — Identity Layer ✅ COMPLETED

**Objetivo:** El cerebro sabe exactamente qué es cada pista, y el usuario lo confirma visualmente.

**Resultado:**
``` 
Kick → ✅ confirmado     Bass → ✅ confirmado
Vocal → ✅ confirmado    FX   → ✅ confirmado
```

**Implementado:**
- ✅ `TrackRole` enum con 50+ roles (Kick, Bass808, VozPrincipal, etc.)
- ✅ `inferTrackRoleFromName()` — inferencia por keywords EN+ES
- ✅ `SpectralProfiler::inferTrackRole()` — inferencia por perfil espectral
- ✅ `inferTrackRoleCombined()` — combinación nombre + espectral
- ✅ `setTrackRoleWithLearning()` — aprende de correcciones
- ✅ `CorrectionLearner` — mejora futuras inferencias
- ✅ Feedback Loop V9: MixCoach → Messenger
- ✅ Role pills en MessengerList con confianza (✅ ⚠️ ❌)
- ✅ Badge 🎯 `X/Y` en health bar (confirmed/identified)
- ✅ ⚡ badge pulsante en tracks no confirmados
- ✅ Confirmación individual (clic ⚡) + masiva ("Confirmar todo")
- ✅ ✅/⚠️/❌ según confianza (≥0.75 / ≥0.4 / <0.4)
- ✅ Botón "Confirmar todo" + avance de fase automático
- ✅ Inferencia continua cada 8s en `periodicAnalysis()` para nuevas pistas
- ✅ Signal Order Inference V13 (silence + load order para nombres genéricos)
- ✅ `showIdentitySummary()` con resumen por familias (Drums, Bass, Vocals...)

**Archivos clave:** `CoachEngine.cpp/h`, `MessengerListComponent`, `MessengerListRoles`, `SpectralProfiler`, `CorrectionLearner`, `TrackRole.h`

---

### Sprint 2 — Mix Map ✅ COMPLETED

**Objetivo:** Visualizar toda la sesión como un árbol jerárquico de buses.

**Resultado esperado:**
```
📋 SESIÓN
├── 🥁 Drums → Drum Bus
├── 🎸 Bass → Bass Bus
├── 🎹 Synths → Music Bus
├── 🎤 Vocals → Vox Bus
└── 🌊 Todos → Master
```

**Implementado:**
- ✅ `SessionMap` struct con categorías y jerarquía
- ✅ `SessionMapEntry` serializable a JSON
- ✅ `buildSessionMap()` en CoachEngine
- ✅ Persistencia en `session_memory.json`
- ✅ `MixMapComponent` con jerarquía Drums→Drum Bus→Master
- ✅ Routing visual: líneas de conexión buses → Master
- ✅ Flechas con triángulo relleno + glow dots
- ✅ Sección MASTER BUS al fondo con resumen de buses
- ✅ Per-track: color dot, level bar, freq bar, stereo badge
- ✅ Tooltip hover con info detallada + sugerencias de bus
- ✅ Botón "Confirmar mapa" con avance de fase
- ✅ Colores por bus (`MixCoachTheme::busColour()`)

**Archivos clave:** `MixMapComponent.h/.cpp`, `CoachEngine.cpp/h` (buildSessionMap, onMapConfirmed), `SessionMap.h`

---

### Sprint 3 — Per Track Analysis ✅ COMPLETED

**Objetivo:** El cerebro entiende cada pista en profundidad y lo muestra visualmente.

**Resultado:**
```
Kick_01:
  PK -4.2 | RMS -18.3 | CR 14.2 🟢
  ▓▓▓░░░░░░░ [espectral 6-bandas]
  ▓▓▓▓░░░░░░ [stereo width 12px]
```

**Implementado:**
- ✅ `TrackAudioResult` con peak/RMS/correlation/crest/bandEnergies/transientRatio
- ✅ Envelope descriptors (attack/release/sustain) desde bg worker
- ✅ Mid/Side decomposition por región
- ✅ `SpectralProfiler::computeProfile()` → `TrackSpectralProfile` completo
- ✅ **Crest factor** coloreado en línea de métricas: `PK -12.3 | RMS -18.5 | CR 5.8`
- ✅ Color coding crest: 🟢 4-12dB, 🟡 12-18dB, 🔴 <4dB o >24dB
- ✅ **Spectral mini-bar** (6 bandas, 3px) al pie de cada card
- ✅ Colores: violeta/azul/verde/ámbar/naranja/rojo por región
- ✅ **Stereo width indicator** (barrita 12px con color por zona)
- ✅ Rojo=fase, gris=mono, cyan=estrecho, violeta=wide
- ✅ Bug espectro siempre -80.0 dBFS arreglado
- ✅ Bug `isPercussive` siempre false arreglado

**Archivos clave:** `MessengerListDrawing.cpp`, `MessengerListTelemetry.cpp`, `TrackFeedCore`, `AudioAnalyzer`

---

### Sprint 4 — Reference-Driven Coaching ✅ COMPLETED

**Objetivo:** La referencia dirige toda la mezcla con mensajes cualitativos.

**Resultado:**
```
🎯 Modo Referencia activado — ahora toda la mezcla se mide contra la referencia.
📈 La mezcla está mejorando contra la referencia.
📋 Áreas con diferencia:
  • Balance espectral (casi listo)
  • Rango dinámico (requiere atención)
```

**Implementado:**
- ✅ `ReferenceDrivenMode` flag en CoachEngine
- ✅ `ReferenceProgress` struct con match %, delta, tendencia
- ✅ `computeReferenceMatchProgress()` — match % = deltaScore*0.5 + gapScore*0.3 + lufsScore*0.2
- ✅ `sendReferenceDrivenAnalysis()` — prompt al LLM o fallback cualitativo al chat
- ✅ Hook en `periodicAnalysis()` cada 30s
- ✅ `buildSystemPrompt()` incluye `[REFERENCE-DRIVEN MODE ACTIVO]`
- ✅ `toLLMContext()` formatea progreso para el LLM
- ✅ Toggle REF MODE en header del panel
- ✅ Progress bar con SmoothValue + flecha de tendencia ▲/▼
- ✅ Menú contextual (clic derecho): Configurar, Historial, Reset
- ✅ **Bienvenida al activar:** mensaje explicativo al chat
- ✅ **Mensajes cualitativos periódicos** cada ~30-90s
- ✅ Estados: 🎯 Muy cerca / Buen camino / Avanzando / Empezando
- ✅ Describe top DomainGaps sin números crudos
- ✅ Tendencia 📈 mejorando / 📉 alejándose / ➡ estable
- ✅ Gaps resueltos celebrados con ✅
- ✅ LLM path: prompt estructurado con gaps + plan + match %
- ✅ DifferenceProfile + DomainGap + PlanManager

**Archivos clave:** `CoachEngine.cpp/h`, `ReferenceDrivenEngine`, `ReferencePanelComponent`, `ReferenceMatchPanel`, `PlanManager`

---

### Sprint 5 — TrackFeed Live ✅ COMPLETED

**Objetivo:** TrackFeedCore es el motor central de la UI, con eventos vivos, filtros por salud y tooltips.

**Resultado:**
```
🎯 TRACKFEED — 3 tracks necesitan atención
═══════════════════════════════════════════
🔴 Kick     — Crest 2.1dB (sobre-comprimido)
🟡 808 Bass — Crest 22dB (muy dinámico)
🟢 Voz      — Crest 10dB (perfecto para lead)

❌ 3 ⚠️ 2 ✓ 5 ✰ 1    ← click para filtrar
```

**Implementado:**

**Fase A — Corazón del sistema 🧠**
- ✅ `trackFeedCore_` inicializado en constructor de `CoachEngine` (`make_unique<TrackFeedCore>()`)
- ✅ `syncTrackFeedCore()` implementado: itera 128 slots activos, lee `TrackAudioResult` + `SlotInfo` + `TrackRole`, llama `trackFeedCore_->updateTrackState()`
- ✅ `syncTrackFeedCore()` conectado al inicio de `periodicAnalysis()` (~8s)

**Fase B — UI en vivo 🖥️**
- ✅ **TrackFeed Banner** colapsable arriba de MessengerList con top 3 eventos globales
- ✅ Banner muestra dots de severidad (🔴🟡🟢) + texto truncado + contador "+N"
- ✅ Flecha ▶/▼ colapso/expande
- ✅ **Health Filter Pills:** click en ❌/⚠️/✓ filtra la lista a solo esos tracks
- ✅ `HealthFilter` enum (None, Critical, Warning, Clean, Silent)
- ✅ `isFilteredOut()` chequea health de cada track contra el filtro activo
- ✅ **Event Tooltip** al hover del health dot muestra últimos eventos de esa pista
- ✅ Tooltip con dots de severidad + mensajes truncados
- ✅ `updateTopEvents()` llamado desde `updateCoachAdvice()` cada ciclo

**Flujo activo:**
```
periodicAnalysis() cada ~8s
    └── syncTrackFeedCore()
        └── TrackFeedCore::updateTrackState() x 128 slots
            ├── Genera eventos, computa health + attention
            └── Almacena en eventHistory[]

EditorTimer ~60fps
    └── updateCoachAdvice()
        ├── coachAdviceText + status por track
        └── updateTopEvents() → banner con top eventos

MessengerList paint()
    ├── TrackFeed banner (colapsable)
    ├── Health pills ❌⚠️✓ (click → filtro)
    ├── Track cards con health dot + tooltip hover
    └── Filtro por salud activo
```

**Archivos clave:** `CoachEngine.cpp/h`, `MessengerListComponent.h/.cpp`, `MessengerListDrawing.cpp`, `MessengerListAdvice.cpp`, `TrackFeedCore`, `TrackState`

---

## 🏃 Próximo Sprint

### Sprint 6 — Track Intelligence Layer: 3 dominios, 0 IA

> **Filosofía:** GPT lo dijo mejor: *"No gastes tokens para detectar crest bajo. Eso es matemática. Hazlo en C++. Gratis. Instantáneo. 0 tokens."*
>
> La IA entra DESPUÉS, cuando el sistema ya sabe qué está mal. El LLM recibe el diagnóstico ya estructurado y responde como ingeniero.

**Arquitectura común para los 3 sub-sprints:**

```
analyzeTrackX(int slotIndex) → TrackXAdvice
    ├── Lee TrackState del TrackFeedCore (o telemetría directa)
    ├── Compara contra ExpectedProfile del rol
    ├── Status: OnTarget / NearTarget / OffTarget
    └── suggestedDelta: cuantificado (+4.2 dB, "baja ratio 2:1", "corta 3dB en 3kHz")

analyzeAllTracksX() → vector ordenado por severidad
    ├── Itera slots activos, filtra actionable
    └── Conectado en periodicAnalysis() con cooldown
```

---

### Sprint 6A — Track Gain Intelligence ✅ COMPLETED (22 junio 2026)

**Objetivo:** El cerebro responde: ¿Está muy bajo? ¿Está muy alto? ¿Cuál sería el target?

**Resultado:**
```
VOCAL
  Actual: -2.1 dBFS | Target: -6 dBFS
  🔴 Demasiado alto. Baja ~4 dB.
```

**Implementado:**
- ✅ `TrackGainAdvice` struct con: currentPeak/RMS/LUFS/Crest, peakTarget/Tolerance, peakDeviation, suggestedDeltaDb, Status enum (OnTarget/NearTarget/OffTarget/NoSignal/UnknownRole), isActionable()
- ✅ `analyzeTrackGain(int slotIndex)` — lee telemetría + rol, compara peak vs `ExpectedProfile.peakTargetDb`, genera mensaje accionable con emoji + dB sugerido
- ✅ `analyzeAllTracksGain()` — itera 128 slots, filtra actionable, ordena OffTarget primero
- ✅ Conectado en `periodicAnalysis()`: después de `syncTrackFeedCore()`, envía mensaje consolidado al chat con top 3 OffTarget (cooldown 120s)
- ✅ `ExpectedProfile` en `TrackRole.h` ya tiene `peakTargetDb` + `peakTolerance` para TODOS los roles (50+)
- ✅ **Sin IA** — reglas C++, 0 tokens, instantáneo

**Firma espectral por rol (ejemplos de targets):**

| Rol | Peak Target | Crest Target |
|:----|:-----------|:-------------|
| 🥁 Kick | -6 dB | 14 dB |
| 🥁 Snare | -8 dB | 16 dB |
| 🎤 Voz Principal | -6 dB | 10 dB |
| 🎸 Bass Finger | -8 dB | 10 dB |
| 🎹 Synth Pad | -12 dB | 6 dB |
| 🎹 HiHat | -12 dB | 18 dB |
| 🎵 Synth Lead | -8 dB | 10 dB |

**Archivos clave:** `CoachEngine.h` (TrackGainAdvice + declarations), `CoachEngine.cpp` (implementation + periodicAnalysis hook + fusión con gainStaging), `TrackRole.h` (ExpectedProfile con targets por rol)

**Integración TrackFeedCore:**
- ✅ `checkAndSetGainCooldown()` en TrackFeedCore (usa lastClipWarningUs)
- ✅ Bloque gain en `syncTrackFeedCore()`: eventos ClippingDetected/LevelSpike/LowSignal + health ClippingRisk/LowSignal
- ✅ Cooldown 120s vía kGainAdviceCooldownUs

---

### Sprint 6B — Track Dynamics Intelligence ✅ COMPLETED (22 junio 2026)

**Objetivo:** El cerebro responde: ¿Está sobre-comprimido? ¿Muy dinámico? ¿Cuál sería el crest target?

**Utiliza:** `crestFactor` del `TrackAudioResult` contra `ExpectedProfile.crestTargetDb`

**Resultado:**
```
Kick:
  🔴 Sobre-comprimido (crest 2.1 dB, target 14 dB). Baja el ratio o sube el threshold 6 dB.

Snare:
  🔴 Demasiado dinámico (crest 22 dB, target 16 dB). Prueba compresor 4:1 con attack rápido.

Voz:
  🟡 Crest 12 dB (target 10 dB). Cerca del límite.
```

**Implementado:**
- ✅ `TrackDynamicsAdvice` struct: currentCrest, crestTarget, crestTolerance, crestDeviation, suggestedAction, Status enum (OnTarget/NearTarget/OffTarget), SubType enum (None/Overcompressed/TooDynamic), isOvercompressed(), isTooDynamic(), isActionable()
- ✅ `analyzeTrackDynamics(int slotIndex)` — compara crest vs `ExpectedProfile.crestTargetDb` ± crestTolerance, detecta sobre-compresión vs demasiada dinámica, sugiere acción específica
- ✅ `analyzeAllTracksDynamics()` — itera 128 slots, filtra actionable, ordena por severidad (Overcompressed > TooDynamic > NearTarget)
- ✅ Conectado en `periodicAnalysis()` después de Sprint 6A con cooldown 120s
- ✅ **Sin IA** — reglas C++, 0 tokens, instantáneo
- ✅ Fixes: UTF-8 malformado corregido, "reduce el threshold" → "sube el threshold" (subir threshold = menos compresión = más dinámica)

**Reglas C++ (0 IA):**
- crest < (target - tolerance) → 🔴 Sobre-comprimido → "Baja el ratio o sube el threshold X dB"
- crest > (target + tolerance) → 🔴 Demasiado dinámico → "Prueba compresor 4:1 con attack rápido"
- crest in range → ✅ En rango o 🟡 Cerca del límite

**Archivos:** `CoachEngine.h/.cpp` (TrackDynamicsAdvice + declarations + implementation + periodicAnalysis hook), `TrackRole.h` (ya tiene crestTargetDb y crestTolerance en TODOS los roles)

**Integración TrackFeedCore:**
- ✅ `checkAndSetCrestCooldown()` en TrackFeedCore (usa lastCrestWarningUs)
- ✅ Bloque dynamics en `syncTrackFeedCore()`: eventos CrestTooLow/CrestTooHigh + health Overcompressed/NeedsCompression
- ✅ Cooldown 120s vía kDynamicsAdviceCooldownUs

---

### Sprint 6C — Track Tonal Intelligence ✅ COMPLETED (22 junio 2026)

**Objetivo:** El cerebro responde: ¿Exceso de energía en alguna región espectral? ¿Falta? ¿Cuál es el target?

**Utiliza:** `bandEnergies[30]` agrupadas en 6 regiones contra `ExpectedProfile.spectralOffset[6]`

**Resultado:**
```
Kick:
  🔴 Exceso de energía en Sub (20-86 Hz, -8.2 dBFS, esperado -14.0 dBFS). Prueba reduce 50-100 Hz.

Pad:
  🔴 Falta de energía en HiMid (1076-3532 Hz, -22.3 dBFS, esperado -16.0 dBFS). Prueba refuerza 1-3 kHz.

Vocal:
  🟡 Ligero desbalance espectral Exceso en Pres (3.5 dB sobre target). reduce 3-8 kHz.
```

**Implementado:**
- ✅ `TrackTonalAdvice` struct: regionEnergy[6], regionExpected[6], regionDeviation[6], worstRegion, worstDeviation, isExcess, Status enum (OnTarget/NearTarget/OffTarget), helpers estáticos: kRegionName(), kRegionFreq(), kExcessSuggestion(), kDeficitSuggestion(), isActionable()
- ✅ `analyzeTrackTonal(int slotIndex)` — mapea 30 bandEnergies → 6 regiones espectrales (5 bandas c/u: Sub/Bass/LoMid/HiMid/Pres/Air), compara energía actual vs `peakDb + spectralOffset[region]`, detecta exceso/déficit con tolerancia ±6dB, genera mensaje con región, rango de frecuencia, valores dBFS y sugerencia EQ
- ✅ `analyzeAllTracksTonal()` — itera 128 slots, filtra actionable, ordena por severidad (exceso > déficit > NearTarget)
- ✅ Conectado en `periodicAnalysis()` después de Sprint 6B con cooldown 120s
- ✅ **Sin IA** — reglas C++, 0 tokens, instantáneo

**Reglas C++ (0 IA):**
| Desviación vs target | Diagnóstico | Acción sugerida |
|:---------------------|:------------|:----------------|
| > +12 dB | 🔴 Exceso por región | "reduce XXX-XXX Hz" |
| < -12 dB | 🔴 Déficit por región | "refuerza XXX-XXX Hz" |
| ±6-12 dB | 🟡 Desbalance ligero | Monitorear |
| ±6 dB | ✅ En rango | — |

**Mapeo 30 bandas → 6 regiones:**
| Región | Bands | Rango Frecuencia |
|:-------|:-----:|:----------------|
| Sub | 0-4 | 20-86 Hz |
| Bass | 5-9 | 86-301 Hz |
| LoMid | 10-14 | 301-1076 Hz |
| HiMid | 15-19 | 1076-3532 Hz |
| Pres | 20-24 | 3532-8355 Hz |
| Air | 25-29 | 8355-16458 Hz |

**Archivos:** `CoachEngine.h/.cpp` (TrackTonalAdvice + declarations + implementation + periodicAnalysis hook + fix español), `TrackRole.h` (ya tiene spectralOffset[6] por rol)

**Integración TrackFeedCore:**
- ✅ `checkAndSetTonalCooldown()` en TrackFeedCore (usa lastSpectralWarningUs)
- ✅ Bloque tonal en `syncTrackFeedCore()`: eventos SpectralImbalance (0.7/0.3) + health NeedsEQ
- ✅ Cooldown 120s vía kTonalAdviceCooldownUs

---

### 📌 Prioridad Siguiente Sesión

```
🥇 Sprint 7 — TrackFeed Inteligente
    Mostrar 🟢🟡🔴 por pista en la UI con TrackAdvice consolidado
    Banner con top issues + clic → detalle del advice

🥈 LLM Mentor v2
    Dar al LLM el vector completo de TrackAdvice[] (gain + dynamics + tonal)
    para que responda como ingeniero: "Juan, empezaría por la voz..."

🥉 Fusión: analyzeGainStagingReal() → analyzeTrackGain()
    ✅ COMPLETED — mensajes de clipping ya muestran target del rol

④ TrackGainTonalAdvice unificado (opcional)
    Fusionar los 3 advices (Gain/Dynamics/Tonal) en TrackAdvice único
```

**Logros de esta sesión (22 junio 2026):**
- ✅ Sprint 6A — Track Gain Intelligence (analyzeTrackGain + targets por rol)
- ✅ Fusión gainStaging → analyzeTrackGain (mensajes con target del rol + fix build error)
- ✅ Sprint 6B — Track Dynamics Intelligence (analyzeTrackDynamics + crest targets)
- ✅ Sprint 6C — Track Tonal Intelligence (analyzeTrackTonal + 30→6 bandas + spectralOffset)
- ✅ Integración TrackFeedCore: gain + dynamics + tonal events en banner y health dots

**Ver archivos:**
- `Source/MixCoach/engine/CoachEngine.cpp/h` — motor de análisis
- `Source/MixCoach/engine/TrackRole.h` — ExpectedProfile con targets por rol
- `Source/MixCoach/UI/MessengerListAdvice.cpp` — conexión con UI

---

## ❌ Lo que NO construir (en el próximo mes)

| Feature | Razón |
|:--------|:------|
| ❌ Mastering Mode | No acerca a la visión |
| ❌ User Profiles | No acerca a la visión |
| ❌ Cloud / Mac | Windows + offline primero |
| ❌ Reportes PDF | Sin valor para el core loop |
| ❌ Gamificación | Distrae del aprendizaje real |
| ❌ Más analizadores | Ya hay suficientes |
| ❌ Más medidores | Ya hay suficientes |

---

*Roadmap de producto — MixCoach — 22 junio 2026*
*Documento para IA — leer al inicio de cada sesión*

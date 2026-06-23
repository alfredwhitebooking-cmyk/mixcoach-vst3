# 🎯 PRODUCT VISION — MixCoach

> **Documento canónico de experiencia de usuario y visión de producto.**
> Cualquier feature, UI o cambio arquitectónico debe contrastarse contra esta visión.
>
> **Actualizado para:** MASTER VISION v3
> **Versión:** 2.0 | **Última actualización:** 13 junio 2026

---

## 📋 Índice

1. [🏛️ Filosofía Central](#️-filosofía-central)
2. [🎭 La Experiencia Completa (Simulación UX 6 Fases)](#-la-experiencia-completa-simulación-ux-6-fases)
3. [🧠 Principios de Diseño](#-principios-de-diseño)
4. [🏗️ Mapa de Implementación](#️-mapa-de-implementación)
5. [📊 Roadmap Técnico](#-roadmap-técnico)

---

## 🏛️ Filosofía Central

### La jerarquía de MixCoach

```
1. El oído humano está por encima de los números
2. La organización define la calidad del análisis
3. El balance es más importante que el procesamiento
4. La referencia define el estándar real
5. El sistema debe formar ingenieros, no crear dependencia
```

### Arquitectura del Ingeniero Virtual

MixCoach funciona con **tres capas estrictamente separadas**:

```
MESSENGER (por pista)        → Identidad estructural
       ↓
COACH ENGINE (global)        → Inteligencia musical
       ↓
MASTER OUTPUT (referencia)   → Validación final
```

| Capa | Rol | Lo que NO hace |
|:-----|:----|:---------------|
| **Messenger** | Identifica y estructura la sesión | No toma decisiones musicales |
| **Coach Engine** | Interpreta y guía la mezcla | No identifica pistas ni construye estructura |
| **Master Output** | Valida contra la referencia | No reemplaza al coach |

### Regla crítica

> **El Coach Engine nunca trabaja sin Messenger.**
>
> Si Messenger no está activo en cada pista:
> - No hay coaching avanzado
> - No hay decisiones estructurales
> - Solo análisis básico de audio

---

## 🎭 La Experiencia Completa (Simulación UX 6 Fases)

> *Esta simulación fue creada por el fundador del proyecto y describe la experiencia de usuario definitiva de MixCoach. Es el norte del producto.*

---

### FASE 1: BIENVENIDA — El primer contacto

```
MixCoach: 🎧 ¡Hola Ingeniero! ¿Qué vamos a mezclar hoy?

Tú: Hoy es reggaetón tipo Bad Bunny.

MixCoach: 🔥 ¡Perfecto! Reggaetón. Vamos a hacer que tu mezcla suene enorme.
          Te voy a guiar paso a paso. Primero, necesito que actives el Messenger
          en CADA pista de tu sesión.

📋 CHECKLIST INICIAL:
☐ Activar Messenger en cada pista
☐ Organizar por colores (batería, bajo, voces, melodías, fx)
☐ Verificar que MixCoach recibe señal en el master
```

**Mecánica:** El coach pregunta intención, configura expectativas según género, y da la primera instrucción clara: activar Messenger.

---

### FASE 2: ACTIVACIÓN DE MESSENGER — Identidad por pista

```
Tú: Listo, Messenger activo en todas las pistas.

MixCoach: ✅ Detectando tu sesión...

📡 MESSENGER ACTIVO — IDENTIFICANDO PISTAS...

🎧 Total de pistas detectadas: 30

Identificando rol de cada pista...

Track 1:  Kick     → Rol: Kick (Drum Bus)     ✅
Track 2:  Snare    → Rol: Snare (Drum Bus)     ✅
Track 3:  HiHat    → Rol: HiHat (Drum Bus)     ✅
Track 4:  808_1    → Rol: 808 Bass (Bass Bus)  ✅
Track 5:  Vox      → Rol: Lead Vocal (Vox Bus) ✅
...

⚠️ 3 pistas sin identificar:
  - "Audio 1" → ¿Qué es esto? (seleccionar rol)
  - "Track 5" → ¿Qué es esto? (seleccionar rol)
  - "Synth 2" → ¿Qué es esto? (seleccionar rol)
```

**Mecánica CLAVE:**
- Messenger responde: **"¿Qué es este sonido dentro de la mezcla?"**
- Identifica: tipo de instrumento, rol musical, rango frecuencial, bus, routing
- Las pistas que no puede identificar, las marca para que el usuario las etiquete
- Messenger NO toma decisiones musicales — solo identifica y estructura

**Resultado de Messenger por pista:**
```
Track: Vocal 1
Role: Lead vocal
Bus: Vox Bus
Position: Center
Range: 1k–5k Hz
```

---

### FASE 3: CONSTRUCCIÓN DEL MAPA DE MEZCLA

```
MixCoach: 📊 Generando mapa de mezcla...

Con la información de Messenger, construyo el mapa completo de tu sesión:

📋 MAPA DE MEZCLA — REGGAETÓN
═══════════════════════════════════════════════════════════════
🥁 DRUM BUS (4)
  Kick  → Drum Bus → Master    [Center | 60-100Hz fund.]
  Snare → Drum Bus → Master    [Center | 200Hz-4kHz]
  HiHat → Drum Bus → Master    [Stereo | 8-15kHz]
  Percu → Drum Bus → Master    [Stereo | 200Hz-3kHz]

🎸 BASS BUS (2)
  808_1 → Bass Bus → Master    [Center | 50-60Hz fund.]
  808_2 → Bass Bus → Master    [Center | 45-55Hz fund.]

🎹 MUSIC BUS (8)
  Piano  → Music Bus → Master  [Stereo]
  Synth  → Music Bus → Master  [Stereo]
  ...

🎤 VOX BUS (9)
  Voz Principal → Vox Bus → Master  [Center]
  Adlibs        → Vox Bus → Master  [Stereo]
  ...

🌊 FX BUS (4)
  Risers → FX Bus → Master    [Stereo]
  ...
═══════════════════════════════════════════════════════════════

MixCoach: ¿Es correcto este mapa? Confirma el ruteo antes de seguir.
```

**Mecánica:** Messenger genera la identidad de todas las pistas, la relación entre elementos y el flujo hacia el master. El usuario valida el mapa antes de continuar.

---

### FASE 4: COACHING ACTIVO — Balance, problemas, técnicas

**Subfase 4.1 — Balance inicial**

```
MixCoach: 📊 FASE 4: BALANCE INICIAL

Vamos a establecer el balance de niveles antes de procesar nada.
Usa el trim o ganancia de entrada — no toques los faders aún.

🥁 Kick         → RMS: -3dB  🔴 Bájale -2dB (muy caliente)
🥁 Snare        → RMS: -8dB  🟢 Sube +1dB (un poco bajo)
🎸 808_1        → RMS: -10dB 🟢 Sube +3dB (importante peso)
🎤 Voz Principal→ RMS: -4dB  🔴 Bájale -1dB
...
```

**Subfase 4.2 — Problemas críticos**

```
MixCoach: ⚠️ Detecté problemas que atender antes de seguir:

🔴 MASTER EN CLIPPING (+0.3dBFS) — Baja el master fader 3dB YA
🔴 Kick y 808 compiten en 60Hz — enmascaramiento de graves
🟡 Voz sin presencia en 2-4kHz — se pierde en la mezcla

Empecemos por el clipping. Después ajustamos el resto.
```

**Subfase 4.3 — Organización**

```
MixCoach: Antes de profundizar, arreglemos el orden:

⚠️ Track "Audio 1" sin nombre → renombrar
⚠️ Track "Synth 2" sin color asignado → asignar color del bus Music
⚠️ Guitarra en bus de voces → mover a Music Bus

El orden es la base de todo. 5 minutos y seguimos.
```

**Subfase 4.4 — Técnicas de mezcla**

```
MixCoach: 🎧 DINÁMICA

🥁 Kick → El ataque se pierde. Prueba compresor 4:1,
          attack 10ms, release 80ms.

Tú: [Aplico compresión 8:1 en el kick]

MixCoach: ⚠️ 8:1 aplasta el transiente. Bájalo a 4:1.
          El kick debe "empujar", no "golpear seco".

Tú: [Ajusto a 4:1]

MixCoach: ✅ ¡Mejor! Ahora el kick tiene pegada pero no rompe.
          ¿Escuchas la diferencia?
```

**Subfase 4.5 — Correcciones progresivas**

Loop de corrección:
1. Coach recomienda un valor exacto
2. Usuario aplica (a veces exagera)
3. Coach escucha el resultado y corrige
4. Se repite hasta "óptimo"

---

### FASE 5: REFERENCIAS — Comparación contra tracks reales

```
MixCoach: 🎯 FASE 5: REFERENCIAS

Carga tu referencia. Un archivo de audio o un enlace.

Tú: [Cargo "Tití Me Preguntó" de Bad Bunny]

MixCoach: 🔍 Analizando referencia...

📊 COMPARACIÓN MIX vs REFERENCIA
═══════════════════════════════════════════════════════════════
🎚️ LUFS:   Mix: -12dB  |  Ref: -8.2dB  |  Diff: +3.8dB 🔴
🥁 808:    Mix: -10dB  |  Ref: -6dB    |  Diff: +4dB   🔴
🎤 Voz:    Mix: plano  |  Ref: +3dB@3kHz | Sin presencia 🟡
✅ Batería: Bien posicionada
✅ Melodías: Buen balance
═══════════════════════════════════════════════════════════════

MixCoach: Tu 808 está 4dB más bajo que la referencia.
          Sube 2dB en la fundamental (50-60Hz) y escuchamos.
```

**Mecánica:** La referencia se usa para evaluar cercanía y guiar ajustes. No es opcional — es parte fundamental del flujo.

---

### FASE 6: REFINAMIENTO — Profundidad, estéreo, impacto final

```
MixCoach: 🌊 FASE 6: REFINAMIENTO

Vamos a darle profundidad y dimensión a la mezcla.

🎤 Voz → Reverb de sala, pre-delay 40ms, decay 1.8s, envío 30%
🥁 Drum Bus → Compresión paralela 8:1, mix 20%
🎸 Melodías → Delay estéreo 1/8, feedback 20%, mix 15%

Tú: [Aplico los ajustes]

MixCoach: ✅ Ahora la mezcla tiene profundidad.

📊 IMPACTO FINAL:
═══════════════════════════════════════════════════════════════
✅ Estructura de ganancia
✅ Compresión en elementos clave
✅ EQ quirúrgico y de realce
✅ Saturación y clipping
✅ Reverb, delays y procesamiento paralelo
✅ Mezcla traducible a otros sistemas
═══════════════════════════════════════════════════════════════

🎯 RESUMEN DE SESIÓN:
• 30 pistas organizadas y procesadas
• 12 recomendaciones aplicadas
• 3 loops de corrección
• Referencia: Tití Me Preguntó (80% de cercanía)

🏆 ¿Qué quieres hacer ahora?
A) Exportar la mezcla
B) Guardar sesión y continuar mañana
C) Empezar una nueva sesión
```

**Mecánica:** El refinamiento incluye profundidad, estéreo, automatización e impacto final. La sesión termina con un resumen de logros y opciones para el usuario.

---

## 🧠 Principios de Diseño

Extraídos de la simulación anterior. Toda decisión de producto debe alinearse con estos principios.

### 1. Doble capa: Messenger + Coach Engine
```
Messenger = identidad estructural por pista
Coach Engine = inteligencia musical global
```
Ninguna puede reemplazar a la otra. Trabajan en serie: primero Messenger, después Coach.

### 2. Messenger obligatorio
El Coach Engine **nunca** trabaja sin Messenger. Si Messenger no está activo en cada pista, el coach solo da análisis básico — no hay mentoría avanzada ni decisiones estructurales.

### 3. Mentor, no procesador
MixCoach **nunca toca el audio**. Solo analiza, sugiere y verifica. El usuario tiene el control creativo total.

### 4. Loop de corrección
Recomendar → Usuario aplica → Coach verifica → Corrige si es necesario. Este ciclo es el corazón de la experiencia de aprendizaje.

### 5. Mapa de mezcla explícito
La sesión no solo se escanea — se construye como un mapa visual que el usuario puede ver y validar. El mapa incluye: identidad de cada pista, routing, relación entre elementos.

### 6. Referencia como estándar
El usuario siempre debe tener una referencia. MixCoach compara espectro, LUFS, balance tonal y da diferencias cuantificables. La referencia no es opcional — es parte del flujo.

### 7. Organización ante todo
Si la sesión está desordenada, el coach lo detecta y lo corrige **antes** de cualquier análisis de audio. Tracks sin nombre, colores aleatorios, buses mal ruteados — se ataca primero.

### 8. Tono profesional
"Nunca critiques. Siempre sugiere con fundamento." Tono de ingeniero senior ayudando a un colega.

### 9. Persistencia multi-sesión
MixCoach recuerda: qué fase ibas, qué ajustes hiciste, qué referencia usaste, qué mapa de mezcla construiste.

### 10. Sin puntuaciones visibles
MixScore existe solo para consumo interno del LLM. El usuario nunca ve un score 0-100. Ve texto descriptivo: "bien encaminado", "podemos mejorar", "hay problemas críticos".

### 11. Formar ingenieros, no crear dependencia
El objetivo final no es corregir mezclas. Es formar usuarios que escuchan con criterio, organizan sesiones, entienden el flujo de señal, usan referencias y piensan como ingenieros reales.

---

## 🏗️ Mapa de Implementación

### Ya existe en el código V3

| Concepto | Implementación actual |
|:---------|:----------------------|
| **Chat interactivo** | `CoachChatComponent` + `ChatMessagesComponent` |
| **Messenger por pista** | `MessengerPlugin` envía RAW a `SlotRegistry` + `SharedAudioMemory` |
| **Master analyzer** | `AudioAnalyzer` (FFT, LUFS, fase, vectorscope) |
| **Track scanning** | `forceFullSync()` + `forEachActive()` |
| **Colores por bus** | `MixCoachTheme::busColour(BusType)` |
| **Persistencia básica** | `SharedData` + `AI_SESSION_STATE.json` |
| **Envelope detection** | Per-track attack/release/sustain en bg worker |
| **Mid/Side decomposition** | midEnergyPerBand[6], sideEnergyPerBand[6] |
| **Spectral profiling** | `SpectralProfiler` con 30-band spectrum por track |
| **MixScore** | Interno para LLM, no visible al usuario |

### Falta para llegar a la visión completa

| Feature | Prioridad | Esfuerzo |
|:--------|:---------:|:--------:|
| **Messenger como capa de identidad** (rol, función, rango, routing explícito) | 🥇 1 | ⭐⭐⭐ |
| **Mapa de mezcla visual** (entregable explícito de Fase 3) | 🥇 2 | ⭐⭐⭐ |
| **TrackFeed** (mensajes por track con estado 🟢🟡🔴 en la lista) | 🥇 3 | ⭐⭐ |
| **CoachEngine con análisis por track** (gain, compresión, EQ específicos) | 🥇 4 | ⭐⭐⭐⭐ |
| **Reference matching** (comparación espectro + LUFS contra referencia real) | 🥇 5 | ⭐⭐⭐ |
| **Loop de corrección** (detectar si el usuario exageró una recomendación) | 🥇 6 | ⭐⭐⭐⭐ |
| **Fase de refinamiento** (profundidad, estéreo, automatización, impacto final) | 🥈 7 | ⭐⭐⭐ |
| **Persistencia multi-sesión avanzada** (guardar/restaurar mapa de mezcla completo) | 🥈 8 | ⭐⭐ |
| **Modo Mastering** (perfil separado del cerebro) | 🥉 9 | ⭐⭐⭐⭐⭐ |
| **Informes de mezcla exportables** | 🥉 10 | ⭐ |

---

## 📊 Roadmap Técnico

### Fase 1 — Messenger Identity Layer
Transformar Messenger de "sensor RAW" a "capa de identidad de sonido". Que responda: ¿qué es este sonido? Rol, función, rango, bus, routing.

### Fase 2 — Mapa de Mezcla
Construir el mapa visual de la sesión a partir de los datos de Messenger. Mostrar routing completo, relaciones entre pistas, flujo hacia el master.

### Fase 3 — TrackFeed
Implementar notificaciones por track en `MessengerListComponent`. Cada Messenger muestra un indicador 🟢🟡🔴 con recomendación textual corta.

### Fase 4 — CoachEngine v2
Que el coach sea capaz de analizar cada track individualmente (gain, dinámica, EQ, envelope) y generar recomendaciones específicas con datos reales.

### Fase 5 — Reference Analyzer
Extender `ReferencePanelComponent` para analizar el audio de referencia (espectro, LUFS, balance tonal) y comparar contra cada track y el master.

### Fase 6 — Loop de corrección
Después de dar una recomendación, el coach mustrea el master a los pocos segundos para verificar que el cambio fue aplicado correctamente.

### Fase 7 — Refinamiento y Veredicto
Implementar la Fase 6 completa: profundidad estéreo, automatización, impacto final, resumen de sesión, exportación.

---

*Documento de visión de producto — MixCoach — 13 junio 2026*
*Alineado con MASTER VISION v3 y AI_CONTEXT.md §8*

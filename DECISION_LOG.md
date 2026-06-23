# 📜 DECISION_LOG.md — Registro de Decisiones Arquitectónicas

> **ADRs (Architecture Decision Records) del proyecto MixCoach.**
> **Versión:** 1.1 | **Última actualización:** 2026-06-06

---

## ¿Qué es esto?

Cada ADR documenta una decisión arquitectónica importante: contexto, alternativa elegida, alternativas descartadas, y consecuencias. **Léelos antes de modificar el código para entender POR QUÉ está como está.**

---

## ADR-001: Two-Phase Spinlock para Shared Memory

**Fecha:** 2026-06-02 | **Estado:** Aceptado

### Contexto
Con 60+ Messengers en paralelo, cada uno adquiría el spinlock de shared memory 2 veces por bloque de audio (`readSlot` + `writeSlot`). El spinlock usaba `Sleep(0)` en CADA iteración del while, causando context switches completos (~1-15μs cada uno). El scheduler de Windows se saturaba con 60 threads compitiendo.

### Decisión
Reemplazar spinlock monofásico con two-phase:
1. **Fase 1 (~1000 iteraciones):** `_mm_pause()` (intrinsic x86, ~1μs, sin syscall)
2. **Fase 2 (después de 1000):** `Sleep(0)` (context switch, solo si muy contenido)

### Consecuencias
- **+** Cero context switches en el hot path del spinlock
- **+** Compatible con Intel/AMD (no ARM)
- **~** `_mm_pause()` no disponible en ARM, pero target es x86-64 Windows

### Archivos afectados
- `Source/Common/memory/SharedMemory.cpp` — Two-phase spinlock

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| `std::mutex` | Priority inversion en audio thread |
| Lock-free SHM | Requiere reescribir todo el IPC, riesgo alto |
| Spinlock con solo `_mm_pause()` | Puede no ceder suficiente con 100+ threads |

---

## ADR-002: writeSlotTelemetry() — 1 Lock en Lugar de 2

**Fecha:** 2026-06-02 | **Estado:** Aceptado

### Contexto
`updateSharedTelemetry()` hacía `readSlot()` (1 acquireLock) + modificaba + `writeSlot()` (1 acquireLock) = 2 locks por bloque. Con 60+ Messengers = 120 lock acquisitions por ciclo.

### Decisión
Crear `writeSlotTelemetry()` que adquiere el lock 1 sola vez y escribe solo los campos de telemetría directamente, sin leer primero.

### Consecuencias
- **+** 50% menos adquisiciones de lock por Messenger (120→60 locks/bloque)
- **+** No afecta metadatos (slotIndex, trackName, colour, bus) — esos se escriben por separado
- **~** Si otro proceso modificó metadatos entre processBlock calls, no se sobrescriben (antes se sobrescribían por accidente)

### Archivos afectados
- `Source/Common/memory/SharedMemory.h` — Nueva función
- `Source/Common/memory/SharedMemory.cpp` — Implementación
- `Source/Common/memory/SlotRegistry.cpp` — `updateSharedTelemetry()` usa el nuevo método

---

## ADR-003: Backup File Diferido (~500ms)

**Fecha:** 2026-06-02 | **Estado:** Aceptado

### Contexto
`registerSlot()` escribía `saveSlotToBackupFile()` SIEMPRE (síncrono). Cuando FL Studio copia un Messenger a 100 tracks, cada nueva instancia llama a `registerSlot()` que escribe un archivo. 60+ operaciones de archivo simultáneas saturaban el I/O → FL Studio timeout → crash.

Además, `updateSlotBackupTelemetry()` se llamaba CADA 16 BLOQUES desde el audio thread, incluso con shared memory funcionando.

### Decisión
1. Backup en `registerSlot()` solo cuando SHM no está disponible
2. Backup diferido ~500ms desde el timer del Messenger (message thread)
3. Backup de telemetría solo cuando SHM falla (cada 64 bloques)

### Consecuencias
- **+** Cero file I/O durante inserción masiva de Messengers
- **+** Cero file I/O en audio thread (caso normal con SHM)
- **-** Si FL Studio crashea antes del backup diferido, el slot no persiste en disco
  (Mitigación: SHM + proyecto FL Studio preservan los datos)

### Archivos afectados
- `Source/Common/memory/SlotRegistry.cpp` — Backup condicional en registerSlot()
- `Source/Messenger/core/PluginProcessor.h` — `pendingBackupWrite_` flag
- `Source/Messenger/core/PluginProcessor.cpp` — Backup en timer + backup condicional

---

## ADR-004: DSP Throttling (kProcessInterval=4)

**Fecha:** 2026-06-03 | **Estado:** Aceptado

### Contexto
Con 100 Messengers, cada uno ejecutaba FFT (1024-point) + LUFS (2 biquads + mean square) + RMS + correlación en CADA bloque de audio. 100 × DSP completo por bloque ≈ saturación de CPU.

### Decisión
`TelemetryCollector.collect()` ahora tiene TWO-PATH:
- **Full DSP** (cada 4 bloques): FFT, LUFS, RMS, correlación, peak
- **Lightweight** (bloques intermedios): solo peak (1 loop, sin log, sin sqrt, sin FFT)

Además:
- `kFFTInterval` aumentado de 4 a 8 (FFT cada ~16ms)
- `kCalcInterval` (LUFS) aumentado de 10 a 20 (LUFS cada ~40ms)
- Shared memory writes reducidos de cada bloque a cada 2

### Consecuencias
- **+** ~75% menos CPU en pipeline DSP
- **+** 50% menos contención de spinlock
- **-** LUFS basado en 25% de los bloques de audio (precisión reducida pero aceptable para mentoría)
- **-** FFT spectrum se actualiza a ~60fps en vez de ~120fps (imperceptible para el usuario)

### Archivos afectados
- `Source/Messenger/telemetry/TelemetryCollector.h` — kProcessInterval, processCounter_, lastTelemetry_
- `Source/Messenger/telemetry/TelemetryCollector.cpp` — Two-path collect()
- `Source/Messenger/core/PluginProcessor.cpp` — SHM write cada 2 bloques

---

## ADR-005: Lazy Initialization en Constructores

**Fecha:** 2026-05-28 | **Estado:** Aceptado

### Contexto
FL Studio escanea plugins VST3 en un sandbox donde:
- `CreateFileMappingW` puede lanzar SEH exceptions que try/catch C++ no captura
- No hay message loop
- Cualquier crash → plugin deshabilitado permanentemente

### Decisión
Los constructores de ambos plugins (Messenger y MixCoach) están **completamente vacíos** de inicialización. Todo se inicializa LAZY:
- `SharedData::getInstance()` → en `prepareToPlay()` o `setStateInformation()`
- `LogHelper::setLogFile()` → primera vez que se necesita
- `juce::dsp::FFT` → en `prepare()`
- Slot registration → en `ensureSlotRegistered()`

### Consecuencias
- **+** Zero crashes durante escaneo VST3
- **+** Los plugins pasan "Verify installed plugins" de FL Studio
- **-** Más código de guard checks (if/else en cada función)
- **-** Posibles edge cases si prepareToPlay nunca se llama (cubiertos por timer fire-once)

### Archivos afectados
- `Source/Messenger/core/PluginProcessor.cpp` — Constructor vacío
- `Source/MixCoach/core/PluginProcessor.cpp` — Constructor vacío
- Ambos `PluginProcessor.h` — Flags `prepared_`, `slotRegistered_`

---

## ADR-006: slotRegistered_ Flag Booleana

**Fecha:** 2026-06-02 | **Estado:** Aceptado

### Contexto
`processBlock()` verificaba `if (slotIndex_ < 0)` y llamaba `ensureSlotRegistered()` en CADA bloque de audio. `ensureSlotRegistered()` tiene try/catch, SharedData::getInstance(), LogHelper::setLogFile(), etc. Con 60+ Messengers, decenas de llamadas redundantes por bloque.

### Decisión
Agregar `bool slotRegistered_{false}` que se establece a `true` después del primer registro exitoso. `processBlock()` ahora verifica la flag booleana (~1ns, sin branch misprediction).

### Consecuencias
- **+** Llamada a función + try/catch eliminados del hot path
- **+** Branch prediction-friendly (la flag cambia 1 vez en toda la vida del plugin)
- **~** La flag se resetea a false si el slot se libera (destructor)

### Archivos afectados
- `Source/Messenger/core/PluginProcessor.h` — Nueva flag
- `Source/Messenger/core/PluginProcessor.cpp` — Flag check en processBlock()

---

## ADR-007: Stack Buffer a Heap Vector

**Fecha:** 2026-06-02 | **Estado:** Aceptado

### Contexto
`pollTelemetryFromShared()` declaraba `SharedSlotEntry batchBuffer[kMaxSlots]` en el stack. Con `kMaxTracks=128` y `SharedSlotEntry` de ~2KB, el buffer ocupaba ~274KB en stack, superando el default de 1MB con 60+ llamadas anidadas → stack overflow.

### Decisión
Reemplazar `batchBuffer[128]` en stack con `std::vector<SharedSlotEntry>` en heap.

### Consecuencias
- **+** Stack seguro con cualquier número de slots
- **+** El vector se asigna una vez y se reusa (no heap alloc en cada ciclo)
- **-** Mínimo overhead de indirección (imperceptible)

### Archivos afectados
- `Source/MixCoach/core/PluginEditor.cpp` — batchBuffer de stack a vector

---

## ADR-008: MixCoach no Procesa Audio

**Fecha:** 2026-05-01 | **Estado:** Aceptado (decisión fundacional)

### Contexto
MixCoach empezó como un plugin de análisis. La decisión de NO procesar audio fue intencional desde el día 1.

### Decisión
MixCoach (plugin master) **NO tiene entrada de audio**. Solo recibe datos de los Messengers via IPC. El `AudioProcessor` de MixCoach:
- Tiene `processBlock()` vacío (solo pasa el audio sin tocarlo)
- No tiene buses de entrada de audio
- No tiene parámetros de audio processing

### Consecuencias
- **+** Claridad de propósito: mentor, no procesador
- **+** No hay riesgo de modificar el audio del usuario accidentalmente
- **+** Menos CPU en el master (solo UI + análisis)
- **-** No puede hacer análisis del master bus (los datos vienen de los Messengers)

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Procesar audio del master | El producto sería "otro iZotope", no un mentor |
| Análisis híbrido (Messenger + master) | Complejidad innecesaria, los Messengers ya dan datos por pista |

---

## 🔗 Referencias Cruzadas

| ADR | Documentos Relacionados |
|:---|------------------------|
| ADR-001 | `AI_CONTEXT.md` § Threading Architecture | `AI_CONTEXT_MAP.md` § 5. Flujo de Datos |
| ADR-002 | `IPC_CONTRACT.md` § 2.1 SharedSlotEntry | `AI_CONTEXT.md` § Fuentes de Datos |
| ADR-003 | `FL_STUDIO_BEHAVIORS.md` #3 (Copia Masiva) | `SAFE_EDIT_GUIDE.md` § 2 |
| ADR-004 | `AI_CONTEXT.md` § DSP Throttling | `AI_CONTEXT_MAP.md` § 5. Flujo de Datos |
| ADR-005 | `FL_STUDIO_BEHAVIORS.md` #1 (Sandbox) | `SAFE_EDIT_GUIDE.md` § 2 |
| ADR-006 | `FL_STUDIO_BEHAVIORS.md` #2 (processBlock antes) | `SAFE_EDIT_GUIDE.md` § 3 |
| ADR-007 | `AI_VISION.md` § UX/Sensación | `AI_RULES.md` Ley 5 (No heap en audio thread) |
| ADR-008 | `PRODUCT_VISION.md` (Mentor, no procesador) | `AI_VISION.md` § Anti-Visión |
| ADR-009 | `ERROR_PATTERNS.json` (COORDINATE_MATH_BUG) | `AI_CONTEXT.md` (Lecciones Sesión 4) |
| ADR-010 | `ERROR_PATTERNS.json` (SCALE_RANGE_MISMATCH) | `AI_CONTEXT.md` (Lecciones Sesión 4) |
| ADR-011 | `ERROR_PATTERNS.json` (HEADER_DECLARATION_MISMATCH) | `AI_CONTEXT.md` (Lecciones Sesión 4) |

---

## ADR-009: VU Meters Analógicos con Escala Elíptica

**Fecha:** 2026-06-06 | **Estado:** Aceptado

### Contexto
Los VU meters vintage en `AnalyzersPanelComponent` usaban coordenadas circulares para una escala que era elíptica (arco comprimido verticalmente con halfR = radius * 0.5). Esto causaba que ticks, números y aguja se dibujaran fuera del arco.

### Decisión
Para cualquier punto en el arco del VU meter:
- **X** = cx + cos(ángulo) × radius
- **Y** = cy + sin(ángulo) × halfR (con halfR = radius × 0.5)
- El centro del arco es (cx, cy), NO (cx, cy - halfR)
- El rango de la aguja debe coincidir con el rango de la escala (markEndAngle - startAngle), no con el rango completo del arco

### Consecuencias
- **+** Ticks, números y aguja coinciden exactamente con el arco
- **+** Fórmula documentada en ERROR_PATTERNS.json como COORDINATE_MATH_BUG
- **+** Se agregó `dbToNorm()` para mapeo no-lineal VU (no es lineal como un medidor dBFS)
- **-** Requiere verificación visual después de cualquier cambio en coordenadas de arco

### Archivos afectados
- `Source/MixCoach/UI/AnalyzersPanelComponent.cpp` — drawVUMeter() completo
- `AI_CONTEXT.md` — Lecciones aprendidas SESIÓN 4
- `ERROR_PATTERNS.json` — 3 nuevos patrones de error

---

## ADR-010: Peak Hold en VU Meters

**Fecha:** 2026-06-06 | **Estado:** Aceptado

### Contexto
Los VU meters analógicos no tenían indicación de pico, lo que dificulta ver transientes rápidas (el ojo humano no puede seguir una aguja que sube y baja en milisegundos).

### Decisión
Implementar peak hold con marcador tipo diamante en el arco:
- **Hold time**: 1.5 segundos desde el último pico
- **Decay rate**: 30 dB/segundo después del hold
- **Floor**: No puede caer por debajo del nivel actual (evita que el marcador se quede en -80dB en silencio)
- **Umbral visual**: Solo se dibuja si peakHold > -19dB (evita marcadores invisibles)

### Consecuencias
- **+** Transientes visibles aunque sean demasiado rápidas para la aguja
- **+** Timers de hold y decay independientes (no bloquean la aguja)
- **-** Se agregó `peakHold` y `peakHoldTimer` al struct `VUChannel`
- **-** Nueva función `dbToNorm()` para mapear dB a posición en el arco

### Archivos afectados
- `Source/MixCoach/UI/AnalyzersPanelComponent.h` — VUChannel struct
- `Source/MixCoach/UI/AnalyzersPanelComponent.cpp` — setLevels(), advanceVisuals(), drawVUMeter()

---

## ADR-011: Lecciones Aprendidas como Documentación de Primer Clase

**Fecha:** 2026-06-06 | **Estado:** Aceptado

### Contexto
Esta sesión tuvo 3 bugs de coordenadas que costaron 3 iteraciones de build/deploy. Las lecciones aprendidas estaban solo en la memoria de la IA, no documentadas para futuras sesiones.

### Decisión
Documentar las lecciones aprendidas en 3 lugares:
1. **AI_CONTEXT.md** — Sección "Lecciones Aprendidas" visible para cualquier IA
2. **ERROR_PATTERNS.json** — Patrones de error auto-aprendidos con fix strategies
3. **AI_SESSION_STATE.json** — Errores recientes con timestamp para trazabilidad

### Consecuencias
- **+** Cualquier IA futura encontrará los bugs antes de cometerlos
- **+** Trazabilidad completa de qué bugs ocurrieron y cómo se arreglaron
- **-** Un paso más antes de codear (leer las lecciones)

### Archivos afectados
- `AI_CONTEXT.md` — Nueva sección "🐛 Lecciones Aprendidas"
- `ERROR_PATTERNS.json` — 3 nuevos patrones auto-learned
- `AI_SESSION_STATE.json` — recent_errors y successful_fixes poblados

---

---

## ADR-012: Mentor, No Juez

**Fecha:** 2026-06-13 | **Estado:** Aceptado (decisión fundacional)

### Contexto
El usuario pidió un sistema que califique mezclas. Es técnicamente posible mostrar un score 0-100, ranking de pistas, o comparativas agresivas. Sin embargo, el fundador identificó que:
- Los ingenieros principiantes se intimidan con puntuaciones bajas
- Los ingenieros avanzados ignoran scores simplistas
- Un número no enseña nada — solo juzga

### Decisión
MixCoach **nunca juzga mezclas**. No hay:
- ❌ Scores visibles 0-100 al usuario
- ❌ Rankings de pistas
- ❌ Frases como "tu mezcla está mal"
- ❌ Comparativas agresivas contra referencia

En su lugar:
- ✅ Siempre sugiere con fundamento: "Prueba esto porque..."
- ✅ Da datos objetivos (dB, frecuencias, ratios) sin etiquetarlos como "bueno/malo"
- ✅ Enmarca todo como "podemos mejorar" no como "está mal"

### Consecuencias
- **+** El usuario nunca se siente juzgado — la tasa de retención es mayor
- **+** El foco está en aprender, no en "ganarle al score"
- **+** Los ingenieros avanzados respetan más las sugerencias que los números
- **-** Más difícil de implementar (es más fácil mostrar un número que explicar un concepto)
- **-** MixScore existe internamente para el LLM, pero NUNCA se muestra al usuario como número crudo

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Score visible 0-100 en la UI | El usuario optimizaría para el score, no para aprender |
| Ranking de pistas ("peor track: Kick") | Humillante, no constructivo |
| Semáforo rojo/verde en tracks | Útil solo si viene con explicación textual |

### Documentos relacionados
- `CHEFFX_BRAIN.md` § Core absoluto: enseñar a mejorar
- `AI_VISION.md` § Comportamiento del Coach (nunca critiques sin fundamento)
- `AI_EXAMPLES.md` Ejemplo 8 (usuario ignoró recomendación)

---

## ADR-013: La Referencia es el Norte

**Fecha:** 2026-06-13 | **Estado:** Aceptado (decisión fundacional)

### Contexto
MixCoach puede funcionar sin referencia cargada (usando targets genéricos por género). Pero el fundador observó que:
- Los mejores ingenieros SIEMPRE usan referencias
- Los principiantes mezclan "en el vacío" — no saben a qué suena "bien"
- Los targets genéricos son útiles pero no reemplazan una referencia real

### Decisión
La referencia es **parte fundamental del flujo de mezcla**, no un accesorio opcional:
- MixCoach debe PEDIR una referencia al inicio de cada sesión
- Las fases de mezcla se comparan constantemente contra la referencia
- El coach puede trabajar sin referencia SOLO en fases de organización y ganancia
- En fases de balance tonal, dinámica y espacial, la referencia es obligatoria

### Consecuencias
- **+** El usuario desarrolla el hábito profesional de usar referencias
- **+** Las recomendaciones son más precisas (comparan contra algo real, no contra un target genérico)
- **+** El usuario entiende QUÉ sonido está buscando
- **-** Sin referencia cargada, el coach no puede dar consejos de balance tonal precisos
- **-** Requiere que el usuario tenga o consiga una referencia (barrera de entrada)

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Targets genéricos siempre | No reemplazan el oído humano ni el contexto cultural de cada canción |
| Referencia opcional siempre | El usuario procrastina y nunca carga una — mezcla a ciegas |
| Solo referencia de LUFS | El balance tonal es igual de importante |

### Documentos relacionados
- `CHEFFX_BRAIN.md` § Propósito #5: "Comparar constantemente con referencias reales"
- `PRODUCT_VISION.md` FASE 1: Referencia
- `AI_EXAMPLES.md` Ejemplo 3 (referencia cargada)

---

## ADR-014: Sin Puntuaciones Visibles

**Fecha:** 2026-06-13 | **Estado:** Aceptado (decisión fundacional)

### Contexto
MixScore calcula internamente scores 0-100 por dominio (Gain, Tonal, Dynamics, Spatial). Es tentador mostrar estos números al usuario como "dashboard de salud de la mezcla". Sin embargo:
- Un número frío no enseña nada
- El usuario puede sentir que "fracasó" si el score es bajo
- El foco se desplaza de aprender a "subir el score"

### Decisión
MixScore existe **solo para consumo interno**:
- El LLM lo usa para priorizar recomendaciones
- El CoachEngine lo usa para decidir si avanzar de fase
- **NUNCA se muestra al usuario como número crudo**

Lo que sí se muestra:
- ✅ Texto descriptivo: "La ganancia está bien encaminada, pero 2 tracks están cerca del clipping"
- ✅ Recomendaciones priorizadas: "Empecemos por el Kick que necesita gain staging"
- ✅ Progreso cualitativo: "Vamos mejor que hace 5 minutos"

### Consecuencias
- **+** El usuario no se siente juzgado por un número
- **+** El foco está en entender QUÉ mejorar, no en CUÁNTO mejorar
- **+** El LLM tiene datos precisos para priorizar sin exponerlos crudos
- **-** El equipo de desarrollo pierde una métrica visible para debugging
  (Mitigación: logs internos en `LogHelper::writeToLog()`)

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Score visible en la UI | Viola el principio de mentor no juez |
| Score solo para debugging | Ya existe en logs |
| Score con "nota" tipo escolar | Peor aún — escuela no es el tono de MixCoach |

### Documentos relacionados
- `CHEFFX_BRAIN.md` § Core absoluto: "Enseñarte a mejorar como ingeniero"
- `MixScore.h` — Solo para uso interno del LLM
- `AI_VISION.md` § Anti-Visión: "Nunca critiques sin fundamento"

---

## ADR-015: Organización Antes que Procesamiento

**Fecha:** 2026-06-13 | **Estado:** Aceptado (decisión fundacional)

### Contexto
El fundador identifica el desorden como su principal obstáculo al abrir una sesión. Tracks sin nombre, colores aleatorios, buses mal ruteados. La mayoría de los plugins de audio ignoran este problema y van directo al análisis espectral.

### Decisión
La **organización de la sesión es una fase explícita del flujo de mezcla**, no un paso opcional:
- Antes de cualquier análisis de audio, MixCoach verifica que la sesión esté ordenada
- Si detecta desorden (tracks sin nombre, sin color, sin bus asignado), LO DICE primero
- La fase de organización (FASE 0.5 en PRODUCT_VISION.md) es obligatoria
- No se avanza a gain staging hasta que la sesión esté organizada

### Consecuencias
- **+** El usuario aprende que la organización es parte del proceso de mezcla
- **+** Las recomendaciones de gain/EQ/dinámica son más precisas sobre una base ordenada
- **+** Reduce la fricción del fundador al abrir sesiones ajenas
- **-** Puede frustrar al usuario que quiere "ir directo al grano"
  (Mitigación: coach puede saltar la fase si el usuario insiste, pero documenta la decisión)

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Ignorar el desorden | El fundador no puede ignorarlo — es parte de su identidad como ingeniero |
| Auto-organizar la sesión | El usuario debe aprender a hacerlo, no delegarlo |
| Organización como feature secundario | Sería inconsistente con la prioridad real del fundador |

### Documentos relacionados
- `CHEFFX_BRAIN.md` § Lo que odio: "Desorden. Sesiones sucias. Caos evitable."
- `PRODUCT_VISION.md` FASE 0.5: Escaneo
- `AI_EXAMPLES.md` Ejemplo 2 (sesión desordenada)

---

## ADR-016: Todo Depende del Género

**Fecha:** 2026-06-13 | **Estado:** Aceptado (decisión fundacional)

### Contexto
La mayoría de los plugins de análisis y mentoría usan valores universales: "el RMS ideal es -6dB", "el LUFS target es -14dB", "el crest factor saludable es 8-16dB". Pero el fundador sabe que estos valores cambian drásticamente según el género.

### Decisión
Cada recomendación de MixCoach debe considerar el género primero:
- No hay valores universales — hay valores POR GÉNERO
- `CoachEngine::getGenreProfile()` define targets específicos (LUFS, crest, headroom, offsets espectrales)
- El género se pregunta al inicio de la sesión (FASE 0: SETUP)
- Si el usuario no especifica género, el coach puede inferirlo del análisis espectral o preguntar
- Las recomendaciones explícitamente mencionan el contexto de género: "Para reggaetón, el 808 debe estar a -6dB..."

### Consecuencias
- **+** Consejos precisos y relevantes para el género que se mezcla
- **+** El usuario aprende que CADA GÉNERO tiene sus propias reglas
- **+** MixCoach se vuelve útil para múltiples géneros, no solo uno
- **-** Más complejidad en el motor de perfiles (mantener N perfiles de género)
- **-** Riesgo de perfil de género incorrecto si el usuario da un género equivocado
  (Mitigación: el coach puede detectar inconsistencias y preguntar "¿Seguro que es reggaetón? El espectro se ve más como rock")

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Valores universales | Serían incorrectos para la mayoría de los géneros |
| Detección automática de género | Tecnología no confiable, puede equivocarse y el usuario pierde confianza |
| Sin género — solo referencia | La referencia no siempre está disponible |

### Documentos relacionados
- `CHEFFX_BRAIN.md` § Mi filosofía: "Todo depende del género"
- `CoachEngine.h` — `getGenreProfile()`
- `AI_EXAMPLES.md` Ejemplo 5 (consejo género-específico)

---

## ADR-017: Enseñar, No Automatizar

**Fecha:** 2026-06-13 | **Estado:** Aceptado (decisión fundacional)

### Contexto
Es técnicamente posible que MixCoach ajuste automáticamente gains, EQ, compresión. Es la tentación de todo producto de IA musical. Sin embargo, el core absoluto de MixCoach es "enseñar al usuario a ser mejor ingeniero".

### Decisión
MixCoach **nunca automatiza decisiones de mezcla**:
- ❌ No ajusta faders automáticamente
- ❌ No aplica EQ automático
- ❌ No pone compresores por el usuario
- ❌ No tiene modo "auto-mix"

En su lugar:
- ✅ Recomienda valores exactos: "Sube 2dB en 60Hz con Q de 1.5"
- ✅ Explica por qué: "Porque el kick y el 808 están compitiendo en esa zona"
- ✅ Verifica el resultado: loop de corrección (recomendar → aplicar → escuchar → corregir)
- ✅ El usuario hace el ajuste con sus propias manos y desarrolla memoria muscular

### Consecuencias
- **+** El usuario desarrolla criterio y habilidad manual — no dependencia del plugin
- **+** Después de 10 sesiones, el usuario es NOTABLEMENTE mejor ingeniero
- **+** MixCoach se diferencia de iZotope, Sonible, etc. — no es un "auto-mix" más
- **-** Más lento que un auto-mix (el usuario tiene que hacer los ajustes)
  (Pero el objetivo NO es velocidad — es aprendizaje)
- **-** El usuario impaciente puede frustrarse
  (Mitigación: el coach puede mostrar "en automático sería X, pero prefiero que aprendas haciéndolo")

### Alternativas descartadas
| Alternativa | Razón |
|-------------|-------|
| Auto-mix completo | El usuario no aprende, se vuelve dependiente |
| Auto-mix con "explicación" | El usuario no desarrolla memoria muscular — no siente el ajuste |
| Modo automático para avanzados | Los avanzados también necesitan practicar — y prefieren control |

### Documentos relacionados
- `CHEFFX_BRAIN.md` § Core absoluto: "Enseñarte a mejorar como ingeniero"
- `CHEFFX_BRAIN.md` § Lo que NUNCA haría: "Auto-mix — el usuario debe aprender, no delegar"
- `AI_VISION.md` § Anti-Visión: "Modo auto-mix es lo que NUNCA seremos"
- `AI_EXAMPLES.md` Ejemplo 4 (loop de corrección)

---

## 🔗 Referencias Cruzadas — Actualizado

| ADR | Documentos Relacionados |
|:---|------------------------|
| ADR-001 | `AI_CONTEXT.md` § Threading Architecture | `AI_CONTEXT_MAP.md` § 5. Flujo de Datos |
| ADR-002 | `IPC_CONTRACT.md` § 2.1 SharedSlotEntry | `AI_CONTEXT.md` § Fuentes de Datos |
| ADR-003 | `FL_STUDIO_BEHAVIORS.md` #3 (Copia Masiva) | `SAFE_EDIT_GUIDE.md` § 2 |
| ADR-004 | `AI_CONTEXT.md` § DSP Throttling | `AI_CONTEXT_MAP.md` § 5. Flujo de Datos |
| ADR-005 | `FL_STUDIO_BEHAVIORS.md` #1 (Sandbox) | `SAFE_EDIT_GUIDE.md` § 2 |
| ADR-006 | `FL_STUDIO_BEHAVIORS.md` #2 (processBlock antes) | `SAFE_EDIT_GUIDE.md` § 3 |
| ADR-007 | `AI_VISION.md` § UX/Sensación | `AI_RULES.md` Ley 5 (No heap en audio thread) |
| ADR-008 | `PRODUCT_VISION.md` (Mentor, no procesador) | `AI_VISION.md` § Anti-Visión |
| ADR-009 | `ERROR_PATTERNS.json` (COORDINATE_MATH_BUG) | `AI_CONTEXT.md` (Lecciones Sesión 4) |
| ADR-010 | `ERROR_PATTERNS.json` (SCALE_RANGE_MISMATCH) | `AI_CONTEXT.md` (Lecciones Sesión 4) |
| ADR-011 | `ERROR_PATTERNS.json` (HEADER_DECLARATION_MISMATCH) | `AI_CONTEXT.md` (Lecciones Sesión 4) |
| ADR-012 | `CHEFFX_BRAIN.md`, `AI_VISION.md`, `AI_EXAMPLES.md` | Mentor, no juez |
| ADR-013 | `CHEFFX_BRAIN.md`, `PRODUCT_VISION.md`, `AI_EXAMPLES.md` | Referencia es el norte |
| ADR-014 | `CHEFFX_BRAIN.md`, `MixScore.h`, `AI_VISION.md` | Sin puntuaciones visibles |
| ADR-015 | `CHEFFX_BRAIN.md`, `PRODUCT_VISION.md`, `AI_EXAMPLES.md` | Organización primero |
| ADR-016 | `CHEFFX_BRAIN.md`, `CoachEngine.h`, `AI_EXAMPLES.md` | Todo depende del género |
| ADR-017 | `CHEFFX_BRAIN.md`, `AI_VISION.md`, `AI_EXAMPLES.md` | Enseñar, no automatizar |

---

*Documento de decisiones arquitectónicas — MixCoach Project — Actualizado 2026-06-13*

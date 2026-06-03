# 📜 DECISION_LOG.md — Registro de Decisiones Arquitectónicas

> **ADRs (Architecture Decision Records) del proyecto MixCoach.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

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

| ADR | Relacionado con | Documentos |
|:---|-----------------|------------|
| ADR-001 | `APPROVED_PATTERNS.md` P6 (Two-Phase Spinlock) | `RISK_MATRIX.md` 🔴 SharedMemory.h |
| ADR-002 | `APPROVED_PATTERNS.md` P6 | `RISK_MATRIX.md` 🔴 SlotRegistry.h |
| ADR-003 | `FL_STUDIO_BEHAVIORS.md` #3 (Copia Masiva) | `KNOWN_ERRORS.md` |
| ADR-004 | `PROJECT_PRIORITIES.md` P3 (Rendimiento) | `RISK_MATRIX.md` 🟠 TelemetryCollector |
| ADR-005 | `FL_STUDIO_BEHAVIORS.md` #1 (Sandbox) | `APPROVED_PATTERNS.md` P1 (Lazy Init) |
| ADR-006 | `FL_STUDIO_BEHAVIORS.md` #2 (processBlock antes) | `APPROVED_PATTERNS.md` P1 |
| ADR-007 | `KNOWN_ERRORS.md` (crash con 60+ tracks) | `RISK_MATRIX.md` 🔴 PluginEditor |
| ADR-008 | `PRODUCT_VISION.md` (Mentor, no procesador) | `AGENTS.md` |

---

*Documento de decisiones arquitectónicas — MixCoach Project*

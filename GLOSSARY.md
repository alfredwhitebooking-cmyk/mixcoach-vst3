# 📖 GLOSSARY.md — Vocabulario del Proyecto MixCoach

> **Glosario de términos técnicos: audio, código y arquitectura.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## 🎵 Términos de Audio

### Peak (dB)
**Qué es:** El valor máximo absoluto de la señal de audio en un bloque. Se mide en dBFS (decibels relative to Full Scale).
**En el código:** `computePeak()` — 1 loop, 1 abs(), 1 max() por sample.
**Dónde se usa:** `TelemetryCollector`, `CoachEngine` (detección de clipping).
**Rango típico:** `-100 dB` (silencio) a `0 dB` (full scale). Clipping > `-0.5 dB`.

### RMS (dB)
**Qué es:** Root Mean Square — la energía promedio de la señal. Representa el volumen percibido mejor que el peak.
**En el código:** `computeRMS()` — sum of squares, sqrt, log10.
**Dónde se usa:** `TelemetryCollector`, `CoachEngine` (nivel general).
**Rango típico:** `-30 dB` a `-6 dB` en una mezcla balanceada.

### Crest Factor (dB)
**Qué es:** La diferencia entre Peak y RMS (`peakDb - rmsDb`). Mide cuánta "dinámica" tiene una señal.
**En el código:** `computeCrestFactor()` — simple resta.
**Dónde se usa:** `CoachEngine::analyzeDynamicsReal()`.
**Rango típico:**
- `< 6 dB` → Muy comprimido (posible over-compression)
- `6-12 dB` → Rango normal para mezcla moderna
- `12-18 dB` → Dinámico (clásico, jazz)
- `> 24 dB` → Muy dinámico (necesita compresión)

### Correlation / Fase
**Qué es:** Mide qué tan similares son los canales izquierdo y derecho. `1.0` = idénticos (mono). `0.0` = completamente diferentes. `-1.0` = invertidos (mal).
**En el código:** `computeCorrelation()` — sumProduct / sqrt(sumSqL * sumSqR).
**Dónde se usa:** `CoachEngine::analyzePhaseReal()`.
**Rango:**
- `> 0.7` → ✅ Bueno, compatible mono
- `0.3 a 0.7` → ⚠️ Ancho estéreo significativo
- `< 0.3` → 🔴 Posible problema de fase
- `< 0.0` → 🔴🔴 Fase invertida, CORREGIR

### FFT / Spectrum
**Qué es:** Fast Fourier Transform — convierte la señal de audio del dominio del tiempo al dominio de la frecuencia.
**En el código:** `computeSpectrum()` — 1024-point FFT con ventana Hann.
**Parámetros:** `kFFTOrder = 10` (1024-point), `kFFTSize = 1024`, `kSpectrumBins = 512`.
**Frecuencia:** Cada 8 bloques (~16ms a 48kHz/96samples).
**Dónde se usa:** `SpectrographComponent`, `CoachEngine::analyzeTonalBalanceReal()`.

### LUFS (EBU R128)
**Qué es:** Loudness Units relative to Full Scale — estándar EBU R128 / ITU BS.1770 para medir loudness percibido.
**Componentes:**
| Medida | Ventana | Actualización |
|--------|---------|:------------:|
| **Momentary** | 400ms | Cada ~40ms |
| **Short-term** | 3s | Cada ~160ms |
| **Integrated** | Acumulativa | Cada ~160ms |
| **Range (LRA)** | Distribución | Cada ~160ms |

**En el código:** `LoudnessMeter` — K-weighting filter (2 biquads) + mean square accumulation.
**Target típico:** `-14 LUFS` (integrated) para streaming.
**Dónde se usa:** `LUFSMeter` (UI), `CoachEngine::analyzeDynamicsReal()`.

### K-Weighting Filter
**Qué es:** Filtro de 2 etapas definido por ITU BS.1770-4:
1. **Shelving +4dB @ 1.5kHz** (pre-filter)
2. **High-pass 38Hz Butterworth** (RLB weighting)

**En el código:** `LoudnessMeter::applyKFilter()` — 2 biquads IIR por sample.
**Importante:** Es IIR, debe procesar CADA sample para mantener estado. No se puede saltar.

### Headroom
**Qué es:** El espacio entre el pico más alto de la mezcla y 0 dBFS (digital clipping). Target: `-6 dB`.
**En el código:** `CoachEngine::analyzeGainStagingReal()`.
**Frase típica:** "El rango ideal es -6 dB a -3 dB de pico en el master."

### Enmascaramiento Espectral
**Qué es:** Cuando dos pistas ocupan el mismo rango de frecuencia y compiten por la atención del oyente.
**En el código:** `CoachEngine::analyzeSpectralMaskingReal()` — pairwise comparison de espectros por bandas críticas.
**Banda crítica:** Escala Bark simplificada para 1024-FFT @ 48kHz (~46.875 Hz/bin).

### Bandas Espectrales (para análisis tonal)
**En el código:** `CoachEngine::analyzeTonalBalanceReal()`

| Banda | Bins | Frecuencia | Detección |
|-------|:----:|:-----------|:----------|
| Sub-bass | 0-1 | 20-47 Hz | Exceso si domina sobre mids |
| Low-bass | 1-3 | 47-141 Hz | |
| Bass | 3-6 | 141-281 Hz | Falta si < -35 dBFS |
| Low mids | 6-13 | 281-609 Hz | |
| Mids | 13-25 | 609-1172 Hz | |
| High mids | 25-53 | 1.17-2.48 kHz | |
| Presence | 53-106 | 2.48-4.97 kHz | Exceso si > -15 dBFS |
| Highs | 106-213 | 4.97-9.98 kHz | Mezcla opaca si < -40 dBFS |
| Air | 213-426 | 9.98-19.97 kHz | Exceso si > -15 dBFS |

---

## 🧩 Términos de Código y Arquitectura

### Slot
**Qué es:** Un "asiento" numerado que representa una pista con Messenger en el `SlotRegistry`.
**Capacidad máxima:** `kMaxSlots = 128` (definido en `Constants.h`).
**Ciclo de vida:**
```
registerSlot() → Slot activo (envía telemetría)
    → releaseSlot() → Slot liberado (Messenger removido)
    → Backup file permanece en disco para re-detección
```

### SlotRegistry
**Qué es:** El corazón del IPC. Singleton que mantiene el registro de todos los slots activos.
**Archivo:** `Source/Common/memory/SlotRegistry.h`
**Métodos clave:**
- `registerSlot()` — Asigna slot libre, modo SHM o backup
- `releaseSlot()` — Libera slot
- `syncFromShared()` — Lee shared memory → actualiza registry local
- `forceFullSync()` — Lee backup files + SHM → sincronización completa
- `forEachActive()` — Itera slots activos con callback
- `updateSharedTelemetry()` — Escribe telemetría a shared memory

### SharedMemoryManager
**Qué es:** Maneja la memoria compartida entre procesos via Windows `CreateFileMappingW`.
**Archivo:** `Source/Common/memory/SharedMemory.h`
**Estructura:**
```
SharedMemoryBlock:
  ├── SharedMemoryHeader (lock, changeCount, structVersion)
  └── SharedSlotEntry slots[kSharedMaxSlots]
```
**Lock:** Spinlock two-phase (`_mm_pause()` + `Sleep(0)`)

### SharedSlotEntry
**Qué es:** Estructura POD de 128 bytes que se comparte entre procesos via shared memory.
**Campos:** active, slotName[64], slotColourARGB, busType + telemetría (peak, RMS, correlation, crest, LUFS, FFT).
**⚠️ Importante:** Si se modifica, INCREMENTAR `kCurrentStructVersion`.

### Backup File
**Qué es:** Archivo binario en `%LOCALAPPDATA%/MixCoach/SlotBackup/slot_N.bin`.
**Formato:** Magic number "MCOC" + structVersion + slotIndex + SharedSlotEntry + CRC32.
**Propósito:** Fallback garantizado cuando la shared memory entre DLLs separadas falla.

### TelemetryBuffer
**Qué es:** Ring buffer lock-free (512 slots) para comunicación thread-safe entre audio thread y UI thread.
**Archivo:** `Source/Common/types/TelemetryData.h`
**Operaciones:** `push()` (writer), `latest()` (reader), lock-free.

### Spinlock
**Qué es:** Mecanismo de sincronización ultra-rápido para shared memory. NO es un mutex.
**Cómo funciona:**
```cpp
while (lock_.exchange(1, memory_order_acquire) == 1) {
    _mm_pause();  // Fase 1: ~1μs sin syscall
    // si sigue ocupado tras ~1000 intentos:
    Sleep(0);     // Fase 2: context switch
}
```
**⚠️ NUNCA usar desde el audio thread.**

### Two-Phase Spinlock
**Qué es:** Optimización del spinlock para reducir context switches con 60+ threads.
**Fase 1:** `_mm_pause()` × 1000 (~1μs, no hay syscall)
**Fase 2:** `Sleep(0)` (solo si el lock sigue ocupado)

### ProcessBlock
**Qué es:** El método que JUCE llama en CADA bloque de audio (~2ms a 48kHz/96samples).
**Restricciones:**
- ❌ NO heap allocation
- ❌ NO file I/O
- ❌ NO locks bloqueantes (mutex, spinlock)
- ✅ Stack allocation, atomics, ring buffer push

### Collect (TelemetryCollector)
**Qué es:** El método que recopila toda la telemetría de un bloque de audio.
**Frecuencia:** Full DSP cada 4 bloques (~8ms). Peak cada bloque (~2ms).
**Pipeline completo (cada 4 bloques):**
1. `computePeak()` — 1 loop, 1 abs(), max()
2. `computeRMS()` — 1 loop, sum of squares, sqrt, log10
3. `computeCorrelation()` — 1 loop, 3 multiplicaciones
4. `computeCrestFactor()` — 1 resta
5. `computeSpectrum()` — FFT 1024-point + window Hann + magnitudes
6. `LoudnessMeter.process()` — 2 biquads por sample + mean square

### CoachEngine
**Qué es:** El motor de mentoría. Analiza telemetría y genera mensajes en el chat.
**Archivo:** `Source/MixCoach/engine/CoachEngine.h`
**Frecuencia de análisis:** Cada ~8 segundos (periodicAnalysis).
**Fases:** Welcome → GainStaging → Organisation → TonalBalance → Dynamics → Spatial.

### PhaseManager
**Qué es:** Máquina de estados que controla la progresión de fases de mentoría.
**Archivo:** `Source/MixCoach/engine/PhaseManager.h`
**Transición:** Manual (comando `/next` del usuario).

---

## 🔧 Términos Técnicos

| Término | Significado | Relevancia |
|---------|-------------|:----------:|
| **VST3** | Formato de plugin de audio (Steinberg) | Formato de salida del build |
| **Bundle** | Directorio estructurado con extensión `.vst3` | Lo que FL Studio espera |
| **CreateFileMappingW** | API de Windows para memoria compartida entre procesos | Mecanismo IPC principal |
| **Spinlock** | Lock de espera activa (no duerme) | Sincronización SHM |
| **SEH** | Structured Exception Handling (Windows) | Puede crashear sin try/catch |
| **MSBuild** | Build system de Microsoft | Generator correcto para Release |
| **C1001** | Internal Compiler Error con Ninja + Release | Error conocido, usar MSBuild |
| **LNK2019** | Unresolved external symbol | Falta .cpp en CMakeLists.txt |
| **PCH** | Precompiled Header | Desactivado por conflicto con JUCE |
| **Lock-free** | Algoritmo sin locks (atómicos) | `TelemetryBuffer`, `AudioRingBuffer` |
| **Lazy init** | Inicialización diferida (no en constructor) | Patrón para evitar crashes en escaneo |
| **Two-path DSP** | DSP completo en 1 de N bloques | Optimización de CPU con 100+ tracks |

---

## 🔗 Referencias

| Documento | Contiene estos términos en contexto |
|-----------|-------------------------------------|
| `PRODUCT_VISION.md` | Términos de producto y visión |
| `APPROVED_PATTERNS.md` | Patrones de código con estos conceptos |
| `AI_CONTEXT.md` | Arquitectura completa del sistema |
| `IPC_CONTRACT.md` | Contrato formal de comunicación IPC |
| `DECISION_LOG.md` | Decisiones que definieron estos términos |

---

*Glosario del proyecto — MixCoach*

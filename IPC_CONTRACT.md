# 🖧 IPC Contract — Messenger ↔ MixCoach

> **Contrato formal de comunicación inter-procesos entre los plugins VST3.**
> **Versión:** 2.0 | **Última actualización:** 2026-05-31

---

## 1. Visión General

Messenger y MixCoach son **DLLs VST3 separados** que se ejecutan en el mismo proceso del DAW (por instancia de plugin), pero en **diferentes pistas** y por tanto en diferentes instancias del plugin dentro del mismo proceso (o procesos separados, dependiendo del DAW).

### Canales de comunicación

| Canal | Dirección | Latencia | Persistencia | Uso |
|-------|-----------|----------|-------------|-----|
| **Shared Memory** (Windows `CreateFileMappingW`) | Messenger → MixCoach | ~1μs | Volátil (sesión) | Telemetría en tiempo real (picos, RMS, FFT, fase, LUFS) |
| **Backup Files** (`%LOCALAPPDATA%/MixCoach/SlotBackup/slot_N.bin`) | Messenger → MixCoach | ~5ms | Persistente (disco) | Metadatos (nombre, color, bus) + telemetría de respaldo garantizado |

### Arquitectura

```
┌─────────────────────────┐     Shared Memory (CreateFileMappingW)     ┌─────────────────────────┐
│  Messenger (Pista N)    │ ──────────────────────────────────────────→│  MixCoach (Master)      │
│                         │    SlotEntry (POD, ~8KB/slot)              │                         │
│  processBlock() cada    │    - peakL, peakR, rmsL, rmsR              │  timer 30fps:            │
│  ~2ms (48kHz, 96 samps) │    - correlation, crestFactor              │    syncFromShared()      │
│                         │    - fftData[512]                          │    detectNewMsngrs()     │
│  Escribe:               │    - lufsIntegrated, lufsShortTerm         │    updateMessengers()    │
│    - shared memory (ráp)│    - lufsRange, truePeak, momentario       │    updateAnalyzers()     │
│    - backup file (gar)  │    - slotName[64], slotColourARGB, busType │                         │
│                         │                                            │                         │
│                         │     Backup Files (SLOT_N.bin)              │                         │
│                         │ ──────────────────────────────────────────→│  fallback sync:          │
│                         │    (mismos datos, escritura garantizada)    │    loadSlotsFromBackup() │
└─────────────────────────┘                                            └─────────────────────────┘
```

---

## 2. Estructuras de Datos Compartidas

### 2.1 SharedSlotEntry (POD-only — 128 bytes)

```cpp
// File: Source/Common/memory/SharedMemory.h
// Versión: kCurrentStructVersion (uint32_t)

struct SharedSlotEntry {
    bool     active;             // Slot ocupado?
    char     slotName[64];       // Nombre de pista (null-terminated)
    uint32_t slotColourARGB;     // Color ARGB de la pista
    int      busType;            // BusType enum (0=None, 1=Drums, ..., 6=FX)

    // Telemetría (protegida por spinlock en el MemoryMappedFile)
    float peakL;                 // Peak instantáneo izquierdo (dB)
    float peakR;                 // Peak instantáneo derecho (dB)
    float rmsL;                  // RMS izquierdo (dB)
    float rmsR;                  // RMS derecho (dB)
    float correlation;           // Correlación de fase (-1.0 a 1.0)
    float crestFactor;           // Crest Factor (dB)
    float lufsIntegrated;        // LUFS integrado (EBU R128)
    float lufsShortTerm;         // LUFS short-term (3s sliding window)
    float lufsRange;             // Rango LUFS (LRA)
    float truePeak;              // True Peak (dBTP)
    float momentary;             // LUFS momentáreo (400ms)
};
```

**⚠️ Importante:** Al modificar este struct, INCREMENTAR `kCurrentStructVersion` en `SharedMemory.h`. Versiones diferentes causan reinicialización silenciosa de la shared memory.

### 2.2 Backup File (`slot_N.bin`)

Formato binario con magic number + struct serializado:

```
┌──────────────────────────────┐
│ uint32_t magic = 0x4D434F43  │  ← "MCOC" (MixCoach)
│ uint32_t structVersion       │  ← kCurrentStructVersion
│ uint32_t slotIndex           │  ← Número de slot
│ SharedSlotEntry data         │  ← 128 bytes de datos
│ uint32_t checksum            │  ← CRC32 simple
└──────────────────────────────┘
```

Ubicación: `%LOCALAPPDATA%/MixCoach/SlotBackup/slot_{N}.bin`

---

## 3. Ciclo de Vida de un Slot

```
Messenger instanciado en pista
    │
    ▼
1. REGISTRATION (processBlock → primer prepareToPlay)
    ├─ SlotRegistry::registerSlot(nombre, color, bus)
    │   ├─ Asigna slotIndex libre (round-robin)
    │   ├─ Marca active=true en shared memory
    │   ├─ Crea backup file slot_N.bin
    │   └─ Retorna slotIndex
    │
    ▼
2. DATA COLLECTION (cada processBlock, ~2ms)
    ├─ TelemetryCollector analiza buffer de audio
    │   ├─ Peak (sample-by-sample)
    │   ├─ RMS (sliding window)
    │   ├─ FFT (Hann 512, cada 4 bloques ~8ms)
    │   ├─ Phase Correlation
    │   ├─ Crest Factor
    │   └─ LUFS (EBU R128, 3 ventanas)
    │
    ├─ Escribe a shared memory (cada bloque)
    └─ Escribe a backup file (cada 8 bloques ~16ms)
    │
    ▼
3. SYNCHRONIZATION (MixCoach timer 30fps)
    ├─ syncFromShared() → lee shared memory → actualiza SlotRegistry local
    │   ├─ Frecuencia: ~3fps (cada 10 ticks)
    │   └─ Propósito: telemetría fresca + detección de nuevos slots
    │
    ├─ detectNewMessengers() → escanea backup files
    │   ├─ Frecuencia: ~3fps
    │   └─ Propósito: detectar slots que no están en shared memory
    │
    └─ loadSlotsFromBackupFiles() → recuperación completa
        ├─ Frecuencia: bajo demanda (forceFullSync)
        └─ Propósito: recuperación tras desconexión/reconexión
    │
    ▼
4. DEREGISTRATION (Messenger removido o DAW cerrado)
    ├─ releaseSlot(slotIndex)
    │   ├─ active=false en shared memory
    │   └─ Backup file permanece (para re-detección futura)
    │
    └─ restoreSlotFromBackup() (opcional)
        └─ Si el Messenger se reinscribe, restaura nombre/color/bus
```

---

## 4. Timing Constraints

| Operación | Frecuencia | Thread | Prohibido |
|-----------|-----------|--------|-----------|
| Messenger processBlock() | Cada bloque de audio (~2ms @ 48kHz/96samples) | Audio | Heap allocation, file I/O, lock acquisition |
| Messenger backup file write | Cada 8 bloques (~16ms) | Audio (lazy) | NO hacer en processBlock; diferir a timer |
| MixCoach syncFromShared() | ~3fps (cada 330ms) | Timer UI | NO bloquear más de 1ms |
| MixCoach updateMessengers() | ~10fps | Timer UI | NO hacer I/O |
| MixCoach updateAnalyzers() | ~30fps (cada tick) | Timer UI | Sólo repaint, sin I/O |

### Garantías de consistencia

| Dato | Staleness máximo | Prioridad |
|------|-----------------|-----------|
| Nombre de pista | ~330ms (3fps) | Backup file (persistente) |
| Color de pista | ~330ms | Backup file |
| Bus assignment | ~330ms | Backup file |
| Peak values | ~33ms (1 tick) | Shared memory (fresco) |
| RMS values | ~100ms (3 ticks) | Shared memory |
| FFT spectrum | ~100ms | Shared memory (cada 4 bloques) |
| LUFS integrated | ~1s | Shared memory |
| Phase correlation | ~100ms | Shared memory |

---

## 5. Locking & Thread Safety

### Shared Memory Spinlock

```cpp
// Adquirir: acquireLock(timeoutMs=100)
// Liberar: releaseLock()
//
// El spinlock usa std::atomic<uint32_t> en la cabecera de shared memory.
// Timeout de 100ms para evitar deadlocks si un proceso crasheó.
// NUNCA adquirir desde el audio thread de Messenger.
```

### TelemetryBuffer (lock-free)

```cpp
// File: Source/Common/types/TelemetryData.h
//
// Ring buffer lock-free con:
// - writeIndex: std::atomic<uint64_t> (solo el escritor incrementa)
// - readIndex: std::atomic<uint64_t> (solo el lector incrementa)
//
// Garantías:
// - Writer nunca espera (overwrite si lleno)
// - Reader siempre ve datos consistentes (memoria atómica)
// - Sin locks, sin espera, sin excepciones
```

---

## 6. Manejo de Errores

| Escenario | Comportamiento | Recuperación |
|-----------|---------------|-------------|
| Shared memory no existe (1er Messenger) | `CreateFileMappingW` lo crea | Automática |
| Slot lleno (32/32) | `registerSlot()` retorna -1 | Liberar slots o reducir instancias |
| Messenger crashea sin deregistrar | Slot queda stale (`active=true` sin escritura) | Timeout de 5s en MixCoach marca como inactivo |
| Backup file corrupto | `loadSlotsFromBackupFiles()` salta el archivo | Re-crear desde shared memory |
| Struct version mismatch | Shared memory se reinicia | Backup files prevalecen para metadatos |

---

## 7. Version History

| Versión | Fecha | Cambios |
|---------|-------|---------|
| 1.0 | 2026-05-01 | Versión inicial |
| 1.1 | 2026-05-15 | Agregados campos LUFS a SharedSlotEntry |
| 2.0 | 2026-05-31 | Documentación formal del contrato IPC |

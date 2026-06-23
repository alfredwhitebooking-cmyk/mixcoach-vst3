# 🖧 IPC Contract V3 — Messenger ↔ MixCoach

> **Contrato formal de comunicación inter-procesos entre los plugins VST3.**
> **Versión:** 3.0 (V3 Sensor-Cerebro) | **Última actualización:** 4 junio 2026

---

## 1. Visión General

Messenger y MixCoach son **DLLs VST3 separados** que se ejecutan como plugins en el mismo DAW. En V3, Messenger es un **sensor** que solo transmite audio RAW + identidad. MixCoach es el **cerebro** que analiza TODO desde el Master.

### Canales de comunicación V3

| Canal | Dirección | Latencia | Persistencia | Contenido |
|-------|-----------|----------|-------------|-----------|
| **SharedMemory V6** (`Local\MixCoachMemV3`) | Messenger → MixCoach | ~1μs | Sesión | Identidad: slotIndex, trackName, colourARGB, active, bus |
| **SharedAudioMemory V1** (`Local\MixCoachAudioMemV3`) | Messenger → MixCoach | ~1μs | Sesión | Audio RAW mono: 128 slots × 4096 samples ring buffer |

**Eliminado en V3:** Backup files, TelemetryBuffer, telemetría per-slot (RMS, Peak, FFT, LUFS, correlación).

### Arquitectura V3

```
┌─────────────────────────┐  SharedMemory V6 (5 campos)  ┌─────────────────────────┐
│  Messenger (Sensor)     │ ────────────────────────────→│  MixCoach (Cerebro)     │
│                         │   slotIndex, trackName,       │                         │
│  processBlock():        │   colourARGB, active, bus    │  Background Worker:      │
│  1. RAW audio passthru  │                               │  • forceFullSync()      │
│  2. writeSamples() →    │  SharedAudioMemory V1         │  • readSamples()        │
│     SharedAudioMemory   │ ────────────────────────────→│  • RMS/Peak real        │
│  3. Heartbeat + ident   │  128 slots × 4096 samples    │                         │
│                         │  Lock-free ring buffer       │  CoachEngine:           │
│  SIN telemetría, SIN    │  (volatile int64_t + barrera)│  • AudioAnalyzer Master │
│  análisis, SIN medición │                               │  • TrackAudioResult     │
└─────────────────────────┘                               └─────────────────────────┘
```

---

## 2. Estructuras de Datos Compartidas

### 2.1 SharedSlotEntry V6 (POD-only — ~80 bytes)

```cpp
// File: Source/Common/memory/SharedMemory.h
// Versión: kCurrentStructVersion = 6

struct SharedSlotEntry {
    int      slotIndex;       // Quién soy (0-127), -1 = libre
    char     trackName[64];   // Nombre de pista (null-terminated)
    uint32_t colourARGB;      // Color ARGB de la pista
    bool     active;          // El Messenger está vivo?
    int      bus;             // BusType enum (0=None, 1=Drums, ..., 6=FX)
};
```

**⚠️ Importante:** Al modificar este struct, INCREMENTAR `kCurrentStructVersion` en `SharedMemory.h`.

**Eliminado de V5:** `peakL`, `peakR`, `rmsL`, `rmsR`, `correlation`, `crestFactor`, `lufsIntegrated`, `lufsShortTerm`, `lufsRange`, `truePeak`, `momentary`, `fftData`.

### 2.2 SharedAudioMemory V1

```cpp
// File: Source/Common/memory/SharedAudioMemory.h
// File mapping separado: "Local\\MixCoachAudioMemV3"

struct SharedAudioSlot {
    volatile int64_t writePos;         // Solo Messenger incrementa
    volatile int64_t readPos;          // Solo MixCoach incrementa
    float            buffer[4096];     // Samples mono RAW (~85ms @ 48kHz)
};

struct SharedAudioBlock {
    SharedAudioHeader header;           // initialized flag
    SharedAudioSlot   slots[128];       // ~2 MB total
};
```

**Sincronización:** `volatile int64_t` + `_WriteBarrier()` / `_ReadBarrier()` (MSVC/x86).

**Escritura (Messenger audio thread):**
```cpp
int64_t wp = slot.writePos;
for (int i = 0; i < numSamples; ++i)
    slot.buffer[(wp + i) % kAudioBufferSize] = data[i];
_WriteBarrier();
slot.writePos = wp + numSamples;  // Publicación atómica
```

**Lectura (MixCoach background worker):**
```cpp
int64_t wp = slot.writePos;
_ReadBarrier();  // Entre lectura de posición y lectura del buffer
int64_t rp = slot.readPos;
int64_t available = wp - rp;
if (available > kAudioBufferSize) available = kAudioBufferSize;  // Overrun protection
// Leer samples del buffer...
slot.readPos = rp + toRead;
```

---

## 3. Ciclo de Vida de un Slot (V3)

```
Messenger instanciado en pista
    │
    ▼
1. REGISTRATION
    ├─ PluginProcessor::ensureSlotRegistered()
    │   ├─ SharedData::getInstance() (singleton thread-safe)
    │   ├─ SlotRegistry::registerSlot(nombre, color, bus)
    │   │   ├─ Asigna slotIndex libre (scan 0..127)
    │   │   ├─ Escribe SharedSlotEntry a SharedMemory V6
    │   │   └─ Retorna slotIndex
    │   └─ slotIndex_ almacenado para futuros writes
    │
    ▼
2. RAW AUDIO STREAMING (cada processBlock, ~2ms)
    ├─ Audio pasa INTACTO (ni un cálculo por muestra)
    ├─ Sum estéreo → mono
    ├─ writeSamples() a SharedAudioMemory (64-sample chunks)
    ├─ Heartbeat: lastHeartbeatMs_ = now
    └─ registry.setActive(slotIndex_, true)
    │
    ▼
3. SYNCHRONIZATION (MixCoach Background Worker ~10Hz)
    ├─ forceFullSync() → lee SharedMemory V6 → actualiza SlotRegistry local
    ├─ forEachActive():
    │   ├─ readSamples() desde SharedAudioMemory
    │   ├─ Computa RMS + Peak real
    │   └─ updateTrackAudioResult() → cache thread-safe
    └─ checkStaleSlots() → marca inactivos si no hay heartbeat
    │
    ▼
4. DEREGISTRATION (Messenger removido o DAW cerrado)
    ├─ PluginProcessor destructor → slotIndex_ = -1
    ├─ SharedMemory V6: active = false
    └─ SharedAudioMemory: writePos / readPos sin cambios
```

---

## 4. Timing Constraints V3

| Operación | Frecuencia | Thread | Prohibido |
|-----------|-----------|--------|-----------|
| Messenger processBlock() (RAW passthrough) | Cada bloque (~2ms @ 48kHz) | Audio | Heap alloc, file I/O, locks |
| Messenger writeSamples() a SHM | Cada bloque | Audio | Lock acquisition |
| MixCoach SharedAudioMemory read | ~10Hz (cada 100ms) | Background | Bloquear más de 5ms |
| MixCoach forceFullSync() | ~5s | Background | Bloquear más de 10ms |
| MixCoach smoothMeters() | 60fps (cada 16ms) | Message (Timer) | I/O, locks bloqueantes |
| MixCoach CoachEngine analysis | ~8s | Message (Timer) | I/O pesada |

### Garantías de consistencia V3

| Dato | Fuente | Staleness máximo |
|------|--------|-----------------|
| Nombre de pista | SharedMemory V6 | ~5s (forceFullSync) |
| Color de pista | SharedMemory V6 | ~5s |
| Bus assignment | SharedMemory V6 | ~5s |
| Audio RAW (RMS/Peak per-track) | SharedAudioMemory | ~100ms (10Hz) |
| Master FFT/LUFS/fase | AudioAnalyzer | ~500ms (processBlock) |

---

## 5. Thread Safety V3

### SharedMemory V6 (Spinlock)

```cpp
// Adquirir: acquireLock(timeoutMs=100)
// Liberar: releaseLock()
//
// Spinlock two-phase: _mm_pause() ×1000 → Sleep(0)
// NUNCA adquirir desde el audio thread.
```

### SharedAudioMemory (Lock-free)

```cpp
// Sin locks. Usa volatile int64_t + _WriteBarrier/_ReadBarrier.
// Solo en x86/x64 (Windows). No portable a ARM.
// Overrun protegido: available limitado a kAudioBufferSize.
```

### TrackAudioResult Cache (std::atomic)

```cpp
// Escrito por Background Worker (release ordering)
// Leído por CoachEngine desde message thread (acquire ordering)
// memory_order_release/acquire para consistencia
```

---

## 6. Manejo de Errores V3

| Escenario | Comportamiento | Recuperación |
|-----------|---------------|-------------|
| SharedMemory V6 no existe | `CreateFileMappingW` lo crea | Automática |
| SharedAudioMemory no existe | `CreateFileMappingW` lo crea | Automática |
| Slot lleno (128/128) | `registerSlot()` retorna -1 | Liberar slots |
| Messenger crashea | `checkStaleSlots()` marca inactivo | Reconexión automática |
| Struct version mismatch | SharedMemory se reinicia | forceFullSync() recupera |
| Audio ring buffer overrun | readSamples() limita a kAudioBufferSize | Datos parciales |

---

## 7. Version History

| Versión | Fecha | Cambios |
|---------|-------|---------|
| 1.0 | 2026-05-01 | Versión inicial (V1) |
| 1.1 | 2026-05-15 | Agregados campos LUFS (V2) |
| 2.0 | 2026-05-31 | Documentación formal V2 |
| **3.0** | **2026-06-04** | **V3 Sensor-Cerebro: SharedMemory V6 (solo identidad) + SharedAudioMemory (audio RAW). Eliminados backup files y telemetría per-slot.** |

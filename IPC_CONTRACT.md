# 🖧 IPC Contract V4 — Messenger ↔ MixCoach (Session-Isolated)

> **Contrato formal de comunicación inter-procesos entre los plugins VST3.**
> **Versión:** 4.0 (V4 Session-Isolated) | **Última actualización:** 20 julio 2026

---

## 1. Visión General

Messenger y MixCoach son **DLLs VST3 separados** que se ejecutan como plugins en el mismo DAW. En V4, cada sesión de DAW tiene su propio **GUID de sesión** que aísla los datos IPC entre proyectos.

### Canales de comunicación V4

| Canal | Nombre | Dirección | Latencia | Contenido |
|-------|--------|-----------|----------|-----------|
| **SessionDiscovery** (`Local\MixCoachSessionV3`) | Fijo (siempre el mismo) | MixCoach → Messenger | ~1μs | GUID de sesión: `"MixCoach_<32-hex>"` + heartbeat |
| **SharedMemory** (`Local\MixCoach_<GUID>_Slots`) | GUID-derived | Messenger → MixCoach | ~1μs | Identidad: slotIndex, trackName, colourARGB, active, bus, trackType, muted, soloed, faderDb, panValue |
| **SharedAudioMemory V2** (`Local\MixCoach_<GUID>_Audio`) | GUID-derived | Messenger → MixCoach | ~1μs | Audio RAW estéreo: 128 slots × 4096 samples ring buffer + overrun counters |

### Arquitectura V4

```
MixCoach (Brain)                          Messenger (Sensor x128)
     │                                            │
     │  1. generateSessionGUID()                  │
     │  2. SessionDiscovery.publishGUID()         │
     │     └── Local\MixCoachSessionV3            │
     │                                            │
     │  3. SharedData.initializeSession(GUID)     │  3. SessionDiscovery.readGUID()
     │     └── Local\MixCoach_<GUID>_Slots        │  4. SharedData.initializeSession(GUID, false)
     │     └── Local\MixCoach_<GUID>_Audio        │     └── Local\MixCoach_<GUID>_Slots
     │                                            │     └── Local\MixCoach_<GUID>_Audio
     │                                            │
     │  ┌─ MixCoachBgService ──────────────────┐  │  5. processBlock():
     │  │  10x/s: readStereoSamples()          │  │     - writeStereoSamples() → Audio SHM
     │  │  FFT, RMS, Peak, envelope,           │  │     - setActive(true) → Identity SHM
     │  │  crest, stereo width                 │  │     - NO locks, NO I/O, NO heap alloc
     │  └──────────────────────────────────────┘  │
     │                                            │
     │  (NUNCA se detiene aunque la UI esté       │  (Sensor puro: solo identidad + audio RAW)
     │   cerrada — vive en el Processor)          │
```

---

## 2. SessionDiscovery Protocol

### 2.1 Discovery Shared Memory (nombre fijo)

```cpp
// File: Source/Common/memory/SharedMemory.h
// Nombre fijo: "Local\MixCoachSessionV3"
// Siempre el mismo — no depende de GUID

struct SessionDiscoveryBlock {
    char     sessionGUID[48];   // "MixCoach_<32-hex>" + null
    uint32_t timestampMs;       // Heartbeat de MixCoach
    uint32_t active;            // 1 = MixCoach vivo, 0 = sesión terminada
    uint32_t protocolVer;       // 1 = versión actual del protocolo
};
```

### 2.2 Ciclo de vida

```
MixCoach startup:
  1. generateSessionGUID() → "MixCoach_A1B2C3D4..."
  2. SessionDiscovery.initialize() → CreateFileMapping(Local\MixCoachSessionV3)
  3. SessionDiscovery.publishGUID(guid) → escribe SessionDiscoveryBlock
  4. SharedData.initializeSession(guid, isBrain=true) → crea mappings GUID-derived

Messenger startup:
  1. SessionDiscovery.initialize() → OpenFileMapping(Local\MixCoachSessionV3)
  2. SessionDiscovery.readGUID() → espera hasta que MixCoach publique
  3. SharedData.initializeSession(guid, isBrain=false)
  4. RegisterSlot en la nueva session

MixCoach shutdown:
  1. SessionDiscovery.closeSession() → active=0, GUID limpio
```

---

## 3. Estructuras de Datos Compartidas

### 3.1 SharedSlotEntry V9 (POD-only — ~80 bytes)

```cpp
// File: Source/Common/memory/SharedMemory.h
// Versión: kCurrentStructVersion = 9
// Mapeo: Local\MixCoach_<GUID>_Slots

struct SharedSlotEntry {
    int      slotIndex;       // Quién soy (0-127), -1 = libre
    char     trackName[64];   // Nombre de pista (null-terminated)
    uint32_t colourARGB;      // Color ARGB de la pista
    int      active;          // bool como int (señal de vida)
    int      bus;             // BusType enum (0=None, 1=Drums, ..., 6=FX)
    int      trackType;       // TrackType explícito del Messenger (V7)
    int      muted;           // 1 = mute activo (V8)
    int      soloed;          // 1 = solo activo (V8)
    float    faderDb;         // Nivel de fader en dB (V9)
    float    panValue;        // Pan -1.0 a +1.0 (V9)
};
```

### 3.2 SharedAudioSlotStereo V2 (con overrun counter)

```cpp
// File: Source/Common/memory/SharedAudioMemoryV2.h
// Mapeo: Local\MixCoach_<GUID>_Audio
// Sincronización: std::atomic<int64_t> con memory_order_release/acquire

struct SharedAudioSlotStereo {
    std::atomic<int64_t> writePosL{0};    // Writer left (Messenger)
    std::atomic<int64_t> writePosR{0};    // Writer right
    std::atomic<int64_t> readPosL{0};     // Reader left (MixCoach)
    std::atomic<int64_t> readPosR{0};     // Reader right
    std::atomic<uint32_t> overrunCount{0}; // V4: contador de overruns
    float bufferL[kAudioBufferSize];      // Ring left (4096 samples)
    float bufferR[kAudioBufferSize];      // Ring right
};
```

---

## 4. Timing Constraints V4

| Operación | Frecuencia | Thread | Prohibido |
|-----------|-----------|--------|-----------|
| Messenger processBlock() (passthrough + write) | Cada bloque (~2ms @ 48kHz) | Audio | Heap alloc, file I/O, locks |
| Messenger writeStereoSamples() a Audio SHM | Cada bloque | Audio | Lock acquisition (lock-free ring) |
| MixCoach SharedAudioMemoryV2 read | ~20Hz (cada 50ms) | Background (BgService) | Bloquear más de 5ms |
| MixCoach forceFullSync() / SessionDiscovery | ~5s / ~1s | Background | I/O pesada |
| MixCoach smoothMeters() | 60fps (cada 16ms) | Message (Timer) | I/O, locks bloqueantes |

### Ring Buffer Throughput

| Frecuencia proyecto | Escritura por ciclo (512 samples @ 48kHz) | Lectura BgService | Margen |
|:-------------------:|:------------------------------------------:|:-----------------:|:------:|
| 44.1 kHz | 512 samples / 2ms = 44,100 samples/s | 4,096 / 25ms = 163,840 samples/s | **+271%** ✅ |
| 48 kHz | 512 samples / 2ms = 48,000 samples/s | 4,096 / 25ms = 163,840 samples/s | **+241%** ✅ |
| 96 kHz | 512 samples / 1ms = 96,000 samples/s | 4,096 / 25ms = 163,840 samples/s | **+71%** ✅ |

---

## 5. Thread Safety V4

### SharedMemory (Spinlock)

```cpp
// V4: timeout reducido de 100ms→5ms para no bloquear el message thread.
// Si el lock no se libera en 5ms, la lectura se omite (datos > frescos > bloqueo).
bool acquireLock(int timeoutMs = 5);  // InterlockedExchange + _mm_pause
void releaseLock();                    // InterlockedExchange(0)
```

### SharedAudioMemoryV2 (Lock-free con std::atomic)

```cpp
// V4: std::atomic<int64_t> con memory_order_release/acquire.
// Garantías cross-process en MSVC/Windows x64.
// Overrun detection: si available > kAudioBufferSize, incrementa
// overrunCount atómico (solo diagnóstico, no bloquea).

Escritura (Messenger audio thread):
    slot.writePosL.store(wpL + numSamples, std::memory_order_release);

Lectura (MixCoach BgService):
    int64_t wpL = slot.writePosL.load(std::memory_order_acquire);
    if (available > kAudioBufferSize) {
        slot.overrunCount.fetch_add(1, std::memory_order_relaxed);
        available = kAudioBufferSize;
    }
```

---

## 6. Manejo de Errores V4

| Escenario | Comportamiento | Recuperación |
|-----------|---------------|-------------|
| SessionDiscovery no existe (MixCoach no arrancó) | Messenger espera (retry en prepareToPlay) | Automática cuando MixCoach publique GUID |
| SessionDiscovery activo pero SHM GUID no existe | MixCoach no terminó initializeSession() | Reintento exponencial en Messenger |
| Shared memory GUID no disponible | Messenger usa legacy (nombres fijos) | Retry en prepareToPlay |
| Overrun en ring buffer | overrunCount incrementado, samples más recientes preservados | Ajustar tasa de consumo |
| Messenger crashea | checkStaleSlots() marca inactivo (5s) | Reconexión automática |
| Dos DAWs simultáneos | GUIDs diferentes → datos aislados completamente ✅ | Sin contaminación cruzada |
| Struct version mismatch | SharedMemory se reinicia | forceFullSync() recupera |

---

---

## 8. VST3 Generic Limitations

> ⚠️ **IMPORTANTE:** Esta sección documenta qué NO puede hacer un plugin VST3
genérico (como MixCoach) a través de la API estándar de Steinberg. Estas
limitaciones son inherentes a la arquitectura VST3 y no se pueden resolver sin
integraciones específicas por DAW o sin acompañamiento de una aplicación
companion independiente.

### 8.1 Limitaciones Fundamentales del VST3

| # | Capacidad | ¿Posible en VST3 genérico? | Alternativa MixCoach |
|:-:|:----------|:--------------------------:|:---------------------|
| 1 | **Enumerar pistas del proyecto** ❌ | No. Un VST3 solo ve su propio audio entrante/saliente y sus parámetros. No hay API para listar pistas del host. | Los **Messengers** (plugin sensor por pista) son la solución arquitectónica: cada Messenger se inserta manualmente en cada pista y reporta su identidad via IPC. |
| 2 | **Leer plugins insertados** ❌ | No. Un VST3 no puede inspeccionar la cadena de inserts de otras pistas ni la suya propia. | `PluginScanner` escanea los directorios de VST3 instalados en el sistema (`C:\Program Files\Common Files\VST3`), pero NO sabe qué plugins están insertados activamente en cada pista. El usuario puede **confirmar manualmente** los plugins que usa en cada pista via la UI del MixCoach. |
| 3 | **Mutear/Solear pistas** ❌ | No. La API VST3 no expone el channel strip del host. | Messenger reporta su estado interno de mute/solo (`muted`, `soloed` en `SharedSlotEntry`), pero NO es un espejo autoritativo del DAW. Son valores que el usuario configura manualmente en la UI del Messenger. |
| 4 | **Leer fader/pan del DAW** ❌ | No. VST3 no expone los niveles de fader ni paneo del mixer del host. | Messenger tiene campos `faderDb` y `panValue` en `SharedSlotEntry`, pero son metadatos manuales, no reflejan el channel strip real del DAW. |
| 5 | **Automatizar parámetros del host** ❌ | No. Un VST3 no puede escribir automatización en pistas del host. | Las recomendaciones del coach son **textuales**: "Baja el fader -3 dB". El usuario aplica manualmente. La verificación mide el cambio en la métrica de audio para inferir si se aplicó. |
| 6 | **Leer transporte del host** ✅ PARCIAL | `AudioPlayHead::getCurrentPosition()` (VST3) o `getPlayHead()` (JUCE) proveen: tempo, time signature, posición (PPQ/segundos), estado play/stop, loop. NO proveen: marcadores, secciones, compases, color de región, nombres de track del playlist. | MixCoach usa `getPlayHead()` para obtener BPM, posición, time sig y loop state. Para secciones musicales (verso/coro), usa **detección heurística**: cambios de >2 compases en la posición son tratados como cambio de sección. |
| 7 | **Render/Bounce detection** ✅ PARCIAL | `processBlock()` recibe `isNonRealtime()` (true durante bounce). NO hay evento "el usuario empezó a renderizar". | MixCoach usa `isNonRealtime()` para activar `renderSafe_` en el `ReferenceAudioPlayer`, evitando que la referencia se imprima en el bounce. |
| 8 | **Sample rate / Buffer size** ✅ | `prepareToPlay()` recibe sample rate y block size. Se actualiza automáticamente si cambian. | Todos los analizadores se re-inicializan correctamente en `prepareToPlay()`. |
| 9 | **UI del plugin** ✅ COMPLETO | VST3 permite ventanas flotantes completas (editors personalizados). JUCE abstrae completamente esto. | MixCoach tiene UI completa: split-view coaching, analyzers, chat, reportes. |
| 10 | **Persistencia de estado** ✅ | `getStateInformation()` / `setStateInformation()` guardan/restauran estado del plugin entre sesiones del DAW. | MixCoach guarda sesión, roles, setup, referencias vía `session_memory.json` + estado VST3. |

### 8.2 Implicaciones para el Coaching

Cada recomendación del coach debe reflejar honestamente estas limitaciones:

| Afirmación del coach | Forma correcta (VST3-aware) |
|:---------------------|:----------------------------|
| "Aplicaste exactamente +2 dB" | "La métrica cambió aproximadamente +2 dB después de tu ajuste" |
| "Este plugin está insertado en tu pista" | "Este plugin parece estar disponible en tu sistema" |
| "La voz se perdió en el coro" | "En este intervalo analizado, la voz perdió presencia relativa" |
| "Tu mezcla mejoró X%" | "El score de similitud cambió X puntos bajo esta métrica" |
| "Tu fader está a -6 dB" | "El fader reportado por Messenger indica -6 dB (valor manual)" |

### 8.3 Comparativa: VST3 Genérico vs. Integración Específica

| Capacidad | VST3 Genérico | Integración por DAW (ej: FL Studio API) | App Companion |
|:----------|:-------------:|:----------------------------------------:|:-------------:|
| Leer pistas del proyecto | ❌ No | ✅ Sí (API scripting) | ✅ Sí |
| Leer inserts activos | ❌ No | ✅ Sí | ✅ Sí (OSC/MIDI) |
| Mutear/Solear tracks | ❌ No | ✅ Sí | ✅ Sí |
| Leer fader/pan real | ❌ No | ✅ Parcial | ✅ Sí |
| Escribir automatización | ❌ No | ✅ Sí | ✅ Sí |
| Leer marcadores/secciones | ❌ No | ✅ Sí | ❌ No |
| Funciona sin instalación extra | ✅ Sí | ❌ Requiere script | ❌ Requiere app |
| Multi-DAW | ✅ Universal | ❌ Un solo DAW | ❌ Un solo DAW |
| Mantenimiento | ✅ Bajo | ❌ Alto (API specs cambian) | ❌ Medio |

### 8.4 Recomendación Arquitectónica

Mantener MixCoach como **VST3 genérico** con Messengers como sensores por pista
(arquitectura actual). Las limitaciones VST3 se mitigan con:

1. **Mensajes honestos del coach** — usando el lenguaje de "métrica cambió" no
   "aplicaste exactamente" (ver §8.2)
2. **Confirmación manual** — el usuario puede decirle a MixCoach qué plugins
   tiene en cada pista via la UI
3. **API de scripting opcional** — para DAWs que la soporten (Reaper,
   Cubase), un script companion puede leer inserts reales y enviarlos vía
   IPC al MixCoach
4. **App companion** — para integración profunda (producto futuro), una
   aplicación independiente que se comunique con MixCoach vía puerto local

---

## 7. Version History

| Versión | Fecha | Cambios |
|---------|-------|---------|
| 1.0 | 2026-05-01 | Versión inicial (V1) |
| 1.1 | 2026-05-15 | Agregados campos LUFS (V2) |
| 2.0 | 2026-05-31 | Documentación formal V2 |
| 3.0 | 2026-06-04 | V3 Sensor-Cerebro: SharedMemory V6 (solo identidad) + SharedAudioMemory (audio RAW). Eliminados backup files y telemetría per-slot. |
| **4.0** | **2026-07-20** | **V4 Session-Isolated: GUID de sesión negociado via SessionDiscovery + nombres GUID-derived para SharedMemory/SharedAudioMemory + overrun counters en ring buffer. Aislamiento completo entre proyectos DAW.** |

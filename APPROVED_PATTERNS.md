# ✅ APPROVED_PATTERNS.md

> **Patrones de código permitidos, desaconsejados y prohibidos.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## 1. Patrones Permitidos (Recomendados)

### P1 — Lazy Initialization Segura
```cpp
// ✅ PERMITIDO: Inicialización lazy en PluginProcessor/PluginEditor
void MixCoachAudioProcessor::ensureSharedData() {
    if (sharedData_ != nullptr) return;  // Guard check
    try {
        sharedData_ = &SharedData::getInstance();
        if (shm_ != nullptr) shm_->reconnect();
    } catch (const std::exception& e) {
        LogHelper::writeToLog("[MixCoach] ensureSharedData failed: " + String(e.what()));
    }
}
```
**Cuándo:** Plugin lifecycle, constructores, prepareToPlay.  
**Por qué:** Evita crashes durante el escaneo VST3 (FL Studio escanea sin prepareToPlay).

### P2 — Try/Catch en Constructores de UI
```cpp
// ✅ PERMITIDO: Constructor de componente UI con try/catch
MiComponente() {
    try {
        addAndMakeVisible(titleLabel_);
        titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    } catch (const std::exception& e) {
        LogHelper::writeToLog("[MiComponente] Exception: " + String(e.what()));
    } catch (...) {
        LogHelper::writeToLog("[MiComponente] Unknown exception");
    }
}
```
**Cuándo:** Cualquier constructor de componente JUCE.  
**Por qué:** JUCE puede lanzar excepciones en entornos VST3 con recursos limitados.

### P3 — SmoothValue para Meters UI
```cpp
// ✅ PERMITIDO: SmoothValue con ballistics ajustables
SmoothValue peakLeft_;
peakLeft_.setBallistics(5.0f, 100.0f);  // attack=5ms, release=100ms
peakLeft_.setTarget(newValue, 30.0);     // sampleRate=30fps (timer)
float displayValue = peakLeft_;          // operator float()
```
**Cuándo:** Todos los meters y barras de UI.  
**Por qué:** Suavizado exponencial evita flickering y da sensación profesional.

### P4 — Background Worker Thread para I/O Pesada
```cpp
// ✅ PERMITIDO: Background worker para operaciones I/O
class BackgroundWorker : public juce::Thread {
    void run() override {
        while (!threadShouldExit()) {
            {
                juce::GenericScopedLock lock(bgLock_);
                registry.syncFromShared();        // IPC read
                registry.forceFullSync();          // Backup files
            }
            wait(5000);  // ~5s entre ciclos
        }
    }
};
```
**Cuándo:** File I/O, backup sync, health checks.  
**Por qué:** No bloquear el timer de UI (30fps) con operaciones lentas.

### P5 — Lock-Free Ring Buffer para IPC Thread-Safe
```cpp
// ✅ PERMITIDO: TelemetryBuffer (lock-free circular buffer)
TelemetryBuffer buffer;
buffer.push(latestTelemetry);    // Writer (audio thread)
auto data = buffer.latest();     // Reader (UI timer)
```
**Cuándo:** Comunicación audio thread → UI thread.  
**Por qué:** Sin locks, sin espera, sin excepciones.

### P6 — Spinlock Two-Phase para Shared Memory
```cpp
// ✅ PERMITIDO: Spinlock con _mm_pause() + Sleep(0) two-phase
bool acquireLock(int timeoutMs = 100) {
    int pauseCount = 0;
    while (lock_.exchange(1, std::memory_order_acquire) == 1) {
        if (++pauseCount < 1000) {
            _mm_pause();  // Fase 1: ~1μs sin syscall
        } else {
            Sleep(0);     // Fase 2: context switch si muy contenido
        }
        if (pauseCount > timeoutMs * 1000) return false;  // Timeout
    }
    return true;
}
```
**Cuándo:** Protección de shared memory entre procesos.  
**Por qué:** `_mm_pause()` evita context switches innecesarios con 60+ threads.

### P7 — ChangeBroadcaster para Notificar UI
```cpp
// ✅ PERMITIDO: ChangeBroadcaster para comunicación Processor → Editor
class MixCoachAudioProcessor : public juce::AudioProcessor,
                                public juce::ChangeBroadcaster { ... };

// En el timer del Editor:
processor.addChangeListener(this);
void changeListenerCallback(ChangeBroadcaster*) override {
    // UI thread: actualizar componentes
    updateAllPanels();
}
```
**Cuándo:** Cuando el Processor necesita notificar al Editor desde un hilo diferente.  
**Por qué:** Thread-safe, evita race conditions.

### P8 — Timer con Throttling por Frecuencia
```cpp
// ✅ PERMITIDO: timerCallback con throttling inteligente
void timerCallback() override {
    tickCounter_++;

    if (tickCounter_ % 10 == 0) {   // ~3fps (cada 10 ticks a 30fps)
        registry.syncFromShared();
        detectNewMessengers();
    }
    if (tickCounter_ % 3 == 0) {    // ~10fps
        updateMessengers();
    }
    // ~30fps (cada tick)
    updateAnalyzers(registry);
    repaint();
}
```
**Cuándo:** Timers de UI que ejecutan múltiples operaciones.  
**Por qué:** Diferentes frecuencias para diferentes tipos de update.

---

## 2. Patrones Desaconsejados (Evitar)

| Patrón | Alternativa recomendada | Por qué |
|--------|------------------------|---------|
| `Sleep(0)` en spinlock sin `_mm_pause()` | Two-phase: `_mm_pause()` + `Sleep(0)` | `Sleep(0)` causa context switch (~1-15μs) en cada iteración |
| File I/O en `processBlock()` | Backup diferido a timer/message thread | File I/O es no-determinista, bloquea audio thread |
| `std::vector` en stack de `processBlock()` | `std::array` o miembro pre-asignado | `std::vector` hace heap allocation |
| Buffer grande en stack (`float[128]`) | `std::vector` en heap (miembro de clase) | Stack overflow con ~274KB (como se vio en el crash de 60+ tracks) |
| `std::lock_guard` en audio thread | Lock-free ring buffer o atomics | `std::mutex` puede causar priority inversion |
| Forward declarations de tipos IPC | Include completo | SharedSlotEntry necesita definición completa para acceso a miembros |
| Lógica de mentoría en `paint()` | Separar en métodos de update | `paint()` se llama en cada repaint, no es lugar para análisis |

---

## 3. Patrones Prohibidos (Nunca)

### ❌ PX1 — Heap Allocation en Audio Thread
```cpp
void processBlock(...) {
    // ❌ PROHIBIDO: Heap allocation en audio thread
    auto* temp = new float[256];
    std::vector<float> data(512);
    std::string s = "hello";
    juce::String js = "world";  // Puede heap-alloc internamente
}
```

### ❌ PX2 — Lógica de UI en Archivos Engine/
```cpp
// En MixCoach/engine/CoachEngine.h
// ❌ PROHIBIDO: Dependencia de UI en clase de engine
#include "MixCoach/UI/AnalyzersPanelComponent.h"  // PROHIBIDO
class CoachEngine {
    AnalyzersPanelComponent* panel_;  // PROHIBIDO
};
```

### ❌ PX3 — Lógica de Audio en Archivos UI/
```cpp
// En MixCoach/UI/AnalyzersPanelComponent.cpp
// ❌ PROHIBIDO: Análisis de audio en UI
void paint(Graphics& g) override {
    computeFFT(buffer_, fftData_);   // PROHIBIDO
    analyzePhase(samples_);           // PROHIBIDO
    drawSpectrum(g);
}
```

### ❌ PX4 — Hardcodear Colores de Bus
```cpp
// ❌ PROHIBIDO: Colores hardcodeados
juce::Colour busColor = juce::Colour(0xFF8B5CF6);  // PROHIBIDO
if (busType == 1) drawColour(0xFF8B5CF6);           // PROHIBIDO

// ✅ CORRECTO: Usar MixCoachTheme
juce::Colour busColor = MixCoachTheme::getBusColour(BusType::Drums);
```

### ❌ PX5 — Modificar SharedSlotEntry sin Incrementar Versión
```cpp
// ❌ PROHIBIDO: Modificar struct sin actualizar versión
struct SharedSlotEntry {
    bool active;
    // Agregar nuevo campo AQUÍ sin incrementar kCurrentStructVersion
    float newField;  // ❌ Corrompe shared memory existente
};
```

### ❌ PX6 — Llamadas a Funciones DAW/OS en Audio Thread
```cpp
void processBlock(...) {
    // ❌ PROHIBIDO: Llamadas bloqueantes en audio thread
    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);  // PROHIBIDO
    LogHelper::writeToLog("block processed");  // PROHIBIDO (file I/O)
    juce::Time::getMillisecondCounter();  // DESACONSEJADO (syscall)
}
```

### ❌ PX7 — Mutex/Spinlock Adquirido Desde Audio Thread
```cpp
void processBlock(...) {
    // ❌ PROHIBIDO: Cualquier lock blocking en audio thread
    sharedMemory_.acquireLock();  // PROHIBIDO (spinlock blocking)
    std::lock_guard lock(mutex_); // PROHIBIDO (mutex blocking)
}
```

---

## 📋 Resumen Rápido

| Contexto | ✅ Permitido | ❌ Prohibido |
|----------|-------------|-------------|
| **Audio thread** | Stack alloc, atomics, ring buffer push | Heap alloc, file I/O, locks, syscalls |
| **UI timer (30fps)** | Leer datos, repaint, updates visuales | File I/O síncrono, spinlock blocking, O(n²) |
| **Background worker** | File I/O, sync IPC, health checks | Acceso a UI, paint(), audio processing |
| **Constructor UI** | Try/catch, addAndMakeVisible, layout | Lógica de audio, IPC, file I/O |
| **Engine/ (mentoría)** | Análisis de telemetría, generación de tips | Dependencias de UI, audio processing |
| **Memory/ (IPC)** | Spinlock, shared memory, backup files | Dependencias de audio o UI |
| **paint()** | Drawing commands, colores del theme | Lógica de negocio, IPC, audio analysis |

---

*Documento de gobernanza para agentes IA — MixCoach Project*

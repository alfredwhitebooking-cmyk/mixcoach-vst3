# ⚙️ 02 — CODING RULES

> **El código de conducta del código. Reglas que todo agente debe respetar al escribir, modificar o eliminar código.**
>
> Si una línea de código viola estas reglas, debe refactorizarse antes de hacer merge.
>
> **Versión:** 1.0 | **Última actualización:** 26 junio 2026

---

## 📋 Índice

1. [Regla de Oro: No Romper Nada](#1-regla-de-oro-no-romper-nada)
2. [Patrones Obligatorios](#2-patrones-obligatorios)
3. [Prohibiciones Absolutas](#3-prohibiciones-absolutas)
4. [Estilo y Formato](#4-estilo-y-formato)
5. [Manejo de Memoria](#5-manejo-de-memoria)
6. [Thread Safety](#6-thread-safety)
7. [IPC y Compatibilidad](#7-ipc-y-compatibilidad)
8. [Logging y Debug](#8-logging-y-debug)
9. [Testing](#9-testing)
10. [Naming Conventions](#10-naming-conventions)
11. [Estructura de Archivos](#11-estructura-de-archivos)
12. [Checklist Pre-Merge](#12-checklist-pre-merge)

---

## 1. Regla de Oro: No Romper Nada

**La regla más importante de todo el proyecto:**

> Si el código compila y pasa tests antes de tu cambio, debe seguir compilando y pasando tests después.

| Acción | Permitido | Prohibido |
|:-------|:----------|:----------|
| Agregar campo nuevo a struct IPC | ✅ Al final, con version bump | ❌ En medio, sin version bump |
| Refactorizar función privada | ✅ Con tests | ❌ Sin tests de regresión |
| Cambiar firma de función pública | ✅ Si actualizas TODOS los callers | ❌ Dejar callers rotos |
| Eliminar archivo | ✅ Si `git grep` no muestra referencias | ❌ Si hay includes olvidados |
| Modificar `TrackState` | ✅ Si no rompe serialización existente | ❌ Si `sizeof` cambia sin version bump |

---

## 2. Patrones Obligatorios

### 2.1 Stack-allocated structs > clases dinámicas

```cpp
// ✅ CORRECTO: Stack allocation, POD, sin heap
struct TrackWorkOrder {
    int slotIndex;
    float priority;
    juce::String description;
};

// ❌ INCORRECTO: Heap allocation innecesaria
class TrackWorkOrder {
    std::unique_ptr<InternalData> data_;
    // ...
};
```

### 2.2 `JUCE_TRY` / `JUCE_CATCH` en paint()

Todo `paint()` y `resized()` debe envolverse en `JUCE_TRY`:

```cpp
void paint(juce::Graphics& g) override
{
    JUCE_TRY
    {
        auto area = getLocalBounds();
        // render code...
    }
    JUCE_CATCH_EXCEPTION
    {
        jassertfalse; // No silenciar en debug
    }
}
```

### 2.3 SmoothValue para animaciones

Nunca usar interpociones manuales. Usar `SmoothValue`:

```cpp
// ✅ CORRECTO
SmoothValue meterValue;
meterValue.setTarget(0.75f);

// ❌ INCORRECTO
float current = 0.0f;
// ... frame a frame con lerp manual
```

### 2.4 NVI (Non-Virtual Interface) para métodos polimórficos

```cpp
// ✅ CORRECTO
class AnalyzerBase {
public:
    void process() { processImpl(); }
private:
    virtual void processImpl() = 0;
};

// ❌ INCORRECTO
class AnalyzerBase {
public:
    virtual void process(); // público y virtual
};
```

### 2.5 Timer-driven UI, nunca callback-driven

```cpp
// ✅ CORRECTO: 60fps timer
void timerCallback() override {
    updateMeters();
    repaint();
}

// ❌ INCORRECTO: callback desde otro thread
void onAnalyzerUpdate(float value) {
    setValue(value); // podría venir de audio thread
}
```

---

## 3. Prohibiciones Absolutas

### 🚫 En el Audio Thread

| Prohibición | Riesgo |
|:------------|:-------|
| ❌ `new` / `delete` / `malloc` / `free` | Heap allocation → xrun |
| ❌ `std::vector::push_back` | Realloc → xrun |
| ❌ `std::map` / `std::unordered_map` | Tree/hash → O(n) + alloc |
| ❌ `std::mutex::lock()` | Contention → xrun |
| ❌ `std::cout` / `printf` | File I/O → xrun |
| ❌ `juce::Logger::writeToLog` | File I/O → xrun |
| ❌ Acceso a `SharedMemory` con spinlock | Deadlock potencial |
| ❌ Cualquier `dynamic_cast` | Costo no determinista |
| ❌ Cualquier excepción no capturada | Crash del motor de audio |

### 🚫 Globales

| Prohibición | Alternativa |
|:------------|:------------|
| ❌ Variables globales no constantes | Pasar por constructor o SharedData |
| ❌ Singletons mutables | Dependency injection |
| ❌ `static` no-const en class scope | Miembro de instancia |

### 🚫 En todo el proyecto

| Prohibición | Razón |
|:------------|:------|
| ❌ `std::cout` / `printf` | Usar `MIXCOACH_LOG` |
| ❌ `using namespace std` | Contaminación de namespace |
| ❌ `auto` para tipos no obvios | Preferir tipos explícitos en API pública |
| ❌ `#pragma once` sin `#ifndef` guard | JUCE requiere include guards |
| ❌ Cast a `any` | Type safety. Usar `std::variant` o herencia |
| ❌ Números mágicos | Constantes con nombre en `Constants.h` |
| ❌ C-style casts | Usar `static_cast`, `dynamic_cast` (con check) |
| ❌ Raw pointers compartidos | `std::unique_ptr`, `juce::WeakReference` |

---

## 4. Estilo y Formato

### 4.1 ClangFormat

El proyecto usa `.clang-format`. Siempre ejecutar antes de commit:

```bash
# Los archivos ya están formateados automáticamente
# No ejecutar clang-format manualmente si el pre-commit lo hace
```

### 4.2 Nombres

```cpp
// Clases: PascalCase
class MixCoachEngine {};
class TrackFeedCore {};

// Métodos: camelCase
void updateAnalyzers();
float computeScore();

// Variables miembro: snake_case con trailing underscore
int slotIndex_;
float currentGain_;
bool isActive_;

// Constantes: kPascalCase
constexpr int kMaxSlots = 128;
constexpr float kDefaultThreshold = -18.0f;

// Enums: PascalCase con prefijo
enum class TrackRole { kKick, kSnare, kHiHat, k808, kBass };
enum class MixPhase { kSetup, kIdentity, kCoaching, kRefinement, kReport };
```

### 4.3 Comentarios

```cpp
// ✅ CORRECTO
// ═══════════════════════════════════════════════════════════
//  computeSpectralProfile — Análisis FFT por bands
//  Recibe: sample buffer, sample rate
//  Retorna: energy[0..kNumBands) normalizado
// ═══════════════════════════════════════════════════════════

// ❌ INCORRECTO
// This function computes the spectral profile

// ✅ Comentarios de sección: bloques de ══ en métodos largos
void paint(...) {
    // ─── Background ────────────────────────────────────────────
    ...
    // ─── Meters ────────────────────────────────────────────────
    ...
}
```

---

## 5. Manejo de Memoria

### 5.1 Stack > Heap

```cpp
// ✅ CORRECTO (stack, 128 entradas es trivial)
std::array<TrackState, 128> tracks_;

// ❌ INCORRECTO (heap + alloc)
std::vector<TrackState> tracks_;
tracks_.reserve(128); // Sigue siendo heap
```

### 5.2 SharedMemory es POD-only

```cpp
// ✅ CORRECTO
struct SharedSlotEntry {
    char name[64];
    uint32_t colour;
    int32_t busIndex;
    uint8_t active;
    // ... todos POD
};

// ❌ INCORRECTO
struct SharedSlotEntry {
    std::string name;   // NO: heap allocation
    juce::Colour colour; // NO: no es POD
};
```

### 5.3 Pre-allocar buffers

```cpp
// ✅ CORRECTO: pre-allocado, reutilizado
juce::AudioBuffer<float> scratchBuffer_ { 2, 4096 };

// ❌ INCORRECTO: alloc en cada callback
void processBlock() {
    juce::AudioBuffer<float> temp(2, blockSize); // heap cada bloque
}
```

---

## 6. Thread Safety

| Regla | Descripción |
|:------|:------------|
| **El audio thread nunca espera** | Spinlocks con timeout máximo 100ns. Si no hay lock, skip |
| **La UI nunca toca SharedMemory sin mutex** | Usar `SharedData` como bridge thread-safe |
| **Dos hilos no escriben el mismo cache** | Cada hilo tiene su región de escritura exclusiva |
| **Los objetos JUCE se crean en el message thread** | Especialmente `Graphics`, `Font`, `Image` |
| **Los timers nunca se crean desde el audio thread** | `startTimerHz()` solo desde message thread |
| **std::atomic para flags cross-thread** | `std::atomic<bool>`, `std::atomic<int>` |
| **No shared_ptr entre threads** | `juce::WeakReference` + check en message thread |

### Thread Affinity Map

```
┌──────────────────┬──────────────────┬─────────────────────┐
│ Audio Thread     │ Background Worker│ Message Thread      │
│ (~2.9ms/bloque)  │ (10-30Hz)        │ (60fps)             │
├──────────────────┼──────────────────┼─────────────────────┤
│ AudioAnalyzer    │ SharedData sync  │ CoachEngine         │
│ FFT compute      │ RMS/Peak compute │ AiCoachAdapter      │
│ SharedAudioMem   │ staleSlot check  │ UI paint/resized    │
│ VectorscopeData  │ TrackFeed update │ LLM inference       │
│ PhaseCorrelation │                  │ File I/O            │
└──────────────────┴──────────────────┴─────────────────────┘
```

---

## 7. IPC y Compatibilidad

### 7.1 Versionado de Structs

```cpp
// SIEMPRE incrementar cuando se modifica SharedSlotEntry
static constexpr uint32_t kCurrentStructVersion = 6;
```

**Reglas:**
- Nuevos campos siempre al FINAL del struct
- NO reordenar campos existentes
- NO cambiar tipos de campos existentes
- NO eliminar campos (marcar deprecated si es necesario)
- Al agregar campo: `static_assert` de `sizeof` en test

### 7.2 Backward Compatibility

> Un Messenger compilado hace 6 meses debe funcionar con MixCoach de hoy.

Strategia: el MixCoach lee `kCurrentStructVersion` del Messenger. Si es menor a la esperada, usa defaults para campos nuevos. Nunca crash.

---

## 8. Logging y Debug

### 8.1 MIXCOACH_LOG

```cpp
// ✅ CORRECTO
MIXCOACH_LOG("Track " << slotIndex << " crest=" << crestValue);

// ❌ INCORRECTO
std::cout << "Track " << slotIndex << " crest=" << crestValue << std::endl;
std::printf("Track %d crest=%.2f\n", slotIndex, crestValue);
juce::Logger::writeToLog("Track...");
```

### 8.2 Niveles de log

| Nivel | Uso | Macro |
|:------|:----|:------|
| ERROR | Algo se rompió, el usuario debe saberlo | `MIXCOACH_LOG_ERROR` |
| WARN | Algo inesperado, no crítico | `MIXCOACH_LOG_WARN` |
| INFO | Eventos importantes del flujo | `MIXCOACH_LOG` |
| DEBUG | Diagnóstico interno, desactivado en release | `MIXCOACH_LOG_DEBUG` |

### 8.3 jassert vs MIXCOACH_LOG_ERROR

```cpp
// ✅ Para invariantes que NUNCA deberían romperse
jassert(slotIndex >= 0 && slotIndex < kMaxSlots);

// ✅ Para condiciones que pueden ocurrir en producción
if (slotIndex < 0 || slotIndex >= kMaxSlots) {
    MIXCOACH_LOG_ERROR("Invalid slotIndex: " << slotIndex);
    return;
}
```

---

## 9. Testing

### 9.1 Cobertura Mínima

| Componente | Tests Mínimos | Archivo asociado |
|:-----------|:--------------|:-----------------|
| CoachEngine | TestCoachEngine.cpp | CoachEngine.h |
| MixScore | TestMixScore.cpp | MixScore.h |
| AudioAnalyzer | TestAudioAnalyzer.cpp | AudioAnalyzer.h |
| PhaseManager | TestPhaseManager.cpp | PhaseManager.h |
| SpectralProfiler | TestSpectralBands.cpp | SpectralProfiler.h |
| DifferenceProfile | TestDifferenceProfile.cpp | DifferenceProfile.h |
| SessionProgression | TestExperienceLevel.cpp | SessionProgression.h |
| ReferenceDrivenEngine | TestReferenceProfile.cpp | ReferenceDrivenEngine.h |
| RefinementProfile | TestRefinementProfile.cpp | RefinementProfile.h |

### 9.2 Reglas de Testing

- **Todo archivo nuevo en `engine/` o `audio/` debe tener test**
- **Test por API pública**, no por implementación interna
- **Usar datos sintéticos** (no depender de archivos de audio reales)
- **No usar LLM en tests** (mockear `AiCoachAdapter`)
- **No dormir (`wait()`) en tests** (usar callbacks o `JUCE MessageManager`)
- **Tests estáticos para compute(). Tests dinámicos para flujos.**

### 9.3 Formato de Test

```cpp
// ═══════════════════════════════════════════════════════════
//  Test: computeDepthScore — Valores esperados para mezcla densa
// ═══════════════════════════════════════════════════════════
static void test_compute_depth_score_dense_mix()
{
    std::printf("\n── Test: computeDepthScore (Dense Mix) ──\n");
    std::fflush(stdout);

    // Arrange
    auto profile = RefinementProfile::create(...);

    // Act
    float score = profile.computeDepthScore();

    // Assert
    TEST("Depth score ~0.70", std::abs(score - 0.70f) < 0.05f);
}
```

---

## 10. Naming Conventions

| Tipo | Convención | Ejemplo |
|:-----|:-----------|:--------|
| Clases | PascalCase | `CoachEngine`, `MixMapComponent` |
| Métodos públicos | camelCase | `collectAllIssues()`, `setTrackRoles()` |
| Métodos privados | camelCase | `computeHealth()`, `syncInternalState()` |
| Getters | camelCase sin get | `trackCount()`, `slotIndex()` |
| Setters | set + PascalCase | `setTrackName()`, `setActivePhase()` |
| Variables miembro | snake_case_ | `slotIndex_`, `currentGain_` |
| Variables locales | snake_case | `int trackCount;` |
| Constantes | kPascalCase | `kMaxSlots`, `kDefaultThreshold` |
| Enums | PascalCase | `TrackRole`, `MixPhase` |
| Enum values | kPascalCase | `kKick`, `kSetup`, `kActive` |
| Namespaces | lowercase | `mixcoach::`, `audiobridge::` |
| Archivos | PascalCase | `CoachEngine.cpp`, `MixMapComponent.h` |
| Tests | Test + PascalCase | `TestCoachEngine.cpp` |

---

## 11. Estructura de Archivos

### 11.1 Organización de Directorios

```
Source/
├── Messenger/            # Plugin Messenger (captura audio)
│   ├── Core/             # PluginProcessor, SharedMemory
│   └── UI/               # Interfaz del Messenger
├── MixCoach/             # Plugin principal
│   ├── core/             # PluginProcessor, PluginEditor, App entry
│   ├── audio/            # Análisis de audio DSP
│   ├── engine/           # CoachEngine, analizadores, priorización
│   ├── ai/               # LLM, prompts, adaptadores
│   └── UI/               # Toda la interfaz visual
└── Common/               # Código compartido
    ├── types/            # Constantes, tipos, enums globales
    ├── memory/           # SharedMemory, SlotRegistry, IPC
    └── audio/            # AudioAnalysis, LoudnessAnalyzer (reutilizable)
```

### 11.2 Estructura de cada archivo .h/.cpp

```
// ═══════════════════════════════════════════════════════════
//  ClassName.h — Descripción breve
// ═══════════════════════════════════════════════════════════

#pragma once
#ifndef INCLUDED_CLASSNAME_H // Include guard
#define INCLUDED_CLASSNAME_H

// Includes del proyecto primero, luego JUCE, luego std
#include "Common/types/Types.h"
#include <juce_core/juce_core.h>
#include <array>

namespace mixcoach {

class ClassName {
public:
    // Constructor explícito
    explicit ClassName();

    // API pública
    void doSomething();

private:
    // Miembros privados
    int value_;
};

} // namespace mixcoach
#endif
```

---

## 12. Checklist Pre-Merge

Antes de marcar cualquier tarea como completa, verificar:

### 🟢 Compilación
- [ ] Compila en Release sin warnings nuevos
- [ ] Compila en Debug (+ aserciones pasan)
- [ ] Todos los targets afectados compilan

### 🟢 Tests
- [ ] Tests existentes pasan
- [ ] Tests nuevos agregados si hay lógica nueva
- [ ] `run_tests` completa sin fallos

### 🟢 Regresiones
- [ ] No se rompió compatibilidad IPC
- [ ] No se eliminaron campos de SharedSlotEntry
- [ ] No se cambiaron firmas de API pública sin actualizar callers
- [ ] No se introdujeron nuevas dependencias circulares

### 🟢 Calidad
- [ ] Sin `std::cout`, `printf`, `MIXCOACH_LOG` en hot paths
- [ ] Sin números mágicos
- [ ] Sin `auto` en API pública donde el tipo no es obvio
- [ ] Sin `new`/`delete` manual en hot paths
- [ ] Sin includes innecesarios

### 🟢 Documentación
- [ ] `CMakeLists.txt` actualizado si hay archivos nuevos
- [ ] Si cambió API pública, actualizar header comment
- [ ] Si cambió flujo IPC, actualizar `IPC_CONTRACT.md`

---

*Documento de reglas de código — MixCoach — 26 junio 2026*
*Todo agente debe seguir estas reglas. Violaciones deben reportarse como issues.*

# 🛡️ MixCoach — Safe Edit Guide

> **Guía de modificación segura para IA.**
> Cómo modificar el proyecto sin romper IPC, sin duplicar módulos,
> y manteniendo compatibilidad.
> **Versión:** 1.0 | **Última actualización:** 6 junio 2026

---

## 📋 ÍNDICE

1. [Principio Fundamental](#1-principio-fundamental)
2. [Cómo Modificar IPC Sin Romperlo](#2-cómo-modificar-ipc-sin-romperlo)
3. [Cómo Agregar Nuevas Features](#3-cómo-agregar-nuevas-features)
4. [Cómo Extender AudioAnalyzer Sin Tocar Engine](#4-cómo-extender-audioanalyzer-sin-tocar-engine)
5. [Cómo Mantener Compatibilidad de Versiones](#5-cómo-mantener-compatibilidad-de-versiones)
6. [Cómo Evitar Duplicación de Módulos](#6-cómo-evitar-duplicación-de-módulos)
7. [Cómo Agregar un Nuevo Componente UI](#7-cómo-agregar-un-nuevo-componente-ui)
8. [Cómo Agregar un Nuevo Test](#8-cómo-agregar-un-nuevo-test)
9. [Guía Rápida de Refactoring Seguro](#9-guía-rápida-de-refactoring-seguro)

---

## 1. PRINCIPIO FUNDAMENTAL

> **"No estás construyendo desde cero. Estás modificando un sistema que funciona."**

Cada línea de código existente tiene una razón de ser. Antes de cambiarla:

1. **Entiende por qué está ahí** (lee comentarios, AI_CONTEXT.md, DECISION_LOG.md)
2. **Verifica que ningún otro componente depende de ella** (usa code-searcher)
3. **Haz el cambio más pequeño posible**
4. **Verifica que compila y los tests pasan**

---

## 2. CÓMO MODIFICAR IPC SIN ROMPERLO

### ⚠️ ADVERTENCIA: Romper IPC = Todos los plugins dejan de comunicarse

### Regla de Oro
**Si modificas `SharedSlotEntry` en `SharedMemory.h`, debes incrementar `kCurrentStructVersion`.**

### Procedimiento Seguro

```cpp
// 1. UBICA la estructura actual (NO la cambies sin leer esto)
struct SharedSlotEntry {
    int      slotIndex;       // (0-127), -1 = libre
    char     trackName[64];   // null-terminated
    uint32_t colourARGB;      // ARGB
    bool     active;
    int      bus;             // BusType enum
};

// 2. SOLO si es absolutamente necesario, AGREGA al FINAL:
struct SharedSlotEntry {
    int      slotIndex;
    char     trackName[64];
    uint32_t colourARGB;
    bool     active;
    int      bus;
    // 🔴 NUEVO: solo agregar al final, NUNCA en medio
    int      newField;
};

// 3. INCREMENTA la versión:
static constexpr uint32_t kCurrentStructVersion = 7; // ← era 6

// 4. Actualiza IPC_CONTRACT.md con la nueva versión
// 5. Reconstruye AMBOS plugins y redespliega
// 6. Limpia la shared memory vieja (reiniciar DAW)
```

### ❌ Lo que NUNCA debes hacer al IPC

| Acción | Consecuencia |
|--------|-------------|
| Insertar campo en medio de `SharedSlotEntry` | Todos los datos existentes se leen corruptos |
| Cambiar tipo de campo (int→float) | Mismatch de tamaño, datos basura |
| Reordenar campos | Versiones viejas leen campo equivocado |
| Cambiar tamaño de `trackName[64]` | Buffer overflow o truncación |
| Eliminar campo legacy (aunque no se use) | Breaking change innecesario |
| Usar `std::string` en struct compartido | Heap allocation cross-process = crash |

---

## 3. CÓMO AGREGAR NUEVAS FEATURES

### Proceso Paso a Paso

```
1. DEFINIR
   └── ¿Qué problema resuelve?
   └── ¿Ya existe algo similar en el proyecto?
       ├── Sí → Extender, no duplicar
       └── No → Continuar

2. UBICAR
   └── ¿Dónde pertenece?
       ├── ¿Es análisis de audio? → AudioAnalyzer
       ├── ¿Es UI? → Componente existente o nuevo en UI/
       ├── ¿Es engine/mentoría? → CoachEngine
       ├── ¿Es IPC? → SharedMemory / SharedAudioMemory
       └── ¿Es dato compartido? → SharedData

3. IMPLEMENTAR
   └── Cambio mínimo. Un archivo a la vez.
   └── No mezclar features (una solicitud = un cambio)

4. VALIDAR
   └── Compila? (cmake --build)
   └── Tests pasan? (los que correspondan)
   └── No rompió nada más? (code-searcher)
```

### Ejemplo: Agregar una nueva métrica al Master

```cpp
// ✅ CORRECTO: Agregar a AudioAnalyzer (donde pertenece)
// AudioAnalyzer.h
class AudioAnalyzer {
    // ...
    [[nodiscard]] float getNewMetric() const noexcept { return newMetric_; }
private:
    float newMetric_ = 0.0f;
};

// AudioAnalyzer.cpp
void AudioAnalyzer::processBlock(const AudioBuffer<float>& buffer) {
    // ... análisis existente ...
    newMetric_ = computeNewMetric(left, right, numSamples);
}

// MasterMeterPanel.cpp (consumidor)
void MasterMeterPanel::updateMeters(const AudioAnalyzer& analyzer) {
    // ... existente ...
    newMetricSmooth_.setTargetValue(analyzer.getNewMetric());
}
```

```cpp
// ❌ INCORRECTO: Meter lógica de análisis en UI
void MasterMeterPanel::paint(Graphics& g) {
    // ❌ PROHIBIDO: análisis de audio en paint()
    computeNewMetric(masterBuffer_);  // ← NO
}
```

---

## 4. CÓMO EXTENDER AudioAnalyzer SIN TOCAR ENGINE

### Patrón Aprobado: Nueva función pública

```cpp
// AudioAnalyzer.h
class AudioAnalyzer {
public:
    // ✅ Agregar nueva función de análisis aquí
    [[nodiscard]] float getStereoWidth() const noexcept;
    // ...
};
```

### Cómo la consumen los componentes correctos

```cpp
// ✅ UI consume directamente (lectura, sin lógica)
void MasterMeterPanel::updateMeters(const AudioAnalyzer& analyzer) {
    stereoWidth_.setTargetValue(analyzer.getStereoWidth());
}

// ✅ CoachEngine consume para mentoría
void CoachEngine::analyzeSpatial() {
    float width = audioAnalyzer_.getStereoWidth();
    if (width < 0.3f) {
        respondWith("El estéreo es muy estrecho...");
    }
}
```

### Lo que NO debes hacer

```cpp
// ❌ NO: Meter análisis complejo en CoachEngine
void CoachEngine::computeStereoWidthFromScratch() {
    // Si AudioAnalyzer ya debería tenerlo, no lo computes aquí
}

// ❌ NO: Meter análisis en UI
void MasterMeterPanel::computeFFT() {
    // El FFT ya está en AudioAnalyzer
}
```

---

## 5. CÓMO MANTENER COMPATIBILIDAD DE VERSIONES

### IPC Versions

| Componente | Versión | Dónde se define |
|------------|:-------:|-----------------|
| SharedMemory struct | 6 | `kCurrentStructVersion` en `SharedMemory.h` |
| Messenger state format | 2 | `kStateVersion` en `PluginProcessor.cpp` |
| IPC Contract | 3.0 | `IPC_CONTRACT.md` |

### Al incrementar versión IPC

```markdown
1. Incrementar kCurrentStructVersion en SharedMemory.h
2. Actualizar IPC_CONTRACT.md (versión + changelog)
3. Reconstruir AMBOS plugins (MixCoach + Messenger)
4. Redesplegar a VST3 system folder
5. CERRAR y REABRIR el DAW (la memoria compartida se reinicia)
6. Documentar en DECISION_LOG.md
```

### Backward compatibility pattern

```cpp
// ✅ CÓDIGO SEGURO: manejar versiones viejas en setStateInformation
void MessengerAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    int version = mis.readInt();  // ← Leer versión PRIMERO

    if (version == kStateVersion) {
        // Formato actual
        readCurrentFormat(mis);
    } else {
        // Formato legacy (V1)
        readLegacyFormat(mis);
    }
}
```

---

## 6. CÓMO EVITAR DUPLICACIÓN DE MÓDULOS

### Checklist anti-duplicación

Antes de crear un nuevo archivo/clase/función:

- [ ] **Busca en el proyecto** si ya existe algo similar (usa `code-searcher` + `file-picker`)
- [ ] **Pregunta: "¿Esto es una responsabilidad nueva o ya existe?"**
- [ ] **No copies lógica de análisis de audio fuera de `AudioAnalyzer`**
- [ ] **No copies lógica de mentoría fuera de `CoachEngine`**
- [ ] **No copies lógica de IPC fuera de `Common/memory/`**

### Duplicaciones conocidas que evitar

```cpp
// ❌ DUPLICADO: No crear otro cache de audio per-track
class AnotherTrackCache { /* SharedData::trackAudioCache_ ya existe */ };

// ❌ DUPLICADO: No crear otro analizador FFT
class MyFFT { /* AudioAnalysis::FFT ya existe */ };

// ❌ DUPLICADO: No crear otro sistema de persistencia
class MySessionSaver { /* AI_SESSION_STATE.json + SharedData ya existen */ };

// ❌ DUPLICADO: No crear otro motor de mentoría
class MyCoach { /* CoachEngine ya existe */ };
```

---

## 7. CÓMO AGREGAR UN NUEVO COMPONENTE UI

### Pasos

1. **Crear header + .cpp** siguiendo el naming `PascalCaseComponent.h/.cpp`
2. **Ubicar en** `Source/MixCoach/UI/`
3. **Agregar a** `CMakeLists.txt` en `target_sources(MixCoach PRIVATE ...)`
4. **Registrar en** su padre (ej: `AnalyzersPanelComponent`, `MixCoachPanel`)
5. **Usar MixCoachTheme** para colores (NUNCA hardcodear)
6. **Usar SmoothValue** para animación (NUNCA animación manual)
7. **Fuente de datos**: desde `AudioAnalyzer` o `SharedData` (según corresponda)

### Template mínimo

```cpp
// MiNuevoComponente.h
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

class MiNuevoComponente : public juce::Component {
public:
    MiNuevoComponente();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setValue(float v);
private:
    float value_ = 0.0f;
};

} // namespace mixcoach
```

---

## 8. CÓMO AGREGAR UN NUEVO TEST

### Template

```cpp
// tests/TestMiFeature.cpp
#include <cstdio>

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                               \
    if (!(expr)) {                                          \
        std::fprintf(stderr, "  ❌ FAIL: %s (%s:%d)\n",     \
                     name, __FILE__, __LINE__);              \
        gTestsFailed++;                                     \
    } else {                                                \
        std::printf("  ✅ PASS: %s\n", name);               \
        gTestsPassed++;                                     \
    }                                                       \
} while(0)

// ─── Tests ───────────────────────────────────────────────

static void test_basic() {
    std::printf("\n── Test: Basic ──\n");
    TEST("1 + 1 == 2", 1 + 1 == 2);
}

// ─── Main ────────────────────────────────────────────────

int main() {
    std::printf("═══ Test Suite: MiFeature ═══\n\n");
    test_basic();
    std::printf("\nResults: %d passed, %d failed\n",
                gTestsPassed, gTestsFailed);
    return gTestsFailed > 0 ? 1 : 0;
}
```

### Agregar a CMakeLists.txt

```cmake
add_executable(TestMiFeature
    tests/TestMiFeature.cpp
    Source/... # dependencias
)
target_include_directories(TestMiFeature PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${JUCE_ROOT}/modules
)
target_link_libraries(TestMiFeature PRIVATE
    juce_core
    # ... otros módulos JUCE necesarios
)
set_target_properties(TestMiFeature PROPERTIES
    CXX_STANDARD 20
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
```

---

## 9. GUÍA RÁPIDA DE REFACTORING SEGURO

### Renombrar un símbolo

```
1. Encuentra TODOS los usos: code-searcher con el símbolo exacto
2. Cambia la declaración
3. Actualiza TODAS las referencias (NO dejes wrappers)
4. Verifica que compila
5. Ejecuta tests relevantes
```

### Mover una función de un archivo a otro

```
1. Mueve la implementación
2. Actualiza #include en los archivos que la usan
3. NO dejes wrappers de compatibilidad
4. Verifica que compila
5. Limpia el archivo original (elimina función muerta)
```

### Cambiar una firma de función

```
1. Actualiza declaración + implementación
2. Actualiza TODOS los callers (code-searcher)
3. Si es un override, verifica que la clase base coincida
4. Verifica que compila
5. Ejecuta tests
```

### Agregar un parámetro opcional

```cpp
// ✅ SEGURO (default parameter):
void funcion(int obligatorio, int opcional = 0);

// ⚠️ callers existentes siguen funcionando SIN cambios
```

---

## 🔗 REFERENCIAS

- `AI_CONTEXT.md §11` — Leyes del sistema (NO violar)
- `AI_CONTEXT.md §2` — Mapa visual del sistema (consolidado)
- `AI_COMPONENT_INDEX.yaml` — Índice semántico de componentes
- `DECISION_LOG.md` — ADRs para cambios arquitectónicos
- `IPC_CONTRACT.md` — Contrato IPC (leer ANTES de tocar SharedMemory)

---

*Guía de modificación segura — MixCoach V3 — 6 junio 2026*

# ✅ 05 — DEFINITION OF DONE

> **El estándar de completitud para toda tarea en MixCoach.**
>
> Una tarea NO termina cuando compila. Termina cuando cumple TODOS estos criterios.
>
> Ningún agente debe marcar una tarea como "completa" sin pasar esta checklist.
>
> **Versión:** 1.0 | **Última actualización:** 26 junio 2026

---

## 📋 Índice

1. [La Regla Fundamental](#1-la-regla-fundamental)
2. [Niveles de Definición de Done](#2-niveles-de-definici%C3%B3n-de-done)
3. [Nivel 1: Compilación](#3-nivel-1-compilaci%C3%B3n)
4. [Nivel 2: Tests](#4-nivel-2-tests)
5. [Nivel 3: Arquitectura](#5-nivel-3-arquitectura)
6. [Nivel 4: Visión del Producto](#6-nivel-4-visi%C3%B3n-del-producto)
7. [Nivel 5: Calidad de Código](#7-nivel-5-calidad-de-c%C3%B3digo)
8. [Nivel 6: UX](#8-nivel-6-ux)
9. [Nivel 7: Documentación](#9-nivel-7-documentaci%C3%B3n)
10. [Done Checklist por Tipo de Cambio](#10-done-checklist-por-tipo-de-cambio)
11. [Ejemplos: Done vs Not Done](#11-ejemplos-done-vs-not-done)
12. [El Veto del Guardian](#12-el-veto-del-guardi%C3%A1n)

---

## 1. La Regla Fundamental

> **Una tarea NO está completa si:**
> 1. Compila pero no pasa tests
> 2. Pasa tests pero rompe la arquitectura
> 3. Respeta la arquitectura pero contradice la visión del producto
> 4. Sigue la visión pero duplica código existente
> 5. No duplica código pero no tiene documentación necesaria
> 6. Tiene documentación pero degrada la UX

**La tarea solo está DONE cuando cumple TODOS los niveles.**

---

## 2. Niveles de Definición de Done

```
NIVEL 1: 🟢 COMPILACIÓN
  └── El código compila sin errores ni warnings nuevos

NIVEL 2: 🟢 TESTS
  └── Tests existentes pasan + tests nuevos cubren la lógica nueva

NIVEL 3: 🟢 ARQUITECTURA
  └── No rompe la arquitectura. Sigue el flujo Messenger→SharedMem→TrackFeed→CoachEngine→LLM→UI

NIVEL 4: 🟢 VISIÓN
  └── No contradice 00_PROJECT_IDENTITY.md. MixCoach sigue siendo un mentor.

NIVEL 5: 🟢 CALIDAD
  └── Sigue 02_CODING_RULES.md. Sin deuda técnica nueva. Sin código duplicado.

NIVEL 6: 🟢 UX
  └── Sigue 03_UI_GUIDELINES.md. Las 3 preguntas se responden. El chat es protagonista.

NIVEL 7: 🟢 DOCUMENTACIÓN
  └── Archivos nuevos en CMakeLists.txt. API documentada. Documentos constitucionales intactos.
```

**Si algún nivel está en rojo, la tarea NO está DONE.**

---

## 3. Nivel 1: Compilación

### 3.1 Targets Obligatorios

| Target | ¿Cuándo? |
|:-------|:---------|
| `MixCoach_VST3` | Release + Debug |
| `MixCoach_Standalone` | Release (si aplica) |
| `run_tests` | Release (todos los tests) |
| Targets de tests afectados | Release (los que tocan archivos modificados) |

### 3.2 Checklist de Compilación

- [ ] **Release build sin errores**
- [ ] **Debug build sin errores** (las aserciones compilan)
- [ ] **0 warnings nuevos** con `/W4`
- [ ] **Todos los targets que dependen de archivos modificados compilan**
- [ ] **No hay linking errors** (símbolos duplicados, missing references)
- [ ] **No hay errores de LNK4221** (obj sin symbols)

### 3.3 Errores Comunes que NO son Aceptables

```
❌ C4702: unreachable code
❌ C4018: signed/unsigned mismatch
❌ C4244: conversion, possible loss of data
❌ C4267: conversion from size_t to type
❌ C26451: arithmetic overflow
❌ C26812: enum class vs enum
❌ LNK2005: symbol already defined
```

---

## 4. Nivel 2: Tests

### 4.1 Cobertura Mínima por Tipo de Cambio

| Tipo de Cambio | Tests Requeridos |
|:----------------|:-----------------|
| **Nuevo archivo en `engine/`** | Test unitario del archivo + integración con `TestCoachEngine` si aplica |
| **Nuevo archivo en `audio/`** | Test de análisis con datos sintéticos |
| **Nuevo archivo en `UI/`** | Test de render (si es componente complejo) + test de layout |
| **Nuevo archivo en `ai/`** | Test de formato de prompt + test de parsing de respuesta |
| **Modificación de API pública** | Test de regresión para la API modificada |
| **Modificación de IPC** | `TestStress128Slots` + `TestIPCIntegration` |
| **Modificación de CoachEngine** | `TestCoachEngine` completo |
| **Bug fix** | Test que reproduce el bug + test que verifica la corrección |
| **Refactor** | Tests existentes pasan (sin cambios en tests) |

### 4.2 Formato de Test

```cpp
// ═══════════════════════════════════════════════════════════
//  Test: [nombre del test] — [qué prueba]
// ═══════════════════════════════════════════════════════════
static void test_[nombre]()
{
    std::printf("\n── Test: [Nombre] ──\n");
    std::fflush(stdout);

    // Arrange
    // ...

    // Act
    // ...

    // Assert
    TEST("Descripción del assert", condicion);
}
```

### 4.3 Reglas de Testing

| Regla | Explicación |
|:------|:------------|
| Tests independientes | Cada test se ejecuta solo y no depende de otro |
| Sin sleeps | Usar `JUCE MessageManager` para async |
| Sin LLM | Mockear `AiCoachAdapter` |
| Datos sintéticos | No depender de archivos WAV externos |
| setup/teardown | Cada test limpia su estado |
| Nombres descriptivos | `test_compute_depth_score_dense_mix()` no `test_1()` |

### 4.4 Cuando NO se Requieren Tests

- Cambios de solo UI que no tocan lógica
- Cambios de solo documentación
- Hotfixes que requieren deploy inmediato (con aprobación)
- Warnings fixes (solo si no hay cambio de lógica)

En estos casos, la tarea se marca como "NO TEST REQUIRED" con justificación.

---

## 5. Nivel 3: Arquitectura

### 5.1 Reglas Arquitectónicas Inviolables

- [ ] **El flujo de datos respeta la direccionalidad:** Messenger → SharedMem → TrackFeed → CoachEngine → LLM → UI
- [ ] **No hay caminos inversos** (UI no decide, LLM no calcula, CoachEngine no ejecuta UI)
- [ ] **El audio thread no toca heap, ni file I/O, ni locks**
- [ ] **La UI se actualiza desde el message thread (60fps timer), no desde callbacks**
- [ ] **No se agregaron dependencias circulares** entre módulos

### 5.2 Verificaciones Arquitectónicas

- [ ] **`git diff CMakeLists.txt`** — archivos nuevos agregados correctamente
- [ ] **No hay nuevos includes de `UI/` en `engine/`** (eso rompe la direccionalidad)
- [ ] **No hay nuevos includes de `audio/` en `ai/`** (el LLM no debe saber de FFT)
- [ ] **No hay nuevos includes de `engine/` en `Common/`** (el Common no depende del plugin)
- [ ] **Si se tocó IPC:** `kCurrentStructVersion` incrementado si cambió layout de struct
- [ ] **Si se tocó IPC:** campos nuevos al final del struct, no reordenados

### 5.3 Dependencias Permitidas (Refresco Rápido)

```
Common/types/ → (standalone)
Common/memory/ → Common/types/
Common/audio/ → Common/types/
MixCoach/audio/ → Common/audio/, Common/types/
MixCoach/engine/ → MixCoach/audio/, Common/memory/, Common/types/, Common/audio/
MixCoach/ai/ → MixCoach/engine/, Common/types/
MixCoach/UI/ → MixCoach/engine/, MixCoach/audio/, Common/memory/, Common/types/
MixCoach/core/ → (todo, es entry point)
```

---

## 6. Nivel 4: Visión del Producto

### 6.1 Verificaciones de Visión

- [ ] **¿MixCoach sigue siendo un mentor?** (no un auto-mixer, no un plugin de procesamiento)
- [ ] **¿El cambio ayuda al productor a entender mejor su mezcla?** (Golden Rule #1)
- [ ] **¿Hace el producto más educativo?** (Golden Rule #2)
- [ ] **¿Reduce la complejidad para el usuario?** (Golden Rule #3)
- [ ] **¿Respeta el flujo natural de mezcla?** (Golden Rule #4)
- [ ] **¿El control sigue en manos del usuario?** (Core Philosophy)
- [ ] **¿El LLM interpreta pero no calcula?** (Intelligence Philosophy)

### 6.2 Antipatrones de Visión

| Antipatrón | Problema |
|:-----------|:---------|
| Agregar auto-EQ o auto-compressor | MixCoach no procesa audio |
| Mostrar MixScore al usuario | El usuario no debe sentirse calificado |
| Dejar que el LLM decida prioridades | Las decide CoachEngine |
| Agregar un "modo automático" | MixCoach enseña, no automatiza |
| Mostrar FFT sin contexto | El usuario no es un analista de espectro |
| Agregar chatbots genéricos | MixCoach es especializado en audio |

---

## 7. Nivel 5: Calidad de Código

### 7.1 Verificaciones de Calidad

- [ ] **No se duplicó lógica existente** (check `git grep` antes de crear función nueva)
- [ ] **No se introdujeron números mágicos** (usar constantes de `Constants.h` o del archivo)
- [ ] **No hay `std::cout`, `printf`, `MIXCOACH_LOG` en hot paths del audio thread**
- [ ] **No hay `auto` en API pública donde el tipo no es obvio**
- [ ] **No hay `new`/`delete` manual** (usar `std::unique_ptr`, `std::array`)
- [ ] **No hay `using namespace std`**
- [ ] **No se cambiaron tipos de variables en hot paths sin perfilamiento**
- [ ] **No hay includes innecesarios** (no incluir `juce_audio_processors` en tests que no lo necesitan)
- [ ] **Las funciones nuevas tienen comentario de sección**
- [ ] **No hay bucles infinitos ni condiciones siempre verdaderas/falsas**

### 7.2 Métricas de Calidad

| Métrica | Límite | Excepción |
|:--------|:-------|:----------|
| Líneas por función | ≤ 80 líneas | paint() complejo, init con muchos campos |
| Líneas por archivo | ≤ 500 líneas (cada archivo debe cumplir una función clara) | Excepciones para grandes estructuras de datos o enumeraciones extensas |
| Parámetros por función | ≤ 5 | Si son más, probablemente necesitas un struct |
| Nivel de indentación | ≤ 4 niveles | Raro, refactorizar si llega a 5+ |
| Complejidad ciclomática | ≤ 15 | Si es mayor, refactorizar en funciones más pequeñas |

### 7.3 Deuda Técnica NO Aceptable

```
❌ Código comentado
❌ "TODO" sin issue asociado
❌ "FIXME" sin issue asociado
❌ "HACK" sin issue asociado
❌ Funciones sin usar (dead code)
❌ Variables sin usar
❌ Includes sin usar
❌ Código duplicado (copiar-pegar)
❌ Constantes mágicas repetidas 3+
```

---

## 8. Nivel 6: UX

### 8.1 Verificaciones de UX

- [ ] **¿La pantalla responde las 3 preguntas?** (¿Qué está pasando? ¿Por qué? ¿Qué hago ahora?)
- [ ] **¿Hay una sola acción primaria por pantalla?**
- [ ] **¿El chat sigue siendo el protagonista?**
- [ ] **¿No hay números sin contexto?** (ningún número debe aparecer sin etiqueta o explicación)
- [ ] **¿Los analizadores tienen contexto y no son la pantalla principal?**
- [ ] **¿Las animaciones duran ≤ 200ms?**
- [ ] **¿Se usó SmoothValue para métricas animadas?**
- [ ] **¿Los estados de carga/empty/error están manejados?**
- [ ] **¿No hay términos técnicos sin traducción al lenguaje del usuario?**
- [ ] **¿El layout funciona en al menos 3 resoluciones?** (800px, 1024px, 1440px)

### 8.2 Anti-patrones de UX

| Síntoma | Problema | Solución |
|:--------|:---------|:---------|
| Tabla de números | Sensación de debugger | Visualización con contexto |
| Score numérico visible | Usuario se siente calificado | Indicador cualitativo |
| Más de un botón primario | Usuario no sabe qué hacer | Priorizar, uno visible |
| Popup de error con código | Frustración | Mensaje amigable + acción |
| Layout roto en 800px | Inaccesible en pantallas pequeñas | Responsive con breakpoints |

---

## 9. Nivel 7: Documentación

### 9.1 Verificaciones de Documentación

- [ ] **Archivos nuevos: agregados a `CMakeLists.txt`** en el target correcto
- [ ] **API pública documentada** en el header con comentario de sección
- [ ] **Funciones nuevas con comentario:** `// computeX — qué hace, qué recibe, qué retorna`
- [ ] **Si cambió API pública:** actualizar header comment y buscar callers
- [ ] **Si cambió IPC:** actualizar `IPC_CONTRACT.md`
- [ ] **Si cambió arquitectura:** actualizar `01_ARCHITECTURE.md`
- [ ] **Si cambió comportamiento del LLM:** actualizar `04_AI_RULES.md`
- [ ] **Si cambió UI:** verificar `03_UI_GUIDELINES.md`
- [ ] **No se modificaron documentos constitucionales sin aprobación**

### 9.2 Documentación por Tipo de Cambio

| Tipo de Cambio | Documentos a Verificar |
|:----------------|:-----------------------|
| Nuevo archivo | `CMakeLists.txt` |
| Nueva API | Header del archivo |
| Nuevo componente UI | `03_UI_GUIDELINES.md` (solo si introduce nuevo patrón) |
| Nuevo prompt | `04_AI_RULES.md` |
| Cambio IPC | `IPC_CONTRACT.md`, `01_ARCHITECTURE.md` |
| Cambio arquitectura | `01_ARCHITECTURE.md` |
| Nueva dependencia | `01_ARCHITECTURE.md` (sección de dependencias) |

---

## 10. Done Checklist por Tipo de Cambio

### 10.1 Bug Fix

```
☐ Compila Release + Debug
☐ Test que reproduce el bug agregado
☐ Test que verifica la corrección
☐ No rompe tests existentes
☐ No introduce código duplicado
☐ No tiene efectos secundarios no intencionales
```

### 10.2 Nueva Feature (engine)

```
☐ Compila Release + Debug
☐ Tests unitarios para la feature
☐ Tests de regresión pasan
☐ No rompe arquitectura (verificar includes)
☐ Sigue 00_PROJECT_IDENTITY.md (Golden Rule)
☐ No duplica `TrackState` u otras estructuras existentes
☐ CMakeLists.txt actualizado
☐ API documentada en header
```

### 10.3 Nueva Feature (UI)

```
☐ Compila Release + Debug
☐ No toca engine, audio, o ai
☐ Sigue 03_UI_GUIDELINES.md (las 3 preguntas, chat protagonista)
☐ Maneja loading, empty, error, active states
☐ Layout funciona en 800px, 1024px, 1440px
☐ Transiciones ≤ 200ms usando SmoothValue
☐ Sin números sin contexto
☐ Sin tablas de datos
```

### 10.4 Cambio de LLM/Prompts

```
☐ Compila Release + Debug
☐ Test de formato de prompt
☐ El LLM no calcula métricas (verificar prompt)
☐ Fallback implementado (si LLM falla)
☐ Tono sigue 04_AI_RULES.md
☐ Sin inventar datos
☐ Prompt actualizado en AiCoachAdapterPrompts.cpp (constantes)
```

### 10.5 Refactor

```
☐ Compila Release + Debug
☐ Tests existentes pasan (sin cambios)
☐ No cambia comportamiento observable
☐ No introduce nuevas dependencias
☐ Código eliminado: `git grep` verifica que no hay referencias
☐ Mejora métricas de calidad (complejidad, duplicación)
```

---

## 11. Ejemplos: Done vs Not Done

### 11.1 ✅ DONE

> **Tarea:** Agregar analizador de fase con indicador de correlación.
>
> - [x] Compila Release + Debug ✅
> - [x] TestPhaseCorrelationMeter.cpp cubre 0, +1, -1, sweep ✅
> - [x] No toca SharedMemory (usa AudioAnalyzer existente) ✅
> - [x] El indicador muestra "Mono compatible" o "Riesgo de fase", no el número crudo ✅
> - [x] SmoothValue para la animación de la barra ✅
> - [x] `PhaseCorrelationMeter.h/.cpp` en CMakeLists.txt ✅
> - [x] Las 3 preguntas: "Fase: buena" / "Porque correlación > 0.7" / "Sigue con dinámica" ✅
> - [x] Sin duplicar lógica de `VectorscopeComponent` (reutiliza) ✅

### 11.2 ❌ NOT DONE

> **Tarea:** Agregar analizador de fase con indicador de correlación.
>
> - [x] Compila Release ✅
> - [ ] No se probó Debug ❌
> - [ ] No hay test ❌
> - [ ] Usa `std::cout` para debug ❌
> - [ ] Muestra "Correlation: 0.72" sin contexto ❌
> - [ ] No maneja estado empty (cuando no hay audio) ❌
> - [ ] El número parpadea (no usa SmoothValue) ❌
> - [ ] No se agregó a CMakeLists.txt ❌

---

## 12. El Veto del Guardián

### 12.1 Guardian de la Visión

Cada PR debe ser revisado por un **Guardian de la Visión** (agente o humano) que responde:

```
¿Esto acerca MixCoach a ser el mejor mentor de mezcla del mercado?

☐ SÍ
☐ NO

POR QUÉ:
[Explicación de 1-2 párrafos]
```

### 12.2 Poder de Veto

El Guardian puede vetar un cambio si:

1. **Contradice la identidad del producto** (00_PROJECT_IDENTITY.md)
2. **Rompe la arquitectura fundamental** (01_ARCHITECTURE.md)
3. **Degrada la UX seriamente** (03_UI_GUIDELINES.md)
4. **Introduce deuda técnica crítica** (02_CODING_RULES.md)

**El veto es vinculante.** Si el Guardian vota NO, el cambio no se mergea sin revisión del fundador.

### 12.3 Preguntas del Guardian

```
1. ¿Este cambio acerca MixCoach a cumplir su misión?
2. ¿Este cambio respeta la Golden Rule?
3. ¿Este cambio mantiene el control en manos del usuario?
4. ¿Este cambio es educativo o solo técnico?
5. ¿Este cambio simplifica o complica la experiencia?
6. ¿Este cambio podría hacer que un productor confíe menos en sus decisiones?
```

### 12.4 Activación

El Guardian se activa automáticamente cuando:

- Se agrega una nueva feature mayor
- Se modifica la arquitectura
- Se cambia el comportamiento del LLM
- Se agrega un nuevo componente de UI
- Cualquier cambio que toque `PRODUCT_VISION.md`, `00_PROJECT_IDENTITY.md`

---

*Documento de Definición de Done — MixCoach — 26 junio 2026*
*Ninguna tarea está completa hasta que pasa TODOS los niveles.*
*El Guardian de la Visión tiene la última palabra.*

# 🤖 AGENT_MODES.md

> **Perfiles de trabajo especializados para agentes IA.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## Introducción

Cada modo define un alcance, riesgos y restricciones específicos para que el
agente IA trabaje de forma enfocada y segura en un subsistema del proyecto.

**Antes de comenzar cualquier tarea, el agente DEBE declarar su modo de trabajo.**

---

## 📋 Tabla de Modos

| Modo | Enfoque | Riesgo | Prioridad |
|------|---------|:------:|:---------:|
| `MODE_UI` | Componentes visuales, tema, layout | 🟡 Medio | Fluidez UI |
| `MODE_DSP` | Análisis de audio, FFT, LUFS, meters | 🟠 Alto | Rendimiento audio |
| `MODE_IPC` | Shared memory, backup files, slot registry | 🔴 Crítico | Integridad IPC |
| `MODE_BUILD` | CMake, scripts, deploy, tests | 🟠 Alto | Compilación |
| `MODE_ARCHITECTURE` | Cambios cross-cutting, refactors, ADRs | 🔴 Crítico | Estabilidad |

---

## MODE_UI — Componentes Visuales

### Objetivo
Crear, modificar o mantener componentes de interfaz de usuario en MixCoach y Messenger.

### Archivos típicos
- `Source/MixCoach/UI/*.h` y `*.cpp`
- `Source/Messenger/ui/PluginEditor.h` y `*.cpp`
- `Source/MixCoach/UI/MixCoachTheme.h`

### Riesgos
| Riesgo | Impacto |
|--------|---------|
| Introducir lógica de audio en UI | Repaints costosos, UI lenta |
| Hardcodear colores | Inconsistencia visual, difícil de cambiar después |
| Operaciones lentas en `paint()` | UI congelada, timer salta ticks |
| No coincidir con imágenes de referencia | UX diferente a la esperada |

### Restricciones
```
🚫 NO poner lógica de audio o IPC en paint()
🚫 NO hardcodear colores (usar MixCoachTheme + Constants.h)
🚫 NO hacer file I/O en timer de UI
🚫 NO usar forward declarations de componentes JUCE (incluir headers)
🚫 NO modificar SharedSlotEntry, BusType, o estructuras de datos compartidas
✅ USAR SmoothValue para meters y barras
✅ USAR MixCoachTheme::fillGlassPanel() para fondos
✅ SEGUIR workspace_memory/visual_design.md como guía
✅ RESPETAR workspace_memory/ui_map.yaml
```

### Validaciones requeridas
```markdown
- [ ] Compila MixCoach_VST3 (Release)
- [ ] UI coincide con imágenes de referencia (UI_REFERENCES/)
- [ ] Timer 30fps mantenido (profiling visual básico)
- [ ] Sin fugas de memoria de componentes
- [ ] Responsive a resize de ventana
```

### Referencias
- `workspace_memory/visual_design.md` — Guía visual canónica
- `workspace_memory/ui_map.yaml` — Mapa de componentes
- `workspace_memory/component_map.md` — Qué archivo controla qué componente
- `UI_REFERENCES/*.png` — Imágenes de referencia (fuente de verdad)
- `APPROVED_PATTERNS.md` — P3: SmoothValue, P8: Timer Throttling

---

## MODE_DSP — Análisis de Audio

### Objetivo
Modificar o mejorar el pipeline de análisis de audio: FFT, RMS, LUFS, correlación de fase, crest factor.

### Archivos típicos
- `Source/Messenger/telemetry/TelemetryCollector.h` y `*.cpp`
- `Source/Common/audio/AudioAnalysis.h` y `*.cpp`
- `Source/MixCoach/audio/AudioAnalyzer.h` y `*.cpp`
- `Source/MixCoach/engine/CoachEngine.h` y `*.cpp` (análisis de fase)

### Riesgos
| Riesgo | Impacto |
|--------|---------|
| NaN/denormals en audio thread | Cracking, popping, crash del DAW |
| Heap allocation en processBlock() | Latencia no determinista, crash |
| CPU > 0.05% por Messenger | FL Studio overload, audio cutting out |
| Overflow en acumuladores LUFS | Mediciones incorrectas, mentoría errónea |
| Modificar struct de telemetría | Rompe compatibilidad IPC |

### Restricciones
```
🚫 NO heap allocation en processBlock()
🚫 NO file I/O en audio thread
🚫 NO modificar SharedSlotEntry sin incrementar kCurrentStructVersion
🚫 NO incluir lógica de UI en archivos DSP
🚫 NO asumir sample rate fijo (usar prepareToPlay)
✅ USAR safe_sqrt() y safe_atan2() de AudioAnalysis.h
✅ USAR FloatVectorOperations::disableDenormals() en processBlock
✅ PREFERIR aritmética de enteros para índices de buffer
✅ RESPETAR límite de FFT cada 4 bloques (no en cada bloque)
```

### Validaciones requeridas
```markdown
- [ ] Compila Messenger_VST3 (Release)
- [ ] TestStress128Slots pasa (incluye tests de telemetría)
- [ ] TestIPCIntegration pasa (86 tests)
- [ ] Sin NaN (verificar con inputs de silencio, DC, senoidal)
- [ ] CPU < 0.05% por Messenger a 96 samples/block
- [ ] Sin heap allocation en processBlock (verificar con /analyze)
```

### Referencias
- `ERROR_PATTERNS.json` — RUNTIME_CRASH_AUDIO (NaN/denormal patterns)
- `KNOWN_ERRORS.md` — Audio thread crash / NaN fix
- `APPROVED_PATTERNS.md` — P5: Lock-Free Ring Buffer
- `AI_CONTEXT.md` — Pipeline de Audio del Messenger

---

## MODE_IPC — Comunicación Entre Plugins

### Objetivo
Modificar o mantener el sistema de comunicación entre Messenger y MixCoach:
shared memory, backup files, slot registry.

### Archivos típicos
- `Source/Common/memory/SharedMemory.h` y `*.cpp`
- `Source/Common/memory/SlotRegistry.h` y `*.cpp`
- `Source/Common/memory/SharedData.h` y `*.cpp`
- `Source/Common/types/Types.h` (estructuras de datos compartidas)
- `Source/Common/types/Constants.h`
- `Source/Common/types/TelemetryData.h`
- `IPC_CONTRACT.md`

### Riesgos
| Riesgo | Impacto |
|--------|---------|
| Corromper shared memory | Datos basura en meters, crashes |
| Deadlock en spinlock | Ambos plugins congelados, FL Studio crash |
| Perder backup files | Slots perdidos al reiniciar FL Studio |
| No incrementar kCurrentStructVersion | Shared memory silenciosamente corrupta |
| Race condition en slots | Slots duplicados, datos inconsistentes |

### Restricciones
```
🚫 NO modificar SharedSlotEntry sin incrementar kCurrentStructVersion
🚫 NO adquirir spinlock desde audio thread
🚫 NO hacer file I/O síncrono en timer de UI
🚫 NO cambiar el formato de backup files sin migración
🚫 NO eliminar canales de comunicación (siempre mantener SHM + backup)
🚫 NO modificar BusType sin actualizar: busNames[], kBusColourARGB, VirtualBusesComponent, SlotRegistry
✅ SIEMPRE mantener graceful degradation (si SHM falla, usar backup)
✅ USAR two-phase spinlock (_mm_pause() + Sleep(0))
✅ TESTEAR con 128 slots concurrentes (TestStress128Slots)
✅ DOCUMENTAR cambios en IPC_CONTRACT.md
```

### Validaciones requeridas
```markdown
- [ ] Compilan ambos plugins (MixCoach_VST3 + Messenger_VST3) (Release)
- [ ] TestStress128Slots pasa (1595 tests)
- [ ] TestIPCIntegration pasa (86 tests)
- [ ] Ambos canales verificados: SHM + backup files
- [ ] Sin deadlocks en escenario de 128 slots concurrentes
- [ ] Graceful degradation cuando SHM no disponible
```

### Referencias
- `IPC_CONTRACT.md` — Contrato formal de comunicación
- `RISK_MATRIX.md` — 🔴 Crítico: SharedMemory.h, SlotRegistry.h, SharedData.h
- `APPROVED_PATTERNS.md` — P6: Two-Phase Spinlock
- `ERROR_PATTERNS.json` — RUNTIME_IPC_FAILURE patterns

---

## MODE_BUILD — Compilación y Deploy

### Objetivo
Configurar el sistema de build, scripts de deploy, tests, y validación.

### Archivos típicos
- `CMakeLists.txt`
- `build.ps1`
- `DeployVST3.ps1`
- `scripts/validate.ps1`
- `scripts/*.ps1` y `*.bat`

### Riesgos
| Riesgo | Impacto |
|--------|---------|
| Romper la compilación | Proyecto no compila, nadie puede trabajar |
| Usar Ninja en Release | C1001 en juce_graphics_Harfbuzz.cpp |
| Deployar a ruta incorrecta | FL Studio no encuentra los VST3 |
| No registrar .cpp nuevo | LNK2019 en tiempo de link |
| PCH mal configurado | Conflictos con JUCE_IMPLEMENT_MODULE |

### Restricciones
```
🚫 NO usar Ninja como generator (usar Visual Studio 17 2022)
🚫 NO cambiar el directorio de build (siempre build/ minúscula)
🚫 NO modificar flags de JUCE sin verificar compatibilidad
🚫 NO eliminar targets sin verificar dependencias
🚫 NO cambiar la ruta de deploy (C:\Program Files\Common Files\VST3\)
✅ USAR build.ps1 como entry point único
✅ REGISTRAR nuevos .cpp en CMakeLists.txt target_sources()
✅ VERIFICAR validate.ps1 después de cambios en build
✅ DOCUMENTAR cambios en build en AI_CONTEXT.md si son significativos
```

### Validaciones requeridas
```markdown
- [ ] build.ps1 funciona (Release + Debug, con y sin -Clean)
- [ ] validate.ps1 pasa (11 checks)
- [ ] Ambos VST3s se despliegan correctamente
- [ ] Tests C++ se ejecutan (run_tests target)
- [ ] VST3 bundles contienen DLL (no carpetas vacías)
```

### Errores comunes de build
| Error | Causa | Fix |
|-------|-------|-----|
| LNK2019 | .cpp no registrado en CMakeLists.txt | Agregar a target_sources() |
| C1001 | Ninja + Release | Usar Visual Studio 17 2022 |
| MSB3073 | FL Studio abierto bloquea deploy | Cerrar FL Studio, rebuild |
| VST3 vacío | Build falló silenciosamente | Verificar cmake --build output |
| LNK2038 | Mix de Debug/Release objects | Clean rebuild |

### Referencias
- `AI_CONTEXT.md` — Build System section
- `KNOWN_ERRORS.md` — Build errors section
- `ERROR_PATTERNS.json` — CMake + MSBuild patterns

---

## MODE_ARCHITECTURE — Visibilidad Global

### Objetivo
Cambios que afectan a múltiples subsistemas: refactors grandes, cambios en
estructuras de datos compartidas, nuevo subsistema, cambios en el flujo de datos.

### Archivos típicos
- Cualquier archivo (especialmente `Types.h`, `Constants.h`, `SlotRegistry.h`)
- Múltiples archivos en diferentes subsistemas simultáneamente

### Riesgos
| Riesgo | Impacto |
|--------|---------|
| Romper TODO el proyecto | Múltiples subsistemas afectados |
| Introducir dependencias circulares | Link errors, diseño frágil |
| Cambiar flujo de datos | Comportamiento impredecible en runtime |
| No actualizar documentación IA | Próximos agentes trabajan con info incorrecta |

### Restricciones
```
🚫 NO hacer cambios arquitectónicos sin ADR (DECISIONS_LOG_TEMPLATE.md)
🚫 NO cambiar el flujo de datos sin verificar ambos plugins
🚫 NO eliminar canales de comunicación IPC
🚫 NO introducir dependencias de UI en Common/ o viceversa
🚫 NO cambiar firmas de funciones exportadas sin actualizar todas las referencias
✅ CREAR checkpoint antes de empezar: .\scripts\ProjectCheckpoint.ps1
✅ DOCUMENTAR en ADR (workspace_memory/decisions/)
✅ ACTUALIZAR PROJECT_GRAPH.json y SYMBOL_GRAPH.json si cambian dependencias
✅ ACTUALIZAR AI_CONTEXT.md y workspace_memory/ si cambia la arquitectura
✅ EJECUTAR suite completa de tests después del cambio
✅ SOLICITAR aprobación del usuario antes de comenzar
```

### Proceso obligatorio
```markdown
1. [ ] Escribir ADR (usar DECISIONS_LOG_TEMPLATE.md)
2. [ ] Obtener aprobación del usuario
3. [ ] Crear checkpoint del proyecto
4. [ ] Implementar cambio
5. [ ] Compilar ambos plugins
6. [ ] Ejecutar suite completa de tests
7. [ ] Actualizar documentación:
    - AI_CONTEXT.md
    - PROJECT_GRAPH.json (si cambian dependencias)
    - SYMBOL_GRAPH.json (si cambian símbolos)
    - workspace_memory/* (si cambia estructura)
8. [ ] Code review
9. [ ] Deploy y probar en FL Studio
```

### Validaciones requeridas
```markdown
- [ ] Compilan ambos plugins (Release + Debug)
- [ ] TODOS los tests pasan (TestStress128Slots, TestIPCIntegration, TestCoachEngine, TestPhaseManager, TestSmoothValue)
- [ ] Sin dependencias circulares nuevas
- [ ] ADR registrado en workspace_memory/decisions/
- [ ] Documentación actualizada
- [ ] Sin regresiones IPC (verificar con TestStress128Slots)
- [ ] Sin regresiones de rendimiento (ver CPU profile)
```

### Referencias
- `DECISIONS_LOG_TEMPLATE.md` — Plantilla ADR
- `RISK_MATRIX.md` — Archivos 🔴 Críticos
- `PROJECT_GRAPH.json` — Dependencias actuales
- `SYMBOL_GRAPH.json` — Símbolos actuales
- `PROJECT_PRIORITIES.md` — Prioridades a proteger

---

## 📋 Cómo Seleccionar un Modo

```
¿El cambio afecta UI/visual?
    → MODE_UI

¿El cambio afecta análisis de audio (FFT, RMS, LUFS)?
    → MODE_DSP

¿El cambio afecta comunicación entre plugins (shared memory, backups)?
    → MODE_IPC

¿El cambio afecta build, scripts, deploy?
    → MODE_BUILD

¿El cambio afecta MÚLTIPLES subsistemas o cambia el flujo de datos?
    → MODE_ARCHITECTURE
```

**Si hay duda entre dos modos, usar el de mayor riesgo.**
Ejemplo: cambiar un struct en Types.h que afecta IPC + DSP → `MODE_ARCHITECTURE`.

---

*Documento de gobernanza para agentes IA — MixCoach Project*

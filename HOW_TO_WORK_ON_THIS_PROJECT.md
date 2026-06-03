# 📋 HOW_TO_WORK_ON_THIS_PROJECT.md

> **Flujo obligatorio de trabajo para cualquier agente IA.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## ⚠️ Regla de Oro

**No modifiques nada sin antes leer este documento y seguir el flujo completo.**
Cada paso existe por una razón — saltarte uno puede causar regresiones, crashes en FL Studio o corrupción de datos IPC.

---

## 🔄 Flujo de Trabajo Obligatorio (5 Fases)

```
FASE 0: CONTEXTUALIZACIÓN
    │
    ▼
FASE 1: ANÁLISIS
    │
    ▼
FASE 2: PLANIFICACIÓN
    │
    ▼
FASE 3: EJECUCIÓN
    │
    ▼
FASE 4: VALIDACIÓN
```

---

### FASE 0: Contextualización (Siempre, ~30s)

Antes de tocar CUALQUIER archivo:

```markdown
- [ ] Leer `AGENTS.md` (visión del producto, north star)
- [ ] Leer `AI_CONTEXT.md` (documentación consolidada completa)
- [ ] Leer `workspace_memory/current_state.md` (estado actual del proyecto)
- [ ] Leer `workspace_memory/project_rules.md` (reglas de arquitectura)
```

**Si el cambio es UI/visual:**
```markdown
- [ ] Inspeccionar imágenes en `UI_REFERENCES/` (fuente de verdad visual)
- [ ] Leer `workspace_memory/visual_design.md`
- [ ] Leer `workspace_memory/component_map.md`
```

**Si el cambio es IPC/Memory:**
```markdown
- [ ] Leer `IPC_CONTRACT.md` (contrato formal Messenger↔MixCoach)
```

---

### FASE 1: Análisis (~2-5min)

Antes de planificar cambios:

```markdown
- [ ] Revisar `PROJECT_GRAPH.json` (dependencias: qué archivos dependen de qué)
- [ ] Revisar `SYMBOL_GRAPH.json` (símbolos exportados y referencias)
- [ ] Revisar `ERROR_PATTERNS.json` (errores conocidos y fixes)
- [ ] Revisar `KNOWN_ERRORS.md` (bugs históricos y workarounds)
- [ ] Identificar hotspots y archivos peligrosos (sección 🔥 en AI_CONTEXT.md)
- [ ] Leer los archivos relevantes ANTES de editarlos
- [ ] Buscar usos existentes de cualquier símbolo que vayas a modificar
```

**Herramientas de análisis disponibles:**
```powershell
# Buscar dependencias de un archivo
grep -rn "include.*Types.h" Source/ --include="*.cpp" --include="*.h"

# Buscar referencias a un símbolo
grep -rn "miFuncion" Source/ --include="*.cpp" --include="*.h"

# Verificar grafo de dependencias (PROJECT_GRAPH.json)
# Buscar "depended_by" para ver impacto de cambios
```

---

### FASE 2: Planificación (~1-3min)

```markdown
- [ ] Evaluar riesgo del cambio (ver RISK_MATRIX.md)
- [ ] Verificar prioridades del proyecto (ver PROJECT_PRIORITIES.md)
- [ ] Verificar patrones permitidos (ver APPROVED_PATTERNS.md)
- [ ] Proponer solución:
    - ¿Qué archivos se modifican?
    - ¿Cuál es el impacto en dependencias?
    - ¿Requiere cambios en tests?
    - ¿Requiere actualización de documentación?
- [ ] Esperar aprobación del usuario para cambios > 3 archivos
      o que afecten archivos CRÍTICOS
```

---

### FASE 3: Ejecución

```markdown
- [ ] Aplicar cambios mínimos y enfocados (una cosa a la vez)
- [ ] NO modificar más de lo necesario
- [ ] SIEMPRE preservar el flujo de datos IPC existente
- [ ] NO eliminar funcionalidad existente sin confirmación
- [ ] Documentar cambios en el código (comentarios breves)
- [ ] Si modificas un símbolo exportado: buscar y actualizar TODAS las referencias
- [ ] Si agregas archivos nuevos: registrarlos en CMakeLists.txt
```

**Reglas de edición estrictas:**

| Contexto | Regla |
|----------|-------|
| Audio thread (`processBlock()`) | ❌ NO heap allocation, NO file I/O, NO locks |
| `paint()` de componentes UI | ❌ NO lógica de audio, NO IPC, NO side effects |
| Archivos de Engine/ | ❌ NO dependencias de UI |
| `SharedSlotEntry` struct | ⚠️ Incrementar `kCurrentStructVersion` |
| `BusType` enum | ⚠️ Actualizar `busNames[]`, `kBusColourARGB`, VirtualBusesComponent, SlotRegistry |
| Archivos >500 líneas | ⚠️ Considerar dividir en submódulos |

---

### FASE 4: Validación

```markdown
- [ ] Compilar el target modificado (Release)
- [ ] Compilar ambos plugins (MixCoach_VST3 + Messenger_VST3)
- [ ] Ejecutar tests relevantes:
    - `TestStress128Slots` si tocaste SlotRegistry/SharedMemory
    - `TestIPCIntegration` si tocaste IPC
    - `TestCoachEngine` / `TestPhaseManager` si tocaste engine
    - `TestSmoothValue` si tocaste UI meters
- [ ] Verificar sin warnings nuevos
- [ ] Code review (code-reviewer-deepseek-flash)
- [ ] Si aplica: deploy y probar en FL Studio
- [ ] Actualizar `AI_SESSION_STATE.json` si es relevante
- [ ] Actualizar `workspace_memory/current_state.md` si el cambio es significativo
```

**Comandos de validación rápida:**
```powershell
# Build + Deploy
.\build.ps1

# Solo compilar (más rápido)
cmake --build build --config Release --target MixCoach_VST3 --target Messenger_VST3

# Tests específicos
.\build\tests\Release\TestStress128Slots.exe
.\build\tests\Release\TestIPCIntegration.exe

# Validación completa
.\scripts\validate.ps1
```

---

## 📋 Checklist Pre-Commit

```markdown
Antes de hacer commit:
- [ ] ¿Compila sin errores?
- [ ] ¿Tests pasan?
- [ ] ¿Sin regresiones IPC?
- [ ] ¿Sin heap allocation en audio thread?
- [ ] ¿Sin warnings nuevos?
- [ ] ¿Documentación actualizada si aplica?
- [ ] ¿Checkpoint creado antes de cambios riesgosos?
      → .\scripts\ProjectCheckpoint.ps1 -Action Save -Name "antes_de_x"
```

---

## 🚫 Errores Comunes que Evitar

| Error | Por qué ocurre | Cómo evitarlo |
|-------|---------------|---------------|
| Modificar `Types.h` sin actualizar dependencias | Types.h es usado por 18 archivos | Verificar `depended_by` en PROJECT_GRAPH.json |
| Heap allocation en `processBlock()` | Se necesita memoria temporal | Usar `std::array` o miembros pre-asignados |
| Hardcodear colores de bus | El diseñador quiere cambiarlos después | Usar SIEMPRE `MixCoachTheme` + `Constants.h` |
| Deployar con FL Studio abierto | Los VST3 están bloqueados | Cerrar FL Studio primero o usar `-NoDeploy` |
| Usar Ninja en Release | C1001 en `juce_graphics_Harfbuzz.cpp` | Usar Visual Studio 17 2022 (MSBuild) |
| Olvidar `kCurrentStructVersion` | Shared memory silenciosamente corrupta | Incrementar siempre que se modifique `SharedSlotEntry` |
| Poner lógica en `paint()` | Repaints innecesarios, UI lenta | `paint()` solo dibuja, nunca ejecuta lógica |
| No spawnear code-reviewer | Bugs sutiles que los tests no capturan | Siempre ejecutar code-reviewer en cambios > 10 líneas |

---

*Documento de gobernanza para agentes IA — MixCoach Project*

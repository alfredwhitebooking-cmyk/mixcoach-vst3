# 🚨 RISK_MATRIX.md

> **Clasificación de todos los archivos importantes por nivel de riesgo.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03
> **Fuentes:** `PROJECT_GRAPH.json`, `SYMBOL_GRAPH.json`, `AI_SESSION_STATE.json`, `ERROR_PATTERNS.json`

---

## Leyenda de Riesgo

| Nivel | Significado | Acción requerida antes de modificar |
|-------|-------------|--------------------------------------|
| 🔴 **CRÍTICO** | Afecta a TODO el proyecto. Un error aquí crashea el DAW o corrompe datos. | Aprobación del usuario + tests específicos + code review obligatorio |
| 🟠 **ALTO** | Afecta a un subsistema completo. Puede causar regresiones en IPC, DSP o UI. | Tests del subsistema + code review |
| 🟡 **MEDIO** | Afecta a un componente específico. Riesgo acotado. | Tests del componente |
| 🟢 **BAJO** | Afecta solo a sí mismo o a archivos de documentación. | Validación básica (compila) |

---

## 🔴 CRÍTICO — No modificar sin aprobación

Estos archivos son el **núcleo del sistema**. Cualquier error aquí puede causar crashes en FL Studio, corrupción de datos IPC, o pérdida de telemetría.

| Archivo | Crit. | Dependientes | Riesgo específico |
|---------|:-----:|:------------:|-------------------|
| `Source/Common/types/Types.h` | **10/10** | 18 archivos | Define `TrackTelemetry`, `BusType`, `SlotInfo`, `MentorPhase`. Cambiar cualquier struct aquí repercute en TODO el proyecto. |
| `Source/Common/memory/SlotRegistry.h` | **10/10** | 7 archivos + tests | Corazón del IPC. `registerSlot()`, `syncFromShared()`, `forceFullSync()`. Un error aquí rompe toda la comunicación Messenger↔MixCoach. |
| `Source/Common/memory/SharedMemory.h` | **9/10** | 4 archivos | `SharedSlotEntry`, `SharedMemoryManager`, spinlock. Versiones incorrectas → datos corruptos. |
| `Source/Common/memory/SharedData.h` | **9/10** | 10 archivos | Singleton bridge entre SlotRegistry y SharedMemoryManager. Error de inicialización → ambos plugins mudos. |
| `Source/MixCoach/core/PluginProcessor.h` | **10/10** | 2 archivos (pero dependencias masivas) | Lifecycle VST3 de MixCoach. `ensureSharedData()` con retry/backoff. Error → plugin no carga. |
| `Source/Messenger/core/PluginProcessor.h` | **10/10** | 2 archivos (pero dependencias masivas) | Lifecycle VST3 del Messenger. `processBlock()`, slot registration. Error → crash en audio thread. |
| `Source/Common/types/Constants.h` | **9/10** | 6 archivos | Constantes globales (`kMaxTracks`, `kFFTSize`, `busNames[]`, colores de bus). Cambiar valores aquí afecta límites del sistema. |
| `CMakeLists.txt` | **10/10** | build system completo | Configuración de targets, módulos JUCE, flags MSVC. Error → no compila o VST3 vacío. |

### 🔴 Checklist para modificar archivos CRÍTICOS

```markdown
- [ ] Aprobación explícita del usuario
- [ ] Checkpoint del proyecto: .\scripts\ProjectCheckpoint.ps1 -Action Save -Name "antes_de_cambio_critico"
- [ ] Leer TODOS los archivos que dependen de este (PROJECT_GRAPH.json → "depended_by")
- [ ] Verificar que NO hay símbolos rotos (SYMBOL_GRAPH.json → "referenced_by")
- [ ] Tests específicos para el cambio (mínimo: test unitario que cubra el nuevo comportamiento)
- [ ] Compilar ambos plugins (MixCoach_VST3 + Messenger_VST3)
- [ ] Ejecutar TestStress128Slots completo
- [ ] Code review por code-reviewer-deepseek-flash
- [ ] Si toca SharedSlotEntry: INCREMENTAR kCurrentStructVersion
- [ ] Si toca BusType: actualizar busNames[], kBusColourARGB, VirtualBusesComponent, SlotRegistry
- [ ] Deploy y probar en FL Studio
```

---

## 🟠 ALTO — Requiere tests del subsistema

| Archivo | Crit. | Dependientes | Subsistema |
|---------|:-----:|:------------:|------------|
| `Source/Common/memory/SharedData.cpp` | **8/10** | Implementa SharedData | IPC |
| `Source/Common/memory/SharedMemory.cpp` | **8/10** | Implementa SharedMemoryManager | IPC |
| `Source/Common/memory/SlotRegistry.cpp` | **9/10** | Implementa SlotRegistry | IPC |
| `Source/Common/audio/AudioAnalysis.h` | **9/10** | 1 archivo (AudioAnalyzer.h) | DSP |
| `Source/Common/audio/AudioAnalysis.cpp` | **8/10** | Implementa AudioAnalysis | DSP |
| `Source/MixCoach/core/PluginProcessor.cpp` | **10/10** | Implementa MixCoachAudioProcessor | Brain |
| `Source/MixCoach/core/PluginEditor.h` | **10/10** | 2 archivos | Brain + UI |
| `Source/MixCoach/core/PluginEditor.cpp` | **10/10** | Background worker, timers | Brain + UI |
| `Source/MixCoach/engine/CoachEngine.h` | **9/10** | 1 archivo (PluginProcessor.h) | Engine |
| `Source/MixCoach/engine/CoachEngine.cpp` | **9/10** | Implementa CoachEngine | Engine |
| `Source/MixCoach/engine/PhaseManager.h` | **7/10** | 2 archivos (CoachEngine, Proc) | Engine |
| `Source/MixCoach/engine/PhaseManager.cpp` | **7/10** | Implementa PhaseManager | Engine |
| `Source/Messenger/core/PluginProcessor.cpp` | **10/10** | Implementa MessengerAudioProcessor | Messenger |
| `Source/Messenger/telemetry/TelemetryCollector.h` | **8/10** | 1 archivo (PluginProcessor.h) | Messenger DSP |
| `Source/Messenger/telemetry/TelemetryCollector.cpp` | **8/10** | Implementa TelemetryCollector | Messenger DSP |
| `Source/Common/types/TelemetryData.h` | **7/10** | 2 archivos | Datos |
| `Source/MixCoach/UI/MixCoachTheme.h` | **6/10** | **9 archivos UI** | UI (alto acoplamiento) |
| `Source/MixCoach/UI/MainTabbedComponent.h` | **8/10** | 2 archivos | UI |
| `Source/MixCoach/UI/MainTabbedComponent.cpp` | **8/10** | Implementa tabs | UI |
| `Source/MixCoach/UI/AnalyzersPanelComponent.h` | **8/10** | 1 archivo (MainTabbed) | UI (archivo enorme) |
| `Source/MixCoach/UI/AnalyzersPanelComponent.cpp` | **8/10** | Implementa analyzers | UI (archivo enorme) |
| `Source/MixCoach/audio/AudioAnalyzer.h` | **8/10** | 1 archivo (PluginProcessor) | DSP Brain |
| `Source/MixCoach/audio/AudioAnalyzer.cpp` | **8/10** | Implementa AudioAnalyzer | DSP Brain |
| `build.ps1` | **9/10** | Entry point de build | Build System |
| `DeployVST3.ps1` | **9/10** | Deploy a sistema | Build System |

### 🟠 Checklist para modificar archivos ALTOS

```markdown
- [ ] Leer dependencias directas (PROJECT_GRAPH.json)
- [ ] Tests del subsistema (ej: TestCoachEngine si tocas CoachEngine)
- [ ] Compilar target específico
- [ ] Si tocas UI: verificar con imágenes de referencia (UI_REFERENCES/)
- [ ] Si tocas IPC: ejecutar TestStress128Slots + TestIPCIntegration
- [ ] Code review (recomendado)
```

---

## 🟡 MEDIO — Riesgo acotado

| Archivo | Crit. | Dependientes | Subsistema |
|---------|:-----:|:------------:|------------|
| `Source/MixCoach/UI/CoachChatComponent.h` | **7/10** | 1 archivo (MainTabbed) | UI Chat |
| `Source/MixCoach/UI/CoachChatComponent.cpp` | **7/10** | Implementa chat | UI Chat |
| `Source/Messenger/ui/PluginEditor.h` | **7/10** | 2 archivos | UI Messenger |
| `Source/Messenger/ui/PluginEditor.cpp` | **7/10** | Implementa editor | UI Messenger |
| `Source/Common/types/LogHelper.h` | **5/10** | 9 archivos (uso amplio pero simple) | Utility |
| `Source/MixCoach/UI/ProfessionalAnalyzersComponent.h` | **6/10** | 0 dependientes directos | UI |
| `Source/MixCoach/UI/ProfessionalAnalyzersComponent.cpp` | **6/10** | Implementa | UI |
| `Source/MixCoach/UI/TrackDashboardComponent.h` | **5/10** | 0 dependientes directos | UI |
| `Source/MixCoach/UI/TrackDashboardComponent.cpp` | **5/10** | Implementa | UI |
| `Source/MixCoach/UI/VirtualBusesComponent.h` | **5/10** | 0 dependientes directos | UI |
| `Source/MixCoach/UI/VirtualBusesComponent.cpp` | **5/10** | Implementa | UI |
| `Source/MixCoach/UI/ReferencePanelComponent.h` | **5/10** | 1 archivo (CoachChat) | UI |
| `Source/MixCoach/UI/ReferencePanelComponent.cpp` | **5/10** | Implementa | UI |
| `scripts/validate.ps1` | **7/10** | Build validation | Build System |

### 🟡 Checklist para modificar archivos MEDIOS

```markdown
- [ ] Compilar target
- [ ] Tests básicos del componente
- [ ] Verificar que no se introdujeron dependencias circulares
```

---

## 🟢 BAJO — Bajo riesgo

| Archivo | Crit. | Dependientes | Subsistema |
|---------|:-----:|:------------:|------------|
| `AI_CONTEXT.md` | **N/A** | Documentación | Docs |
| `AGENTS.md` | **N/A** | Documentación | Docs |
| `README.md` | **N/A** | Documentación | Docs |
| `HOW_TO_WORK_ON_THIS_PROJECT.md` | **N/A** | Gobernanza IA | Docs |
| `PROJECT_PRIORITIES.md` | **N/A** | Gobernanza IA | Docs |
| `APPROVED_PATTERNS.md` | **N/A** | Gobernanza IA | Docs |
| `RISK_MATRIX.md` | **N/A** | Gobernanza IA | Docs |
| `DEFINITION_OF_DONE.md` | **N/A** | Gobernanza IA | Docs |
| `DECISIONS_LOG_TEMPLATE.md` | **N/A** | Gobernanza IA | Docs |
| `AGENT_MODES.md` | **N/A** | Gobernanza IA | Docs |
| `IPC_CONTRACT.md` | **N/A** | Documentación | Docs |
| `KNOWN_ERRORS.md` | **N/A** | Documentación | Docs |
| `PROJECT_INDEX.json` | **N/A** | Índice IA | AI Tools |
| `PROJECT_GRAPH.json` | **N/A** | Grafo IA | AI Tools |
| `SYMBOL_GRAPH.json` | **N/A** | Grafo IA | AI Tools |
| `EMBEDDING_STORE.json` | **N/A** | Embeddings IA | AI Tools |
| `ERROR_PATTERNS.json` | **N/A** | Patrones IA | AI Tools |
| `AI_SESSION_STATE.json` | **N/A** | Sesión IA | AI Tools |
| `context_selector.py` | **N/A** | Herramienta IA | AI Tools |
| `token_optimizer.py` | **N/A** | Herramienta IA | AI Tools |
| `project_intelligence.py` | **N/A** | Herramienta IA | AI Tools |
| `ai_build_loop.py` | **N/A** | Herramienta IA | AI Tools |
| `workspace_memory/*` | **N/A** | Memoria de proyecto | Docs |
| `tests/*.cpp` | **N/A** | Tests | Tests |

### 🟢 Checklist para modificar archivos BAJOS

```markdown
- [ ] Actualizar documentación relacionada
- [ ] Si tocas AI tools: mantener compatibilidad con PROJECT_GRAPH.json / SYMBOL_GRAPH.json
- [ ] Si tocas tests: compilar y ejecutar
```

---

## 📊 Resumen por Subsistema

| Subsistema | Archivos | Criticidad promedio | Riesgo de cambio |
|------------|:--------:|:-------------------:|:-----------------|
| **Common/types/** | 4 | 9/10 | 🔴 Alto — afecta a TODO |
| **Common/memory/** (IPC) | 6 | 9/10 | 🔴 Alto — núcleo IPC |
| **Common/audio/** (DSP) | 2 | 8/10 | 🟠 Medio-Alto |
| **MixCoach/core/** | 4 | 10/10 | 🔴 Crítico — lifecycle |
| **MixCoach/engine/** | 4 | 8/10 | 🟠 Medio-Alto |
| **MixCoach/UI/** | 14 | 6/10 | 🟡 Medio (excepto MixCoachTheme) |
| **Messenger/core/** | 2 | 10/10 | 🔴 Crítico — lifecycle |
| **Messenger/telemetry/** | 2 | 8/10 | 🟠 Medio-Alto |
| **Messenger/ui/** | 2 | 7/10 | 🟡 Medio |
| **Build System** | 4 | 8/10 | 🟠 Medio-Alto |
| **Tests** | ~10 | N/A | 🟢 Bajo |
| **Documentación** | 15+ | N/A | 🟢 Bajo |

---

## 🔗 Dependencias entre RISK_MATRIX.md y otros documentos

| Documento | Relación |
|-----------|----------|
| `PROJECT_PRIORITIES.md` | Define qué prioridades protege cada nivel de riesgo |
| `HOW_TO_WORK_ON_THIS_PROJECT.md` | FASE 2 usa este documento para evaluar riesgo |
| `APPROVED_PATTERNS.md` | Archivos CRÍTICOS requieren patrones específicos |
| `DEFINITION_OF_DONE.md` | Archivos CRÍTICOS requieren validación extra |

---

*Documento de gobernanza para agentes IA — MixCoach Project*

# ✅ DEFINITION_OF_DONE.md

> **Criterios para considerar una tarea como terminada.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## Criterios Esenciales (Mínimo Obligatorio)

Una tarea NO está terminada hasta que TODOS estos criterios se cumplen:

```markdown
- [ ] Compila sin errores (Release config)
- [ ] Tests relevantes pasan
- [ ] Sin regresiones IPC (si aplica)
- [ ] Sin degradación de CPU (si aplica)
- [ ] Sin warnings nuevos
- [ ] Sin heap allocation en audio thread
```

---

## Criterios por Tipo de Cambio

### 🧠 Cambio en Engine/Mentoría
```markdown
- [ ] Compila MixCoach_VST3 + Messenger_VST3 (Release)
- [ ] TestCoachEngine pasa (41 tests)
- [ ] TestPhaseManager pasa (80 tests)
- [ ] Sin nuevas dependencias de UI en engine/
- [ ] Mensajes de mentoría en español, tono profesional
- [ ] Fase de mentoría documentada en PhaseManager
```

### 🔌 Cambio en IPC/Memory
```markdown
- [ ] Compilan ambos plugins (Release)
- [ ] TestStress128Slots pasa (1595 tests, 0 failures)
- [ ] TestIPCIntegration pasa (86 tests)
- [ ] TestSlotRegistry pasa
- [ ] Si se modificó SharedSlotEntry: kCurrentStructVersion incrementado
- [ ] Shared memory y backup files: ambos canales verificados
- [ ] Sin deadlocks en escenario de 128 slots concurrentes
- [ ] Graceful degradation cuando SHM no está disponible
```

### 🎨 Cambio en UI
```markdown
- [ ] Compila MixCoach_VST3 (Release)
- [ ] UI coincide con imágenes de referencia (UI_REFERENCES/)
- [ ] Sin hardcodeo de colores (usar MixCoachTheme + Constants.h)
- [ ] Timer a 30fps mantenido (sin operaciones lentas en timerCallback)
- [ ] Sin lógica de audio en paint() o componentes UI
- [ ] Sin fugas de memoria (componentes JUCE addAndMakeVisible balanceados)
- [ ] Responsive a resize (componentes con resized() correcto)
```

### 🎵 Cambio en DSP/Audio
```markdown
- [ ] Compila Messenger_VST3 (Release)
- [ ] TestTelemetry pasa
- [ ] Sin NaN/denormals (safe_sqrt, safe_atan2 usados)
- [ ] Sin heap allocation en processBlock()
- [ ] CPU < 0.05% por Messenger en 96 samples/block
- [ ] FFT cada 4 bloques máx (no cada bloque)
- [ ] Valores dB dentro de rango esperado (-60dB a +6dB)
```

### 🔧 Cambio en Build System
```markdown
- [ ] build.ps1 funciona (Release + Debug)
- [ ] validate.ps1 pasa (11 checks)
- [ ] Ambos VST3s se despliegan correctamente
- [ ] Tests C++ se ejecutan desde run_tests target
- [ ] Sin cambios en flags de compilación que afecten rendimiento
```

### 📝 Cambio en Documentación
```markdown
- [ ] Markdown válido (sin broken links, sin tablas mal formateadas)
- [ ] Información factual verificada contra código fuente
- [ ] Archivos de IA (PROJECT_GRAPH.json, AI_CONTEXT.md) actualizados si cambió la arquitectura
- [ ] workspace_memory/ actualizado si cambió estructura
- [ ] AI_SESSION_STATE.json actualizado si es relevante
```

---

## Criterios de Calidad (Recomendados)

```markdown
- [ ] Código revisado por code-reviewer-deepseek-flash
- [ ] Sin código duplicado (DRY)
- [ ] Funciones < 50 líneas
- [ ] Archivos < 500 líneas (si se supera, considerar dividir)
- [ ] Comentarios en español o inglés consistentes
- [ ] Nombres de variables/funciones descriptivos (no abreviaturas crípticas)
- [ ] LogHelper.writeToLog() para errores (no solo excepciones silenciosas)
```

---

## Criterios de Documentación para IA

```markdown
- [ ] El cambio es comprensible por el próximo agente IA
- [ ] Si se modificó un símbolo exportado: todas las referencias actualizadas
- [ ] Si se agregó un archivo: registrado en CMakeLists.txt
- [ ] Si se eliminó un archivo: referencias limpiadas en PROJECT_GRAPH.json y SYMBOL_GRAPH.json
```

---

## 🚫 Lo que NO califica como "Done"

| Situación | Por qué no es válido |
|-----------|---------------------|
| "Compila pero no lo probé en FL Studio" | El comportamiento en DAW real es impredecible |
| "Los tests pasan pero hay warnings" | Warnings pueden indicar bugs sutiles |
| "Funciona en Debug pero no en Release" | Release es el target de producción |
| "Solo cambié una línea, no necesita test" | Una línea puede romper IPC |
| "No toqué tests porque el cambio es pequeño" | Tests son la red de seguridad |
| "No actualicé documentación porque es obvio" | El próximo agente IA no sabe lo que cambiaste |
| "El código está comentado" | Prefiero código auto-explicativo a comentarios post-facto |
| "Funciona con 10 tracks, seguro funciona con 60" | No asumas escalabilidad lineal |

---

## 📋 Resumen: Checklist Universal

```markdown
Toda tarea completada debe tener:

[FASE 1] Contexto
   □ AGENTS.md leído
   □ AI_CONTEXT.md consultado
   □ workspace_memory/ actualizado

[FASE 2] Implementación
   □ Cambios mínimos y enfocados
   □ Sin heap allocation en audio thread
   □ Sin hardcodeo de colores
   □ Sin lógica de UI en engine/ o viceversa
   □ Sin lógica de audio en UI/ o viceversa

[FASE 3] Validación
   □ Compila Release sin errores
   □ Tests relevantes pasan
   □ Code review realizado
   □ Sin regresiones IPC
   □ Sin degradación de CPU

[FASE 4] Documentación
   □ Cambio documentado (este archivo o comentarios en código)
   □ workspace_memory/current_state.md actualizado (si cambio significativo)
   □ AI_SESSION_STATE.json considerado
```

---

*Documento de gobernanza para agentes IA — MixCoach Project*

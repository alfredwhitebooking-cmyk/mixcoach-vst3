# 📝 DECISIONS_LOG_TEMPLATE.md

> **Plantilla para registrar decisiones arquitectónicas futuras (ADR).**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## Instrucciones

Usa esta plantilla cada vez que tomes una decisión arquitectónica significativa:
- Cambio en la estructura de IPC
- Nueva dependencia externa
- Cambio en el flujo de datos
- Decisión que afecta a ambos plugins (Messenger + MixCoach)
- Decisión que afecta compatibilidad futura

---

## 📄 Plantilla

```markdown
# ADR-[NÚMERO]: [Título descriptivo de la decisión]

- **Fecha:** YYYY-MM-DD
- **Estado:** [Propuesto / Aceptado / Reemplazado por ADR-X]
- **Autor:** [Nombre del agente/humano]

---

## Contexto

[¿Qué problema estamos resolviendo? ¿Qué limitación del sistema actual nos motiva?
Incluir referencias a archivos, bugs, o limitaciones observadas.]

Ejemplo:
> Con 60+ Messengers en paralelo, el spinlock con `Sleep(0)` causaba saturación
> del scheduler de Windows. Cada Messenger hacía 2 adquisiciones de lock por
> bloque de audio (120 locks/bloque con 60 Msgrs). FL Studio crasheaba durante
> la inserción masiva de plugins.

## Decisión

[¿Qué decidimos hacer? Describir el cambio concreto.]

Ejemplo:
> Reemplazar spinlock monofásico con two-phase: 1000 iteraciones de `_mm_pause()`
> (sin syscall, ~1μs) seguidas de `Sleep(0)` solo si el lock sigue ocupado.
> Además, crear `writeSlotTelemetry()` que adquiere el lock 1 sola vez en vez de 2.

## Consecuencias

### Positivas (+)
- [Beneficio 1]
- [Beneficio 2]

### Negativas (-)
- [Costo/riesgo 1]
- [Costo/riesgo 2]

### Neutrales (~)
- [Cambio no funcional 1]

## Archivos Afectados

| Archivo | Cambio |
|---------|--------|
| `Source/Common/memory/SharedMemory.cpp` | [Descripción del cambio] |
| `Source/Messenger/core/PluginProcessor.cpp` | [Descripción del cambio] |

## Riesgo Asociado

| Riesgo | Probabilidad | Mitigación |
|--------|:------------:|------------|
| [Riesgo 1] | Alta/Media/Baja | [Mitigación] |
| [Riesgo 2] | Alta/Media/Baja | [Mitigación] |

## Validación

- [ ] Test unitario que cubre el cambio
- [ ] Test de estrés (128 slots) pasa
- [ ] Compilación Release exitosa
- [ ] Deploy verificado en FL Studio

## Alternativas Consideradas

| Alternativa | Razón para NO elegirla |
|-------------|------------------------|
| [Alternativa 1] | [Por qué se descartó] |
| [Alternativa 2] | [Por qué se descartó] |

## Referencias

- `PROJECT_PRIORITIES.md` — [Prioridad que protege]
- `APPROVED_PATTERNS.md` — [Patrón usado]
- `RISK_MATRIX.md` — [Nivel de riesgo]
- `KNOWN_ERRORS.md` — [Bug relacionado si aplica]
```

---

## 📋 Ejemplo Completado

```markdown
# ADR-001: Two-Phase Spinlock para Reducir Contención con 60+ Messengers

- **Fecha:** 2026-06-03
- **Estado:** Aceptado
- **Autor:** Codebuff AI Agent

---

## Contexto

Con 60+ Messengers en paralelo, cada uno adquiría el spinlock de shared memory
2 veces por bloque de audio (readSlot + writeSlot). El spinlock usaba `Sleep(0)`
en CADA iteración del while, causando context switches completos (~1-15μs cada
uno). El scheduler de Windows se saturaba con 60 threads compitiendo.

Además, cada `registerSlot()` escribía un backup file síncrono. Con 60+
Messengers creándose en paralelo (FL Studio copia masiva), 60+ operaciones de
archivo simultáneas saturaban el I/O → timeout → crash del DAW.

## Decisión

1. Spinlock two-phase: `_mm_pause()` × 1000 iteraciones + `Sleep(0)` solo después
2. `writeSlotTelemetry()`: nuevo método que adquiere 1 lock en vez de 2
3. Backup file diferido: del audio thread al message thread (~500ms después)
4. Backup en `registerSlot()` solo cuando SHM no está disponible

## Consecuencias

### Positivas
- 50% menos adquisiciones de lock por Messenger (120 → 60 locks/bloque)
- Cero context switches en spinlock (sin `Sleep(0)` en hot path)
- Cero file I/O durante inserción masiva de Messengers
- Cero file I/O en audio thread (caso normal con SHM)

### Negativas
- Backup files no se crean inmediatamente al registrar slot (~500ms de retraso)
- Si FL Studio crashea antes del backup diferido, el slot no persiste en disco
  (mitigación: SHM + proyecto FL Studio tienen los datos)

### Neutrales
- `_mm_pause()` es Intel/AMD-only (no ARM). El target es Windows x86-64.

## Archivos Afectados

| Archivo | Cambio |
|---------|--------|
| `Source/Common/memory/SharedMemory.cpp` | Two-phase spinlock + writeSlotTelemetry() |
| `Source/Common/memory/SharedMemory.h` | Nueva función writeSlotTelemetry() |
| `Source/Common/memory/SlotRegistry.cpp` | Backup condicional en registerSlot() |
| `Source/Messenger/core/PluginProcessor.h` | Nueva flag slotRegistered_ + pendingBackupWrite_ |
| `Source/Messenger/core/PluginProcessor.cpp` | Backup diferido al timer + slotRegistered_ flag |

## Riesgo Asociado

| Riesgo | Probabilidad | Mitigación |
|--------|:------------:|------------|
| `_mm_pause()` no disponible en ARM Windows | Baja | Target es x86-64, no ARM |
| Backup diferido no se escribe | Baja | Timer se ejecuta siempre; si falla, reintenta |
| Slot sin backup si FL crash temprano | Media | SHM + FL project file preservan datos |

## Validación

- [✅] TestStress128Slots (1595 tests, 0 failures)
- [✅] Compilación Release MixCoach + Messenger
- [✅] Deploy verificado
- [✅] Code review sin issues críticos

## Alternativas Consideradas

| Alternativa | Razón para NO elegirla |
|-------------|------------------------|
| Eliminar backup files completamente | Se perdería persistencia cross-sesión sin SHM |
| Usar std::mutex en vez de spinlock | std::mutex causa priority inversion en audio thread |
| Queue de backups con hilo dedicado | Complejidad innecesaria; el timer del Messenger basta |

## Referencias

- PROJECT_PRIORITIES.md — Prioridad 1: Estabilidad, Prioridad 2: Integridad IPC
- APPROVED_PATTERNS.md — P6: Two-Phase Spinlock, P4: Background Worker
- RISK_MATRIX.md — 🔴 Crítico: SharedMemory.h, SlotRegistry.h
```

---

## 📂 Ubicación de ADRs

Los ADRs se registran en el archivo acumulativo en la raíz del proyecto:
```
MixCoach/DECISION_LOG.md
```

Agrega cada nuevo ADR al final del archivo, incrementando el número de ADR
secuencialmente. Cada ADR debe incluir: contexto, decisión, consecuencias,
alternativas descartadas, y referencias cruzadas.

> ⚠️ **NO crear archivos individuales por ADR.** Usar el archivo único `DECISION_LOG.md`
> para mantener todos los ADRs en un solo lugar, visible y referenciable.

---

## 🔗 Referencias Cruzadas

| Campo | Documentos relacionados |
|-------|------------------------|
| Contexto | `KNOWN_ERRORS.md`, `AI_CONTEXT.md` (hotspots) |
| Decisión | `APPROVED_PATTERNS.md` (patrón usado) |
| Archivos | `RISK_MATRIX.md` (nivel de riesgo) |
| Prioridad | `PROJECT_PRIORITIES.md` |
| Validación | `DEFINITION_OF_DONE.md` |

---

*Documento de gobernanza para agentes IA — MixCoach Project*

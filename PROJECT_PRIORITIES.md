# 🎯 PROJECT_PRIORITIES.md

> **Prioridades absolutas del proyecto MixCoach.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## ⚡ Jerarquía de Prioridades

```
ESTABILIDAD > FUNCIONALIDAD > RENDIMIENTO > UI/UX > DOCUMENTACIÓN
```

**Esta jerarquía es inviolable.** Ninguna mejora de UI justifica un crash.
Ninguna optimización de rendimiento justifica corromper datos IPC.

---

## 🥇 Prioridad 1: Estabilidad del Sistema

### Reglas absolutas

| Regla | Por qué | Violación = |
|-------|---------|-------------|
| **Nunca crashear el DAW** | FL Studio no tolera plugins que crashean | Plugin deshabilitado, usuario furioso |
| **Nunca perder datos de slot** | La telemetría de 60+ pistas es valiosa | Mezcla incorrecta, mentoría errónea |
| **Nunca bloquear el audio thread** | El audio thread debe ser RT | Pop, crack, crash del DAW |
| **Nunca corromper shared memory** | Otros plugins dependen de ella | Slots huérfanos, datos basura |
| **Siempre graceful degradation** | Si algo falla, el sistema sigue funcionando | UX frágil, crashes evitables |

### Comportamiento esperado en fallo

```
Fallo en Shared Memory
    → MixCoach sigue funcionando con backup files
    → Los meters pueden tardar ~1s en actualizarse
    → Nunca crashear, nunca congelar UI

Fallo en registro de slot (todos ocupados)
    → registerSlot() retorna -1
    → Messenger funciona sin telemetría (no crashea)
    → LogHelper registra el evento

Fallo en backup file (disco lleno)
    → saveSlotToBackupFile() falla silenciosamente
    → shared memory sigue funcionando
    → LogHelper registra el evento
```

---

## 🥈 Prioridad 2: Integridad del IPC

### Contrato inviolable

```markdown
- La comunicación Messenger → MixCoach NUNCA debe romperse
- Ambos canales (shared memory + backup files) deben funcionar independientemente
- Los backup files SIEMPRE prevalecen para metadatos (nombre, color, bus)
- Shared memory SIEMPRE prevalece para telemetría en tiempo real
- kCurrentStructVersion debe incrementarse al modificar SharedSlotEntry
- Si struct version mismatch → shared memory se reinicia, backup files preservan metadatos
```

### Restricciones de threading IPC

| Operación | Thread permitido | Prohibido en |
|-----------|-----------------|--------------|
| `acquireLock()` | Background thread | Audio thread |
| `writeSlot()` | Background thread | Audio thread |
| `syncFromShared()` | Timer/background | Audio thread |
| `forceFullSync()` | Background worker | Timer de UI |
| `loadSlotsFromBackupFiles()` | Background worker | Timer de UI |

---

## 🥉 Prioridad 3: Rendimiento del Audio Thread

### Lo que NUNCA debe pasar en processBlock()

```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) {
    // ❌ PROHIBIDO:
    std::vector<float> temp(buffer.getNumSamples());  // Heap allocation
    std::string name = trackName_;                     // Heap allocation
    juce::File file; file.create();                    // File I/O
    std::lock_guard<std::mutex> lock(mutex_);          // Blocking lock
    logger_->writeToLog("...");                        // Logger I/O

    // ✅ PERMITIDO:
    float localBuffer[128];                            // Stack allocation
    std::memcpy(&entry, &slot, sizeof(entry));         // Stack operations
    atomic_counter_.load();                            // Lock-free atomics
}
```

### Límites de CPU

| Componente | Presupuesto CPU | Medición |
|------------|----------------|----------|
| Messenger `processBlock()` | < 0.05% | En DAW con 96 samples/block |
| TelemetryCollector DSP | < 0.03% | Peak + RMS + FFT cada 4 bloques |
| MixCoach timer (30fps) | < 1% | Con 60+ tracks activos |
| Background worker | < 2% | En ráfagas cortas (~5ms cada 5s) |
| `forEachActive()` | < 100μs | Con 128 slots registrados |

---

## 4. Prioridad 4: Fluidez de UI

### Targets

| Métrica | Target | Medición |
|---------|--------|----------|
| Timer principal (MixCoach) | 30fps (33ms) | `tickCounter_` basado |
| Sync de slots | ~3fps (cada 330ms) | Cada 10 ticks |
| Update de messengers | ~10fps (cada 100ms) | Cada 3 ticks |
| Repaint de meters | < 5ms | Profiling en Release |
| `bgLock_.tryEnter()` | Nunca bloquear > 1ms | Timeout configurables |

### Lo que NUNCA debe pasar en el timer de UI

```
❌ File I/O síncrono (loadSlotsFromBackupFiles)
❌ Spinlock acquisition prolongada (>1ms)
❌ Heap allocation en cada tick
❌ Operaciones O(n²) con n = número de tracks
```

---

## 5. Prioridad 5: Compatibilidad y Mantenibilidad

### Compatibilidad

| Requisito | Detalle |
|-----------|---------|
| Windows 10/11 | Única plataforma soportada |
| FL Studio | DAW primario |
| Otros DAWs | Compatibles con VST3 estándar |
| JUCE 8.0.13 | Versión fijada del framework |
| MSVC 2022 | Compilador oficial (no Ninja en Release) |

### Mantenibilidad

```markdown
- NO duplicar lógica entre Messenger y MixCoach
- NO código sin test (mínimo: compila sin errores)
- NO eliminar archivos de backup/workspace_memory sin migración
- SIEMPRE mantener AI_CONTEXT.md actualizado (es el punto de entrada IA)
- SIEMPRE mantener workspace_memory/ actualizado
```

---

## 📊 Matriz de Trade-offs

| Si priorizas... | Sacrificas... | Ejemplo |
|-----------------|---------------|---------|
| Rendimiento de audio thread | Precisión de telemetría (menos FFTs) | FFT cada 4 bloques en vez de cada bloque |
| Fluidez de UI | Frecuencia de sync | syncFromShared() a 3fps en vez de 10fps |
| Persistencia de datos | Velocidad de registro | Backup file diferido (~500ms) vs síncrono |
| Compatibilidad cross-DAW | Features específicas de FL Studio | Sandbox handling específico de FL |
| Simplicidad del código | Optimizaciones agresivas | Preferir std::vector sobre allocators custom |

---

## 🔗 Referencias

| Documento | Contenido relevante |
|-----------|-------------------|
| `AI_CONTEXT.md` | Reglas críticas, hotspots, prioridades por tipo de cambio |
| `IPC_CONTRACT.md` | Contrato formal de comunicación IPC |
| `KNOWN_ERRORS.md` | Errores conocidos que violan estas prioridades |
| `HOW_TO_WORK_ON_THIS_PROJECT.md` | Flujo de trabajo que protege estas prioridades |
| `ERROR_PATTERNS.json` | Patrones de error asociados a violaciones comunes |

---

*Documento de gobernanza para agentes IA — MixCoach Project*

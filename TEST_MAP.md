# 🧪 TEST_MAP.md — Mapa de Tests por Subsistema

> **Qué prueba cada test, cómo ejecutarlo, y qué esperar.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## ⚡ Ejecución Rápida

```powershell
# Todos los tests
cmake --build build --config Release --target run_tests

# Test individual
.\build\tests\Release\TestStress128Slots.exe
```

---

## 🧪 Tests de C++

### TestStress128Slots (1595 tests) — 🔴 IPC Crítico

**Qué cubre:** El corazón del sistema IPC.
- `SlotRegistry` — register, release, forEachActive, syncFromShared, forceFullSync
- `SharedMemory` — CreateFileMappingW, spinlock, readSlot, writeSlot
- `SharedData` — Singleton, getInstance, bridge SHM↔backup
- Escenario de estrés: 128 slots concurrentes, lecturas/escrituras simultáneas
- Graceful degradation: SHM no disponible → backup files

**Cuándo ejecutar:** SIEMPRE que toques archivos de `Common/memory/` o `Common/types/`.

```powershell
.\build\tests\Release\TestStress128Slots.exe
# Esperado: 1595 passed, 0 failed
# Tiempo: ~2s
```

---

### TestIPCIntegration (86 tests) — 🟠 IPC Funcional

**Qué cubre:** Integración real del pipeline IPC.
- `SharedData` — inicialización, reconexión, fallo
- `SharedMemory` — versión de struct, reinicio silencioso
- Backup files — escritura, lectura, CRC32
- Comunicación bidireccional Messenger ↔ MixCoach

**Cuándo ejecutar:** Cuando toques `SharedData.cpp`, formato de backup, o flujo IPC.

```powershell
.\build\tests\Release\TestIPCIntegration.exe
# Esperado: 86 passed, 0 failed
# Tiempo: ~1s
```

---

### TestCoachEngine (41 tests) — 🟠 Mentoría

**Qué cubre:** El motor de análisis y mentoría.
- `CoachEngine` — analyzeGainStaging, analyzeOrganisation, analyzeTonalBalance, analyzeDynamics, analyzePhase
- Detección de clipping, fase negativa, enmascaramiento espectral
- Mensajes de mentoría generados
- PeriodicAnalysisTrigger y fases

**Cuándo ejecutar:** Cuando toques `CoachEngine.cpp`, `CoachEngine.h`, o lógica de análisis.

```powershell
.\build\tests\Release\TestCoachEngine.exe
# Esperado: 41 passed, 0 failed
# Tiempo: ~1s
```

---

### TestPhaseManager (80 tests) — 🟡 Fases de Mentoría

**Qué cubre:** La máquina de estados de fases.
- `PhaseManager` — todas las transiciones de fase
- Comandos /next, /reset, /phase
- Progreso por fase
- Estados de completitud

**Cuándo ejecutar:** Cuando toques `PhaseManager.cpp`, `PhaseManager.h`, o las fases.

```powershell
.\build\tests\Release\TestPhaseManager.exe
# Esperado: 80 passed, 0 failed
# Tiempo: ~1s
```

---

### TestSmoothValue — 🟢 UI Meters

**Qué cubre:** El sistema de suavizado de meters.
- `SmoothValue` — ballistics, attack/release, targets
- Diferentes configuraciones de sample rate
- Comportamiento con values extremos

**Cuándo ejecutar:** Cuando toques `SmoothValue.h` o meters de UI.

```powershell
.\build\tests\Release\TestSmoothValue.exe
# Esperado: all passed, 0 failed
# Tiempo: ~0.5s
```

---

### Tests Adicionales

| Test | Archivos | Cómo ejecutar |
|------|----------|--------------|
| TestMeterComponent | Componentes de meter UI | `TestMeterComponent.exe` |
| TestLUFSMeter | LUFSMeter, LoudnessMeter | `TestLUFSMeter.exe` |
| TestSpectrographComponent | SpectrographComponent | `TestSpectrographComponent.exe` |
| TestVectorscopeComponent | VectorscopeComponent | `TestVectorscopeComponent.exe` |
| TestPhaseCorrelationMeter | PhaseCorrelationMeter | `TestPhaseCorrelationMeter.exe` |
| TestSlotRegistry | SlotRegistry unitario | `TestSlotRegistry.exe` |

---

## 🐍 Tests de Python

### test_ms_calculation.py — 🟡 Verificación de LUFS

**Qué cubre:** Cálculos de mean square para LUFS, verificación contra estándar EBU R128.

```powershell
cd tests
python test_ms_calculation.py
```

---

## 📊 Matriz de Cobertura

```
                    ┌─────────────────────────────────────────────────┐
                    │  SlotReg  ShMem   Coach   Phase   Smooth   IPC  │
                    │          Ry      Engine  Mgr     Value    Int   │
┌───────────────────┼─────────────────────────────────────────────────┤
│ TestStress128Slots│   ✅      ✅      -       -       -       ✅   │
│ TestIPCIntegration│   ✅      ✅      -       -       -       ✅   │
│ TestCoachEngine   │   -       -      ✅      -       -       -    │
│ TestPhaseManager  │   -       -      -      ✅       -       -    │
│ TestSmoothValue   │   -       -      -       -      ✅       -    │
│ TestSlotRegistry  │   ✅      -      -       -       -       -    │
└───────────────────┴─────────────────────────────────────────────────┘
```

---

## 🚨 Lo que NO cubren los tests

| Aspecto | Riesgo | Cobertura manual |
|---------|:------:|------------------|
| Comportamiento en FL Studio real | 🔴 Alto | Deploy + prueba manual |
| Rendimiento con 100+ pistas | 🟠 Medio | Perfilamiento en DAW real |
| UI visual (píxel perfect) | 🟡 Medio | Comparación con UI_REFERENCES/ |
| Memoria (fugas) | 🟡 Medio | Verificación visual en Task Manager |
| Latencia de UI | 🟢 Bajo | Percepción visual |

---

## 🔗 Referencias

| Documento | Relación |
|-----------|----------|
| `DEFINITION_OF_DONE.md` | Checklist por tipo de cambio que referencia estos tests |
| `HOW_TO_WORK_ON_THIS_PROJECT.md` | FASE 4: Validación con comandos de tests |
| `AGENT_MODES.md` | Cada modo lista sus validaciones requeridas |

---

*Mapa de tests para agentes IA — MixCoach Project*

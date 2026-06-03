# 🏃 QUICK_REFERENCE.md — Cheat Sheet para Agentes IA

> **Para IAs que YA conocen el proyecto. Si es tu primera vez, empieza por `AGENTS.md` + `HOW_TO_WORK_ON_THIS_PROJECT.md`.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## 📚 Documentos en Orden de Lectura (IA nueva → IA experta)

```
1. AGENTS.md                  (~2 min)   → Visión general, north star
2. HOW_TO_WORK_ON_THIS_PROJECT.md (~3 min) → Flujo obligatorio 5 fases
3. PRODUCT_VISION.md          (~3 min)   → Alma del producto, QUÉ ES / NO ES
4. [El documento relevante a tu tarea]   → Ver tabla abajo
```

---

## 🎯 Modo Rápido por Tipo de Cambio

| Si tu cambio es... | Usa modo | Documentación obligatoria |
|-------------------|:--------:|--------------------------|
| 🎨 UI, visual, tema | `MODE_UI` | `visual_design.md`, `component_map.md`, `ui_map.yaml`, `UI_REFERENCES/*.png` |
| 🎵 Audio DSP, FFT, LUFS | `MODE_DSP` | `GLOSSARY.md`, `ERROR_PATTERNS.json` |
| 🔌 IPC, memoria compartida | `MODE_IPC` | `IPC_CONTRACT.md`, `DECISION_LOG.md`, `ERROR_PATTERNS.json` |
| 🔧 Build, deploy, scripts | `MODE_BUILD` | `ERROR_PATTERNS.json` (sección CMake) |
| 🏗️ Multi-subsistema | `MODE_ARCHITECTURE` | `DECISION_LOG.md`, `RISK_MATRIX.md`, `PROJECT_GRAPH.json` |
| 📝 Documentación | — | `DEFINITION_OF_DONE.md` (sección docs) |

---

## ⚡ 5 Reglas de Oro (No Negociables)

| # | Regla | Violación = |
|:-:|-------|:-----------:|
| 1 | ❌ **NUNCA** heap alloc en `processBlock()` | Crash, popping, DAW deshabilita plugin |
| 2 | ❌ **NUNCA** file I/O en audio thread | Timeout, crash durante inserción masiva |
| 3 | ❌ **NUNCA** hardcodear colores de bus | Inconsistencia visual, difícil de cambiar |
| 4 | ❌ **NUNCA** modificar `SharedSlotEntry` sin ++`kCurrentStructVersion` | Datos corruptos silenciosamente |
| 5 | ✅ **SIEMPRE** graceful degradation: si SHM falla → backup files | FL Studio no crashea nunca |

---

## 🛠️ Comandos Rápidos

```powershell
# Build completo + deploy
.\build.ps1

# Solo build (más rápido)
cmake --build build --config Release

# Build targets específicos
cmake --build build --config Release --target MixCoach_VST3
cmake --build build --config Release --target Messenger_VST3

# Deploy
.\DeployVST3.ps1 -Config Release -Plugin MixCoach
.\DeployVST3.ps1 -Config Release -Plugin Messenger

# Tests
.\build\tests\Release\TestStress128Slots.exe    # IPC (1595 tests)  → Ver TEST_MAP.md
.\build\tests\Release\TestIPCIntegration.exe    # IPC (86 tests)
.\build\tests\Release\TestCoachEngine.exe       # Mentoría (41 tests)
.\build\tests\Release\TestPhaseManager.exe      # Fases (80 tests)
.\build\tests\Release\TestSmoothValue.exe       # UI meters

# Validación completa
.\scripts\validate.ps1

# Validación de patrones (12 checks contra APPROVED_PATTERNS.md)
python .\scripts\pattern_validator.py

# Checkpoint antes de cambios riesgosos
.\scripts\ProjectCheckpoint.ps1 -Action Save -Name "antes_de_cambio"
```

---

## 🚫 Antipatrones Mortales

```markdown
✗ Sleep(0) en spinlock sin _mm_pause()       → Context switch por iteración
✗ Ninja como generator en Release            → C1001 crash del compilador
✗ registerSlot() con file I/O síncrono       → 60+ archivos simultáneos saturan I/O
✗ buffer grande en stack (float[274KB])      → Stack overflow con 60+ tracks
✗ createEditor() inicializando IPC           → Crash durante escaneo VST3
✗ processBlock() sin verificar prepared_     → FL Studio llama antes de prepareToPlay
```

---

## 📋 Checklist Pre-Commit Exprés

```
□ Compila?       (Release, ambos plugins)
□ Tests?         (los relevantes a tu cambio)
□ Sin warnings?  (0 nuevos)
□ Code review?   (code-reviewer-deepseek-flash)
□ Deploy?        (a C:\Program Files\Common Files\VST3\)
```

---

*Cheat sheet para agentes IA — MixCoach Project*

# Current State — MixCoach

## Estado Actual del Proyecto
- **Versión**: 1.0.0
- **Framework**: JUCE 8.0.13
- **C++ Standard**: 20
- **Plataforma**: Windows (VST3)
- **Build**: CMake + MSVC

## Estado IA / Multi-agente
- **Entrada canonica para agentes**: `AGENTS.md`
- **Contexto largo consolidado**: `AI_CONTEXT.md`
- **Referencias UI fuente de verdad**: `UI_REFERENCES/Messenger.png`, `UI_REFERENCES/MixCoach_Tab1_AICoach.png`, `UI_REFERENCES/MixCoach_Tab2_Analyzers.png`
- **Guia visual detallada**: `workspace_memory/visual_design.md`
- **Objetivo del producto**: mentor de mezcla + analizadores; NO procesador que altera audio
- **Validacion reciente**: 12 suites C++ ejecutadas directamente desde `build/tests/Release`, 0 fallos
- **Fix aplicado**: target CMake `run_tests` ejecuta `$<TARGET_FILE:...>` para funcionar en builds multi-config de Visual Studio
- **Backups de proyecto**: `scripts/ProjectCheckpoint.ps1` guarda/restaura checkpoints ZIP en `workspace_backups/project_checkpoints/`
- **Backups de deploy VST3**: `DeployVST3.ps1` respalda el VST3 anterior en `workspace_backups/vst3_deploy/` antes de reemplazarlo
- **Rollback de VST3 desplegado**: `DeployVST3.ps1 -Action ListBackups` y `DeployVST3.ps1 -Action Restore -Backup latest -Force`
- **Deploy verificado**: `DeployVST3.ps1 -Config Release` copio MixCoach/Messenger a `C:\Program Files\Common Files\VST3\` y respaldo versiones anteriores
- **Checkpoint final creado**: `workspace_backups/project_checkpoints/20260531_202514_after_deploy_backup_system/`
- **Actualizacion deploy/rollback**: `DeployVST3.ps1` soporta `-Action Deploy`, `-Action ListBackups`, `-Action Restore -Backup latest -Force`
- **Deploy reciente**: `build.ps1` compilo Release y desplego VST3 creando backup `workspace_backups/vst3_deploy/20260531_202912/`

## Últimos Cambios Significativos
1. **IPC Dual**: SharedMemory + backup files en `%LOCALAPPDATA%/MixCoach/SlotBackup/`
2. **FIX overflow uint32**: `fftTimestamp` convertido a `int64_t` antes de multiplicar por 1000
3. **FIX backup FFT**: Backup files ya no sobreescriben telemetría FFT fresca de shared memory
4. **FIX VST2**: `JUCE_VST3_CAN_REPLACE_VST2=0` forzado para evitar bug de JUCE 8
5. **FIX directorio backup**: Migrado de `%TEMP%` a `%LOCALAPPDATA%` (persistente)
6. **FIX metadatos backup**: Backup prevalece sobre shared memory para nombre/color/bus
7. **FIX PCH**: Precompiled headers desactivados por conflictos con módulos JUCE

## Fases de Mentoría Implementadas
| Fase | Estado | Análisis |
|------|--------|----------|
| Welcome | ✅ Completo | Mensaje de bienvenida, detección de pistas |
| GainStaging | ✅ Completo | Clipping, headroom, señal baja, crest factor |
| Organisation | ✅ Completo | Conteo, nombres, buses, colores |
| TonalBalance | ✅ Completo | Espectro por bandas, 6 detecciones de desbalance |
| Dynamics | ✅ Completo | Crest factor, LUFS, loudness range |
| Spatial | ✅ Completo | Correlación, fase negativa, compatibilidad mono |
| SpectralMasking | ✅ Completo | Pairwise overlap por bandas críticas |

## Problemas Conocidos
1. **PCH desactivado**: JUCE módulos no pueden estar en precompiled headers (conflicto con JUCE_IMPLEMENT_MODULE)
2. **Reconexión IPC**: Si Messenger carga después de MixCoach, la shared memory puede no estar disponible al inicio. `retryInitSharedMemory()` reintenta periódicamente
3. **macOS/Linux**: No soportado actualmente (solo Windows VST3)
4. **Backup files**: Si el proceso termina abruptamente, puede dejar archivos temporales `_tmp_*.bin`
5. **FL Studio sandbox**: Los plugins VST3 se ejecutan en procesos separados, la shared memory entre DLLs puede fallar intermitentemente

## ⚠️ Estructura Canónica
- **Build directory**: `build/` (minúscula) — ✅ ÚNICO oficial
- **NO usar**: `Builds/` (mayúscula) — ❌ Eliminado (era duplicado accidental)
- **Deploy path**: `C:\Program Files\Common Files\VST3\`
- **Entry point**: `build.ps1` (NO `do_build.bat` — usa Ninja, produce VST3s vacíos)

## Próximos Pasos Recomendados
1. Probar reconexión IPC en FL Studio con carga en diferentes órdenes
2. Alinear la UI implementada con las 3 referencias en `UI_REFERENCES/`
3. Completar fase de producción (mastering) como extensión
4. Implementar exportación de informes de mezcla
5. Soporte para arrastrar/soltar archivos de referencia

## Estructura de Directorios (Post-Refactor)
```
Source/
├── Common/
│   ├── types/         → Tipos, constantes, logging
│   ├── memory/        → IPC, registros, persistencia
│   └── audio/         → Análisis de audio helpers
├── MixCoach/
│   ├── core/          → PluginProcessor, PluginEditor
│   ├── engine/        → CoachEngine, PhaseManager, Analizadores
│   ├── audio/         → AudioAnalyzer (master)
│   └── ui/            → Componentes de interfaz
└── Messenger/
    ├── core/          → PluginProcessor
    ├── ui/            → PluginEditor (compacto)
    └── telemetry/     → TelemetryCollector, LoudnessMeter, FFT
```

# Current State — MixCoach

## Estado Actual del Proyecto
- **Versión**: 1.0.0
- **Framework**: JUCE 8.0.13
- **C++ Standard**: 20
- **Plataforma**: Windows (VST3)
- **Build**: CMake + MSVC

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
2. Añadir unit tests para PhaseManager (puramente lógico, sin JUCE)
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

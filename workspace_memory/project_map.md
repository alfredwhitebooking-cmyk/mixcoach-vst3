# MixCoach — Mapa del Proyecto

## ¿Qué es?
**MixCoach** es un sistema de mentoría para mezcla de audio en tiempo real. Se implementa como plugins **VST3** para FL Studio (y DAWs compatibles) usando **JUCE 8** (C++20).

## Arquitectura General

```
┌──────────────────────────────────────────────────────────────┐
│                         DAW HOST                             │
│                                                              │
│  ┌──────────────────────────┐   ┌──────────────────────────┐ │
│  │  MESSENGER (xN pistas)   │   │     MIXCOACH (Master)    │ │
│  │  ┌────────────────────┐  │IPC│  ┌────────────────────┐  │ │
│  │  │ PluginProcessor    │──┼──┼──│ PluginProcessor    │  │ │
│  │  │ TelemetryCollector │  │   │  │ CoachEngine        │  │ │
│  │  │ LoudnessMeter      │  │   │  │ PhaseManager       │  │ │
│  │  │ FFT (espectro)     │  │   │  │ AudioAnalyzer      │  │ │
│  │  └────────────────────┘  │   │  └────────┬───────────┘  │ │
│  │         │                │   │           │              │ │
│  │  ┌──────▼─────────────┐  │   │  ┌────────▼───────────┐  │ │
│  │  │ Editor UI           │  │   │  │ MainTabbedComponent│  │ │
│  │  │ (name,color,bus,VU) │  │   │  │ ├─ CoachChat       │  │ │
│  │  └─────────────────────┘  │   │  │ ├─ Analyzers       │  │ │
│  └──────────────────────────┘   │  │  │ └─ TrackDashbrd  │  │ │
│                                 │  │  └──────────────────┘  │ │
│                                 │  └──────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

## Flujo de Datos

1. **Messenger** (por pista) analiza audio en tiempo real (peak, RMS, FFT, LUFS, correlación)
2. **IPC**: Datos viajan a MixCoach por **SharedMemory** (Windows) + **backup files** (fallback)
3. **MixCoach** recibe datos, CoachEngine analiza según la fase de mentoría activa
4. **CoachEngine** genera mensajes de mentor → aparecen en el chat UI
5. **PhaseManager** controla la progresión: Welcome → GainStaging → Organisation → TonalBalance → Dynamics → Spatial

## Comunicación
- `Source/Common/memory/` → SlotRegistry, SharedMemoryManager, SharedData
- Windows `CreateFileMappingW` con spinlock para IPC entre procesos VST3
- Backup files en `%LOCALAPPDATA%/MixCoach/SlotBackup/` como fallback

## Plugins
| Plugin | Rol | Archivos clave |
|--------|-----|----------------|
| **MixCoach** | Cerebro (canal master) | PluginProcessor, CoachEngine, PhaseManager, UI |
| **Messenger** | Oídos (por pista) | PluginProcessor, TelemetryCollector, Editor UI |
| **Common** | Compartido | Tipos, constantes, IPC, logging |

## Fases de Mentoría
1. **Welcome** → Configuración inicial
2. **GainStaging** → Ajuste de niveles, headroom, clipping
3. **Organisation** → Nombres, colores, buses
4. **TonalBalance** → Espectro, enmascaramiento
5. **Dynamics** → Crest factor, compresión
6. **Spatial** → Fase, panoramas, profundidad

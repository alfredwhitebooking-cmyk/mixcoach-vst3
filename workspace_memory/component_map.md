# Component Map — MixCoach

## MixCoach (Plugin Master)

### Core (Plugin Lifecycle)
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| MixCoachAudioProcessor | `MixCoach/core/PluginProcessor.h` | Lifecycle VST3, parámetros, timer, inicialización SharedData |
| MixCoachAudioProcessorEditor | `MixCoach/core/PluginEditor.h` | Ventana del editor, crea MainTabbedComponent |

### Engine (Lógica de Mentoría)
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| CoachEngine | `MixCoach/engine/CoachEngine.h` | Orquestador: recibe telemetría, decide qué análisis ejecutar, genera mensajes |
| PhaseManager | `MixCoach/engine/PhaseManager.h` | Máquina de estados de fases, logros, progreso |
| GainStagingAnalyzer | `MixCoach/engine/GainStagingAnalyzer.h` | Análisis de niveles: clipping, headroom, señal baja |
| TonalBalanceAnalyzer | `MixCoach/engine/TonalBalanceAnalyzer.h` | Análisis espectral: bandas de frecuencia, balance tonal |
| DynamicsAnalyzer | `MixCoach/engine/DynamicsAnalyzer.h` | Análisis de dinámica: crest factor, LUFS, compresión |
| PhaseAnalyzer | `MixCoach/engine/PhaseAnalyzer.h` | Análisis de fase: correlación, compatibilidad mono |
| SpectralMaskingAnalyzer | `MixCoach/engine/SpectralMaskingAnalyzer.h` | Detección de enmascaramiento entre pares de pistas |
| MixAnalyzer | `MixCoach/engine/MixAnalyzer.h` | Análisis global: organización, resumen de mezcla |
| AchievementManager | `MixCoach/engine/AchievementManager.h` | Gestión de logros desbloqueables |

### Audio (Procesamiento)
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| AudioAnalyzer | `MixCoach/audio/AudioAnalyzer.h` | Análisis de audio del master (delega en ChannelAnalyzer) |
| ChannelAnalyzer | `MixCoach/audio/ChannelAnalyzer.h` | Peak, RMS, FFT por canal individual |

### UI (Interfaz)
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| MixCoachTheme | `MixCoach/ui/MixCoachTheme.h` | LookAndFeel, colores, fuentes, estilos |
| MainTabbedComponent | `MixCoach/ui/MainTabbedComponent.h` | TabBar con 4 pestañas (Chat, Analyzers, Tracks, References) |
| CoachChatComponent | `MixCoach/ui/CoachChatComponent.h` | Chat de mentoría: mensajes, input, comandos |
| AnalyzersPanelComponent | `MixCoach/ui/AnalyzersPanelComponent.h` | Contenedor de analizadores con selector de pista |
| ProfessionalAnalyzersComponent | `MixCoach/ui/ProfessionalAnalyzersComponent.h` | Espectro, fase, VU, field estéreo |
| VirtualBusesComponent | `MixCoach/ui/VirtualBusesComponent.h` | Gestión de pistas y buses |
| ReferencePanelComponent | `MixCoach/ui/ReferencePanelComponent.h` | Pistas de referencia A/B |
| TrackDashboardComponent | `MixCoach/ui/TrackDashboardComponent.h` | Dashboard lateral de métricas de pista |

## Messenger (Plugin por Pista)

### Core
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| MessengerAudioProcessor | `Messenger/core/PluginProcessor.h` | Lifecycle VST3, audio buffer processing |
| MessengerAudioProcessorEditor | `Messenger/ui/PluginEditor.h` | UI compacta de pista |

### Telemetry
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| TelemetryCollector | `Messenger/telemetry/TelemetryCollector.h` | DSP en tiempo real: peak, RMS, correlation, spectrum |
| LoudnessMeter | `Messenger/telemetry/LoudnessMeter.h` | Medición LUFS (EBU R128): momentary, short-term, integrated, loudness range |
| FFTProcessor | `Messenger/telemetry/FFTProcessor.h` | FFT spectrum analysis con ventana Hann |

## Common (Compartido)

### Types
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| Types | `Common/types/Types.h` | TrackTelemetry, SlotInfo, MentorMessage, BusType, MentorPhase, Achievement, TelemetryBuffer, AudioRingBuffer |
| Constants | `Common/types/Constants.h` | Constantes: kMaxTracks, kFFTSize, colores de bus, niveles de referencia |
| TelemetryData | `Common/types/TelemetryData.h` | SharedSlotEntry, estructuras de datos IPC |
| LogHelper | `Common/types/LogHelper.h` | Logging a archivo con JUCE FileLogger |

### Memory (IPC)
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| SharedMemoryManager | `Common/memory/SharedMemory.h` | IPC con CreateFileMappingW, spinlock, lectura/escritura de slots |
| SlotRegistry | `Common/memory/SlotRegistry.h` | Registro de slots, sincronización shared memory + backup |
| SlotPersistence | `Common/memory/SlotPersistence.h` | Backup a archivos en %LOCALAPPDATA%/MixCoach/SlotBackup/ |
| SharedData | `Common/memory/SharedData.h` | Singleton: puente entre SlotRegistry y SharedMemoryManager |

### Audio
| Component | Archivo | Responsabilidad |
|-----------|---------|----------------|
| AudioAnalysis | `Common/audio/AudioAnalysis.h` | ChannelAnalysis: peak, RMS, FFT processing helpers |

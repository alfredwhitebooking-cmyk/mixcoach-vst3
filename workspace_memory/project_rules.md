# Project Rules — MixCoach

## Reglas de Arquitectura

### 1. Separación por Capas
- **Core/**: Plugin lifecycle (PluginProcessor, PluginEditor) — SOLO inicialización
- **Engine/**: Lógica de mentoría — SIN dependencias de UI
- **Audio/**: Procesamiento de audio — SIN lógica de UI ni de mentoría
- **UI/**: Interfaz visual — SOLO renderizado, llama a engine para datos
- **Memory/**: IPC y persistencia — Aislado, sin dependencias de audio o UI

### 2. Creación de Componentes UI
- Cada componente en su propio archivo `.h` + `.cpp`
- Hereda de `juce::Component`
- Constructor recibe solo las dependencias necesarias (nunca el mundo entero)
- El `paint()` solo dibuja, nunca ejecuta lógica
- Usar `MixCoachTheme` para colores y estilos, nunca valores hardcodeados

### 3. Naming Conventions
- **Clases**: PascalCase (ej: `CoachEngine`, `SlotRegistry`)
- **Archivos**: PascalCase, mismo nombre que la clase (ej: `CoachEngine.h`)
- **Métodos**: camelCase (ej: `analyzeGainStaging()`, `getLatestTelemetry()`)
- **Miembros privados**: snake_case con trailing underscore (ej: `phaseManager_`)
- **Constantes**: kPrefixedCamelCase (ej: `kMaxTracks`, `kFFTSize`)
- **Enums**: PascalCase con `enum class` (ej: `MentorPhase::GainStaging`)
- **Archivos de respaldo en disco**: slot_N.bin, formato binario con magic number

### 4. Comunicación UI ↔ Lógica
- **NUNCA** incluyas lógica de audio en UI ni viceversa
- La UI lee datos de `SlotRegistry` (thread-safe) mediante callbacks `onSlotRegistered`, `onSlotChanged`, `onSlotReleased`
- El `CoachEngine` escribe mensajes en `SharedData` (thread-safe)
- La UI de `CoachChatComponent` lee mensajes de `SharedData` via timer

### 5. IPC entre Procesos
- MixCoach y Messenger son DLLs VST3 separadas
- Comunicación via `SharedMemoryManager` (Windows `CreateFileMappingW`)
- **Backup files** en `%LOCALAPPDATA%/MixCoach/SlotBackup/` como fallback
- `SlotRegistry` sincroniza dual: shared memory + backup files
- Los backup files siempre prevalecen para metadatos (nombre, color, bus)

### 6. Thread Safety
- Audio thread: NUNCA hacer heap allocation
- UI thread: Lectura de datos thread-safe via atomic operations
- IPC writes: Spinlock en shared memory (brief, < 1μs)
- Las colas de telemetría (`TelemetryBuffer`) son lock-free ring buffers

### 7. Dependencias entre Directorios
```
Common/types/ → (sin dependencias internas)
Common/memory/ → Common/types/
Common/audio/ → Common/types/
MixCoach/audio/ → Common/types/
MixCoach/engine/ → Common/memory/, Common/types/, MixCoach/audio/
MixCoach/ui/ → MixCoach/engine/, Common/memory/, Common/types/
MixCoach/core/ → MixCoach/engine/, MixCoach/ui/, Common/memory/
Messenger/telemetry/ → Common/types/
Messenger/ui/ → Common/memory/, Messenger/telemetry/
Messenger/core/ → Messenger/ui/, Messenger/telemetry/, Common/memory/
```

### 8. Testing
- Cada `Analyzer` en `MixCoach/engine/` debe tener lógica testeable sin UI
- `PhaseManager` es puramente lógico (sin dependencias de audio)
- `SlotPersistence` debe poder probarse con archivos temporales

### 9. Prohibido
- Archivos > 500 líneas (dividir en submódulos)
- Lógica de UI en archivos de engine
- Lógica de audio en archivos de UI
- Heap allocation en audio thread
- Valores hardcodeados de color (usar MixCoachTheme)
- Includes de JUCE en headers públicos (usar forward declarations donde posible)

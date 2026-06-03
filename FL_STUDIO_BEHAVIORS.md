# 🎹 FL_STUDIO_BEHAVIORS.md — Peculiaridades de FL Studio

> **Comportamientos específicos de FL Studio que afectan al desarrollo de plugins VST3.**
> **Versión:** 1.0 | **Última actualización:** 2026-06-03

---

## ⚠️ Por Qué Este Documento Existe

FL Studio NO se comporta como otros DAWs. Muchos bugs y crashes de MixCoach fueron causados específicamente por peculiaridades de FL Studio que ningún otro DAW tiene. **Cualquier agente IA debe leer esto ANTES de modificar el PluginProcessor de Messenger o MixCoach.**

---

## 1. Escaneo VST3 en Sandbox

### El problema
Cuando FL Studio escanea plugins VST3 (al iniciar, al hacer "Rescan", o al validar plugins), **carga el plugin en un sandbox** donde:

- No hay message loop funcionando
- `createEditor()` puede llamarse sin `prepareToPlay()`
- `CreateFileMappingW` (shared memory) puede lanzar structured exceptions (SEH) que try/catch C++ normal NO captura
- Cualquier crash durante escaneo → FL Studio **deshabilita el plugin permanentemente**

### La solución (implementada)

```cpp
// ✅ CORRECTO: Constructor vacío, sin inicialización de nada
MessengerAudioProcessor::MessengerAudioProcessor()
    : AudioProcessor(...)
{
    // NADA — ni SharedData, ni LogHelper, ni archivos, ni FFT
    // Todo se inicializa LAZY en prepareToPlay() o setStateInformation()
}

// ✅ CORRECTO: createEditor() sin inicializar IPC
juce::AudioProcessorEditor* MessengerAudioProcessor::createEditor()
{
    // NO inicializar SharedData/IPC aquí
    return new MessengerAudioProcessorEditor(*this);
}
```

### La lección
```
NUNCA hacer en el constructor del plugin:
  ❌ SharedData::getInstance()
  ❌ CreateFileMapping / OpenFileMapping
  ❌ File I/O de cualquier tipo
  ❌ juce::dsp::FFT (aloja memoria interna)
  ❌ LogHelper::setLogFile()

TODO debe ser LAZY: en prepareToPlay() o setStateInformation()
```

---

## 2. processBlock ANTES de prepareToPlay

### El problema
FL Studio puede llamar a `processBlock()` **antes** de `prepareToPlay()`. Esto es ilegal según la especificación VST3, pero FL Studio lo hace.

### La solución

```cpp
void MessengerAudioProcessor::processBlock(...)
{
    // Protección: algunos DAWs llaman processBlock ANTES de prepareToPlay
    if (!prepared_) {
        return;  // ← SALIR TEMPRANO, no procesar nada
    }
    // ... resto del procesamiento
}
```

### La lección
```
SIEMPRE verificar prepared_ al inicio de processBlock()
NUNCA asumir que prepareToPlay() ya fue llamado
NUNCA inicializar recursos en processBlock() (solo verificar y salir)
```

---

## 3. Copia Masiva de Plugins (Clone / Paste)

### El problema
FL Studio permite copiar un plugin (Ctrl+C) y pegarlo en múltiples pistas (Ctrl+Shift+V en selección). Esto crea **N instancias del Messenger en paralelo en milisegundos**.

Cada nueva instancia llama a `setStateInformation()` (para restaurar estado) y luego `processBlock()` empieza a fluir cuando el usuario da play.

### El riesgo
- 60+ instancias llamando a `registerSlot()` simultáneamente
- Cada `registerSlot()` ANTES escribía un backup file síncrono → 60+ archivos simultáneos → I/O saturado → timeout → crash
- 60+ `SharedData::getInstance()` llamadas en paralelo

### La solución (implementada)

```cpp
// ✅ Backup file DIFERIDO (~500ms después del registro, desde el message thread)
// NO desde el audio thread ni desde registerSlot()
void MessengerAudioProcessor::ensureSlotRegistered()
{
    // ... registro del slot ...
    pendingBackupWrite_ = true; // Backup se escribe desde el timer (~500ms)
}

// En timerCallback() — message thread, seguro para file I/O
if (pendingBackupWrite_ && slotIndex_ >= 0 && sharedData_) {
    pendingBackupWrite_ = false;
    SlotRegistry::saveSlotToBackupFile(slotIndex_, slotInfo);
}
```

### La lección
```
NUNCA hacer file I/O síncrono durante el registro de slots
     (60+ instancias en paralelo saturan el disco)
TODO file I/O debe ser diferido al message thread
     (timer de 500ms es suficiente)
```

---

## 4. setStateInformation() sin Estado Previo

### El problema
FL Studio llama a `setStateInformation()` con `sizeInBytes = 0` cuando crea una instancia NUEVA de plugin (no restaurada de un proyecto guardado).

Originalmente, el código retornaba temprano si `sizeInBytes < 4`, lo que impedía que las instancias nuevas registraran su slot hasta que el audio empezara a fluir.

### La solución

```cpp
// ✅ SIEMPRE registrar slot, incluso sin datos de estado
void MessengerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    ensureSlotRegistered();  // ← AHORA está ANTES del early return

    juce::MemoryInputStream mis(data, sizeInBytes, false);
    if (sizeInBytes < 4) {
        // Sincronizar metadatos por defecto
        registry.updateSlotName(currentSlot, trackName_.toStdString());
        registry.updateSlotColour(currentSlot, trackColour_);
        return;
    }
    // ... leer estado persistente ...
}
```

### La lección
```
ensureSlotRegistered() debe estar SIEMPRE al inicio de setStateInformation()
     ANTES de cualquier verificación de tamaño de datos
Las instancias nuevas deben registrarse aunque no tengan estado previo
```

---

## 5. Timer sin Message Loop Durante Escaneo

### El problema
Durante el escaneo VST3 de FL Studio, no hay message loop del DAW funcionando. Esto significa que `timerCallback()` NUNCA se dispara durante el escaneo.

**Esto es BUENO:** permite usar un timer de 500ms como fire-once para registrar slots en DAWs que no llaman `setStateInformation()` ni `prepareToPlay()`, con la seguridad de que durante el escaneo no se ejecutará.

### La implementación

```cpp
// En el constructor:
startTimer(500);  // Fire once, pero nunca se dispara durante escaneo

// Timer callback:
void MessengerAudioProcessor::timerCallback()
{
    // Bypass seguro para shutdown
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
        return;

    // Registrar slot si no se registró antes
    if (slotIndex_ < 0)
        ensureSlotRegistered();

    // Escribir backup diferido
    if (pendingBackupWrite_ && slotIndex_ >= 0 && sharedData_)
        SlotRegistry::saveSlotToBackupFile(slotIndex_, slotInfo);

    stopTimer();  // Fire once
}
```

### La lección
```
El timer de 500ms es SEGURO porque FL Studio no tiene message loop durante escaneo
El timer es fire-once: se detiene después del primer disparo
El timer es un "fallback" para cuando setStateInformation() no se llama
```

---

## 6. Validación de Plugins con `Verify installed plugins`

### El problema
FL Studio tiene una función "Verify installed plugins" (ícono ⚡ rayo) que:
1. Carga el plugin
2. Llama a `createEditor()` 
3. Verifica que el plugin no crashee
4. Descarga el plugin

Si el plugin crashea durante este proceso, FL Studio lo **deshabilita permanentemente** (aparece tachado en la lista).

### La lección
```
NUNCA hacer nada en createEditor() que pueda crashear
     (no SharedData, no IPC, no file I/O)
createEditor() debe ser un return de new Editor(...) y nada más
El editor debe ser lazy en su constructor también
```

---

## 7. Procesos Separados (FL Studio Sandbox)

### El problema
FL Studio ejecuta plugins VST3 en procesos separados (sandbox) por defecto. Esto significa que:

- Messenger (Pista 1) y Messenger (Pista 2) pueden estar en **diferentes procesos**
- MixCoach y Messenger pueden estar en **diferentes procesos**
- `CreateFileMappingW` es necesario para comunicación inter-process
- Las variables globales (singletons) NO se comparten entre procesos
- Los backup files en disco son el mecanismo de comunicación **garantizado**

### La lección
```
Los singletons (SharedData::getInstance()) son solo intra-proceso
La comunicación cross-DLL requiere CreateFileMappingW (shared memory)
Los backup files son esenciales como fallback cross-process
NUNCA asumir que dos instancias de plugin comparten memoria
```

---

## 8. Ruta de Deploy Correcta

### El problema
FL Studio SOLO lee plugins VST3 de:
```
C:\Program Files\Common Files\VST3\MixCoach.vst3
C:\Program Files\Common Files\VST3\Messenger.vst3
```

Los scripts de deploy a veces resuelven mal las rutas relativas, copiando a:
```
C:\Proyectos\Program Files\Common Files\VST3\  ← ❌ INCORRECTO
```

### La solución
```powershell
# ✅ RUTA CORRECTA (absoluta, sin variables relativas)
$vst3Dir = "C:\Program Files\Common Files\VST3\"

# ❌ NO USAR rutas relativas en scripts de deploy
$wrongDir = "$PSScriptRoot\..\Program Files\Common Files\VST3\"
```

### La lección
```
SIEMPRE verificar la ruta de deploy después de copiar
SIEMPRE usar rutas absolutas en scripts de deploy
El validate.ps1 verifica que los VST3s existen en la ruta correcta
```

---

## 9. Uso de MSBuild (No Ninja)

### El problema
FL Studio en Windows requiere VST3s compilados con MSVC. Usar Ninja como generator de CMake produce:

- En Debug: funciona (pero no es óptimo)
- En Release: **C1001 Internal Compiler Error** en `juce_graphics_Harfbuzz.cpp`

### La solución
```powershell
# ✅ USAR: Visual Studio 17 2022 (MSBuild)
cmake -B build -G "Visual Studio 17 2022"

# ❌ NO USAR: Ninja
cmake -B build -G Ninja  # → C1001 en Release
```

### La lección
```
SIEMPRE usar Visual Studio 17 2022 como generator
NUNCA usar Ninja para builds de Release
build.ps1 ya usa el generator correcto
```

---

## 10. VST3 Bundle vs DLL

### El problema
FL Studio espera que un VST3 sea un **directorio** (bundle) con una estructura específica:
```
MixCoach.vst3/
  └── Contents/
      ├── Resources/moduleinfo.json
      └── x86_64-win/MixCoach.vst3   ← DLL real
```

Si el bundle está vacío (solo directorios, sin DLL), FL Studio muestra el plugin pero **no funciona** (no aparece en la lista o aparece tachado).

### Causa raíz
Esto ocurrió cuando el build con Ninja falló silenciosamente: `cmake --build build` reportó éxito pero no compiló nada, y el deploy copió directorios vacíos.

### La lección
```
Siempre verificar que el VST3 bundle TIENE la DLL adentro
dir "C:\Program Files\Common Files\VST3\MixCoach.vst3\Contents\x86_64-win\"
    → Debe mostrar: MixCoach.vst3 (~4.2 MB)
validate.ps1 ya verifica esto automáticamente
```

---

## 📋 Resumen: Lo que NO Debe Asumir un Agente IA

```
❌ "El DAW siempre llama a prepareToPlay antes de processBlock"
   → FL Studio NO lo hace. Verificar prepared_.

❌ "El constructor del plugin es seguro para hacer de todo"
   → NO. FL Studio escanea en sandbox. Constructor debe estar VACÍO.

❌ "createEditor() solo se llama cuando el usuario abre la UI"
   → NO. FL Studio lo llama durante validación de plugins.

❌ "Las instancias de plugin comparten memoria"
   → NO. FL Studio usa sandbox. Usar CreateFileMappingW para IPC.

❌ "El registro de slots puede escribir archivos"
   → NO con 60+ instancias. Backup diferido al message thread.

❌ "El VST3 es un solo archivo"
   → NO. Es un bundle (directorio) con estructura específica.
```

---

## 🔗 Referencias

| Documento | Relación |
|-----------|----------|
| `KNOWN_ERRORS.md` | Bugs causados por estos comportamientos |
| `DECISION_LOG.md` | ADR-005: Lazy init (causado por #1) |
| `DECISION_LOG.md` | ADR-002: Backup diferido (causado por #3) |
| `RISK_MATRIX.md` | 🔴 PluginProcessors: afectados por estos comportamientos |
| `APPROVED_PATTERNS.md` | P1: Lazy Initialization (solución a #1 y #2) |

---

*Documento de comportamientos de FL Studio — MixCoach Project*

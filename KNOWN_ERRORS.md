# Known Errors — MixCoach Project

> Error memory system. Add entries as errors are encountered and fixed.
> IA: **Before debugging a new error, check this file for known patterns.**

---

## 🔴 Linker Errors

### LNK2019 — Unresolved External Symbol (common)

| When | Fix | Verified |
|------|-----|----------|
| Missing .cpp in CMake | Add source file to `juce_add_plugin()` or `add_library()` | ❌ |
| Missing JUCE module | Add `juce_dsp`, `juce_audio_utils`, etc. to `JUCE_MODULES` | ❌ |
| Virtual method missing impl | Implement all pure virtuals from JUCE base classes | ❌ |
| Static member undefined | Add `Type Class::member;` in .cpp file | ❌ |

### LNK2038 — _ITERATOR_DEBUG_LEVEL mismatch

**Symptom:** Linking Debug .obj into Release VST3 (or vice versa)  
**Fix:** Clean rebuild entire solution: `cmake --build Builds --config Release --clean-first`  
**Root cause:** Mixing configurations between `MixCoach_SharedCode` and the final VST3 artifact.

---

## 🟡 Compiler Errors

### C2664 — Cannot convert argument

| When | Fix | Verified |
|------|-----|----------|
| juce::String → const char* | Use `.toRawUTF8()` | ❌ |
| Missing const& | Add `const` to parameter | ❌ |
| int → size_t mismatch | Cast with `static_cast<size_t>()` | ❌ |

### C2259 — Cannot instantiate abstract class

**Common missing overrides:**
- `AudioProcessor::getName()`
- `AudioProcessor::acceptsMidi()` / `producesMidi()`
- `AudioProcessor::getTailLengthSeconds()`
- `AudioProcessorEditor::resized()` / `paint()`

**Fix:** Add all required `override` methods from JUCE base classes.

### C2280 — Deleted function (copy)

**When:** Using `std::atomic` member in copyable class, or passing `std::unique_ptr` by value.  
**Fix:** Delete copy constructor or use pointer/reference.

---

## 🟠 CMake Errors

### JUCE module not found

**Fix:** Add to `CMakeLists.txt`:
```cmake
JUCE_MODULES = ... juce_dsp juce_audio_utils juce_graphics ...
```

### CMake configuration failed

**Fix:**
```bash
cmake -B Builds -S . -G "Visual Studio 17 2022"
```
Check `JUCE_ROOT` path if project was moved.

---

## 🔵 Build System Errors

### MSB3073 — Post-build command failed

**Symptom:** DeployVST3.ps1 fails in post-build step  
**Fix:** Run manually: `powershell -ExecutionPolicy Bypass -File DeployVST3.ps1`  

### MSB4018 — Internal MSBuild crash

**Fix:** Rerun build. If persistent:
```bash
cmake --build Builds --config Release --clean-first
```

---

## ⚪ Runtime Errors

### Audio thread crash / NaN

**Symptom:** Cracking, popping, or DAW crash during playback.  
**Causes:**
- Division by zero in RMS/LUFS calculation (silent input)
- `sqrt(negative_value)` in statistical calculations
- Phase `atan2(0, 0)` → NaN
- FFT normalization with zero energy

**Fix:**
```cpp
#include <cmath>
float safe_sqrt(float v) { return v > 0.f ? std::sqrt(v) : 0.f; }
float safe_atan2(float y, float x) {
    return (x == 0.f && y == 0.f) ? 0.f : std::atan2(y, x);
}
```

### IPC: Shared memory open failure

**Symptom:** Messenger doesn't appear in MixCoach, slots are empty.  
**Causes:**
- All 32 slots full (remove some instances)
- %TEMP% directory not writable (use %LOCALAPPDATA% instead)
- Crashed VST3 left stale mappings (reboot DAW)

**Quick fix:** Close DAW, delete `%LOCALAPPDATA%\MixCoach\SlotBackup\`, reopen.

### UI Freeze / Timer overload

**Symptom:** UI stutters when many tracks are loaded.  
**Fix:** Move heavy sync operations to background thread. Current design uses background worker in `PluginEditor.cpp`.

---

---

## 🟣 Runtime UI Errors

### TrackSelectorStrip: Playlist vacío / strips invisibles (FIXED 30-May-2026)

**Síntoma:** Los Messengers aparecen en la lista de Tab 1 (Mentoría) pero NO en el playlist de Tab 2 (METERING). El placeholder "No hay Messengers conectados..." siempre visible aunque haya pistas activas.

**Causa raíz:** En el constructor de `TrackSelectorStrip`, todos los strips se crean con `addAndMakeVisible(s)` seguido de `s->setVisible(false)`, y `placeholderLabel_` se crea con `addAndMakeVisible(placeholderLabel_)`. El método `updateTracks()` poblaba datos en los strips pero NUNCA cambiaba la visibilidad.

**Fix:** Agregar al final de `TrackSelectorStrip::updateTracks()`:
```cpp
bool hasTracks = (activeTrackCount_ > 0);
placeholderLabel_.setVisible(!hasTracks);
for (int i = 0; i < kMaxVisible; ++i)
    strips_[i]->setVisible(i < activeTrackCount_);
```

**Archivo:** `Source/MixCoach/UI/AnalyzersPanelComponent.cpp` (~141 KB)
**Clase:** `TrackSelectorStrip`
**Método:** `updateTracks()`
**Verified:** ✅ Construido y desplegado 30-May-2026 14:01

---

## 🔴 Build System Errors

### VST3 bundle vacío (solo directorios, sin DLL) (FIXED 30-May-2026)

**Síntoma:** Después de compilar con `do_build.bat`, ambos VST3s (MixCoach, Messenger) son carpetas vacías con estructura `Contents/x86_64-win/` pero sin el archivo `.vst3` DLL.

**Causa raíz:** El script `scripts\do_build.bat` configura CMake con el generator **Ninja** (`-G Ninja`). En el entorno actual, la configuración de Ninja falló silenciosamente — no generó `build.ninja`, pero `cmake` retornó éxito parcial. `cmake --build build` no compiló nada. El deploy copió los directorios vacíos.

**Fix:**
```powershell
# Forzar limpieza y reconstruir con Visual Studio 17 2022:
.\build.ps1 -Clean

# Deploy manual:
powershell -ExecutionPolicy Bypass -File DeployVST3.ps1
# O manualmente:
# Remove-Item "C:\Program Files\Common Files\VST3\MixCoach.vst3" -Recurse -Force
# Copy-Item "build\MixCoach_artefacts\Release\VST3\MixCoach.vst3" "C:\Program Files\Common Files\VST3\" -Recurse
```

**Regla:** Usar SIEMPRE `build.ps1`, NUNCA `do_build.bat`. El generator correcto es **Visual Studio 17 2022**.

**Diagnóstico rápido:**
```cmd
:: Verificar si el bundle tiene DLL:
dir "C:\Program Files\Common Files\VST3\MixCoach.vst3\Contents\x86_64-win"
:: Debe mostrar: MixCoach.vst3 (~4.2 MB)
```

**Verified:** ✅ Builds posteriores con VS 2022 producen VST3s correctos

---

### Ninja + Release = C1001 Internal Compiler Error (KNOWN)

**Síntoma:** `fatal error C1001: Internal compiler error` al compilar `juce_graphics_Harfbuzz.cpp` en Release con Ninja.

**Causa:** Bug en MSVC con optimización LTCG + Ninja en ciertos archivos de JUCE.

**Fix:** Usar Visual Studio 17 2022 (MSBuild) en vez de Ninja.

---

## 📝 Historial de Fixes Recientes

| Fecha | Error | Archivo | Fix |
|-------|-------|---------|-----|
| 30-May-2026 | Playlist vacío en Tab 2 | `AnalyzersPanelComponent.cpp` | Agregar gestión de visibilidad en `updateTracks()` |
| 30-May-2026 | VST3s vacíos (Ninja) | `build system` | Usar `build.ps1 -Clean` en vez de `do_build.bat` |
## 📊 Error Statistics

- **Total errors encountered:** 2
- **Total fixes applied:** 2
- **Common error categories:**
  - UI visibility bugs (1)
  - Build system misconfiguration (1)

---

*Last updated: 2026-05-30*

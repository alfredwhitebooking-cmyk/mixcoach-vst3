# ⚡ MixCoach 🎚️🤖 — AI Mixing Mentor (VST3)

**MixCoach no es un procesador de audio. Es un Mentor Profesional que te guía a través del proceso de mezcla, convirtiéndote en un ingeniero de audio más capaz, organizado y seguro de tus decisiones.**

Arquitectura **Sensor-Cerebro V3**: `Messenger` (uno por pista, solo pasa audio RAW + identidad) → `MixCoach` (en el Master, analiza TODO y mentorea). Ningún plugin toca tu audio.

## Build

```powershell
.\build.ps1                    # Build + deploy automático (Release)
.\build.ps1 -NoDeploy          # Solo compilar
cmake --build build --config Release --target MixCoach_VST3
```

Los VST3 se despliegan a `C:\Program Files\Common Files\VST3\`. Requiere: **Windows 10/11**, **VS 2022**, **JUCE 8**, **CMake 3.22+**.

## Stack

C++20 · JUCE 8 · Visual Studio 17 2022 · CMake · Windows VST3 · FL Studio (DAW primario)

---

**Para agentes IA**: Toda la documentación del proyecto está en [`AI_CONTEXT.md`](AI_CONTEXT.md). Léelo — ahí está TODO lo que necesitas saber.

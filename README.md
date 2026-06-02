# MixCoach 🎚️🤖

**Sistema Inteligente de Mentoría para Mezcla**

MixCoach no es un procesador de audio — es un **Mentor Profesional** que te guía a través del proceso de mezcla, convirtiéndote en un ingeniero de audio más capaz, organizado y seguro de tus decisiones.

## Filosofía

- 🧠 **Mentor, no Juez** — La IA guía, enseña y motiva. No altera tu audio.
- 🎯 **Enfoque 80/20** — Excelencia profesional sin parálisis por perfeccionismo.
- 🏆 **Gamificación** — Logros y validación positiva para retención.
- 🤝 **Relación Equipo** — "Ingeniero + MixCoach". Tú decides, la IA provee criterio.

## Arquitectura

### Messenger (Los Oídos)
Plugin VST3 distribuido en pistas individuales. Consumo de CPU ultrabajo (<0.05%).
Captura: Picos, RMS, FFT y Fase.

### MixCoach (El Cerebro)
Plugin VST3 en el canal Master. Hub central que recibe la telemetría, aloja la IA,
el chat de mentoría y los analizadores visuales.

## RoadMap de Mentoría

| Fase | Enfoque | Descripción |
|------|---------|-------------|
| 0 | Setup | Saludo profesional y configuración de género |
| 1 | Gain Staging | Limpieza de clipping y headroom |
| 2 | Organización | Colores y agrupación en Buses Virtuales |
| 3 | Balance Tonal | Análisis comparativo con referencias |
| 4 | Dinámica | Estabilización de picos |
| 5 | Espacialidad | Profundidad y toques finales |

## Requisitos

- **Windows 10/11** (macOS próximamente)
- **FL Studio** (otros DAWs compatibles con VST3)
- **Visual Studio 2022** con carga de trabajo "Desarrollo de escritorio con C++"
- **JUCE 8** ([descargar](https://juce.com/get-juce/))
- **CMake 3.22+**

## Compilación

```bash
cd MixCoach
cmake -B Builds -DJUCE_ROOT="C:/JUCE"
cmake --build Builds --config Release
```

Los plugins compilados estarán en `Builds/MixCoach_artefacts/Release/` y
`Builds/Messenger_artefacts/Release/`.

## Stack Tecnológico

- **Framework:** JUCE 8 (C++20)
- **IA:** LLM vía API (OpenAI/Claude) o local (Ollama)
- **Comunicación:** SharedResourcePointer + memoria compartida
- **Analizadores:** FFT, Correlación de Fase, RMS, Picos

## Para agentes IA

Si trabajas con Codex, Antigravity, Freebuff u otro agente, empieza por
[`AGENTS.md`](AGENTS.md). Ese archivo resume la vision del producto, las
referencias UI canonicas, los comandos de build/test y los archivos de alto
riesgo antes de tocar codigo.

## Seguridad y despliegue

- `.\build.ps1` compila y despliega los VST3 a `C:\Program Files\Common Files\VST3\`.
- `.\DeployVST3.ps1 -Config Release` despliega manualmente y guarda copia del VST3 anterior en `workspace_backups/vst3_deploy/`.
- `.\DeployVST3.ps1 -Action ListBackups` muestra versiones VST3 desplegadas anteriormente.
- `.\DeployVST3.ps1 -Action Restore -Backup latest -Force` restaura la ultima version VST3 respaldada.
- `.\scripts\ProjectCheckpoint.ps1 -Action Save -Name "antes_del_cambio"` crea una copia ZIP restaurable del proyecto, incluyendo archivos no trackeados importantes.
- `.\scripts\ProjectCheckpoint.ps1 -Action List` muestra checkpoints disponibles.
- `.\scripts\ProjectCheckpoint.ps1 -Action Restore -Checkpoint "<ruta>" -Force` restaura un checkpoint sobre el workspace actual.

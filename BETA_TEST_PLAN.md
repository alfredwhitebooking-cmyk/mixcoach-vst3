# 🧪 BETA TEST PLAN — MixCoach + Messenger

> **Versión:** 1.0
> **Fecha:** 24 junio 2026
> **Build target:** Release (`cmake --build build --config Release`)
> **Tests unitarios:** ~5190 tests, 0 fallos

---

## 📋 Índice

1. [🎯 Propósito](#-propósito)
2. [🔧 Prerrequisitos](#-prerrequisitos)
3. [📦 Entorno de Prueba](#-entorno-de-prueba)
4. [✅ Smoke Test Checklist](#-smoke-test-checklist)
   - [4.1 Instalación y Carga](#41-instalación-y-carga)
   - [4.2 Messenger — Por Pista](#42-messenger--por-pista)
   - [4.3 MixCoach — Tab 1 AI Coach](#43-mixcoach--tab-1-ai-coach)
   - [4.4 MixCoach — Tab 2 Analyzers](#44-mixcoach--tab-2-analizadores)
   - [4.5 Sistema de Referencias](#45-sistema-de-referencias)
   - [4.6 Comandos del Chat](#46-comandos-del-chat)
   - [4.7 Rendimiento y Estabilidad](#47-rendimiento-y-estabilidad)
   - [4.8 Recuperación de Errores](#48-recuperación-de-errores)
   - [4.9 Sesiones Multi-sesión](#49-sesiones-multi-sesión)
   - [4.10 Integración FL Studio](#410-integración-fl-studio)
5. [📊 Reporte de Bugs](#-reporte-de-bugs)
6. [📈 Criterios de Aceptación](#-criterios-de-aceptación)

---

## 🎯 Propósito

Este plan define las pruebas de humo (smoke tests) para la beta de **MixCoach v1.0**. El objetivo es verificar que:

- Ambos plugins (MixCoach + Messenger) cargan y funcionan en FL Studio
- La comunicación IPC entre Messenger y MixCoach es estable
- La UI se renderiza correctamente (sin crashes, sin artefactos)
- El chat con IA responde y las recomendaciones son coherentes
- Los analizadores (spectrum, vectorscope, LUFS, VU) muestran datos correctos
- El sistema de referencias funciona (archivos de audio + URLs)
- La sesión persiste correctamente entre reinicios
- No hay crashes, memory leaks, ni degradación de rendimiento

---

## 🔧 Prerrequisitos

### Sistema

| Requisito | Especificación |
|:----------|:---------------|
| **OS** | Windows 10 22H2+ / Windows 11 |
| **DAW** | FL Studio 21.2+ (build 3589+) |
| **CPU** | Intel i5-10400 / AMD Ryzen 5 3600 o superior |
| **RAM** | 16 GB mínimo (32 GB recomendado) |
| **Disco** | 500 MB libres para VST3 + archivos de referencia |
| **Resolución** | 1920×1080 mínimo (2560×1440 recomendado) |
| **GPU** | Cualquier GPU compatible con OpenGL 3.3+ |

### Build

```powershell
# Build completo (Release)
.\build.ps1

# Build manual
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target MixCoach_VST3
cmake --build build --config Release --target Messenger_VST3
```

Los VST3 se despliegan automáticamente a:
```
C:\Program Files\Common Files\VST3\MixCoach.vst3
C:\Program Files\Common Files\VST3\Messenger.vst3
```

### Tests Unitarios (ejecutar antes de pruebas manuales)

```powershell
# Todos los tests
cmake --build build --config Release --target run_tests

# Test específico del motor principal
./build/tests/Release/TestCoachEngine.exe

# Tests de estrés (128 slots)
./build/tests/Release/TestStress128Slots.exe

# Tests de IPC
./build/tests/Release/TestIPCIntegration.exe
```

**Criterio:** 100% de tests pasando antes de cualquier prueba manual.

---

## 📦 Entorno de Prueba

### Proyecto de prueba recomendado

Crear un proyecto FL Studio con la siguiente configuración mínima:

| # | Tipo | Nombre | Bus | Color | Notas |
|:-:|:----:|:-------|:---:|:-----:|:------|
| 1 | Sampler | Kick | Drums | 🔴 | 808 kick sample |
| 2 | Sampler | Snare | Drums | 🟠 | Snare acústica |
| 3 | Sampler | HiHat | Drums | 🟡 | Closed hi-hat |
| 4 | Sampler | 808_Bass | Bass | 🔵 | 808 sub-bass |
| 5 | Sampler | Piano | Keys | 🟢 | Piano acústico |
| 6 | Audio | Voz | Vocals | 🟣 | Voz grabada |
| 7 | Sampler | FX | FX | ⚪ | Risers / efectos |

Cada pista debe tener una instancia de **Messenger** insertada.

### Archivos de referencia (preparar antes)

1. Archivo WAV/MP3/FLAC de referencia (16/24 bit, 44.1kHz o 48kHz)
2. URL de YouTube/Spotify válida

---

## ✅ Smoke Test Checklist

### 4.1 Instalación y Carga

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 1.1 | Build Release | `.\build.ps1` (o cmake) | Build exitoso, sin errores | ⬜ |
| 1.2 | Deploy VST3 | Verificar `C:\Program Files\Common Files\VST3\` | MixCoach.vst3 + Messenger.vst3 existen | ⬜ |
| 1.3 | Cargar MixCoach | FL Studio → Mixer → Master → Insertar MixCoach | Plugin se abre sin crash, UI visible | ⬜ |
| 1.4 | Cargar Messenger | FL Studio → Mixer → Track 1 → Insertar Messenger | Plugin se abre sin crash, UI visible | ⬜ |
| 1.5 | Múltiples Messengers | Cargar Messenger en 8+ tracks | Todos cargan sin error | ⬜ |
| 1.6 | Carga en proyecto existente | Abrir proyecto FL existente con 16+ tracks | Sin crashes ni lentitud | ⬜ |
| 1.7 | Reinicio del DAW | Cerrar y reabrir FL Studio con plugins cargados | Plugins recuerdan estado | ⬜ |
| 1.8 | Scan sandbox (FL) | Forzar re-scan de VST3 en FL Studio | Plugins no se deshabilitan | ⬜ |

### 4.2 Messenger — Por Pista

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 2.1 | UI inicial | Abrir Messenger en pista con audio | Panel flotante con fondo glass, título "INFORMACIÓN DE PISTA" | ⬜ |
| 2.2 | Nombre de pista | El input muestra el nombre de la pista | Nombre correcto, edición posible | ⬜ |
| 2.3 | Color picker | Click en selector de color | Dropdown con 8 colores, preview circle | ⬜ |
| 2.4 | Tipo de pista | Dropdown de tipo | Menú completo con tipos (Kick, Snare, Bass, Vocal, etc.) | ⬜ |
| 2.5 | Bus assignment | Dropdown de bus | Opciones: Drums, Bass, Guitars, Keys, Vocals, FX, None | ⬜ |
| 2.6 | Iconos hover | Hover sobre iconos ✏️ 🎨 📦 ➡️ | Animación hover (fade to purple) | ⬜ |
| 2.7 | Focus glow | Click en input de nombre | Borde con glow animado (SmoothValue) | ⬜ |
| 2.8 | Divisores UI | Verificar 4 líneas divisorias | Divisores sutiles visibles entre cada fila | ⬜ |
| 2.9 | Comunicación IPC | Cambiar nombre/color en Messenger | MixCoach refleja el cambio en < 5s | ⬜ |
| 2.10 | Múltiples instancias | 8+ Messengers en diferentes tracks | Cada instancia es independiente | ⬜ |

### 4.3 MixCoach — Tab 1 AI Coach

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 3.1 | UI general | Abrir MixCoach → Tab 1 | Split: chat 38% izq + pistas 62% der | ⬜ |
| 3.2 | Chat IA burbuja | Enviar mensaje al chat | Burbuja purple (IA) + cyan (usuario) con glassmorphism | ⬜ |
| 3.3 | Mensaje de bienvenida | Abrir MixCoach sin sesión previa | Coach pregunta género ("¿Qué vamos a mezclar?") | ⬜ |
| 3.4 | Setting género | Escribir "reggaeton" | Coach confirma género, avanza a setup | ⬜ |
| 3.5 | Avatar IA | Verificar avatar junto al chat | Robot Path-drawing: cabeza metallic + ojos cyan + antena glow | ⬜ |
| 3.6 | Send button | Click en botón enviar | Icono paper plane, hover glow morado | ⬜ |
| 3.7 | Master Meter panel | Verificar VU meters en panel izquierdo | VU L/R con gradiente verde→amarillo→rojo, peak glow, clip monitor | ⬜ |
| 3.8 | Track list | Verificar pistas detectadas | Lista con nombre, color dot, level bar, freq bar, stereo badge | ⬜ |
| 3.9 | Health dots | Verificar 🟢🟡🔴 por pista | Indicador de salud (gain, dynamics, tonal) | ⬜ |
| 3.10 | Track health tooltip | Hover sobre health dot | Tooltip con últimos eventos de esa pista | ⬜ |
| 3.11 | Health filter pills | Click en ❌/⚠️/✓ | Lista se filtra por salud de pista | ⬜ |
| 3.12 | TrackFeed banner | Verificar banner colapsable | Top 3 eventos globales con dots de severidad | ⬜ |
| 3.13 | /status command | Escribir "/status" | Coach responde con resumen de sesión | ⬜ |
| 3.14 | /analyze command | Escribir "/analyze" | Coach ejecuta análisis y responde | ⬜ |
| 3.15 | /help command | Escribir "/help" | Coach lista comandos disponibles | ⬜ |
| 3.16 | AI recommendation | Preguntar "¿Qué puedo mejorar?" | Coach responde con issue prioritario + sugerencia accionable | ⬜ |
| 3.17 | Gain advice | Preguntar por niveles de pista | Muestra TrackGainAdvice con target del rol y delta sugerido | ⬜ |
| 3.18 | Dynamics advice | Preguntar por compresión | Muestra TrackDynamicsAdvice con crest y sugerencia | ⬜ |
| 3.19 | Tonal advice | Preguntar por EQ | Muestra TrackTonalAdvice con región y sugerencia EQ | ⬜ |

### 4.4 MixCoach — Tab 2 Analizadores

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 4.1 | Layout 2×2 | Abrir Tab 2 | Grid: Meter 28% × Spectrograph 72% (arriba), PhaseScope 28% × VU Meters 47% (abajo) + status bar | ⬜ |
| 4.2 | VU Meter (L/R) | Reproducir audio | Barras VU con gradiente verde→amarillo→rojo, peak marker blanco | ⬜ |
| 4.3 | Metric cards | Verificar PEAK/RMS/LUFS/DR | 4 mini cards con valores numéricos actualizados | ⬜ |
| 4.4 | LUFS dual | Verificar LUFS L/R en meter | Barras cyan con target triangles | ⬜ |
| 4.5 | Spectrograph RTA | Reproducir audio | 40 bandas log, cyan glow, barras animadas | ⬜ |
| 4.6 | Waterfall 3D | Reproducir audio continuo | 80 slices de historial, fade quadrático | ⬜ |
| 4.7 | Freq labels | Verificar etiquetas de frecuencia | Hz visibles (accentCyan 9px bold + strip oscuro) | ⬜ |
| 4.8 | dB axis labels | Verificar escala dB | dB axis visibles (accentCyan 8px bold) | ⬜ |
| 4.9 | PhaseScope | Verificar correlation meter | Barra horizontal con diamante, zone markers | ⬜ |
| 4.10 | Vectorscope | Reproducir audio estéreo | Trazo morado eléctrico, phosphor trail, grid concéntrico | ⬜ |
| 4.11 | Crest gauge | Verificar gauge semicircular | Needle, segmentos de color, scale marks | ⬜ |
| 4.12 | Metrics table (PhaseScope) | Verificar PEAK/RMS/CREST | Labels 7.5px bold | ⬜ |
| 4.13 | Vintage VU Meters | Reproducir audio | 4 meters (L/R/M/S) estilo vintage, cream bg, needle negro | ⬜ |
| 4.14 | Status bar footer | Verificar info en footer | ⚡ MIXCOACH | ANALYZERS | GENRE | TARGET LUFS | kHz | ⬜ |
| 4.15 | Dot grid background | Verificar fondo | iZotope/NUGEN style dot grid | ⬜ |

### 4.5 Sistema de Referencias

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 5.1 | Panel de referencias | Tab 1 → sección referencias | Sub-tabs "Audio" + "Enlaces" visibles | ⬜ |
| 5.2 | Cargar archivo WAV | Arrastrar WAV a drop zone | Archivo se carga, fingerprint se analiza | ⬜ |
| 5.3 | Cargar archivo MP3 | Arrastrar MP3 | MP3 se carga correctamente | ⬜ |
| 5.4 | Cargar URL | Pegar URL de YouTube | Coach infiere título y plataforma | ⬜ |
| 5.5 | Reference match | Después de cargar referencia | DifferenceProfile se genera con gaps por dominio | ⬜ |
| 5.6 | Reference gaps | Verificar gaps detectados | Gaps de tonal, dynamics, spatial con severidad (Critical/Warning/Info) | ⬜ |
| 5.7 | Reference mode toggle | Activar "REF MODE" | Coach cambia a modo referencia-driven | ⬜ |
| 5.8 | Reference progress | Verificar progress bar | Barra con match %, flecha de tendencia ▲/▼ | ⬜ |
| 5.9 | Limpiar referencia | Botón de limpiar | Referencia se elimina, coach vuelve a modo normal | ⬜ |
| 5.10 | Múltiples referencias | Cargar + limpiar varias | Sin memory leaks, cada carga es independiente | ⬜ |

### 4.6 Comandos del Chat

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 6.1 | /next | Escribir "/next" | Coach avanza a siguiente fase | ⬜ |
| 6.2 | /status | Escribir "/status" | Resumen completo: tracks, fase, métricas master | ⬜ |
| 6.3 | /analyze | Escribir "/analyze" | Análisis completo de la sesión | ⬜ |
| 6.4 | /help | Escribir "/help" | Lista de comandos disponibles | ⬜ |
| 6.5 | /map | Escribir "/map" | Mapa jerárquico de la sesión | ⬜ |
| 6.6 | /reset | Escribir "/reset" | Resetea estados de pista sin perder sesión | ⬜ |
| 6.7 | /skip | Escribir "/skip" | Salta el setup inicial | ⬜ |
| 6.8 | Comando desconocido | Escribir "/xyzzy" | Coach responde "no reconocido" + lista comandos | ⬜ |
| 6.9 | Mensaje libre | Pregunta en lenguaje natural | Coach responde con análisis contextual | ⬜ |

### 4.7 Rendimiento y Estabilidad

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 7.1 | CPU idle | Plugins cargados, DAW en stop | CPU < 2% en task manager | ⬜ |
| 7.2 | CPU playback | Reproducir proyecto 8 tracks | CPU < 15% adicional | ⬜ |
| 7.3 | CPU 50+ tracks | Proyecto con 50 tracks + Messengers | CPU < 30% adicional, sin pops/clicks | ⬜ |
| 7.4 | Memoria baseline | Cargar MixCoach + 1 Messenger | RAM < 200 MB adicional | ⬜ |
| 7.5 | Memoria 50 Messengers | Cargar 50 instancias | RAM < 500 MB adicional (10 MB c/u) | ⬜ |
| 7.6 | FPS UI | Tab 2 con todos los analizadores | UI responde, sin congelamiento | ⬜ |
| 7.7 | Frecuencia análisis | Verificar periodicAnalysis() | CoachEngine analiza cada ~8s | ⬜ |
| 7.8 | Fast analysis | Verificar fastTrackAnalysis() | Peak/RMS check cada ~2s | ⬜ |
| 7.9 | Cooldowns | Verificar cooldown de warnings | Mismo warning no se repite en < 60s | ⬜ |
| 7.10 | 24h uptime | Dejar corriendo 24h con audio | Sin crashes, sin memory leak (monitorear con Task Manager) | ⬜ |

### 4.8 Recuperación de Errores

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 8.1 | Sin audio | FL Studio en stop | Plugins no crashean, UI muestra valores por defecto | ⬜ |
| 8.2 | Sin Messengers | Cargar MixCoach sin Messengers | Coach detecta "sin Messengers" y guía al usuario | ⬜ |
| 8.3 | Messenger removido | Quitar Messenger de una pista durante reproducción | MixCoach detecta slot inactivo | ⬜ |
| 8.4 | FL Studio crash | Forzar cierre de FL | Sin archivos corruptos | ⬜ |
| 8.5 | Archivo referencia corrupto | Cargar WAV inválido | Coach muestra error, no crashea | ⬜ |
| 8.6 | Sample rate change | Cambiar sample rate en FL (44.1→48→96 kHz) | Plugins se adaptan sin crash | ⬜ |
| 8.7 | Buffer size change | Cambiar buffer (64→256→1024 samples) | Sin pops/clicks | ⬜ |
| 8.8 | Cerrar UI de plugin | Cerrar ventana del plugin durante reproducción | Sin crash | ⬜ |
| 8.9 | Múltiples instancias MixCoach | Cargar 2 MixCoach en diferentes tracks | Segundo no se abre (solo Master) | ⬜ |

### 4.9 Sesiones Multi-sesión

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 9.1 | Persistencia de setup | Configurar género, cerrar FL, reabrir | Coach recuerda género y fase | ⬜ |
| 9.2 | Persistencia de roles | Asignar roles, cerrar, reabrir | Roles preservados en session_memory.json | ⬜ |
| 9.3 | Persistencia de referencias | Cargar referencia, cerrar, reabrir | Referencia recordada (path/URL) | ⬜ |
| 9.4 | Persistencia de mapa | Construir mapa de mezcla, cerrar, reabrir | Mapa preservado | ⬜ |
| 9.5 | Nueva sesión | "Nuevo proyecto" en FL | Coach detecta sesión limpia y empieza desde cero | ⬜ |
| 9.6 | Returning user | Misma sesión después de días | Coach saluda por nombre ("Bienvenido de nuevo, [nombre]") | ⬜ |

### 4.10 Integración FL Studio

| # | Test | Procedimiento | Resultado esperado | ✅/❌ |
|:-:|:-----|:--------------|:-------------------|:----:|
| 10.1 | Clone pattern | Copiar y pegar track con Messenger | Nueva instancia se registra con slot diferente | ⬜ |
| 10.2 | Ctrl+Shift+V masivo | Pegar 10+ Messengers simultáneamente | Todos se registran, sin race conditions | ⬜ |
| 10.3 | Playlist → Mixer routing | Re-rutear pista en mixer | Messenger refleja nuevo bus | ⬜ |
| 10.4 | Mixer track renaming | Renombrar pista en mixer | Messenger/MixCoach muestra nuevo nombre | ⬜ |
| 10.5 | Project save/load | Guardar y cargar proyecto FLP | Plugins restauran estado completo | ⬜ |
| 10.6 | Export audio | Exportar mezcla a WAV/MP3 | Sin interferencia de plugins en export | ⬜ |
| 10.7 | Pause/resume | Pausar y reanudar reproducción | Análisis continúa normalmente | ⬜ |
| 10.8 | Mute/solo tracks | Silenciar pistas individuales | MixCoach detecta pista muteada | ⬜ |

---

## 📊 Reporte de Bugs

Formato para reportar bugs encontrados durante la beta:

```markdown
### 🐛 [Área] Título descriptivo

**Severidad:** 🔴 Crítico / 🟡 Alto / 🟢 Medio / ⚪ Bajo
**Componente:** MixCoach / Messenger / Ambos
**Test relacionado:** #X.Y (del checklist)

**Entorno:**
- FL Studio versión: [ej: 21.2 build 3589]
- Windows versión: [ej: 11 23H2]
- CPU/RAM: [ej: i7-12700K, 32GB]
- Resolución: [ej: 2560×1440]

**Pasos para reproducir:**
1. Abrir FL Studio
2. Cargar MixCoach en el Master
3. ...

**Resultado actual:** [lo que pasa]
**Resultado esperado:** [lo que debería pasar]

**Logs (si aplica):**
```
[clip del log relevante]
```

**Captura/screen recording (si aplica):**
```

---

## 📈 Criterios de Aceptación

### 🔴 Bloqueantes (must-fix antes de release)

| # | Criterio | Check |
|:-:|:---------|:-----:|
| B1 | Tests unitarios: 100% pass | ⬜ |
| B2 | Build Release: sin errores | ⬜ |
| B3 | MixCoach carga en FL Studio sin crash | ⬜ |
| B4 | Messenger carga en FL Studio sin crash | ⬜ |
| B5 | Comunicación IPC estable (sin pérdida de slots) | ⬜ |
| B6 | UI no se congela durante reproducción | ⬜ |
| B7 | No hay memory leaks detectable en 1h de uso | ⬜ |
| B8 | Chat IA responde sin crashes | ⬜ |

### 🟡 Altos (deben funcionar para beta)

| # | Criterio | Check |
|:-:|:---------|:-----:|
| H1 | Smoke tests 4.1-4.6 ≥ 90% pass rate | ⬜ |
| H2 | Carga de referencia de audio funciona | ⬜ |
| H3 | Analizadores muestran datos correctos | ⬜ |
| H4 | Comandos /status, /analyze, /help responden | ⬜ |
| H5 | Persistencia multi-sesión (setup + roles) | ⬜ |
| H6 | 8+ Messengers simultáneos sin degradación | ⬜ |

### 🟢 Medios (ideales para beta)

| # | Criterio | Check |
|:-:|:---------|:-----:|
| M1 | Smoke tests 4.7-4.10 ≥ 80% pass rate | ⬜ |
| M2 | Reference mode con gaps y progress | ⬜ |
| M3 | TrackFeed banner + health dots + filtros | ⬜ |
| M4 | Gain/Dynamics/Tonal advice responden | ⬜ |
| M5 | Vintage VU meters renderizan correctamente | ⬜ |

---

### 📊 Score Tracker de Beta

| Fecha | 4.1 | 4.2 | 4.3 | 4.4 | 4.5 | 4.6 | 4.7 | 4.8 | 4.9 | 4.10 | **Total** |
|:-----|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:----:|:---------:|
| — | — | — | — | — | — | — | — | — | — | — | **0%** |
| **Meta** | 8/8 | 10/10 | 19/19 | 15/15 | 10/10 | 9/9 | 10/10 | 9/9 | 6/6 | 8/8 | **104/104 (100%)** |

---

*Fin del BETA_TEST_PLAN.md — MixCoach Beta 1.0*

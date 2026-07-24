# 🎨 UX VISION PLAN — MixCoach → Experiencia Objetivo

> **LEER ANTES DE TOCAR CUALQUIER ARCHIVO DE UI.**
> Este documento define el plan exacto para llevar la UX de MixCoach del 65% al 90%+
> de la experiencia objetivo definida por el fundador en 5 imágenes de referencia.
>
> **Última actualización:** Julio 2026
> **Versión:** 1.0
> **Estado global:** 🟡 65% — 4 Sprints UX pendientes

---

## 📋 Índice

1. [🖼️ Las 5 Imágenes Objetivo](#️-las-5-imágenes-objetivo)
2. [💡 Descubrimiento Crítico](#-descubrimiento-crítico)
3. [📊 Estado Actual vs. Objetivo](#-estado-actual-vs-objetivo)
4. [🏃 Sprint UX-1 — Pantalla de Selección de Modo](#-sprint-ux-1--pantalla-de-selección-de-modo)
5. [🏃 Sprint UX-2 — Chat-First con Referencia + Barra 0→100%](#-sprint-ux-2--chat-first-con-referencia--barra-0100)
6. [🏃 Sprint UX-3 — MixMap con Panel de Detalle Lateral](#-sprint-ux-3--mixmap-con-panel-de-detalle-lateral)
7. [🏃 Sprint UX-4 — TrackProblemCard + Quick-Reply Buttons](#-sprint-ux-4--trackproblemcard--quick-reply-buttons)
8. [📅 Timeline](#-timeline)
9. [⚠️ Reglas Estrictas](#️-reglas-estrictas)

---

## 🖼️ Las 5 Imágenes Objetivo

El fundador definió la experiencia exacta a través de 5 imágenes que representan
el flujo completo de onboarding y sesión. **Cada IA debe conocer estas pantallas:**

| Imagen | Pantalla | Descripción |
|:------:|:---------|:------------|
| 1 | **Welcome** | Avatar robot 3D con headphones, campo de nombre, botón COMENZAR |
| 2 | **Modo** | ¿Qué vamos a hacer hoy? — Tarjetas grandes Mezclar / Masterizar |
| 3 | **Referencia** | Coach habla primero en el chat, luego aparece widget de carga inline |
| 4 | **MixMap** | Árbol jerárquico de pistas con panel lateral de detalle por track |
| 5 | **Coaching** | Coach agrupa problemas por familia con tarjetas ricas + quick-reply |

---

## 💡 Descubrimiento Crítico

> **NO hay que rehacer la arquitectura. Solo crear los componentes visuales.**

El análisis del código (Julio 2026) reveló que la arquitectura de flujo **ya está implementada**:

| Lo que parece que falta | Lo que realmente existe en código |
|:------------------------|:----------------------------------|
| Flujo de 5 pantallas | `CoachRoomState` con 14 fases (Welcome → Intention → Genre → Reference → Messenger → MixMap → Coaching) |
| Transición pantalla 1→2 | `NavigationShell::onStart` ya transiciona y llama `setSuggestions({"Mezclar", "Masterizar"})` |
| Lógica Mix vs Master | `CoachMode` enum + `PhaseManager::setCoachMode()` ya existe |
| Estados de referencia | `CoachRoomState::ReferenceStage` + `onReferenceFileAdded` callback ya wired |
| Transición a MixMap | `CoachRoomState::MixMapStage` + `onMixMapConfirmed` ya conectado |
| Análisis por track | Sprints 6A/B/C ✅ — `analyzeTrackGain/Dynamics/Tonal()` completamente funcionales |
| Sistema de prioridad | `MixPriorityEngine` ✅ implementado con score multidimensional |

**El problema es visual, no lógico.** Los componentes UI no reflejan la riqueza de la experiencia objetivo.

---

## 📊 Estado Actual vs. Objetivo

| Pantalla | Completitud | Qué falta |
|:---------|:-----------:|:----------|
| 1 — Welcome + Robot | 85% | Robot es 2D (no 3D), coach no "habla" antes del input |
| 2 — Selección de Modo | 20% | `CoachMode` existe pero NO hay `ModeSelectionCard` visual |
| 3 — Referencia en chat | 70% | `ReferencePanelComponent` existe separado, no inline en chat |
| Barra 0→100% análisis | 50% | `AnalysisStatus::Analyzing` existe en datos, no en UI |
| 4 — MixMap árbol | 60% | Árbol existe, falta panel lateral de detalle + íconos por instrumento |
| 5 — Coach con tarjetas | 65% | Inteligencia ✅, pero UI es burbujas de texto, no tarjetas ricas |

---

## 🏃 Sprint UX-1 — Pantalla de Selección de Modo

> **Imagen objetivo: Imagen 2**
> **Prioridad: 🔴 MÁXIMA — pieza más visible que falta**
> **Estimado: 2-3 días**

### Contexto

`CoachRoomState::Intention` (STATE 1) ya está definido.
`setSuggestions({"Mezclar", "Masterizar"})` ya se llama en `NavigationShell.cpp:45`.
El problema: las sugerencias se muestran como chips de texto. La imagen 2 muestra tarjetas grandes.

### Archivos a crear

#### [NEW] `Source/MixCoach/UI/ModeSelectionCard.h/.cpp`
```
Componente visual para cada tarjeta (Mezclar / Masterizar):
  - Fondo oscuro #0D0B1A con borde 1px rgba(255,255,255,0.08)
  - Ícono grande custom en área superior (230px height)
    • Mezclar: 3 faders verticales con glow morado (estilo RobotAvatarImage)
    • Masterizar: espectro de barras verticales + knob central
  - Título: 36px bold, color accent #A855F7
  - Descripción: 16px, gris claro
  - Features bullet: 14px, con ícono MixCoach pequeño
  - Hover: borde morado con glow, scale 1.02, fondo #1A0A2E
  - Click: gradiente morado en fondo + flecha → animada
  - Animación entrada: slide-up 200ms con fade (igual que PanelRevealAnim)
```

### Archivos a modificar

#### [MODIFY] `Source/MixCoach/UI/CoachChatComponent.cpp/h`
```
Cuando CoachRoomState == Intention:
  - En vez de chips, mostrar 2x ModeSelectionCard side-by-side
  - Los chips siguen para Genre y estados posteriores
  - Al seleccionar → CoachEngine::setCoachMode(CoachMode::Mix/Master)
  - → setCoachRoomState(CoachRoomState::Genre)
  - → addSystemMessage("Perfecto. ¿Con qué género trabajas hoy?")
```

### Criterio de completitud
- [ ] Usuario ve tarjetas grandes Mezclar/Masterizar al salir del Welcome
- [ ] Hover con animación funciona
- [ ] Selección transiciona al siguiente estado
- [ ] Compila sin errores: `cmake --build build --config Release --target MixCoach_VST3`

---

## 🏃 Sprint UX-2 — Chat-First con Referencia + Barra 0→100%

> **Imagen objetivo: Imagen 3**
> **Prioridad: 🔴 ALTA**
> **Estimado: 3-4 días**

### Contexto

`ReferencePanelComponent` existe como panel separado con `DropZoneComponent`.
La imagen 3 muestra el coach hablando primero y luego el widget de carga
aparece **dentro del flujo del chat** (no en un panel separado).

### Archivos a crear

#### [NEW] `Source/MixCoach/UI/ReferenceOnboardingCard.h/.cpp`
```
Tarjeta inline dentro del chat (se inserta como ChatBubble especial):
  Columna izquierda (CARGAR REFERENCIA):
    - DropZoneComponent reutilizado (no reescribir — ya existe)
    - Info del archivo cargado: nombre, duración, bit depth, sample rate
    - Botón "Analizar referencia" (morado, con ícono de waveform)
  Columna derecha (ENLACE DE REFERENCIA):
    - TextEditor para URL
    - Logos YouTube / Spotify / Apple Music (inline SVG)
    - Botón "Analizar enlace"
  Footer: mensaje del coach expandible
```

#### [NEW] `Source/MixCoach/UI/ReferenceAnalysisProgressCard.h/.cpp`
```
Aparece al clic de "Analizar referencia" — reemplaza o se añade después del card:
  - Número animado 0→100% con SmoothValue (suave, no lineal)
  - Barra de progreso con gradiente morado
  - Texto de estado: "Analizando espectro..." → "Calculando LUFS..." → "Listo"
  - Al llegar a 100%: collapse + aparece resumen:
    • LUFS integrado detectado
    • Rango dinámico
    • Estimación de género (si disponible)
  Timer: 60fps con SmoothValue, animación de ~3s
```

### Archivos a modificar

#### [MODIFY] `Source/MixCoach/audio/ReferenceAnalyzer.h/.cpp`
```
Agregar callback de progreso:
  std::function<void(float progress)> onProgress;  // 0.0 → 1.0

Reportar en etapas durante el análisis:
  - 0.10 al iniciar lectura de archivo
  - 0.30 al terminar lectura
  - 0.60 al completar FFT
  - 0.80 al calcular LUFS
  - 1.00 al finalizar
```

#### [MODIFY] `Source/MixCoach/UI/ChatMessagesComponent.h/.cpp`
```
Agregar soporte para ChatBubble inline especial:
  struct ChatBubble {
    ...
    bool isReferenceCard = false;           // [NUEVO]
    // El card se renderiza inline en el scroll
  };
  Método: void insertReferenceOnboardingCard();
```

#### [MODIFY] `Source/MixCoach/UI/CoachChatComponent.cpp`
```
Cuando CoachRoomState == ReferenceStage:
  - Postear mensaje del coach: "Genial! Ahora quiero saber cómo quieres sonar."
  - insertReferenceOnboardingCard() en el chat
  - NO mostrar el ReferencePanelComponent tradicional (es para sesiones activas)
```

### Criterio de completitud
- [ ] Coach habla → widget de referencia aparece inline en el chat
- [ ] Drag-drop de archivo funciona dentro del card
- [ ] Al clic "Analizar" → barra 0→100% con animación fluida
- [ ] Al llegar a 100% → coach avanza al siguiente estado
- [ ] Compila sin errores

---

## 🏃 Sprint UX-3 — MixMap con Panel de Detalle Lateral

> **Imagen objetivo: Imagen 4**
> **Prioridad: 🟠 MEDIA-ALTA**
> **Estimado: 3-4 días**

### Contexto

`MixMapComponent` (73KB, ~1,300 líneas) ya funciona con árbol jerárquico.
`onTrackSelected` callback ya existe. La imagen 4 añade:
- Panel lateral derecho con detalle rico del track seleccionado
- Switch "Vista: Mapa | Lista" en el header
- Nodos con íconos por tipo de instrumento

### Archivos a crear

#### [NEW] `Source/MixCoach/UI/MixMapDetailPanel.h/.cpp`
```
Panel lateral 320px — visible cuando el usuario hace clic en un nodo:

Header:
  - Ícono de instrumento grande + nombre + "× cerrar"
  - Badge "Problema detectado" (rojo) si hay TrackGainAdvice/DynamicsAdvice OffTarget
  - Badge "Prioridad Alta/Media/Baja" con color

Sección NIVEL:
  - Medidor RMS horizontal animado (usando SmoothValue)
  - Valor actual en dB (peak + RMS)
  - Valor target del rol (desde ExpectedProfile)

Sección PANORAMA:
  - Slider visual L — C — R (read-only, solo muestra posición)
  - Dot morado en posición actual (desde correlation/stereoWidth)

Sección EQUALIZACIÓN:
  - Curva EQ mini (120px altura) con 3 puntos de control
  - Dibujada desde bandEnergies[30] agrupadas en 6 regiones
  - Labels de frecuencias: 60Hz, 250Hz, 1kHz, 5kHz, 16kHz
  - Curva rellena con gradiente translúcido morado

Sección RUTA DE SEÑAL:
  - Lista vertical: Track → Bus → Drum Bus → Master
  - Flechas entre etapas (↓)

Botón "Escuchar en solo":
  - Icono auricular + texto
  - Callback: onRequestSolo(slotIndex)

Datos: lee de SharedData::getTrackAudioResult(slot) + TrackGainAdvice + TrackTonalAdvice
Actualización: cada 300ms desde NavigationShell timer
```

### Archivos a modificar

#### [MODIFY] `Source/MixCoach/UI/MixMapComponent.h/.cpp`
```
1. Switch Mapa/Lista en header:
   - Dos botones pill "Mapa" / "Lista" con estado activo
   - Modo Lista: render vertical compacto (nombre + rol + bus + health dot + nivel)

2. Íconos por TrackRole en nodos:
   - Kick/Snare/HiHat → ícono batería 🥁
   - Bass/808 → ícono bajo 🎸
   - Vocal/Lead → ícono micrófono 🎤
   - Piano/Synth → ícono teclado 🎹
   - Guitar → ícono guitarra 🎸
   - FX/Pad → ícono ola 🌊
   Usar glifos UTF-8 o mini SVG 16x16 por rol

3. onTrackSelected ya existe — conectar a MixMapDetailPanel:
   coachPanel_->showMixMapDetail(slotIndex);
```

#### [MODIFY] `Source/MixCoach/UI/CoachChatComponent.h/.cpp` (o NavigationShell.cpp)
```
- Añadir MixMapDetailPanel como child component (oculto por defecto)
- Mostrar/ocultar cuando onTrackSelected dispara
- Timer 300ms para actualizar datos del panel con SharedData
```

### Criterio de completitud
- [ ] Clic en nodo del árbol → panel lateral aparece con detalle
- [ ] Switch Mapa/Lista funciona con animación
- [ ] Íconos por tipo de instrumento visibles en nodos
- [ ] Panel actualiza datos en tiempo real
- [ ] Compila sin errores

---

## 🏃 Sprint UX-4 — TrackProblemCard + Quick-Reply Buttons

> **Imagen objetivo: Imagen 5**
> **Prioridad: 🟠 MEDIA**
> **Estimado: 4-5 días**

### Contexto

La inteligencia de análisis es completa (Sprints 6A/B/C, MixPriorityEngine).
La imagen 5 muestra mensajes del coach con tarjetas ricas agrupadas por familia
y quick-reply buttons debajo del último mensaje del coach.

### Archivos a crear

#### [NEW] `Source/MixCoach/UI/TrackProblemCard.h/.cpp`
```
Tarjeta por track problemático — aparece dentro del ChatBubble:

Header de grupo (un header por familia de pistas):
  - Ícono de familia + "Grupo: Batería" + contador "● 3 pistas problemáticas"

Por cada track en el grupo:
  - Ícono instrumento + nombre del track
  - Badge "Problema" (rojo) / "Advertencia" (amarillo)
  - Descripción corta: "Exceso en 2.5 kHz" / "Demasiado subgrave"
  - Mini-spectrum: waveform de 80px con color por severity
    (rojo = crítico, amarillo = advertencia, verde = ok)
  - Impacto: dots rellenos (●●●●○ = 4/5)
    Valor de MixPriorityEngine::getPriorityScore(slotIndex)
  - Enmascaramiento: "Con Kick, Bass" (desde analyzeTrackTonal comparando slots)
  - Botón "Ver más" → expande detalles + sugerencia exacta del coach

Footer del grupo:
  - Tip del coach: "Te recomiendo trabajar primero en estas pistas..."
  - Botón "Ir a Tools" → setActiveTab(Tools)
```

#### [NEW] `Source/MixCoach/UI/QuickReplyBar.h/.cpp`
```
Barra de botones de respuesta rápida debajo del último mensaje del coach:

Layout: fila horizontal de botones pill
  - Max 4 botones, overflow: scroll horizontal
  - Fondo: rgba(168, 85, 247, 0.1), borde morado tenue
  - Hover: fondo morado 20%, escala 1.02
  - Click: envía el texto como mensaje del usuario + barra desaparece

API pública:
  void setReplies(const std::vector<juce::String>& replies);
  void clearReplies();
  std::function<void(const juce::String&)> onReplySelected;

Integración: el callback dispara el mismo flujo que el TextEditor del chat
```

### Archivos a modificar

#### [MODIFY] `Source/MixCoach/UI/ChatMessagesComponent.h/.cpp`
```
1. Soporte para ChatBubble con group cards:
   struct ChatBubble {
     ...
     bool isTrackGroupCard = false;           // [NUEVO]
     std::vector<TrackProblemData> trackProblems; // [NUEVO]
   };

2. QuickReplyBar management:
   void setQuickReplies(const std::vector<juce::String>& replies);
   - QuickReplyBar mostrada debajo del scroll (sticky bottom)
   - Se limpia cuando el usuario escribe en el TextEditor
```

#### [MODIFY] `Source/MixCoach/engine/CoachEngine.cpp`
```
En sendGainAnalysisConsolidated() / periodicAnalysis():

1. Agrupar problemas por bus/familia antes de postear al chat:
   buildTrackGroupsByBus() → vector<TrackGroup{busType, vector<slotIndex>}>

2. Postear grupo como TrackGroupCard en vez de texto plano:
   addTrackGroupCardMessage(groups, topMessage)

3. Después del mensaje → setQuickReplies({
   "Sí, guíame paso a paso",
   "Dame sugerencias generales",
   "No, lo revisaré después"
   });

4. Handler de cada respuesta:
   "Sí, guíame" → empezar loop de corrección uno por uno
   "Sugerencias" → postear lista con todos los ajustes sugeridos
   "Después" → coach respeta, guarda en pendientes
```

### Criterio de completitud
- [ ] Mensajes de problemas aparecen como tarjetas agrupadas por familia
- [ ] Mini-spectrum animado en cada tarjeta
- [ ] Quick-reply buttons aparecen después del mensaje del coach
- [ ] Seleccionar quick-reply avanza la conversación
- [ ] Compila sin errores

---

## 📅 Timeline

```
Semana 1:
  Días 1-3: Sprint UX-1 (ModeSelectionCard)
  Días 4-5: Sprint UX-2 parte 1 (ReferenceOnboardingCard)

Semana 2:
  Días 1-2: Sprint UX-2 parte 2 (barra 0→100% + ReferenceAnalyzer)
  Días 3-5: Sprint UX-3 (MixMapDetailPanel + switch + íconos)

Semana 3:
  Días 1-3: Sprint UX-4 parte 1 (TrackProblemCard)
  Días 4-5: Sprint UX-4 parte 2 (QuickReplyBar + CoachEngine grouping)
```

| Sprint | Duración | Resultado visible |
|:-------|:--------:|:------------------|
| UX-1 | 2-3 días | Tarjetas grandes Mezclar/Masterizar |
| UX-2 | 3-4 días | Widget de referencia inline + barra 0→100% |
| UX-3 | 3-4 días | Panel lateral de detalle en MixMap |
| UX-4 | 4-5 días | Tarjetas de problemas por familia + quick-reply |
| **Total** | **~3 semanas** | **Experiencia objetivo ~90%** |

---

## ⚠️ Reglas Estrictas para esta Área del Código

> Estas reglas son adicionales a las 14 Leyes del Sistema en AI_CONTEXT.md §11.
> Son específicas para los Sprints UX.

1. **No tocar archivos CORE** — `SharedMemory`, `SlotRegistry`, `SharedData`, `AudioAnalyzer`, `CoachEngine` base
2. **Cambio mínimo al modificar** — Reutilizar siempre: `DropZoneComponent`, `SmoothValue`, `MixCoachTheme`, `PanelRevealAnim`
3. **Íconos: UTF-8 o SVG embebido** — Mismo approach que `RobotAvatarImage` (array de bytes en `.cpp`)
4. **Colores: solo via MixCoachTheme** — Nunca `Colour(0xFF...)` directo. `accent()` = `#A855F7`
5. **Timers: solo en Message Thread** — Nada de análisis pesado en paint() o timer de UI
6. **Compilar después de cada sprint** — `cmake --build build --config Release --target MixCoach_VST3`
7. **Los sprints son secuenciales** — UX-1 → UX-2 → UX-3 → UX-4. No saltear.

---

## 🏁 Criterio de Éxito Global

La experiencia está al 90%+ cuando el usuario puede:

```
1. ✅ Abrir plugin → robot + nombre → COMENZAR
2. ✅ Ver tarjetas Mezclar/Masterizar → elegir una con animación
3. ✅ Coach pide referencia en el chat → drag-drop → barra 0→100% → listo
4. ✅ MixMap árbol → clic en track → panel lateral con nivel/EQ/ruta
5. ✅ Coach agrupa problemas por familia → tarjetas ricas → quick-reply → coaching
```

---

*UX Vision Plan — MixCoach — Julio 2026*
*Leer junto con: workspace_memory/PLAN_10_10.md + workspace_memory/ROADMAP.md*

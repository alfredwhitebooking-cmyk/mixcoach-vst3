# 🎯 PLAN UX V2 — MixCoach → Experiencia de Ingeniero de Mezcla

> **LEER PRIMERO.** Este documento define el plan maestro para llevar MixCoach
> desde su estado actual hasta la experiencia descrita por el fundador:
> *"Un ingeniero de mezcla con 25 años sentado a tu lado."*
>
> Cualquier IA que entre al proyecto DEBE leer este documento ANTES de tocar código.
> La sección [📍 Estado Actual](#-estado-actual--julio-2026) indica exactamente
> en qué punto del plan estamos y cuál es la siguiente tarea.
>
> **Última actualización:** Julio 2026
> **Versión:** 1.0
> **Plan complementario:** `workspace_memory/PLAN_10_10.md` (plan maestro técnico)
> **Docs de referencia:** `workspace_memory/UX_VISION_PLAN.md`, `workspace_memory/SCENE_VISUAL_SPEC.md`

---

## 📋 Índice

1. [📍 Estado Actual — Julio 2026](#-estado-actual--julio-2026)
2. [🎯 Experiencia Objetivo (V2)](#-experiencia-objetivo-v2)
3. [📊 Gap Analysis: Código Actual vs V2](#-gap-analysis-código-actual-vs-v2)
4. [🗺️ Las 4 Fases del Plan](#️-las-4-fases-del-plan)
5. [📌 Prioridad Actual — Siguiente Acción Concreta](#-prioridad-actual--siguiente-acción-concreta)
6. [✅ Lo que YA FUNCIONA (no tocar)](#-lo-que-ya-funciona-no-tocar)
7. [⚠️ Lo que ESTÁ CREADO pero desconectado](#-lo-que-está-creado-pero-desconectado)
8. [❌ Lo que NO EXISTE (hay que crear)](#-lo-que-no-existe-hay-que-crear)
9. [🔧 Detalle de cada Fase](#-detalle-de-cada-fase)
10. [📐 Reglas de Arquitectura](#-reglas-de-arquitectura)

---

## 📍 Estado Actual — Julio 2026

```text
⚠️ INTERRUPCIÓN: El archivo CoachChatComponent.cpp se perdió por git checkout
   y fue RECONSTRUIDO desde el historial de la conversación. Verificar que
   todo el código de intención (setShowIntentionScreen, timerCallback,
   resized MODO INTENCION, startIntentionAnimation) esté presente y funcione.
   ✅ RECONSTRUIDO: Build exitoso, standalone corre sin errores.

⚠️ Las tarjetas ModeSelectionCard y GenreSelectionCard NO tenían
   addAndMakeVisible() en el constructor. Fue ARREGLADO en la reconstrucción.

⚠️ El flujo NavigationShell → setShowIntentionScreen → ModeSelectionCard
   onCardSelected → onSuggestionClicked → setShowGenreCards → setShowGenreCards
   → onGenreCardSelected → ReferenceStage → MessengerStage → MixMapStage
   → GainStaging (FullUI) está ✅ CONECTADO.

⚠️ SessionPrepChecklist existe pero NO está en el flujo actual
   (NavigationShell no conecta onSessionPrepConfirmed).
```

### Punto exacto del plan

```text
FASE 0 — RECONSTRUCCIÓN Y ESTABILIZACIÓN: ✅ COMPLETADO
  CoachChatComponent.cpp reconstruido, build exitoso, flujo de setup conectado.

FASE 1 — COACHING EN CHAT CON TARJETAS RICAS: 🔴 SIGUIENTE
  TrackProblemCard + QuickReplyBar existen pero no están conectados al chat.

FASE 2 — PANEL DE DETALLE MIXMAP: 🟡 PREPARACIÓN
  MixMapDetailPanel existe como .h pero MixMapComponent no lo muestra.

FASE 3 — REFERENCIA INLINE + PROGRESO: ⏳ PENDIENTE
  ReferenceOnboardingCard y AnalysisProgressCard existen pero no conectados.

FASE 4 — PLUGIN INVENTORY + TIERED SUGGESTIONS: ⏳ PENDIENTE
  PluginScanner y PluginSuggestionsProvider existen pero no integrados en chat.
```

### Archivos críticos estado actual

| Archivo | Estado | Observaciones |
|:--------|:------:|:--------------|
| `Source/MixCoach/UI/CoachChatComponent.cpp` | ✅ Reconstruido | Build exitoso, ~2550 líneas |
| `Source/MixCoach/UI/CoachChatComponent.h` | ✅ Intacto | Nunca se perdió |
| `Source/MixCoach/UI/NavigationShell.cpp` | ✅ Intacto | Flujo completo conectado |
| `Source/MixCoach/UI/WelcomeComponent.cpp` | ✅ Intacto | Sin cambios |
| `Source/MixCoach/UI/ModeSelectionCard.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/GenreSelectionCard.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/QuickReplyBar.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/TrackProblemCard.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/ReferenceOnboardingCard.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/ReferenceAnalysisProgressCard.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/SessionPrepChecklist.cpp` | ✅ Existe | .h/.cpp untracked |
| `Source/MixCoach/UI/MixMapDetailPanel.cpp` | ⚠️ Parcial | .h existe, .cpp no verificado |

---

## 🎯 Experiencia Objetivo (V2)

El usuario debe sentir que un ingeniero de mezcla con 25 años de experiencia
está escuchando su sesión en tiempo real. La experiencia completa es:

### Setup Flow

```text
1. BIENVENIDA
   🤖 "Hola, Carlos. Soy MixCoach."
   [Input: nombre] [COMENZAR]

2. SELECCIÓN DE MODO
   🤖 "¿En qué vamos a trabajar hoy?"
   [🎚 MEZCLAR] [🎛 MASTERIZAR]
   Tarjetas grandes con íconos + features bullet

3. GÉNERO
   🤖 "¿Qué género vamos a mezclar?"
   10 tarjetas de género con íconos
   → Coach responde con perfil del género:
   "El Afrobeat necesita: Groove, Punch, Movimiento estéreo..."

4. REFERENCIA
   🤖 "¿Tienes una referencia?"
   [📁 Arrastrar WAV] [🔗 Pegar enlace]
   → Barra de análisis 0→100% animada
   → Resumen con: LUFS, True Peak, Stereo, Crest, Sub, Air, Transientes

5. PREPARACIÓN DE SESIÓN
   □ Inserta Messenger en todos los canales
   □ Renombra pistas
   □ Agrupa por colores
   □ Crea buses
   □ Ruta correctamente

6. SCANEO DE SESIÓN
   ███ 52 pistas detectadas
   → Identificando instrumentos con % de confianza
   → Confirmación de identidad dudosa

7. MIX MAP
   Árbol jerárquico con buses
   "¿Confirmas que el mapa es correcto?"
```

### Coaching Flow (Loop principal)

```text
8. COACHING ACTIVO (por cada problema):
   
   a. Coach detecta problema en tiempo real
   b. Coach explica en lenguaje humano
   c. Se abre automáticamente el analyzer correspondiente
   d. Coach muestra evidencia visual (espectro, medidor, fase)
   e. Coach pregunta "¿Cómo quieres solucionarlo?"
   f. Coach muestra 3 tiers de plugins:
      🎛 Nativo (FL Studio native)
      🟢 Gratis
      💎 Profesional
   g. Coach da instrucción exacta paso a paso
   h. Usuario aplica
   i. Coach verifica resultado
   j. Coach celebra o corrige
   
   Tipos de problemas en orden de prioridad:
   1. Clipping en master
   2. Masking (Kick vs Bass)
   3. Phase issues
   4. Falta de presencia (EQ)
   5. Falta de profundidad (Reverb)
   6. Demasiada dinámica (Crest)
```

---

## 📊 Gap Analysis: Código Actual vs V2

| # | Componente V2 | Estado | Código existente | Qué falta |
|:-:|:--------------|:------:|:-----------------|:----------|
| 1 | Welcome + nombre + COMENZAR | ✅ COMPLETO | `WelcomeComponent` con animación staggered 800ms | Nada |
| 2 | Modo Selection cards | ✅ CONECTADO | `ModeSelectionCard` + recién cableado a NavigationShell | Probar visualmente |
| 3 | Género selection cards | ✅ CONECTADO | `GenreSelectionCard` + recién cableado | Probar visualmente |
| 4 | Coach respuesta por género | ⚠️ PARCIAL | CoachEngine.detectAndSetGenre() postea al chat | Mensaje más rico con perfil del género |
| 5 | Referencia inline en chat | ⚠️ PARCIAL | `ReferenceOnboardingCard` existe + `ReferencePanelComponent` separado | Conectar ReferenceOnboardingCard inline en ChatMessagesComponent |
| 6 | Barra de análisis 0→100% | ⚠️ PARCIAL | `ReferenceAnalysisProgressCard` existe con showSummary() | Conectar al flujo de análisis + animación SmoothValue |
| 7 | Métricas de análisis (LUFS, TP, etc.) | ⚠️ PARCIAL | ReferenceAnalyzer calcula todo | ReferenceAnalysisProgressCard.showSummary() necesita expandirse |
| 8 | Session Prep Checklist | ⚠️ PARCIAL | `SessionPrepChecklist` existe | NavigationShell no conecta onSessionPrepConfirmed |
| 9 | Scan de sesión animado | ❌ NO EXISTE | N/A | Nuevo componente: SessionScanCard con barra + contador |
| 10 | Identificación con confianza % | ✅ COMPLETO | `TrackRole` con confidence + CorrectionLearner | Ya se muestra en MessengerList |
| 11 | MixMap jerárquico | ✅ COMPLETO | `MixMapComponent` con árbol + buses | Conectar MixMapDetailPanel |
| 12 | **MixMapDetailPanel** | ⚠️ PARCIAL | `.h` existe, funciones declaradas | No implementado/compilado en build actual |
| 13 | TrackProblemCard en chat | ⚠️ PARCIAL | `TrackProblemCard` existe .h/.cpp | No conectado como ChatBubble inline |
| 14 | **QuickReplyBar** | ⚠️ PARCIAL | `QuickReplyBar` existe .h/.cpp | No wired a CoachChatComponent |
| 15 | Plugin suggestions 3 tiers | ⚠️ PARCIAL | `PluginDatabase` + `PluginSuggestionsProvider` + `PluginScanner` existen | No conectados al flujo de coaching en chat |
| 16 | Plugin inventory detection | ❌ NO EXISTE | `PluginScanner` escanea directorios | No integrado al setup flow |
| 17 | Auto-abrir analyzer | ❌ NO EXISTE | AnalyzersPanelComponent existe | No hay comando para abrir analyzer específico |
| 18 | Step-by-step correction loop | ⚠️ PARCIAL | `CorrectionLearner` + `MixHistory` (parcial) | No conectado como loop completo |
| 19 | Progress bar con milestones | ❌ NO EXISTE | `ProgressScreen` existe pero no la barra de milestones | UI de progreso no coincide con V2 |
| 20 | Every problem as rich card | ❌ NO EXISTE | TrackFeedCore genera eventos | No hay tarjetas visuales por problema |

---

## 🗺️ Las 4 Fases del Plan

### Fase 1 — Conectar Componentes Existentes 🔴 PRIORIDAD #1

> **Objetivo:** Todos los componentes que YA ESTÁN CREADOS aparecen visibles
> en la UI y están cableados al flujo correcto.

| # | Tarea | Archivos | Esfuerzo |
|:-:|:------|:---------|:---------|
| 1.1 | Verificar visualmente ModeSelectionCard + GenreSelectionCard en standalone | `Source/MixCoach/UI/ModeSelectionCard.cpp`, `GenreSelectionCard.cpp` | 🟢 30min |
| 1.2 | Conectar QuickReplyBar a CoachChatComponent (aparece/desaparece con mensajes) | `Source/MixCoach/UI/CoachChatComponent.cpp`, `QuickReplyBar.h/.cpp` | 🟡 2-3h |
| 1.3 | Conectar TrackProblemCard como tipo de ChatBubble especial | `Source/MixCoach/UI/ChatMessagesComponent.h/.cpp`, `TrackProblemCard.h/.cpp` | 🟡 3-4h |
| 1.4 | Conectar ReferenceOnboardingCard inline en ChatMessagesComponent | `Source/MixCoach/UI/ChatMessagesComponent.h/.cpp`, `ReferenceOnboardingCard.h/.cpp` | 🟡 3-4h |
| 1.5 | Conectar ReferenceAnalysisProgressCard con animación SmoothValue | `Source/MixCoach/UI/ChatMessagesComponent.h/.cpp`, `ReferenceAnalysisProgressCard.h/.cpp` | 🟡 2-3h |
| 1.6 | Conectar SessionPrepChecklist a NavigationShell via onSessionPrepConfirmed | `Source/MixCoach/UI/NavigationShell.cpp` | 🟢 1h |
| 1.7 | Conectar MixMapDetailPanel a MixMapComponent.onTrackSelected | `Source/MixCoach/UI/MixMapComponent.cpp`, `MixMapDetailPanel.cpp` | 🟡 3-4h |
| 1.8 | Build + test visual de todo el flujo | — | 🟢 1h |

**Duración estimada:** ~4-5 días
**Criterio de completitud:** Todos los componentes existen en la UI y son visibles.

---

### Fase 2 — Coach con Tarjetas de Problemas 🔴 PRIORIDAD #1

> **Objetivo:** El Coach postea problemas como tarjetas visuales ricas
> con agrupación por familia, no como texto plano.

| # | Tarea | Archivos | Esfuerzo |
|:-:|:------|:---------|:---------|
| 2.1 | Integrar TrackProblemCard en CoachEngine para postear problemas como tarjetas | `Source/MixCoach/engine/CoachEngine.cpp`, `ai/AiCoachAdapterPrompts.cpp` | 🟡 3-4h |
| 2.2 | Agrupar problemas por bus/familia antes de postear | `Source/MixCoach/engine/CoachEngine.cpp` (nuevo método `buildTrackGroupsByBus()`) | 🟡 2-3h |
| 2.3 | Mini-waveform en TrackProblemCard desde bandEnergies | `Source/MixCoach/UI/TrackProblemCard.cpp` | 🟡 2h |
| 2.4 | Impact dots basados en MixPriorityEngine | `Source/MixCoach/UI/TrackProblemCard.cpp` | 🟢 1h |
| 2.5 | Conectar QuickReplyBar después de cada mensaje del coach con opciones | `Source/MixCoach/UI/QuickReplyBar.cpp`, `CoachChatComponent.cpp` | 🟡 2-3h |
| 2.6 | Manejar respuesta de QuickReply (guíame / sugerencias / después) | `Source/MixCoach/engine/CoachEngineCommands.cpp` | 🟡 3-4h |
| 2.7 | Build + test | — | 🟢 1h |

**Duración estimada:** ~5-6 días
**Criterio de completitud:** Cada problema aparece como tarjeta visual con
mini-waveform + impact dots + quick-reply buttons.

---

### Fase 3 — Referencia + Plugin Ecosystem 🟡 PRIORIDAD #2

> **Objetivo:** La referencia se carga inline en el chat con barra animada.
> El Coach conoce los plugins del usuario y sugiere en 3 tiers.

| # | Tarea | Archivos | Esfuerzo |
|:-:|:------|:---------|:---------|
| 3.1 | ReferenceOnboardingCard soporte drag-drop + URL inline en chat | `Source/MixCoach/UI/ReferenceOnboardingCard.cpp`, `DropZoneComponent.cpp` | 🟡 3-4h |
| 3.2 | ReferenceAnalysisProgressCard con barra animada SmoothValue + estados | `Source/MixCoach/UI/ReferenceAnalysisProgressCard.cpp` | 🟡 2-3h |
| 3.3 | ReferenceAnalyzer.onProgress callback para reportar progreso 0→1 | `Source/MixCoach/audio/ReferenceAnalyzer.h/.cpp` | 🟢 1-2h |
| 3.4 | Resumen expandido con todas las métricas V2 | `Source/MixCoach/UI/ReferenceAnalysisProgressCard.cpp` | 🟡 2-3h |
| 3.5 | PluginScanner detectar plugins instalados al inicio | `Source/MixCoach/engine/PluginScanner.cpp` | 🟡 3-4h |
| 3.6 | PluginSuggestionsProvider generar 3 tiers por problema | `Source/MixCoach/engine/PluginSuggestionsProvider.cpp` | 🟡 3-4h |
| 3.7 | Integrar tiered suggestions en tarjetas de problema | `Source/MixCoach/engine/CoachEngineCommands.cpp`, `TrackProblemCard.cpp` | 🟡 3-4h |
| 3.8 | Auto-open analyzer cuando coach menciona un dominio | `Source/MixCoach/engine/CoachEngine.cpp`, `NavigationShell.cpp` | 🟡 2-3h |
| 3.9 | Build + test | — | 🟢 1h |

**Duración estimada:** ~7-8 días
**Criterio de completitud:** Referencia inline + barra animada + plugin tiered
suggestions integrados.

---

### Fase 4 — Coach Loop + Progress Journey 🟡 PRIORIDAD #2

> **Objetivo:** Loop completo de corrección: Coach recomienda → usuario aplica
> → Coach verifica → celebra/corrige. Progreso visible con milestones.

| # | Tarea | Archivos | Esfuerzo |
|:-:|:------|:---------|:---------|
| 4.1 | MixHistory: lista circular de últimos 50 eventos | `Source/MixCoach/engine/MixHistory.h/.cpp` (nuevo) | 🟡 3-4h |
| 4.2 | Loop de corrección: detectar si usuario aplicó el cambio | `Source/MixCoach/engine/CoachEngineCorrection.cpp` | 🟡 4-5h |
| 4.3 | Conectar al chat: "Bájale 4dB, te pasaste" / "✅ Exacto" | `Source/MixCoach/engine/CoachEngineCommands.cpp` | 🟡 2-3h |
| 4.4 | Session scanning animation (tarjeta con barra) | `Source/MixCoach/UI/ChatMessagesComponent.cpp` (nuevo tipo bubble) | 🟡 3-4h |
| 4.5 | Progress milestones bar (0→100% con checkpoints) | `Source/MixCoach/UI/ProgressScreen.cpp` | 🟡 3-4h |
| 4.6 | Build + test | — | 🟢 1h |

**Duración estimada:** ~6-7 días
**Criterio de completitud:** Loop de corrección funcional, progreso visible
con milestones celebrados.

---

## 📌 Prioridad Actual — Siguiente Acción Concreta

```text
🥇 FASE 1.1 — Verificar visualmente ModeSelectionCard + GenreSelectionCard
    Acción:
    1. Build Debug → standalone
    2. Abrir MixCoach.exe
    3. Click COMENZAR → ¿se ven las 2 tarjetas Mezclar/Masterizar?
    4. Click Mezclar → ¿se ocultan las tarjetas de modo y aparecen las de género?
    5. Click Afrobeat → ¿transiciona a ReferenceStage?

🥇 FASE 1.2 — Conectar QuickReplyBar a CoachChatComponent
    Acción: QuickReplyBar es visible en el chat cuando se llama showQuickReplies()

🥇 FASE 1.3 — Conectar TrackProblemCard como ChatBubble
    Acción: El coach puede postear problemas como tarjetas visuales
```

---

## ✅ Lo que YA FUNCIONA (no tocar)

| Componente | Archivos | Confianza |
|:-----------|:---------|:---------:|
| WelcomeComponent con animación | `WelcomeComponent.cpp/h` | 🟢 100% |
| RobotAvatarComponent | `RobotAvatarComponent.cpp/h` | 🟢 100% |
| NavigationShell 14 estados | `NavigationShell.cpp/h` | 🟢 100% |
| CoachEngine 6 fases | `CoachEngine.cpp/h` | 🟢 100% |
| TrackGainAdvice (Sprint 6A) | `CoachEngine.cpp/h` | 🟢 100% |
| TrackDynamicsAdvice (Sprint 6B) | `CoachEngine.cpp/h` | 🟢 100% |
| TrackTonalAdvice (Sprint 6C) | `CoachEngine.cpp/h` | 🟢 100% |
| MixPriorityEngine | `MixPriorityEngine.cpp/h` | 🟢 100% |
| MixMapComponent árbol | `MixMapComponent.cpp/h` | 🟢 100% |
| ReferenceAnalyzer | `ReferenceAnalyzer.cpp/h` | 🟢 100% |
| ReferenceDrivenEngine | `ReferenceDrivenEngine.cpp/h` | 🟢 100% |
| TrackFeedCore + health | `TrackFeedCore`, `TrackState` | 🟢 100% |
| PluginDatabase + Scanner | `PluginDatabase`, `PluginScanner` | 🟢 100% |
| PhaseManager progresión | `PhaseManager.cpp/h` | 🟢 100% |
| SceneManager director | `SceneManager.cpp/h` | 🟢 100% |
| CorrectionLearner | `CorrectionLearner.cpp/h` | 🟢 100% |
| MODO INTENCION + animaciones | `CoachChatComponent.cpp` | 🟡 Reconstruido |

---

## ⚠️ Lo que ESTÁ CREADO pero desconectado

| Componente | Existe en | Conectar en | Prioridad |
|:-----------|:----------|:------------|:---------:|
| `ModeSelectionCard` | `ModeSelectionCard.h/.cpp` | ✅ RECIÉN CONECTADO | — |
| `GenreSelectionCard` | `GenreSelectionCard.h/.cpp` | ✅ RECIÉN CONECTADO | — |
| `QuickReplyBar` | `QuickReplyBar.h/.cpp` | `CoachChatComponent.cpp` | 🔴 Fase 1 |
| `TrackProblemCard` | `TrackProblemCard.h/.cpp` | `ChatMessagesComponent.cpp` | 🔴 Fase 1 |
| `ReferenceOnboardingCard` | `ReferenceOnboardingCard.h/.cpp` | `ChatMessagesComponent.cpp` | 🟡 Fase 3 |
| `ReferenceAnalysisProgressCard` | `ReferenceAnalysisProgressCard.h/.cpp` | `ChatMessagesComponent.cpp` | 🟡 Fase 3 |
| `SessionPrepChecklist` | `SessionPrepChecklist.h/.cpp` | `NavigationShell.cpp` | 🟡 Fase 3 |
| `MixMapDetailPanel` | `MixMapDetailPanel.h (?)` | `MixMapComponent.cpp` | 🟡 Fase 1 |

---

## ❌ Lo que NO EXISTE (hay que crear)

| Componente | Descripción | Prioridad |
|:-----------|:------------|:---------:|
| Plugin inventory detection | Escanear plugins instalados al inicio + mensaje de bienvenida | 🟡 Fase 3 |
| Session scanning card | Tarjeta animada "52 pistas detectadas" con barra | 🟡 Fase 4 |
| Progress milestones bar | Barra 0→100% con checkpoints por fase completada | 🟡 Fase 4 |
| Auto-open analyzer | Comando para abrir analyzer específico (espectro, fase, etc.) | 🟡 Fase 3 |
| MixHistory (lista circular) | Historial de últimos 50 eventos de corrección | 🟡 Fase 4 |
| Tiered plugin suggestions UI | Interfaz de tarjeta con 3 tiers nativo/gratis/profesional | 🟡 Fase 3 |

---

## 📐 Reglas de Arquitectura

1. **No tocar archivos CORE**: `SharedMemory`, `SlotRegistry`, `SharedData`, `AudioAnalyzer`
2. **Reutilizar siempre**: `SmoothValue`, `DropZoneComponent`, `MixCoachTheme`, `PanelRevealAnim`
3. **Colores solo vía MixCoachTheme**: `accent()` = `#A855F7`, `textDim()`, `border()`
4. **Messenger no se modifica**: Solo se toca `Source/MixCoach/`, nunca `Source/Messenger/`
5. **Timers solo en Message Thread**: Nada de análisis pesado en paint() o timer de UI
6. **Cada fase compila**: `cmake --build build --config Debug --target MixCoach_Standalone`
7. **No crear nuevas pantallas**: Todo debe vivir dentro del chat o del mix map existente
8. **El chat es el centro**: Cualquier interacción del usuario pasa por el chat

---

## 🔄 Cómo Continuar

Cuando entres a este proyecto:

1. **LEE ESTE DOC** (`workspace_memory/PLAN_UX_V2.md`) — especialmente el
   [📍 Estado Actual](#-estado-actual--julio-2026) y
   [📌 Prioridad Actual](#-prioridad-actual--siguiente-acción-concreta)

2. **LEE** `workspace_memory/PLAN_10_10.md` — el plan maestro técnico

3. **VERIFICA** que el estado actual en `PLAN_UX_V2.md §Estado Actual`
   sigue siendo correcto (el estado cambia cuando se completan fases)

4. **TRABAJA** en la siguiente tarea de la [📌 Prioridad Actual](#-prioridad-actual--siguiente-acción-concreta)

5. **ACTUALIZA** este documento cuando completes una fase:
   - Marca la tarea como ✅ COMPLETADA
   - Mueve la [📍 Prioridad Actual](#-prioridad-actual--siguiente-acción-concreta) a la siguiente tarea

---

*Plan UX V2 — MixCoach — Julio 2026*
*Documento "tatuado" — cualquier IA debe leerlo al entrar al proyecto.*

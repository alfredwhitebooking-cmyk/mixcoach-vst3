# 🎭 COACHING_NARRATIVE.md

> **El loop narrativo de coaching de MixCoach — El corazón de la experiencia.**
> Define cómo el Coach guía al usuario a través de cada problema de mezcla,
> desde la detección hasta la celebración.
>
> **Versión:** 1.0 | **Última actualización:** 19 julio 2026
> **Documentos relacionados:**
> - `03_SESSION_FLOW.md` — Flujo de sesión completo
> - `05_UI_ARCHITECTURE.md` — Arquitectura UI con split-view
> - `THE_MIXCOACH_PHILOSOPHY.md` — Filosofía del producto

---

## 📋 Índice

1. [El Loop de 7 Pasos](#1-el-loop-de-7-pasos)
2. [Timing Targets](#2-timing-targets)
3. [Diagrama de Estados](#3-diagrama-de-estados)
4. [Estructuras de Datos](#4-estructuras-de-datos)
5. [Ciclo de Vida](#5-ciclo-de-vida)
6. [Mapeo Problema → Evidencia](#6-mapeo-problema--evidencia)
7. [Integración con Componentes UI](#7-integracion-con-componentes-ui)
8. [Ejemplo Completo](#8-ejemplo-completo)
9. [Referencia de API](#9-referencia-de-api)

---

## 1. El Loop de 7 Pasos

El `CoachingNarrativeDirector` ejecuta exactamente 7 pasos por cada problema detectado.
Ningún paso se salta. Ningún paso cambia de orden.

### Diagrama Conceptual

```
                    ┌──────────────────────────────────────┐
                    │       COACHING NARRATIVE LOOP        │
                    │         (por problema)                │
                    └──────────────────────────────────────┘

  Motor detecta problema
         │
         ▼
 ┌─────────────────┐
 │  1. DETECT      │  (implícito: MixPriorityEngine)
 │                 │  → "He encontrado un problema"
 └────────┬────────┘
          │ 800ms typing
          ▼
 ┌─────────────────┐
 │  2. SHOW        │  → Abre analyzer contextual
 │     EVIDENCE    │  → Highlight frequency (línea roja)
 │                 │  → System msg: evidencias
 └────────┬────────┘
          │ 1000ms delay
          ▼
 ┌─────────────────┐
 │  3. EXPLAIN     │  → Coach msg: qué está pasando
 │                 │  → Avatar nod + thinking
 │                 │  → "El Kick y Bass compiten en 60 Hz"
 └────────┬────────┘
          │ 1200ms typing
          ▼
 ┌─────────────────┐
 │  4. SHOW        │  → 3 tarjetas visuales (Native/Free/Premium)
 │     OPTIONS     │  → QuickReplyBar con las opciones
 │                 │  → "Puedes resolverlo de varias maneras:"
 └────────┬────────┘
          │ 800ms typing → WAIT
          ▼
 ┌─────────────────┐
 │  5. WAIT USER   │  → QuickReplies visibles
 │                 │  → Input habilitado
 │                 │  → Usuario elige opción
 └────────┬────────┘
          │ Usuario confirma cambio
          ▼
 ┌─────────────────┐
 │  6. VERIFY      │  → "Aplicando cambio..."
 │                 │  → "Escuchando... Verificando..."
 │                 │  → "✅ Masking reducido. Δ -2.3 dB"
 └────────┬────────┘
          │ 1500ms escalonado
          ▼
 ┌─────────────────┐
 │  7. CELEBRATE   │  → "🎉 ¡Excelente trabajo!"
 │                 │  → Avatar celebrate + nod
 │                 │  → "⚡ +45 XP · +12% progreso"
 │                 │  → PhaseProgressBar burst animation
 └────────┬────────┘
          │
          ▼
 ┌─────────────────┐
 │     NEXT        │  → "Buscando siguiente problema..."
 │                 │  → Vuelve al paso 1 o avanza de fase
 └─────────────────┘
```

---

## 2. Timing Targets

Cada paso tiene delays intencionales para dar ritmo narrativo.
Los valores están CALIBRADOS para que la secuencia completa de un problema
se sienta fluida (no apresurada, no lenta), en ~15-20 segundos totales.

### Tabla de Timings

```
Paso            Delay typing    Delay post-msg    Duración total
────────────────────────────────────────────────────────────────
1. Detect       —               —                 (implícito)
2. Show          800 ms          300 ms           ~1,100 ms
3. Explain     1,200 ms           —               ~1,200 ms
4. Options       800 ms           —               ~800 ms
5. Wait User      —               —               (indefinido, hasta input)
6. Verify      1,500 ms        1,200+1,800 ms    ~4,500 ms (escalonado)
7. Celebrate   1,000 ms          500 ms           ~1,500 ms
8. Next          300 ms           —               ~300 ms
                ─────────   ─────────────────     ─────────
                ~5,600 ms   ~3,800 ms             ~9,400 ms (sin wait user)
```

### Implementación en C++

```cpp
// En CoachingNarrativeDirector.h
static constexpr int kTimingDetectMs    = 800;   // ~800ms typing antes de evidencia
static constexpr int kTimingEvidenceMs  = 1000;  // ~1000ms delay después de evidencia
static constexpr int kTimingExplainMs   = 1200;  // ~1200ms typing antes de explicación
static constexpr int kTimingOptionsMs   = 800;   // ~800ms antes de waiting
static constexpr int kTimingVerifyMs    = 1500;  // ~1500ms de verificación
static constexpr int kTimingCelebrateMs = 1000;  // ~1000ms de celebración
```

Estos valores se usan en `ChatMessageSequencer::enqueueBatch()`:

```cpp
messageSequencer_.enqueueBatch({
    SequencerStep::typingOn(),
    SequencerStep::delay(kTimingDetectMs),    // 800ms typing
    SequencerStep::system(evidenceMsg),
    SequencerStep::typingOff(),
    SequencerStep::delay(kTimingEvidenceMs),  // 1000ms pausa
    SequencerStep::callback([this]() {
        transitionTo(Step::Explain, 0);
    })
});
```

---

## 3. Diagrama de Estados

### NarrativeStep (enum en CoachingNarrativeDirector.h)

```cpp
enum class Step : uint8_t {
    Idle,            // No active coaching cycle
    ShowEvidence,    // Step 2: Open analyzer + update EvidencePanel
    Explain,         // Step 3: Post chat message with explanation
    ShowOptions,     // Step 4: Show 3-tier QuickReply options
    WaitingForUser,  // Step 5: Await user input (option + confirmation)
    ConfirmApplied,  // Step 6: Confirm the correction was applied
    CelebrateStep,   // Step 7: XP burst + avatar celebration + message
    NextProblemStep, // Step 8: Advance to next problem or phase
    Complete         // All problems resolved
};
```

### Máquina de Estados

```
                startProblem()
                     │
                     ▼
                ┌──────────┐
         ┌──────┤ SHOW     │◄──── (desde motor)
         │      │ EVIDENCE │
         │      └────┬─────┘
         │           │ sequencer callback
         │           ▼
         │      ┌──────────┐
         │      │ EXPLAIN  │
         │      └────┬─────┘
         │           │ sequencer callback
         │           ▼
         │      ┌──────────┐
         │      │ SHOW     │
         │      │ OPTIONS  │
         │      └────┬─────┘
         │           │ QuickReply displayed
         │           ▼
         │      ┌──────────────┐
         │      │ WAITING FOR  │◄─── usuario elige opción
         │      │ USER         │─────► onOptionSelected()
         │      └──────────────┘
         │           │
         │           ▼
         │      ┌────────────────┐
         │      │ CONFIRM        │◄─── usuario confirma cambio
         │      │ APPLIED        │─────► onCorrectionConfirmed()
         │      └───────┬────────┘
         │              │
         │              ▼
         │      ┌──────────────┐
         │      │ CELEBRATE    │
         │      │ STEP         │
         │      └──────┬───────┘
         │             │ onCycleComplete
         │             ▼
         │      ┌──────────────┐
         │      │ NEXT PROBLEM ├─────► Complete → Idle
         │      └──────┬───────┘
         │             │
         └─────────────┘  (vuelve a ShowEvidence con siguiente problema)
```

---

## 4. Estructuras de Datos

### CoachingProblem

```cpp
struct CoachingProblem {
    TrackProblemGroup trackGroup;           // Pistas afectadas
    CoachRoomState phase;                   // Fase de coaching (GainStaging, EQ, etc.)

    // Evidence
    juce::String evidenceAnalyzer;          // "vu", "spectrum", "crest", "vectorscope", "lufs"
    float highlightFreq;                    // Frecuencia a resaltar (Hz)
    juce::String highlightLabel;            // Label del highlight

    // Explain
    juce::String explanationMessage;        // Mensaje de explicación (opcional)

    // Options (3 tiers)
    PluginSuggestionGroup nativeOption;     // Tier 0: Native
    PluginSuggestionGroup freeOption;       // Tier 1: Free
    PluginSuggestionGroup premiumOption;    // Tier 2: Premium

    // Verify
    CorrectionCardData correctionData;      // Datos para verificación

    bool isValid() const noexcept;
};
```

### SequencerStep

```cpp
struct SequencerStep {
    enum class Type : uint8_t {
        TypingOn,        // Muestra typing indicator
        TypingOff,       // Oculta typing indicator
        CoachMessage,    // Mensaje del coach (burbuja izquierda)
        SystemMessage,   // Mensaje del sistema (compacto, dimmed)
        Delay,           // Pausa por delayMs milisegundos
        Callback         // Ejecuta onComplete sin delay
    };

    Type type;
    juce::String text;
    int delayMs;
    std::function<void()> onComplete;

    // Factory methods
    static SequencerStep coach(const juce::String& msg, ...);     // CoachMessage
    static SequencerStep system(const juce::String& msg, ...);    // SystemMessage
    static SequencerStep typingOn(...);                           // TypingOn
    static SequencerStep typingOff(...);                          // TypingOff
    static SequencerStep delay(int ms, ...);                      // Delay
    static SequencerStep callback(std::function<void()> fn);      // Callback
};
```

### AnalyzerMapping (desde ProblemAnalyzerMap)

```cpp
struct AnalyzerMapping {
    ProblemType problemType;     // El tipo de problema
    PanelId panelId;             // Qué panel/tab abrir
    const char* viewId;          // "spectrum", "vectorscope", "meter", "crest", "lufs"
    float highlightFreq;         // Frecuencia a resaltar en Hz (0 = ninguna)
    const char* highlightLabel;  // Label para el highlight contextual
    const char* explanationPhrase;
    const char* emoji;

    bool hasHighlight() const noexcept;
};
```

---

## 5. Ciclo de Vida

### Creación

```cpp
// En NavigationShell constructor
narrativeDirector_ = std::make_unique<CoachingNarrativeDirector>(*this, *coachPanel_);
```

### Inicio de un problema

```cpp
// 1. Motor detecta problema (CoachEngine, MixPriorityEngine)
// 2. NavigationShell construye CoachingProblem
auto problem = CoachingNarrativeDirector::buildFromProblemType(
    detectedProblemType,
    *pluginSuggestionsProvider
);

// 3. Director inicia el loop
narrativeDirector_->startProblem(problem);

// El director:
//   - Activa su timer interno a 60fps
//   - Transiciona a ShowEvidence
//   - ChatMessageSequencer comienza a procesar pasos
```

### Durante el loop

El director NO depende de NavigationShell para avanzar. Cada paso
encola su propio callback en el ChatMessageSequencer, que llama a
`transitionTo(NextStep, 0)` cuando el paso actual termina.

### Fin del ciclo

```cpp
// En doNextProblemStep():
messageSequencer_.enqueueBatch({
    SequencerStep::typingOn(),
    SequencerStep::delay(300),
    SequencerStep::system("🔎 Buscando el siguiente problema..."),
    SequencerStep::typingOff(),
    SequencerStep::callback([this]() {
        if (onCycleComplete)
            onCycleComplete();
        currentStep_ = Step::Complete;
    })
});

// NavigationShell reacciona al callback:
narrativeDirector_->onCycleComplete = [this]() {
    phaseProgressBar_.triggerXpBurst(45);
    // Avanzar al siguiente problema o fase
};
```

---

## 6. Mapeo Problema → Evidencia

### ProblemAnalyzerMap

El `ProblemAnalyzerMap` es una lookup table estática con 12 entradas.
Cada entrada mapea un `ProblemType` a su analyzer view + highlight + label.

| ProblemType | viewId | highlightFreq | highlightLabel | PanelId |
|:------------|:-------|:--------------|:---------------|:--------|
| Gain | meter | — | — | Tools |
| Clipping | meter | — | — | Tools |
| Masking | spectrum | 60.0 Hz | "Masking zone" | Tools |
| TonalExcess | spectrum | 2000.0 Hz | "Exceso" | Tools |
| TonalDeficit | spectrum | 5000.0 Hz | "Deficiencia" | Tools |
| DynamicsOvercompressed | crest | — | — | Tools |
| DynamicsTooDynamic | crest | — | — | Tools |
| Phase | vectorscope | — | — | Tools |
| Spatial | vectorscope | — | — | Tools |
| Reverb | vectorscope | — | — | Tools |
| Saturation | spectrum | — | — | Tools |
| Limiting | lufs | — | — | Tools |

### Flujo de evidencia visual

```
1. Director.doShowEvidence()
2. Consulta ProblemAnalyzerMap::lookup(problemType)
3. Si highlightFreq > 0:
   a. evidencePanel_.highlightedFreq_ = freq
   b. evidencePanel_.highlightLabel_ = label
   c. navShell_.setSpectrumHighlight(freq, label, MixCoachTheme::error())
4. El SpectrographComponent dibuja:
   - Banda glow ROJA pulsante (~1/3 octava)
   - Líneas verticales brillantes en los bordes
   - Label badge centrado (ej: "2.5 kHz — Masking")
   - Diamante en frecuencia central + líneas guía
5. EvidencePanel recibe highlight para su mini-visor
```

---

## 7. Integración con Componentes UI

### Componentes que intervienen en el loop

```
              ┌──────────────────────────┐
              │ CoachingNarrativeDirector │
              └──────┬───────┬───────┬───┘
                     │       │       │
          ┌──────────┘       │       └──────────┐
          ▼                  ▼                  ▼
  ┌──────────────┐  ┌──────────────┐  ┌────────────────┐
  │    Chat      │  │  Evidence    │  │  QuickReply    │
  │  Message     │  │  Host /      │  │  Bar           │
  │  Sequencer   │  │  Panel       │  │                │
  └──────────────┘  └──────────────┘  └────────────────┘
          │                 │                 │
          ▼                 ▼                 ▼
  ┌──────────────┐  ┌──────────────┐  ┌────────────────┐
  │  CoachPanel  │  │  Spectro-    │  │  Navigation    │
  │  (addMessage,│  │  graph       │  │  Shell         │
  │  addSystem,  │  │  Component   │  │  (avatar, toast,│
  │  setTyping)  │  │  (highlight) │  │  XP burst)     │
  └──────────────┘  └──────────────┘  └────────────────┘
```

### Callbacks del Director

| Callback | Se dispara cuando | Qué hace NavigationShell |
|:---------|:-----------------|:-------------------------|
| `onCycleComplete` | El ciclo de un problema termina | `triggerXpBurst(45)`, avanza al siguiente problema |
| `onTierSelected` | Usuario elige un tier | Postea "🔌 Aplicando: [plugin]" |
| `onStartVerification` | Inicia verify loop | Llama a `CoachEngine::requestDiagnosticUpdate()` |

### Estado del Director consultado por NavigationShell

| Método | Retorna | Uso |
|:-------|:--------|:----|
| `isActive()` | true si hay ciclo en progreso | No cambiar de tab si activo |
| `getCurrentStep()` | Step actual | Para debug y tests |
| `getCurrentProblem()` | Problema actual | Para acceso a datos de verify |

---

## 8. Ejemplo Completo

### Problema: Masking entre Kick y Bass a 60 Hz

```
1. MixPriorityEngine detecta masking en frecuencias bajas
2. NavigationShell construye CoachingProblem:
   problemType = ProblemType::Masking
   phase = CoachRoomState::EQ
   evidenceAnalyzer = "spectrum"
   highlightFreq = 60.0 Hz
   highlightLabel = "Masking zone"
   nativeOption = [Fruity Parametric EQ 2, 60Hz, -3dB, Q 2.0]
   freeOption = [TDR Nova, Dynamic Bell, 60Hz]
   premiumOption = [FabFilter Pro-Q 4, Dynamic Band, 60Hz]

3. Director.startProblem(problem):
   → timer 60fps activado
   → transitionTo(ShowEvidence, 0)

4. doShowEvidence():
   → ProblemAnalyzerMap::lookup(Masking) → spectrum + 60Hz
   → navShell_.setSpectrumHighlight(60.0f, "Masking zone", error())
   → evidencePanel_.setCoachRoomState(EQ)
   → Sequencer:
        [typingOn] → [delay 800ms] → [system "🔊 Evidencia: En Kick — masking"]
        → [typingOff] → [delay 1000ms] → [callback → Explain]

5. doExplain():
   → Sequencer:
        [typingOn] → [delay 1200ms] → [coach "El Kick y Bass compiten en 60 Hz..."]
        → [typingOff] → [avatar nod 600ms] → [callback → ShowOptions]

6. doShowOptions():
   → addPluginSuggestionCard(combined_options)
   → showQuickReplies(["🎛 Nativo", "🟢 Gratis", "⭐ Profesional"])
   → currentStep = WaitingForUser

7. Usuario clicka "🎛 Nativo"
   → onOptionSelected("🎛 Nativo")
   → onTierSelected(0, nativeOption)
   → hideQuickReplies()
   → addMessage("✅ Has elegido Nativo: Masking. Aplica el cambio...")
   → transitionTo(ConfirmApplied, 0)

8. doConfirm():
   → Sequencer: [typingOn] → [system "Esperando confirmación..."]
   → Avatar Serious

9. Usuario escribe "Listo" / O confirma via CorrectionLearner
   → onCorrectionConfirmed()
   → addMessage("🔄 Verificando...")
   → onStartVerification(correctionData)
   → Sequencer: [verify typing + delay] → [callback → Celebrate]

10. doCelebrateStep():
    → Avatar Celebrating
    → postUIEvent("✨", "¡Corrección aplicada!")
    → Sequencer:
        [typingOn] → [delay 1000ms] → [coach "🎉 ¡Excelente trabajo!"]
        → [typingOff] → [delay 500ms] → [system "⚡ +45 XP · +12% progreso"]
        → [callback → NextProblem]

11. doNextProblemStep():
    → onCycleComplete → phaseProgressBar_.triggerXpBurst(45)
    → currentStep = Complete

12. NavigationShell busca siguiente problema → vuelve al paso 1
```

---

## 9. Referencia de API

### CoachingNarrativeDirector

```cpp
// === API PÚBLICA ===

// Inicia el ciclo narrativo con un problema detectado
void startProblem(const CoachingProblem& problem);

// El usuario seleccionó una opción (plugin tier o texto)
void onOptionSelected(const juce::String& option);

// El usuario confirmó que aplicó el cambio
void onCorrectionConfirmed();

// Resetea el director a Idle
void reset();

// Retorna el paso actual
Step getCurrentStep() const noexcept;

// Retorna true si hay un ciclo narrativo activo
bool isActive() const noexcept;

// Construye un CoachingProblem desde un ProblemType + sugerencias
static CoachingProblem buildFromProblemType(
    ProblemType detectedProblem,
    const PluginSuggestionsProvider& provider);

// Retorna el problema actual
const CoachingProblem& getCurrentProblem() const noexcept;

// === CALLBACKS ===
std::function<void()> onCycleComplete;
std::function<void(int tierIndex, const PluginSuggestionGroup& option)> onTierSelected;
std::function<void(const CorrectionCardData& data)> onStartVerification;
```

### ChatMessageSequencer

```cpp
// === API PÚBLICA ===

// Encola un solo paso
void enqueue(SequencerStep step);

// Encola un lote de pasos (se ejecutan en orden secuencial)
void enqueueBatch(std::vector<SequencerStep> steps);

// Cancela la secuencia actual
void cancel();

// Retorna true si hay una secuencia activa
bool isBusy() const noexcept;

// Callback cuando toda la secuencia termina
std::function<void()> onSequenceComplete;
```

### ProblemAnalyzerMap

```cpp
// === API PÚBLICA (estática) ===

// Retorna el mapeo completo para un ProblemType
static AnalyzerMapping lookup(ProblemType type) noexcept;

// Retorna el viewId como string
static const char* lookupViewId(ProblemType type) noexcept;

// Retorna el panel a abrir
static PanelId lookupPanel(ProblemType type) noexcept;

// Construye un mensaje de evidencia para el chat
static juce::String buildEvidenceMessage(
    ProblemType type,
    const juce::String& trackName,
    float highlightHz = 0.0f);
```

---

*Documento de coaching narrativo — MixCoach v2.0 — 19 julio 2026*
*Este documento define el corazón de la experiencia de MixCoach:*
*el loop narrativo que transforma cada corrección en una experiencia de aprendizaje.*

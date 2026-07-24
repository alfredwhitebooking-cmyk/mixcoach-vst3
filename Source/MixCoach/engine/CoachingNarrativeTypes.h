#pragma once
#include <juce_core/juce_core.h>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  NarrativeStep — Estado del ciclo narrativo de coaching
    //
    //  Cada paso representa una etapa en el loop de 7 pasos:
    //    DETECT (implícito) → SHOW EVIDENCE → EXPLAIN → SHOW OPTIONS
    //    → WAITING FOR USER → CONFIRM APPLIED → CELEBRATE → NEXT PROBLEM
    // ═══════════════════════════════════════════════════════════════════════════
    enum class NarrativeStep : uint8_t
    {
        Idle,            // No active coaching cycle
        ShowEvidence,    // Step 2: Open analyzer + update EvidencePanel
        Explain,         // Step 3: Post chat message explaining the problem
        ShowOptions,     // Step 4: Show 3-tier QuickReply options (Native/Free/Premium)
        WaitingForUser,  // Step 5: Await user input (option selection + confirmation)
        ConfirmApplied,  // Step 6: Confirm the correction was applied
        CelebrateStep,   // Step 7: XP burst + avatar celebration
        NextProblemStep, // Step 8: Advance to next problem or phase
        Complete         // All problems resolved — cycle ends
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidenceView — Tipo de vista de evidencia contextual
    //
    //  Determina qué visualización se muestra en el panel derecho
    //  (CoachingEvidenceHost) durante cada fase de coaching.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class EvidenceView : uint8_t
    {
        VU,           // Gain Staging — VU meter + peak bars
        Spectrum,     // EQ / Masking — frequency spectrum with highlight
        Crest,        // Compression — crest factor gauge
        Vectorscope,  // Space / Phase — vectorscope + correlation meter
        LUFS,         // Master Check — loudness meter + match score
        DNA,          // Audio DNA — sonic fingerprint visualization
        StereoWidth,  // Stereo Width — width meter
        None          // No evidence view
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidenceConfig — Configuración completa de evidencia para un problema
    //
    //  Mapea un tipo de problema a:
    //    - Qué vista de analyzer abrir (VU, Spectrum, Crest, etc.)
    //    - Título del panel contextual
    //    - Mensaje del sistema al abrir la evidencia
    //    - Frecuencia a resaltar (para Spectrum)
    //    - Slot a resaltar (para MixMap)
    // ═══════════════════════════════════════════════════════════════════════════
    struct EvidenceConfig
    {
        EvidenceView view;              // VU, Spectrum, Crest, Vectorscope, LUFS
        juce::String panelTitle;        // Título del panel ("Gain Staging", "EQ", etc.)
        juce::String systemMessage;     // Mensaje al abrir ("🔍 Abriendo medidores...")
        float highlightFreqHz = 0.0f;   // Frecuencia a resaltar (para EQ)
        int highlightSlot = -1;         // Slot a resaltar (para MixMap)

        bool hasHighlight() const noexcept { return highlightFreqHz > 0.0f || highlightSlot >= 0; }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Timing targets — Delays del prototipo HTML (usados por ChatMessageSequencer)
    //
    //  Replican el ritmo narrativo del prototipo HTML para que la secuencia
    //  de mensajes se sienta natural y guiada.
    // ═══════════════════════════════════════════════════════════════════════════
    namespace NarrativeTiming
    {
        inline constexpr int kDetectMs = 800;      // ~800ms typing antes de evidencia
        inline constexpr int kEvidenceMs = 1000;   // ~1000ms delay después de evidencia
        inline constexpr int kExplainMs = 1200;    // ~1200ms typing antes de explicación
        inline constexpr int kOptionsMs = 800;     // ~800ms antes de mostrar opciones
        inline constexpr int kVerifyMs = 1500;     // ~1500ms de verificación
        inline constexpr int kCelebrateMs = 1000;  // ~1000ms de celebración

        // Phase transition delays (FASE 8 — refinamiento de timing)
        // 600ms entre fases de coaching (Gain→Balance→EQ→Compression→Space→Automation→MasterCheck)
        // replicando el ease-out del prototipo HTML.
        inline constexpr int kAdvancePhaseMs = 600;
        // 300ms fade-in del nuevo panel al cambiar de fase
        inline constexpr int kPhaseFadeMs = 300;
        // Auto-advance delays for setup flow (FASE 6)
        inline constexpr int kSetupAutoAdvanceMs = 400; // 400ms entre pasos de onboarding
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  EngineeringIntervention — Intervención de ingeniería en 5 pasos
    //
    //  Cada respuesta del Coach sigue la estructura de un ingeniero senior:
    //    1. 👁️ Observation  → "Detecté X en la pista Y"
    //    2. 🔍 Probable Cause → "Esto ocurre porque Z"
    //    3. 🛠️ Exact Action   → "Ajusta el control W a valor V"
    //    4. 👂 How to Listen  → "Escucha cómo cambia la relación..."
    //    5. ✅ Criterion      → "Cuando escuches X, pasa al siguiente paso"
    //
    //  El método buildEngineeringMessage() en CoachingNarrativeDirector
    //  formatea estos 5 campos en un mensaje coherente para el chat.
    // ═══════════════════════════════════════════════════════════════════════════
    struct EngineeringIntervention
    {
        juce::String observation;    // 👁️ Lo que el coach escuchó/vió
        juce::String probableCause;  // 🔍 Por qué ocurre
        juce::String exactAction;    // 🛠️ Qué hacer exactamente
        juce::String howToListen;    // 👂 Qué escuchar para confirmar
        juce::String criterion;      // ✅ Criterio para avanzar
        // Contexto musical opcional: "durante la reproducci\xC3\xB3n a 0:45 (128 BPM)"
        // Se incluye al inicio de todos los mensajes del EngineeringIntervention.
        juce::String transportContext;

        bool isValid() const noexcept
        {
            return observation.isNotEmpty();
        }

        /** Construye el mensaje completo con formato markdown.
            Retorna el texto listo para postear en el chat.
            Si hay transportContext, se incluye al inicio del mensaje. */
        [[nodiscard]] juce::String format() const
        {
            juce::String msg;

            // Incluir contexto temporal al inicio si est\xC3\xA1 disponible
            if (transportContext.isNotEmpty())
                msg << "\xF0\x9F\x95\x90 " << transportContext << "\n\n";

            if (observation.isNotEmpty())
                msg << "\xF0\x9F\x91\x81 **Observaci\xC3\xB3n:** " << observation << "\n\n";

            if (probableCause.isNotEmpty())
                msg << "[SEARCH] **Causa probable:** " << probableCause << "\n\n";

            if (exactAction.isNotEmpty())
                msg << "[TOOLS] **Acci\xC3\xB3n exacta:** " << exactAction << "\n\n";

            if (howToListen.isNotEmpty())
                msg << "\xF0\x9F\x91\x82 **Forma de escuchar:** " << howToListen << "\n\n";

            if (criterion.isNotEmpty())
                msg << "[DONE] **Criterio de avance:** " << criterion;

            return msg;
        }

        /** Versión compacta para el paso Explain (solo observación + causa).
            Incluye transportContext al inicio si est\xC3\xA1 disponible. */
        [[nodiscard]] juce::String formatCompact() const
        {
            juce::String msg;

            // Incluir contexto temporal al inicio si est\xC3\xA1 disponible
            if (transportContext.isNotEmpty())
                msg << "\xF0\x9F\x95\x90 " << transportContext << "\n\n";

            if (observation.isNotEmpty())
                msg << "\xF0\x9F\x91\x81 " << observation << "\n\n";

            if (probableCause.isNotEmpty())
                msg << "[SEARCH] " << probableCause;

            return msg;
        }

        /** Versión de acción para el paso ShowOptions (solo acción + escucha).
            Incluye transportContext al inicio si est\xC3\xA1 disponible. */
        [[nodiscard]] juce::String formatAction() const
        {
            juce::String msg;

            // Incluir contexto temporal al inicio si est\xC3\xA1 disponible
            if (transportContext.isNotEmpty())
                msg << "\xF0\x9F\x95\x90 " << transportContext << "\n\n";

            if (exactAction.isNotEmpty())
                msg << "[TOOLS] " << exactAction << "\n\n";

            if (howToListen.isNotEmpty())
                msg << "\xF0\x9F\x91\x82 " << howToListen << "\n\n";

            if (criterion.isNotEmpty())
                msg << "[DONE] " << criterion;

            return msg;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  TransportContext — Contexto musical del DAW para mensajes del coach
    //
    //  Formatea la posición actual del transporte del DAW para incluirla
    //  en los mensajes del coach, dando contexto musical como:
    //    "a las 0:45, durante el coro..."
    //    "a 128 BPM en la sección..."
    // ═══════════════════════════════════════════════════════════════════════════
    struct TransportContext
    {
        bool valid = false;           // true si el DAW reportó datos válidos
        bool isPlaying = false;       // true si el DAW está reproduciendo
        bool isLooping = false;       // ═══ V2b: true si el DAW está en modo loop ═══
        double bpm = 120.0;           // Beats per minute actuales
        double timeInSeconds = 0.0;   // Posición en segundos
        double ppqPositionOfLastBarStart = 0.0; // ═══ V2b: PPQ del último compás iniciado ═══
        int timeSigNumerator = 4;     // Compás: numerador (ej: 4)
        int timeSigDenominator = 4;   // Compás: denominador (ej: 4)

        /** Construye un TransportContext desde un TransportInfo del processor.
            Requiere incluir PluginProcessor.h o pasar los campos directamente.
            @param isPlaying   true si el DAW está reproduciendo
            @param isLooping   true si el DAW está en modo loop
            @param bpm         Beats per minute (default 120)
            @param timeSec     Posición actual en segundos (default 0)
            @param ppqLastBar  PPQ position of last bar start (default 0)
            @param tsNum       Time signature numerator (default 4)
            @param tsDen       Time signature denominator (default 4)
            @param valid       true si los datos provienen del DAW real */
        static TransportContext fromFields(bool isValid, bool playing, bool looping,
                                            double bpmVal,
                                            double timeSec, double ppqLastBar,
                                            int tsNum, int tsDen) noexcept
        {
            TransportContext tc;
            tc.valid = isValid;
            tc.isPlaying = playing;
            tc.isLooping = looping;
            tc.bpm = (bpmVal > 0.0) ? bpmVal : 120.0;
            tc.timeInSeconds = timeSec;
            tc.ppqPositionOfLastBarStart = (ppqLastBar >= 0.0) ? ppqLastBar : 0.0;
            tc.timeSigNumerator = (tsNum > 0) ? tsNum : 4;
            tc.timeSigDenominator = (tsDen > 0) ? tsDen : 4;
            return tc;
        }

        /** Retorna el tiempo formateado como "MM:SS". */
        [[nodiscard]] juce::String formatTime() const
        {
            if (!valid || timeInSeconds < 0.0) return {};
            int totalSec = static_cast<int>(timeInSeconds);
            int mins = totalSec / 60;
            int secs = totalSec % 60;
            return juce::String(mins) + ":" + juce::String(secs).paddedLeft('0', 2);
        }

        /** Retorna un string de contexto temporal listo para insertar en mensajes.
            Ejemplos:
              " (a las 0:45, 128 BPM)"
              " (reproduciendo, 128 BPM)"
              "" (si transport no es válido o no hay reproducción)
            @return String con contexto temporal, o vacío si no aplica. */
        [[nodiscard]] juce::String formatContext() const
        {
            if (!valid) return {};

            juce::String ctx;

            // Incluir BPM
            int bpmInt = juce::roundToInt(bpm);
            ctx << " (a " << bpmInt << " BPM";

            // Incluir posición si está reproduciendo
            if (isPlaying && timeInSeconds > 0.0) {
                ctx << " \xE2\x80\x94 " << formatTime();
            }

            // Incluir indicador de loop si está activo
            if (isLooping) {
                ctx << ", en loop";
            }

            ctx << ")";
            return ctx;
        }

        /** Versión más descriptiva: "durante la reproducción a 0:45 (128 BPM, loop)".
            Se usa al inicio de mensajes del coach para dar contexto musical. */
        [[nodiscard]] juce::String formatDescription() const
        {
            if (!valid) return {};

            juce::String desc;
            int bpmInt = juce::roundToInt(bpm);

            if (isPlaying && timeInSeconds > 0.0) {
                desc << " durante la reproducci\xC3\xB3n a " << formatTime();
                if (bpmInt >= 40 && bpmInt <= 300) {
                    desc << " (" << bpmInt << " BPM";
                    if (isLooping) desc << ", en loop";
                    desc << ")";
                }
                else if (isLooping) {
                    desc << " (en loop)";
                }
            }
            else if (isPlaying) {
                desc << " en reproducci\xC3\xB3n";
                if (bpmInt >= 40 && bpmInt <= 300) {
                    desc << " a " << bpmInt << " BPM";
                    if (isLooping) desc << " (loop)";
                }
                else if (isLooping) {
                    desc << " (loop)";
                }
            }
            else if (bpmInt >= 40 && bpmInt <= 300) {
                desc << " (proyecto a " << bpmInt << " BPM";
                if (isLooping) desc << ", en loop";
                desc << ")";
            }
            else if (isLooping) {
                desc << " (modo loop activo)";
            }

            return desc;
        }
    };

} // namespace mixcoach

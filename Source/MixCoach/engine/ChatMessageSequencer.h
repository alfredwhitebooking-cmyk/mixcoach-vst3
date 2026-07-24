#pragma once
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace mixcoach {

    // Forward declarations
    class MixCoachPanel;

    // ═══════════════════════════════════════════════════════════════════════════
    //  SequencerStep — Un paso individual en la secuencia narrativa.
    //
    //  Cada paso define un tipo de acción, un texto opcional, un delay en ms
    //  y un callback opcional que se ejecuta al completar el paso.
    // ═══════════════════════════════════════════════════════════════════════════
    struct SequencerStep
    {
        enum class Type : uint8_t
        {
            TypingOn,       // Muestra el indicador de escritura
            TypingOff,      // Oculta el indicador de escritura
            CoachMessage,   // Mensaje del coach (burbuja izquierda)
            SystemMessage,  // Mensaje del sistema (compacto, dimmed)
            Delay,          // Pausa por delayMs milisegundos
            Callback        // Ejecuta onComplete sin delay
        };

        Type type = Type::Delay;
        juce::String text;
        juce::String tag;          // Opcional: tag para coach messages
        int delayMs = 0;           // Delay en milisegundos (para Type::Delay)
        std::function<void()> onComplete; // Callback al finalizar este paso

        /** Crea un paso de mensaje del coach. */
        static SequencerStep coach(const juce::String& msg, const juce::String& tag = {},
                                    int postDelayMs = 0,
                                    std::function<void()> onDone = nullptr)
        {
            return { Type::CoachMessage, msg, tag, postDelayMs, std::move(onDone) };
        }

        /** Crea un paso de mensaje del sistema. */
        static SequencerStep system(const juce::String& msg, int postDelayMs = 0,
                                     std::function<void()> onDone = nullptr)
        {
            return { Type::SystemMessage, msg, {}, postDelayMs, std::move(onDone) };
        }

        /** Crea un paso de typing indicator on. */
        static SequencerStep typingOn(std::function<void()> onDone = nullptr)
        {
            return { Type::TypingOn, {}, {}, 0, std::move(onDone) };
        }

        /** Crea un paso de typing indicator off. */
        static SequencerStep typingOff(std::function<void()> onDone = nullptr)
        {
            return { Type::TypingOff, {}, {}, 0, std::move(onDone) };
        }

        /** Crea un paso de delay puro. */
        static SequencerStep delay(int ms, std::function<void()> onDone = nullptr)
        {
            return { Type::Delay, {}, {}, ms, std::move(onDone) };
        }

        /** Crea un paso de callback. */
        static SequencerStep callback(std::function<void()> fn)
        {
            return { Type::Callback, {}, {}, 0, std::move(fn) };
        }

        /** Crea un paso de eco del usuario (se muestra como mensaje del coach con tag). */
        static SequencerStep userEcho(const juce::String& text,
                                      std::function<void()> onDone = nullptr)
        {
            return { Type::CoachMessage, text, "USR", 0, std::move(onDone) };
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ChatMessageSequencer — Cola de mensajes con timing controlado.
    //
    //  Procesa una cola de SequencerStep en orden, respetando delays y
    //  mostrando el typing indicator en los momentos apropiados.
    //
    //  Uso típico:
    //    sequencer.enqueueBatch({
    //        SequencerStep::typingOn(),
    //        SequencerStep::delay(800),
    //        SequencerStep::coach("He encontrado un problema..."),
    //        SequencerStep::typingOff(),
    //        SequencerStep::system("-> Abriendo medidores..."),
    //        SequencerStep::callback([this]{ onEvidenceShown(); })
    //    });
    //
    //  Timing targets (del prototipo HTML):
    //    Typing on → Delay 800ms → Coach msg → Delay 300ms → System msg
    //    → Delay 500ms → Options → Delay 800ms → Verify → Delay 1500ms → Celebrate
    // ═══════════════════════════════════════════════════════════════════════════
    class ChatMessageSequencer : private juce::Timer
    {
    public:
        explicit ChatMessageSequencer(MixCoachPanel& coachPanel);
        ~ChatMessageSequencer() override = default;

        // ─── API pública ─────────────────────────────────────────────────

        /** Encola un solo paso. */
        void enqueue(SequencerStep step);

        /** Encola un lote de pasos (se ejecutan en orden secuencial). */
        void enqueueBatch(std::vector<SequencerStep> steps);

        /** Cancela la secuencia actual (limpia la cola y detiene el timer). */
        void cancel();

        /** Retorna true si hay una secuencia activa. */
        bool isBusy() const noexcept { return sequencerBusy_; }

        /** Callback cuando toda la secuencia termina. */
        std::function<void()> onSequenceComplete;

    private:
        void timerCallback() override;

        /** Procesa el siguiente paso en la cola. */
        void processNext();

        /** Ejecuta el paso actual (sin timer involvement). */
        void executeStep(const SequencerStep& step);

        // ─── Referencia al panel de chat ────────────────────────────────
        MixCoachPanel& coachPanel_;

        // ─── Cola de pasos ──────────────────────────────────────────────
        std::vector<SequencerStep> pendingSteps_;
        bool sequencerBusy_ = false;
        int remainingDelayMs_ = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChatMessageSequencer)
    };

} // namespace mixcoach

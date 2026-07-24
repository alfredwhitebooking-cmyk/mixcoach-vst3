#include "ChatMessageSequencer.h"
#include "../UI/CoachChatComponent.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    ChatMessageSequencer::ChatMessageSequencer(MixCoachPanel& coachPanel)
        : coachPanel_(coachPanel)
    {
        // Timer starts only when there's work to do
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  enqueue — Añade un paso a la cola
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessageSequencer::enqueue(SequencerStep step)
    {
        pendingSteps_.push_back(std::move(step));

        if (!sequencerBusy_) {
            sequencerBusy_ = true;
            startTimerHz(60); // ~16.6ms per tick
            processNext();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  enqueueBatch — Añade múltiples pasos a la cola
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessageSequencer::enqueueBatch(std::vector<SequencerStep> steps)
    {
        for (auto& step : steps)
            pendingSteps_.push_back(std::move(step));

        if (!sequencerBusy_) {
            sequencerBusy_ = true;
            startTimerHz(60);
            processNext();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  cancel — Limpia la cola y detiene el timer
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessageSequencer::cancel()
    {
        pendingSteps_.clear();
        sequencerBusy_ = false;
        remainingDelayMs_ = 0;
        stopTimer();

        // Asegurar que el typing indicator se apaga
        coachPanel_.setAiTyping(false);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  processNext — Toma el siguiente paso de la cola y lo ejecuta
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessageSequencer::processNext()
    {
        if (pendingSteps_.empty()) {
            // Secuencia completa
            sequencerBusy_ = false;
            remainingDelayMs_ = 0;
            stopTimer();

            coachPanel_.setAiTyping(false);

            if (onSequenceComplete)
                onSequenceComplete();

            return;
        }

        // Tomar el siguiente paso
        auto step = std::move(pendingSteps_.front());
        pendingSteps_.erase(pendingSteps_.begin());

        // Si es un paso de delay con tiempo, configurar el timer de espera
        if (step.type == SequencerStep::Type::Delay && step.delayMs > 0) {
            remainingDelayMs_ = step.delayMs;
            // El timerCallback() se encargará de contar los ms
            return;
        }

        // Si es un callback, ejecutar inmediatamente y pasar al siguiente
        if (step.type == SequencerStep::Type::Callback) {
            if (step.onComplete)
                step.onComplete();
            processNext(); // Continuar inmediatamente
            return;
        }

        // Ejecutar el paso
        executeStep(step);

        // Si el paso no tiene delay, pasar al siguiente inmediatamente
        // Si tiene delay (postDelayMs), el timerCallback lo manejará
        if (step.delayMs <= 0) {
            processNext();
        } else {
            remainingDelayMs_ = step.delayMs;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  executeStep — Ejecuta la acción del paso actual
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessageSequencer::executeStep(const SequencerStep& step)
    {
        switch (step.type) {
            case SequencerStep::Type::TypingOn:
                coachPanel_.setAiTyping(true);
                break;

            case SequencerStep::Type::TypingOff:
                coachPanel_.setAiTyping(false);
                break;

            case SequencerStep::Type::CoachMessage:
                coachPanel_.setAiTyping(false);
                coachPanel_.addMessage(step.text, step.tag);
                break;

            case SequencerStep::Type::SystemMessage:
                coachPanel_.setAiTyping(false);
                coachPanel_.addSystemMessage(step.text);
                break;

            // UserEcho removed — use CoachMessage with tag "USR" instead

            default:
                break;
        }

        // Ejecutar callback de finalización del paso (si existe)
        if (step.onComplete)
            step.onComplete();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Gestiona delays entre pasos
    // ═══════════════════════════════════════════════════════════════════════════
    void ChatMessageSequencer::timerCallback()
    {
        if (!sequencerBusy_ || pendingSteps_.empty()) {
            if (pendingSteps_.empty() && !sequencerBusy_) {
                stopTimer();
            }
            return;
        }

        // Si estamos esperando un delay, contar hacia atrás
        if (remainingDelayMs_ > 0) {
            // ~16.6ms por tick a 60fps
            remainingDelayMs_ -= 16;
            if (remainingDelayMs_ <= 0) {
                remainingDelayMs_ = 0;

                // El paso actual (Delay) ya fue consumido de la cola
                // Avanzar al siguiente paso
                processNext();
            }
            return;
        }

        // No hay delay activo — procesar siguiente paso
        processNext();
    }

} // namespace mixcoach

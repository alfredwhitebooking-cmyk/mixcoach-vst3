#include "CoachEngine.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MANEJO DE MENSAJES DEL USUARIO
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::handleUserMessage(const juce::String& message)
    {
        auto lower = message.toLowerCase();

        // Comandos de sistema
        if (lower.startsWith("/")) {
            executeCommand(message);
            return;
        }

        // ═══ RUTEO AL LLM (lenguaje natural) ═══════════════════════════════
        if (llmEnabled_ && setupStep_ == SetupStep::Complete && llmStreamingCallback_) {
            bool accepted = llmStreamingCallback_(
                message,
                [this](const juce::String& token) {
                    if (streamTokenCb_) streamTokenCb_(token);
                },
                [this](const juce::String& response) {
                    if (streamEndedCb_) streamEndedCb_();
                    LogHelper::writeToLog("[CoachEngine] LLM streaming completado (" + juce::String(response.length())
                                          + " chars)");
                });

            if (accepted) {
                if (streamStartedCb_) streamStartedCb_();
                setUserInteracted();
                return;
            }
        }

        // ═══ Fallback: LLM sin streaming ═════
        if (llmEnabled_ && setupStep_ == SetupStep::Complete && llmResponseCallback_) {
            bool accepted =
                llmResponseCallback_(message, [this](const juce::String& response) { respondWithLLM(response); });

            if (accepted) {
                setUserInteracted();
                return;
            }
        }

        // ─── FALLBACK: Respuesta contextual según la fase ─────
        auto phase        = phaseManager_.getCurrentPhase();
        auto& registry    = sharedData_.getSlotRegistry();
        bool hasTelemetry = (registry.activeCount() > 0);

        switch (phase) {
            case MentorPhase::GainStaging:
                if (hasTelemetry) {
                    analyzeGainStagingReal();
                    respondWith(
                        "\xF0\x9F\x93\x8A Puedes preguntar \"c\xC3\xB3mo est\xC3\xA1n los niveles\" o "
                        "\"\xC2\xBFhay clipping?\" para un an\xC3\xA1lisis detallado.",
                        MentorMessage::Type::Info);
                }
                else {
                    respondWith(
                        "A\xC3\xBAn no detecto pistas activas. Aseg\xC3\xBArate de tener "
                        "Messengers cargados en tus pistas.",
                        MentorMessage::Type::Info);
                }
                break;

            case MentorPhase::Organizacion:
                if (hasTelemetry) {
                    analyzeOrganisationReal();
                }
                else {
                    respondWith(
                        "En fase de Organizaci\xC3\xB3n. Recomiendo:\n"
                        "1. Nombra cada pista descriptivamente\n"
                        "2. Asigna colores por familia\n"
                        "3. Agrupa en buses virtuales",
                        MentorMessage::Type::Tip);
                }
                break;

            case MentorPhase::Balance:
                if (hasTelemetry) {
                    analyzeTonalBalanceReal();
                }
                else {
                    respondWith(
                        "En fase de Balance Tonal. Revisa el espectro y "
                        "comp\xC3\xA1ralo con referencias de tu g\xC3\xA9nero.",
                        MentorMessage::Type::Info);
                }
                break;

            case MentorPhase::Compresion:
                if (hasTelemetry) {
                    analyzeDynamicsReal();
                }
                else {
                    respondWith(
                        "En fase de Din\xC3\xA1mica. Considera compresores en buses "
                        "y limitador en el master (solo 1-2 dB).",
                        MentorMessage::Type::Tip);
                }
                break;

            case MentorPhase::Espacio:
                if (hasTelemetry) {
                    analyzePhaseReal();
                }
                respondWith(
                    "En fase de Espacialidad. Trabaja panoramas, reverbs, "
                    "y efectos de profundidad.",
                    MentorMessage::Type::Tip);
                break;
        }

        // Verificar logros
        auto activeCount = registry.activeCount();
        if (activeCount >= 1) phaseManager_.unlockAchievement(Achievement::FirstTrack);
        if (activeCount >= 5) phaseManager_.unlockAchievement(Achievement::FiveTracks);
        if (activeCount >= 10) phaseManager_.unlockAchievement(Achievement::TenTracks);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TIP PROACTIVO
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::generateProactiveTip()
    {
        auto& registry = sharedData_.getSlotRegistry();
        int active     = registry.activeCount();

        if (active == 0) {
            respondWith(
                "\xF0\x9F\x92\xA1 A\xC3\xBAn no hay pistas activas. Carga Messengers en tus pistas "
                "para empezar a recibir an\xC3\xA1lisis en tiempo real.",
                MentorMessage::Type::Tip);
            return;
        }

        auto phase = phaseManager_.getCurrentPhase();
        switch (phase) {
            case MentorPhase::GainStaging:
                analyzeGainStagingReal();
                if (juce::Time::getMillisecondCounter() * 1000 - lastPeakWarningUs_ > kWarningCooldownUs) {
                    respondWith(
                        "\xF0\x9F\x92\xA1 Tip r\xC3\xA1pido: revisa que el fader de ganancia de cada pista "
                        "permita picos de -18 dB a -12 dB en el submix antes de "
                        "tocar el fader de volumen.",
                        MentorMessage::Type::Tip);
                }
                break;

            case MentorPhase::Organizacion:
                analyzeOrganisationReal();
                break;

            case MentorPhase::Balance:
                respondWith(
                    "\xF0\x9F\x92\xA1 Para evaluar el balance tonal, revisa el Analyzer "
                    "(pesta\xC3\xB1"
                    "a 2). Busca una curva suave de menos de 3 dB/octava "
                    "de diferencia entre bandas adyacentes.",
                    MentorMessage::Type::Tip);
                analyzeTonalBalanceReal();
                break;

            case MentorPhase::Compresion:
                analyzeDynamicsReal();
                break;

            case MentorPhase::Espacio:
                analyzePhaseReal();
                break;

            default:
                respondWith(
                    "\xF0\x9F\x92\xA1 Escribe /help para ver comandos disponibles o preg\xC3\xBAntame "
                    "\"\xC2\xBF"
                    "c\xC3\xB3mo va la mezcla?\" para un an\xC3\xA1lisis completo.",
                    MentorMessage::Type::Tip);
                break;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PROGRESO Y LOGROS
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::checkProgress()
    {
        auto phase     = phaseManager_.getCurrentPhase();
        auto progress  = phaseManager_.getPhaseProgress(phase);
        auto& registry = sharedData_.getSlotRegistry();

        respondWith(juce::String("\xF0\x9F\x93\x88 **Progreso** en fase '") + phaseManager_.phaseDescription(phase)
                        + "': " + juce::String(static_cast<int>(progress * 100.0f)) + "%\n"
                        + "Pistas activas: " + juce::String(registry.activeCount()) + "\n"
                        + "Logros: " + juce::String(phaseManager_.getAchievementCount()),
                    MentorMessage::Type::Info);

        if (phaseManager_.isPhaseComplete(phase)) {
            respondWith(
                "\xF0\x9F\x8E\x89 \xC2\xA1Has completado esta fase! Escribe **/next** para avanzar "
                "a la siguiente.",
                MentorMessage::Type::Achievement);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  ANUNCIO DE NUEVA PISTA
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::announceNewTrack(int slotIndex, const juce::String& trackName, const juce::Colour& colour)
    {
        juce::ignoreUnused(slotIndex, colour);

        auto& registry = sharedData_.getSlotRegistry();
        auto telem     = getLatestTelemetry(slotIndex);

        juce::String msg = "\xF0\x9F\x8E\x9B\xEF\xB8\x8F **Nuevo Messenger detectado:** " + trackName.trim() + "\n\n";

        if (telem.timestamp == 0) {
            msg +=
                "Esperando datos de telemetr\xC3\xAD"
                "a... (aparecer\xC3\xA1n en unos segundos)\n";
        }
        else {
            msg += "Niveles actuales:\n";
            msg += "\xE2\x80\xA2 Peak: **" + juce::String(telem.peakLeft, 1) + " dB**\n";
            msg += "\xE2\x80\xA2 RMS: **" + juce::String(telem.rmsLeft, 1) + " dB**\n";
        }

        if (telem.crestFactor > 0.0f)
            msg += "\xE2\x80\xA2 Crest factor: **" + juce::String(telem.crestFactor, 1) + " dB**\n";

        if (telem.correlation < 1.0f)
            msg += "\xE2\x80\xA2 Correlaci\xC3\xB3n: **" + juce::String(telem.correlation, 2) + "**\n";

        if (registry.activeCount() >= 3) {
            msg += "\n\xF0\x9F\x93\x8A Ya tienes **" + juce::String(registry.activeCount())
                   + " pistas** activas \xE2\x80\x94 la mezcla empieza a tomar forma.";
        }

        respondWith(msg, MentorMessage::Type::Info);

        if (phaseManager_.getCurrentPhase() == MentorPhase::Organizacion) {
            respondWith(
                "\xF0\x9F\x9A\x80 \xC2\xA1"
                "Excelente! Como ya tienes pistas, pasamos directo a "
                "**Gain Staging** para ajustar niveles.",
                MentorMessage::Type::Tip);
            phaseManager_.advanceToNextPhase();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  respondWithLLM — Responde con mensaje premium (burbuja completa)
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::respondWithLLM(const juce::String& text)
    {
        respondWithPremium(text, MentorMessage::Type::Info);
    }

} // namespace mixcoach

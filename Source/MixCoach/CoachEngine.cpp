#include "CoachEngine.h"

namespace mixcoach {

CoachEngine::CoachEngine(PhaseManager& phaseManager, SharedData& sharedData)
    : phaseManager_(phaseManager)
    , sharedData_(sharedData)
{
}

void CoachEngine::handleUserMessage(const juce::String& message)
{
    auto lower = message.toLowerCase();

    // Comandos de sistema
    if (lower.startsWith("/")) {
        executeCommand(message);
        return;
    }

    // Responder según la fase actual
    auto phase = phaseManager_.getCurrentPhase();
    switch (phase) {
        case MentorPhase::Welcome:
            respondWith("¡Bienvenido a MixCoach! 🎧 Estoy aquí para guiarte en tu mezcla. "
                        "Cuéntame qué género musical estás trabajando y cuántas pistas tienes.",
                        MentorMessage::Type::Question);
            break;

        case MentorPhase::GainStaging:
            analyzeGainStaging();
            break;

        case MentorPhase::Organisation:
            analyzeOrganisation();
            break;

        case MentorPhase::TonalBalance:
            analyzeTonalBalance();
            break;

        case MentorPhase::Dynamics:
            analyzeDynamics();
            break;

        case MentorPhase::Spatial:
            analyzeSpatial();
            break;
    }

    // Verificar logros
    auto activeCount = sharedData_.getSlotRegistry().activeCount();
    if (activeCount >= 1)  phaseManager_.unlockAchievement(Achievement::FirstTrack);
    if (activeCount >= 5)  phaseManager_.unlockAchievement(Achievement::FiveTracks);
    if (activeCount >= 10) phaseManager_.unlockAchievement(Achievement::TenTracks);
}

void CoachEngine::generateProactiveTip()
{
    // Por ahora, tip genérico
    respondWith("Recuerda mantener un headroom de -6dB en el master para "
                "tener espacio de maniobra en la masterización.",
                MentorMessage::Type::Tip);
}

void CoachEngine::checkProgress()
{
    auto phase = phaseManager_.getCurrentPhase();
    auto progress = phaseManager_.getPhaseProgress(phase);

    respondWith(juce::String("Progreso en fase '") +
                phaseManager_.phaseDescription(phase) + "': " +
                juce::String(static_cast<int>(progress * 100.0f)) + "%",
                MentorMessage::Type::Info);

    if (phaseManager_.isPhaseComplete(phase)) {
        respondWith("¡Has completado esta fase! 🎉 ¿Quieres avanzar a la siguiente?",
                    MentorMessage::Type::Achievement);
    }
}

void CoachEngine::announceNewTrack(int slotIndex, const juce::String& trackName, const juce::Colour& colour)
{
    juce::ignoreUnused(slotIndex);
    
    auto colourHex = colour.toDisplayString(false);
    juce::String colourBlock = "[" + colourHex + "]";
    
    respondWith("! He detectado un nuevo Messenger en el mixer!\n\n"
                "**Pista:** " + trackName + "\n"
                "**Color:" + colourBlock + "\n\n"
                "Ya estoy recibiendo telemetria de esta pista. "
                "Puedes ver sus datos en el Dashboard y Analizadores.",
                MentorMessage::Type::Info);
    
    // Si estamos en Welcome, avanzar automaticamente a GainStaging
    if (phaseManager_.getCurrentPhase() == MentorPhase::Welcome) {
        respondWith("!Excelente! Como ya tienes pistas en el mezclador, "
                    "vamos directo a la fase de Gain Staging para ajustar niveles.",
                    MentorMessage::Type::Tip);
        phaseManager_.advanceToNextPhase();
    }
}

void CoachEngine::executeCommand(const juce::String& command)
{
    auto lower = command.toLowerCase();

    if (lower == "/next" || lower == "/avanzar") {
        phaseManager_.advanceToNextPhase();
        auto phase = phaseManager_.getCurrentPhase();
        respondWith(juce::String("✅ Avanzando a fase: ") + phaseNames[static_cast<int>(phase)],
                    MentorMessage::Type::Achievement);
        respondWith(phaseManager_.phaseDescription(phase), MentorMessage::Type::Tip);
    }
    else if (lower == "/status" || lower == "/progreso") {
        checkProgress();
    }
    else if (lower == "/help" || lower == "/ayuda") {
        respondWith("Comandos disponibles:\n"
                    "/next - Avanzar a la siguiente fase\n"
                    "/status - Ver progreso actual\n"
                    "/help - Mostrar esta ayuda",
                    MentorMessage::Type::Info);
    }
    else {
        respondWith("Comando no reconocido. Escribe /help para ver los comandos disponibles.",
                    MentorMessage::Type::Warning);
    }
}

void CoachEngine::respondWith(const juce::String& text, MentorMessage::Type type)
{
    MentorMessage msg;
    msg.type      = type;
    msg.text      = text.toStdString();
    msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
    msg.context   = "MixCoach";
    sharedData_.pushMessage(msg);
}

void CoachEngine::respondWithContext(const juce::String& text, const juce::String& context, MentorMessage::Type type)
{
    MentorMessage msg;
    msg.type      = type;
    msg.text      = text.toStdString();
    msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
    msg.context   = context.toStdString();
    sharedData_.pushMessage(msg);
}

void CoachEngine::analyzeGainStaging()
{
    respondWith("Estamos en fase de Gain Staging. Asegúrate de que:\n"
                "1. Ninguna pista haga clipping (picos < 0dB)\n"
                "2. El master tenga -6dB de headroom\n"
                "3. Usa el fader de ganancia, no el volume",
                MentorMessage::Type::Tip);
}

void CoachEngine::analyzeOrganisation()
{
    respondWith("En fase de Organización. Recomiendo:\n"
                "1. Nombra cada pista descriptivamente\n"
                "2. Asigna colores por familia (rojo=batería, azul=bajo, etc.)\n"
                "3. Agrupa en buses virtuales",
                MentorMessage::Type::Tip);
}

void CoachEngine::analyzeTonalBalance()
{
    respondWith("Analizando balance tonal... Revisa el espectro y compáralo "
                "con las referencias de tu género en el panel de Analizadores.",
                MentorMessage::Type::Info);
}

void CoachEngine::analyzeDynamics()
{
    respondWith("En fase de Dinámica. Considera:\n"
                "1. Compresor en buses de batería y voces\n"
                "2. Limitador en el master (solo 1-2dB de reducción)\n"
                "3. Sidechain para bombo y bajo",
                MentorMessage::Type::Tip);
}

void CoachEngine::analyzeSpatial()
{
    respondWith("Trabajando la espacialidad... Sugerencias:\n"
                "1. Panoramas: batería al centro, guitarras a los lados\n"
                "2. Reverb: usa un send auxiliar\n"
                "3. Delay en voces para profundidad",
                MentorMessage::Type::Tip);
}

} // namespace mixcoach

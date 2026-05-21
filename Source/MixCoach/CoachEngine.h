#pragma once
#include <juce_core/juce_core.h>
#include "../Common/Types.h"
#include "../Common/SharedData.h"
#include "PhaseManager.h"

namespace mixcoach {

// ─── Motor de IA (sistema experto + comandos) ────────────────────────────────
class CoachEngine
{
public:
    CoachEngine(PhaseManager& phaseManager, SharedData& sharedData);

    // Procesar mensaje del usuario
    void handleUserMessage(const juce::String& message);

    // Generar sugerencia proactiva basada en la fase actual
    void generateProactiveTip();

    // Verificar progreso general
    void checkProgress();

    // Comandos especiales
    void executeCommand(const juce::String& command);

private:
    PhaseManager& phaseManager_;
    SharedData&   sharedData_;

    // Helpers de respuesta
    void respondWith(const juce::String& text, MentorMessage::Type type);
    void respondWithContext(const juce::String& text, const juce::String& context, MentorMessage::Type type);

    // Analizadores por fase
    void analyzeGainStaging();
    void analyzeOrganisation();
    void analyzeTonalBalance();
    void analyzeDynamics();
    void analyzeSpatial();
};

} // namespace mixcoach

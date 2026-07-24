#include "CoachingNarrativeDirector.h"
#include "../../_legacy/CoachingScriptConstants.h"
#include "ProblemAnalyzerMap.h"
#include "PluginSuggestionsProvider.h"
#include "TrackGainAnalyzer.h"
#include "../UI/TrackProblemBuilder.h"
#include "TrackRole.h"
#include "../UI/NavigationShell.h"
#include "../UI/CoachChatComponent.h"
#include "../UI/QuickReplyBar.h"
#include "../UI/PhaseProgressBar.h"
#include "../audio/AudioAnalyzer.h"
#include "CoachEngine.h"

namespace mixcoach {

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Helpers estÃ¡ticos
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

    juce::Colour CoachingNarrativeDirector::tierColour(int index)
    {
        switch (index) {
            case 0:  return juce::Colour(0xFF4ADE80); // Native  â†’ green
            case 1:  return juce::Colour(0xFF22D3EE); // Free    â†’ cyan
            case 2:  return juce::Colour(0xFFA855F7); // Premium â†’ purple
            case 3:  return juce::Colour(0xFFFBBF24); // UserHas â†’ amber
            default: return juce::Colour(0xFF888888);
        }
    }

    const char* CoachingNarrativeDirector::tierIcon(int index)
    {
        switch (index) {
            case 0:  return "[COACH]";  // ðŸŽ›
            case 1:  return "\xF0\x9F\x9F\xA2";  // ðŸŸ¢
            case 2:  return "\xE2\xAD\x90";      // â­
            case 3:  return "[BOLT]";      // âš¡
            default: return "";
        }
    }

    const char* CoachingNarrativeDirector::tierLabel(int index)
    {
        switch (index) {
            case 0:  return "Nativo";
            case 1:  return "Gratis";
            case 2:  return "Profesional";
            case 3:  return "Ya tienes";
            default: return "";
        }
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Constructor
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

    CoachingNarrativeDirector::CoachingNarrativeDirector(NavigationShell& navShell, MixCoachPanel& coachPanel,
                                                         SharedData& sharedData)
        : navShell_(navShell),
          coachPanel_(coachPanel),
          evidencePanel_(coachPanel.getEvidencePanel()),
          messageSequencer_(coachPanel),
          sharedData_(sharedData)
    {
        // Timer starts only when a problem is in progress â€” not permanently

    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  buildFromProblemType â€” Construye un CoachingProblem desde un ProblemType
    //
    //  Separa las sugerencias por tier (Native/Free/Premium) y mapea el
    //  ProblemType al CoachRoomState correcto + analyzer contextual.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    CoachingProblem CoachingNarrativeDirector::buildFromProblemType(
        ProblemType detectedProblem,
        const PluginSuggestionsProvider& provider)
    {
        CoachingProblem problem;

        TrackProblemGroup group;
        TrackProblemData track;
        track.problemType = juce::String(problemTypeToString(detectedProblem));

        // Inferir domain desde ProblemType
        switch (detectedProblem) {
            case ProblemType::Masking:
            case ProblemType::TonalExcess:
            case ProblemType::TonalDeficit:
                track.domain = "tonal"; break;
            case ProblemType::DynamicsOvercompressed:
            case ProblemType::DynamicsTooDynamic:
                track.domain = "dynamics"; break;
            case ProblemType::Phase:
                track.domain = "phase"; break;
            case ProblemType::Spatial:
            case ProblemType::Reverb:
                track.domain = "spatial"; break;
            case ProblemType::Gain:
            case ProblemType::Clipping:
                track.domain = "gain"; break;
            case ProblemType::Saturation:
                track.domain = "saturation"; break;
            case ProblemType::Limiting:
                track.domain = "loudness"; break;
            default:
                track.domain = "unknown";
        }

        track.severity = 0.5f;

        // Asignar roleName descriptivo para buildExplainMessage()
        switch (detectedProblem) {
            case ProblemType::Gain:
            case ProblemType::Clipping:
                track.roleName = "pista con nivel fuera de rango"; break;
            case ProblemType::Masking:
            case ProblemType::TonalExcess:
            case ProblemType::TonalDeficit:
                track.roleName = "pista con desbalance espectral"; break;
            case ProblemType::DynamicsOvercompressed:
                track.roleName = "pista sobrecomprimida"; break;
            case ProblemType::DynamicsTooDynamic:
                track.roleName = "pista con dinámica excesiva"; break;
            case ProblemType::Phase:
                track.roleName = "pista con problema de fase"; break;
            case ProblemType::Spatial:
            case ProblemType::Reverb:
                track.roleName = "pista con imagen estéreo"; break;
            case ProblemType::Saturation:
                track.roleName = "pista que necesita saturación"; break;
            case ProblemType::Limiting:
                track.roleName = "pista que necesita limitación"; break;
            default:
                track.roleName = "pista";
        }
        track.trackName = "tu mezcla";

        group.tracks.push_back(std::move(track));
        problem.trackGroup = std::move(group);

        // Obtener sugerencias y separar por tier
        auto suggestions = provider.getSuggestionsForProblem(detectedProblem, 0.0f, 0.0f);

        auto fillGroup = [&](const juce::String& tierName, PluginTier targetTier) -> PluginSuggestionGroup {
            PluginSuggestionGroup g;
            g.problemTitle = juce::String(problemTypeToString(detectedProblem));
            g.severity = 0.5f;
            for (const auto& sug : suggestions) {
                if (sug.displayTier() != targetTier) continue;
                PluginSuggestionGroup::TierSuggestion ts;
                ts.pluginName = (sug.plugin != nullptr) ? sug.plugin->name : "Plugin";
                ts.tier = tierName;
                ts.isUserHas = (sug.displayTier() == PluginTier::UserHas);
                if (sug.config != nullptr) {
                    ts.actionText = PluginSuggestionsProvider::interpolateAction(
                        sug.config->actionText, sug.delta, sug.frequencyHz);
                } else {
                    ts.actionText = "Abre el plugin y ajusta según recomendación";
                }
                g.suggestions.push_back(std::move(ts));
            }
            return g;
        };

        problem.nativeOption  = fillGroup("Nativo",      PluginTier::Native);
        problem.freeOption    = fillGroup("Gratis",      PluginTier::Free);
        problem.premiumOption = fillGroup("Profesional", PluginTier::Premium);

        // Mapear ProblemType â†’ CoachRoomState
        switch (detectedProblem) {
            case ProblemType::Gain:
            case ProblemType::Clipping:
                problem.phase = CoachRoomState::GainStaging; break;
            case ProblemType::Masking:
            case ProblemType::TonalExcess:
            case ProblemType::TonalDeficit:
            case ProblemType::Saturation:
                problem.phase = CoachRoomState::EQ; break;
            case ProblemType::DynamicsOvercompressed:
            case ProblemType::DynamicsTooDynamic:
                problem.phase = CoachRoomState::Compression; break;
            case ProblemType::Phase:
            case ProblemType::Spatial:
            case ProblemType::Reverb:
                problem.phase = CoachRoomState::Space; break;
            case ProblemType::Limiting:
                problem.phase = CoachRoomState::MasterCheck; break;
            default:
                problem.phase = CoachRoomState::GainStaging;
        }

        // Mapear ProblemType â†’ analyzer contextual (via ProblemAnalyzerMap)
        auto mapping = ProblemAnalyzerMap::lookup(detectedProblem);
        problem.evidenceAnalyzer = juce::String(mapping.viewId);
        if (mapping.hasHighlight()) {
            problem.highlightFreq = mapping.highlightFreq;
            problem.highlightLabel = juce::String(juce::CharPointer_UTF8(mapping.highlightLabel));
        }

        return problem;
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  convertPhaseToAnalyzerId â€” Mapea fase â†’ ID de analyzer
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    juce::String CoachingNarrativeDirector::phaseToAnalyzerId(CoachRoomState phase) const
    {
        switch (phase) {
            case CoachRoomState::GainStaging: return "vu";
            case CoachRoomState::EQ:          return "spectrum";
            case CoachRoomState::Compression: return "crest";
            case CoachRoomState::Space:       return "vectorscope";
            case CoachRoomState::Automation:  return "lufs";
            case CoachRoomState::MasterCheck: return "lufs";
            default:                          return "vu";
        }
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  buildExplainMessage â€” Construye mensaje de explicaciÃ³n del problema
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    juce::String CoachingNarrativeDirector::buildExplainMessage() const
    {
        if (currentProblem_.explanationMessage.isNotEmpty())
            return currentProblem_.explanationMessage;

        auto& tracks = currentProblem_.trackGroup.tracks;
        if (tracks.empty()) return {};

        // â•â•â• Obtener contexto temporal del DAW â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
        // Si el DAW estÃ¡ reproduciendo, incluimos la posiciÃ³n y BPM
        // para que el coach pueda referirse al contexto musical.
        // Ej: "âš¡ Detecto un problema durante la reproducciÃ³n a 1:23 (128 BPM)..."
        auto transportInfo = navShell_.getTransportInfo();
        auto transportCtx = TransportContext::fromFields(
            transportInfo.valid,
            transportInfo.isPlaying,
            transportInfo.isLooping,
            transportInfo.bpm,
            transportInfo.timeInSeconds,
            transportInfo.ppqPositionOfLastBarStart,
            transportInfo.timeSigNumerator,
            transportInfo.timeSigDenominator);
        juce::String timeCtx = transportCtx.formatDescription();

        juce::String msg;

        // --- IntegraciÃ³n del Guion de la Pantalla 7 ---
        juce::String userName = "Ingeniero"; // Default
        if (auto* eng = navShell_.getCoachEngine()) userName = eng->getEngineerName();

        if (currentProblem_.phase == CoachRoomState::EQ && tracks.size() >= 1)
        {
            // Guion EQ: "Detecto que el Kick y el Bass compiten..."
            juce::String t1 = tracks[0].roleName;
            juce::String t2 = !tracks[0].maskingInfo.isEmpty() ? tracks[0].maskingInfo : "otro track";
            msg = script::Coaching::EQ::getMasking(t1, t2, juce::roundToInt(currentProblem_.highlightFreq));

            // AÃ±adir contexto temporal despuÃ©s del guiÃ³n
            if (timeCtx.isNotEmpty()) {
                msg << "\n\n" << "\xF0\x9F\x95\x90 " << timeCtx;
            }

            msg << "\n\n" << script::Coaching::EQ::getAskPreference(userName);
            return msg;
        }

        if (currentProblem_.phase == CoachRoomState::Compression)
        {
            // Guion CompresiÃ³n
            msg = script::Coaching::Compression::getCrestInfo(currentProblem_.trackGroup.groupName,
                                                              juce::roundToInt(currentProblem_.beforeCrestDb), 10);

            // AÃ±adir contexto temporal despuÃ©s del guiÃ³n
            if (timeCtx.isNotEmpty()) {
                msg << "\n\n" << "\xF0\x9F\x95\x90 " << timeCtx;
            }

            return msg;
        }

        // Fallback genÃ©rico mejorado
        int count = (int)tracks.size();
        if (count == 1) {
            msg << "He encontrado un problema en **" << tracks[0].roleName << "**"
                << " (" << tracks[0].trackName << ")";
        } else {
            msg << "He encontrado un problema en **" << count << " pistas** "
                << "del grupo **" << currentProblem_.trackGroup.groupName << "**";
        }

        if (!tracks[0].problemType.isEmpty()) {
            msg << ": **" << tracks[0].problemType << "**";
            if (!tracks[0].maskingInfo.isEmpty())
                msg << " (conflicto con: " << tracks[0].maskingInfo << ")";
        }

        msg << ".\n\n";

        // AÃ±adir contexto temporal
        if (timeCtx.isNotEmpty()) {
            msg << "\xF0\x9F\x95\x90 " << timeCtx << "\n\n";
        }

        msg << "Te muestro la evidencia para que puedas verlo claramente.";

        return msg;
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  pickOption â€” Selecciona una opciÃ³n de plugin por Ã­ndice
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    PluginSuggestionGroup CoachingNarrativeDirector::pickOption(int index) const
    {
        switch (index) {
            case 0:  return currentProblem_.nativeOption;
            case 1:  return currentProblem_.freeOption;
            case 2:  return currentProblem_.premiumOption;
            default: return {};
        }
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  API PÃºblica
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

    void CoachingNarrativeDirector::startProblem(const CoachingProblem& problem)
    {
        if (!problem.isValid()) return;

        currentProblem_ = problem;
        selectedTierIndex_ = -1;
        reset();
        invalidateInterventionCache(); // Cache no vÃ¡lida con nuevo problema

        startTimerHz(60); // Start timer for narrative delays
        transitionTo(Step::ShowEvidence, 0);
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  processCoachMessage â€” Integra respuesta del LLM en el ciclo narrativo
    //
    //  Si el Director estÃ¡ Idle, postea la respuesta directamente como mensaje
    //  del coach (flujo normal de chat).
    //  Si el Director estÃ¡ en medio de un ciclo, la respuesta se almacena en
    //  pendingLlmResponse_ y se entrega al final del ciclo actual.
    //  Esto evita que el LLM interrumpa el ritmo narrativo del Director.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::processCoachMessage(const juce::String& llmResponse)
    {
        if (llmResponse.isEmpty()) return;

        if (currentStep_ == Step::Idle || currentStep_ == Step::Complete) {
            // â”€â”€â”€ Director inactivo: postear LLM response directamente â”€â”€â”€â”€â”€â”€
            messageSequencer_.enqueueBatch({
                SequencerStep::coach(llmResponse),
                SequencerStep::callback([this]() {
                    navShell_.setAvatarExpression(AvatarExpression::Neutral);
                })
            });
            return;
        }

        if (currentStep_ == Step::WaitingForUser) {
            // â”€â”€â”€ Esperando al usuario: encolar pero no interrumpir â”€â”€â”€â”€â”€â”€â”€â”€â”€
            // El Director seguirÃ¡ esperando la confirmaciÃ³n del usuario
            pendingLlmResponse_ = llmResponse;
            return;
        }

        // â”€â”€â”€ Director en medio de un ciclo: almacenar para entrega posterior â”€â”€
        pendingLlmResponse_ = llmResponse;
    }

    void CoachingNarrativeDirector::onOptionSelected(const juce::String& option)
    {
        if (currentStep_ != Step::ShowOptions && currentStep_ != Step::WaitingForUser)
            return;

        // Parsear el Ã­ndice del tier desde la opciÃ³n
        int tierIndex = -1;
        for (int i = 0; i < 4; ++i) {
            if (option.containsIgnoreCase(tierLabel(i))
                || option.containsIgnoreCase(pickOption(i).suggestions.empty() ? "" : pickOption(i).suggestions[0].pluginName))
            {
                tierIndex = i;
                break;
            }
        }

        // Si no encontramos el tier por label, buscar por coincidencia de plugin
        if (tierIndex < 0) {
            for (int i = 0; i < 4; ++i) {
                auto group = pickOption(i);
                for (auto& sug : group.suggestions) {
                    if (option.containsIgnoreCase(sug.pluginName)) {
                        tierIndex = i;
                        break;
                    }
                }
                if (tierIndex >= 0) break;
            }
        }

        if (tierIndex < 0) tierIndex = 0; // Fallback a Native

        selectedTierIndex_ = tierIndex;

        // Notificar a NavigationShell
        if (onTierSelected) {
            onTierSelected(tierIndex, pickOption(tierIndex));
        }

        // Transicionar a Waiting â†’ Verify
        coachPanel_.hideQuickReplies();

        // Postear confirmaciÃ³n del tier seleccionado
        auto selected = pickOption(tierIndex);
        juce::String confirmMsg;
        if (!selected.suggestions.empty()) {
            confirmMsg << "[DONE] Has elegido **" << tierLabel(tierIndex) << "**: "
                       << selected.problemTitle << ".\n\n";
            confirmMsg << "Aplica el cambio y confirma cuando esté listo.";
        } else {
            confirmMsg << "Perfecto. Aplica el cambio y confirma cuando esté listo.";
        }

        coachPanel_.addMessage(confirmMsg);

        // â•â•â• Tomar snapshot de niveles ANTES de que el usuario aplique el cambio â•â•â•
        takeBeforeSnapshot();

        // â•â•â• Registrar ventana de transporte para invalidación por cambio de sección â•â•â•
        // Esto guarda la posición actual del DAW para detectar si el usuario
        // se salta > 2 compases antes de confirmar la corrección.
        {
            auto transportInfo = navShell_.getTransportInfo();
            if (auto* coach = navShell_.getCoachEngine()) {
                auto& learner = coach->getCorrectionLearner();
                learner.startTransportWindow(
                    transportInfo.timeInSeconds,
                    transportInfo.bpm,
                    transportInfo.timeSigNumerator,
                    transportInfo.timeSigDenominator);

                // â•â•â• Incremento 4b (wire): Registrar intento de verificación â•â•â•
                // Cada vez que el usuario selecciona una opción y se inicia un
                // verify loop, contamos como un intento. Si luego se invalida
                // por cambio de sección, decrementamos la confianza.
                learner.recordVerifyAttempt();
            }
        }

        // â•â•â• GAP #1: Input disabled durante verify â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
        coachPanel_.setChatInputEnabled(false);

        // Ir a ConfirmApplied
        // (retryCount_ se resetea en reset() al inicio de cada problema)
        transitionTo(Step::ConfirmApplied, 0);
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  populateGainStaging â€” Pobla el GainStagingPanel con datos reales
    //
    //  Analiza todas las pistas activas, calcula advices de ganancia, pobla
    //  la tabla en GainStagingPanel, y cablea los botones onApplyGain (individual)
    //  y onApplyAll (masivo). Cada botÃ³n registra la recomendaciÃ³n en el engine,
    //  trackea el plugin aplicado, y actualiza el avatar.
    //
    //  ExtraÃ­do de NavigationShell::setCoachRoomState() bloque entGain (~215 lÃ­neas).
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::populateGainStaging(CoachEngine& coach)
    {
        auto& gainPanel = coachPanel_.getGainStagingPanel();
        auto trackRoles = coach.getTrackRoles();
        auto advicesG = analyzeAllTracksGain(sharedData_, trackRoles,
                                              coach.getSetupGenre(), false);

        std::vector<TrackGainRow> rowsG;
        int lowG = 0, highG = 0, clipG = 0;
        for (const auto& adv : advicesG) {
            if (!adv.isActionable()) continue;
            TrackGainRow row;
            row.slotIndex = adv.slotIndex;
            row.trackName = adv.trackName;
            row.roleName = getRoleName(adv.role);
            row.currentPeakDb = adv.currentPeak;
            row.targetPeakDb = adv.peakTarget;
            row.suggestedDeltaDb = adv.suggestedDeltaDb;
            row.clipping = (adv.currentPeak > -0.5f);
            if (row.clipping) clipG++;
            if (adv.suggestedDeltaDb > 0) lowG++;
            else if (adv.suggestedDeltaDb < 0) highG++;
            rowsG.push_back(std::move(row));
        }
        gainPanel.setTrackData(rowsG);

        // â”€â”€â”€ BotÃ³n Aplicar (individual) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        gainPanel.onApplyGain = [this](int slotIdx, float deltaDb) {
            // Obtener engine desde el processor via navShell_
            auto* eng = navShell_.getCoachEngine();
            if (eng == nullptr) return;

            auto& reg = sharedData_.getSlotRegistry();
            auto info = reg.getSlotInfo(slotIdx);
            juce::String tName = juce::String(info.trackName).trim();
            if (tName.isEmpty()) tName = "Track " + juce::String(slotIdx + 1);

            eng->storeRecommendation(slotIdx, tName, TrackRecommendation::Domain::Gain,
                "Ajustar ganancia " + juce::String(deltaDb, 1) + " dB",
                0.0f, deltaDb, deltaDb, "peak");

            CoachEngine::MixHistoryEntry mh;
            mh.timestampUs = juce::Time::getMillisecondCounter() * 1000;
            mh.slotIndex = slotIdx;
            mh.trackName = tName;
            mh.domain = "gain";
            mh.description = "Ajuste Gain: " + juce::String(deltaDb, 1) + " dB";
            mh.beforeValue = 0.0f;
            mh.afterValue = deltaDb;
            mh.delta = deltaDb;
            mh.source = CoachEngine::MixHistoryEntry::Source::UserAction;
            eng->recordAppliedPlugin("Ajuste Gain: " + tName + " " + juce::String(deltaDb, 1) + " dB");
            eng->pushMixHistory(mh);

            navShell_.setAvatarNod(400);
        };

        // â”€â”€â”€ BotÃ³n Aplicar Todas (masivo) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        gainPanel.onApplyAll = [this, advicesG]() {
            auto* eng = navShell_.getCoachEngine();
            if (eng == nullptr) return;

            int applied = 0;
            for (const auto& adv : advicesG) {
                if (!adv.isActionable()) continue;
                auto& reg = sharedData_.getSlotRegistry();
                auto info = reg.getSlotInfo(adv.slotIndex);
                juce::String tName = juce::String(info.trackName).trim();
                if (tName.isEmpty()) tName = "Track " + juce::String(adv.slotIndex + 1);

                eng->storeRecommendation(adv.slotIndex, tName, TrackRecommendation::Domain::Gain,
                    "Ajustar ganancia " + juce::String(adv.suggestedDeltaDb, 1) + " dB",
                    0.0f, adv.suggestedDeltaDb, adv.suggestedDeltaDb, "peak");
                applied++;
            }

            navShell_.postUIEvent("[COACH]",
                "Aplicadas " + juce::String(applied) + " sugerencias de gain.");

            if (applied > 0) {
                navShell_.setAvatarExpression(AvatarExpression::Happy);
                navShell_.setAvatarWave(true);
                navShell_.setAvatarNod(600);
                navShell_.celebrate("Gain Staging: " + juce::String(applied) + " ajustes aplicados");

                juce::String celebMsg;
                celebMsg << "\xF0\x9F\x8E\x89 Gain Staging completo! " << applied
                         << " ajustes aplicados. \xF0\x9F\x91\x8F Buen trabajo, la base esta solida.";
                coachPanel_.addSystemMessage(celebMsg);
            }
        };

        // â”€â”€â”€ Mensaje resumen con el anÃ¡lisis â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        int actCount = lowG + highG + clipG;
        if (actCount > 0) {
            juce::String summary = "[CHART] **" + script::FinalReport::kTitle + "**\n\n";
            summary += script::Coaching::Gain::getSummary(lowG, highG + clipG) + "\n\n";
            
            for (const auto& adv : advicesG) {
                if (!adv.isActionable()) continue;
                if (adv.currentPeak > -0.5f)
                    summary += "[EXCLAMATION] **" + adv.trackName + "**: " + script::Coaching::Gain::getClipping(adv.trackName) + "\n";
                else
                    summary += "\xE2\x80\xA2 " + adv.trackName + ": RMS "
                               + juce::String(adv.currentRMS, 1) + " dB [RIGHT] "
                               + juce::String(adv.peakTarget, 1) + " dB\n";
            }
            summary += "\nSelecciona **Aplicar** en cada pista o usa **Aplicar todas**.";
            coachPanel_.addSystemMessage(summary);

            // TrackProblemCard inline
            auto groups = buildTrackProblemGroups(coach);
            for (const auto& grp : groups)
                coachPanel_.addTrackGroupCard(grp);
        }
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  takeBeforeSnapshot â€” Lee niveles actuales + nombre del slot afectado
    //  y los guarda en currentProblem_ antes de que el usuario aplique el cambio.
    //
    //  Lee del SlotRegistry vÃ­a SharedData para obtener:
    //    - Peak, RMS, Crest (para calcular delta real)
    //    - Track name (para mostrar "Î” -2.3 dB en Kick")
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::takeBeforeSnapshot()
    {
        // Determinar quÃ© slot monitorear
        int slotIndex = -1;
        if (!currentProblem_.trackGroup.tracks.empty())
            slotIndex = currentProblem_.trackGroup.tracks[0].slotIndex;

        // Si slotIndex no estÃ¡ disponible, buscar el primer slot activo
        if (slotIndex < 0) {
            auto& registry = sharedData_.getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                if (slotIndex < 0)
                    slotIndex = info.slotIndex;
            });
        }

        currentProblem_.beforeSlotIndex = slotIndex;

        if (slotIndex < 0) {
            // No hay slots activos â€” no podemos tomar snapshot
            currentProblem_.beforePeakDb = -100.0f;
            currentProblem_.beforeRmsDb = -100.0f;
            currentProblem_.beforeCrestDb = 0.0f;
            currentProblem_.beforeTrackName = "Track";
            return;
        }

        // â•â•â• Leer nombre del track desde el SlotRegistry â•â•â•
        auto& registry = sharedData_.getSlotRegistry();
        auto slotInfo = registry.getSlotInfo(slotIndex);
        juce::String trackName = juce::String(slotInfo.trackName).trim();
        if (trackName.isEmpty())
            trackName = "Track " + juce::String(slotIndex + 1);
        currentProblem_.beforeTrackName = trackName;

        // â•â•â• Leer telemetrÃ­a actual del SlotRegistry â•â•â•
        auto tr = sharedData_.getTrackAudioResult(slotIndex);
        currentProblem_.beforePeakDb = juce::jmax(tr.peakLeft, tr.peakRight);
        currentProblem_.beforeRmsDb = juce::jmax(tr.rmsLeft, tr.rmsRight);

        // Crest: promedio de crestPerBand que tengan datos
        float crestSum = 0.0f;
        int crestCount = 0;
        for (int b = 0; b < 6; ++b) {
            if (tr.crestPerBand[b] > 0.01f) {
                crestSum += tr.crestPerBand[b];
                crestCount++;
            }
        }
        currentProblem_.beforeCrestDb = (crestCount > 0) ? (crestSum / (float)crestCount) : 0.0f;

        LogHelper::writeToLog("[Director] Before snapshot: slot=" + juce::String(slotIndex)
                              + " track=\"" + trackName + "\""
                              + " Peak=" + juce::String(currentProblem_.beforePeakDb, 1)
                              + " RMS=" + juce::String(currentProblem_.beforeRmsDb, 1)
                              + " Crest=" + juce::String(currentProblem_.beforeCrestDb, 1));
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  verifyWithRealData â€” Lee niveles actuales del SlotRegistry, calcula
    //  delta real comparando con el snapshot, y actualiza CorrectionCardData.
    //
    //  El delta se calcula con la mÃ©trica apropiada segÃºn el dominio:
    //    gain/clipping â†’ Peak (dBFS)
    //    tonal/masking â†’ RMS (dBFS)
    //    dynamics      â†’ Crest (dB)
    //
    //  El mensaje de feedback incluye el nombre del track y el delta exacto:
    //    "âœ… En Kick â€” Peak: -12.5 â†’ -15.2 dB (Î” -2.7 dB)"
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::verifyWithRealData()
    {
        auto& data = currentProblem_.correctionData;

        // â•â•â• Invalidación por cambio de sección â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
        // Si el DAW avanzó más de 2 compases desde que se propuso la corrección,
        // la canción cambió de sección y la verificación no es válida.
        // Esto evita que el coach celebre cambios que no son del usuario.
        if (auto* coach = navShell_.getCoachEngine()) {
            auto transportInfo = navShell_.getTransportInfo();
            auto& learner = coach->getCorrectionLearner();
            if (learner.hasTransportWindow()
                && learner.isSectionChanged(transportInfo.timeInSeconds,
                                             transportInfo.bpm,
                                             transportInfo.timeSigNumerator,
                                             transportInfo.timeSigDenominator))
            {
                // â•â•â• Incremento 4b (wire): Registrar invalidación por cambio de sección â•â•â•
                // Esto reduce la confianza del reporte: las correcciones invalidadas
                // no se cuentan como verificaciones exitosas.
                // learner ya está definido en el enclosing if (coach != null).
                learner.recordVerifyInvalidation();

                data.status = CorrectionCardData::Status::InvalidatedBySection;
                data.feedbackMessage = "\xF0\x9F\x94\x80 La canci\xF3n cambi\xF3 de secci\xF3n mientra aplicabas el cambio. "
                    "La verificaci\xF3n se ha invalidado autom\xE1ticamente. "
                    "Reproduce la secci\xF3n anterior y vuelve a intentarlo.";
                LogHelper::writeToLog("[Director] Verification invalidated — section changed since correction proposed");
                return;
            }
        }

        int slot = currentProblem_.beforeSlotIndex;
        if (slot < 0) {
            // Fallback: no hay snapshot, buscar el primer slot activo
            auto& registry = sharedData_.getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                if (slot < 0) slot = info.slotIndex;
            });
        }

        if (slot < 0) {
            // No hay datos â€” verificaciÃ³n simulada
            data.status = CorrectionCardData::Status::Verified;
            data.verifiedAfter = -100.0f;
            data.verifiedDelta = 0.0f;
            data.feedbackMessage = "Cambio aplicado.";
            return;
        }

        // â•â•â• Obtener nombre real del track desde SlotRegistry â•â•â•
        juce::String liveTrackName;
        {
            auto& registry = sharedData_.getSlotRegistry();
            auto info = registry.getSlotInfo(slot);
            liveTrackName = juce::String(info.trackName).trim();
            if (liveTrackName.isEmpty())
                liveTrackName = "Track " + juce::String(slot + 1);
        }
        currentProblem_.beforeTrackName = liveTrackName;
        data.verifiedTrackName = liveTrackName;

        // â•â•â• Leer valor actual del SlotRegistry â•â•â•
        auto tr = sharedData_.getTrackAudioResult(slot);
        float afterPeak = juce::jmax(tr.peakLeft, tr.peakRight);
        float afterRms = juce::jmax(tr.rmsLeft, tr.rmsRight);

        // Calcular crest actual
        float afterCrest = 0.0f;
        int crestCount = 0;
        for (int b = 0; b < 6; ++b) {
            if (tr.crestPerBand[b] > 0.01f) {
                afterCrest += tr.crestPerBand[b];
                crestCount++;
            }
        }
        afterCrest = (crestCount > 0) ? (afterCrest / (float)crestCount) : 0.0f;

        // â•â•â• Determinar quÃ© mÃ©trica usar segÃºn el dominio del problema â•â•â•
        float beforeValue = currentProblem_.beforePeakDb;
        float afterValue = afterPeak;
        juce::String metricLabel = "Peak";

        if (!currentProblem_.trackGroup.tracks.empty()) {
            auto domain = currentProblem_.trackGroup.tracks[0].domain;
            if (domain == "gain" || domain == "clipping") {
                beforeValue = currentProblem_.beforePeakDb;
                afterValue = afterPeak;
                metricLabel = "Peak";
            } else if (domain == "tonal" || domain == "masking") {
                beforeValue = currentProblem_.beforeRmsDb;
                afterValue = afterRms;
                metricLabel = "RMS";
            } else if (domain == "dynamics") {
                beforeValue = currentProblem_.beforeCrestDb;
                afterValue = afterCrest;
                metricLabel = "Crest";
            }
        }

        data.verifiedAfter = afterValue;
        data.verifiedDelta = std::abs(afterValue - beforeValue);
        data.metricLabel = metricLabel;

        // Si no hay seÃ±al, marcar como pendiente
        if (afterValue < -80.0f) {
            data.status = CorrectionCardData::Status::Pending;
            data.feedbackMessage = "En **" + liveTrackName + "** — no puedo verificar, "
                "parece que el track no tiene señal.";
            data.verifiedDelta = 0.0f;
            return;
        }

        // â•â•â• Determinar resultado basado en el delta real â•â•â•
        float delta = afterValue - beforeValue;
        if (std::abs(delta) < 0.3f) {
            // Cambio muy pequeÃ±o o nulo
            data.status = CorrectionCardData::Status::Failed;
            data.feedbackMessage = "En **" + liveTrackName + "** — no se detectó un cambio significativo. "
                + metricLabel + ": " + juce::String(afterValue, 1) + " dB (era "
                + juce::String(beforeValue, 1) + " dB). Â¿Seguro que aplicaste el cambio?";
        } else {
            // â•â•â• Cambio detectado â€” postear delta exacto â•â•â•
            data.status = CorrectionCardData::Status::Verified;
            data.feedbackMessage = "En **" + liveTrackName + "** - "
                + metricLabel + ": " + juce::String(beforeValue, 1) + " -> "
                + juce::String(afterValue, 1) + " dB (Î” "
                + juce::String(delta, 1) + " dB).";
        }

        // Registrar la correcciÃ³n en el log
        LogHelper::writeToLog("[Director] Verify real: track=\"" + liveTrackName
                              + "\" slot=" + juce::String(slot)
                              + " " + metricLabel + " " + juce::String(beforeValue, 1)
                              + " -> " + juce::String(afterValue, 1)
                              + " dB (Î” " + juce::String(delta, 1) + ")"
                              + " status=" + juce::String(static_cast<int>(data.status)));
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  EngineeringIntervention â€” IntervenciÃ³n estructurada en 5 pasos
    //
    //  Genera respuestas del Coach que siguen la estructura de un ingeniero
    //  senior sentado al lado del usuario:
    //    1. ðŸ‘ï¸ ObservaciÃ³n â€” quÃ© detectÃ³
    //    2. ðŸ” Causa probable â€” por quÃ© ocurre
    //    3. ðŸ› ï¸ AcciÃ³n exacta â€” quÃ© hacer
    //    4. ðŸ‘‚ Forma de escuchar â€” quÃ© atender
    //    5. âœ… Criterio de avance â€” cuÃ¡ndo seguir
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    EngineeringIntervention CoachingNarrativeDirector::buildEngineeringIntervention() const
    {
        // Usar cache si estÃ¡ disponible
        if (interventionCacheValid_)
            return cachedIntervention_;

        EngineeringIntervention intervention;

        if (!currentProblem_.isValid()) {
            interventionCacheValid_ = true;
            cachedIntervention_ = intervention;
            return intervention;
        }

        auto& tracks = currentProblem_.trackGroup.tracks;
        juce::String trackName = tracks.empty() ? "tu mezcla" : tracks[0].trackName;
        juce::String roleName = tracks.empty() ? "pista" : tracks[0].roleName;
        juce::String domain = tracks.empty() ? "" : tracks[0].domain;
        float freq = currentProblem_.highlightFreq;

        // â”€â”€â”€ Construir segÃºn el dominio del problema â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (domain == "gain" || domain == "clipping" || currentProblem_.phase == CoachRoomState::GainStaging)
        {
            float peak = currentProblem_.beforePeakDb;
            intervention.observation = "Detecté que **" + trackName + "** (" + roleName
                + ") tiene un nivel de pico de " + (peak > -80.0f ? juce::String(peak, 1) + " dBFS" : juce::String("señal insuficiente"))
                + ". El rango óptimo para empezar a mezclar es entre -18 dBFS y -12 dBFS.";

            intervention.probableCause = juce::String("El gain de entrada en el canal o el plugin está ")
                + (peak > -0.5f ? "demasiado alto, causando clipping en el convertidor AD."
                   : peak > -12.0f ? "por encima del rango óptimo, reduciendo el headroom disponible para procesamiento."
                   : "por debajo del rango óptimo, lo que obligará a subir ganancia en etapas posteriores y aumentará el piso de ruido.");

            intervention.exactAction = juce::String("Ajusta el **gain de entrada** (trim) del canal ")
                + trackName + juce::String(" para que el pico se sitúe alrededor de **-18 dBFS**. ")
                + (peak > -0.5f ? "Baja el fader o trim unos 12-15 dB para eliminar el clipping inmediatamente."
                   : peak > -12.0f ? "Baja el fader " + juce::String(peak + 18.0f, 1) + " dB para alcanzar el nivel objetivo."
                   : "Sube el fader " + juce::String(-18.0f - peak, 1) + " dB para alcanzar el nivel objetivo.");

            intervention.howToListen = juce::String("Escucha cómo el track conserva su carácter pero ")
                + (peak > -0.5f ? "ahora respira sin distorsión. El transiente del ataque se escucha limpio."
                   : "ahora tiene suficiente espacio para que los plugins trabajen correctamente. El nivel es más consistente.");

            intervention.criterion = "El meter de peak debe mostrar valores entre **-18 dBFS y -12 dBFS** "
                "en las partes más fuertes del track, sin superar -6 dBFS en ningún momento. "
                "Cuando logres eso en todos los tracks, podemos pasar al balance.";
        }
        else if (domain == "tonal" || domain == "masking" || currentProblem_.phase == CoachRoomState::EQ)
        {
            intervention.observation = "Hay un conflicto espectral en la zona de **"
                + (freq > 0.0f ? juce::String((int)freq) + " Hz** " : "frecuencias medias** ")
                + "entre " + trackName + " (" + roleName + ") y otros elementos de la mezcla. "
                + "Ambos compiten por el mismo espacio sonoro, lo que resta claridad al conjunto.";

            intervention.probableCause = "Cuando dos o más tracks tienen energía significativa en la misma banda "
                "de frecuencia, el oído no puede diferenciarlos. Esto produce una mezcla "
                "opaca, sin definición, especialmente en el rango medio (200 Hz - 5 kHz) "
                "donde somos más sensibles.";

            intervention.exactAction = "En el **ecualizador** de " + trackName
                + ", aplica un **corte suave (campana Q~1.0) de 2-3 dB** en la frecuencia "
                + (freq > 0.0f ? juce::String((int)freq) + " Hz" : "conflictiva")
                + ". Si prefieres que " + trackName + " domine, deja su ecualizador plano y "
                + "aplica el corte en los elementos que compiten.";

            intervention.howToListen = "Activa y desactiva el ecualizador para comparar. "
                "Escucha cómo los elementos ahora tienen su propio espacio: "
                "cada uno ocupa una región distinta del espectro. La mezcla "
                "se vuelve más transparente y cada instrumento es identificable.";

            intervention.criterion = "Debes poder escuchar claramente ambos elementos por separado "
                "incluso cuando suenan al mismo tiempo. Si cierras los ojos y aún puedes "
                "distinguir cada uno, el EQ está funcionando.";
        }
        else if (domain == "dynamics" || currentProblem_.phase == CoachRoomState::Compression)
        {
            float crest = currentProblem_.beforeCrestDb;
            intervention.observation = "La dinámica de **" + trackName + "**"
                + " (" + roleName + ") tiene un crest factor de " + (crest > 0.01f ? juce::String(crest, 1) + " dB" : "muy comprimido")
                + ". Los rangos saludables para este rol son entre 8-14 dB de crest.";

            intervention.probableCause = (crest < 8.0f && crest > 0.01f)
                ? "El crest bajo indica que el rango dinámico está excesivamente comprimido. "
                  "Los transientes pierden impacto y el track suena plano, sin vida. "
                  "Esto suele ocurrir por compresión en cadena (varias etapas) o un threshold demasiado agresivo."
                : (crest > 14.0f
                   ? "El crest alto sugiere que la dinámica es demasiado errática. "
                     "Las partes suaves se pierden y las fuertes sobresalen abruptamente, "
                     "dando una sensación de inestabilidad en la mezcla."
                   : "El crest está en el rango esperado pero podemos optimizarlo "
                     "para que el track se sienta más cohesionado dentro de la mezcla.");

            intervention.exactAction = "Ajusta el **compresor** de " + trackName
                + ": usa un **ratio entre 2:1 y 4:1**, un **threshold** que reduzca 3-6 dB "
                + (crest < 8.0f && crest > 0.01f
                   ? "(si está sobrecomprimido, sube el threshold 2-3 dB para reducir la ganancia de reducción)."
                   : "(si la dinámica es errática, baja el threshold 2-3 dB para atrapar los picos).")
                + " Ajusta el **attack** entre 10-30 ms (rápido para controlar transientes) "
                + "y **release** entre 40-80 ms (medio para que suene natural).";

            intervention.howToListen = "Con el compresor activo, escucha cómo el track se "
                "asienta en la mezcla sin perder su carácter. Los transientes (golpes, ataques) "
                "deben seguir siendo audibles pero controlados. El track debe sonar más "
                "cohesionado sin sonar aplastado.";

            intervention.criterion = "El crest factor debe situarse entre **8-14 dB** después del ajuste. "
                "Además, la reducción de ganancia no debe superar los 6 dB en los picos más fuertes. "
                "Cuando el track suene controlado pero vivo, el compresor está bien ajustado.";
        }
        else if (domain == "spatial" || domain == "phase" || currentProblem_.phase == CoachRoomState::Space)
        {
            float corr = currentProblem_.correctionData.verifiedAfter;
            intervention.observation = "La imagen estéreo de **" + trackName + "** (" + roleName
                + ") tiene una correlación de " + (corr > -99.0f ? juce::String(corr, 2) : "no disponible")
                + ". Los valores óptimos para el ancho estéreo están entre -0.3 y 0.3 de correlación, "
                + (corr > 0.5f ? "pero actualmente la señal es casi mono, lo que reduce la profundidad."
                   : corr < -0.3f ? "pero hay problemas de fase que pueden cancelar la señal al sumar a mono."
                   : " y actualmente está en el rango saludable.");

            intervention.probableCause = (corr > 0.5f)
                ? "Una correlación cercana a 1.0 significa que ambos canales son casi idénticos. "
                  "Esto ocurre cuando el track está grabado en mono o cuando se usan plugins "
                  "que procesan ambos canales de forma idéntica."
                : (corr < -0.3f
                   ? "Una correlación negativa indica que los canales izquierdo y derecho están "
                     "en desfase. Esto puede causar cancelaciones al reproducir en mono "
                     "(altavoces de celular, sistemas Bluetooth)."
                   : "La imagen estéreo actual es equilibrada. Podemos explorar si "
                     "conviene abrirla más para géneros como pop o rock.");

            intervention.exactAction = (corr > 0.5f)
                ? "Usa un **plugin de ancho estéreo** (como un imager o mid-side processor) "
                  "para expandir la señal. Aplica 2-4 dB de diferencia entre canales "
                  "en las frecuencias por encima de 200 Hz, dejando el grave (sub 200 Hz) en mono "
                  "para preservar la compatibilidad."
                : (corr < -0.3f
                   ? "Revisa la fase de los canales: invierte la fase de uno de ellos "
                     "(botón de phase en el canal o plugin) y observa si la correlación "
                     "mejora. Si es un problema de grabación, corrige con un plugin de ajuste de fase."
                   : "Mantén la configuración actual. Si buscas un sonido más amplio, "
                     "puedes abrir ligeramente el ancho en las frecuencias altas (>5 kHz).");

            intervention.howToListen = "Escucha la mezcla en mono (suma a mono) y en estéreo. "
                "En mono, todos los elementos deben seguir audibles sin desaparecer ni "
                "cambiar drásticamente su nivel. En estéreo, la escena debe sentir "
                "que tiene profundidad, con elementos en el centro, izquierda y derecha.";

            intervention.criterion = "La correlación debe estar entre **-0.3 y 0.3** para la mayoría "
                "de las frecuencias. Al sumar a mono, la mezcla debe sonar completa, sin "
                "pérdida significativa de nivel en ningún elemento.";
        }
        else if (currentProblem_.phase == CoachRoomState::Automation)
        {
            float lufs = currentProblem_.correctionData.verifiedAfter;
            intervention.observation = "He analizado la **distribución de loudness** en tu sesión. "
                + (lufs > -99.0f ? "El LUFS integrado es de " + juce::String(lufs, 1) + " dB." : "Aún no hay suficiente información de loudness.")
                + " Las secciones (verso, coro, puente) deben tener una progresión "
                + "intencional de energía, no uniforme.";

            intervention.probableCause = "La automatización de volumen es la herramienta más "
                "poderosa para dar movimiento a una mezcla. Sin ella, la canción suena "
                "estática: el coro no impacta más que el verso, y el puente no "
                "ofrece contraste dinámico. El oído humano necesita cambios de "
                "intensidad para mantener la atención.";

            intervention.exactAction = "Crea **automatización de volumen** en el master o en los "
                "elementos principales: el coro debe estar **1-2 dB más alto** que el verso. "
                "Usa automatización con curva (no escalones) para que las transiciones "
                "sean suaves. Automatiza también el **envío a reverb** para crear "
                "profundidad variable entre secciones.";

            intervention.howToListen = "Escucha la canción completa sin tocar nada. "
                "¿Sientes que el coro tiene más energía que el verso? "
                "¿La intensidad sube y baja de forma natural? Una buena mezcla "
                "cuenta una historia a través de su dinámica.";

            intervention.criterion = "El LUFS integrado del coro debe ser **1-3 dB mayor** que el del verso. "
                "La diferencia entre la sección más suave y la más intensa debe ser "
                "de al menos 3 dB para que la canción tenga viaje emocional.";
        }
        else if (currentProblem_.phase == CoachRoomState::MasterCheck)
        {
            float matchScore = 0.0f;
            if (auto* eng = navShell_.getCoachEngine()) {
                auto ctx = eng->buildSessionContext();
                matchScore = ctx.matchScore;
            }
            intervention.observation = "Comparé tu mezcla con la referencia. El **match score** "
                + (matchScore > 0.0f ? "es de " + juce::String((int)(matchScore * 100.0f)) + "%."
                   : "aún no está disponible para esta canción.")
                + " Estamos buscando que la mezcla capture la intención sonora "
                + "de la referencia sin perder tu identidad artística.";

            intervention.probableCause = "El oído se acostumbra rápidamente a tu mezcla (fatiga auditiva). "
                "Comparar contra una referencia externa revela desbalances que ya no "
                "escuchas: exceso de graves, falta de presencia, o desnivel entre "
                "canciones de un mismo proyecto.";

            intervention.exactAction = (matchScore > 0.7f)
                ? "Tu mezcla está cerca de la referencia. Ajusta el **volumen general** "
                  "(gain stage final) para que el LUFS integrado coincida dentro de 0.5 dB. "
                  "Presta atención a la **sonoridad percibida**: usa un medidor de "
                  "loudness en el master y empareja el short-term LUFS."
                : "Compara el **espectro de frecuencia** de tu mezcla contra la referencia. "
                  "¿Las proporciones entre graves, medios y agudos son similares? "
                  "Usa un ecualizador en el master para ajustes suaves (cortes/realces "
                  "de 1-2 dB como máximo) que alineen el balance tonal.";

            intervention.howToListen = "Alterna entre tu mezcla y la referencia cada 5-10 segundos. "
                "No busques que suenen igual, busca que tu mezcla **comunique la misma "
                "energía y emoción**. ¿La batería impacta igual? "
                "¿La voz tiene la misma presencia? ¿El bajo llena el mismo espacio?";

            intervention.criterion = "El match score debe ser superior al **75%** para considerar "
                "la mezcla lista para masterización. Más importante que el número: "
                "al alternar entre ambas, tu mezcla debe sentirse igual de impactante "
                "y profesional que la referencia.";
        }
        else
        {
            // Fallback gen\xC3\A9rico con estructura de 5 pasos
            intervention.observation = "Detecté un aspecto que podemos mejorar en **" + trackName
                + "** (" + roleName + "). El comportamiento actual se desvía del "
                + "estándar esperado para este rol en el género de tu proyecto.";

            intervention.probableCause = "Los ingenieros de mezcla experimentados construyen su "
                "criterio escuchando cientos de referencias. El ajuste que haremos "
                "ahora es uno de esos detalles que separan una mezcla amateur de "
                "una profesional.";

            intervention.exactAction = "Aplica el cambio sugerido en el panel de opciones. "
                "Cada opción (Nativo, Gratis, Profesional) tiene un enfoque "
                "distinto para lograr el mismo objetivo sonoro.";

            intervention.howToListen = "Antes y después del cambio, escucha el elemento "
                "en el contexto de toda la mezcla. El objetivo es que suene integrado, "
                "no procesado de forma aislada.";

            intervention.criterion = "Cuando el cambio suene natural y no llame la atención "
                "como 'procesado', estará correcto. El mejor procesamiento es "
                "el que no se nota, pero se extraña cuando se quita.";
        }

        return intervention;
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  postEngineeringMessage â€” Postea intervenciÃ³n completa (5 pasos) en el chat
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::postEngineeringMessage(const EngineeringIntervention& intervention)
    {
        if (!intervention.isValid()) return;
        coachPanel_.addSystemMessage(intervention.format());
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  postActionMessage â€” Postea solo acciÃ³n + escucha + criterio
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::postActionMessage(const EngineeringIntervention& intervention)
    {
        if (!intervention.isValid()) return;
        coachPanel_.addSystemMessage(intervention.formatAction());
    }

    void CoachingNarrativeDirector::onCorrectionConfirmed()
    {
        if (currentStep_ != Step::ConfirmApplied && currentStep_ != Step::WaitingForUser)
            return;

        // â•â•â• VERIFY REAL: Leer SlotRegistry y calcular delta exacto â•â•â•
        verifyWithRealData();

        auto& data = currentProblem_.correctionData;

        // Disparar callback de verificaciÃ³n (NavigationShell lo usa para storeRecommendation)
        if (onStartVerification) {
            onStartVerification(currentProblem_.correctionData);
        }

        // â•â•â• GAP #3: Retry logic â€” si fallÃ³ y quedan reintentos, volver a ShowOptions â•â•â•
        if (data.status == CorrectionCardData::Status::Failed && retryCount_ < kMaxRetries) {
            retryCount_++;

            juce::String retryMsg;
            retryMsg << "\xE2\x9D\x8C " << data.feedbackMessage << "\n\n";
            retryMsg << "Intento " << retryCount_ << " de " << kMaxRetries
                     << ". Puedes elegir otra opci\xC3\xB3n o intentarlo de nuevo.";

            messageSequencer_.enqueueBatch({
                SequencerStep::typingOn(),
                SequencerStep::delay(NarrativeTiming::kVerifyMs),
                SequencerStep::coach(retryMsg),
                SequencerStep::typingOff(),
                SequencerStep::delay(400),
                SequencerStep::callback([this]() {
                    // Reiniciar el ciclo desde ShowOptions para que el usuario
                    // pueda elegir otra opciÃ³n
                    doShowOptions();
                })
            });
            return;
        }

        // â•â•â• GAP #3: Si llegamos al mÃ¡ximo de reintentos, forzar Celebrate con advertencia â•â•â•
        if (data.status == CorrectionCardData::Status::Failed && retryCount_ >= kMaxRetries) {
            data.status = CorrectionCardData::Status::Partial;
            data.feedbackMessage = "Se alcanz\xC3\xB3 el m\xC3\xA1ximo de " + juce::String(kMaxRetries)
                                  + " intentos. Pasamos al siguiente problema.";
        }

        // â•â•â• Construir mensaje de verificaciÃ³n con delta exacto â•â•â•
        // Incluye: track name, mÃ©trica, before â†’ after, delta
        // Ej: "âœ… En Kick â€” Peak: -12.5 â†’ -15.2 dB (Î” -2.7 dB)"
        juce::String verifyMsg;
        if (data.status >= CorrectionCardData::Status::Verified) {
            verifyMsg = "[DONE] " + data.feedbackMessage;
        } else if (data.status == CorrectionCardData::Status::Partial) {
            verifyMsg = "[WARN] " + data.feedbackMessage;
        } else {
            verifyMsg = "\xE2\x9D\x8C " + data.feedbackMessage;
        }

        messageSequencer_.enqueueBatch({
            SequencerStep::typingOn(),
            SequencerStep::delay(NarrativeTiming::kVerifyMs),
            SequencerStep::system(verifyMsg),
            SequencerStep::typingOff(),
            SequencerStep::callback([this]() {
                transitionTo(Step::CelebrateStep, 0);
            })
        });
    }

    void CoachingNarrativeDirector::reset()
    {
        currentStep_ = Step::Idle;
        awaitingTransition_ = false;
        remainingTicks_ = 0;
        pendingStep_ = Step::Idle;
        selectedTierIndex_ = -1;
        retryCount_ = 0;

        // â•â•â• GAP #1: Asegurar que input estÃ© habilitado al resetear â•â•â•â•â•â•â•
        coachPanel_.setChatInputEnabled(true);

        stopTimer(); // No need for timer when idle
        messageSequencer_.cancel();
        coachPanel_.hideQuickReplies();
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  timerCallback â€” GestiÃ³n de delays entre pasos
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::timerCallback()
    {
        if (!awaitingTransition_) return;

        if (--remainingTicks_ <= 0) {
            awaitingTransition_ = false;
            executeStep(pendingStep_);
        }
    }

    void CoachingNarrativeDirector::transitionTo(Step step, int delayFrames)
    {
        if (delayFrames <= 0) {
            awaitingTransition_ = false;
            executeStep(step);
            return;
        }

        awaitingTransition_ = true;
        remainingTicks_ = delayFrames;
        pendingStep_ = step;
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  executeStep â€” Dispatcher de pasos
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::executeStep(Step step)
    {
        currentStep_ = step;

        switch (step) {
            case Step::ShowEvidence:   doShowEvidence();   break;
            case Step::Explain:        doExplain();        break;
            case Step::ShowOptions:    doShowOptions();    break;
            case Step::ConfirmApplied: doConfirm();        break;
            case Step::CelebrateStep:      doCelebrateStep();      break;
            case Step::NextProblemStep:    doNextProblemStep();    break;
            default: break;
        }
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Step 2: ShowEvidence â€” Abre el analyzer + evidencia con typing
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::doShowEvidence()
    {
        // â”€â”€â”€ GESTO DEL COACH: SeÃ±alar el problema â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        navShell_.setAvatarExpression(mixcoach::AvatarExpression::Thinking);
        navShell_.postUIEvent("\xF0\x9F\x91\x89", "Mira esto..."); 

        // Limpiar highlight anterior
        navShell_.setSpectrumHighlight(0.0f, {});
        navShell_.clearFocus();

        // --- Inferir ProblemType desde el CoachingProblem ---
        ProblemType problemType = ProblemType::Gain; // default
        int slotIndex = -1;
        if (!currentProblem_.trackGroup.tracks.empty()) {
            slotIndex = currentProblem_.trackGroup.tracks[0].slotIndex;
            auto& firstTrack = currentProblem_.trackGroup.tracks[0];

            if (!firstTrack.issueType.isEmpty()) {
                auto it = firstTrack.issueType.toLowerCase();
                if (it == "clipping")               problemType = ProblemType::Clipping;
                else if (it == "excess")             problemType = ProblemType::TonalExcess;
                else if (it == "deficit")            problemType = ProblemType::TonalDeficit;
                else if (it == "sobrecomprimido")    problemType = ProblemType::DynamicsOvercompressed;
                else if (it == "saturado")           problemType = ProblemType::Saturation;
                else if (it == "fase")               problemType = ProblemType::Phase;
                else if (it == "reverb")             problemType = ProblemType::Reverb;
                else if (!firstTrack.domain.isEmpty()) {
                    auto dom = firstTrack.domain.toLowerCase();
                    if (dom == "gain")      problemType = ProblemType::Gain;
                    else if (dom == "tonal") problemType = ProblemType::Masking;
                    else if (dom == "dynamics") problemType = ProblemType::DynamicsOvercompressed;
                    else if (dom == "spatial")  problemType = ProblemType::Spatial;
                    else if (dom == "phase")    problemType = ProblemType::Phase;
                }
            }
            else if (!firstTrack.domain.isEmpty()) {
                auto dom = firstTrack.domain.toLowerCase();
                if (dom == "gain")      problemType = ProblemType::Gain;
                else if (dom == "tonal") problemType = ProblemType::Masking;
                else if (dom == "dynamics") problemType = ProblemType::DynamicsOvercompressed;
                else if (dom == "spatial")  problemType = ProblemType::Spatial;
                else if (dom == "phase")    problemType = ProblemType::Phase;
            }
        }

        // Obtener mapeo estructural
        auto mapping = ProblemAnalyzerMap::lookup(problemType);

        // Construir mensaje de evidencia usando el mapping
        juce::String trackRoleName;
        if (!currentProblem_.trackGroup.tracks.empty())
            trackRoleName = currentProblem_.trackGroup.tracks[0].roleName;

        juce::String evidenceMsg = ProblemAnalyzerMap::buildEvidenceMessage(
            problemType, trackRoleName, currentProblem_.highlightFreq);

        // Auto-open analyzer tab via PanelRevealManager's structural method
        auto result = navShell_.getRevealManager().processProblemType(
            problemType,
            slotIndex,
            currentProblem_.highlightFreq);
        for (auto panel : result.panelsToReveal)
            navShell_.revealPanel(panel);

        // --- SincronizaciÃ³n Audio/Texto: Focus en Clipping/Gain ---
        if (problemType == ProblemType::Clipping || problemType == ProblemType::Gain) {
            if (slotIndex >= 0) {
                navShell_.showFocusOverlay(slotIndex);
            }
        }

        // Configurar el EvidencePanel para la fase actual
        evidencePanel_.setCoachRoomState(currentProblem_.phase);

        // â•â•â• Propagar highlight al SpectrographComponent + EvidencePanel â•â•â•
        float highlightFreq = 0.0f;
        juce::String highlightLabel;

        if (currentProblem_.highlightFreq > 0.0f) {
            highlightFreq = currentProblem_.highlightFreq;
            highlightLabel = currentProblem_.highlightLabel;
        }
        else if (mapping.hasHighlight()) {
            highlightFreq = mapping.highlightFreq;
            highlightLabel = juce::String(juce::CharPointer_UTF8(mapping.highlightLabel));
        }

        if (highlightFreq > 0.0f) {
            evidencePanel_.highlightedFreq_ = highlightFreq;
            evidencePanel_.highlightLabel_ = highlightLabel;
            // Propagar al SpectrographComponent via NavigationShell
            navShell_.setSpectrumHighlight(highlightFreq, highlightLabel);
        }

        // Configurar la expresiÃ³n del avatar
        navShell_.setAvatarExpression(AvatarExpression::Thinking);

        // â•â•â• EVIDENCIA VISUAL PRIMERO: auto-switch a Tools para que el usuario VEA el analyzer â•â•â•
        // El usuario ve el analyzer abierto con highlight ANTES de leer la explicaciÃ³n.
        // Auto-return al Coach despuÃ©s de 4s para que alcance a ver el mensaje de evidencia.
        navShell_.autoSwitchTab(TabBarComponent::Tools, 4.0f);

        // â•â•â• LUEGO: mensaje con delay inicial de 300ms (el ojo ve ANTES que el cerebro lea) â•â•
        // Primera pausa de 300ms: el usuario ve el analyzer highlight SIN texto.
        // Luego typing + evidencia + pausa + transiciÃ³n a Explain.
        messageSequencer_.enqueueBatch({
            SequencerStep::delay(300),     // 300ms — el usuario VE la evidencia primero (antes de leer texto)
            SequencerStep::typingOn(),
            SequencerStep::delay(NarrativeTiming::kDetectMs),   // 800ms typing antes de evidencia
            SequencerStep::system(evidenceMsg),
            SequencerStep::typingOff(),
            SequencerStep::delay(NarrativeTiming::kEvidenceMs), // 1000ms antes de explicaciÃ³n
            SequencerStep::callback([this]() {
                transitionTo(Step::Explain, 0);
            })
        });
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Step 3: Explain â€” ExplicaciÃ³n con typing + avatar nod + secuenciador
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    /** Construye el string de contexto temporal desde el TransportInfo del DAW.
        Retorna vacÃ­o si el DAW no reporta datos vÃ¡lidos. */
    static juce::String getTransportContext(NavigationShell& navShell)
    {
        auto transportInfo = navShell.getTransportInfo();
        auto transportCtx = TransportContext::fromFields(
            transportInfo.valid,
            transportInfo.isPlaying,
            transportInfo.isLooping,
            transportInfo.bpm,
            transportInfo.timeInSeconds,
            transportInfo.ppqPositionOfLastBarStart,
            transportInfo.timeSigNumerator,
            transportInfo.timeSigDenominator);
        return transportCtx.formatDescription();
    }

    void CoachingNarrativeDirector::doExplain()
    {
        // â•â•â• EngineeringIntervention: observar + causa (formato compacto) â•â•â•
        auto intervention = buildEngineeringIntervention();
        intervention.transportContext = getTransportContext(navShell_);

        // Fallback: si no se puede construir la intervenciÃ³n, usar mensaje legacy
        juce::String msg;
        if (intervention.isValid()) {
            msg = intervention.formatCompact();
        } else {
            msg = buildExplainMessage();
        }

        if (msg.isNotEmpty()) {
            messageSequencer_.enqueueBatch({
                SequencerStep::typingOn(),
                SequencerStep::delay(NarrativeTiming::kExplainMs),
                SequencerStep::coach(msg),
                SequencerStep::typingOff(),
                SequencerStep::callback([this]() {
                    // Avatar asiente mientras explica
                    navShell_.setAvatarNod(600);
                    transitionTo(Step::ShowOptions, 0);
                })
            });
        } else {
            // Mensaje vacÃ­o â€” avanzar directamente
            messageSequencer_.enqueueBatch({
                SequencerStep::delay(300),
                SequencerStep::callback([this]() {
                    transitionTo(Step::ShowOptions, 0);
                })
            });
        }
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Step 4: ShowOptions â€” 3 tiers como OptionCards visuales + QuickReply
    //
    //  Renderiza cada tier como una tarjeta visual (OptionCardComponent) dentro
    //  de una burbuja PluginSuggestionCard, usando la funciÃ³n estÃ¡tica
    //  OptionCardComponent::drawOptionCard() que ya existe en el renderer de chat.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::doShowOptions()
    {
        // â•â•â• QuickReplies con los 3 tiers (Nativo / Gratis / Profesional) â•â•â•
        // Cada chip muestra el nombre del plugin principal de cada tier.
        // Sin tarjeta inline — el usuario elige con UN clic.
        std::vector<juce::String> replies;
        if (!currentProblem_.nativeOption.suggestions.empty())
            replies.push_back(juce::String(tierLabel(0)));
        if (!currentProblem_.freeOption.suggestions.empty())
            replies.push_back(juce::String(tierLabel(1)));
        if (!currentProblem_.premiumOption.suggestions.empty())
            replies.push_back(juce::String(tierLabel(2)));

        // Usar sequencer: typing â†’ formatAction â†’ QuickReplies (sin tarjeta inline)
        messageSequencer_.enqueueBatch({
            SequencerStep::typingOn(),
            SequencerStep::delay(NarrativeTiming::kOptionsMs),
            // â•â•â• EngineeringIntervention: acciÃ³n + escucha + criterio â•â•â•
            SequencerStep::callback([this]() {
                auto actionMsg = buildEngineeringIntervention().formatAction();
                if (actionMsg.isNotEmpty())
                    coachPanel_.addSystemMessage(actionMsg);
            }),
            SequencerStep::typingOff(),
            SequencerStep::delay(200),
            SequencerStep::callback([this, replies]() {
                // â•â•â• Mostrar SOLO QuickReplies (sin PluginSuggestionCard) â•â•â•
                if (!replies.empty()) {
                    auto& quickReply = coachPanel_.getQuickReplyBar();
                    // Guardar callback anterior y reemplazar para este ciclo
                    auto prevCallback = quickReply.onReplySelected;
                    quickReply.onReplySelected = [this, prevCallback](const juce::String& reply) {
                        // Restaurar callback anterior inmediatamente (evita doble disparo)
                        coachPanel_.getQuickReplyBar().onReplySelected = prevCallback;
                        // Notificar al Director que se seleccionÃ³ un tier
                        onOptionSelected(reply);
                    };
                    coachPanel_.showQuickReplies(replies);
                }

                // Avatar en modo escucha
                navShell_.setAvatarExpression(AvatarExpression::Neutral);

                // Transicionar a Waiting
                currentStep_ = Step::WaitingForUser;
            })
        });
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Step 6: ConfirmApplied â€” ConfirmaciÃ³n con typing + delay
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::doConfirm()
    {
        juce::String confirmMsg = "[WAIT] Esperando confirmación... Aplica el cambio y "
                                   "responde *'Listo'* o *'Hecho'* cuando esté aplicado.";

        messageSequencer_.enqueueBatch({
            SequencerStep::typingOn(),
            SequencerStep::delay(NarrativeTiming::kOptionsMs),
            SequencerStep::system(confirmMsg),
            SequencerStep::typingOff(),
            SequencerStep::callback([this]() {
                // Avatar en modo escucha
                navShell_.setAvatarExpression(AvatarExpression::Serious);
            })
        });
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Step 7: CelebrateStep â€” CelebraciÃ³n + XP con sequencer
    //
    //  Muestra el delta exacto (Î” -2.3 dB) con el nombre del track
    //  usando los datos REALES de verifyWithRealData():
    //    "ðŸŽ‰ Â¡Excelente trabajo!
    //
    //     Has aplicado Gain Staging con la opciÃ³n Nativo.
    //
    //     âœ… Verificado con datos reales
    //
    //     En Kick â€” Peak: -12.5 â†’ -15.2 dB
    //     Î” -2.7 dB
    //
    //     âš¡ +45 XP Â· +12% progreso"
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::doCelebrateStep()
    {
        navShell_.setAvatarExpression(AvatarExpression::Celebrating);
        navShell_.postUIEvent("\xE2\x9C\xA8", "Â¡Objetivo logrado!");

        // 1. Construir mensaje de celebraciÃ³n
        juce::String celebMsg = "\xF0\x9F\x8E\x89 ";
        juce::String userName = "Ingeniero";
        if (auto* eng = navShell_.getCoachEngine()) userName = eng->getEngineerName();

        if (currentProblem_.phase == CoachRoomState::Compression)
            celebMsg << "**" << script::Coaching::Compression::celebrate() << "**";
        else if (currentProblem_.phase == CoachRoomState::EQ)
            celebMsg << "**" << script::Coaching::EQ::verification() << currentProblem_.beforeTrackName << script::Coaching::EQ::kMaskingFixed << "**";
        else
            celebMsg << "**" << script::FinalReport::getSuccess(userName) << "**";

        // 2. Mostrar datos REALES del delta (si existen)
        // Incluye: nombre del track, frecuencia, delta dB exacto
        auto& corr = currentProblem_.correctionData;
        if (corr.status >= CorrectionCardData::Status::Verified) {
            float before = currentProblem_.beforePeakDb;
            float after = corr.verifiedAfter;
            float delta = corr.verifiedDelta;
            juce::String trackName = currentProblem_.beforeTrackName;
            if (trackName.isEmpty()) trackName = "la pista";

            // Frequency context if available
            juce::String freqCtx;
            if (currentProblem_.highlightFreq > 0.0f) {
                freqCtx = " a " + juce::String(static_cast<int>(currentProblem_.highlightFreq)) + "Hz";
            }

            if (delta > 0.1f) {
                // Determine domain label for the metric
                const char* metricLabel = "nivel";
                if (currentProblem_.phase == CoachRoomState::Compression)
                    metricLabel = "crest";
                else if (currentProblem_.phase == CoachRoomState::EQ)
                    metricLabel = "RMS";
                else if (currentProblem_.phase == CoachRoomState::GainStaging)
                    metricLabel = "Peak";

                celebMsg << "\n\n✅ **Verificado con datos reales en **" << trackName << "**\n";
                celebMsg << "  " << juce::String(metricLabel) << ": "
                         << juce::String(before, 1) << " → " << juce::String(after, 1) << " dB"
                         << freqCtx
                         << " (Δ " << juce::String(delta, 1) << " dB)";
            } else {
                celebMsg << "\n\n✅ **Verificado en **" << trackName << "** — cambio aplicado correctamente";
            }
        }

        // 3. AÃ±adir el criterio de avance desde EngineeringIntervention
        {
            auto engIntervention = buildEngineeringIntervention();
            if (engIntervention.isValid()) {
                celebMsg << "\n\n" << engIntervention.criterion;
            }
        }
        celebMsg << "\n\nHas demostrado un gran criterio tÃ©cnico. Â¡Vamos a por el siguiente paso!";

        // 4. Orquestar secuencia de salida
        navShell_.getPhaseProgressBar().triggerXpBurst(45);
        juce::String xpMsg = "[BOLT] **+45 XP** \xC2\xB7 +12% progreso";

        coachPanel_.setChatInputEnabled(true);

        messageSequencer_.enqueueBatch({
            SequencerStep::typingOn(),
            SequencerStep::delay(NarrativeTiming::kCelebrateMs),
            SequencerStep::coach(celebMsg),
            SequencerStep::typingOff(),
            SequencerStep::delay(500),
            SequencerStep::system(xpMsg),
            SequencerStep::callback([this]() {
                transitionTo(Step::NextProblemStep, 0);
            })
        });

        // Limpiar Spotlight
        navShell_.setSpectrumHighlight(0.0f, {});
        navShell_.clearFocus();
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  Step 8: NextProblemStep â€” TransiciÃ³n al siguiente problema
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::doNextProblemStep()
    {
        coachPanel_.hideQuickReplies();
        coachPanel_.setChatInputEnabled(true);

        if (pendingLlmResponse_.isNotEmpty()) {
            juce::String llmMsg = pendingLlmResponse_;
            pendingLlmResponse_.clear();

            messageSequencer_.enqueueBatch({
                SequencerStep::coach(llmMsg),
                SequencerStep::callback([this]() {
                    deliverNextProblemStep();
                })
            });
        } else {
            deliverNextProblemStep();
        }
    }

    void CoachingNarrativeDirector::deliverNextProblemStep()
    {
        juce::String nextMsg = "[PHASE] Buscando el siguiente problema...";

        // FASE 8: 600ms de pausa entre fases para ritmo teatral (ease-out)
        // Antes del typing, añadimos un delay deliberado que permite al usuario
        // asimilar el XP y la celebración antes de lanzar el siguiente problema.
        messageSequencer_.enqueueBatch({
            SequencerStep::delay(NarrativeTiming::kAdvancePhaseMs),
            SequencerStep::typingOn(),
            SequencerStep::delay(300),
            SequencerStep::system(nextMsg),
            SequencerStep::typingOff(),
            SequencerStep::callback([this]() {
                if (onCycleComplete) onCycleComplete();
                currentStep_ = Step::Complete;
            })
        });
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  applyPhaseUI â€” Aplica la UI de fase de coaching
    //
    //  ExtraÃ­do de NavigationShell::setCoachRoomState() (~50 lÃ­neas).
    //  Controla visibilidad de paneles de fase y auto-abre analyzers contextuales.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::applyPhaseUI(CoachRoomState newState)
    {
        // â”€â”€â”€ Solo ocultar paneles de setup si estamos en modo coaching â”€â”€
        // (isCoachingState verifica que newState >= GainStaging)
        if (isCoachingState(newState)) {
            coachPanel_.setShowModeCards(false);
            coachPanel_.setShowGenreCards(false);
            coachPanel_.setShowSessionPrepCard(false);
            coachPanel_.setInlineReferenceDropZone(false);
            coachPanel_.setInlineMessengerStatus(false, 0, {});
        }

        // â”€â”€â”€ Visibilidad de paneles de fase â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        bool entGain = (newState == CoachRoomState::GainStaging);
        coachPanel_.getGainStagingPanel().setVisible(entGain);
        bool entEQ = (newState == CoachRoomState::EQ);
        coachPanel_.getEQPanel().setVisible(entEQ);
        bool entComp = (newState == CoachRoomState::Compression);
        coachPanel_.getCompressionPanel().setVisible(entComp);
        bool entSpace = (newState == CoachRoomState::Space);
        coachPanel_.getSpacePanel().setVisible(entSpace);
        bool entAuto = (newState == CoachRoomState::Automation);
        coachPanel_.getAutomationPanel().setVisible(entAuto);
        coachPanel_.getMasterCheckPanel().setVisible(newState == CoachRoomState::MasterCheck);

        // â•â•â• Asegurar que el evidence panel refleje la fase actual â•â•â•
        // El EvidencePanel cambia su vista automÃ¡ticamente segÃºn el CoachRoomState
        // (VU para GainStaging, Spectrum para EQ, Crest para Compression, etc.)
        evidencePanel_.setCoachRoomState(newState);

        // Limpiar highlight del espectro al cambiar de fase
        if (newState != CoachRoomState::EQ)
            navShell_.setSpectrumHighlight(0.0f, {});
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  feedEvidenceForPhase â€” Alimenta evidence panel con datos en tiempo real
    //
    //  ExtraÃ­do de NavigationShell::timerCallback() (~50 lÃ­neas).
    //  Se llama desde NavigationShell::timerCallback() cada ~15 ticks.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::feedEvidenceForPhase(AudioAnalyzer& analyzer, CoachEngine& coach)
    {
        auto& evidence = evidencePanel_;
        if (!evidence.isVisible()) return;

        CoachRoomState cs = navShell_.getCoachRoomState();

        if (cs == CoachRoomState::Compression || cs == CoachRoomState::Balance) {
            // Calcular promedios de tracks activos
            float sumPeak = 0.0f, sumRms = 0.0f, sumCrest = 0.0f;
            int activeCount = 0;
            auto& registry = sharedData_.getSlotRegistry();
            for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
                auto info = registry.getSlotInfo(i);
                if (info.active) {
                    auto tr = sharedData_.getTrackAudioResult(i);
                    sumPeak += juce::jmax(tr.peakLeft, tr.peakRight);
                    sumRms += juce::jmax(tr.rmsLeft, tr.rmsRight);
                    sumCrest += tr.crestPerBand[0];
                    activeCount++;
                }
            }
            if (activeCount > 0) {
                float avgPeak = sumPeak / (float)activeCount;
                float avgRms = sumRms / (float)activeCount;
                float avgCrest = sumCrest / (float)activeCount;
                evidence.updateCrestData(avgPeak, avgRms, avgCrest);
            }
        }
        else if (cs == CoachRoomState::Space) {
            // Leer correlaciÃ³n y stereo width del master
            auto& masterAnalysis = analyzer.getMasterAnalysis();
            float correlation = masterAnalysis.getCorrelation();
            float stereoWidth = analyzer.getAvgStereoWidth();
            evidence.updateSpatialData(correlation, stereoWidth);
        }
        else if (cs == CoachRoomState::Automation || cs == CoachRoomState::MasterCheck) {
            // Leer match score desde el engine de coach
            float progress = coach.getPhaseManager().getPhaseProgress(
                coach.getPhaseManager().getCurrentPhase());
            evidence.updateMatchScore(progress);
        }

        // Siempre actualizar datos base del analyzer
        evidence.updateFromAnalyzer(analyzer);
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  handleCorrectionResult â€” Maneja reacciones UI tras verificar correcciÃ³n
    //
    //  ExtraÃ­do de NavigationShell::onCorrectionApplied (~50 lÃ­neas).
    //  Delega a CorrectionLearner, actualiza avatar y navegaciÃ³n segÃºn resultado.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::handleCorrectionResult(CorrectionCardData& data, bool /*correctionMode*/)
    {
        if (data.status >= CorrectionCardData::Status::Verified)
            return;

        navShell_.postUIEvent("[PHASE]",
            "Verificando correcci\xC3\xB3n en " + data.trackName + "...");

        // Delegar verificaciÃ³n a CorrectionLearner
        auto* coach = navShell_.getCoachEngine();
        if (coach != nullptr) {
            coach->getCorrectionLearner().verifyMixCorrection(
                data, sharedData_.getSlotRegistry(), sharedData_);

            // Reacciones UI segÃºn el resultado
            if (data.status == CorrectionCardData::Status::Verified) {
                navShell_.setAvatarExpression(AvatarExpression::Celebrating);
                navShell_.setAvatarNod(800);
                navShell_.postUIEvent("[DONE]",
                    "Verificaci\xC3\xB3n exitosa! Volviendo al chat...");
                navShell_.cancelAutoReturn();
                if (navShell_.getActiveTab() != TabBarComponent::Coach)
                    navShell_.setActiveTab(TabBarComponent::Coach);
            } else if (data.status >= CorrectionCardData::Status::Partial) {
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
            }
        } else {
            data.status = CorrectionCardData::Status::Verified;
            data.feedbackMessage = "Gracias! El cambio se nota en la mezcla.";
            navShell_.setAvatarExpression(AvatarExpression::Encouraging);
        }

        coachPanel_.repaint();
    }

    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    //  updateCoachingGuide â€” Actualiza CoachingGuideWidget con fase actual
    //
    //  ExtraÃ­do de NavigationShell (~65 lÃ­neas que aparecÃ­an 3 veces: en
    //  setCoachRoomState, onAdvanceStage, y onRequestHelp).
    //  Mapea MentorPhase â†’ CoachingStage y actualiza el widget con datos
    //  de progreso y sugerencias de la fase actual.
    // â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
    void CoachingNarrativeDirector::updateCoachingGuide()
    {
        auto* coach = navShell_.getCoachEngine();
        if (coach == nullptr) return;

        auto& phaseMgr = coach->getPhaseManager();
        MentorPhase mp = phaseMgr.getCurrentPhase();
        CoachingStage cs = CoachingStage::GainStaging;

        switch (mp) {
            case MentorPhase::GainStaging: cs = CoachingStage::GainStaging; break;
            case MentorPhase::Balance:     cs = CoachingStage::Balance; break;
            case MentorPhase::EQ:          cs = CoachingStage::EQ; break;
            case MentorPhase::Compresion:  cs = CoachingStage::Compression; break;
            case MentorPhase::Espacio:     cs = CoachingStage::Spatial; break;
            case MentorPhase::MasterCheck: cs = CoachingStage::Refinement; break;
            default:                       cs = CoachingStage::GainStaging; break;
        }

        // â•â•â• Automation override: MentorPhase no tiene Automation como fase separada,
        // pero CoachRoomState::Automation existe. Si estamos en Automation, forzar
        // CoachingStage::Automation directamente desde el estado de la UI.
        auto roomState = navShell_.getCoachRoomState();
        if (roomState == CoachRoomState::Automation) {
            cs = CoachingStage::Automation;
        }

        coachPanel_.getCoachingGuide().setStageDirectly(
            cs,
            phaseMgr.getPhaseProgress(mp),
            static_cast<int>(mp) / 7.0f,
            &coach->getPluginSuggestionsProvider());
    }

} // namespace mixcoach


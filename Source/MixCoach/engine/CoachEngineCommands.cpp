#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include <set>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  COMANDOS
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::executeCommand(const juce::String& command)
    {
        auto lower = command.toLowerCase();

        if (lower == "/next" || lower == "/avanzar") {
            // ═══ Usar CoachingStageManager.forceAdvance() si está inicializado ═══
            if (stageManager_.isInitialized()) {
                stageManager_.forceAdvance();
            } else {
                // Fallback al PhaseManager si el stage manager no está activo
                phaseManager_.advanceToNextPhase();
                auto phase = phaseManager_.getCurrentPhase();
                respondWith("[DONE] **Avanzando a fase:** " + juce::String(phaseNames[static_cast<int>(phase)]),
                            MentorMessage::Type::Achievement);
                respondWith("[NOTES] " + juce::String(phaseManager_.phaseDescription(phase)),
                            MentorMessage::Type::Tip);
            }
        }
        else if (lower == "/status" || lower == "/progreso") {
            auto issues      = collectAllIssues();
            juce::String ctx = buildSessionContextText(&issues);
            respondWith(ctx, MentorMessage::Type::Info);
        }
        else if (lower == "/analisis" || lower == "/analyze") {
            respondWith(
                "[SEARCH] Ejecutando an\xC3\xA1"
                "lisis completo de la mezcla...",
                MentorMessage::Type::Info);
            analyzeGainStagingReal();
            analyzeTonalBalanceReal();
            analyzeDynamicsReal();
            analyzePhaseReal();
            analyzeSpectralMaskingReal();
            analyzeOverallMixReal();

            // === Semantic analysis per role ===
            auto semanticDiffs = runSemanticAnalysis();
            if (!semanticDiffs.empty()) {
                int totalProblems = 0;
                int totalPraises  = 0;
                for (const auto& diff : semanticDiffs) {
                    totalProblems += diff.problemCount();
                    totalPraises += (int)diff.praises.size();
                }

                juce::String semMsg =
                    "\n[BRAIN] **An\xC3\xA1"
                    "lisis por rol** ("
                    + juce::String((int)semanticDiffs.size()) + " pistas analizadas):\n";
                for (const auto& diff : semanticDiffs) {
                    juce::String summary = diff.toTextSummary();
                    if (summary.isNotEmpty()) semMsg += "  [SEARCH] " + summary + "\n";
                }
                semMsg += "\n[CHART] **Total:** " + juce::String(totalProblems) + " problemas, "
                          + juce::String(totalPraises) + " aciertos";

                respondWith(semMsg, MentorMessage::Type::Info);
            }
            else {
                respondWith(
                    "[SEARCH] No se encontraron issues semanticos — las pistas estan en su rango esperado.",
                    MentorMessage::Type::Info);
            }

            // ═══ Plugin suggestions 3-tier por dominio ════════════════════
            {
                // Colectar issues para saber qué dominios están activos
                auto issues = collectAllIssues();
                std::set<juce::String> activeDomains;
                for (const auto& issue : issues) {
                    auto d = issue.domain.trim().toLowerCase();
                    if (d.isNotEmpty())
                        activeDomains.insert(d);
                }

                if (!activeDomains.empty()) {
                    // Enviar sugerencias de plugins para los dominios activos
                    for (const auto& domain : activeDomains) {
                        auto block = buildPluginSuggestionBlock(domain, {}, {}, 0.0f, 0.0f);
                        if (block.isNotEmpty()) {
                            juce::String header;
                            if (domain == "gain")
                                header = "[COACH] **Plugins para Gain Staging**";
                            else if (domain == "tonal" || domain == "spectral" || domain == "eq")
                                header = "[COACH] **Plugins para EQ / Balance Tonal**";
                            else if (domain == "dynamics" || domain == "dynamic")
                                header = "[TREND] **Plugins para Compresi\xC3\xB3n / Din\xC3\xA1mica**";
                            else if (domain == "phase" || domain == "spatial" || domain == "stereo")
                                header = "\xF0\x9F\x94\xAE **Plugins para Fase / Espacio**";
                            else if (domain == "masking")
                                header = "\xF0\x9F\x94\x8A **Plugins para Enmascaramiento**";
                            else
                                header = "\xF0\x9F\x92\xA1 **Plugins sugeridos para " + domain + "**";

                            respondWith(header + block, MentorMessage::Type::Tip);
                        }
                    }
                } else {
                    // Si no hay issues activos, mostrar sugerencias generales combinadas
                    // en UN solo mensaje para no saturar el chat
                    juce::String combinedMsg;
                    combinedMsg += "\xF0\x9F\x92\xA1 **Plugins recomendados para tu flujo de mezcla**\n\n"
                                  "Aqu\xC3\xAD tienes sugerencias organizadas por categor\xC3\xAD"
                                  "a para que siempre tengas a mano las herramientas adecuadas:";

                    auto gainBlock = buildPluginSuggestionBlock("gain", {}, {}, 0.0f, 0.0f);
                    auto tonalBlock = buildPluginSuggestionBlock("tonal", {}, {}, 0.0f, 0.0f);
                    auto dynamicsBlock = buildPluginSuggestionBlock("dynamics", {}, {}, 0.0f, 0.0f);
                    auto phaseBlock = buildPluginSuggestionBlock("phase", {}, {}, 0.0f, 0.0f);

                    if (gainBlock.isNotEmpty())
                        combinedMsg += "\n\n[COACH] **Gain Staging**" + gainBlock;
                    if (tonalBlock.isNotEmpty())
                        combinedMsg += "\n\n[COACH] **EQ / Balance Tonal**" + tonalBlock;
                    if (dynamicsBlock.isNotEmpty())
                        combinedMsg += "\n\n[TREND] **Compresi\xC3\xB3n / Din\xC3\xA1mica**" + dynamicsBlock;
                    if (phaseBlock.isNotEmpty())
                        combinedMsg += "\n\n\xF0\x9F\x94\xAE **Fase / Espacio**" + phaseBlock;

                    respondWith(combinedMsg, MentorMessage::Type::Tip);
                }
            }
        }
        else if (lower == "/historial" || lower == "/history") {
            int count = sharedData_.getMessageCount();
            int start = juce::jmax(0, count - 10);
            int shown = count - start;
            if (shown == 0) {
                respondWith("\xF0\x9F\x93\x9C No hay mensajes en el historial aun.", MentorMessage::Type::Info);
            }
            else {
                respondWith("\xF0\x9F\x93\x9C **Ultimos " + juce::String(shown) + " mensajes:**",
                            MentorMessage::Type::Info);
                for (int i = start; i < count; ++i) {
                    auto msg = sharedData_.getMessage(i);
                    juce::String prefix;
                    switch (msg.type) {
                        case MentorMessage::Type::Warning:
                            prefix = "\xF0\x9F\x94\xB4";
                            break;
                        case MentorMessage::Type::Tip:
                            prefix = "\xF0\x9F\x92\xA1";
                            break;
                        case MentorMessage::Type::Achievement:
                            prefix = "\xF0\x9F\x8E\x89";
                            break;
                        case MentorMessage::Type::Question:
                            prefix = "[QUESTION]";
                            break;
                        default:
                            prefix = "[NOTES]";
                            break;
                    }
                    juce::String text = juce::String(msg.text).substring(0, 120);
                    respondWith(prefix + " " + text, msg.type);
                }
            }
        }
        else if (lower == "/reset" || lower == "/reiniciar") {
            resetTrackStates();
            auto phase            = phaseManager_.getCurrentPhase();
            juce::String phaseStr = phaseNames[static_cast<int>(phase)];
            respondWith(
            "[PHASE] **Estados reiniciados** para la fase '" + phaseStr + "'.\\n"
            "Los cooldowns se han limpiado — los an\xC3\xA1" "lisis y advertencias "
            "empezar\xC3\xA1" "n desde cero.",
            MentorMessage::Type::Achievement);
        }
        else if (lower == "/phase" || lower == "/fase") {
            auto phase       = phaseManager_.getCurrentPhase();
            auto phaseStr    = juce::String(phaseNames[static_cast<int>(phase)]);
            auto progress    = phaseManager_.getPhaseProgress(phase);
            auto minTracks   = PhaseManager::minTracksForPhase(phase);
            auto active      = sharedData_.getSlotRegistry().activeCount();
            bool complete    = phaseManager_.isPhaseComplete(phase);
            int achievements = phaseManager_.getAchievementCount();
            respondWith(
            "\xF0\x9F\x97\xBA **Fase actual:** '" + phaseStr + "' — "
            + juce::String(phaseManager_.phaseDescription(phase)) + "\\n\\n"
            "[CHART] **Progreso:** " + juce::String(static_cast<int>(progress * 100.0f)) + "%\\n"
            "[TARGET] **Pistas m\xC3\xAD" "nimas requeridas:** " + juce::String(minTracks) + "\\n"
            "[COACH] **Pistas activas:** " + juce::String(active) + "\\n"
            "[TROPHY] **Logros:** " + juce::String(achievements) + "\\n"
            + (complete ? "[DONE] **Fase completada** — usa /next para avanzar" : "\xE2\x8F\xB3 **Fase en progreso**"),
            complete ? MentorMessage::Type::Achievement : MentorMessage::Type::Info);
        }
        else if (lower == "/map" || lower == "/mapa" || lower == "/session") {
            juce::String map = buildSessionMapText();
            if (map.isEmpty())
                respondWith("\xF0\x9F\x97\xBA No hay pistas activas para construir el mapa de sesi\xC3\xB3n.",
                            MentorMessage::Type::Info);
            else
                respondWith(map, MentorMessage::Type::Info);
        }
        else if (lower == "/export" || lower == "/reporte") {
            auto& registry = sharedData_.getSlotRegistry();
            int active     = registry.activeCount();
            if (active == 0) {
                respondWith("[EXPORT] No hay pistas activas para exportar.", MentorMessage::Type::Info);
            }
            else {
                juce::String report;
                report = "[EXPORT] **INFORME DE MEZCLA — MixCoach**\\n"
                     "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n\\n"
                     "\xF0\x9F\x93\x85 Generado: " + juce::String(juce::Time::getCurrentTime().formatted("%d/%m/%Y %H:%M")) + "\\n"
                     "[COACH] Pistas activas: " + juce::String(active) + "\\n"
                     "\xF0\x9F\x97\xBA Fase actual: " + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())]) + "\\n"
                     "[TROPHY] Logros: " + juce::String(phaseManager_.getAchievementCount()) + "\\n\\n"
                     "\xE2\x80\x94\xE2\x80\x94 Pistas \xE2\x80\x94\xE2\x80\x94\\n";

                registry.forEachActive([&](const SlotInfo& info) {
                    auto telem        = getLatestTelemetry(info.slotIndex);
                    juce::String name = juce::String(info.trackName).trim();
                    if (name.isEmpty()) name = "Pista " + juce::String(info.slotIndex + 1);
                    report += "\\n  " + name;
                    juce::String busName = "Sin bus";
                    if (info.bus >= BusType::Drums && info.bus <= BusType::FX) {
                        busName = juce::String(busNames[static_cast<int>(info.bus)]);
                    }
                    report += "  [Bus: " + busName + "]\\n";
                    if (telem.timestamp != 0) {
                        report += "    Peak: " + juce::String(telem.peakLeft, 1) + " dB | "
                              "RMS: " + juce::String(telem.rmsLeft, 1) + " dB\\n";
                        if (telem.crestFactor > 0.0f)
                            report += "    Crest: " + juce::String(telem.crestFactor, 1) + " dB | ";
                        if (telem.correlation < 1.0f) report += "Corr: " + juce::String(telem.correlation, 2);
                        report += "\\n";
                        if (telem.lufsMomentary > -80.0f)
                            report += "    LUFS: " + juce::String(telem.lufsMomentary, 1) + " (M) / "
                                      + juce::String(telem.lufsShortTerm, 1) + " (S)\\n";
                    }
                });

                report +=
                    "\\n\\n\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95"
                    "\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95"
                    "\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95"
                    "\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n"
                    "\xF0\x9F\x92\xA1 Consejo: copia este reporte y p\xC3\xA9"
                    "galo en tu DAW o "
                    "en tus notas de sesi\xC3\xB3"
                    "n para llevar registro.";

                respondWith(report, MentorMessage::Type::Info);
            }
        }
        else if (lower == "/estadisticas" || lower == "/stats") {
            auto& registry = sharedData_.getSlotRegistry();
            int active     = registry.activeCount();
            if (active == 0) {
                respondWith(
                    "[CHART] No hay pistas activas para generar estad\xC3\xAD"
                    "sticas.",
                    MentorMessage::Type::Info);
            }
            else {
                int clippingCount   = 0;
                int nearClipCount   = 0;
                int phaseIssueCount = 0;
                int bussedCount     = 0;
                int unnamedCount    = 0;
                float sumCrest      = 0.0f;
                int crestCount      = 0;
                float sumRms        = 0.0f;
                int rmsCount        = 0;

                registry.forEachActive([&](const SlotInfo& info) {
                    auto telem = getLatestTelemetry(info.slotIndex);
                    if (telem.timestamp == 0) return;
                    float peak = juce::jmax(telem.peakLeft, telem.peakRight);
                    if (peak > -0.5f) clippingCount++;
                    else if (peak > -3.0f)
                        nearClipCount++;
                    if (telem.correlation < 0.3f && telem.rmsLeft > -30.0f) phaseIssueCount++;
                    if (info.bus != BusType::None) bussedCount++;
                    juce::String name = juce::String(info.trackName).trim();
                    if (name.isEmpty() || name.startsWith("Pista")) unnamedCount++;
                    if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) {
                        sumCrest += telem.crestFactor;
                        crestCount++;
                    }
                    if (telem.rmsLeft > -60.0f) {
                        sumRms += (telem.rmsLeft + telem.rmsRight) * 0.5f;
                        rmsCount++;
                    }
                });

                juce::String stats;
                stats = "[CHART] **ESTAD\xC3\x8D" "STICAS DE LA SESI\xC3\x93" "N**\\n"
                    "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n\\n"
                    "[COACH] Pistas activas: " + juce::String(active) + "\\n"
                    "\xF0\x9F\x94\xA4 Sin nombre: " + juce::String(unnamedCount) + "\\n"
                    "\xF0\x9F\x94\x97 Con bus: " + juce::String(bussedCount) + "/" + juce::String(active) + "\\n\\n"
                    "[WARN] **Problemas detectados:**\\n"
                    "\xF0\x9F\x94\xB4 Clipping: " + juce::String(clippingCount) + " pistas\\n"
                    "\xF0\x9F\x9F\xA1 Near-clip: " + juce::String(nearClipCount) + " pistas\\n"
                    "\xF0\x9F\x94\xAE Fase baja (<0.3): " + juce::String(phaseIssueCount) + " pistas\\n\\n";

                if (crestCount > 0) {
                    float avgCrest = sumCrest / crestCount;
                    stats +=
                        "[TREND] **Rendimiento:**\\n"
                        "Crest factor promedio: "
                        + juce::String(avgCrest, 1) + " dB\\n";
                }
                if (rmsCount > 0) {
                    float avgRms = sumRms / rmsCount;
                    stats += "RMS promedio: " + juce::String(avgRms, 1) + " dB\\n";
                }

                stats +=
                    "\\n\\n\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95"
                    "\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95"
                    "\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95"
                    "\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n"
                    "Fase: "
                    + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())]);

                respondWith(stats, MentorMessage::Type::Info);
            }
        }
        else if (lower == "/ok" || lower == "/aplicado") {
            // Marcar la recomendación más urgente como "aplicada"
            // Usamos auto (no const auto*) para obtener el overload no-const
            TrackRecommendation* urgent = getMostUrgentRecommendation();
            if (urgent != nullptr && urgent->status == TrackRecommendation::Status::Pending) {
                urgent->status = TrackRecommendation::Status::Applied;
                respondWith("\xE2\x9C\x85 **Registrado!** Revisando si el cambio aplico el efecto deseado...",
                            MentorMessage::Type::Achievement);
            } else {
                respondWith("\xF0\x9F\x93\x8B No hay correcciones pendientes en este momento.",
                            MentorMessage::Type::Info);
            }
        }
        else if (lower == "/skip" || lower == "/saltar") {
            // Saltar la recomendación más urgente
            TrackRecommendation* urgent = getMostUrgentRecommendation();
            if (urgent != nullptr && urgent->status == TrackRecommendation::Status::Pending) {
                urgent->status = TrackRecommendation::Status::Ignored;
                respondWith("\xE2\x8F\xAD\xEF\xB8\x8F **Saltado.** Pasamos al siguiente problema.",
                            MentorMessage::Type::Info);
            } else {
                respondWith("\xF0\x9F\x93\x8B No hay recomendaciones pendientes para saltar.",
                            MentorMessage::Type::Info);
            }
        }
        else if (lower == "/why" || lower == "/porque") {
            // Explicar la recomendación más urgente en detalle
            auto* urgent = getMostUrgentRecommendation();
            if (urgent != nullptr && urgent->status == TrackRecommendation::Status::Pending) {
                juce::String explanation;
                explanation += "\xF0\x9F\x94\x8D **Explicación detallada:**\n\n";
                explanation += "**Pista:** " + urgent->trackName + "\n";
                explanation += "**Acción sugerida:** " + urgent->action + "\n";
                explanation += "**Dominio:** " + juce::String(TrackRecommendation::domainName(urgent->domain)) + "\n";
                explanation += "**Valor actual:** " + juce::String(urgent->beforeValue, 1) + "\n";
                explanation += "**Valor esperado:** " + juce::String(urgent->expectedAfter, 1) + "\n";
                explanation += "**Delta:** " + juce::String(urgent->delta, 2) + "\n\n";

                // Contexto adicional según dominio
                switch (urgent->domain) {
                    case TrackRecommendation::Domain::Gain:
                        explanation += "El nivel de esta pista esta fuera del rango optimo. ";
                        explanation += "Ajustar el fader de gain ayudara a que la pista se siente ";
                        explanation += "correctamente en la mezcla sin distorsionar ni perderse.";
                        break;
                    case TrackRecommendation::Domain::Tonal:
                        explanation += "El balance espectral de esta pista tiene un exceso o deficit ";
                        explanation += "en una region de frecuencia. Un ajuste de EQ ayudara a que ";
                        explanation += "la pista suene mas natural y se integre mejor con las demas.";
                        if (urgent->frequencyHz > 0.0f) {
                            explanation += "\n\n**Frecuencia clave:** " + juce::String(urgent->frequencyHz, 0) + " Hz";
                        }
                        break;
                    case TrackRecommendation::Domain::Dynamics:
                        explanation += "La dinamica de esta pista no esta en el rango esperado. ";
                        explanation += "Un compresor con los parametros adecuados ayudara a controlar ";
                        explanation += "los picos y mantener un nivel consistente.";
                        break;
                    case TrackRecommendation::Domain::Spatial:
                        explanation += "La correlacion estereo de esta pista indica un problema de fase. ";
                        explanation += "Ajustar el paneo o usar un plugin de fase puede resolverlo.";
                        break;
                }

                respondWith(explanation, MentorMessage::Type::Info);
            } else {
                respondWith("\xF0\x9F\x93\x8B No hay recomendaciones activas para explicar.",
                            MentorMessage::Type::Info);
            }
        }
        else if (lower == "/ref" || lower == "/referencia") {
            if (hasReference() && hasReferenceAudio()) {
                auto gaps = getReferenceGaps();
                float match = referenceProgress_.currentMatch;
                juce::String refMsg;
                refMsg += "\xF0\x9F\x8E\xAF **Comparacion con referencia:**\n\n";
                refMsg += "**Match actual:** " + juce::String(static_cast<int>(match * 100.0f)) + "%\n";
                refMsg += "**Referencia:** " + getReferenceName() + "\n";
                if (referenceProgress_.delta != 0.0f) {
                    refMsg += "**Tendencia:** " + juce::String(referenceProgress_.trendEmoji()) + " "
                              + juce::String(referenceProgress_.trendLabel()) + "\n";
                }
                refMsg += "**Gaps activos:** " + juce::String((int)gaps.size()) + "\n";

                if (!gaps.empty()) {
                    refMsg += "\n**Principales diferencias:**\n";
                    int maxShow = juce::jmin(3, (int)gaps.size());
                    for (int i = 0; i < maxShow; ++i) {
                        refMsg += "  \xE2\x80\xA2 " + gaps[i].metric + ": "
                                  + juce::String(gaps[i].actualValue, 1) + " (target: "
                                  + juce::String(gaps[i].targetValue, 1) + ")\n";
                    }
                }

                respondWith(refMsg, MentorMessage::Type::Info);
            } else {
                respondWith("\xF0\x9F\x93\x8B No hay referencia de audio cargada. ",
                            MentorMessage::Type::Info);
            }
        }
        else if (lower == "/help" || lower == "/ayuda") {
            respondWith(
                "**Comandos disponibles:**\\n"
                "/ok — Marcar la correccion actual como aplicada\\n"
                "/skip — Saltar la correccion actual\\n"
                "/why — Explicar la correccion actual en detalle\\n"
                "/next — Avanzar a la siguiente fase\\n"
                "/status — Ver progreso actual\\n"
                "/ref — Comparar con la referencia cargada\\n"
                "/an\xC3\xA1"
                "lisis — An\xC3\xA1"
                "lisis completo de la mezcla\\n"
                "/historial — \xC3\x9A"
                "ltimos mensajes del chat\\n"
                "/reset — Reiniciar estados y cooldowns de la fase actual\\n"
                "/phase — Informaci\xC3\xB3"
                "n detallada de la fase actual\\n"
                "/export — Generar informe de la mezcla\\n"
                "/estad\xC3\xAD"
                "sticas — Estad\xC3\xAD"
                "sticas de la sesi\xC3\xB3"
                "n\\n"
                "/map — Mostrar el Mapa de Sesi\xC3\xB3"
                "n (\xC3\xA1"
                "rbol de pistas por categor\xC3\xAD"
                "a)\\n"
                "/help — Mostrar esta ayuda\\n\\n"
                "**Atajos de teclado:**\\n"
                "Ctrl+Enter — /ok (aplicar correccion)\\n"
                "Ctrl+Shift+Enter — /skip (saltar correccion)\\n"
                "Ctrl+? — /why (explicar correccion)\\n\\n"
                "Tambi\xC3\xA9"
                "n puedes preguntar:\\n"
                "  \xE2\x80\xA2 \"\xC2\xB"
                "C\xC3\xB3"
                "mo va la mezcla?\"\\n"
                "  \xE2\x80\xA2 \"Hay clipping?\"\\n"
                "  \xE2\x80\xA2 \"Revisa din\xC3\xA1"
                "mica\"\\n"
                "  \xE2\x80\xA2 \"Problemas de fase?\"",
                MentorMessage::Type::Info);
        }
        else {
            respondWith("[QUESTION] Comando no reconocido. Escribe **/help** para ver los disponibles.",
                        MentorMessage::Type::Warning);
        }
    }

} // namespace mixcoach

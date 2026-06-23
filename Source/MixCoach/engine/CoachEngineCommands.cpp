#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  COMANDOS
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::executeCommand(const juce::String& command)
{
    auto lower = command.toLowerCase();

    if (lower == "/next" || lower == "/avanzar") {
        phaseManager_.advanceToNextPhase();
        auto phase = phaseManager_.getCurrentPhase();
        respondWith("\xE2\x9C\x85 **Avanzando a fase:** " + juce::String(phaseNames[static_cast<int>(phase)]),
                    MentorMessage::Type::Achievement);
        respondWith("\xF0\x9F\x93\x8B " + juce::String(phaseManager_.phaseDescription(phase)),
                    MentorMessage::Type::Tip);
    }
    else if (lower == "/status" || lower == "/progreso") {
        auto issues = collectAllIssues();
        juce::String ctx = buildSessionContextText(&issues);
        respondWith(ctx, MentorMessage::Type::Info);
    }
    else if (lower == "/analisis" || lower == "/analyze") {
        respondWith(
            "\xF0\x9F\x94\x8D Ejecutando an\xC3\xA1" "lisis completo de la mezcla...",
            MentorMessage::Type::Info);
        analyzeGainStagingReal();
        analyzeTonalBalanceReal();
        analyzeDynamicsReal();
        analyzePhaseReal();
        analyzeSpectralMaskingReal();
        analyzeOverallMixReal();

        // === Semantic analysis per role ===
        auto semanticDiffs = runSemanticAnalysis();
        if (!semanticDiffs.empty())
        {
            int totalProblems = 0;
            int totalPraises = 0;
            for (const auto& diff : semanticDiffs)
            {
                totalProblems += diff.problemCount();
                totalPraises += (int)diff.praises.size();
            }

            juce::String semMsg = "\n\xF0\x9F\xA7\xA0 **An\xC3\xA1" "lisis por rol** (" + juce::String((int)semanticDiffs.size())
                + " pistas analizadas):\n";
            for (const auto& diff : semanticDiffs)
            {
                juce::String summary = diff.toTextSummary();
                if (summary.isNotEmpty())
                    semMsg += "  \xF0\x9F\x94\x8D " + summary + "\n";
            }
            semMsg += "\n\xF0\x9F\x93\x8A **Total:** " + juce::String(totalProblems)
                + " problemas, " + juce::String(totalPraises) + " aciertos";

            respondWith(semMsg, MentorMessage::Type::Info);
        }
        else
        {
            respondWith(
                "\xF0\x9F\x94\x8D No se encontraron issues semanticos — las pistas estan en su rango esperado.",
                MentorMessage::Type::Info);
        }
    }
    else if (lower == "/historial" || lower == "/history") {
        int count = sharedData_.getMessageCount();
        int start = juce::jmax(0, count - 10);
        int shown = count - start;
        if (shown == 0) {
            respondWith("\xF0\x9F\x93\x9C No hay mensajes en el historial aun.",
                        MentorMessage::Type::Info);
        } else {
            respondWith("\xF0\x9F\x93\x9C **Ultimos " + juce::String(shown) + " mensajes:**",
                        MentorMessage::Type::Info);
            for (int i = start; i < count; ++i) {
                auto msg = sharedData_.getMessage(i);
                juce::String prefix;
                switch (msg.type) {
                    case MentorMessage::Type::Warning:    prefix = "\xF0\x9F\x94\xB4"; break;
                    case MentorMessage::Type::Tip:        prefix = "\xF0\x9F\x92\xA1"; break;
                    case MentorMessage::Type::Achievement: prefix = "\xF0\x9F\x8E\x89"; break;
                    case MentorMessage::Type::Question:   prefix = "\xE2\x9D\x93"; break;
                    default:                              prefix = "\xF0\x9F\x93\x8B"; break;
                }
                juce::String text = juce::String(msg.text).substring(0, 120);
                respondWith(prefix + " " + text,
                            msg.type);
            }
        }
    }
    else if (lower == "/reset" || lower == "/reiniciar") {
        resetTrackStates();
        auto phase = phaseManager_.getCurrentPhase();
        juce::String phaseStr = phaseNames[static_cast<int>(phase)];
        respondWith(
            "\xF0\x9F\x94\x84 **Estados reiniciados** para la fase '" + phaseStr + "'.\\n"
            "Los cooldowns se han limpiado — los an\xC3\xA1" "lisis y advertencias "
            "empezar\xC3\xA1" "n desde cero.",
            MentorMessage::Type::Achievement);
    }
    else if (lower == "/phase" || lower == "/fase") {
        auto phase = phaseManager_.getCurrentPhase();
        auto phaseStr = juce::String(phaseNames[static_cast<int>(phase)]);
        auto progress = phaseManager_.getPhaseProgress(phase);
        auto minTracks = PhaseManager::minTracksForPhase(phase);
        auto active = sharedData_.getSlotRegistry().activeCount();
        bool complete = phaseManager_.isPhaseComplete(phase);
        int achievements = phaseManager_.getAchievementCount();
        respondWith(
            "\xF0\x9F\x97\xBA **Fase actual:** '" + phaseStr + "' — "
            + juce::String(phaseManager_.phaseDescription(phase)) + "\\n\\n"
            "\xF0\x9F\x93\x8A **Progreso:** " + juce::String(static_cast<int>(progress * 100.0f)) + "%\\n"
            "\xF0\x9F\x8E\xAF **Pistas m\xC3\xAD" "nimas requeridas:** " + juce::String(minTracks) + "\\n"
            "\xF0\x9F\x8E\x9B **Pistas activas:** " + juce::String(active) + "\\n"
            "\xF0\x9F\x8F\x86 **Logros:** " + juce::String(achievements) + "\\n"
            + (complete ? "\xE2\x9C\x85 **Fase completada** — usa /next para avanzar" : "\xE2\x8F\xB3 **Fase en progreso**"),
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
        int active = registry.activeCount();
        if (active == 0) {
            respondWith("\xF0\x9F\x93\x84 No hay pistas activas para exportar.",
                        MentorMessage::Type::Info);
        } else {
            juce::String report;
            report = "\xF0\x9F\x93\x84 **INFORME DE MEZCLA — MixCoach**\\n"
                     "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n\\n"
                     "\xF0\x9F\x93\x85 Generado: " + juce::String(juce::Time::getCurrentTime().formatted("%d/%m/%Y %H:%M")) + "\\n"
                     "\xF0\x9F\x8E\x9B Pistas activas: " + juce::String(active) + "\\n"
                     "\xF0\x9F\x97\xBA Fase actual: " + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())]) + "\\n"
                     "\xF0\x9F\x8F\x86 Logros: " + juce::String(phaseManager_.getAchievementCount()) + "\\n\\n"
                     "\xE2\x80\x94\xE2\x80\x94 Pistas \xE2\x80\x94\xE2\x80\x94\\n";

            registry.forEachActive([&](const SlotInfo& info) {
                auto telem = getLatestTelemetry(info.slotIndex);
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
                    if (telem.correlation < 1.0f)
                        report += "Corr: " + juce::String(telem.correlation, 2);
                    report += "\\n";
                    if (telem.lufsMomentary > -80.0f)
                        report += "    LUFS: " + juce::String(telem.lufsMomentary, 1) + " (M) / "
                                  + juce::String(telem.lufsShortTerm, 1) + " (S)\\n";
                }
            });

            report += "\\n\\n\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n"
                      "\xF0\x9F\x92\xA1 Consejo: copia este reporte y p\xC3\xA9" "galo en tu DAW o "
                      "en tus notas de sesi\xC3\xB3" "n para llevar registro.";

            respondWith(report, MentorMessage::Type::Info);
        }
    }
    else if (lower == "/estadisticas" || lower == "/stats") {
        auto& registry = sharedData_.getSlotRegistry();
        int active = registry.activeCount();
        if (active == 0) {
            respondWith("\xF0\x9F\x93\x8A No hay pistas activas para generar estad\xC3\xAD" "sticas.",
                        MentorMessage::Type::Info);
        } else {
            int clippingCount = 0;
            int nearClipCount = 0;
            int phaseIssueCount = 0;
            int bussedCount = 0;
            int unnamedCount = 0;
            float sumCrest = 0.0f;
            int crestCount = 0;
            float sumRms = 0.0f;
            int rmsCount = 0;

            registry.forEachActive([&](const SlotInfo& info) {
                auto telem = getLatestTelemetry(info.slotIndex);
                if (telem.timestamp == 0) return;
                float peak = juce::jmax(telem.peakLeft, telem.peakRight);
                if (peak > -0.5f) clippingCount++;
                else if (peak > -3.0f) nearClipCount++;
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
            stats = "\xF0\x9F\x93\x8A **ESTAD\xC3\x8D" "STICAS DE LA SESI\xC3\x93" "N**\\n"
                    "\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n\\n"
                    "\xF0\x9F\x8E\x9B Pistas activas: " + juce::String(active) + "\\n"
                    "\xF0\x9F\x94\xA4 Sin nombre: " + juce::String(unnamedCount) + "\\n"
                    "\xF0\x9F\x94\x97 Con bus: " + juce::String(bussedCount) + "/" + juce::String(active) + "\\n\\n"
                    "\xE2\x9A\xA0 **Problemas detectados:**\\n"
                    "\xF0\x9F\x94\xB4 Clipping: " + juce::String(clippingCount) + " pistas\\n"
                    "\xF0\x9F\x9F\xA1 Near-clip: " + juce::String(nearClipCount) + " pistas\\n"
                    "\xF0\x9F\x94\xAE Fase baja (<0.3): " + juce::String(phaseIssueCount) + " pistas\\n\\n";

            if (crestCount > 0) {
                float avgCrest = sumCrest / crestCount;
                stats += "\xF0\x9F\x93\x88 **Rendimiento:**\\n"
                         "Crest factor promedio: " + juce::String(avgCrest, 1) + " dB\\n";
            }
            if (rmsCount > 0) {
                float avgRms = sumRms / rmsCount;
                stats += "RMS promedio: " + juce::String(avgRms, 1) + " dB\\n";
            }

            stats += "\\n\\n\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\\n"
                     "Fase: " + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())]);

            respondWith(stats, MentorMessage::Type::Info);
        }
    }
    else if (lower == "/help" || lower == "/ayuda") {
        respondWith(
            "**Comandos disponibles:**\\n"
            "/next — Avanzar a la siguiente fase\\n"
            "/status — Ver progreso actual\\n"
            "/an\xC3\xA1" "lisis — An\xC3\xA1" "lisis completo de la mezcla\\n"
            "/historial — \xC3\x9A" "ltimos mensajes del chat\\n"
            "/reset — Reiniciar estados y cooldowns de la fase actual\\n"
            "/phase — Informaci\xC3\xB3" "n detallada de la fase actual\\n"
            "/export — Generar informe de la mezcla\\n"
            "/estad\xC3\xAD" "sticas — Estad\xC3\xAD" "sticas de la sesi\xC3\xB3" "n\\n"
            "/map — Mostrar el Mapa de Sesi\xC3\xB3" "n (\xC3\xA1" "rbol de pistas por categor\xC3\xAD" "a)\\n"
            "/help — Mostrar esta ayuda\\n\\n"
            "Tambi\xC3\xA9" "n puedes preguntar:\\n"
            "  \xE2\x80\xA2 \"\xC2\xB" "C\xC3\xB3" "mo va la mezcla?\"\\n"
            "  \xE2\x80\xA2 \"Hay clipping?\"\\n"
            "  \xE2\x80\xA2 \"Revisa din\xC3\xA1" "mica\"\\n"
            "  \xE2\x80\xA2 \"Problemas de fase?\"",
            MentorMessage::Type::Info);
    }
    else {
        respondWith(
            "\xE2\x9D\x93 Comando no reconocido. Escribe **/help** para ver los disponibles.",
            MentorMessage::Type::Warning);
    }
}

} // namespace mixcoach

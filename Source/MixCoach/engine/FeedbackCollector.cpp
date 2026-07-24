#include "FeedbackCollector.h"
#include "CorrectionLearner.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  toShortReport — Resumen de una línea para FeedbackStats
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String FeedbackStats::toShortReport() const
    {
        juce::String s;
        s += juce::String(total) + " total: ";
        s += juce::String(static_cast<int>(acceptanceRate * 100.0f)) + "% accepted";
        s += ", " + juce::String(static_cast<int>(overRate * 100.0f)) + "% over";
        s += ", " + juce::String(static_cast<int>(underRate * 100.0f)) + "% under";
        s += ", " + juce::String(static_cast<int>(ignoreRate * 100.0f)) + "% ignored";
        if (trend != 0.0f) {
            s += " [trend: ";
            s += (trend > 0.02f ? "\\xE2\\x96\\xB2 improving" :
                  trend < -0.02f ? "\\xE2\\x96\\xBC declining" : "\\xE2\\x9E\\xA1 stable");
            s += "]";
        }
        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  recordOutcome — Registra una entrada de feedback
    // ═══════════════════════════════════════════════════════════════════════════
    void FeedbackCollector::recordOutcome(int slotIndex,
                                          const juce::String& trackName,
                                          int domain,
                                          int finalStatus,
                                          float appliedRatio,
                                          float beforeValue,
                                          float afterValue,
                                          bool hadFollowUp,
                                          int64_t ageUs)
    {
        FeedbackEntry entry;
        entry.timestampUs         = juce::Time::getMillisecondCounter() * 1000;
        entry.slotIndex           = slotIndex;
        entry.trackName           = trackName;
        entry.domainInt           = domain;

        // Map domain int to string
        switch (domain) {
            case 0: entry.domain = "gain";     break;
            case 1: entry.domain = "tonal";    break;
            case 2: entry.domain = "dynamics"; break;
            case 3: entry.domain = "spatial";  break;
            default: entry.domain = "unknown"; break;
        }

        entry.finalStatus        = finalStatus;
        entry.appliedRatio       = appliedRatio;
        entry.beforeValue        = beforeValue;
        entry.afterValue         = afterValue;
        entry.absDeltaDb         = std::abs(afterValue - beforeValue);
        entry.recommendationAgeUs = ageUs;
        entry.hadFollowUp        = hadFollowUp;

        // FIFO: remove oldest if at capacity
        if ((int)entries_.size() >= kMaxEntries)
            entries_.erase(entries_.begin());

        entries_.push_back(entry);

        LogHelper::writeToLog("[FeedbackCollector] Recorded: slot=" + juce::String(slotIndex)
                              + " domain=" + entry.domain
                              + " status=" + juce::String(finalStatus)
                              + " ratio=" + juce::String(appliedRatio, 2));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeStats — Computa FeedbackStats desde un rango de entradas
    // ═══════════════════════════════════════════════════════════════════════════
    FeedbackStats FeedbackCollector::computeStats(int startIdx, int count) const noexcept
    {
        FeedbackStats stats;
        int endIdx = std::min(startIdx + count, (int)entries_.size());
        if (startIdx >= (int)entries_.size() || count <= 0) return stats;

        int64_t totalAgeUs = 0;
        int ageCount = 0;

        for (int i = startIdx; i < endIdx; ++i) {
            const auto& e = entries_[i];
            stats.total++;

            // Domain breakdown
            if (e.domainInt >= 0 && e.domainInt < 4) {
                stats.perDomainTotal[e.domainInt]++;
                if (e.finalStatus == 1) // Applied
                    stats.perDomainAccepted[e.domainInt]++;
            }

            // Status counts
            switch (e.finalStatus) {
                case 1: // Applied
                    stats.accepted++;
                    break;
                case 2: // OverApplied
                    stats.overApplied++;
                    break;
                case 3: // UnderApplied
                    stats.underApplied++;
                    break;
                case 4: // Ignored
                case 5: // Superseded
                    stats.ignored++;
                    break;
                default:
                    break;
            }

            // Reaction time
            if (e.recommendationAgeUs > 0) {
                totalAgeUs += e.recommendationAgeUs;
                ageCount++;
            }
        }

        // ─── Compute ratios ────────────────────────────────────────────────
        if (stats.total > 0) {
            stats.acceptanceRate = static_cast<float>(stats.accepted) / stats.total;
            stats.overRate       = static_cast<float>(stats.overApplied) / stats.total;
            stats.underRate      = static_cast<float>(stats.underApplied) / stats.total;
            stats.ignoreRate     = static_cast<float>(stats.ignored) / stats.total;
        }

        if (ageCount > 0)
            stats.avgReactionTimeUs = totalAgeUs / ageCount;

        return stats;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getGlobalStats — Estadísticas de todas las entradas
    // ═══════════════════════════════════════════════════════════════════════════
    FeedbackStats FeedbackCollector::getGlobalStats() const noexcept
    {
        auto stats = computeStats(0, (int)entries_.size());

        // ─── Compute trend: compare last 10 vs 10 before that ──────────────
        int n = (int)entries_.size();
        if (n >= 20) {
            auto recent    = computeStats(n - 10, 10);
            auto previous  = computeStats(n - 20, 10);
            stats.recentAcceptance  = recent.acceptanceRate;
            stats.previousAcceptance = previous.acceptanceRate;
            stats.trend              = recent.acceptanceRate - previous.acceptanceRate;
        }
        else if (n >= 10) {
            auto recent    = computeStats(n - std::min(5, n), std::min(5, n));
            auto previous  = computeStats(0, std::min(5, n / 2));
            stats.recentAcceptance  = recent.acceptanceRate;
            stats.previousAcceptance = previous.acceptanceRate;
            stats.trend              = recent.acceptanceRate - previous.acceptanceRate;
        }

        return stats;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getDomainStats — Estadísticas filtradas por dominio
    // ═══════════════════════════════════════════════════════════════════════════
    FeedbackStats FeedbackCollector::getDomainStats(int domainInt) const noexcept
    {
        FeedbackStats stats;
        int64_t totalAgeUs = 0;
        int ageCount = 0;

        for (const auto& e : entries_) {
            if (e.domainInt != domainInt) continue;
            stats.total++;

            switch (e.finalStatus) {
                case 1: stats.accepted++;     break;
                case 2: stats.overApplied++;   break;
                case 3: stats.underApplied++;  break;
                case 4: case 5: stats.ignored++; break;
                default: break;
            }

            if (e.recommendationAgeUs > 0) {
                totalAgeUs += e.recommendationAgeUs;
                ageCount++;
            }
        }

        if (stats.total > 0) {
            stats.acceptanceRate = static_cast<float>(stats.accepted) / stats.total;
            stats.overRate       = static_cast<float>(stats.overApplied) / stats.total;
            stats.underRate      = static_cast<float>(stats.underApplied) / stats.total;
            stats.ignoreRate     = static_cast<float>(stats.ignored) / stats.total;
        }

        if (ageCount > 0)
            stats.avgReactionTimeUs = totalAgeUs / ageCount;

        return stats;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getRecentStats — Estadísticas de las últimas N entradas
    // ═══════════════════════════════════════════════════════════════════════════
    FeedbackStats FeedbackCollector::getRecentStats(int count) const noexcept
    {
        int n = (int)entries_.size();
        int startIdx = std::max(0, n - count);
        int actualCount = std::min(count, n - startIdx);
        return computeStats(startIdx, actualCount);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  hasSignificantData — ¿Hay suficientes datos para ajustar thresholds?
    // ═══════════════════════════════════════════════════════════════════════════
    bool FeedbackCollector::hasSignificantData() const noexcept
    {
        return (int)entries_.size() >= kMinEntriesForAdjustment;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  adjustLearnerThresholds — Ajusta CorrectionLearner según feedback
    //
    //  Analiza el historial de feedback y ajusta los thresholds de
    //  CorrectionLearner para adaptarse al comportamiento del usuario:
    //
    //    • Alta aceptación (>70%): usuario confía en las inferencias
    //      → Reducir kMinCorrectionsForBoost (de 2 → 1)
    //      → Aumentar kBoostPerCorrection (de 0.10 → 0.15)
    //      → Aumentar kMaxBoost (de 0.30 → 0.40)
    //      → Bajar threshold de matching espectral (más permisivo)
    //
    //    • Baja aceptación (<40%): usuario no confía en las inferencias
    //      → Aumentar kMinCorrectionsForBoost (de 2 → 3)
    //      → Reducir kBoostPerCorrection (de 0.10 → 0.05)
    //      → Reducir kMaxBoost (de 0.30 → 0.20)
    //      → Subir threshold de matching espectral (más estricto)
    //
    //    • Muchos OverApplied (>40%): usuario es agresivo
    //      → Reducir kBoostPerCorrection (evitar sobre-confianza)
    //      → Subir threshold espectral (más conservador)
    //
    //    • Muchos Ignored (>50%): usuario ignora recomendaciones
    //      → Aumentar kMinCorrectionsForBoost significativamente
    //      → Reducir kMaxBoost a mínimo
    // ═══════════════════════════════════════════════════════════════════════════
    void FeedbackCollector::adjustLearnerThresholds(CorrectionLearner& learner) const
    {
        if (!hasSignificantData()) return;

        auto global = getGlobalStats();

        // ═══ Capturar valores BEFORE antes de cualquier mutación ═════════════
        int minBefore   = learner.getEffectiveMinCorrectionsForBoost();
        float boostBefore   = learner.getEffectiveBoostPerCorrection();
        float maxBefore     = learner.getEffectiveMaxBoost();
        float spectralBefore = learner.getEffectiveSpectralMatchThreshold();

        // ─── Helper to record adjustment (captura before del closure) ─────
        auto recordAdjustment = [&](const juce::String& trigger,
                                    int newMin, float newBoost, float newMax, float newSpectral,
                                    bool reset)
        {
            ThresholdAdjustment adj;
            adj.timestampUs = juce::Time::getMillisecondCounter() * 1000;
            adj.trigger     = trigger;

            adj.minCorrectionsBefore   = minBefore;
            adj.minCorrectionsAfter    = newMin;
            adj.boostBefore            = boostBefore;
            adj.boostAfter             = newBoost;
            adj.maxBoostBefore         = maxBefore;
            adj.maxBoostAfter          = newMax;
            adj.spectralThresholdBefore = spectralBefore;
            adj.spectralThresholdAfter  = newSpectral;
            adj.wasReset               = reset;

            adjustmentHistory_.push_back(adj);
        };

        // ═══ Caso 1: Alta aceptación — usuario confía en el sistema ════════
        if (global.acceptanceRate > 0.70f) {
            learner.setMinCorrectionsForBoost(1);
            learner.setBoostPerCorrection(0.15f);
            learner.setMaxBoost(0.40f);
            learner.setSpectralMatchThreshold(0.70f);

            recordAdjustment(
                "High acceptance (" + juce::String(static_cast<int>(global.acceptanceRate * 100.0f)) + "%)",
                1, 0.15f, 0.40f, 0.70f, false);

            LogHelper::writeToLog(
                "[FeedbackCollector] Alta aceptacion (" + juce::String(static_cast<int>(global.acceptanceRate * 100.0f))
                + "%): ajustados thresholds (min=1, boost=0.15, max=0.40, spectral=0.70)");
        }
        // ═══ Caso 2: Baja aceptación — usuario no confía ═══════════════════
        else if (global.acceptanceRate < 0.40f) {
            learner.setMinCorrectionsForBoost(3);
            learner.setBoostPerCorrection(0.05f);
            learner.setMaxBoost(0.20f);
            learner.setSpectralMatchThreshold(0.85f);

            recordAdjustment(
                "Low acceptance (" + juce::String(static_cast<int>(global.acceptanceRate * 100.0f)) + "%)",
                3, 0.05f, 0.20f, 0.85f, false);

            LogHelper::writeToLog(
                "[FeedbackCollector] Baja aceptacion (" + juce::String(static_cast<int>(global.acceptanceRate * 100.0f))
                + "%): ajustados thresholds (min=3, boost=0.05, max=0.20, spectral=0.85)");
        }
        // ═══ Caso 3: Aceptación moderada — comportamiento mixto ══════════
        else {
            if (global.overRate > 0.40f) {
                learner.setBoostPerCorrection(0.07f);
                learner.setSpectralMatchThreshold(0.80f);

                recordAdjustment(
                    "Many over-applies (" + juce::String(static_cast<int>(global.overRate * 100.0f)) + "%)",
                    minBefore, 0.07f, maxBefore, 0.80f, false);

                LogHelper::writeToLog(
                    "[FeedbackCollector] Muchos over-applied (" + juce::String(static_cast<int>(global.overRate * 100.0f))
                    + "%): reducido boost=0.07, spectral=0.80");
            }
            if (global.ignoreRate > 0.50f) {
                learner.setMinCorrectionsForBoost(3);
                learner.setMaxBoost(0.20f);

                recordAdjustment(
                    "Many ignores (" + juce::String(static_cast<int>(global.ignoreRate * 100.0f)) + "%)",
                    3, boostBefore, 0.20f, spectralBefore, false);

                LogHelper::writeToLog(
                    "[FeedbackCollector] Muchos ignorados (" + juce::String(static_cast<int>(global.ignoreRate * 100.0f))
                    + "%): aumentado min=3, max-boost=0.20");
            }
            // Balanced behavior — reset to defaults
            if (global.overRate <= 0.40f && global.ignoreRate <= 0.50f) {
                learner.setMinCorrectionsForBoost(0);
                learner.setBoostPerCorrection(0.0f);
                learner.setMaxBoost(0.0f);
                learner.setSpectralMatchThreshold(0.0f);

                recordAdjustment(
                    "Balanced behavior — reset to defaults",
                    2, 0.10f, 0.30f, 0.75f, true);

                LogHelper::writeToLog(
                    "[FeedbackCollector] Comportamiento balanceado: thresholds reseteados a default");
            }
        }

        // ─── Per-domain adjustments (logged for analysis) ──────────────────
        for (int d = 0; d < 4; ++d) {
            auto domainStats = getDomainStats(d);
            if (!domainStats.hasData()) continue;

            const char* domainName = (d == 0) ? "gain" :
                                      (d == 1) ? "tonal" :
                                      (d == 2) ? "dynamics" : "spatial";

            if (domainStats.acceptanceRate > 0.75f) {
                LogHelper::writeToLog("[FeedbackCollector] Dominio '" + juce::String(domainName)
                                      + "' alta aceptacion: " + juce::String(static_cast<int>(domainStats.acceptanceRate * 100.0f))
                                      + "% -> reforzar inferencias de este dominio");
            }
            else if (domainStats.acceptanceRate < 0.30f && domainStats.total >= 3) {
                LogHelper::writeToLog("[FeedbackCollector] Dominio '" + juce::String(domainName)
                                      + "' baja aceptacion: " + juce::String(static_cast<int>(domainStats.acceptanceRate * 100.0f))
                                      + "% -> ser mas conservador en este dominio");
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Persistence
    // ═══════════════════════════════════════════════════════════════════════════
    void FeedbackCollector::toJson(juce::DynamicObject& obj) const
    {
        juce::Array<juce::var> entriesArray;
        for (const auto& e : entries_) {
            auto* entryObj = new juce::DynamicObject();
            entryObj->setProperty("timestampUs", static_cast<int64_t>(e.timestampUs));
            entryObj->setProperty("slotIndex", e.slotIndex);
            entryObj->setProperty("trackName", e.trackName);
            entryObj->setProperty("domain", e.domain);
            entryObj->setProperty("domainInt", e.domainInt);
            entryObj->setProperty("finalStatus", e.finalStatus);
            entryObj->setProperty("appliedRatio", e.appliedRatio);
            entryObj->setProperty("beforeValue", e.beforeValue);
            entryObj->setProperty("afterValue", e.afterValue);
            entryObj->setProperty("absDeltaDb", e.absDeltaDb);
            entryObj->setProperty("recommendationAgeUs", static_cast<int64_t>(e.recommendationAgeUs));
            entryObj->setProperty("hadFollowUp", e.hadFollowUp);
            entriesArray.add(juce::var(entryObj));
        }
        obj.setProperty("feedbackEntries", entriesArray);
    }

    void FeedbackCollector::fromJson(const juce::DynamicObject& obj)
    {
        entries_.clear();

        auto entriesVar = obj.getProperty("feedbackEntries");
        auto* entriesArray = entriesVar.getArray();
        if (!entriesArray) return;

        for (const auto& item : *entriesArray) {
            auto* entryObj = item.getDynamicObject();
            if (!entryObj) continue;

            FeedbackEntry e;
            e.timestampUs          = static_cast<int64_t>(entryObj->getProperty("timestampUs"));
            e.slotIndex            = entryObj->getProperty("slotIndex");
            e.trackName            = entryObj->getProperty("trackName").toString();
            e.domain               = entryObj->getProperty("domain").toString();
            e.domainInt            = entryObj->getProperty("domainInt");
            e.finalStatus          = entryObj->getProperty("finalStatus");
            e.appliedRatio         = static_cast<float>(static_cast<double>(entryObj->getProperty("appliedRatio")));
            e.beforeValue          = static_cast<float>(static_cast<double>(entryObj->getProperty("beforeValue")));
            e.afterValue           = static_cast<float>(static_cast<double>(entryObj->getProperty("afterValue")));
            e.absDeltaDb           = static_cast<float>(static_cast<double>(entryObj->getProperty("absDeltaDb")));
            e.recommendationAgeUs  = static_cast<int64_t>(entryObj->getProperty("recommendationAgeUs"));
            e.hadFollowUp          = entryObj->getProperty("hadFollowUp");

            entries_.push_back(e);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  dumpReport — Reporte completo de debug
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String FeedbackCollector::dumpReport() const
    {
        juce::String s;
        s += "=== FeedbackCollector Report ===\n";
        s += "Total entries: " + juce::String((int)entries_.size()) + "\n";
        s += "\n";

        auto global = getGlobalStats();
        s += "[Global] " + global.toShortReport() + "\n";
        s += "  Avg reaction time: " + juce::String(global.avgReactionTimeUs / 1000000) + "s\n";
        s += "  Trend: " + juce::String(global.trend, 3) + "\n";
        s += "\n";

        const char* domainNames[] = {"Gain", "Tonal", "Dynamics", "Spatial"};
        for (int d = 0; d < 4; ++d) {
            auto ds = getDomainStats(d);
            if (ds.hasData()) {
                s += "[" + juce::String(domainNames[d]) + "] " + ds.toShortReport() + "\n";
            }
        }

        s += "\n[Recent 10] ";
        auto recent = getRecentStats(10);
        s += recent.toShortReport() + "\n";

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toLLMContext — Resumen del feedback del usuario para el LLM
    //  Produce un bloque estructurado que se inyecta en buildSystemPrompt()
    //  para que el LLM adapte su tono según la tasa de aceptación.
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String FeedbackCollector::toLLMContext() const
    {
        if (!hasSignificantData()) return {};

        auto global = getGlobalStats();

        juce::String s;
        s += "[USER FEEDBACK BEHAVIOR]\n";

        // Tasa de aceptación global
        int acceptPct = static_cast<int>(global.acceptanceRate * 100.0f);
        s += "  Acceptance rate: " + juce::String(acceptPct) + "% (";
        if (global.acceptanceRate > 0.70f)        s += "high - user follows recommendations)";
        else if (global.acceptanceRate > 0.40f)   s += "moderate - user is selective)";
        else                                       s += "low - user is skeptical)";
        s += "\n";

        // Tendencia
        if (global.trend > 0.02f) {
            s += "  Trend: " + juce::String(static_cast<int>(global.trend * 100.0f)) + "% (improving)\n";
        }
        else if (global.trend < -0.02f) {
            s += "  Trend: " + juce::String(static_cast<int>(std::abs(global.trend) * 100.0f)) + "% (declining)\n";
        }
        else if (global.total >= 10) {
            s += "  Trend: stable\n";
        }

        // Reacciones (over/under/ignore)
        s += "  Over-applies: " + juce::String(static_cast<int>(global.overRate * 100.0f))
            + "% | Under-applies: " + juce::String(static_cast<int>(global.underRate * 100.0f))
            + "% | Ignores: " + juce::String(static_cast<int>(global.ignoreRate * 100.0f)) + "%\n";

        // Desglose por dominio
        {
            juce::String domainLine;
            const char* domainNames[] = {"Gain", "Tonal", "Dynamics", "Spatial"};
            for (int d = 0; d < 4; ++d) {
                auto ds = getDomainStats(d);
                if (!ds.hasData()) continue;
                if (domainLine.isNotEmpty()) domainLine += " | ";
                domainLine += juce::String(domainNames[d]) + ": "
                              + juce::String(static_cast<int>(ds.acceptanceRate * 100.0f)) + "%";
            }
            if (domainLine.isNotEmpty()) {
                s += "  By domain: " + domainLine + "\n";
            }
        }

        // Consejo para el LLM según el comportamiento
        s += "  Advice: ";
        if (global.acceptanceRate > 0.70f) {
            s += "User trusts your recommendations. Be confident and direct. Celebrate exact applications.\n";
            if (global.overRate > 0.40f) {
                s += "  Note: User tends to over-apply. Suggest SMALLER adjustments than usual (half the delta).\n";
            }
        }
        else if (global.acceptanceRate < 0.40f) {
            s += "User is skeptical. Be more conservative. Ask for confirmation before suggesting changes.\n";
            s += "  Note: Frame suggestions as questions: \"Que te parece si probamos...?\"\n";
        }
        else {
            s += "User is balanced but selective. Give precise numbers and explain why.\n";
            if (global.ignoreRate > 0.50f) {
                s += "  Note: User ignores many suggestions. Avoid repeating topics.\n";
            }
        }
        s += "\n";

        return s;
    }

} // namespace mixcoach

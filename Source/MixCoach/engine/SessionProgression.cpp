#include "SessionProgression.h"
#include "CoachEngine.h"
#include "../audio/AudioAnalyzer.h"
#include "RefinementProfile.h"
#include "ReferenceSummary.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Phase metadata — static arrays matching Phase enum order
    // ═══════════════════════════════════════════════════════════════════════════

    static const char* kPhaseNames[static_cast<int>(SessionProgression::Phase::COUNT)] = {
        "Setup",            // 0
        "Load Reference",   // 1
        "Deep Analysis",    // 2
        "Guided Coaching",  // 3
        "Refinement",       // 4
        "Session Report",   // 5
        "Memory Saved"      // 6
    };

    static const char* kPhaseShortNames[static_cast<int>(SessionProgression::Phase::COUNT)] = {
        "Setup",
        "Reference",
        "Analysis",
        "Coaching",
        "Refine",
        "Report",
        "Done"
    };

    static const char* kPhaseIcons[static_cast<int>(SessionProgression::Phase::COUNT)] = {
        "[GEAR]",   // ⚙ — Setup
        "\xF0\x9F\x93\x80", // 🎀 — Reference
        "[SEARCH]", // 🔍 — Analysis
        "\xF0\x9F\x97\xA3", // 🗣 — Coaching
        "[ART]", // 🎨 — Refinement
        "[EXPORT]", // 📄 — Report
        "\xF0\x9F\x92\xBE"  // 💾 — Memory
    };

    static const char* kPhaseEmojis[static_cast<int>(SessionProgression::Phase::COUNT)] = {
        "[GEAR]",
        "\xF0\x9F\x93\x80",
        "[SEARCH]",
        "\xF0\x9F\x97\xA3",
        "[ART]",
        "[EXPORT]",
        "\xF0\x9F\x92\xBE"
    };

    static const char* kPhaseDescriptions[static_cast<int>(SessionProgression::Phase::COUNT)] = {
        "Configuraci\\xC3\\xB3n inicial: selecciona g\\xC3\\xA9nero, modo y nombre del ingeniero",
        "Carga una referencia (archivo WAV/MP3 o URL de YouTube) para comparar",
        "An\\xC3\\xA1lisis espectral completo: energ\\xC3\\ADa, brillo, profundidad, g\\xC3\\A9nero inferido",
        "Coaching activo: el coach analiza, recomienda y verificas correcciones",
        "Refinamiento art\\xC3\\ADstico: profundidad, impacto, movimiento, pegamento, emoci\\xC3\\B3n",
        "Reporte final: puntajes, cambios, comparaci\\xC3\\B3n con referencia, evoluci\\xC3\\B3n multi-sesi\\xC3\\B3n",
        "Sesi\\xC3\\B3n guardada en el historial. Listo para la pr\\xC3\\B3xima sesi\\xC3\\B3n"
    };

    static const char* kPhaseTips[static_cast<int>(SessionProgression::Phase::COUNT)] = {
        "Escribe tu nombre, selecciona g\\xC3\\A9nero musical y el modo (Mix o Master)",
        "Arrastra un archivo de audio o pega una URL de YouTube en el panel Reference",
        "Espera unos segundos mientras se analiza la referencia en segundo plano",
        "Pregunta al coach: \\\"C\\xC3\\B3mo va la mezcla?\\\" o prueba las sugerencias r\\xC3\\A1pidas",
        "Cuando el MixScore supere 70, el coach hablar\\xC3\\A1 de calidad art\\xC3\\ADstica",
        "Haz clic en END SESSION para ver tu reporte completo",
        "El reporte y snapshot se guardan autom\\xC3\\A1ticamente. Puedes cerrar el plugin"
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  advanceTo
    // ═══════════════════════════════════════════════════════════════════════════
    bool SessionProgression::advanceTo(Phase newPhase, bool force)
    {
        int oldIdx = static_cast<int>(currentPhase);
        int newIdx = static_cast<int>(newPhase);

        if (newIdx < 0 || newIdx >= static_cast<int>(Phase::COUNT))
            return false;

        if (!force && newIdx <= oldIdx)
            return false; // No retroceder a menos que sea forzado

        Phase oldPhase = currentPhase;
        currentPhase = newPhase;
        markPhaseCompleted(newPhase);

        if (onPhaseChanged)
            onPhaseChanged(oldPhase, newPhase);

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  reset
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionProgression::reset()
    {
        Phase oldPhase = currentPhase;
        currentPhase = Phase::Setup;

        setupCompleted        = false;
        referenceLoaded       = false;
        deepAnalysisCompleted = false;
        coachingActive        = false;
        refinementAchieved    = false;
        reportGenerated       = false;
        memorySaved           = false;

        for (auto& ts : phaseStartedAtUs)
            ts = 0;

        if (onPhaseChanged && oldPhase != currentPhase)
            onPhaseChanged(oldPhase, currentPhase);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  isPhaseComplete
    // ═══════════════════════════════════════════════════════════════════════════
    bool SessionProgression::isPhaseComplete(Phase phase) const noexcept
    {
        switch (phase) {
            case Phase::Setup:         return setupCompleted;
            case Phase::LoadReference: return referenceLoaded;
            case Phase::DeepAnalysis:  return deepAnalysisCompleted;
            case Phase::GuidedCoaching: return coachingActive;
            case Phase::Refinement:    return refinementAchieved;
            case Phase::Report:        return reportGenerated;
            case Phase::Memory:        return memorySaved;
            default:                   return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getOverallProgress
    // ═══════════════════════════════════════════════════════════════════════════
    float SessionProgression::getOverallProgress() const noexcept
    {
        int currentIdx = static_cast<int>(currentPhase);
        int total      = static_cast<int>(Phase::COUNT) - 1; // 0-indexed, last=6

        if (total <= 0) return 1.0f;

        // Base: phase index / total phases
        float base = static_cast<float>(currentIdx) / static_cast<float>(total);

        // Bonus: dentro de la fase actual, añadir un pequeño % basado en completitud
        float bonus = 0.0f;
        switch (currentPhase) {
            case Phase::Setup:
                // Will be refined by caller if they know setup step
                bonus = setupCompleted ? 0.0f : 0.0f;
                break;
            case Phase::LoadReference:
                bonus = referenceLoaded ? 0.5f : 0.0f;
                break;
            case Phase::DeepAnalysis:
                bonus = deepAnalysisCompleted ? 0.5f : 0.0f;
                break;
            case Phase::GuidedCoaching:
                // More bonus as the mix progresses through MentorPhases
                // This is intentionally left simple - the caller can refine
                bonus = coachingActive ? 0.3f : 0.0f;
                break;
            case Phase::Refinement:
                bonus = refinementAchieved ? 0.5f : 0.0f;
                break;
            case Phase::Report:
                bonus = reportGenerated ? 0.5f : 0.0f;
                break;
            case Phase::Memory:
                bonus = 1.0f;
                break;
            default:
                break;
        }

        float stepSize = 1.0f / static_cast<float>(total);
        return juce::jlimit(0.0f, 1.0f, base + bonus * stepSize * 0.5f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPhaseDurationUs
    // ═══════════════════════════════════════════════════════════════════════════
    int64_t SessionProgression::getPhaseDurationUs() const noexcept
    {
        int idx = static_cast<int>(currentPhase);
        if (idx < 0 || idx >= static_cast<int>(Phase::COUNT))
            return 0;
        int64_t start = phaseStartedAtUs[idx];
        if (start <= 0)
            return 0;
        return juce::Time::getMillisecondCounter() * 1000 - start;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectPhase — Pure logic, no side effects
    // ═══════════════════════════════════════════════════════════════════════════
    SessionProgression::Phase
    SessionProgression::detectPhase(const CoachEngine& engine,
                                     const AudioAnalyzer& analyzer) noexcept
    {
        // ─── Check Memory first (saved session) ─────────────────────────────
        // We can't detect this from engine state alone; it's set explicitly.
        // Fall through to normal detection.

        // ─── Setup still in progress? ───────────────────────────────────────
        auto setupStep = engine.getSetupStep();
        if (setupStep != CoachEngine::SetupStep::Complete)
            return Phase::Setup;

        // ─── Reference loaded? ──────────────────────────────────────────────
        bool hasRef = engine.hasReference();
        bool hasRefAudio = engine.hasReferenceAudio();

        // ─── Reference summary valid? (deep analysis done) ──────────────────
        bool hasSummary = false;
        {
            auto summary = engine.getReferenceSummary();
            hasSummary = summary.valid;
        }

        // ─── Refinement profile relevant? (MixScore >= 70) ──────────────────
        bool isRefinement = false;
        {
            auto refine = engine.getCachedRefinementProfile();
            isRefinement = refine.valid && refine.isRelevant;
        }

        // ─── Coaching active? (recommendations flowing) ─────────────────────
        bool hasCoaching = false;
        {
            // Use correction history as the primary signal for active coaching.
            // Avoid calling non-const methods (collectAllIssues) through const ref.
            auto& history = engine.getCorrectionHistory();
            if (!history.empty())
                hasCoaching = true;

            // Secondary signal: check if MixScore has been computed (active tracks analyzed)
            // We check the number of active tracks — if there are tracks and issues have
            // been detected, coaching is likely active.
            if (!hasCoaching) {
                // Use the identity progress to determine if tracking has begun
                auto idProgress = engine.getIdentityProgress();
                if (idProgress.totalActive >= 2 && idProgress.identified >= 1)
                    hasCoaching = true;
            }
        }

        // ─── Phase detection logic (highest priority first) ────────────────
        if (hasSummary && isRefinement)
            return Phase::Refinement;

        if (hasSummary && hasCoaching)
            return Phase::GuidedCoaching;

        if (hasSummary)
            return Phase::DeepAnalysis;

        if (hasRef || hasRefAudio)
            return Phase::LoadReference;

        // Default: setup complete, no reference yet
        return Phase::Setup;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateFromEngine — Detect and auto-advance
    // ═══════════════════════════════════════════════════════════════════════════
    bool SessionProgression::updateFromEngine(const CoachEngine& engine,
                                               const AudioAnalyzer& analyzer)
    {
        Phase detected = detectPhase(engine, analyzer);

        if (detected == currentPhase)
            return false;

        // Only advance forward (never go backward from auto-detection)
        if (static_cast<int>(detected) <= static_cast<int>(currentPhase))
            return false;

        Phase oldPhase = currentPhase;
        currentPhase = detected;
        markPhaseCompleted(detected);

        if (onPhaseChanged)
            onPhaseChanged(oldPhase, detected);

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  markPhaseCompleted
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionProgression::markPhaseCompleted(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT) && phaseStartedAtUs[idx] == 0)
            phaseStartedAtUs[idx] = juce::Time::getMillisecondCounter() * 1000;

        switch (phase) {
            case Phase::Setup:         setupCompleted = true; break;
            case Phase::LoadReference: referenceLoaded = true; break;
            case Phase::DeepAnalysis:  deepAnalysisCompleted = true; break;
            case Phase::GuidedCoaching: coachingActive = true; break;
            case Phase::Refinement:    refinementAchieved = true; break;
            case Phase::Report:        reportGenerated = true; break;
            case Phase::Memory:        memorySaved = true; break;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Phase metadata accessors
    // ═══════════════════════════════════════════════════════════════════════════
    const char* SessionProgression::phaseName(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT))
            return kPhaseNames[idx];
        return "";
    }

    const char* SessionProgression::phaseShortName(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT))
            return kPhaseShortNames[idx];
        return "";
    }

    const char* SessionProgression::phaseIcon(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT))
            return kPhaseIcons[idx];
        return "";
    }

    const char* SessionProgression::phaseEmoji(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT))
            return kPhaseEmojis[idx];
        return "";
    }

    const char* SessionProgression::phaseDescription(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT))
            return kPhaseDescriptions[idx];
        return "";
    }

    const char* SessionProgression::phaseTip(Phase phase) noexcept
    {
        int idx = static_cast<int>(phase);
        if (idx >= 0 && idx < static_cast<int>(Phase::COUNT))
            return kPhaseTips[idx];
        return "";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  JSON serialization
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionProgression::toJson(juce::DynamicObject& obj) const
    {
        obj.setProperty("version", 1);
        obj.setProperty("currentPhase", static_cast<int>(currentPhase));

        obj.setProperty("setupCompleted", setupCompleted);
        obj.setProperty("referenceLoaded", referenceLoaded);
        obj.setProperty("deepAnalysisCompleted", deepAnalysisCompleted);
        obj.setProperty("coachingActive", coachingActive);
        obj.setProperty("refinementAchieved", refinementAchieved);
        obj.setProperty("reportGenerated", reportGenerated);
        obj.setProperty("memorySaved", memorySaved);

        // Serialize timestamps as JSON array
        juce::Array<juce::var> timestamps;
        for (int i = 0; i < static_cast<int>(Phase::COUNT); ++i)
            timestamps.add(static_cast<int64_t>(phaseStartedAtUs[i]));
        obj.setProperty("phaseStartedAtUs", timestamps);
    }

    SessionProgression SessionProgression::fromJson(const juce::DynamicObject& obj)
    {
        SessionProgression prog;

        if (obj.hasProperty("currentPhase")) {
            int phase = static_cast<int>(obj.getProperty("currentPhase"));
            if (phase >= 0 && phase < static_cast<int>(Phase::COUNT))
                prog.currentPhase = static_cast<Phase>(phase);
        }

        if (obj.hasProperty("setupCompleted"))        prog.setupCompleted = obj.getProperty("setupCompleted");
        if (obj.hasProperty("referenceLoaded"))       prog.referenceLoaded = obj.getProperty("referenceLoaded");
        if (obj.hasProperty("deepAnalysisCompleted")) prog.deepAnalysisCompleted = obj.getProperty("deepAnalysisCompleted");
        if (obj.hasProperty("coachingActive"))        prog.coachingActive = obj.getProperty("coachingActive");
        if (obj.hasProperty("refinementAchieved"))    prog.refinementAchieved = obj.getProperty("refinementAchieved");
        if (obj.hasProperty("reportGenerated"))       prog.reportGenerated = obj.getProperty("reportGenerated");
        if (obj.hasProperty("memorySaved"))           prog.memorySaved = obj.getProperty("memorySaved");

        if (obj.hasProperty("phaseStartedAtUs")) {
            auto arr = obj.getProperty("phaseStartedAtUs").getArray();
            if (arr != nullptr) {
                int n = juce::jmin(arr->size(), static_cast<int>(Phase::COUNT));
                for (int i = 0; i < n; ++i)
                    prog.phaseStartedAtUs[i] = static_cast<int64_t>((*arr)[i]);
            }
        }

        return prog;
    }

} // namespace mixcoach

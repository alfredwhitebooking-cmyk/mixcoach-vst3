#include "SectionDetector.h"

namespace mixcoach {

    SectionDetector::SectionDetector()
    {
        energyHistoryDb_.fill(-80.0f);
    }

    bool SectionDetector::analyzeFrame(float currentRmsDb,
                                        float currentLufs,
                                        float currentCorr,
                                        float currentCentroid,
                                        float songTimeSec,
                                        int64_t timestampUs)
    {
        // ═══ Throttle: solo cada ~2s ══════════════════════════════════════════
        if (timestampUs - lastAnalysisUs_ < kAnalysisIntervalUs)
            return false;
        lastAnalysisUs_ = timestampUs;

        // ═══ Guardar en historial (ring buffer) ═══════════════════════════════
        energyHistoryDb_[historyIndex_] = currentRmsDb;
        historyIndex_ = (historyIndex_ + 1) % kHistorySize;
        if (historyCount_ < kHistorySize)
            historyCount_++;

        // ═══ Detectar transición ══════════════════════════════════════════════
        float avgEnergy = getAverageEnergy();
        bool transition = detectTransition(currentRmsDb, avgEnergy);

        // ═══ Cooldown entre transiciones ══════════════════════════════════════
        hasRecentTransition_ = false;
        if (transition && (timestampUs - lastTransitionUs_ >= kTransitionCooldownUs)) {
            lastTransitionUs_ = timestampUs;

            // Guardar sección anterior
            previousSection_ = currentSection_;

            // Clasificar nueva sección
            SectionType newType = classifySection(currentRmsDb, songTimeSec);

            currentSection_.type             = newType;
            currentSection_.startTimeSec     = songTimeSec;
            currentSection_.avgEnergyDb      = currentRmsDb;
            currentSection_.avgSpectralCentroid = currentCentroid;
            currentSection_.avgCorrelation   = currentCorr;
            currentSection_.firstSeenUs      = timestampUs;

            hasRecentTransition_ = true;
            return true;
        }

        // ═══ Actualizar promedios incluso sin transición ═════════════════════
        if (!hasRecentTransition_) {
            // Smoothing exponencial suave
            constexpr float kSmoothing = 0.3f;
            currentSection_.avgEnergyDb = currentSection_.avgEnergyDb * (1.0f - kSmoothing)
                                          + currentRmsDb * kSmoothing;
            if (currentCentroid > 0.0f)
                currentSection_.avgSpectralCentroid =
                    currentSection_.avgSpectralCentroid * (1.0f - kSmoothing)
                    + currentCentroid * kSmoothing;
            currentSection_.avgCorrelation = currentSection_.avgCorrelation * (1.0f - kSmoothing)
                                             + currentCorr * kSmoothing;
        }

        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getSectionContext — Texto formateado para el LLM
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String SectionDetector::getSectionContext() const
    {
        if (!currentSection_.valid())
            return {};

        juce::String s;
        s += "[SECTION] ";
        s += juce::String(sectionTypeEmoji(currentSection_.type)) + " ";
        s += juce::String(sectionTypeName(currentSection_.type));
        s += " (desde " + juce::String(currentSection_.startTimeSec, 0) + "s";
        if (currentSection_.avgEnergyDb > -80.0f)
            s += ", ~" + juce::String(currentSection_.avgEnergyDb, 1) + " dB RMS";
        s += ")\n";

        // Contexto adicional según el tipo
        if (previousSection_.valid() && hasRecentTransition_) {
            s += "  Transicion: " + juce::String(sectionTypeEmoji(previousSection_.type))
                 + " " + juce::String(sectionTypeName(previousSection_.type))
                 + " → " + juce::String(sectionTypeEmoji(currentSection_.type))
                 + " " + juce::String(sectionTypeName(currentSection_.type)) + "\n";
        }

        return s;
    }

    void SectionDetector::reset()
    {
        energyHistoryDb_.fill(-80.0f);
        historyIndex_ = 0;
        historyCount_ = 0;
        currentSection_  = SectionInfo{};
        previousSection_ = SectionInfo{};
        hasRecentTransition_ = false;
        lastAnalysisUs_ = 0;
        lastTransitionUs_ = 0;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PRIVATE HELPERS
    // ═══════════════════════════════════════════════════════════════════════════

    float SectionDetector::getAverageEnergy() const
    {
        if (historyCount_ == 0)
            return -80.0f;

        float sum = 0.0f;
        int count = 0;
        for (int i = 0; i < historyCount_; ++i) {
            if (energyHistoryDb_[i] > -80.0f) {
                sum += energyHistoryDb_[i];
                count++;
            }
        }
        return (count > 0) ? (sum / static_cast<float>(count)) : -80.0f;
    }

    bool SectionDetector::detectTransition(float currentEnergy, float avgEnergy) const
    {
        if (historyCount_ < 2 || avgEnergy < -75.0f)
            return false;

        // Detectar cambio brusco en energía
        float ratio = 0.0f;
        if (std::abs(avgEnergy) > 0.01f) {
            // Usar diferencia absoluta normalizada
            float diffDb = currentEnergy - avgEnergy;
            ratio = std::abs(diffDb) / std::max(std::abs(avgEnergy), 1.0f);
        }

        return ratio > kTransitionThreshold;
    }

    SectionType SectionDetector::classifySection(float avgEnergyDb, float songTimeSec) const
    {
        // ═══ Primeros 10s de canción = Intro ═════════════════════════════════
        if (songTimeSec < 10.0f && avgEnergyDb < kLowEnergyThreshold + 5.0f)
            return SectionType::Intro;

        // ═══ Clasificar por nivel de energía ══════════════════════════════════
        if (avgEnergyDb > kHighEnergyThreshold)
            return SectionType::Chorus;

        if (avgEnergyDb < kLowEnergyThreshold) {
            // Baja energía: podría ser Intro, Bridge, o Outro
            // Si la canción está avanzada (>60%) y la energía bajó → Outro
            // Si estamos en una sección de baja energía rodeada de alta → Bridge
            if (songTimeSec > 90.0f && historyCount_ >= 3) {
                // Verificar si venimos de alta energía (transición reciente)
                // Esto se maneja mejor con el historial
                return SectionType::Outro;
            }
            return SectionType::Bridge;
        }

        // Energía media → Verse
        return SectionType::Verse;
    }

} // namespace mixcoach

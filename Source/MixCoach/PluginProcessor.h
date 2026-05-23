
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../Common/Types.h"
#include "../Common/SharedData.h"
#include "../Common/SlotRegistry.h"
#include "PhaseManager.h"
#include "CoachEngine.h"
#include "AudioAnalyzer.h"
#include "UI/MixCoachTheme.h"

namespace mixcoach {

// ─── MixCoach AudioProcessor (Cerebro) ──────────────────────────────────────
// IMPORTANTE: El constructor NO debe inicializar nada que pueda crashear
// durante el escaneo VST3 (sin SharedData, sin archivos, sin FFT).
// Toda inicialización pesada se hace en prepareToPlay().
class MixCoachAudioProcessor : public juce::AudioProcessor
{
public:
    MixCoachAudioProcessor();
    ~MixCoachAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MixCoach"; }

    bool acceptsMidi() const override    { return false; }
    bool producesMidi() const override   { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                                      { return 1; }
    int getCurrentProgram() override                                   { return 0; }
    void setCurrentProgram(int) override                               {}
    const juce::String getProgramName(int) override                    { return {}; }
    void changeProgramName(int, const juce::String&) override          {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Acceso a datos compartidos (puede ser nullptr si no initializado)
    SharedData* getSharedData() noexcept { return sharedData_; }

    // Analizador y coach (pueden ser nullptr si no initializados)
    AudioAnalyzer& getAudioAnalyzer() noexcept { return audioAnalyzer_; }
    CoachEngine*   getCoachEngine()   noexcept { return coachEngine_.get(); }
    PhaseManager*  getPhaseManager()  noexcept { return phaseManager_.get(); }

    // Inicializar shared data (llamado desde prepareToPlay)
    void ensureSharedData();

private:
    SharedData*                    sharedData_ = nullptr;
    AudioAnalyzer                  audioAnalyzer_;
    std::unique_ptr<PhaseManager>  phaseManager_;
    std::unique_ptr<CoachEngine>   coachEngine_;
    int64_t                        lastAnalysisTime_{0};

    // Logger de diagnóstico LOCAL (sin usar Logger global de JUCE)
    mutable std::unique_ptr<juce::FileOutputStream> logStream_;
    void logMessage(const juce::String& msg) const;
    void logCrash(const juce::String& msg) const;

    // Flag para saber si prepareToPlay fue llamado
    bool prepared_{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessor)
};

} // namespace mixcoach

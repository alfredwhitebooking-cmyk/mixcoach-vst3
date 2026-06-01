
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../engine/PhaseManager.h"
#include "../engine/CoachEngine.h"
#include "../audio/AudioAnalyzer.h"
#include "../ui/MixCoachTheme.h"

// Required for ChangeBroadcaster in the public interface
// (forward-declared in the class body below)

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

    // Inicializar shared data (llamado desde prepareToPlay y editor timer)
    void ensureSharedData();

    // Inicialización LIGERA de módulos (PhaseManager + CoachEngine).
    // A DIFERENCIA de ensureSharedData(), NO hace forceFullSync() ni
    // loadSlotsFromBackupFiles() — esas operaciones I/O pesadas se
    // delegan al background worker del editor.
    // Solo crea los objetos si shared memory está disponible y
    // los módulos no existen aún. Retorna true si se crearon.
    bool initBrainModules();

    // Resetear backoff de ensureSharedData (llamado por el editor cuando
    // detecta una reconexión exitosa de shared memory via backup scan)
    void resetEnsureBackoff() noexcept { ensureSharedDataAttempts_ = 0; lastEnsureAttemptTimeMs_ = 0; }

    // ─── ChangeBroadcaster para notificar al editor ─────────────────────
    // El editor se registra como listener de este broadcaster.
    // Cuando ensureSharedData() completa exitosamente o detecta cambios,
    // envía un change message para que el editor sepa que debe refrescar UI.
    juce::ChangeBroadcaster sharedDataChangeBroadcaster_;

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

    // ═══ Retry logic para ensureSharedData ═══════════════════════════════
    // Contador de intentos para logging de diagnóstico
    int ensureSharedDataAttempts_{0};
    // Timestamp del último intento (para backoff exponencial)
    uint32_t lastEnsureAttemptTimeMs_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessor)
};

} // namespace mixcoach

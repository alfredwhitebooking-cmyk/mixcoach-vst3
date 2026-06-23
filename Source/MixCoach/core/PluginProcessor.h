#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <atomic>
#include <cstdint>
// ═══ excpt.h: define EXCEPTION_EXECUTE_HANDLER para __try/__except ═════
// NO incluir windows.h completo — sus macros (min/max, __forceinline, etc.)
// conflictúan con los módulos DSP de JUCE (juce_SIMDNativeOps_fallback.h).
// excpt.h solo trae las constantes de Structured Exception Handling.
#ifdef _WIN32
#include <excpt.h>
#endif
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/audio/DiagnosticBridge.h"
#include "../engine/PhaseManager.h"
#include "../engine/CoachEngine.h"
#include "../ai/AiCoachAdapter.h"
#include "../audio/AudioAnalyzer.h"
#include "../ui/MixCoachTheme.h"
#include "ReferenceAudioPlayer.h"

namespace mixcoach {

// ─── MixCoach AudioProcessor (Cerebro) ──────────────────────────────────────
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

    SharedData* getSharedData() noexcept { return sharedData_; }

    AudioAnalyzer&    getAudioAnalyzer()    noexcept { return audioAnalyzer_; }
    CoachEngine*      getCoachEngine()      noexcept { return coachEngine_.get(); }
    PhaseManager*     getPhaseManager()     noexcept { return phaseManager_.get(); }
    AiCoachAdapter*   getAiCoachAdapter()   noexcept { return aiCoachAdapter_.get(); }
    ReferenceAudioPlayer& getRefPlayer() noexcept { return refPlayer_; }

    // ─── Persistencia de referencias (cache en processor, siempre disponible) ─
    void setPendingReferencePaths(const std::vector<juce::String>& files,
                                   const std::vector<juce::String>& urls)
    {
        pendingRefFilePaths_ = files;
        pendingRefURLs_ = urls;
    }
    [[nodiscard]] bool hasPendingReferences() const noexcept
    {
        return !pendingRefFilePaths_.empty() || !pendingRefURLs_.empty();
    }
    std::vector<juce::String> takePendingFilePaths()
    {
        return std::move(pendingRefFilePaths_);
    }
    std::vector<juce::String> takePendingURLs()
    {
        return std::move(pendingRefURLs_);
    }
    /** Cachea las rutas actuales desde el ReferencePanel (llamado en cada cambio). */
    void cacheReferencePaths(const std::vector<juce::String>& files,
                              const std::vector<juce::String>& urls)
    {
        pendingRefFilePaths_ = files;
        pendingRefURLs_ = urls;
    }
    /** Cachea las rutas para serialización (lee desde cache, siempre disponible). */
    const std::vector<juce::String>& getCachedFilePaths() const { return pendingRefFilePaths_; }
    const std::vector<juce::String>& getCachedURLs() const { return pendingRefURLs_; }

    void ensureSharedData();

    void setOllamaStatusCallback(std::function<void(bool, const juce::String&)> callback)
    {
        ollamaStatusCallback_ = std::move(callback);
    }
    bool initBrainModules();
    void resetEnsureBackoff() noexcept { ensureSharedDataAttempts_ = 0; lastEnsureAttemptTimeMs_ = 0; }

    /** Actualiza la API key del LlmClient en runtime. */
    void setApiKey(const juce::String& apiKey);

    /** Re-intenta conectar con Ollama (checkAvailability). */
    void retryOllamaConnection();

    /** Retorna true si el LlmClient est\xC3\xA1 disponible. */
    bool isLlmAvailable() const noexcept;

    /** Puente de diagnóstico para overlay visual en analizadores. */
    DiagnosticBridge diagnosticBridge_;
    [[nodiscard]] DiagnosticBridge& getDiagnosticBridge() noexcept { return diagnosticBridge_; }

    juce::ChangeBroadcaster sharedDataChangeBroadcaster_;

private:
    SharedData*                    sharedData_ = nullptr;
    AudioAnalyzer                  audioAnalyzer_;
    std::unique_ptr<PhaseManager>    phaseManager_;
    std::unique_ptr<CoachEngine>     coachEngine_;
    std::unique_ptr<AiCoachAdapter>  aiCoachAdapter_;
    std::unique_ptr<LlmClient>       llmClient_;
    ReferenceAudioPlayer             refPlayer_;

    mutable std::unique_ptr<juce::FileOutputStream> logStream_;
    void logMessage(const juce::String& msg) const;
    void logCrash(const juce::String& msg) const;

    bool prepared_{false};

    // ─── Referencias pendientes de restauración (desde setStateInformation) ─
    std::vector<juce::String> pendingRefFilePaths_;
    std::vector<juce::String> pendingRefURLs_;

    int ensureSharedDataAttempts_{0};
    uint32_t lastEnsureAttemptTimeMs_{0};

    // ─── API key configurada desde la UI ─────────────────────────────────
    juce::String apiKey_;

    // ─── Callback para actualizar la UI del estado de Ollama ────────────
    std::function<void(bool connected, const juce::String& modelName)> ollamaStatusCallback_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessor)
};

} // namespace mixcoach

#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SharedData.h"
#include "MessengerType.h"

namespace mixcoach {

    // ─── Messenger V3 — Sensor puro ───────────────────────────────────────────
    // Filosofía:
    //   "Soy un sensor. Solo transmito audio RAW + identidad."
    class MessengerAudioProcessor : public juce::AudioProcessor
    {
    public:
        MessengerAudioProcessor();
        ~MessengerAudioProcessor() override;

        // ─── Audio processing ────────────────────────────────────────────
        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        // ─── Editor ──────────────────────────────────────────────────────
        juce::AudioProcessorEditor* createEditor() override;

        bool hasEditor() const override { return true; }

        // ─── State ───────────────────────────────────────────────────────
        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        // ─── Name ────────────────────────────────────────────────────────
        const juce::String getName() const override { return "Messenger"; }

        // ─── MIDI / Programs (boilerplate JUCE) ──────────────────────────
        bool acceptsMidi() const override { return false; }

        bool producesMidi() const override { return false; }

        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }

        int getCurrentProgram() override { return 0; }

        void setCurrentProgram(int /*index*/) override {}

        const juce::String getProgramName(int /*index*/) override { return {}; }

        void changeProgramName(int /*index*/, const juce::String& /*newName*/) override {}

        // ─── APIs de identidad (Sensor: quién soy) ────────────────────────
        void setTrackName(const juce::String& newName);

        [[nodiscard]] juce::String getTrackName() const noexcept { return trackName_; }

        void setTrackType(TrackType type);

        [[nodiscard]] TrackType getTrackType() const noexcept { return trackType_; }

        void setTrackColour(const juce::Colour& newColour);

        [[nodiscard]] juce::Colour getTrackColour() const noexcept { return trackColour_; }

        void setBusAssignment(BusType bus);

        [[nodiscard]] BusType getBusAssignment() const noexcept { return busAssignment_; }

        void setFaderDb(float db);

        [[nodiscard]] float getFaderDb() const noexcept { return faderDb_; }

        void setPanValue(float pan);

        [[nodiscard]] float getPanValue() const noexcept { return panValue_; }

        void setMuted(bool mute);

        [[nodiscard]] bool isMuted() const noexcept { return muted_.load(std::memory_order_relaxed); }

        void setSoloed(bool solo);

        [[nodiscard]] bool isSoloed() const noexcept { return soloed_.load(std::memory_order_relaxed); }

        // Para el editor (solo identidad)
        [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }

        [[nodiscard]] SharedData* getSharedData() noexcept { return sharedData_; }

        [[nodiscard]] uint32_t getLastHeartbeatMs() const noexcept
        {
            return lastHeartbeatMs_.load(std::memory_order_relaxed);
        }

        void ensureSlotRegistered();

        // ═══ Sync trackType with SlotRegistry (V7 Identity Layer) ═══
        void syncTrackTypeToRegistry();

        // ═══ Feedback Loop V9: Leer TrackType inferido por MixCoach desde
        // shared memory y actualizar el estado local. Retorna true si hubo cambio.
        // MixCoach escribe el TrackType inferido al slot via updateSlotTrackType()
        // cuando detecta el rol de una pista (por nombre o espectro).
        // El Messenger lee ese cambio desde shared memory y actualiza su
        // ComboBox automaticamente, SIN que el usuario tenga que seleccionarlo.
        bool syncTrackTypeFromSharedMemory();

        // ═══ Name Auto-Suggestion V10: Sugerir TrackType desde el nombre ──────
        // Analiza el nombre que el usuario escribe y sugiere un TrackType
        // usando palabras clave (como inferTrackRoleFromName pero en el Messenger).
        // Retorna TrackType::None si no puede sugerir nada con confianza.
        static TrackType suggestTrackTypeFromName(const juce::String& name) noexcept;

        /** Actualiza el TrackType desde auto-sugerencia de nombre (sin marcar como pinned).
            El editor llama esto desde textEditorTextChanged() cuando detecta una
            coincidencia clara en el nombre. A diferencia de setTrackType(), NO marca
            trackTypePinned_ = true, permitiendo que futuros nombres sigan
            auto-sugiriendo nuevos tipos. */
        void setTrackTypeAutoSuggested(TrackType type);

        /** Indica si el usuario ha seleccionado manualmente el TrackType desde el ComboBox.
            Si es true, ni MixCoach ni la auto-sugerencia por nombre sobrescriben
            la seleccion del usuario. */
        [[nodiscard]] bool isTrackTypePinned() const noexcept { return trackTypePinned_; }

        // ═══ Audio Signal Detection V11: Detectar cuando el audio empieza a fluir ──
        // El Messenger detecta la transicion silencio->senal y usa esa informacion
        // para activar la inferencia de nombre en MixCoach via feedback loop.
        // CPU ultrabajo: solo revisa el primer sample de cada bloque cada ~100ms.
        // No hace FFT, no procesa audio — solo detecta presencia de senal.
        enum class SignalState : uint8_t
        {
            Waiting  = 0,
            Detected = 1
        };

        /** Retorna true si se ha detectado senal de audio en algun momento. */
        [[nodiscard]] bool hasAudioSignal() const noexcept
        {
            return audioSignalDetected_.load(std::memory_order_relaxed) == SignalState::Detected;
        }

        /** Resetea el estado de deteccion de senal (cuando el slot se registra de nuevo). */
        void resetAudioSignal() noexcept
        {
            audioSignalDetected_.store(SignalState::Waiting, std::memory_order_relaxed);
        }

        // ═══ Name Auto-Fill V11: Auto-llenar nombre desde TrackType inferido ──────
        // Cuando MixCoach infiere un TrackType (via Feedback Loop V9) y el nombre
        // de la pista aun es el generico "Pista X", auto-llena el nombre con el
        // nombre del instrumento detectado (ej: "Kick", "Snare", "Voz Principal").
        // @param suggestedType El TrackType inferido por MixCoach.
        // @return true si el nombre fue auto-llenado, false si ya tenia nombre.
        bool autoFillNameFromTrackType(TrackType suggestedType);

        /** Retorna true si el nombre aun es el generico de FL Studio. */
        [[nodiscard]] bool hasDefaultName() const noexcept
        {
            return trackName_.startsWith("Pista ") || trackName_.startsWith("Track ");
        }

    private:
        SharedData* sharedData_ = nullptr;
        int slotIndex_{-1};
        bool useStereo_ = true;  // default to stereo
        int rightSlotIndex_{-1}; // slot for right channel

        // ─── Estado de identidad ──────────────────────────────────────────
        juce::String trackName_;
        TrackType trackType_{TrackType::None};
        bool trackTypePinned_{false}; // true = usuario selecciono manualmente, no sobrescribir
        juce::Colour trackColour_{0xFF808080};
        BusType busAssignment_{BusType::None};

        // ─── Atómicos para UI (LEIDOS desde editor sin lock) ──────────────
        std::atomic<uint32_t> lastHeartbeatMs_{0};

        // ─── Slot management ──────────────────────────────────────────────
        bool prepared_{false};
        bool slotRegistered_{false};

        // ─── Audio Signal Detection V11 ──────────────────────────────────
        std::atomic<SignalState> audioSignalDetected_{SignalState::Waiting};

        // ─── FADER / PAN (V9) ───────────────────────────────────────────
        float faderDb_{0.0f};  // Fader level in dB (0.0 = unity)
        float panValue_{0.0f}; // Pan -1.0 (izq) to +1.0 (der), 0.0 = center

        // ─── MUTE / SOLO ──────────────────────────────────────────────────
        std::atomic<bool> muted_{false};
        std::atomic<bool> soloed_{false};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessor)
    };

} // namespace mixcoach

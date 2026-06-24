#pragma once
#include <juce_core/juce_core.h>
#include "../types/Types.h"
#include "../types/LogHelper.h"
#include "SlotRegistry.h"
#include "SharedMemory.h"
#include "SharedAudioMemoryV2.h"
#include "SharedAudioMemory.h"
#include <array>
#include <atomic>

namespace mixcoach {

    // ─── Resultado de análisis per-track desde SharedAudioMemory ───────────────
    // Computado por el background worker de MixCoach al leer audio RAW
    // del shared memory IPC. TrackTelemetry-like structure for CoachEngine.
    // Ahora con datos estéreo: peak/RMS por canal L/R.
    struct TrackAudioResult
    {
        float peakLeft      = -100.0f;
        float peakRight     = -100.0f;
        float rmsLeft       = -100.0f;
        float rmsRight      = -100.0f;
        float correlation   = 0.0f; // -1 (out of phase) to +1 (in phase), computed from L/R samples
        int64_t timestampUs = 0;

        // ═══ 30-band spectral energy per slot (from backgroundRunLoop FFT 1024) ═══
        // Defined in Constants.h (kSpectralBandBins / kNumSpectralBands)
        float bandEnergies[30] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};

        // ═══ High-level audio descriptors ═══
        float transientRatio        = 0.0f;   // 0.0 = sustained, >2.0 = transient-heavy
        float crestPerBand[6]       = {0.0f}; // Crest factor per frequency region
        float stereoWidthPerBand[6] = {0.0f}; // Stereo width per frequency region
        // ═══ Mid/Side energy per region (from bg worker Mid/Side decomposition) ═══
        float midEnergyPerBand[6]  = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        float sideEnergyPerBand[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        // ═══ Envelope descriptors (from bg worker envelope follower) ═══
        float attackTimeMs   = 0.0f;    // Estimated attack time in ms (0=silence, 1-5=percussive, 10+=slow)
        float releaseTimeMs  = 0.0f;    // Estimated release/decay time in ms
        float sustainLevelDb = -100.0f; // Sustained body level in dBFS

        // Helpers para compatibilidad: combinado (máximo de ambos canales)
        [[nodiscard]] float getPeakCombined() const noexcept { return juce::jmax(peakLeft, peakRight); }

        [[nodiscard]] float getRmsCombined() const noexcept { return juce::jmax(rmsLeft, rmsRight); }
    };

    // ─── Datos compartidos entre plugins ────────────────────────────────────────
    // Gestiona SharedMemoryManager (IPC identidad) + SharedAudioMemory (IPC audio RAW)
    // + SlotRegistry local + PerTrackAudioCache (análisis desde audio RAW compartido).
    // Cuando se usa como VST3 en un DAW, SharedMemoryManager sincroniza los datos
    // de identidad entre MixCoach y todos los Messengers via CreateFileMapping.
    // SharedAudioMemory provee un file mapping SEPARADO para audio RAW cross-process.
    class SharedData : public juce::ReferenceCountedObject
    {
    public:
        // ─── Singleton thread-safe ──────────────────────────────────────────
        static SharedData& getInstance()
        {
            static SharedData instance;
            return instance;
        }

        // ─── Singleton seguro contra excepciones ───────────────────────────
        static SharedData* safeGetInstance() noexcept
        {
            try {
                return &getInstance();
            }
            catch (const std::exception&) {
                return nullptr;
            }
            catch (...) {
                MIXCOACH_LOG_CATCH("SharedData::safeGetInstance");
                return nullptr;
            }
        }

        SharedData() noexcept;
        ~SharedData() override;

        // Verificar que la instancia está en estado usable
        [[nodiscard]] bool isAvailable() const noexcept { return shmInitialized_; }

        // Acceso al registro de slots
        SlotRegistry& getSlotRegistry() noexcept { return slotRegistry_; }

        const SlotRegistry& getSlotRegistry() const noexcept { return slotRegistry_; }

        // Acceso al gestor de memoria compartida (identidad)
        SharedMemoryManager& getSharedMemory() noexcept { return *shm_; }

        bool isSharedMemoryAvailable() const noexcept { return shm_ != nullptr && shm_->isInitialized(); }

        // ─── Acceso a memoria compartida de audio RAW (cross-process) ─────────
        SharedAudioMemory& getAudioMemory() noexcept { return audioMemory_; }

        const SharedAudioMemory& getAudioMemory() const noexcept { return audioMemory_; }

        // Nuevo acceso para audio estéreo
        SharedAudioMemoryV2& getAudioMemoryV2() noexcept;
        const SharedAudioMemoryV2& getAudioMemoryV2() const noexcept;

        // ─── Cache de análisis per-track desde SharedAudioMemory ───────────────
        // Computado por el background worker de MixCoach.
        // CoachEngine usa estos valores reales en vez de defaults.
        void updateTrackAudioResult(int slotIndex, const TrackAudioResult& result) noexcept
        {
            if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) trackAudioCache_[slotIndex].store(result);
        }

        [[nodiscard]] TrackAudioResult getTrackAudioResult(int slotIndex) const noexcept
        {
            if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) return trackAudioCache_[slotIndex].load();
            return TrackAudioResult{};
        }

        // ─── Reintentar inicialización de shared memory ───────────────────────
        bool retryInitSharedMemory();

        // Fase activa de mentoría
        void setCurrentPhase(MentorPhase phase) noexcept { currentPhase_ = phase; }

        MentorPhase getCurrentPhase() const noexcept { return currentPhase_; }

        // Mensajes del chat
        void pushMessage(const MentorMessage& msg);
        [[nodiscard]] int getMessageCount() const noexcept;
        [[nodiscard]] MentorMessage getMessage(int index) const;

    private:
        // ─── Contenedor atomic para TrackAudioResult (estéreo L/R) ─────────────
        struct AtomicAudioResult
        {
            std::atomic<float> peakLeftDb{-100.0f};
            std::atomic<float> peakRightDb{-100.0f};
            std::atomic<float> rmsLeftDb{-100.0f};
            std::atomic<float> rmsRightDb{-100.0f};
            std::atomic<float> correlation{0.0f};
            std::atomic<int64_t> timestampUs{0};
            // Band energies (30-band spectral data, defined in Constants.h)
            std::array<std::atomic<float>, 30> bandEnergyDbs{
                {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                 -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                 -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f}};
            // High-level descriptors
            std::atomic<float> transientRatio{0.0f};
            std::array<std::atomic<float>, 6> crestPerBand{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
            std::array<std::atomic<float>, 6> stereoWidthPerBand{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
            std::array<std::atomic<float>, 6> midEnergyPerBand{{-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f}};
            std::array<std::atomic<float>, 6> sideEnergyPerBand{{-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f}};
            // Envelope descriptors
            std::atomic<float> attackTimeMs{0.0f};
            std::atomic<float> releaseTimeMs{0.0f};
            std::atomic<float> sustainLevelDb{-100.0f};

            void store(const TrackAudioResult& r) noexcept
            {
                peakLeftDb.store(r.peakLeft, std::memory_order_release);
                peakRightDb.store(r.peakRight, std::memory_order_release);
                rmsLeftDb.store(r.rmsLeft, std::memory_order_release);
                rmsRightDb.store(r.rmsRight, std::memory_order_release);
                correlation.store(r.correlation, std::memory_order_release);
                timestampUs.store(r.timestampUs, std::memory_order_release);
                for (int b = 0; b < 30; ++b) bandEnergyDbs[b].store(r.bandEnergies[b], std::memory_order_release);
                transientRatio.store(r.transientRatio, std::memory_order_release);
                for (int b = 0; b < 6; ++b) {
                    crestPerBand[b].store(r.crestPerBand[b], std::memory_order_release);
                    stereoWidthPerBand[b].store(r.stereoWidthPerBand[b], std::memory_order_release);
                    midEnergyPerBand[b].store(r.midEnergyPerBand[b], std::memory_order_release);
                    sideEnergyPerBand[b].store(r.sideEnergyPerBand[b], std::memory_order_release);
                }
                attackTimeMs.store(r.attackTimeMs, std::memory_order_release);
                releaseTimeMs.store(r.releaseTimeMs, std::memory_order_release);
                sustainLevelDb.store(r.sustainLevelDb, std::memory_order_release);
            }

            TrackAudioResult load() const noexcept
            {
                TrackAudioResult r;
                r.peakLeft    = peakLeftDb.load(std::memory_order_acquire);
                r.peakRight   = peakRightDb.load(std::memory_order_acquire);
                r.rmsLeft     = rmsLeftDb.load(std::memory_order_acquire);
                r.rmsRight    = rmsRightDb.load(std::memory_order_acquire);
                r.correlation = correlation.load(std::memory_order_acquire);
                r.timestampUs = timestampUs.load(std::memory_order_acquire);
                for (int b = 0; b < 30; ++b) r.bandEnergies[b] = bandEnergyDbs[b].load(std::memory_order_acquire);
                r.transientRatio = transientRatio.load(std::memory_order_acquire);
                for (int b = 0; b < 6; ++b) {
                    r.crestPerBand[b]       = crestPerBand[b].load(std::memory_order_acquire);
                    r.stereoWidthPerBand[b] = stereoWidthPerBand[b].load(std::memory_order_acquire);
                    r.midEnergyPerBand[b]   = midEnergyPerBand[b].load(std::memory_order_acquire);
                    r.sideEnergyPerBand[b]  = sideEnergyPerBand[b].load(std::memory_order_acquire);
                }
                r.attackTimeMs   = attackTimeMs.load(std::memory_order_acquire);
                r.releaseTimeMs  = releaseTimeMs.load(std::memory_order_acquire);
                r.sustainLevelDb = sustainLevelDb.load(std::memory_order_acquire);
                return r;
            }
        };

        SlotRegistry slotRegistry_;
        std::unique_ptr<SharedMemoryManager> shm_;
        SharedAudioMemory audioMemory_;     // Mono audio IPC (legacy)
        SharedAudioMemoryV2 audioMemoryV2_; // Stereo audio IPC
        std::array<AtomicAudioResult, SlotRegistry::kMaxSlots> trackAudioCache_;
        MentorPhase currentPhase_{MentorPhase::Organizacion};
        static constexpr int kMaxMessages = 256;
        std::array<MentorMessage, kMaxMessages> messages_{};
        int messageCount_{0};
        bool shmInitialized_{false};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedData)
    };

} // namespace mixcoach

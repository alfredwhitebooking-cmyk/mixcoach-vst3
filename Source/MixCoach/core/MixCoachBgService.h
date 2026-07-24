#pragma once
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/audio/DiagnosticBridge.h"
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"

namespace mixcoach {

    // Forward declaration
    class MixCoachAudioProcessor;

    // ═══════════════════════════════════════════════════════════════════════════
    //  SlotSnapshot — results from one analysis cycle, readable by UI thread
    // ═══════════════════════════════════════════════════════════════════════════
    struct SlotSnapshot
    {
        int slotIndex = -1;
        float peakLeft = -100.0f, peakRight = -100.0f;
        float rmsLeft = -100.0f, rmsRight = -100.0f;
        float correlation = 0.0f;
        float attackTimeMs = 0.0f, releaseTimeMs = 0.0f, sustainLevelDb = -100.0f;
        float transientRatio = 0.0f;
        float bandEnergies[kNumSpectralBands]{};
        float crestPerBand[kNumRegions]{};
        float stereoWidthPerBand[kNumRegions]{};
        float midEnergyPerBand[kNumRegions]{};
        float sideEnergyPerBand[kNumRegions]{};
        uint32_t timestampMs = 0;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixCoachBgService — Background analysis service owned by PluginProcessor
    //
    //  Lives as long as the AudioProcessor. Runs a background thread that:
    //    1. Reads audio from SharedAudioMemoryV2 (per-slot ring buffers)
    //    2. Computes peak/RMS/correlation/envelope/FFT metrics
    //    3. Stores results via SharedData::updateTrackAudioResult()
    //    4. Publishes SlotSnapshot snapshots for the UI thread
    //    5. Handles forceFullSync, backup scan, health checks
    //    6. Runs reference analysis (heavy FFT) off the message thread
    //
    //  The UI (PluginEditor) never owns this thread — it only reads snapshots.
    // ═══════════════════════════════════════════════════════════════════════════
    class MixCoachBgService : private juce::Thread
    {
    public:
        MixCoachBgService();
        ~MixCoachBgService() override;

        /** Start the background thread. Requires sharedData to be ready.
            Safe to call multiple times — second call is a no-op. */
        void start(SharedData& sharedData);

        /** Stop the background thread. Called from ~PluginProcessor(). */
        void stop();

        // ─── Scheduling (thread-safe) ────────────────────────────────────────
        void requestFullSync() noexcept     { syncRequested_.store(true); }
        void requestBackupScan() noexcept   { backupRequested_.store(true); }

        /** Schedule reference analysis on the background thread. */
        void requestRefAnalysis(const juce::String& path);

        // ─── Read results (thread-safe) ──────────────────────────────────────
        int  getSyncResult()  const noexcept { return syncResult_.load(); }
        int  getBackupResult() const noexcept { return backupResult_.load(); }
        bool isShmHealthy()   const noexcept { return shmHealthy_.load(); }
        bool isShmReady()     const noexcept { return shmReady_.load(); }

        /** Returns a COPY of the latest snapshots (thread-safe). */
        std::vector<SlotSnapshot> getLatestSnapshots() const;

        /** Set callback for reference analysis (invoked from bg thread). */
        void setReferenceAnalysisCallback(std::function<void(const juce::String& path)> callback)
        {
            refAnalysisCallback_ = std::move(callback);
        }

        /** Iterate over active slots (thread-safe, uses internal lock).
            The callback receives a copy of SlotInfo for each active slot. */
        void forEachActiveSlot(std::function<void(const SlotInfo&)> callback) const;

        /** Returns alive status for diagnostics. */
        bool isRunning() const noexcept { return isThreadRunning(); }

    private:
        void run() override;

        /** One iteration of the background loop. */
        void iteration(int& loopCount, bool& initialSyncDone);

        // ─── Helpers ─────────────────────────────────────────────────────────
        static int safeForceSync(SlotRegistry& registry) noexcept;
        void ensureFFT();

        // ─── Per-slot envelope tracking ──────────────────────────────────────
        struct SlotEnvelopeTracker
        {
            float envLevel       = 0.0f;
            float envPeak        = 0.0f;
            float envFloor       = 0.0f;
            float attackSamples  = 0.0f;
            float releaseSamples = 0.0f;
            float sustainLevel   = 0.0f;
            float prevEnv        = 0.0f;
            int   cycleCount     = 0;
        };

        SharedData* sharedData_{nullptr};

        // ─── Scheduling atoms (written by UI thread, read by bg thread) ──────
        std::atomic<bool> syncRequested_{true};
        std::atomic<bool> backupRequested_{true};
        std::atomic<bool> refAnalysisRequested_{false};

        // ─── Result atoms (written by bg thread, read by UI thread) ──────────
        std::atomic<int>  syncResult_{0};
        std::atomic<int>  backupResult_{0};
        std::atomic<bool> shmHealthy_{true};
        std::atomic<bool> shmReady_{false};

        // ─── Reference analysis callback (set by Processor, invoked from bg thread) ──
        std::function<void(const juce::String& path)> refAnalysisCallback_;

        // ─── Reference analysis path (protected by bgLock_) ──────────────────
        juce::CriticalSection bgLock_;
        juce::String refAnalysisPath_;

        // ─── Timestamps (bg thread only) ─────────────────────────────────────
        uint32_t lastSyncMs_    = 0;
        uint32_t lastBackupMs_  = 0;
        uint32_t lastHealthMs_  = 0;

        // ─── Per-slot analysis state ─────────────────────────────────────────
        std::array<SlotEnvelopeTracker, SlotRegistry::kMaxSlots> envelopeTrackers_;
        std::array<float, SlotRegistry::kMaxSlots> crestAvgs_{};

        // ─── FFT ─────────────────────────────────────────────────────────────
        static constexpr int kFftOrder = 10;
        static constexpr int kFftSize  = 1 << kFftOrder;
        static constexpr int kReadSize = 4096; // Duplicado: de 2048 a 4096 para alcanzar throughput 48kHz

        std::unique_ptr<juce::dsp::FFT> slotFFT_;
        std::array<float, kFftSize>     hannWindow_{};
        bool fftPrepared_ = false;

        // ─── Snapshot bridge (written by bg thread, read by UI thread) ───────
        mutable juce::CriticalSection snapshotLock_;
        std::vector<SlotSnapshot> latestSnapshots_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachBgService)
    };

} // namespace mixcoach

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <memory>
#include "PluginProcessor.h"

#include "../ui/MainTabbedComponent.h"
#include "../ui/MixCoachTheme.h"
#include "../ui/SmoothValue.h"
#include "../../Common/memory/SharedData.h"

namespace mixcoach {

// Forward declaration (MixCoachBgWorker references MixCoachAudioProcessorEditor)
class MixCoachAudioProcessorEditor;

// Background worker thread class
// Defined in PluginEditorBackground.cpp alongside the worker logic
class MixCoachBgWorker : public juce::Thread
{
public:
    MixCoachBgWorker(MixCoachAudioProcessorEditor& editor);
    void run() override;
private:
    MixCoachAudioProcessorEditor& editor_;
};

// ─── MixCoach Plugin Editor — Inicialización segura ─────────────────────────
// CRÍTICO: sharedData_ puede ser nullptr durante la creación del editor.
// No bloqueamos el message thread de FL Studio esperando SharedData.
//
// ARQUITECTURA THREADING:
//   - Message thread (JUCE Timer): initSharedData ligera, actualizaciones UI
//   - Background thread (juce::Thread): forceFullSync, loadBackupFiles, healthCheck
//   - bgLock_ (CriticalSection) protege SlotRegistry entre ambos threads
//   - Atomic flags para comunicación scheduling/resultados
class MixCoachAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer,
                                     private juce::ChangeListener
{
public:
    MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData* sharedData);
    ~MixCoachAudioProcessorEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    // Called from background worker thread (MixCoachBgWorker)
    void backgroundRunLoop();

    // ═══ Safe bg iteration helper (evita MSVC C2712 con __try/__except) ═══
    // backgroundRunLoop() tiene el __try/__except y llama a este helper.
    // El helper tiene SOLO try/catch para C++ exceptions y toda la lógica.
    void bgIteration(int& bgLoopCount, bool& initialSyncDone);


    /** Expone el tabbed component para que el processor pueda recolectar referencias. */
    MainTabbedComponent* getTabbedComponent() const { return tabbedComponent_.get(); }

    /** True si la UI completa está construida. */
    bool isFullUIBuilt() const noexcept { return fullUIBuilt_; }

    /** Helper para timerCallback: contiene toda la logica del timer.
        Separado de timerCallback() para evitar MSVC C2712.
        timerCallback() tiene __try/__except y llama a este helper.
        NOTA: Este helper es una función LIBRE estática en PluginEditor.cpp
        (no un método de clase) porque la firma con 14 parámetros evita
        que el editor llame a una función miembro desde __try.
        Por eso initSharedData(), detectNewMessengers() son públicos. */

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void buildFullUI();

public:
    // ─── Públicos para safeTimerLogic (función libre en PluginEditor.cpp) ─
    void initSharedData();
    void detectNewMessengers();
    void notifyBgIterationResult(int syncFound, int backupFound, bool& initialSyncDone);

    /** Señaliza análisis de referencia al background worker.
        Thread-safe: escribe bgRefAnalysisPath_ bajo bgLock_ y setea flag atómico.
        Llamado desde safeTimerLogic() (message thread). */
    void signalBgRefAnalysis(const juce::String& path) noexcept {
        const juce::ScopedLock lock(bgLock_);
        bgRefAnalysisPath_ = path;
        bgRefAnalysisRequested_.store(true);
    }

private:

    // Callback seguro con SafePointer para evitar crash si el editor
    // es destruido mientras el ChangeBroadcaster tiene mensajes pendientes
    void handleChangeBroadcast();

    MixCoachAudioProcessor& processorRef_;
    SharedData*             sharedData_;

    // UI completa (creada LAZY cuando sharedData esté disponible)
    std::unique_ptr<MainTabbedComponent> tabbedComponent_;

    bool fullUIBuilt_{false};
    uint32_t lastInitAttemptMs_{0}; // backoff para reintentos

    int lastActiveSlotCount_{0};

    uint32_t uiBuiltTimeMs_{0};
    bool     initialSyncDone_{false};

    // Momento de creación del editor (para logging de diagnóstico)
    uint32_t editorCreatedMs_{0};
    
    // Safety timeout: solo se dispara una vez
    bool safetyTimeoutFired_{false};
    
    // Contador de ticks del timer (para logging a 30fps)
    int timerTickCount_{0};

    // ─── Background cached image (dot grid, dibujado una vez) ──────────────
    juce::Image bgCache_;
    bool bgCacheValid_ = false;
    int  lastCachedWidth_  = 0;
    int  lastCachedHeight_ = 0;

    void rebuildBgCache();

    // ═══ Flag antichoque: se marca true al inicio del destructor ═══════════
    // Todas las callbacks (timer, changeListener) deben verificar esta flag
    // y retornar inmediatamente si es true, para evitar accesos a miembros
    // parcialmente destruidos durante la eliminación del editor.
    //
    // ⚠ Debe ser std::atomic porque backgroundRunLoop() lo lee desde el
    //    background thread mientras el destructor (message thread) lo escribe.
    std::atomic<bool> editorBeingDestroyed_{false};

    // ─── Header interactive element bounds (for mouseDown hit detection) ───
    juce::Rectangle<int> headerTab1Bounds_;   // "AI COACH" tab
    juce::Rectangle<int> headerTab2Bounds_;   // "ANALYZERS" tab
    juce::Rectangle<int> headerVerifyBounds_; // "VERIFICAR PROGRESO" button
    juce::Rectangle<int> headerMenuIconBounds_;
    juce::Rectangle<int> headerHelpIconBounds_;
    juce::Rectangle<int> headerSettingsIconBounds_;
    int headerActiveTab_{0};                  // 0 = AI COACH, 1 = ANALYZERS

    // ═══ Header animation & hover state ══════════════════════════════════
    SmoothValue headerTabAnim_{0.0f, 5.0f, 150.0f}; // 0=tab1, 1=tab2
    int hoveredHeaderElement_{-1}; // -1=none, 0=tab1, 1=tab2, 2=verify, 3=menuIcon, 4=helpIcon, 5=settingsIcon

    // Placeholder que se muestra mientras se inicializa la UI
    juce::Label placeholderLabel_;

    // ═══ BACKGROUND WORKER (heavy I/O ops off message thread) ═══════════════
    // forceFullSync, loadSlotsFromBackupFiles, healthCheck se ejecutan
    // en un hilo separado para no bloquear el message thread de FL Studio.
    std::unique_ptr<juce::Thread> backgroundWorker_;
    
    // Mutex para proteger SlotRegistry entre message thread y background
    juce::CriticalSection bgLock_;
    
    // Scheduling flags (set by timer/message thread, consumed by bg worker)
    std::atomic<bool> bgForceSyncRequested_{true};  // initial sync inmediato
    std::atomic<bool> bgBackupScanRequested_{true};  // initial scan inmediato
    
    // Resultados del background worker (set por bg, leidos por timer)
    std::atomic<int>  bgForceSyncResult_{0};
    std::atomic<int>  bgBackupResult_{0};
    std::atomic<bool> bgHasNewResults_{false};
    std::atomic<bool> bgShmHealthy_{true};
    
    // Background thread timestamps (solo accedidos desde bg thread)
    uint32_t lastBgForceSyncMs_{0};
    uint32_t lastBgBackupScanMs_{0};
    uint32_t lastBgHealthCheckMs_{0};
    
    // Shared memory ready signal (set by bg, consumed by timer)
    // Cuando el background worker logra conectar shared memory,
    // el timer debe crear phaseManager/coachEngine desde message thread.
    std::atomic<bool> bgSharedMemoryReady_{false};

    // ═══ Background reference analysis (set by timer, consumed by bg worker) ═
    // safeTimerLogic() consume pendingReferencePath_ de CoachEngine y lo mueve
    // al background worker para que el FFT pesado no bloquee el UI thread.
    std::atomic<bool> bgRefAnalysisRequested_{false};
    juce::String      bgRefAnalysisPath_;     // Protegido por bgLock_
    
    // ═══ Flags estáticos: persisten entre recreaciones del editor ═════════
    // Cuando el usuario minimiza/restaura el plugin en FL Studio, el editor
    // se destruye y recrea. Estos flags evitan que se repita el sync inicial
    // y el anuncio de tracks ya conocidos.
    static std::atomic<bool> s_initialFullSyncDone_;
    static std::array<bool, SlotRegistry::kMaxSlots> s_announcedSlots_;

    // ═══ Ring buffer diagnostic CSV logging ═════════════════════════════
    // Logs per-slot peak comparison every background cycle (~100ms) to
    // a CSV file for offline analysis of ring buffer overflow.
    //
    // Rows are formatted into a string buffer inside the bgLock_ scope,
    // then flushed to disk in one batch AFTER the lock is released.
    //
    // Columns: timestampMs,slotIdx,truePeakL_db,capturedPeakL_db,diffL_db,
    //          truePeakR_db,capturedPeakR_db,diffR_db,availableSamples,nRead
    //
    // Max file size: 100MB (~1M rows) — beyond that, logging stops.
    static constexpr int64_t kRingbufferDiagMaxBytes = 100 * 1024 * 1024;

    juce::File ringbufferDiagCsv_;
    bool       ringbufferDiagCsvHeaderWritten_ = false;
    bool       ringbufferDiagCsvFull_          = false;

    // Appends a formatted CSV row to the given string buffer (no file I/O).
    // The caller must flush the buffer to disk outside the lock.
    void formatRingbufferDiagRow(juce::String& buffer,
                                 int slotIndex,
                                 float truePeakL, float truePeakR,
                                 float capturedPeakL, float capturedPeakR,
                                 int availableSamples, int numRead);


    // ═══ Per-slot envelope tracker (envelope follower + attack/release estimation) ══
    // Mantenido entre ciclos del bg worker para tracking cross-cycle de envolvente.
    struct SlotEnvelopeTracker {
        float envLevel       = 0.0f;   // Current smoothed envelope level (linear)
        float envPeak        = 0.0f;   // Peak envelope since last reset
        float envFloor       = 0.0f;   // Floor/ambient envelope level
        float attackSamples  = 0.0f;   // Smoothed attack time in samples
        float releaseSamples = 0.0f;   // Smoothed release time in samples
        float sustainLevel   = 0.0f;   // Smoothed sustain linear level
        float prevEnv        = 0.0f;   // Previous cycle's envelope (for slope)
        int   cycleCount     = 0;      // Cycles since valid data
    };
    std::array<SlotEnvelopeTracker, SlotRegistry::kMaxSlots> slotEnvelopeTrackers_;

    // ═══ Per-slot analysis state for high-level audio descriptors ═══════
    std::array<float, SlotRegistry::kMaxSlots> slotCrestAvgs_{};
    
    // ═══ FFT for per-track multi-band analysis (1024-point, consistent with master) ══
    static constexpr int kSlotFftOrder = 10;   // 1024-point FFT
    static constexpr int kSlotFftSize  = 1 << kSlotFftOrder;

    std::unique_ptr<juce::dsp::FFT> slotFFT_;
    std::array<float, kSlotFftSize> slotHann_;   // Precomputed Hann window on stack
    bool slotFFTPrepared_{false};
    void ensureSlotFFT();

    // ═══ Samples to read per slot per cycle (2x 1024-FFT for 3 overlapping windows) ═══
    static constexpr int kSlotReadSize  = 2048;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessorEditor)
};

} // namespace mixcoach

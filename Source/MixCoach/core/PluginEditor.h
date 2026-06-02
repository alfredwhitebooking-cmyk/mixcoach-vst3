#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "PluginProcessor.h"
#include "../ui/MainTabbedComponent.h"
#include "../ui/MixCoachTheme.h"
#include "../../Common/memory/SharedData.h"

namespace mixcoach {

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

    // Called from background worker thread (MixCoachBgWorker)
    void backgroundRunLoop();

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void initSharedData();
    void buildFullUI();
    void detectNewMessengers();

    // Callback seguro con SafePointer para evitar crash si el editor
    // es destruido mientras el ChangeBroadcaster tiene mensajes pendientes
    void handleChangeBroadcast();

    MixCoachAudioProcessor& processorRef_;
    SharedData*             sharedData_;

    // UI completa (creada LAZY cuando sharedData esté disponible)
    std::unique_ptr<MainTabbedComponent> tabbedComponent_;

    // Placeholder mientras sharedData no está disponible
    juce::Label placeholderLabel_;
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
    int headerActiveTab_{0};                  // 0 = AI COACH, 1 = ANALYZERS

    // Layout (header is painted, no versionLabel needed)

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
    
    // ═══ Flags estáticos: persisten entre recreaciones del editor ═════════
    // Cuando el usuario minimiza/restaura el plugin en FL Studio, el editor
    // se destruye y recrea. Estos flags evitan que se repita el sync inicial
    // y el anuncio de tracks ya conocidos.
    static std::atomic<bool> s_initialFullSyncDone_;
    static std::array<bool, SlotRegistry::kMaxSlots> s_announcedSlots_;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessorEditor)
};

} // namespace mixcoach

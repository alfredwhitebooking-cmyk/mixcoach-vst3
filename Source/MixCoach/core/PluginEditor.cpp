#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

// earlyCrashLog definida en PluginProcessor.cpp — C puro, sin JUCE
extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

// ═══ Flag static: el sync inicial solo corre UNA vez por vida del proceso DLL.
// Cuando el usuario cierra/reabre el plugin en FL Studio, el editor se
// recrea pero los datos ya están en SlotRegistry (singleton persistente).
// Este flag evita re-escanear backup files y shared memory en cada
// reapertura, eliminando el molesto "re-scan".
std::atomic<bool> MixCoachAudioProcessorEditor::s_initialFullSyncDone_{false};

// ─── Background Worker — Operaciones I/O pesadas en hilo separado ────────────
// Se ejecuta en un juce::Thread para no bloquear el message thread.
// Realiza: forceFullSync, loadSlotsFromBackupFiles, healthCheck.
// La comunicación con el message thread es via atomic flags y bgLock_ (mutex).
class MixCoachBgWorker : public juce::Thread
{
public:
    MixCoachBgWorker(MixCoachAudioProcessorEditor& editor)
        : juce::Thread("MixCoachBG"), editor_(editor) {}
    
    void run() override { editor_.backgroundRunLoop(); }
    
private:
    MixCoachAudioProcessorEditor& editor_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════
MixCoachAudioProcessorEditor::MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData* sharedData)
    : AudioProcessorEditor(&processor)
    , processorRef_(processor)
    , sharedData_(sharedData)
{
    earlyCrashLog("EDITOR", "Editor constructor INICIO");

    setSize(960, 640);
    setResizable(true, true);
    setResizeLimits(800, 500, 1920, 1440);
    earlyCrashLog("EDITOR2", "setSize/setResizable OK");

    // ─── Siempre mostrar placeholder primero ─────────────────────────────
    // CRÍTICO: NUNCA llamar buildFullUI() aquí, incluso si sharedData_ está
    // disponible. La creación de MainTabbedComponent + forceFullSync() pueden
    // bloquear el message thread de FL Studio, causando timeout y crasheo.
    //
    // La UI completa se construye desde initSharedData() en el primer tick
    // del timer (500ms después). FL Studio ya ha terminado de cargar el
    // plugin para entonces.
    placeholderLabel_.setText(
        juce::CharPointer_UTF8("\xF0\x9F\x94\x84 Inicializando MixCoach...\n\n"
                               "Conectando con el sistema compartido.\n"
                               "Esto toma solo un instante."),
        juce::dontSendNotification);
    placeholderLabel_.setFont(juce::Font(juce::FontOptions(18.0f)));
    placeholderLabel_.setJustificationType(juce::Justification::centred);
    placeholderLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(placeholderLabel_);
    earlyCrashLog("EDITOR3", "Placeholder/buildFullUI OK");

    // Version label (siempre visible)
    versionLabel_.setText("v1.0.0", juce::dontSendNotification);
    versionLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    versionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(versionLabel_);
    earlyCrashLog("EDITOR4", "Version label OK");

        // ═══ Timer a 60fps — Fast UI + Slow updates ─────────────────────
    // AHORA: Heavy I/O está en el background worker (backgroundRunLoop),
    // el timer SOLO hace lectura ligera de telemetry + actualización UI.
    //
    // - Cada tick (~16ms): actualiza barras de Messengers y spectrograph
    //   (solo lectura de shared memory y telemetry — operaciones rápidas)
    // - Cada 16 ticks (~266ms): full updateAllPanels (analyzers completos)
    // - Log cada ~5s
    //
    // Esto da movimiento fluido como un DAW profesional (60fps) sin
    // saturar el message thread, porque las operaciones I/O pesadas
    // están en el bg.
    announcedSlots_.fill(false);

    // ═══ Persistencia de Messengers entre sesiones del editor ═══════════
    // Cuando el usuario cierra y reabre el plugin en FL Studio, el editor
    // se destruye y recrea, pero el SlotRegistry (dentro del singleton
    // SharedData) persiste con todos los datos de Messengers.
    //
    // Si ya hicimos el sync inicial en esta vida del proceso, NO volvemos
    // a solicitar forceFullSync/loadBackupFiles. Simplemente leemos los
    // datos existentes del SlotRegistry local.
    //
    // Esto elimina el molesto "re-scan" cada vez que se abre MixCoach.
    // ═══ PERIODIC BACKUP SCAN: Siempre habilitado, incluso después del sync inicial ═══
    // El background worker re-escaneará backup files automáticamente cada ~4s
    // durante los primeros 60 segundos, luego cada ~15s. Esto detecta nuevos
    // Messengers que se cargan DESPUÉS del forceFullSync inicial.
    bgBackupScanRequested_.store(true);
    if (!s_initialFullSyncDone_.load()) {
        bgForceSyncRequested_.store(true);
    }

    startTimerHz(60);

    // Registrar el tiempo de creación del editor para el log de diagnóstico
    editorCreatedMs_ = juce::Time::getMillisecondCounter();

    // ═══ Start background worker para I/O pesada ─────────────────────────
    // Ejecuta forceFullSync, loadBackupFiles, healthCheck fuera del
    // message thread para no bloquear la UI de FL Studio.
    backgroundWorker_ = std::make_unique<MixCoachBgWorker>(*this);
    backgroundWorker_->startThread();
    earlyCrashLog("EDITOR6", "Background worker iniciado OK");

    // ═══ ChangeBroadcaster Listener ═══════════════════════════════════════
    processorRef_.sharedDataChangeBroadcaster_.addChangeListener(this);
    LogHelper::writeToLog("[MixCoachEditor] ChangeBroadcaster listener registrado");
}

// ═══════════════════════════════════════════════════════════════════════════
//  DESTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════
MixCoachAudioProcessorEditor::~MixCoachAudioProcessorEditor()
{
    // ═══ CRÍTICO: Marcar flag ANTES de cualquier cleanup ════════════════
    // Esto evita que callbacks pendientes accedan a miembros parcialmente
    // destruidos.
    editorBeingDestroyed_ = true;

    // ═══ 1. Detener BACKGROUND WORKER primero ═══════════════════════════
    // Esto asegura que no haya callbacks (onSlotChanged, etc.) ejecutándose
    // en el background thread mientras destruimos miembros.
    if (backgroundWorker_) {
        backgroundWorker_->signalThreadShouldExit();
        backgroundWorker_->notify(); // Wake up if sleeping in wait()
        backgroundWorker_->stopThread(5000); // Wait up to 5s
        backgroundWorker_ = nullptr;
    }

    // ═══ 2. Remover ChangeBroadcaster listener ═══════════════════════════
    processorRef_.sharedDataChangeBroadcaster_.removeChangeListener(this);

    // ═══ 3. Detener timer (ya no hay background thread que pueda
    //         disparar callbacks vía forceFullSync) ═══════════════════
    stopTimer();

    // ═══ 4. Limpiar callbacks del SlotRegistry ═══════════════════════════
    // Ahora es seguro porque el background thread ya se detuvo.
    if (sharedData_) {
        auto& registry = sharedData_->getSlotRegistry();
        registry.onSlotChanged    = nullptr;
        registry.onSlotRegistered = nullptr;
        registry.onSlotReleased   = nullptr;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  BACKGROUND RUN LOOP — Heavy I/O en hilo separado
// ═══════════════════════════════════════════════════════════════════════════
// Se ejecuta en MixCoachBgWorker (juce::Thread). No bloquea el message thread.
// Realiza forceFullSync (~1s), loadSlotsFromBackupFiles (~2s), healthCheck (~5s)
// con protección via bgLock_ (CriticalSection).
void MixCoachAudioProcessorEditor::backgroundRunLoop()
{
    earlyCrashLog("BG", "backgroundRunLoop START");
    int bgLoopCount = 0;
    bool initialSyncDone = false;

    while (backgroundWorker_ && !backgroundWorker_->threadShouldExit())
    {
        backgroundWorker_->wait(100); // Sleep 100ms entre ciclos

        if (editorBeingDestroyed_)
            break;

        if (sharedData_ == nullptr)
            continue;
        
        bgLoopCount++;
        if (bgLoopCount % 50 == 0) {
            earlyCrashLog("BG", "backgroundRunLoop alive");
        }

        uint32_t now = juce::Time::getMillisecondCounter();

        // ─── Scope de mutex para SlotRegistry ────────────────────────────
        {
            const juce::ScopedLock lock(bgLock_);

            // 1. forceFullSync: UNA SOLA VEZ al inicio, o si se solicita explícitamente
            //    (via bgForceSyncRequested_). NO se repite periódicamente.
            if (bgForceSyncRequested_.exchange(false))
            {
                lastBgForceSyncMs_ = now;
                auto& registry = sharedData_->getSlotRegistry();
                int found = registry.forceFullSync();
                if (found > 0 || !initialSyncDone) {
                    bgForceSyncResult_.store(found);
                    bgHasNewResults_.store(true);
                }
                if (!initialSyncDone) {
                    initialSyncDone = true;
                    LogHelper::writeToLog("[MixCoachEditor] BG: forceFullSync inicial completado (" + juce::String(found) + " slots)");
                }
            }

            // 2. loadSlotsFromBackupFiles: PERIÓDICO + bajo demanda
            //    ═══ CRITICAL FIX V3: Re-scaneo automático SIEMPRE con forceOverwrite=true ═══
            //    Versiones anteriores usaban forceOverwrite=false para los scans
            //    periódicos, lo que impedía detectar cambios de bus/name/color
            //    que el usuario hacía en el Messenger DESPUÉS del registro inicial.
            //    
            //    Ahora SIEMPRE usamos forceOverwrite=true para que los backup files
            //    (que el Messenger actualiza inmediatamente via saveSlotToBackupFile())
            //    siempre prevalezcan.
            //    
            //    Además se agregó syncFromShared() periódico para capturar datos
            //    de telemetría y FFT desde shared memory (canal IPC entre DLLs).
            {
                uint32_t elapsedSinceLastBackupScan = now - lastBgBackupScanMs_;
                bool longEnoughForPeriodicScan = elapsedSinceLastBackupScan >= 4000;
                bool forceScanRequested = bgBackupScanRequested_.exchange(false);

                if (forceScanRequested || longEnoughForPeriodicScan)
                {
                    lastBgBackupScanMs_ = now;
                    // ═══ CRITICAL: Siempre forceOverwrite=true ═══════════
                    // Los backup files tienen los DATOS MÁS RECIENTES escritos
                    // por el Messenger (procesa audio y actualiza backup en
                    // cada processBlock). Siempre deben prevalecer sobre el
                    // estado local de MixCoach.
                    auto& registry = sharedData_->getSlotRegistry();
                    int found = registry.loadSlotsFromBackupFiles(true);
                    if (found > 0) {
                        bgBackupResult_.store(found);
                        bgHasNewResults_.store(true);
                        LogHelper::writeToLog("[MixCoachEditor] BG: loadBackupFiles encontro " + juce::String(found) + " slots");
                    }

                    // ═══ syncFromShared periódico ═══════════════════
                    // Detecta cambios de metadatos (bus, nombre, color) y
                    // telemetría (FFT, LUFS) desde shared memory.
                    // Es el complemento a los backup files: mientras backup
                    // provee datos fiables de metadatos, shared memory provee
                    // datos de telemetría de alta frecuencia.
                    bool shmChanged = registry.syncFromShared();
                    if (shmChanged) {
                        LogHelper::writeToLog("[MixCoachEditor] BG: syncFromShared detecto cambios");
                        bgHasNewResults_.store(true);
                    }
                }
            }

            // 3. Health check de shared memory (cada ~10s para monitoreo)
            if (now - lastBgHealthCheckMs_ >= 10000)
            {
                lastBgHealthCheckMs_ = now;
                bool healthy = sharedData_->isSharedMemoryAvailable()
                            && sharedData_->getSharedMemory().healthCheck();
                bgShmHealthy_.store(healthy);
            }
        } // ScopedLock release

        // ─── Retry shared memory init (solo si no disponible aún) ──
        // Se reintenta cada ~5s con backoff, pero una vez conectado no se
        // vuelve a intentar a menos que el health check falle.
        if (sharedData_ != nullptr && !sharedData_->isAvailable())
        {
            uint32_t elapsedSinceLastRetry = now - (lastBgHealthCheckMs_ > 0 ? lastBgHealthCheckMs_ : 0);
            if (elapsedSinceLastRetry >= 5000)
            {
                earlyCrashLog("BG", "retryInitSharedMemory");
                bool reconnected = sharedData_->retryInitSharedMemory();
                if (reconnected) {
                    LogHelper::writeToLog("[MixCoachEditor] BG: shared memory reconectada");
                    bgSharedMemoryReady_.store(true);
                    // Programar un solo resync post-reconexión
                    bgForceSyncRequested_.store(true);
                    bgBackupScanRequested_.store(true);
                }
                lastBgHealthCheckMs_ = now; // Actualizar para evitar reintentos continuos
            }
        }
        
        // ─── Una vez completado el sync inicial, dormir más para CPU idle ──
        if (initialSyncDone && sharedData_->isAvailable() && bgShmHealthy_.load())
        {
            // Dormir 500ms en vez de 100ms cuando todo está estable
            backgroundWorker_->wait(400); // wait() adicional
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  changeListenerCallback — Despacha a handleChangeBroadcast con SafePointer
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &processorRef_.sharedDataChangeBroadcaster_)
        return;

    // ═══ Guardia antichoque ═══════════════════════════════════════════════
    // ChangeBroadcaster ya despacha en el message thread, y removeChangeListener
    // en el destructor previene callbacks dangling. Esta guardia es defensa extra.
    if (editorBeingDestroyed_)
        return;

    handleChangeBroadcast();
}

void MixCoachAudioProcessorEditor::handleChangeBroadcast()
{
    // ═══ Guardia antichoque ═══════════════════════════════════════════════
    if (editorBeingDestroyed_)
        return;

    LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: notificación recibida");

    // Refrescar sharedData_ desde el processor
    sharedData_ = processorRef_.getSharedData();

    if (sharedData_ != nullptr && sharedData_->isAvailable()) {
        LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: sharedData AHORA disponible, construyendo UI...");
        
        if (!fullUIBuilt_ || tabbedComponent_ == nullptr) {
            buildFullUI();
            if (tabbedComponent_) {
                tabbedComponent_->setBounds(getLocalBounds()
                    .withTrimmedTop(28));
                resized();
                repaint();
            }
        } else {
            // UI ya construida — solo actualizar todos los paneles (tryLock no-bloqueante)
            if (tabbedComponent_) {
                if (bgLock_.tryEnter()) {
                    double sr = processorRef_.getSampleRate();
                    auto& registry = sharedData_->getSlotRegistry();
                    tabbedComponent_->updateAllPanels(registry, sr);
                    bgLock_.exit();
                }
                repaint();
            }
        }
    } else {
        LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: sharedData sigue NO disponible");
        // Forzar initSharedData en el próximo tick del timer
        if (lastInitAttemptMs_ > 0) {
            lastInitAttemptMs_ = 0; // Reset backoff para reintentar inmediato
        }
    }
}

// ─── Inicialización LIGERA de SharedData (SIN bloquear message thread) ─────
// A DIFERENCIA de la versión anterior, esta función NO llama a
// processorRef_.ensureSharedData() porque ESA función hace operaciones
// I/O pesadas (forceFullSync, loadBackupFiles, retryInitSharedMemory).
//
// Esas operaciones pesadas se ejecutan en el background worker
// (backgroundRunLoop()). Este método solo:
//   1. Obtiene el singleton SharedData (trivial)
//   2. Si bgSharedMemoryReady_ == true, crea phaseManager/coachEngine
//   3. Construye la UI si es necesario
//   4. Envía ChangeBroadcaster al editor
void MixCoachAudioProcessorEditor::initSharedData()
{
    // ═══ Guardia antichoque ═══════════════════════════════════════════════
    if (editorBeingDestroyed_)
        return;

    // ═══ PASO 1: Obtener singleton SharedData si no lo tenemos ───────────
    // SharedData::getInstance() es thread-safe y siempre retorna la misma
    // instancia. No hace I/O ni bloquea.
    if (sharedData_ == nullptr) {
        LogHelper::writeToLog("[MixCoachEditor] initSharedData: obteniendo SharedData singleton...");
        sharedData_ = &SharedData::getInstance();
        LogHelper::writeToLog(juce::String("[MixCoachEditor] initSharedData: sharedData_=")
            + (sharedData_ == nullptr ? "NULL" : "OK"));
    }
    
    if (sharedData_ == nullptr)
        return; // Singleton no disponible (raro, pero protegemos)
    
    // ═══ PASO 2: ¿Shared memory disponible vía background worker? ───────
    // El background worker es el que hace retryInitSharedMemory.
    // Cuando lo logra, setea bgSharedMemoryReady_ = true.
    // Aquí en el message thread recogemos esa señal y creamos
    // phaseManager/coachEngine si es necesario.
    if (bgSharedMemoryReady_.exchange(false) && sharedData_->isAvailable())
    {
        LogHelper::writeToLog("[MixCoachEditor] initSharedData: BG shared memory lista!");
        
        // Crear PhaseManager y CoachEngine si no existen
        auto* phaseManager = processorRef_.getPhaseManager();
        auto* coachEngine  = processorRef_.getCoachEngine();
        
        if (phaseManager == nullptr)
        {
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: creando PhaseManager/CoachEngine...");
            
            // ═══ LLAMADA LIGERA: initBrainModules NO hace forceFullSync ═══
            // Crea PhaseManager y CoachEngine. El background worker
            // (backgroundRunLoop) se encarga de forceFullSync cada ~1s.
            processorRef_.initBrainModules();
            
            phaseManager = processorRef_.getPhaseManager();
            coachEngine  = processorRef_.getCoachEngine();
            
            if (phaseManager != nullptr)
            {
                LogHelper::writeToLog("[MixCoachEditor] initSharedData: PhaseManager/CoachEngine creados OK");
                processorRef_.sharedDataChangeBroadcaster_.sendChangeMessage();
            }
        }
        else
        {
            // PhaseManager ya existe, solo notificar al editor
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: PhaseManager ya existe, enviando ChangeBroadcaster");
            processorRef_.sharedDataChangeBroadcaster_.sendChangeMessage();
        }
    }
    
    // ═══ PASO 3: Construir UI si sharedData disponible y UI no construida ─
    if (sharedData_ != nullptr && sharedData_->isAvailable() 
        && (!fullUIBuilt_ || tabbedComponent_ == nullptr))
    {
        LogHelper::writeToLog("[MixCoachEditor] initSharedData: sharedData OK, construyendo UI...");
        buildFullUI();
        if (tabbedComponent_) {
            tabbedComponent_->setBounds(getLocalBounds()
                .withTrimmedTop(36));
            resized();
            repaint();
        }
    }
}


// ─── Construir UI completa (solo cuando sharedData está disponible) ─────────
void MixCoachAudioProcessorEditor::buildFullUI()
{
    // ═══ Guardia antichoque ═══════════════════════════════════════════════
    if (editorBeingDestroyed_)
        return;

    if (fullUIBuilt_ || sharedData_ == nullptr) {
        LogHelper::writeToLog("[MixCoachEditor] buildFullUI SKIPPED: fullUIBuilt_="
            + juce::String(fullUIBuilt_ ? "SI" : "NO")
            + " sharedData_=" + (sharedData_ == nullptr ? "NULL" : "OK"));
        return;
    }

    LogHelper::writeToLog("[MixCoachEditor] ═══════ BUILD UI START ═══════");

    earlyCrashLog("BUILD", "buildFullUI START");

    placeholderLabel_.setVisible(false);

    LogHelper::writeToLog("[MixCoachEditor] Creando MainTabbedComponent...");
    earlyCrashLog("BUILD", "before new MainTabbedComponent");
    tabbedComponent_ = std::make_unique<MainTabbedComponent>(
        processorRef_, *sharedData_);
    earlyCrashLog("BUILD", "after new MainTabbedComponent");
    addAndMakeVisible(tabbedComponent_.get());
    fullUIBuilt_ = true;

    // ═══ TIMESTAMP: registrar cuándo se construyó la UI ════════════════════
    uiBuiltTimeMs_ = juce::Time::getMillisecondCounter();
    initialSyncDone_ = false;

    // ═══ forceFullSync NO SE HACE AQUÍ ═══════════════════════════════════
    // El background worker (backgroundRunLoop) ejecuta forceFullSync cada
    // ~1s y loadSlotsFromBackupFiles cada ~2s. Los resultados se entregan
    // al timer via bgHasNewResults_ + bgForceSyncResult_ / bgBackupResult_.
    //
    // Esto evita bloquear el message thread con operaciones I/O durante
    // la creación de la UI, lo que causaba timeout de FL Studio.
    earlyCrashLog("BUILD", "UI created (sync deferred to bg worker)");
    LogHelper::writeToLog("[MixCoachEditor] forceFullSync diferido: background worker lo ejecutara en ~1s");
    LogHelper::writeToLog("[MixCoachEditor] ═══════ BUILD UI END ═══════");

    // ─── Conectar botón Re-scan del AnalyzersPanel ────────────────────────
    tabbedComponent_->getAnalyzersPanel().onRescanRequested = [this]() {
        bgBackupScanRequested_.store(true);
        bgForceSyncRequested_.store(true);
        LogHelper::writeToLog("[MixCoachEditor] Re-scan solicitado manualmente");
    };

    // ─── Configurar callbacks en SlotRegistry ────────────────────────────
    auto& registry = sharedData_->getSlotRegistry();
    registry.onSlotChanged = [this](int) {
        juce::Component::SafePointer<MixCoachAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis]() {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (!self->sharedData_ || !self->tabbedComponent_) return;
            self->tabbedComponent_->getCoachPanel().updateMessengers(
                self->sharedData_->getSlotRegistry());
            self->repaint();
        });
    };

    registry.onSlotRegistered = [this](int slotIndex) {
        juce::Component::SafePointer<MixCoachAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis, slotIndex]() {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (!self->sharedData_ || !self->tabbedComponent_) return;
            // Verificar bounds ANTES de acceder a getSlotInfo
            if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
                return;
            auto& reg = self->sharedData_->getSlotRegistry();
            auto info = reg.getSlotInfo(slotIndex);
            self->announcedSlots_[slotIndex] = true;
            auto* coach = self->processorRef_.getCoachEngine();
            if (coach) {
                coach->announceNewTrack(
                    slotIndex,
                    juce::String(info.trackName),
                    info.colour);
            }
            self->tabbedComponent_->getCoachPanel().updateMessengers(reg);
            self->repaint();
        });
    };

    registry.onSlotReleased = [this](int slotIndex) {
        juce::Component::SafePointer<MixCoachAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis, slotIndex]() {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (!self->sharedData_ || !self->tabbedComponent_) return;
            if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) {
                self->announcedSlots_[slotIndex] = false;
            }
            self->tabbedComponent_->getCoachPanel().updateMessengers(
                self->sharedData_->getSlotRegistry());
            self->repaint();
        });
    };
}

// ═══════════════════════════════════════════════════════════════════════════
//  Timer callback — LIGERO (procesa resultados del background worker)
// ═══════════════════════════════════════════════════════════════════════════
// Ya NO hace forceFullSync, loadBackupFiles, ni healthCheck aquí.
// Todas las operaciones I/O pesadas se ejecutan en backgroundRunLoop()
// via MixCoachBgWorker en un hilo separado.
//
// El timer SOLO:
//   1. initSharedData si es necesario (ligero tras primera inicialización)
//   2. Procesa resultados del background worker
//   3. Actualiza UI (updateMessengers, updateAnalyzers)
//   4. Gestiona frecuencia del timer (2fps → 10fps)
void MixCoachAudioProcessorEditor::timerCallback()
{
    // ═══ Guardia antichoque ═══════════════════════════════════════════════
    if (editorBeingDestroyed_)
        return;

    const int timerTick = ++timerTickCount_;

    // ─── Step 1: Startup delay de 3 segundos ──────────────────────────────
    // CRÍTICO: NO intentar initSharedData() inmediatamente. FL Studio
    // necesita tiempo para completar la carga del plugin y establecer el
    // message thread. Si el timer del plugin empieza a hacer trabajo
    // pesado antes de que FL Studio termine, el host detecta "no responde"
    // y pide cerrar el plugin.
    //
    // Esperamos 3 segundos (~180 ticks a 60fps) antes de cualquier init.
    if (timerTick <= 180) {
        if (timerTick == 1) {
            LogHelper::writeToLog("[MixCoachEditor] timer iniciado (60fps). Esperando 3s antes de init...");
        }
        return;
    }

    // ─── Step 2: init ligero (solo si UI no está construida aún) ─────────
    if (!fullUIBuilt_ || tabbedComponent_ == nullptr) {
        initSharedData();
    }

    if (sharedData_ == nullptr || !tabbedComponent_) {
        return;
    }

    // ─── Step 3: Procesar resultados del background worker ────────────────
    if (bgHasNewResults_.exchange(false))
    {
        int syncFound = bgForceSyncResult_.exchange(0);
        int backupFound = bgBackupResult_.exchange(0);

        if (syncFound > 0 || backupFound > 0)
        {
            // ═══ Persistencia: marcar que el sync inicial ya se completó ═══
            // Esto evita re-escanear en futuras reaperturas del editor.
            // Solo se reinicia cuando se recarga el DLL completo.
            if (!s_initialFullSyncDone_.load())
            {
                s_initialFullSyncDone_.store(true);
                LogHelper::writeToLog("[MixCoachEditor] ═════ Primer sync inicial completado "
                    "(persistente - no se repetira al reabrir) ═════");
            }

            if (syncFound > 0 && !initialSyncDone_)
                initialSyncDone_ = true;
            if (backupFound > 0 && !initialSyncDone_)
                initialSyncDone_ = true;
        }

        // ═══ FIX: Siempre actualizar UI cuando hay nuevos resultados ═══
        // Antes esto estaba dentro del if (syncFound > 0 || backupFound > 0),
        // pero syncFromShared() setea bgHasNewResults_ sin modificar syncFound/backupFound.
        // Eso provocaba que se perdieran actualizaciones del TrackSelector y VU meters
        // cuando los Messengers enviaban solo telemetría (sin metadatos nuevos).
        if (tabbedComponent_ && bgLock_.tryEnter())
        {
            double sr = processorRef_.getSampleRate();
            auto& registry = sharedData_->getSlotRegistry();
            tabbedComponent_->updateAllPanels(registry, sr);
            bgLock_.exit();
        }
        detectNewMessengers();
        repaint();
    }

    // ─── Step 4: UI updates (dual-rate: fast + slow) ───────────────────
    if (tabbedComponent_ && sharedData_ && sharedData_->isAvailable())
    {
        auto& registry = sharedData_->getSlotRegistry();

        // ─── Fast update: Messengers + meters ligeros (CADA tick ~16ms) ──
        // Solo lectura ligera de telemetry y shared memory.
        // Sin I/O, protegido con tryLock no-bloqueante contra el bg worker.
        if (bgLock_.tryEnter())
        {
            // Actualizar barras de Messengers (fluido a 60fps)
            tabbedComponent_->getCoachPanel().updateMessengers(registry);

            // ═══ METERS: Siempre actualizar (60fps) — SIN guard de visibilidad ═══
            // CRÍTICO: Antes esto estaba dentro de `if (isAnalyzersVisible())`,
            // lo que congelaba las agujas VU al cambiar de pestaña.
            // Ahora los VU meters reciben datos SIEMPRE que el timer corre,
            // independientemente de la pestaña activa.
            // El Spectrograph también se actualiza siempre (no pesa si no es visible).
            tabbedComponent_->fastUpdateSpectrograph(registry);
            tabbedComponent_->getAnalyzersPanel().fastUpdateMeters(registry);
            bgLock_.exit();
        }

        // ─── Slow update: Full panel update (frecuencia adaptativa)
        // updateAllPanels actualiza LUFS, VU, vectorscope, crest, etc.
        // Durante los primeros ~5s post-startup, corre cada 4 ticks (66ms)
        // para poblar el TrackSelector rápidamente. Luego cada 16 ticks (266ms).
        // CRÍTICO: updateAllPanels llama a updateAnalyzers que setea
        // selectedSlot_ y llama a trackSelector_.updateTracks(). Sin esto,
        // el playlist se queda vacío si bgHasNewResults_ se consumió antes
        // de que la UI estuviera construida.
        //
        // ═══ FASE RÁPIDA: Primeros 20 ticks después del startup ───────────
        // UpdateAllPanels CADA tick para poblar TrackSelector instantáneamente.
        // Esto evita el delay de esperar a timerTick % 4 == 0 para la primera
        // actualización del playlist y auto-selección de slot.
        bool didFullUpdate = false;
        if ((timerTick > 180 && timerTick <= 200) && bgLock_.tryEnter())
        {
            double sr = processorRef_.getSampleRate();
            tabbedComponent_->updateAllPanels(registry, sr);
            bgLock_.exit();
            detectNewMessengers();
            didFullUpdate = true;
        }

        // ─── Fase normal: cada 4 ticks startup, luego 16 ticks ───────────
        if (!didFullUpdate)
        {
            int slowUpdateRate = (timerTick < 180 + 300) ? 4 : 16;
            if (timerTick % slowUpdateRate == 0 && bgLock_.tryEnter())
            {
                double sr = processorRef_.getSampleRate();
                tabbedComponent_->updateAllPanels(registry, sr);
                bgLock_.exit();
                detectNewMessengers();
            }
        }

        // ─── CoachEngine periodic analysis (cada 300 ticks ~5s)
        // El motor experto lee datos reales de telemetría y genera
        // consejos personalizados según la fase actual.
        if (timerTick % 300 == 0) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                coach->periodicAnalysis();
            }
        }


    }

    // ─── Step 5: Log periódico (cada ~5s para no saturar) ─────────────
    if (timerTick % 300 == 0)
    {
        int activeSlots = 0;
        if (sharedData_ && bgLock_.tryEnter()) {
            activeSlots = sharedData_->getSlotRegistry().activeCount();
            bgLock_.exit();
        }
        LogHelper::writeToLog("[MixCoachEditor] tick=" + juce::String(timerTick)
            + " activeSlots=" + juce::String(activeSlots)
            + " shm=" + (sharedData_ && sharedData_->isAvailable() ? "OK" : "NO"));
    }
}



// ═══════════════════════════════════════════════════════════════════════════
//  Layout y pintado
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::resized()
{
    earlyCrashLog("RESIZE", "resized called");

    if (editorBeingDestroyed_)
        return;

    auto area = getLocalBounds();

    // Header ultra-compacto (ahorrado 12px para contenido)
    auto headerBounds = area.removeFromTop(24);
    versionLabel_.setBounds(headerBounds.removeFromRight(60));

    if (tabbedComponent_) {
        tabbedComponent_->setBounds(area);
    } else {
        placeholderLabel_.setBounds(area);
    }
}

void MixCoachAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (editorBeingDestroyed_)
        return;

    auto bounds = getLocalBounds();

    // ═══════════════════════════════════════════════════════════════════════
    //  CANVAS — Deep blue-black background with subtle grid (iZotope style)
    // ═══════════════════════════════════════════════════════════════════════
    g.fillAll(MixCoachTheme::bgCanvas());

    // Subtle dot grid pattern (like NUGEN / iZotope)
    g.setColour(juce::Colour(0x06FFFFFF));
    for (int gy = 0; gy < getHeight(); gy += 32) {
        for (int gx = 0; gx < getWidth(); gx += 32) {
            g.fillRect(gx, gy, 1, 1);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  HEADER BAR — Premium brand header (28px)
    //  T-RackS style: gradient background with thin accent underline
    // ═══════════════════════════════════════════════════════════════════════
    auto headerBounds = bounds.removeFromTop(28);

    // Background: gradient from canvas to elevated
    juce::ColourGradient headerBgGrad(
        MixCoachTheme::bgDark(),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgCanvas(),
        juce::Point<float>(0.0f, (float)headerBounds.getHeight()),
        false);
    g.setGradientFill(headerBgGrad);
    g.fillRect(headerBounds);

    // Subtle cyan glow on left edge
    juce::ColourGradient headerGlow(
        MixCoachTheme::accent().withAlpha(0.08f),
        juce::Point<float>((float)headerBounds.getX(), 0.0f),
        MixCoachTheme::accent().withAlpha(0.0f),
        juce::Point<float>((float)headerBounds.getX() + 200.0f, 0.0f),
        false);
    g.setGradientFill(headerGlow);
    g.fillRect(headerBounds);

    // ═══ Header accent underline (multi-layer glow) ═══════════════════════
    auto accentLineY = headerBounds.getBottom() - 1;
    int headerW = headerBounds.getWidth();

    // Layer 1: Outer glow (wide, faint)
    g.setColour(MixCoachTheme::accentGlow().withAlpha(0.06f));
    g.drawHorizontalLine(accentLineY - 2, 0.0f, (float)headerW);
    // Layer 2: Mid glow
    g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
    g.drawHorizontalLine(accentLineY - 1, 0.0f, (float)headerW);
    // Layer 3: Core line (bright)
    g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
    g.drawHorizontalLine(accentLineY, 0.0f, (float)headerW);
    // Layer 4: Inner glow below
    g.setColour(MixCoachTheme::accentGlow().withAlpha(0.15f));
    g.drawHorizontalLine(accentLineY + 1, 0.0f, (float)headerW);

    // ═══════════════════════════════════════════════════════════════════════
    //  BRANDING — MixCoach logo identity
    //  Layout: [AI icon] [MixCoach] [divider] [tagline]        [version]
    // ═══════════════════════════════════════════════════════════════════════
    auto brandingArea = headerBounds.reduced(12, 0);

    // ─── AI Icon (violet glow circle) ────────────────────────────────────
    auto iconArea = brandingArea.removeFromLeft(24);
    auto iconCentre = iconArea.getCentre().toFloat();

    // Outer glow ring
    g.setColour(MixCoachTheme::accentAI().withAlpha(0.15f));
    g.fillEllipse(juce::Rectangle<float>(iconCentre.x - 10.0f, iconCentre.y - 10.0f, 20.0f, 20.0f));
    // Icon background
    g.setColour(MixCoachTheme::accentAI().withAlpha(0.20f));
    g.fillRoundedRectangle(iconArea.toFloat().reduced(2.0f), 4.0f);
    // AI icon text
    g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    g.setColour(MixCoachTheme::accentAIGlow());
    g.drawText("AI", iconArea, juce::Justification::centred);

    brandingArea.removeFromLeft(6);

    // ─── MixCoach title ──────────────────────────────────────────────────
    auto titleArea = brandingArea.removeFromLeft(110);
    g.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    g.setColour(MixCoachTheme::textBright());
    g.drawText("MixCoach", titleArea, juce::Justification::centredLeft);

    // ─── Subtle vertical divider ─────────────────────────────────────────
    auto sepArea = brandingArea.removeFromLeft(1);
    g.setColour(MixCoachTheme::divider());
    g.fillRect(sepArea.withTop(4).withBottom(headerBounds.getBottom() - 8));
    brandingArea.removeFromLeft(8);

    // ─── Tagline ─────────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(MixCoachTheme::textDim());
    g.drawText("AI-POWERED MIXING MENTOR", brandingArea, juce::Justification::centredLeft);

    // ─── Version label (right side) ──────────────────────────────────────
    auto versionArea = headerBounds.removeFromRight(60);
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(MixCoachTheme::textMuted());
    g.drawText("v1.0.0", versionArea, juce::Justification::centredRight);

    // ═══════════════════════════════════════════════════════════════════════
    //  DECORATIVE ELEMENTS
    // ═══════════════════════════════════════════════════════════════════════

    // ─── Status dot (top-right corner, AI active indicator) ──────────────
    auto dotArea = headerBounds.removeFromRight(8).removeFromTop(8);
    float pulse = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.004f);
    g.setColour(MixCoachTheme::accentAIGlow().withAlpha(pulse * 0.3f));
    g.fillEllipse(dotArea.toFloat().expanded(4.0f));
    g.setColour(MixCoachTheme::accentAI().withAlpha(pulse));
    g.fillEllipse(dotArea.toFloat());

    // ─── Corner accent (bottom-left decorative line) ─────────────────────
    auto cornerArea = juce::Rectangle<int>(0, getHeight() - 20, 40, 20).toFloat();
    juce::ColourGradient cornerGrad(
        MixCoachTheme::accent().withAlpha(0.06f),
        juce::Point<float>(0.0f, cornerArea.getBottom()),
        MixCoachTheme::accent().withAlpha(0.0f),
        juce::Point<float>(cornerArea.getRight(), cornerArea.getY()),
        false);
    g.setGradientFill(cornerGrad);
    g.fillRect(cornerArea);
    g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
    g.fillRect(0.0f, (float)getHeight() - 1.0f, 40.0f, 1.0f);
    g.fillRect(0.0f, (float)getHeight() - 20.0f, 1.0f, 20.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Detección de nuevos Messengers
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::detectNewMessengers()
{
    if (!sharedData_ || !tabbedComponent_) return;
    auto* coach = processorRef_.getCoachEngine();

    if (!coach) return;

    int currentActiveCount = 0;

    // ═══ tryLock no-bloqueante (protegido del background worker) ═════╗
    if (bgLock_.tryEnter())
    {
        auto& registry = sharedData_->getSlotRegistry();

        currentActiveCount = registry.activeCount();

        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx >= 0 && idx < SlotRegistry::kMaxSlots && !announcedSlots_[idx]) {
                announcedSlots_[idx] = true;
                coach->announceNewTrack(idx,
                                        juce::String(info.trackName),
                                        info.colour);
            }
        });

        if (currentActiveCount < lastActiveSlotCount_) {
            for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
                auto info = registry.getSlotInfo(i);
                if (!info.active && announcedSlots_[i]) {
                    announcedSlots_[i] = false;
                }
            }
        }

        lastActiveSlotCount_ = currentActiveCount;
        bgLock_.exit();
    }
}

} // namespace mixcoach

#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

// earlyCrashLog definida en PluginProcessor.cpp — C puro, sin JUCE
extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

// ═══ Flags estáticos: persisten entre recreaciones del editor ═══════════
// Cuando el usuario minimiza/restaura el plugin en FL Studio, el editor se
// destruye y recrea, pero estos flags retienen su estado. Así evitamos
// re-escanear backup files y re-anunciar tracks en cada reapertura.
std::atomic<bool> MixCoachAudioProcessorEditor::s_initialFullSyncDone_{false};
std::array<bool, SlotRegistry::kMaxSlots> MixCoachAudioProcessorEditor::s_announcedSlots_{};

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

    earlyCrashLog("EDITOR4", "Version label removed");

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
    // s_announcedSlots_ es estático — NO se resetea aquí. Persiste entre
    // recreaciones del editor para evitar re-anunciar tracks al restaurar.

    // ═══ SCAN INICIAL: Solo se ejecuta UNA VEZ por vida del proceso DLL ═══
    // s_initialFullSyncDone_ evita re-escanear backup files y s_announcedSlots_
    // evita re-anunciar tracks en futuras reaperturas del editor.
    // El background worker ejecuta forceFullSync + loadBackupFiles UNA SOLA
    // vez al inicio del proceso. Al minimizar/restaurar, NO se re-escanéa.
    if (!s_initialFullSyncDone_.load()) {
        bgForceSyncRequested_.store(true);
        bgBackupScanRequested_.store(true);
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

            // 2. Backup scan: SOLO bajo demanda (UNA VEZ al inicio)
            //    ═══ FIX V4: Eliminado el periodic scan automático ═══
            //    loadSlotsFromBackupFiles solo se ejecuta cuando se solicita
            //    explícitamente (bgBackupScanRequested_). Después del sync
            //    inicial, NUNCA se vuelve a escanear backup files.
            if (bgBackupScanRequested_.exchange(false))
            {
                lastBgBackupScanMs_ = now;
                auto& registry = sharedData_->getSlotRegistry();

                // ═══ loadBackupFiles: UNA SOLA VEZ ═══════════════════
                int found = registry.loadSlotsFromBackupFiles(true);
                if (found > 0) {
                    bgBackupResult_.store(found);
                    bgHasNewResults_.store(true);
                    LogHelper::writeToLog("[MixCoachEditor] BG: loadBackupFiles encontro " + juce::String(found) + " slots");
                }

                // ═══ syncFromShared: solo durante la primera inicialización ═══
                // Después, syncFromShared corre en el bloque 4 (periódico)
                if (!initialSyncDone) {
                    registry.syncFromShared();
                }
            }

            // 4. Telemetría en tiempo real (~10 Hz desde BG; el timer UI hace 60 Hz)
            if (initialSyncDone && sharedData_->isAvailable())
            {
                auto& registry = sharedData_->getSlotRegistry();
                // ═══ CADA CICLO: poll desde backup (100ms) para cambios de metadatos ═══
                // pollTelemetryFromBackups() ahora lee nombre/color/bus + telemetría.
                // Al ejecutarse cada ciclo (sin modulo 3), los cambios del Messenger
                // se reflejan en MixCoach en ~100ms máximo.
                if (bgShmHealthy_.load())
                    registry.pollTelemetryFromShared();
                else
                    registry.pollTelemetryFromBackups();

                if (bgLoopCount % 10 == 0)
                    registry.syncFromShared();
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
        
        // ─── Sin idle extra — cada ciclo es rápido (~100ms) y queremos
        //     la máxima capacidad de respuesta para cambios de metadatos.
        //     pollTelemetryFromBackups() hace I/O de ~140 bytes por slot,
        //     lo que es despreciable incluso a 100ms de intervalo.
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

    // ═══ Callbacks ELIMINADOS ═══
    // Los callbacks onSlotChanged/onSlotRegistered/onSlotReleased llamaban
    // a updateMessengers via callAsync, causando un flood de mensajes
    // asíncronos que saturaban el message thread al minimizar/volver.
    //
    // El timer ya llama a:
    //   • smoothMeters()   → 60fps (actualiza barras, SIN lock)
    //   • updateAllPanels  → cada ~266ms (puebla lista, CON lock)
    //
    // Estos callbacks son REDUNDANTES y causaban el re-scan molesto.
    auto& registry = sharedData_->getSlotRegistry();
    registry.onSlotChanged    = nullptr;
    registry.onSlotRegistered = nullptr;
    registry.onSlotReleased   = nullptr;
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

    // ─── Step 1: Startup delay adaptativo ─────────────────────────────────
    // En la PRIMERA carga: 3s (180 ticks) — FL Studio necesita tiempo para
    // inicializar el plugin sin timeout.
    // En re-aperturas: ~250ms (15 ticks) — el DLL ya está cargado, solo se
    // recrea el editor. Delay mínimo para estabilizar el message thread.
    {
        int startupDelay = s_initialFullSyncDone_.load() ? 15 : 180;
        if (timerTick <= startupDelay) {
            if (timerTick == 1 && !s_initialFullSyncDone_.load())
                LogHelper::writeToLog("[MixCoachEditor] timer iniciado (60fps). Esperando 3s antes de init...");
            return;
        }
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

        // ═══ Solo actualizar paneles si hay NUEVOS slots detectados ═══
        // Si syncFromShared() seteo bgHasNewResults_ sin nuevos slots,
        // NO hacemos full update — Step 4 ya maneja la actualización
        // de meters a 60fps. Esto evita el re-scan periódico.
        if (syncFound > 0 || backupFound > 0)
        {
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
    }

    // ─── Step 4: UI updates (dual-rate: fast + slow) ───────────────────
    if (tabbedComponent_ && sharedData_ && sharedData_->isAvailable())
    {
        auto& registry = sharedData_->getSlotRegistry();

        // ─── Fast update: Messengers + meters + spectrograph (CADA tick ~16ms) ──
        // ═══ smoothMeters() desde el editor timer (60fps) ════════════════════
        // El MessengerListComponent también tiene su propio Timer interno (120fps)
        // que llama a smoothMeters() + repaint() de forma autónoma. La llamada
        // desde aquí es REDUNDANTE para el render loop, pero asegura que el
        // componente se repinte aunque el timer interno no esté activo todavía
        // (porque isPaused_ o activeMessengerCount_ están en 0 inicialmente).
        tabbedComponent_->smoothMeters();

        const bool meteringTabActive = (headerActiveTab_ == 1);
        if (meteringTabActive)
            tabbedComponent_->smoothAnalyzersPanel(60.0);

        // ═══ PASO B: Poll telemetría (60 Hz, necesita lock para shared memory) ══
        // pollTelemetryFromShared() necesita bgLock_ porque accede a shared memory.
        // Pero si tryEnter() falla, NO bloqueamos — es mejor datos un frame viejos
        // que congelar la UI.
        // ═══ CADA TICK: pollTelemetryFromBackups() (60fps) para metadatos instantáneos ═══
        // Antes: timerTick % 2 == 0 (30fps). Ahora: SIEMPRE en cada tick (60fps).
        // Esto asegura que cambios de nombre/color/bus desde el Messenger se
        // detecten en el próximo frame (~16ms) en lugar de esperar al bg worker.
        if (bgLock_.tryEnter())
        {
            if (sharedData_->isSharedMemoryAvailable() && bgShmHealthy_.load())
                registry.pollTelemetryFromShared();
            else
                registry.pollTelemetryFromBackups();

            if (meteringTabActive)
            {
                tabbedComponent_->fastUpdateSpectrograph(registry);
                tabbedComponent_->getAnalyzersPanel().fastUpdateMeters(registry);
            }
            bgLock_.exit();
        }

        // ═══ PASO C: Refrescar datos de messengers SIEMPRE (sin lock) ═════════
        // CRÍTICO: syncTelemetryFromRegistry() SOLO lee del buffer thread-safe
        // de telemetría (TelemetryBuffer::latest()). NO necesita bgLock_.
        // Al ejecutarse SIEMPRE a 60fps, el barLevel se actualiza continuamente
        // aunque tryEnter() falle, eliminando el congelamiento post-resize.
        tabbedComponent_->getCoachPanel().refreshMessengerTelemetry(registry);

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

    // Header con tabs + branding (36px)
    auto headerBounds = area.removeFromTop(36);
    juce::ignoreUnused(headerBounds);

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
    //  HEADER BAR — Premium brand header (36px) con tabs integrados
    //  Layout: [⚡ MixCoach] [AI COACH | ANALYZERS] [VERIFICAR PROGRESO] [≡ ? ⚙]
    // ═══════════════════════════════════════════════════════════════════════
    auto headerBounds = bounds.removeFromTop(36);

    // ─── Background sólido oscuro ───────────────────────────────────────
    g.setColour(MixCoachTheme::bgDark().darker(0.85f));
    g.fillRect(headerBounds);

    // Subtle accent glow on top edge
    juce::ColourGradient headerGlow(
        MixCoachTheme::accent().withAlpha(0.06f),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::accent().withAlpha(0.0f),
        juce::Point<float>(200.0f, 0.0f),
        false);
    g.setGradientFill(headerGlow);
    g.fillRect(headerBounds);

    // ─── Accent underline ────────────────────────────────────────────────
    auto accentLineY = headerBounds.getBottom() - 1;
    int headerW = headerBounds.getWidth();
    g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
    g.drawHorizontalLine(accentLineY - 1, 0.0f, (float)headerW);
    g.setColour(MixCoachTheme::accent().withAlpha(0.35f));
    g.drawHorizontalLine(accentLineY, 0.0f, (float)headerW);

    // ═══════════════════════════════════════════════════════════════════════
    //  LEFT: ⚡ MixCoach branding
    // ═══════════════════════════════════════════════════════════════════════
    auto headerInner = headerBounds.reduced(0, 2);

    // ─── Bolt icon ───────────────────────────────────────────────────────
    auto leftArea = headerInner.removeFromLeft(140);
    auto iconArea = leftArea.removeFromLeft(32);
    g.setFont(juce::Font(juce::FontOptions(20.0f)));
    g.setColour(MixCoachTheme::accentAIGlow());
    g.drawText(juce::CharPointer_UTF8("\xE2\x9A\xA1"), iconArea, juce::Justification::centred);

    leftArea.removeFromLeft(4);

    // ─── MixCoach name ───────────────────────────────────────────────────
    auto nameArea = leftArea.removeFromLeft(95);
    g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
    g.setColour(MixCoachTheme::textBright());
    g.drawText("MIXCOACH", nameArea, juce::Justification::centredLeft);

    // ═══════════════════════════════════════════════════════════════════════
    //  CENTER: Custom tabs "AI COACH" | "ANALYZERS"
    // ═══════════════════════════════════════════════════════════════════════
    auto centreArea = headerInner.removeFromLeft(280);
    int tabY = centreArea.getY();
    int tabH = centreArea.getHeight();

    // ─── Tab 1: AI COACH ─────────────────────────────────────────────────
    auto tab1Area = centreArea.removeFromLeft(100);
    headerTab1Bounds_ = tab1Area;
    bool isTab1Active = (headerActiveTab_ == 0);

    // Active tab background
    if (isTab1Active) {
        g.setColour(juce::Colour(0x14FFFFFF));
        g.fillRoundedRectangle(tab1Area.toFloat().reduced(2, 2), 4.0f);
    }

    g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    g.setColour(isTab1Active ? MixCoachTheme::accent() : MixCoachTheme::textDim());
    g.drawText("AI COACH", tab1Area.reduced(0, 4), juce::Justification::centred);

    // Active underline
    if (isTab1Active) {
        auto underline = juce::Rectangle<int>(tab1Area.getX() + 12, tab1Area.getBottom() - 4,
                                               tab1Area.getWidth() - 24, 2);
        g.setColour(MixCoachTheme::accent());
        g.fillRoundedRectangle(underline.toFloat(), 1.0f);
    }

    centreArea.removeFromLeft(4);

    // ─── Tab 2: ANALYZERS ────────────────────────────────────────────────
    auto tab2Area = centreArea.removeFromLeft(110);
    headerTab2Bounds_ = tab2Area;
    bool isTab2Active = (headerActiveTab_ == 1);

    if (isTab2Active) {
        g.setColour(juce::Colour(0x14FFFFFF));
        g.fillRoundedRectangle(tab2Area.toFloat().reduced(2, 2), 4.0f);
    }

    g.setColour(isTab2Active ? MixCoachTheme::accent() : MixCoachTheme::textDim());
    g.drawText("ANALYZERS", tab2Area.reduced(0, 4), juce::Justification::centred);

    if (isTab2Active) {
        auto underline = juce::Rectangle<int>(tab2Area.getX() + 12, tab2Area.getBottom() - 4,
                                               tab2Area.getWidth() - 24, 2);
        g.setColour(MixCoachTheme::accent());
        g.fillRoundedRectangle(underline.toFloat(), 1.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  RIGHT: VERIFICAR PROGRESO button + icons
    // ═══════════════════════════════════════════════════════════════════════
    auto rightArea = headerInner.removeFromRight(300);

    // ─── Icon buttons (≡ ? ⚙) ────────────────────────────────────────────
    auto iconsArea = rightArea.removeFromRight(90);
    int iconW = 28;

    // Menu icon ≡
    auto menuIconArea = iconsArea.removeFromRight(iconW);
    g.setFont(juce::Font(juce::FontOptions(16.0f)));
    g.setColour(MixCoachTheme::textMuted());
    g.drawText(juce::CharPointer_UTF8("\xE2\x89\xA1"), menuIconArea, juce::Justification::centred);

    // Help icon ?
    auto helpIconArea = iconsArea.removeFromRight(iconW);
    g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
    g.drawText("?", helpIconArea, juce::Justification::centred);

    // Settings icon ⚙
    auto settingsIconArea = iconsArea.removeFromRight(iconW);
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(juce::CharPointer_UTF8("\xE2\x9A\x99"), settingsIconArea, juce::Justification::centred);

    rightArea.removeFromRight(8);

    // ─── VERIFICAR PROGRESO button ─────────────────────────────────────
    auto verifyArea = rightArea.removeFromRight(160);
    headerVerifyBounds_ = verifyArea;

    // Button background (violeta sólido)
    g.setColour(MixCoachTheme::accent());
    g.fillRoundedRectangle(verifyArea.toFloat().reduced(1, 6), 5.0f);

    // Button hover glow
    g.setColour(MixCoachTheme::accentGlow().withAlpha(0.2f));
    g.fillRoundedRectangle(verifyArea.toFloat().reduced(1, 6), 5.0f);

    // Button text
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    g.setColour(juce::Colours::white);
    g.drawText(juce::CharPointer_UTF8("VERIFICAR PROGRESO"), verifyArea.reduced(4, 2), juce::Justification::centred);

    // ═══════════════════════════════════════════════════════════════════════
    //  DECORATIVE: Corner accent (bottom-left)
    // ═══════════════════════════════════════════════════════════════════════
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
//  mouseDown — Manejo de clicks en el header (tabs, botones, iconos)
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (editorBeingDestroyed_ || !tabbedComponent_)
        return;

    auto pos = e.getPosition();

    // ─── Tab click: AI COACH (tab 0) ─────────────────────────────────────
    if (headerTab1Bounds_.contains(pos) && headerActiveTab_ != 0)
    {
        headerActiveTab_ = 0;
        tabbedComponent_->setCurrentTabIndex(0);
        repaint();
        return;
    }

    // ─── Tab click: ANALYZERS (tab 1) ────────────────────────────────────
    if (headerTab2Bounds_.contains(pos) && headerActiveTab_ != 1)
    {
        headerActiveTab_ = 1;
        tabbedComponent_->setCurrentTabIndex(1);
        repaint();
        return;
    }

    // ─── VERIFICAR PROGRESO button ───────────────────────────────────────
    if (headerVerifyBounds_.contains(pos))
    {
        // Trigger a full re-scan + coach analysis
        if (sharedData_)
        {
            bgBackupScanRequested_.store(true);
            bgForceSyncRequested_.store(true);
        }

        // Notificar al coach engine para evaluar progreso
        auto* coach = processorRef_.getCoachEngine();
        if (coach != nullptr)
            coach->periodicAnalysis();

        repaint();
        return;
    }
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
            if (idx >= 0 && idx < SlotRegistry::kMaxSlots && !s_announcedSlots_[idx]) {
                s_announcedSlots_[idx] = true;
                coach->announceNewTrack(idx,
                                        juce::String(info.trackName),
                                        info.colour);
            }
        });

        if (currentActiveCount < lastActiveSlotCount_) {
            for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
                auto info = registry.getSlotInfo(i);
                if (!info.active && s_announcedSlots_[i]) {
                    s_announcedSlots_[i] = false;
                }
            }
        }

        lastActiveSlotCount_ = currentActiveCount;
        bgLock_.exit();
    }
}

} // namespace mixcoach

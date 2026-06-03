#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"
#include "../telemetry/TelemetryCollector.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

// ─── Helpers de logging seguros ──────────────────────────────────────────────
static void writeCrashLog(const juce::String& msg)
{
    try {
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Crash.log");
        logFile.getParentDirectory().createDirectory();
        juce::FileOutputStream fos(logFile, true);
        if (fos.openedOk()) {
            fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                << "] " << msg << "\n";
            fos.flush();
        }
    } catch (...) {
        // Silencio total
    }
}

// ─── Constructor: ABSOLUTAMENTE NADA que pueda crashear ─────────────────────
// Durante el escaneo VST3 de FL Studio, el plugin se carga en un sandbox.
// CreateFileMapping (llamado por SharedData::getInstance()) puede causar
// un structured exception (SEH) que try/catch C++ normal NO captura.
//
// Por eso: NO llamamos SharedData::getInstance() aquí.
//           NO llamamos LogHelper::setLogFile() aquí.
//           NO hacemos file I/O de ningún tipo aquí.
//
// Todo se inicializa LAZY en ensureSlotRegistered() / prepareToPlay().
MessengerAudioProcessor::MessengerAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // NADA — ni SharedData, ni LogHelper, ni archivos, ni FFT
    // sharedData_ = nullptr (por defecto)
    // slotIndex_ = -1 (por defecto)
    // collector_ = default constructible (sin FFT)
    trackName_ = SlotRegistry::defaultTrackName();

    // ═══ Timer 500ms: garantiza registro del slot incluso si el DAW ═══════
    // no llama setStateInformation() ni prepareToPlay() para instancias
    // nuevas (ej: FL Studio añade plugin a pista pero no inicia audio).
    //
    // Durante escaneo VST3 no hay message loop, así que timerCallback()
    // nunca se dispara — 100% seguro contra crashes de escaneo.
    // El timer se detiene tras el primer disparo (fire once).
    //
    // Si setStateInformation() o prepareToPlay() registran el slot
    // antes, slotIndex_ >= 0 y timerCallback() solo detiene el timer.
    startTimer(500);
}

MessengerAudioProcessor::~MessengerAudioProcessor()
{
    try {
        if (slotIndex_ >= 0 && sharedData_)
            sharedData_->getSlotRegistry().releaseSlot(slotIndex_);
    }
    catch (...) {}
}

// ─── Registro lazy del slot + SharedData ────────────────────────────────────
// Se llama desde prepareToPlay() y processBlock(). No desde el constructor.
void MessengerAudioProcessor::ensureSlotRegistered()
{
    if (slotIndex_ >= 0 && sharedData_ != nullptr)
        return; // Ya registrado

    try {
        // Inicializar SharedData LAZY (no en constructor!)
        if (!sharedData_) {
            sharedData_ = &SharedData::getInstance();
        }

        if (!sharedData_) {
            writeCrashLog("[Messenger] SharedData no disponible — modo degradado");
            return;
        }

        // Inicializar LogHelper (solo la primera vez)
        LogHelper::setLogFile(
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("MixCoach_Logs")
                .getChildFile("MixCoach_Messenger.log"));

        // Registrar slot
        slotIndex_ = sharedData_->getSlotRegistry().registerSlot(
            trackName_.toStdString(),
            trackColour_,
            busAssignment_);

        if (slotIndex_ >= 0) {
            slotRegistered_ = true;
            pendingBackupWrite_ = true; // Backup se escribe desde el timer (~500ms)
            writeCrashLog("[Messenger] Slot registrado: "
                + juce::String(slotIndex_) + " | name=" + trackName_);
        } else {
            writeCrashLog("[Messenger] ERROR: No se pudo registrar slot");
        }
    }
    catch (const std::exception& e) {
        writeCrashLog("[Messenger] EXCEPCION en ensureSlotRegistered: " + juce::String(e.what()));
        sharedData_ = nullptr;
        slotIndex_ = -1;
    }
    catch (...) {
        writeCrashLog("[Messenger] EXCEPCION desconocida en ensureSlotRegistered");
        sharedData_ = nullptr;
        slotIndex_ = -1;
    }
}

// ─── Timer: un solo disparo 500ms para registrar slot en DAWs que no ─────
// llaman setStateInformation() ni prepareToPlay() para instancias nuevas.
// Durante escaneo VST3 no hay message loop → timerCallback() nunca se dispara.
// Si el slot ya se registró (via setStateInformation o prepareToPlay), no-op.
void MessengerAudioProcessor::timerCallback()
{
    // ═══ Bypass seguro: si ya estamos en shutdown, no hacer nada ═══════
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
        return;

    // ═══ PASO 1: Registrar slot si no se registró antes ─────────────────
    if (slotIndex_ < 0) {
        writeCrashLog("[Messenger] timerCallback: registrando slot via timer (fire once 500ms)");
        ensureSlotRegistered();
    }

    // ═══ PASO 2: Escribir backup file desde el message thread ──────────
    // CRÍTICO: saveSlotToBackupFile() hace file I/O. NO debe ejecutarse
    // desde el audio thread (processBlock). El timer corre en el message
    // thread de FL Studio, que es seguro para file I/O.
    //
    // El backup se escribe UNA SOLA VEZ, ~500ms después del registro.
    // Esto es intencional: durante la inserción masiva de 60+ Messengers,
    // el registro es ultra-rápido (sin file I/O). Los backups se escriben
    // ~500ms después, cuando FL Studio ya terminó de crear las instancias.
    //
    // Si el slot ya tiene un backup (de sesión anterior), se sobreescribe.
    if (pendingBackupWrite_ && slotIndex_ >= 0 && sharedData_) {
        pendingBackupWrite_ = false;
        auto& registry = sharedData_->getSlotRegistry();
        auto slotInfo = registry.getSlotInfo(slotIndex_);
        if (slotInfo.active && slotInfo.slotIndex == slotIndex_) {
            SlotRegistry::saveSlotToBackupFile(slotIndex_, slotInfo);
            writeCrashLog("[Messenger] Backup escrito para slot "
                + juce::String(slotIndex_) + " (diferido al timer)");
        }
    }

    // ═══ Siempre detener el timer tras el primer disparo (fire once) ──
    stopTimer();
}

void MessengerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepared_ = true;

    try {
        ensureSlotRegistered();
        monoBuffer_.setSize(1, samplesPerBlock);
        collector_.prepare(sampleRate, samplesPerBlock);

        writeCrashLog("[Messenger] prepareToPlay OK: sampleRate="
            + juce::String(sampleRate) + " block=" + juce::String(samplesPerBlock));
    }
    catch (const std::exception& e) {
        writeCrashLog("[Messenger] EXCEPCION en prepareToPlay: " + juce::String(e.what()));
    }
    catch (...) {
        writeCrashLog("[Messenger] EXCEPCION desconocida en prepareToPlay");
    }
}

void MessengerAudioProcessor::releaseResources()
{
}

void MessengerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Protección: algunos DAWs llaman processBlock ANTES de prepareToPlay
    if (!prepared_) {
        return;
    }

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // ═══ Hot path: slot ya registrado (flags chequeada en 1ns, sin branch cost) ═══
    // ensureSlotRegistered() se llama UNA SOLA VEZ desde prepareToPlay,
    // setStateInformation, o el timer. En processBlock solo verificamos la
    // flag booleana. Evitamos la llamada a función, try/catch, y chequeos
    // redundantes en el audio thread.
    if (!slotRegistered_) {
        ensureSlotRegistered();
        if (!slotRegistered_ || !sharedData_)
            return;
    }

    auto& registry = sharedData_->getSlotRegistry();

    // ─── Si está muteado, no enviar telemetría ─────────────────────────
    if (muted_) {
        // El audio sigue pasando (Messenger es transparente por defecto)
        return;
    }

    // 1. Recopilar telemetria
    auto telemetry = collector_.collect(buffer);
    telemetry.slotIndex = slotIndex_;

    // 2. Almacenar localmente
    // Push telemetry to background manager (lock‑free queue)
    TelemetryManager::instance().pushTelemetry(slotIndex_, telemetry);

    // 3. Enviar telemetria al SharedData
    registry.setActive(slotIndex_, true);
    registry.getTelemetry(slotIndex_).push(telemetry);

    // 3b. Sincronizar a shared memory (IPC) — con throttling
    shmWriteCounter_++;
    // ═══ THROTTLE: Escribir a shared memory cada 2 bloques ═══════════════
    // Con 100+ Messengers, CADA UNO adquiriendo el spinlock en CADA bloque
    // = 100 adquisiciones por ciclo de audio (~2ms). Aunque writeSlotTelemetry()
    // usa 1 solo lock (vs los 2 originales), 100 × 1μs de spinlock overhead
    // = 100μs por bloque = 5% de CPU solo en contención del spinlock.
    //
    // Escribiendo cada 2 bloques reducimos la contención del spinlock a la
    // MITAD. Además, collect() ahora solo ejecuta DSP completo cada 4 bloques,
    // así que los valores de RMS/FFT/LUFS solo cambian cada 4 bloques. Escribir
    // a SHM cada 2 bloques asegura que los datos frescos lleguen en ≤ 2 intentos.
    //
    // Los meters de MixCoach se actualizan a 30fps (~33ms de intervalo).
    // Incluso 2 bloques (~4ms a 48kHz/96samples) es 8× más rápido que la UI
    // puede mostrar. El usuario NO percibe diferencia.
    if (shmWriteCounter_ % 2 == 0)
    {
        const float* fftData = telemetry.spectrum;

        registry.updateSharedTelemetry(
            slotIndex_,
            telemetry.peakLeft, telemetry.peakRight,
            telemetry.rmsLeft, telemetry.rmsRight,
            telemetry.correlation,
            telemetry.crestFactor,
            telemetry.sampleL, telemetry.sampleR,
            fftData,
            telemetry.lufsIntegrated,
            telemetry.lufsShortTerm,
            telemetry.lufsMomentary,
            telemetry.lufsTruePeak,
            telemetry.loudnessRange);
    }

    // 3c. Backup file SOLO cuando SHM no está disponible
    //    ═══ FIX CRÍTICO: Eliminar file I/O del audio thread cuando SHM funciona ═══
    //    ANTES: updateSlotBackupTelemetry() se llamaba CADA 16 BLOQUES (~32-64ms)
    //    desde el audio thread, incluso con SHM saludable. Con 60+ Messengers,
    //    eso son 60+ archivos abiertos/escritos/cerrados en paralelo desde
    //    audio threads → contención de disco → audio glitches → FL Studio crash.
    //
    //    AHORA: Solo se escribe backup cuando SHM NO está disponible (cada 64
    //    bloques ~128-256ms). Cuando SHM funciona (caso normal), CERO file I/O
    //    desde el audio thread. La telemetría viaja por shared memory que es
    //    órdenes de magnitud más rápida que el disco.
    //
    //    El backup sigue siendo el mecanismo de FALLBACK para cuando la shared
    //    memory entre DLLs separadas falla — pero no debe ejecutarse en el path
    //    caliente del audio thread si no es necesario.
    if (shmWriteCounter_ % 64 == 0
        && sharedData_
        && !sharedData_->isSharedMemoryAvailable())
    {
        registry.updateSlotBackupTelemetry(
            slotIndex_,
            telemetry.peakLeft, telemetry.peakRight,
            telemetry.rmsLeft, telemetry.rmsRight,
            telemetry.correlation,
            telemetry.crestFactor,
            telemetry.sampleL, telemetry.sampleR,
            telemetry.lufsIntegrated,
            telemetry.lufsShortTerm,
            telemetry.lufsMomentary,
            telemetry.lufsTruePeak,
            telemetry.loudnessRange,
            telemetry.spectrum);
    }

    // 4. Compartir audio al ring buffer
    auto& audioBuf = registry.getAudioBuffer(slotIndex_);

    if (numChannels >= 2) {
        monoBuffer_.setSize(1, numSamples, false, false, true);
        auto* monoData = monoBuffer_.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            monoData[i] = (buffer.getReadPointer(0)[i] + buffer.getReadPointer(1)[i]) * 0.5f;
        audioBuf.write(monoData, numSamples);
    } else if (numChannels == 1) {
        audioBuf.write(buffer.getReadPointer(0), numSamples);
    }
}

juce::AudioProcessorEditor* MessengerAudioProcessor::createEditor()
{
    // No inicializar SharedData/IPC desde createEditor(). FL Studio puede crear
    // editores durante escaneo/validacion, y tocar CreateFileMapping aqui vuelve
    // el plugin mucho mas propenso a cuelgues del host.
    return new MessengerAudioProcessorEditor(*this);
}

// --- APIs de rename y colorear ---

void MessengerAudioProcessor::setTrackName(const juce::String& newName)
{
    try {
        trackName_ = newName;
        if (slotIndex_ >= 0 && sharedData_)
            sharedData_->getSlotRegistry().updateSlotName(slotIndex_, newName.toStdString());
    }
    catch (...) {}
}

void MessengerAudioProcessor::setTrackColour(const juce::Colour& newColour)
{
    try {
        trackColour_ = newColour;
        if (slotIndex_ >= 0 && sharedData_)
            sharedData_->getSlotRegistry().updateSlotColour(slotIndex_, newColour);
    }
    catch (...) {}
}

void MessengerAudioProcessor::setBusAssignment(BusType bus)
{
    try {
        busAssignment_ = bus;
        if (slotIndex_ >= 0 && sharedData_)
            sharedData_->getSlotRegistry().updateSlotBus(slotIndex_, bus);
    }
    catch (...) {}
}

void MessengerAudioProcessor::setMuted(bool mute)
{
    muted_ = mute;
    if (mute && slotIndex_ >= 0 && sharedData_) {
        // Marcar slot como inactivo para que MixCoach no muestre datos stale
        sharedData_->getSlotRegistry().setActive(slotIndex_, false);
    }
}

// --- Estado persistente ---

void MessengerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    try {
        juce::MemoryOutputStream mos(destData, false);
        mos.writeInt(-1); // El slot es runtime; no debe restaurarse entre sesiones.
        auto nameStr = trackName_.toStdString();
        auto nameLen = static_cast<int>(nameStr.length());
        mos.writeInt(nameLen);
        if (nameLen > 0)
            mos.write(nameStr.data(), nameLen);
        mos.writeInt(static_cast<int>(trackColour_.getARGB()));
        mos.writeInt(static_cast<int>(busAssignment_));
    mos.writeInt(muted_ ? 1 : 0);
    }
    catch (...) {}
}

void MessengerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    try {
        // ═══ SIEMPRE registrar slot, incluso sin datos de estado ═══════════
        // CRÍTICO: El early return original (if sizeInBytes < 4) dejaba a las
        // instancias NUEVAS sin slot registrado porque ensureSlotRegistered()
        // estaba DESPUÉS del return. En DAWs como FL Studio, esto impedía que
        // MixCoach detectara Messengers hasta que el audio empezara a fluir.
        //
        // ensureSlotRegistered() ya tiene try/catch + safety checks, y llama
        // a SharedData::getInstance() que es noexcept. Es seguro llamarlo aquí
        // porque setStateInformation() NO se ejecuta durante el escaneo VST3.
        // ═══ GUARDAR slotIndex_ ANTES de leer estado ═══════════════════
        // ensureSlotRegistered() ya asignó slotIndex_. Preservamos ese valor
        // para la sincronización de metadatos al final del método.
        //
        // NOTA: El slotIndex guardado en estado persistente (primer int) se
        // IGNORA porque los slots son runtime-only y no deben restaurarse
        // entre sesiones. Ver getStateInformation() que escribe -1.
        ensureSlotRegistered();
        const int currentSlot = slotIndex_;

        // ─── Leer estado persistente (si existe) ─────────────────────────
        juce::MemoryInputStream mis(data, sizeInBytes, false);
        if (sizeInBytes < 4) {
            // Si no hay estado guardado (instancia nueva), el slot ya se
            // registró con el nombre por defecto. Sincronizar metadata
            // completa (nombre, color, bus) por consistencia.
            if (currentSlot >= 0 && sharedData_) {
                auto& registry = sharedData_->getSlotRegistry();
                registry.updateSlotName(currentSlot, trackName_.toStdString());
                registry.updateSlotColour(currentSlot, trackColour_);
                registry.updateSlotBus(currentSlot, busAssignment_);
            }
            return;
        }
        mis.readInt(); // slotIndex antiguo, descartado.
        if (sizeInBytes >= 8) {
            auto nameLen = mis.readInt();
            if (nameLen > 0 && nameLen <= 256 && (mis.getNumBytesRemaining() >= nameLen)) {
                std::vector<char> nameBuf(nameLen + 1, 0);
                mis.read(nameBuf.data(), nameLen);
                trackName_ = juce::String::fromUTF8(nameBuf.data());
            }
        }
        if (mis.getNumBytesRemaining() >= 4) {
            auto argb = static_cast<juce::uint32>(mis.readInt());
            trackColour_ = juce::Colour(argb);
        }
        if (mis.getNumBytesRemaining() >= 4) {
            busAssignment_ = static_cast<BusType>(mis.readInt());
        }

        // ─── Sincronizar metadatos con el slot registrado ───────────────
        if (currentSlot >= 0 && sharedData_)
        {
            auto& registry = sharedData_->getSlotRegistry();
            registry.updateSlotName(currentSlot, trackName_.toStdString());
            registry.updateSlotColour(currentSlot, trackColour_);
            registry.updateSlotBus(currentSlot, busAssignment_);

            writeCrashLog("[Messenger] setStateInformation: slot "
                + juce::String(slotIndex_) + " sincronizado: name=" + trackName_);
        }
    }
    catch (const std::exception& e)
    {
        writeCrashLog("[Messenger] EXCEPCION en setStateInformation (metadata sync): " + juce::String(e.what()));
    }
    catch (...)
    {
        writeCrashLog("[Messenger] EXCEPCION desconocida en setStateInformation (metadata sync)");
    }

    try {

    // Formato mínimo con mute: slotIndex(4)+nameLen(4)+name(N)+colour(4)+bus(4)+muted(4) = 20+N
    if (sizeInBytes >= 20) {
        juce::MemoryInputStream mis2(data, sizeInBytes, false);
        mis2.readInt(); // slotIndex
        auto nameLen2 = mis2.readInt();
        if (nameLen2 > 0 && mis2.getNumBytesRemaining() >= nameLen2)
            mis2.skipForward(nameLen2);
        mis2.readInt(); // colour
        mis2.readInt(); // bus
        if (mis2.getNumBytesRemaining() >= 4)
            muted_.store(mis2.readInt() != 0, std::memory_order_relaxed);
    }
    catch (const std::exception& e) {
        writeCrashLog("[Messenger] EXCEPCION en setStateInformation: " + juce::String(e.what()));
        slotIndex_ = -1;
    }
    catch (...) {
        writeCrashLog("[Messenger] EXCEPCION desconocida en setStateInformation");
        slotIndex_ = -1;
    }
}

// ─── Logging local (cada plugin escribe su propio archivo) ───────────────────

void MessengerAudioProcessor::logMessage(const juce::String& msg) const
{
    try {
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Messenger.log");
        logFile.getParentDirectory().createDirectory();
        if (!logStream_ || logStream_->getFile() != logFile) {
            logStream_ = std::make_unique<juce::FileOutputStream>(logFile, true);
        }
        if (logStream_ && logStream_->openedOk()) {
            *logStream_ << "[" << juce::Time::getCurrentTime().toString(true, true)
                       << "] [MSGR] " << msg << "\n";
            logStream_->flush();
        }
    }
    catch (...) {}
}

void MessengerAudioProcessor::logCrash(const juce::String& msg) const
{
    try {
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Crash.log");
        logFile.getParentDirectory().createDirectory();
        juce::FileOutputStream fos(logFile, true);
        if (fos.openedOk()) {
            fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                << "] [MSGR] " << msg << "\n";
            fos.flush();
        }
    }
    catch (...) {}
}

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixcoach::MessengerAudioProcessor();
}

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
    // Siempre detener el timer tras el primer disparo (fire once).
    // Si el slot ya está registrado, esta llamada es no-op.
    stopTimer();

    // ═══ Bypass seguro: si ya estamos en shutdown, no hacer nada ═══════
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
        return;

    // Registrar slot si no se registró antes
    if (slotIndex_ < 0) {
        writeCrashLog("[Messenger] timerCallback: registrando slot via timer (fire once 500ms)");
        ensureSlotRegistered();
    }
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

    // Asegurar que el slot esté registrado
    if (slotIndex_ < 0)
        ensureSlotRegistered();

    if (slotIndex_ < 0 || !sharedData_)
        return;

    auto& registry = sharedData_->getSlotRegistry();

    // 1. Recopilar telemetria
    auto telemetry = collector_.collect(buffer);
    telemetry.slotIndex = slotIndex_;

    // 2. Almacenar localmente
    localTelemetry_.push(telemetry);

    // 3. Enviar telemetria al SharedData
    registry.setActive(slotIndex_, true);
    registry.getTelemetry(slotIndex_).push(telemetry);

    // 3b. Sincronizar a shared memory (IPC)
    shmWriteCounter_++;
    // ═══ FIX: Siempre pasar datos FFT a shared memory ═══════════════════
    // ANTES: (shmWriteCounter_ % 8 == 0) causaba que los contadores
    // blockCount_ (FFT se computa cada 4 bloques) y shmWriteCounter_
    // (envío a shared memory cada 8) NUNCA se alinearan:
    //   - FFT computado en bloques 0, 4, 8, 12...
    //   - FFT enviado en escrituras 8, 16, 24... (bloques 7, 15, 23...)
    //   → El FFT NUNCA llegaba a shared memory → spectrograph NEGRO.
    // AHORA: Siempre pasamos telemetry.spectrum. updateSharedTelemetry()
    // internamente chequea hasSpectrum > 0.001f y solo escribe datos
    // válidos, ignorando ceros cuando el FFT no se computó.
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

    // 3c. Backup file (cada 16 bloques ~ cada 32-64ms)
    //    Este es el mecanismo GARANTIZADO para que MixCoach detecte
    //    este Messenger incluso si shared memory falla.
    //    El backup file contiene nombre, color, ruta y telemetría.
    if (shmWriteCounter_ % 16 == 0) {
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
            telemetry.loudnessRange);
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

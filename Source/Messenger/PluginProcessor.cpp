#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TelemetryCollector.h"
#include "../Common/SharedData.h"
#include "../Common/LogHelper.h"

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
    const float* fftData = (shmWriteCounter_ % 8 == 0) ? telemetry.spectrum : nullptr;

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
        juce::MemoryInputStream mis(data, sizeInBytes, false);
        if (sizeInBytes < 4) return;
        mis.readInt(); // slotIndex antiguo, ignorado a proposito.
        slotIndex_ = -1;
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
        // Re-sincronizar con SlotRegistry
        if (!sharedData_) return;
        auto& registry = sharedData_->getSlotRegistry();
        if (slotIndex_ >= 0 && slotIndex_ < SlotRegistry::kMaxSlots) {
            auto info = registry.getSlotInfo(slotIndex_);
            if (!info.active) {
                if (slotIndex_ >= 0)
                    registry.releaseSlot(slotIndex_);
                slotIndex_ = registry.registerSlot(trackName_.toStdString(), trackColour_, busAssignment_);
            } else {
                registry.updateSlotName(slotIndex_, trackName_.toStdString());
                registry.updateSlotColour(slotIndex_, trackColour_);
                registry.updateSlotBus(slotIndex_, busAssignment_);
            }
        } else {
            slotIndex_ = registry.registerSlot(trackName_.toStdString(), trackColour_, busAssignment_);
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

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Common/LogHelper.h"

namespace mixcoach {

// ─── WriteCrashLog (no depende de nada) ─────────────────────────────────────
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
                << "] [BRAIN] " << msg << "\n";
            fos.flush();
        }
    } catch (...) {
        // No podemos hacer nada, silencio total
    }
}

// ─── Constructor: ABSOLUTAMENTE NADA que pueda crashear ─────────────────────
// Durante el escaneo VST3 de FL Studio, el plugin se carga en un sandbox.
// Cualquier CreateFileMapping, heap allocation grande, o file I/O puede
// causar un structured exception (SEH). El try/catch C++ normal NO captura SEH,
// y __try/__except no se puede usar en constructores con objetos C++.
//
// Por eso: todo se inicializa LAZY en ensureSharedData() / prepareToPlay().
MixCoachAudioProcessor::MixCoachAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // NADA — ni SharedData, ni LogHelper, ni archivos, ni FFT
    // sharedData_ = nullptr (por defecto)
    // phaseManager_ = nullptr (unique_ptr)
    // coachEngine_ = nullptr (unique_ptr)
    // audioAnalyzer_ = default constructible (sin heap alloc)
}

MixCoachAudioProcessor::~MixCoachAudioProcessor()
{
    try {
        if (logStream_)
            logStream_->flush();
    }
    catch (...) {}
}

// ─── Inicialización LAZY de SharedData ──────────────────────────────────────
void MixCoachAudioProcessor::ensureSharedData()
{
    if (sharedData_ != nullptr)
        return;

    try {
        sharedData_ = &SharedData::getInstance();

        if (sharedData_ && sharedData_->isAvailable()) {
            phaseManager_ = std::make_unique<PhaseManager>(sharedData_->getSlotRegistry());
            coachEngine_ = std::make_unique<CoachEngine>(*phaseManager_, *sharedData_);

            // Inicializar LogHelper
            LogHelper::setLogFile(
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("MixCoach_Logs")
                    .getChildFile("MixCoach_Brain.log"));

            logMessage("========== MixCoach BRAIN iniciado ==========");
            writeCrashLog("[MixCoach] Constructor OK - sharedData disponible=SI");
        } else {
            writeCrashLog("[MixCoach] SharedData no disponible (shm fallo)");
        }
    }
    catch (const std::exception& e) {
        writeCrashLog("[MixCoach] EXCEPCION en ensureSharedData: " + juce::String(e.what()));
        sharedData_ = nullptr;
    }
    catch (...) {
        writeCrashLog("[MixCoach] EXCEPCION desconocida en ensureSharedData");
        sharedData_ = nullptr;
    }
}

void MixCoachAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    try {
        ensureSharedData();

        audioAnalyzer_.prepare(sampleRate, samplesPerBlock);
        prepared_ = true;

        logMessage("prepareToPlay: sampleRate=" + juce::String(sampleRate)
            + " samplesPerBlock=" + juce::String(samplesPerBlock));
    }
    catch (const std::exception& e) {
        writeCrashLog("[MixCoach] EXCEPCION en prepareToPlay: " + juce::String(e.what()));
    }
    catch (...) {
        writeCrashLog("[MixCoach] EXCEPCION desconocida en prepareToPlay");
    }
}

void MixCoachAudioProcessor::releaseResources()
{
}

void MixCoachAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Protección: algunos DAWs llaman processBlock ANTES de prepareToPlay
    if (!prepared_) {
        return;
    }

    // Pasamos el audio (MixCoach esta en el Master, el audio pasa directo)
    auto now = juce::Time::getMillisecondCounter();
    if (now - lastAnalysisTime_ > 500) // analisis cada 500ms como maximo
    {
        audioAnalyzer_.processBlock(buffer);
        lastAnalysisTime_ = now;
    }
}

juce::AudioProcessorEditor* MixCoachAudioProcessor::createEditor()
{
    // CRÍTICO: NO llamar ensureSharedData() aquí. Eso haría CreateFileMapping
    // en el message thread de FL Studio, congelando la UI si hay problemas.
    //
    // El editor se crea inmediatamente con sharedData_ (que puede ser nullptr).
    // El timer del editor se encarga de llamar ensureSharedData()
    // gradualmente desde su timerCallback.
    return new MixCoachAudioProcessorEditor(*this, sharedData_);
}

void MixCoachAudioProcessor::logMessage(const juce::String& msg) const
{
    try {
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Brain.log");
        logFile.getParentDirectory().createDirectory();
        if (!logStream_ || logStream_->getFile() != logFile) {
            logStream_ = std::make_unique<juce::FileOutputStream>(logFile, true);
        }
        if (logStream_ && logStream_->openedOk()) {
            *logStream_ << "[" << juce::Time::getCurrentTime().toString(true, true)
                       << "] [BRAIN] " << msg << "\n";
            logStream_->flush();
        }
    }
    catch (...) {}
}

void MixCoachAudioProcessor::logCrash(const juce::String& msg) const
{
    writeCrashLog(msg);
}

void MixCoachAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, false);
    if (phaseManager_) {
        mos.writeInt(static_cast<int>(phaseManager_->getCurrentPhase()));
    } else {
        mos.writeInt(static_cast<int>(MentorPhase::Welcome));
    }
}

void MixCoachAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis(data, sizeInBytes, false);
    if (sizeInBytes >= 4) {
        auto phase = static_cast<MentorPhase>(mis.readInt());
        if (phaseManager_) {
            phaseManager_->setPhase(phase);
        }
    }
}

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixcoach::MixCoachAudioProcessor();
}

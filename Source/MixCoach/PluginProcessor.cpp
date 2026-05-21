#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mixcoach {

MixCoachAudioProcessor::MixCoachAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , sharedData_(SharedData::getInstance())
    , phaseManager_(sharedData_.getSlotRegistry())
    , coachEngine_(phaseManager_, sharedData_)
{
}

MixCoachAudioProcessor::~MixCoachAudioProcessor() = default;

void MixCoachAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    audioAnalyzer_.prepare(sampleRate, samplesPerBlock);
}

void MixCoachAudioProcessor::releaseResources()
{
}

void MixCoachAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pasamos el audio (MixCoach está en el Master, el audio pasa directo)
    // Analizamos bajo demanda (cuando el usuario presiona "Verificar Progreso")

    auto now = juce::Time::getMillisecondCounter();
    if (now - lastAnalysisTime_ > 500) // análisis cada 500ms como máximo
    {
        audioAnalyzer_.processBlock(buffer);
        lastAnalysisTime_ = now;
    }
}

juce::AudioProcessorEditor* MixCoachAudioProcessor::createEditor()
{
    return new MixCoachAudioProcessorEditor(*this, sharedData_);
}

void MixCoachAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, false);
    // TODO: serializar estado del PhaseManager y CoachEngine
    mos.writeInt(static_cast<int>(phaseManager_.getCurrentPhase()));
}

void MixCoachAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis(data, sizeInBytes, false);
    if (sizeInBytes >= 4) {
        auto phase = static_cast<MentorPhase>(mis.readInt());
        phaseManager_.setPhase(phase);
    }
}

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixcoach::MixCoachAudioProcessor();
}

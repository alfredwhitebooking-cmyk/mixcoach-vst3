#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TelemetryCollector.h"
#include "../Common/SharedData.h"

namespace mixcoach {

MessengerAudioProcessor::MessengerAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , sharedData_(SharedData::getInstance())
{
    trackName_ = SlotRegistry::defaultTrackName();

    slotIndex_ = sharedData_.getSlotRegistry().registerSlot(
        trackName_.toStdString(),
        trackColour_);
}

MessengerAudioProcessor::~MessengerAudioProcessor()
{
    if (slotIndex_ >= 0)
        sharedData_.getSlotRegistry().releaseSlot(slotIndex_);
}

void MessengerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate);
    monoBuffer_.setSize(1, samplesPerBlock);
}

void MessengerAudioProcessor::releaseResources()
{
}

void MessengerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    if (slotIndex_ < 0) return;

    auto& registry = sharedData_.getSlotRegistry();

    // 1. Recopilar telemetria del buffer de audio
    auto telemetry = collector_.collect(buffer);
    telemetry.slotIndex = slotIndex_;

    // 2. Almacenar localmente
    localTelemetry_.push(telemetry);

    // 3. Enviar telemetria al SharedData para MixCoach
    registry.setActive(slotIndex_, true);
    registry.getTelemetry(slotIndex_).push(telemetry);

    // 4. COMPARTIR AUDIO! Copiar samples al AudioRingBuffer compartido
    auto& audioBuf = registry.getAudioBuffer(slotIndex_);

    if (numChannels >= 2) {
        // Mezclar L+R a mono y compartir (buffer pre-asignado)
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
    return new MessengerAudioProcessorEditor(*this);
}

// --- APIs de rename y colorear ---

void MessengerAudioProcessor::setTrackName(const juce::String& newName)
{
    trackName_ = newName;
    if (slotIndex_ >= 0)
        sharedData_.getSlotRegistry().updateSlotName(slotIndex_, newName.toStdString());
}

void MessengerAudioProcessor::setTrackColour(const juce::Colour& newColour)
{
    trackColour_ = newColour;
    if (slotIndex_ >= 0)
        sharedData_.getSlotRegistry().updateSlotColour(slotIndex_, newColour);
}

// --- Estado persistente ---

void MessengerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, false);

    // slotIndex
    mos.writeInt(slotIndex_);

    // trackName (string: length + data)
    auto nameStr = trackName_.toStdString();
    auto nameLen = static_cast<int>(nameStr.length());
    mos.writeInt(nameLen);
    if (nameLen > 0)
        mos.write(nameStr.data(), nameLen);

    // trackColour (ARGB)
    mos.writeInt(static_cast<int>(trackColour_.getARGB()));
}

void MessengerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis(data, sizeInBytes, false);

    if (sizeInBytes < 4) return;

    // slotIndex
    slotIndex_ = mis.readInt();

    // trackName
    if (sizeInBytes >= 8) {
        auto nameLen = mis.readInt();
        if (nameLen > 0 && nameLen <= 256 && (mis.getNumBytesRemaining() >= nameLen)) {
            std::vector<char> nameBuf(nameLen + 1, 0);
            mis.read(nameBuf.data(), nameLen);
            trackName_ = juce::String::fromUTF8(nameBuf.data());
        }
    }

    // trackColour
    if (mis.getNumBytesRemaining() >= 4) {
        auto argb = static_cast<juce::uint32>(mis.readInt());
        trackColour_ = juce::Colour(argb);
    }

    // Re-sincronizar con SlotRegistry
    if (slotIndex_ >= 0) {
        auto& registry = sharedData_.getSlotRegistry();
        auto info = registry.getSlotInfo(slotIndex_);
        if (info.active) {
            registry.updateSlotName(slotIndex_, trackName_.toStdString());
            registry.updateSlotColour(slotIndex_, trackColour_);
        }
    }
}

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mixcoach::MessengerAudioProcessor();
}

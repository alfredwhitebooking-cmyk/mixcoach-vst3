#pragma once
#include <cstdint>
#include <cstring>
#include <juce_core/juce_core.h>
#include "SharedAudioMemory.h"

namespace mixcoach {

#pragma pack(push, 8)
struct SharedAudioSlotStereo {
    // Atomic positions for lock‑free ring buffers per channel
    volatile int64_t writePosL; // writer (Messenger) left channel
    volatile int64_t writePosR; // writer right channel
    volatile int64_t readPosL;  // reader (MixCoach) left channel
    volatile int64_t readPosR;  // reader MixCoach right channel
    float bufferL[kAudioBufferSize];
    float bufferR[kAudioBufferSize];
};
#pragma pack(pop)

#pragma pack(push, 8)
struct SharedAudioBlockV2 {
    SharedAudioHeader header;
    SharedAudioSlotStereo slots[kMaxAudioSlots];
};
#pragma pack(pop)

static_assert(sizeof(SharedAudioBlockV2) == sizeof(SharedAudioHeader) + kMaxAudioSlots * sizeof(SharedAudioSlotStereo),
    "SharedAudioBlockV2 layout mismatch");

/**
    SharedAudioMemoryV2 – lock‑free ring buffers for **stereo** audio
    between Messenger (writer) and MixCoach (reader).
*/
class SharedAudioMemoryV2 {
public:
    SharedAudioMemoryV2() = default;
    ~SharedAudioMemoryV2();

    // Initialise – creates/open mapping named "MixCoachAudioMemV2"
    bool initialize(const juce::String& mapName = "Local\\MixCoachAudioMemV2");
    void close();

    // Writer – writes interleaved stereo samples to a slot (lock‑free)
    void writeStereoSamples(int slotIndex, const float* left, const float* right, int numSamples) noexcept;

    // Reader – reads left and right buffers separately (consumes)
    int readStereoSamples(int slotIndex, float* outLeft, float* outRight, int maxSamples) noexcept;

    // Scan the full ring buffer for the true peak without consuming
    // Returns the maximum absolute sample value across ALL unread data
    // in both channels. Used for diagnostic comparison against the
    // captured peak from readStereoSamples().
    void scanTruePeak(int slotIndex, float& peakL, float& peakR) const noexcept;

    // Query helpers
    [[nodiscard]] bool isInitialized() const noexcept { return block_ != nullptr; }
    [[nodiscard]] bool hasData(int slotIndex) const noexcept;
    [[nodiscard]] int  available(int slotIndex) const noexcept;

private:
    void*               fileMapping_ = nullptr;
    void*               fileView_    = nullptr;
    SharedAudioBlockV2* block_       = nullptr;
    juce::String        mapName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedAudioMemoryV2)
};

} // namespace mixcoach

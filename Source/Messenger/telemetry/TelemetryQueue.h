// TelemetryQueue.h
#pragma once
#include <juce_core/juce_core.h>
#include "../core/TelemetryData.h"

namespace mixcoach {
/**
    Simple lock‑free FIFO for passing telemetry from the audio thread to a
    background worker. Uses juce::AbstractFifo for the circular buffer.
*/
class TelemetryQueue {
public:
    static constexpr int Capacity = 256; // enough for many Messengers

    TelemetryQueue() { fifo.setTotalSize (Capacity); }

    // Called from the audio thread – never allocates.
    bool push (int slotIndex, const TelemetryData& data) noexcept {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 == 0) return false; // full
        buffer[start1].slotIndex = slotIndex;
        buffer[start1].data = data;
        fifo.finishedWrite (1);
        return true;
    }

    // Called from the worker thread.
    bool pop (int& slotIndex, TelemetryData& data) noexcept {
        int start1, size1, start2, size2;
        fifo.prepareToRead (1, start1, size1, start2, size2);
        if (size1 == 0) return false; // empty
        slotIndex = buffer[start1].slotIndex;
        data = buffer[start1].data;
        fifo.finishedRead (1);
        return true;
    }

private:
    struct Item { int slotIndex; TelemetryData data; };
    juce::AbstractFifo fifo;
    Item buffer[Capacity];
};
} // namespace mixcoach

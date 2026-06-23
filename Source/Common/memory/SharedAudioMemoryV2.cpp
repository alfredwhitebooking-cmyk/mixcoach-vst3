#include "SharedAudioMemoryV2.h"
#include "../types/LogHelper.h"

#ifdef _WIN32
#include <windows.h>
#else
#error "SharedAudioMemoryV2 only supported on Windows"
#endif

namespace mixcoach {

    // ─── Constructor / Destructor
    SharedAudioMemoryV2::~SharedAudioMemoryV2()
    {
        close();
    }

    bool SharedAudioMemoryV2::initialize(const juce::String& mapName)
    {
        if (block_ != nullptr) return true;

        mapName_ = mapName;

        // Try open existing mapping
        fileMapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName_.toWideCharPointer());
        bool isNew   = false;
        if (fileMapping_ == nullptr) {
            // Create new mapping
            fileMapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                              nullptr,
                                              PAGE_READWRITE,
                                              0,
                                              static_cast<DWORD>(sizeof(SharedAudioBlockV2)),
                                              mapName_.toWideCharPointer());
            if (fileMapping_ != nullptr) {
                isNew = (GetLastError() != ERROR_ALREADY_EXISTS);
            }
        }

        if (fileMapping_ == nullptr) {
            LogHelper::writeToLog("[SharedAudioMemoryV2] ERROR: Cannot create/open file mapping ("
                                  + juce::String(GetLastError()) + ")");
            return false;
        }

        fileView_ = MapViewOfFile(fileMapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedAudioBlockV2));
        if (fileView_ == nullptr) {
            LogHelper::writeToLog("[SharedAudioMemoryV2] ERROR: MapViewOfFile failed (" + juce::String(GetLastError())
                                  + ")");
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
            return false;
        }

        block_ = static_cast<SharedAudioBlockV2*>(fileView_);

        if (isNew) {
            memset(block_, 0, sizeof(SharedAudioBlockV2));
            block_->header.initialized = 1;
            LogHelper::writeToLog("[SharedAudioMemoryV2] Created new mapping " + mapName_);
        }
        else {
            LogHelper::writeToLog("[SharedAudioMemoryV2] Opened existing mapping " + mapName_);
        }
        return true;
    }

    void SharedAudioMemoryV2::close()
    {
#ifdef _WIN32
        if (fileView_) {
            UnmapViewOfFile(fileView_);
            fileView_ = nullptr;
        }
        if (fileMapping_) {
            CloseHandle(fileMapping_);
            fileMapping_ = nullptr;
        }
        block_ = nullptr;
#endif
    }

    // Writer: interleaved stereo samples
    void SharedAudioMemoryV2::writeStereoSamples(int slotIndex,
                                                 const float* left,
                                                 const float* right,
                                                 int numSamples) noexcept
    {
        if (!block_ || slotIndex < 0 || slotIndex >= kMaxAudioSlots || left == nullptr || right == nullptr
            || numSamples <= 0)
            return;
        auto& slot  = block_->slots[slotIndex];
        int64_t wpL = slot.writePosL;
        int64_t wpR = slot.writePosR;
        for (int i = 0; i < numSamples; ++i) {
            slot.bufferL[(wpL + i) % kAudioBufferSize] = left[i];
            slot.bufferR[(wpR + i) % kAudioBufferSize] = right[i];
        }
#if defined(_MSC_VER)
        _WriteBarrier();
#endif
        slot.writePosL = wpL + numSamples;
        slot.writePosR = wpR + numSamples;
    }

    int SharedAudioMemoryV2::readStereoSamples(int slotIndex, float* outLeft, float* outRight, int maxSamples) noexcept
    {
        if (!block_ || slotIndex < 0 || slotIndex >= kMaxAudioSlots || outLeft == nullptr || outRight == nullptr
            || maxSamples <= 0)
            return 0;
        auto& slot  = block_->slots[slotIndex];
        int64_t wpL = slot.writePosL;
        int64_t wpR = slot.writePosR;
#if defined(_MSC_VER)
        _ReadBarrier();
#endif
        int64_t rpL        = slot.readPosL;
        int64_t rpR        = slot.readPosR;
        int64_t availableL = wpL - rpL;
        int64_t availableR = wpR - rpR;
        int64_t available  = (availableL < availableR) ? availableL : availableR;
        if (available <= 0) return 0;
        if (available > kAudioBufferSize) available = kAudioBufferSize;
        int64_t toRead = std::min<int64_t>(available, maxSamples);
        for (int64_t i = 0; i < toRead; ++i) {
            outLeft[i]  = slot.bufferL[(rpL + i) % kAudioBufferSize];
            outRight[i] = slot.bufferR[(rpR + i) % kAudioBufferSize];
        }
        slot.readPosL = rpL + toRead;
        slot.readPosR = rpR + toRead;
        return static_cast<int>(toRead);
    }

    void SharedAudioMemoryV2::scanTruePeak(int slotIndex, float& peakL, float& peakR) const noexcept
    {
        peakL = 0.0f;
        peakR = 0.0f;
        if (!block_ || slotIndex < 0 || slotIndex >= kMaxAudioSlots) return;
        const auto& slot = block_->slots[slotIndex];
        int64_t wpL      = slot.writePosL;
        int64_t wpR      = slot.writePosR;
#if defined(_MSC_VER)
        _ReadBarrier();
#endif
        int64_t rpL = slot.readPosL;
        int64_t rpR = slot.readPosR;

        int64_t availableL = wpL - rpL;
        int64_t availableR = wpR - rpR;
        int64_t available  = (availableL < availableR) ? availableL : availableR;
        if (available <= 0) return;
        if (available > kAudioBufferSize) available = kAudioBufferSize;

        for (int64_t i = 0; i < available; ++i) {
            float sL   = slot.bufferL[(rpL + i) % kAudioBufferSize];
            float sR   = slot.bufferR[(rpR + i) % kAudioBufferSize];
            float absL = std::abs(sL);
            float absR = std::abs(sR);
            if (absL > peakL) peakL = absL;
            if (absR > peakR) peakR = absR;
        }
    }

    bool SharedAudioMemoryV2::hasData(int slotIndex) const noexcept
    {
        if (!block_ || slotIndex < 0 || slotIndex >= kMaxAudioSlots) return false;
        const auto& slot = block_->slots[slotIndex];
        return (slot.writePosL != slot.readPosL) || (slot.writePosR != slot.readPosR);
    }

    int SharedAudioMemoryV2::available(int slotIndex) const noexcept
    {
        if (!block_ || slotIndex < 0 || slotIndex >= kMaxAudioSlots) return 0;
        const auto& slot = block_->slots[slotIndex];
        int64_t diffL    = slot.writePosL - slot.readPosL;
        int64_t diffR    = slot.writePosR - slot.readPosR;
        int64_t diff     = (diffL > diffR) ? diffL : diffR;
        if (diff > kAudioBufferSize) diff = kAudioBufferSize;
        return static_cast<int>(diff);
    }

} // namespace mixcoach

#pragma once
#include <cstdint>
#include <cstring>
#include <atomic>
#include <juce_core/juce_core.h>
#include "SharedAudioMemory.h"

namespace mixcoach {

#pragma pack(push, 8)

    struct SharedAudioSlotStereo
    {
        // ═══ QUICK WIN 3: std::atomic en vez de volatile ═══════════════════
        // volatile NO garantiza visibilidad entre procesos en Windows moderno.
        // std::atomic con memory_order_release/acquire SÍ garantiza
        // orden de memoria correcto cross-process cuando se usa sobre
        // memoria compartida mapeada (CreateFileMapping + MapViewOfFile).
        //
        // Nota: std::atomic<int64_t> sobre shared memory es válido en MSVC
        // porque el layout es POD (sin vtable, sin mutex interno).
        // En MSVC, std::atomic<int64_t> para tipos lock-free (is_always_lock_free)
        // no contiene locks internos — solo usa intrínsecos como InterlockedExchange.
        std::atomic<int64_t> writePosL{0}; // writer (Messenger) left channel
        std::atomic<int64_t> writePosR{0}; // writer right channel
        std::atomic<int64_t> readPosL{0};  // reader (MixCoach) left channel
        std::atomic<int64_t> readPosR{0};  // reader MixCoach right channel
        /** Contador de overruns: se incrementa cuando el reader detecta
            que el writer ha sobrescrito samples no leídos (available > buffer size).
            Usado para diagnóstico de congestión IPC. */
        std::atomic<uint32_t> overrunCount{0};
        float bufferL[kAudioBufferSize];
        float bufferR[kAudioBufferSize];
    };

#pragma pack(pop)

#pragma pack(push, 8)

    struct SharedAudioBlockV2
    {
        SharedAudioHeader header;
        SharedAudioSlotStereo slots[kMaxAudioSlots];
    };

#pragma pack(pop)

    static_assert(sizeof(SharedAudioBlockV2)
                      == sizeof(SharedAudioHeader) + kMaxAudioSlots * sizeof(SharedAudioSlotStereo),
                  "SharedAudioBlockV2 layout mismatch");

    /**
        SharedAudioMemoryV2 – lock‑free ring buffers for **stereo** audio
        between Messenger (writer) and MixCoach (reader).
    */
    class SharedAudioMemoryV2
    {
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
        [[nodiscard]] int available(int slotIndex) const noexcept;

        /** Retorna el contador de overruns para un slot, o 0 si el slot no existe. */
        [[nodiscard]] uint32_t getOverrunCount(int slotIndex) const noexcept;

    private:
        void* fileMapping_         = nullptr;
        void* fileView_            = nullptr;
        SharedAudioBlockV2* block_ = nullptr;
        juce::String mapName_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedAudioMemoryV2)
    };

} // namespace mixcoach

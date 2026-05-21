#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <algorithm>
#include <cstring>
#include "Types.h"

namespace mixcoach {

// ─── Buffer circular de telemetría ──────────────────────────────────────────
class TelemetryBuffer {
public:
    TelemetryBuffer() = default;

    void push(const TrackTelemetry& data) noexcept {
        auto head = head_.load(std::memory_order_relaxed);
        buffer_[head % kSize] = data;
        head_.store(head + 1, std::memory_order_release);
    }

    [[nodiscard]] TrackTelemetry latest() const noexcept {
        auto head = head_.load(std::memory_order_acquire);
        if (head == 0) return TrackTelemetry{};
        return buffer_[(head - 1) % kSize];
    }

    [[nodiscard]] int64_t size() const noexcept {
        return static_cast<int64_t>(head_.load(std::memory_order_relaxed));
    }

private:
    static constexpr size_t kSize = 512;
    std::array<TrackTelemetry, kSize> buffer_{};
    std::atomic<size_t> head_{0};
};

// ─── Buffer circular de audio (lock-free, productor-consumidor) ──────────
// Messenger escribe samples de audio aquí → MixCoach los lee para análisis
class AudioRingBuffer {
public:
    static constexpr int kBufferSize = 4096; // ~93ms a 44.1kHz

    AudioRingBuffer() = default;

    // Escribe samples de audio (desde el thread de audio de Messenger)
    void write(const float* data, int numSamples) noexcept {
        for (int i = 0; i < numSamples; ++i) {
            auto wp = writePos_.load(std::memory_order_relaxed);
            buffer_[wp % kBufferSize] = data[i];
            writePos_.store(wp + 1, std::memory_order_release);
        }
    }

    // Lee samples de audio (desde el thread de audio de MixCoach)
    // Retorna el número de samples leídos
    int read(float* outData, int maxSamples) const noexcept {
        auto wp = writePos_.load(std::memory_order_acquire);
        auto rp = readPos_.load(std::memory_order_relaxed);
        auto available = static_cast<int>(wp - rp);
        if (available <= 0) return 0;

        auto toRead = std::min(available, maxSamples);
        for (int i = 0; i < toRead; ++i) {
            outData[i] = buffer_[(rp + i) % kBufferSize];
        }
        readPos_.store(rp + toRead, std::memory_order_release);
        return toRead;
    }

    // Lee sin consumir (peek)
    int peek(float* outData, int maxSamples) const noexcept {
        auto wp = writePos_.load(std::memory_order_acquire);
        auto rp = readPos_.load(std::memory_order_relaxed);
        auto available = static_cast<int>(wp - rp);
        if (available <= 0) return 0;

        auto toRead = std::min(available, maxSamples);
        for (int i = 0; i < toRead; ++i) {
            outData[i] = buffer_[(rp + i) % kBufferSize];
        }
        return toRead;
    }

    [[nodiscard]] bool hasData() const noexcept {
        return writePos_.load(std::memory_order_acquire) !=
               readPos_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] int available() const noexcept {
        return static_cast<int>(
            writePos_.load(std::memory_order_acquire) -
            readPos_.load(std::memory_order_relaxed));
    }

    void reset() noexcept {
        writePos_.store(0, std::memory_order_release);
        readPos_.store(0, std::memory_order_release);
    }

private:
    std::array<float, kBufferSize> buffer_{};
    mutable std::atomic<int64_t> writePos_{0};
    mutable std::atomic<int64_t> readPos_{0};
};

static_assert(sizeof(AudioRingBuffer) > 0, "AudioRingBuffer must be complete");

} // namespace mixcoach

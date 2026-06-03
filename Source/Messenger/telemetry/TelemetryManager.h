// TelemetryManager.h
#pragma once

#include "TelemetryQueue.h"
#include "TelemetryData.h"
#include "../../Common/memory/SharedData.h"
#include <thread>
#include <atomic>
#include <condition_variable>

namespace mixcoach {

class TelemetryManager {
public:
    static TelemetryManager& instance() {
        static TelemetryManager singleton;
        return singleton;
    }

    void pushTelemetry(int slotIndex, const TelemetryData& data) noexcept {
        if (queue.push(slotIndex, data)) {
            cv.notify_one();
        }
    }

    // Called during plugin shutdown to ensure clean exit
    void shutdown() {
        running.store(false);
        cv.notify_one();
        if (worker.joinable())
            worker.join();
    }

private:
    TelemetryManager() : running(true) {
        worker = std::thread([this] { this->run(); });
    }
    ~TelemetryManager() {
        shutdown();
    }

    void run() {
        while (running.load()) {
            int slotIdx = -1;
            TelemetryData data;
            if (queue.pop(slotIdx, data)) {
                // Process telemetry: write to shared memory if available
                try {
                    if (auto* shared = SharedData::getInstanceIfExists()) {
                        auto& registry = shared->getSlotRegistry();
                        // Update shared telemetry (simplified call)
                        registry.updateSharedTelemetry(
                            slotIdx,
                            data.peakLeft, data.peakRight,
                            data.rmsLeft, data.rmsRight,
                            data.correlation,
                            data.crestFactor,
                            data.sampleL, data.sampleR,
                            data.spectrum.empty() ? nullptr : data.spectrum.data(),
                            data.lufsIntegrated, data.lufsShortTerm,
                            data.lufsMomentary, data.lufsTruePeak,
                            data.loudnessRange);
                    }
                } catch (...) {
                    // Swallow any exception – background thread must not crash
                }
            } else {
                // Wait until new telemetry arrives or shutdown
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock, std::chrono::milliseconds(10));
            }
        }
    }

    TelemetryQueue queue;
    std::thread worker;
    std::atomic<bool> running{true};
    std::condition_variable cv;
    std::mutex mutex;
};

} // namespace mixcoach

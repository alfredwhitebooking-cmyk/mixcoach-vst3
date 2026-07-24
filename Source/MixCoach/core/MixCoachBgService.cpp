#include "MixCoachBgService.h"
#include "../../Common/types/LogHelper.h"
#include "../engine/CoachEngine.h"

// excpt.h: define EXCEPTION_EXECUTE_HANDLER para __try/__except
#ifdef _WIN32
#include <excpt.h>
#endif

extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor / Destructor
    // ═══════════════════════════════════════════════════════════════════════════
    MixCoachBgService::MixCoachBgService() :
        juce::Thread("MixCoachBgSvc")
    {
        // Precompute Hann window
        for (int i = 0; i < kFftSize; ++i)
            hannWindow_[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (kFftSize - 1)));
    }

    MixCoachBgService::~MixCoachBgService()
    {
        stop();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Public API
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachBgService::start(SharedData& sharedData)
    {
        if (isThreadRunning()) return; // Already running

        sharedData_ = &sharedData;
        syncRequested_.store(true);
        backupRequested_.store(true);

        startThread();
        LogHelper::writeToLog("[MixCoachBgSvc] Service started");
    }

    void MixCoachBgService::stop()
    {
        if (!isThreadRunning()) return;

        signalThreadShouldExit();
        notify(); // Wake up if waiting
        stopThread(5000); // Wait up to 5s
        sharedData_ = nullptr;
    }

    void MixCoachBgService::requestRefAnalysis(const juce::String& path)
    {
        const juce::ScopedLock lock(bgLock_);
        refAnalysisPath_ = path;
        refAnalysisRequested_.store(true);
    }

    std::vector<SlotSnapshot> MixCoachBgService::getLatestSnapshots() const
    {
        const juce::ScopedLock lock(snapshotLock_);
        return latestSnapshots_;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Thread run loop
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachBgService::run()
    {
        earlyCrashLog("BGSVC", "run START");
        int bgLoopCount      = 0;
        bool initialSyncDone = false;

        while (!threadShouldExit()) {
            wait(25); // P1 Consumer rate: reducido de 50ms a 25ms — duplica throughput para 96kHz (G1)

            // ─── __try/__except: capture SEH in background thread ───────────
            __try {
                iteration(bgLoopCount, initialSyncDone);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                earlyCrashLog("BGSVC_SEH", "SEH in bg service iteration");
            }
        }
        earlyCrashLog("BGSVC", "run END");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  iteration — One cycle of background analysis
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachBgService::iteration(int& bgLoopCount, bool& initialSyncDone)
    {
        try {
            if (sharedData_ == nullptr) return;

            bgLoopCount++;
            if (bgLoopCount % 50 == 0)
                earlyCrashLog("BGSVC", "alive");

            uint32_t now = juce::Time::getMillisecondCounter();

            // ─── SlotRegistry operations (under bgLock_) ────────────────────
            {
                const juce::ScopedLock lock(bgLock_);

                // 1. forceFullSync: once at init, or on explicit request
                if (syncRequested_.exchange(false)) {
                    lastSyncMs_ = now;
                    auto& registry = sharedData_->getSlotRegistry();
                    int found = safeForceSync(registry);
                    if (found > 0 || !initialSyncDone) {
                        syncResult_.store(found);
                    }
                    if (!initialSyncDone) {
                        initialSyncDone = true;
                        LogHelper::writeToLog("[MixCoachBgSvc] forceFullSync inicial completado ("
                                              + juce::String(found) + " slots)");
                    }
                }

                // 2. backup scan: on demand
                if (backupRequested_.exchange(false)) {
                    lastBackupMs_ = now;
                    auto& registry = sharedData_->getSlotRegistry();
                    int found = safeForceSync(registry);
                    if (found > 0) {
                        backupResult_.store(found);
                        LogHelper::writeToLog("[MixCoachBgSvc] forceFullSync encontro "
                                              + juce::String(found) + " slots");
                    }
                }

                // 3. Telemetry — read audio from SharedAudioMemoryV2
                if (initialSyncDone) {
                    if (sharedData_->getAudioMemoryV2().isInitialized()) {
                        try {
                            auto& audioMemV2 = sharedData_->getAudioMemoryV2();
                            std::vector<SlotSnapshot> newSnapshots;
                            newSnapshots.reserve(SlotRegistry::kMaxSlots);

                            sharedData_->getSlotRegistry().forEachActive([&](const SlotInfo& info) {
                                int slotIdx = info.slotIndex;
                                if (slotIdx < 0 || slotIdx >= SlotRegistry::kMaxSlots) return;

                                float left[kReadSize], right[kReadSize];
                                int nRead = audioMemV2.readStereoSamples(slotIdx, left, right, kReadSize);

                                SlotSnapshot snap;
                                snap.slotIndex   = slotIdx;
                                snap.timestampMs = now;

                                if (nRead > 0) {
                                    float peakL = 0.0f, peakR = 0.0f;
                                    double sumSqL = 0.0, sumSqR = 0.0, sumL = 0.0, sumR = 0.0;
                                    for (int i = 0; i < nRead; ++i) {
                                        float absL = std::abs(left[i]);
                                        float absR = std::abs(right[i]);
                                        if (absL > peakL) peakL = absL;
                                        if (absR > peakR) peakR = absR;
                                        sumSqL += (double)left[i] * left[i];
                                        sumSqR += (double)right[i] * right[i];
                                        sumL += left[i];
                                        sumR += right[i];
                                    }
                                    snap.peakLeft  = (peakL > 1e-10f) ? juce::Decibels::gainToDecibels(peakL) : -100.0f;
                                    snap.peakRight = (peakR > 1e-10f) ? juce::Decibels::gainToDecibels(peakR) : -100.0f;
                                    snap.rmsLeft   = (sumSqL > 0.0)
                                                         ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSqL / nRead))
                                                         : -100.0f;
                                    snap.rmsRight  = (sumSqR > 0.0)
                                                         ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSqR / nRead))
                                                         : -100.0f;

                                    if (nRead > 1) {
                                        double meanL = sumL / nRead, meanR = sumR / nRead;
                                        double num = 0.0, denL = 0.0, denR = 0.0;
                                        for (int i = 0; i < nRead; ++i) {
                                            float dl = left[i] - (float)meanL, dr = right[i] - (float)meanR;
                                            num += (double)dl * dr;
                                            denL += (double)dl * dl;
                                            denR += (double)dr * dr;
                                        }
                                        snap.correlation = (denL > 1e-15 && denR > 1e-15)
                                                               ? (float)(num / (std::sqrt(denL) * std::sqrt(denR)))
                                                               : 0.0f;
                                        snap.correlation = juce::jlimit(-1.0f, 1.0f, snap.correlation);
                                    }

                                    // ═══ Envelope follower ════════════════════════════════
                                    {
                                        auto& env = envelopeTrackers_[slotIdx];
                                        float envCurrent = env.envLevel;
                                        float peakLinear = 0.0f;
                                        float envSum     = 0.0f;
                                        int envCount     = 0;

                                        for (int i = 0; i < nRead; ++i) {
                                            float absVal = (std::abs(left[i]) + std::abs(right[i])) * 0.5f;
                                            if (absVal > peakLinear) peakLinear = absVal;
                                        }

                                        constexpr float kAttackCoeff  = 0.5f;
                                        constexpr float kReleaseCoeff = 0.9995f;

                                        for (int i = 0; i < nRead; ++i) {
                                            float absVal = (std::abs(left[i]) + std::abs(right[i])) * 0.5f;
                                            if (absVal > envCurrent)
                                                envCurrent = envCurrent * kAttackCoeff + absVal * (1.0f - kAttackCoeff);
                                            else
                                                envCurrent = envCurrent * kReleaseCoeff + absVal * (1.0f - kReleaseCoeff);
                                            envSum += envCurrent;
                                            envCount++;
                                        }

                                        env.envPeak  = juce::jmax(env.envPeak * 0.995f, peakLinear);
                                        env.envFloor = (envCount > 0)
                                                           ? juce::jmin(envSum / envCount * 0.3f,
                                                                        env.envFloor * 0.97f + 0.03f * (envSum / envCount))
                                                           : env.envFloor;

                                        if (peakLinear > 1e-6f && env.envFloor > 1e-10f
                                            && env.envPeak > env.envFloor * 2.0f) {
                                            float normEnv = (envCurrent - env.envFloor) / (env.envPeak - env.envFloor);
                                            float normPrev = (env.prevEnv - env.envFloor) / (env.envPeak - env.envFloor);

                                            if (normEnv > normPrev + 0.05f && normEnv > 0.1f && normEnv < 0.9f) {
                                                float risePerSample = (normEnv - normPrev) / nRead;
                                                if (risePerSample > 1e-6f) {
                                                    float attackEst = (0.8f / risePerSample) / 44.1f;
                                                    attackEst = juce::jlimit(0.1f, 200.0f, attackEst);
                                                    env.attackSamples =
                                                        env.attackSamples * 0.95f + attackEst * 0.05f;
                                                }
                                            }

                                            if (normEnv > 0.3f && normEnv < 0.6f && normPrev > normEnv) {
                                                float fallPerSample = (normPrev - normEnv) / nRead;
                                                if (fallPerSample > 1e-6f) {
                                                    float releaseEst = (0.5f / fallPerSample) / 44.1f;
                                                    releaseEst = juce::jlimit(1.0f, 2000.0f, releaseEst);
                                                    env.releaseSamples =
                                                        env.releaseSamples * 0.95f + releaseEst * 0.05f;
                                                }
                                            }

                                            float sustainThreshold = env.envFloor + (env.envPeak - env.envFloor) * 0.4f;
                                            float sustainSum = 0.0f;
                                            int sustainCount = 0;
                                            for (int i = 0; i < nRead; ++i) {
                                                float absVal = (std::abs(left[i]) + std::abs(right[i])) * 0.5f;
                                                if (absVal < sustainThreshold) {
                                                    sustainSum += absVal;
                                                    sustainCount++;
                                                }
                                            }
                                            if (sustainCount > nRead / 4) {
                                                float avgSustain = sustainSum / sustainCount;
                                                env.sustainLevel = env.sustainLevel * 0.95f + avgSustain * 0.05f;
                                            }
                                        }

                                        env.prevEnv  = envCurrent;
                                        env.envLevel = envCurrent;
                                        env.cycleCount++;

                                        if (env.cycleCount > 2) {
                                            snap.attackTimeMs   = env.attackSamples;
                                            snap.releaseTimeMs  = env.releaseSamples;
                                            snap.sustainLevelDb = (env.sustainLevel > 1e-10f)
                                                                      ? 20.0f * std::log10(env.sustainLevel)
                                                                      : -100.0f;
                                        }
                                    }

                                    // ═══ High-level audio descriptors + FFT analysis ══════
                                    {
                                        float blockCrest = (snap.peakLeft + snap.peakRight) * 0.5f
                                                           - (snap.rmsLeft + snap.rmsRight) * 0.5f;
                                        if (blockCrest > 2.0f && crestAvgs_[slotIdx] > 0.0f)
                                            snap.transientRatio = blockCrest / crestAvgs_[slotIdx];
                                        else
                                            snap.transientRatio = 0.0f;
                                        crestAvgs_[slotIdx] = crestAvgs_[slotIdx] * 0.9f + blockCrest * 0.1f;

                                        // FFT analysis (if we have enough samples)
                                        if (nRead >= kFftSize) {
                                            ensureFFT();
                                            // Reuse same FFT structure as before but simplified
                                            // Full multi-band/region analysis
                                            constexpr int halfBins = kFftSize / 2;

                                            double bandSum[kNumSpectralBands] = {0.0};
                                            int bandCount[kNumSpectralBands]  = {0};

                                            float regionPeakSum[kNumRegions]  = {0.0f};
                                            float regionAvgSum[kNumRegions]   = {0.0f};
                                            float regionWidthSum[kNumRegions] = {0.0f};
                                            float regionMidSum[kNumRegions]   = {0.0f};
                                            float regionSideSum[kNumRegions]  = {0.0f};
                                            int regionWinCount[kNumRegions]   = {0};

                                            int numChunks = juce::jmax(1, nRead / (kFftSize / 2) - 1);

                                            for (int ch = 0; ch < numChunks; ++ch) {
                                                int off = ch * (kFftSize / 2);
                                                if (off + kFftSize > nRead) break;

                                                std::array<float, kFftSize * 2> mfft{}, lfft{}, rfft{}, sfft{};
                                                for (int i = 0; i < kFftSize; ++i) {
                                                    int idx        = off + i;
                                                    float m        = (left[idx] + right[idx]) * 0.5f;
                                                    float s        = (left[idx] - right[idx]) * 0.5f;
                                                    mfft[i * 2]    = m * hannWindow_[i];
                                                    mfft[i * 2 + 1] = 0.0f;
                                                    sfft[i * 2]    = s * hannWindow_[i];
                                                    sfft[i * 2 + 1] = 0.0f;
                                                    lfft[i * 2]    = left[idx] * hannWindow_[i];
                                                    lfft[i * 2 + 1] = 0.0f;
                                                    rfft[i * 2]    = right[idx] * hannWindow_[i];
                                                    rfft[i * 2 + 1] = 0.0f;
                                                }
                                                slotFFT_->performRealOnlyForwardTransform(mfft.data());
                                                slotFFT_->performRealOnlyForwardTransform(lfft.data());
                                                slotFFT_->performRealOnlyForwardTransform(rfft.data());
                                                slotFFT_->performRealOnlyForwardTransform(sfft.data());

                                                for (int b = 0; b < kNumSpectralBands; ++b) {
                                                    double sumMag = 0.0;
                                                    int cnt = 0;
                                                    int startBin = kSpectralBandBins[b][0];
                                                    int endBin   = juce::jmin(kSpectralBandBins[b][1], halfBins);
                                                    for (int bin = startBin; bin < endBin; ++bin) {
                                                        float re = mfft[bin * 2], im = mfft[bin * 2 + 1];
                                                        float mag = std::sqrt(re * re + im * im);
                                                        sumMag += (mag > 1e-10f) ? 20.0 * std::log10((double)mag) : -100.0;
                                                        cnt++;
                                                    }
                                                    if (cnt > 0) {
                                                        bandSum[b] += sumMag / cnt;
                                                        bandCount[b]++;
                                                    }
                                                }

                                                for (int r = 0; r < kNumRegions; ++r) {
                                                    int startBand = kRegionBands[r][0];
                                                    int endBand   = juce::jmin(kRegionBands[r][1], kNumSpectralBands);

                                                    float maxMag = 0.0f, sumMag = 0.0f;
                                                    int bc = 0;
                                                    float lSum = 0.0f, rSum = 0.0f;
                                                    int wc = 0;
                                                    float mSumAcc = 0.0f, sSumAcc = 0.0f;
                                                    int msCountAcc = 0;

                                                    for (int b = startBand; b < endBand; ++b) {
                                                        int bStart = kSpectralBandBins[b][0];
                                                        int bEnd   = juce::jmin(kSpectralBandBins[b][1], halfBins);
                                                        float mSum = 0.0f, sSum = 0.0f;
                                                        int msCount = 0;

                                                        for (int bin = bStart; bin < bEnd; ++bin) {
                                                            float mRe = mfft[bin * 2], mIm = mfft[bin * 2 + 1];
                                                            float sRe = sfft[bin * 2], sIm = sfft[bin * 2 + 1];
                                                            float lRe = lfft[bin * 2], lIm = lfft[bin * 2 + 1];
                                                            float rRe = rfft[bin * 2], rIm = rfft[bin * 2 + 1];
                                                            float mMag = std::sqrt(mRe * mRe + mIm * mIm);
                                                            float sMag = std::sqrt(sRe * sRe + sIm * sIm);
                                                            float lMag = std::sqrt(lRe * lRe + lIm * lIm);
                                                            float rMag = std::sqrt(rRe * rRe + rIm * rIm);
                                                            sumMag += mMag;
                                                            bc++;
                                                            if (mMag > maxMag) maxMag = mMag;
                                                            lSum += lMag;
                                                            rSum += rMag;
                                                            wc++;
                                                            mSum += mMag;
                                                            sSum += sMag;
                                                            msCount++;
                                                        }
                                                        mSumAcc += mSum;
                                                        sSumAcc += sSum;
                                                        msCountAcc += msCount;
                                                    }

                                                    if (bc > 0) {
                                                        regionPeakSum[r] += maxMag;
                                                        regionAvgSum[r] += sumMag / bc;
                                                        regionWinCount[r]++;
                                                    }
                                                    if (wc > 0) {
                                                        float lA = lSum / wc, rA = rSum / wc;
                                                        float mx = juce::jmax(lA, rA);
                                                        regionWidthSum[r] += (mx > 1e-10f) ? std::abs(lA - rA) / mx : 0.0f;
                                                    }
                                                    if (msCountAcc > 0) {
                                                        float mMid  = mSumAcc / msCountAcc;
                                                        float mSide = sSumAcc / msCountAcc;
                                                        regionMidSum[r]  += (mMid > 1e-10f) ? 20.0f * std::log10(mMid) : -100.0f;
                                                        regionSideSum[r] += (mSide > 1e-10f) ? 20.0f * std::log10(mSide) : -100.0f;
                                                    }
                                                }
                                            }

                                            // Write band energies
                                            for (int b = 0; b < kNumSpectralBands; ++b) {
                                                snap.bandEnergies[b] = (bandCount[b] > 0)
                                                                          ? (float)(bandSum[b] / bandCount[b])
                                                                          : -100.0f;
                                            }

                                            // Write region data
                                            for (int r = 0; r < kNumRegions; ++r) {
                                                if (regionWinCount[r] > 0) {
                                                    float avgPeak = regionPeakSum[r] / regionWinCount[r];
                                                    float avgAvg  = regionAvgSum[r] / regionWinCount[r];
                                                    snap.crestPerBand[r] = (avgAvg > 1e-10f)
                                                                              ? avgPeak / avgAvg
                                                                              : 1.0f;
                                                    if (snap.crestPerBand[r] > 1.0f)
                                                        snap.crestPerBand[r] = 20.0f * std::log10(snap.crestPerBand[r]);
                                                    else
                                                        snap.crestPerBand[r] = 0.0f;
                                                    snap.stereoWidthPerBand[r] = regionWidthSum[r] / regionWinCount[r];
                                                    snap.midEnergyPerBand[r]   = regionMidSum[r] / regionWinCount[r];
                                                    snap.sideEnergyPerBand[r]  = regionSideSum[r] / regionWinCount[r];
                                                }
                                            }
                                        }
                                    }

                                    // Store results in SharedData (for existing consumers)
                                    TrackAudioResult result;
                                    result.timestampUs    = now * 1000;
                                    result.peakLeft       = snap.peakLeft;
                                    result.peakRight      = snap.peakRight;
                                    result.rmsLeft        = snap.rmsLeft;
                                    result.rmsRight       = snap.rmsRight;
                                    result.correlation    = snap.correlation;
                                    result.attackTimeMs   = snap.attackTimeMs;
                                    result.releaseTimeMs  = snap.releaseTimeMs;
                                    result.sustainLevelDb = snap.sustainLevelDb;
                                    result.transientRatio = snap.transientRatio;
                                    std::copy(std::begin(snap.bandEnergies), std::end(snap.bandEnergies),
                                              std::begin(result.bandEnergies));
                                    std::copy(std::begin(snap.crestPerBand), std::end(snap.crestPerBand),
                                              std::begin(result.crestPerBand));
                                    std::copy(std::begin(snap.stereoWidthPerBand), std::end(snap.stereoWidthPerBand),
                                              std::begin(result.stereoWidthPerBand));
                                    std::copy(std::begin(snap.midEnergyPerBand), std::end(snap.midEnergyPerBand),
                                              std::begin(result.midEnergyPerBand));
                                    std::copy(std::begin(snap.sideEnergyPerBand), std::end(snap.sideEnergyPerBand),
                                              std::begin(result.sideEnergyPerBand));

                                    sharedData_->updateTrackAudioResult(slotIdx, result);
                                }
                                newSnapshots.push_back(std::move(snap));
                            });

                            // Publish snapshots for UI thread
                            {
                                const juce::ScopedLock sl(snapshotLock_);
                                latestSnapshots_ = std::move(newSnapshots);
                            }
                        }
                        catch (const std::exception& e) {
                            LogHelper::writeToLog("[MixCoachBgSvc] Telemetry exception: "
                                                  + juce::String(e.what()));
                        }
                        catch (...) {
                            LogHelper::writeToLog("[MixCoachBgSvc] Telemetry unknown exception");
                        }
                    }

                    // Every ~1s: checkStaleSlots() + re-sync metadata
                    if (bgLoopCount % 10 == 0) {
                        auto& reg = sharedData_->getSlotRegistry();
                        reg.checkStaleSlots();
                        safeForceSync(reg);
                    }
                }

                // Health check every ~10s
                if (now - lastHealthMs_ >= 10000) {
                    lastHealthMs_ = now;
                    bool healthy = sharedData_->isSharedMemoryAvailable()
                                   && sharedData_->getSharedMemory().healthCheck();
                    shmHealthy_.store(healthy);
                }
            } // bgLock_ released

            // ═══ Reference analysis (outside bgLock_) ═══════════════════════
            if (refAnalysisRequested_.exchange(false)) {
                juce::String refPath;
                {
                    const juce::ScopedLock lock(bgLock_);
                    refPath = refAnalysisPath_;
                    refAnalysisPath_.clear();
                }
                // ═══ Execute reference analysis via callback (set by processor) ═══
                if (refAnalysisCallback_ && refPath.isNotEmpty()) {
                    refAnalysisCallback_(refPath);
                    LogHelper::writeToLog("[MixCoachBgSvc] Reference analysis dispatched: " + refPath);
                }
                else {
                    LogHelper::writeToLog("[MixCoachBgSvc] Reference analysis deferred (no callback): " + refPath);
                }
                shmReady_.store(true);
            }

            // ═══ Retry shared memory init (if not available yet) ═══════════
            if (!sharedData_->isAvailable()) {
                uint32_t elapsedSinceLastRetry = now - (lastHealthMs_ > 0 ? lastHealthMs_ : 0);
                if (elapsedSinceLastRetry >= 5000) {
                    bool reconnected = sharedData_->retryInitSharedMemory();
                    if (reconnected) {
                        LogHelper::writeToLog("[MixCoachBgSvc] Shared memory reconnected");
                        shmReady_.store(true);
                        syncRequested_.store(true);
                        backupRequested_.store(true);
                    }
                    lastHealthMs_ = now;
                }
            }
        }
        catch (const std::exception& e) {
            earlyCrashLog("BGSVC_CPP", e.what());
        }
        catch (...) {
            earlyCrashLog("BGSVC_CPP", "unknown C++ exception");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers
    // ═══════════════════════════════════════════════════════════════════════════
    int MixCoachBgService::safeForceSync(SlotRegistry& registry) noexcept
    {
        __try {
            return registry.forceFullSync();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            earlyCrashLog("BGSVC_SEH", "forceFullSync AV");
            return -1;
        }
    }

    void MixCoachBgService::ensureFFT()
    {
        if (fftPrepared_) return;
        slotFFT_ = std::make_unique<juce::dsp::FFT>(kFftOrder);
        fftPrepared_ = true;
    }

    void MixCoachBgService::forEachActiveSlot(std::function<void(const SlotInfo&)> callback) const
    {
        if (sharedData_ == nullptr) return;
        const juce::ScopedLock lock(bgLock_);
        sharedData_->getSlotRegistry().forEachActive(callback);
    }

} // namespace mixcoach

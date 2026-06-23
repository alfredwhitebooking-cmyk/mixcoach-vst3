#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"
#include "../engine/SemanticComparator.h"
#include "../engine/SpectralProfiler.h"
#include "../engine/AnalyzerInterpreter.h"

extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

    // MixCoachBgWorker methods (class declared in PluginEditor.h)
    MixCoachBgWorker::MixCoachBgWorker(MixCoachAudioProcessorEditor& editor) :
        juce::Thread("MixCoachBG"),
        editor_(editor)
    {}

    void MixCoachBgWorker::run()
    {
        editor_.backgroundRunLoop();
    }

    // ═══ Helper: forceFullSync protegido con __try/__except ═══════════════
    // SEPARADO de backgroundRunLoop() porque MSVC C2713 prohibe try/catch
    // y __try/__except en la misma funcion. Esta funcion tiene SOLO __try.
    // No tiene objetos C++ con destructor (evita C2712).
    // Retorna el numero de slots encontrados, o -1 si hubo SEH.
    static int safeBgForceSync(SlotRegistry& registry) noexcept
    {
        __try {
            return registry.forceFullSync();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            earlyCrashLog("BG_SEH", "forceFullSync AV");
            return -1;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  BACKGROUND RUN LOOP — Heavy I/O en hilo separado
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::backgroundRunLoop()
    {
        earlyCrashLog("BG", "backgroundRunLoop START");
        int bgLoopCount      = 0;
        bool initialSyncDone = false;

        while (backgroundWorker_ && !backgroundWorker_->threadShouldExit()) {
            backgroundWorker_->wait(100); // Sleep 100ms entre ciclos

            if (editorBeingDestroyed_) break;

            // ═══ __try/__except: captura SEH en background thread ═══════════
            __try {
                bgIteration(bgLoopCount, initialSyncDone);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                earlyCrashLog("BG_SEH", "SEH en background worker - access violation");
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  bgIteration — Cuerpo del background loop (extraído para MSVC C2712)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::bgIteration(int& bgLoopCount, bool& initialSyncDone)
    {
        try {
            if (sharedData_ == nullptr) return;

            bgLoopCount++;
            if (bgLoopCount % 50 == 0) {
                earlyCrashLog("BG", "backgroundRunLoop alive");
            }

            uint32_t now = juce::Time::getMillisecondCounter();

            // ─── Scope de mutex para SlotRegistry ────────────────────────────
            {
                const juce::ScopedLock lock(bgLock_);

                // 1. forceFullSync: UNA SOLA VEZ al inicio, o si se solicita explícitamente
                if (bgForceSyncRequested_.exchange(false)) {
                    lastBgForceSyncMs_ = now;
                    auto& registry     = sharedData_->getSlotRegistry();
                    int found          = safeBgForceSync(registry);
                    if (found > 0 || !initialSyncDone) {
                        bgForceSyncResult_.store(found);
                        bgHasNewResults_.store(true);
                    }
                    if (!initialSyncDone) {
                        initialSyncDone = true;
                        LogHelper::writeToLog("[MixCoachEditor] BG: forceFullSync inicial completado ("
                                              + juce::String(found) + " slots)");
                    }
                }
                // 2. Sync desde Shared Memory: SOLO bajo demanda (UNA VEZ al inicio)
                if (bgBackupScanRequested_.exchange(false)) {
                    lastBgBackupScanMs_ = now;
                    auto& registry      = sharedData_->getSlotRegistry();
                    int found           = safeBgForceSync(registry);
                    if (found > 0) {
                        bgBackupResult_.store(found);
                        bgHasNewResults_.store(true);
                        LogHelper::writeToLog("[MixCoachEditor] BG: forceFullSync encontro " + juce::String(found)
                                              + " slots");
                    }
                }

                // 4. Telemetría — Poblar TrackAudioResult cache desde SharedAudioMemoryV2
                if (initialSyncDone) {
                    if (sharedData_->getAudioMemoryV2().isInitialized()) {
                        try {
                            auto& audioMemV2        = sharedData_->getAudioMemoryV2();
                            constexpr int kReadSize = 2048;
                            float left[kReadSize], right[kReadSize];

                            sharedData_->getSlotRegistry().forEachActive([&](const SlotInfo& info) {
                                int slotIdx = info.slotIndex;
                                if (slotIdx < 0 || slotIdx >= SlotRegistry::kMaxSlots) return;

                                int nRead = audioMemV2.readStereoSamples(slotIdx, left, right, kReadSize);

                                TrackAudioResult result;
                                result.timestampUs = juce::Time::getMillisecondCounter() * 1000;

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
                                    result.peakLeft  = (peakL > 1e-10f) ? juce::Decibels::gainToDecibels(peakL)
                                                                        : -100.0f;
                                    result.peakRight = (peakR > 1e-10f) ? juce::Decibels::gainToDecibels(peakR)
                                                                        : -100.0f;
                                    result.rmsLeft =
                                        (sumSqL > 0.0)
                                            ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSqL / nRead))
                                            : -100.0f;
                                    result.rmsRight =
                                        (sumSqR > 0.0)
                                            ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSqR / nRead))
                                            : -100.0f;
                                    if (nRead > 1) {
                                        double meanL = sumL / nRead, meanR = sumR / nRead;
                                        double num = 0.0, denL = 0.0, denR = 0.0;
                                        for (int i = 0; i < nRead; ++i) {
                                            float dl = left[i] - (float)meanL, dr = right[i] - (float)meanR;
                                            num += dl * dr;
                                            denL += dl * dl;
                                            denR += dr * dr;
                                        }
                                        result.correlation = (denL > 1e-15 && denR > 1e-15)
                                                                 ? (float)(num / (std::sqrt(denL) * std::sqrt(denR)))
                                                                 : 0.0f;
                                        result.correlation = juce::jlimit(-1.0f, 1.0f, result.correlation);
                                    }

                                    // ═══ Envelope follower: attack/release/sustain estimation ══
                                    {
                                        auto& env = slotEnvelopeTrackers_[slotIdx];

                                        float envCurrent = env.envLevel;
                                        float peakLinear = 0.0f;
                                        float envSum     = 0.0f;
                                        int envCount     = 0;

                                        for (int i = 0; i < nRead; ++i) {
                                            float absVal = (std::abs(left[i]) + std::abs(right[i])) * 0.5f;
                                            if (absVal > peakLinear) peakLinear = absVal;
                                        }

                                        float kAttackCoeff  = 0.5f;
                                        float kReleaseCoeff = 0.9995f;

                                        for (int i = 0; i < nRead; ++i) {
                                            float absVal = (std::abs(left[i]) + std::abs(right[i])) * 0.5f;

                                            if (absVal > envCurrent) {
                                                envCurrent = envCurrent * kAttackCoeff + absVal * (1.0f - kAttackCoeff);
                                            }
                                            else {
                                                envCurrent =
                                                    envCurrent * kReleaseCoeff + absVal * (1.0f - kReleaseCoeff);
                                            }

                                            envSum += envCurrent;
                                            envCount++;
                                        }

                                        env.envPeak  = juce::jmax(env.envPeak * 0.995f, peakLinear);
                                        env.envFloor = (envCount > 0) ? juce::jmin(envSum / envCount * 0.3f,
                                                                                   env.envFloor * 0.97f
                                                                                       + 0.03f * (envSum / envCount))
                                                                      : env.envFloor;

                                        if (peakLinear > 1e-6f && env.envFloor > 1e-10f
                                            && env.envPeak > env.envFloor * 2.0f) {
                                            float normEnv = (envCurrent - env.envFloor) / (env.envPeak - env.envFloor);
                                            float normPrev =
                                                (env.prevEnv - env.envFloor) / (env.envPeak - env.envFloor);

                                            if (normEnv > normPrev + 0.05f && normEnv > 0.1f && normEnv < 0.9f) {
                                                float risePerSample = (normEnv - normPrev) / nRead;
                                                if (risePerSample > 1e-6f) {
                                                    float attackEst   = (0.8f / risePerSample) / 44.1f;
                                                    attackEst         = juce::jlimit(0.1f, 200.0f, attackEst);
                                                    env.attackSamples = env.attackSamples * 0.95f + attackEst * 0.05f;
                                                }
                                            }

                                            if (normEnv > 0.3f && normEnv < 0.6f && normPrev > normEnv) {
                                                float fallPerSample = (normPrev - normEnv) / nRead;
                                                if (fallPerSample > 1e-6f) {
                                                    float releaseEst = (0.5f / fallPerSample) / 44.1f;
                                                    releaseEst       = juce::jlimit(1.0f, 2000.0f, releaseEst);
                                                    env.releaseSamples =
                                                        env.releaseSamples * 0.95f + releaseEst * 0.05f;
                                                }
                                            }

                                            float sustainThreshold = env.envFloor + (env.envPeak - env.envFloor) * 0.4f;
                                            float sustainSum       = 0.0f;
                                            int sustainCount       = 0;
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
                                            result.attackTimeMs   = env.attackSamples;
                                            result.releaseTimeMs  = env.releaseSamples;
                                            result.sustainLevelDb = (env.sustainLevel > 1e-10f)
                                                                        ? 20.0f * std::log10(env.sustainLevel)
                                                                        : -100.0f;
                                        }
                                        else {
                                            result.attackTimeMs   = 0.0f;
                                            result.releaseTimeMs  = 0.0f;
                                            result.sustainLevelDb = -100.0f;
                                        }
                                    }

                                    // ═══ High-level audio descriptors ═══════════════════════
                                    {
                                        float blockCrest = result.getPeakCombined() - result.getRmsCombined();
                                        if (blockCrest > 2.0f && slotCrestAvgs_[slotIdx] > 0.0f)
                                            result.transientRatio = blockCrest / slotCrestAvgs_[slotIdx];
                                        else
                                            result.transientRatio = 0.0f;
                                        slotCrestAvgs_[slotIdx] = slotCrestAvgs_[slotIdx] * 0.9f + blockCrest * 0.1f;

                                        if (nRead >= 1024) {
                                            ensureSlotFFT();
                                            constexpr int fftSize = 1024;

                                            double bandSum30[kNumSpectralBands] = {0.0};
                                            int bandCount30[kNumSpectralBands]  = {0};

                                            float regionPeakSum[6]  = {0.0f};
                                            float regionAvgSum[6]   = {0.0f};
                                            float regionWidthSum[6] = {0.0f};
                                            float regionMidSum[6]   = {0.0f};
                                            float regionSideSum[6]  = {0.0f};
                                            int regionWinCount[6]   = {0};

                                            int numChunks    = juce::jmax(1, nRead / (fftSize / 2) - 1);
                                            int totalWindows = 0;

                                            for (int ch = 0; ch < numChunks; ++ch) {
                                                int off = ch * (fftSize / 2);
                                                if (off + fftSize > nRead) break;

                                                std::array<float, fftSize * 2> mfft{}, lfft{}, rfft{}, sfft{};
                                                for (int i = 0; i < fftSize; ++i) {
                                                    int idx         = off + i;
                                                    float m         = (left[idx] + right[idx]) * 0.5f;
                                                    float s         = (left[idx] - right[idx]) * 0.5f;
                                                    mfft[i * 2]     = m * slotHann_[i];
                                                    mfft[i * 2 + 1] = 0.0f;
                                                    sfft[i * 2]     = s * slotHann_[i];
                                                    sfft[i * 2 + 1] = 0.0f;
                                                    lfft[i * 2]     = left[idx] * slotHann_[i];
                                                    lfft[i * 2 + 1] = 0.0f;
                                                    rfft[i * 2]     = right[idx] * slotHann_[i];
                                                    rfft[i * 2 + 1] = 0.0f;
                                                }
                                                slotFFT_->performRealOnlyForwardTransform(mfft.data());
                                                slotFFT_->performRealOnlyForwardTransform(lfft.data());
                                                slotFFT_->performRealOnlyForwardTransform(rfft.data());
                                                slotFFT_->performRealOnlyForwardTransform(sfft.data());

                                                constexpr int halfBins = fftSize / 2;
                                                for (int b = 0; b < kNumSpectralBands; ++b) {
                                                    double sumMag = 0.0;
                                                    int cnt       = 0;
                                                    int startBin  = kSpectralBandBins[b][0];
                                                    int endBin    = juce::jmin(kSpectralBandBins[b][1], halfBins);
                                                    for (int bin = startBin; bin < endBin; ++bin) {
                                                        float re = mfft[bin * 2], im = mfft[bin * 2 + 1];
                                                        float mag = std::sqrt(re * re + im * im);
                                                        if (mag > 1e-10f) sumMag += 20.0 * std::log10((double)mag);
                                                        else
                                                            sumMag += -100.0;
                                                        cnt++;
                                                    }
                                                    if (cnt > 0) {
                                                        bandSum30[b] += sumMag / cnt;
                                                        bandCount30[b]++;
                                                    }
                                                }

                                                for (int r = 0; r < kNumRegions; ++r) {
                                                    int startBand = kRegionBands[r][0];
                                                    int endBand   = juce::jmin(kRegionBands[r][1], kNumSpectralBands);

                                                    float maxMag = 0.0f, sumMag = 0.0f;
                                                    int bc     = 0;
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
                                                        regionWidthSum[r] += (mx > 1e-10f) ? std::abs(lA - rA) / mx
                                                                                           : 0.0f;
                                                    }
                                                    if (msCountAcc > 0) {
                                                        float mMid  = mSumAcc / msCountAcc;
                                                        float mSide = sSumAcc / msCountAcc;
                                                        regionMidSum[r] += (mMid > 1e-10f) ? 20.0f * std::log10(mMid)
                                                                                           : -100.0f;
                                                        regionSideSum[r] += (mSide > 1e-10f) ? 20.0f * std::log10(mSide)
                                                                                             : -100.0f;
                                                    }
                                                }
                                                totalWindows++;
                                            }

                                            for (int b = 0; b < kNumSpectralBands; ++b) {
                                                if (bandCount30[b] > 0)
                                                    result.bandEnergies[b] = (float)(bandSum30[b] / bandCount30[b]);
                                                else
                                                    result.bandEnergies[b] = -100.0f;
                                            }

                                            for (int r = 0; r < kNumRegions; ++r) {
                                                if (regionWinCount[r] > 0) {
                                                    float avgPeak          = regionPeakSum[r] / regionWinCount[r];
                                                    float avgAvg           = regionAvgSum[r] / regionWinCount[r];
                                                    result.crestPerBand[r] = (avgAvg > 1e-10f) ? avgPeak / avgAvg
                                                                                               : 1.0f;
                                                    if (result.crestPerBand[r] > 1.0f)
                                                        result.crestPerBand[r] =
                                                            20.0f * std::log10(result.crestPerBand[r]);
                                                    else
                                                        result.crestPerBand[r] = 0.0f;
                                                    result.stereoWidthPerBand[r] =
                                                        regionWidthSum[r] / regionWinCount[r];
                                                    result.midEnergyPerBand[r]  = regionMidSum[r] / regionWinCount[r];
                                                    result.sideEnergyPerBand[r] = regionSideSum[r] / regionWinCount[r];
                                                }
                                            }
                                        }
                                    }

                                    sharedData_->updateTrackAudioResult(slotIdx, result);
                                }
                            });
                        }
                        catch (const std::exception& e) {
                            LogHelper::writeToLog("[MixCoachEditor] BG: Telemetry exception: "
                                                  + juce::String(e.what()));
                        }
                        catch (...) {
                            LogHelper::writeToLog("[MixCoachEditor] BG: Telemetry unknown exception");
                        }
                    }

                    // ═══ CADA ~1s: checkStaleSlots() + re-sync metadata ═══
                    if (bgLoopCount % 10 == 0) {
                        auto& reg = sharedData_->getSlotRegistry();
                        reg.checkStaleSlots();
                        safeBgForceSync(reg);
                    }
                }

                // 3. Health check de shared memory (cada ~10s para monitoreo)
                if (now - lastBgHealthCheckMs_ >= 10000) {
                    lastBgHealthCheckMs_ = now;
                    bool healthy         = sharedData_->isSharedMemoryAvailable()
                                           && sharedData_->getSharedMemory().healthCheck();
                    bgShmHealthy_.store(healthy);
                }
            } // ScopedLock release

            // ═══ Background reference analysis (FFT fuera del UI thread) ═══
            if (bgRefAnalysisRequested_.exchange(false)) {
                juce::String refPath;
                {
                    const juce::ScopedLock lock(bgLock_);
                    refPath = bgRefAnalysisPath_;
                    bgRefAnalysisPath_.clear();
                }
                if (refPath.isNotEmpty()) {
                    auto* coach = processorRef_.getCoachEngine();
                    if (coach != nullptr) {
                        auto& refPlayer             = processorRef_.getRefPlayer();
                        const int64_t loadedSamples = refPlayer.getLoadedBufferSize();
                        const float* bufL           = refPlayer.getRefBufferL();
                        const float* bufR           = refPlayer.getRefBufferR();

                        if (loadedSamples > 0 && bufL != nullptr) {
                            LogHelper::writeToLog("[MixCoachEditor] BG: analizando referencia desde buffer: "
                                                  + refPath);
                            coach->applyReferenceAnalysis(bufL,
                                                          bufR,
                                                          loadedSamples,
                                                          refPlayer.getLoadedNumChannels(),
                                                          refPlayer.getLoadedSampleRate(),
                                                          refPath);
                            LogHelper::writeToLog("[MixCoachEditor] BG: analisis desde buffer completado");
                        }
                        else {
                            LogHelper::writeToLog("[MixCoachEditor] BG: analizando referencia desde disco (fallback): "
                                                  + refPath);
                            coach->applyReferenceAnalysis(refPath);
                            LogHelper::writeToLog("[MixCoachEditor] BG: analisis desde disco completado");
                        }
                    }
                }
            }

            // ─── Retry shared memory init (solo si no disponible aún) ──
            if (sharedData_ != nullptr && !sharedData_->isAvailable()) {
                uint32_t elapsedSinceLastRetry = now - (lastBgHealthCheckMs_ > 0 ? lastBgHealthCheckMs_ : 0);
                if (elapsedSinceLastRetry >= 5000) {
                    earlyCrashLog("BG", "retryInitSharedMemory");
                    bool reconnected = sharedData_->retryInitSharedMemory();
                    if (reconnected) {
                        LogHelper::writeToLog("[MixCoachEditor] BG: shared memory reconectada");
                        bgSharedMemoryReady_.store(true);
                        bgForceSyncRequested_.store(true);
                        bgBackupScanRequested_.store(true);
                    }
                    lastBgHealthCheckMs_ = now;
                }
            }

        } // try
        catch (const std::exception& e) {
            earlyCrashLog("BG_CPP", e.what());
        }
        catch (...) {
            earlyCrashLog("BG_CPP", "unknown C++ exception");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  changeListenerCallback — Despacha a handleChangeBroadcast con SafePointer
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        if (source != &processorRef_.sharedDataChangeBroadcaster_) return;

        if (editorBeingDestroyed_) return;

        __try {
            handleChangeBroadcast();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            earlyCrashLog("CHANGE", "SEH capturado en changeListenerCallback");
        }
    }

    void MixCoachAudioProcessorEditor::handleChangeBroadcast()
    {
        if (editorBeingDestroyed_) return;

        LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: notificación recibida");

        sharedData_ = processorRef_.getSharedData();

        if (sharedData_ != nullptr && sharedData_->isAvailable()) {
            LogHelper::writeToLog(
                "[MixCoachEditor] ⚡ ChangeBroadcaster: sharedData AHORA disponible, construyendo UI...");

            if (!fullUIBuilt_ || tabbedComponent_ == nullptr) {
                buildFullUI();
                if (tabbedComponent_) {
                    tabbedComponent_->setBounds(getLocalBounds().withTrimmedTop(28));
                    resized();
                    repaint();
                }
            }
            else {
                if (tabbedComponent_) {
                    if (bgLock_.tryEnter()) {
                        double sr      = processorRef_.getSampleRate();
                        auto& registry = sharedData_->getSlotRegistry();
                        tabbedComponent_->updateAllPanels(registry, *sharedData_, sr);
                        bgLock_.exit();
                    }
                    repaint();
                }
            }
        }
        else {
            LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: sharedData sigue NO disponible");
            if (lastInitAttemptMs_ > 0) {
                lastInitAttemptMs_ = 0;
            }
        }
    }

    // ─── Inicialización LIGERA de SharedData (SIN bloquear message thread) ─────
    void MixCoachAudioProcessorEditor::initSharedData()
    {
        if (editorBeingDestroyed_) return;

        if (sharedData_ == nullptr) {
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: obteniendo SharedData singleton...");
            sharedData_ = &SharedData::getInstance();
            LogHelper::writeToLog(juce::String("[MixCoachEditor] initSharedData: sharedData_=")
                                  + (sharedData_ == nullptr ? "NULL" : "OK"));
        }

        if (sharedData_ == nullptr) return;

        if (sharedData_->isAvailable()) {
            // ═══ Llamar a initBrainModules() SIEMPRE que sharedData esté disponible ═══
            // initBrainModules() es IDEMPOTENTE — retorna early si aiCoachAdapter_ ya existe.
            // NO podemos checkear phaseManager_ porque ensureSharedData() ya lo creó.
            // La guardia correcta está DENTRO de initBrainModules(): checkea aiCoachAdapter_.
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: Llamando initBrainModules...");
            bool brainOk = processorRef_.initBrainModules();

            if (brainOk) {
                LogHelper::writeToLog("[MixCoachEditor] initSharedData: Brain Modules listos");

                // Notificar al editor si la UI no está construida aún
                if (!fullUIBuilt_ || tabbedComponent_ == nullptr)
                    processorRef_.sharedDataChangeBroadcaster_.sendChangeMessage();
            }
            else {
                LogHelper::writeToLog("[MixCoachEditor] initSharedData: Brain Modules NO disponibles (shared memory?)");
            }
        }

        if (sharedData_ != nullptr && sharedData_->isAvailable() && (!fullUIBuilt_ || tabbedComponent_ == nullptr)) {
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: sharedData OK, construyendo UI...");
            buildFullUI();
            if (tabbedComponent_) {
                tabbedComponent_->setBounds(getLocalBounds().withTrimmedTop(36));
                resized();
                repaint();
            }
        }
    }

} // namespace mixcoach

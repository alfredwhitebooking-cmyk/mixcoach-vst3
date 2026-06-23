#include "CoachEngine.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  GAIN STAGING — Picos, clipping, headroom
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeGainStagingReal()
    {
        auto now       = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        float maxGlobalPeak = -100.0f;
        int clippingCount   = 0;
        int lowSignalCount  = 0;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;

            if (info.muted || (soloActive_ && !info.soloed)) return;

            float peakDb  = juce::jmax(telem.peakLeft, telem.peakRight);
            maxGlobalPeak = juce::jmax(maxGlobalPeak, peakDb);

            auto& state            = trackStates_[info.slotIndex];
            juce::String trackName = juce::String(info.trackName).trim();
            if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

            auto gainAdvice  = analyzeTrackGain(info.slotIndex);
            float peakTarget = gainAdvice.peakTarget;
            bool hasTarget   = (gainAdvice.role != TrackRole::Unknown && gainAdvice.role != TrackRole::Master);

            if (peakDb > -0.5f) {
                clippingCount++;
                if (!state.wasClipping) {
                    state.wasClipping = true;
                    if (now - lastPeakWarningUs_ > kWarningCooldownUs) {
                        lastPeakWarningUs_ = now;
                        juce::String msg;
                        msg += "\xF0\x9F\x94\xB4 **" + trackName + "** est\xC3\xA1 recortando a **"
                               + juce::String(peakDb, 1) + " dB";
                        if (hasTarget) msg += " (target " + juce::String(peakTarget, 1) + " dBFS)";
                        msg += ". Reduce el gain inmediatamente.";
                        respondWithContext(msg, trackName, MentorMessage::Type::Warning);
                    }
                }
            }
            else if (peakDb > -3.0f && peakDb > state.lastPeakDb) {
                if (now - state.lastWarningUs > kTrackCooldownUs) {
                    state.lastWarningUs = now;
                    juce::String msg;
                    msg += "\xE2\x9A\xA0\xEF\xB8\x8F **" + trackName + "** est\xC3\xA1 a **" + juce::String(peakDb, 1)
                           + " dBFS";
                    if (hasTarget) {
                        float delta = peakDb - peakTarget;
                        msg += " (target " + juce::String(peakTarget, 1) + " dBFS). Baja ~"
                               + juce::String(std::abs(delta), 1) + " dB";
                    }
                    else {
                        msg += "**. Considera reducir el gain 3-6 dB";
                    }
                    msg += " para llegar a rango \xC3\xB3ptimo.";
                    respondWithContext(msg, trackName, MentorMessage::Type::Tip);
                }
            }
            else if (peakDb < -30.0f) {
                lowSignalCount++;
                if (!state.wasLowSignal && registry.activeCount() > 1) {
                    state.wasLowSignal = true;
                    juce::String msg;
                    msg += "\xF0\x9F\x94\x87 **" + trackName + "** tiene se\xC3\xB1" "al muy baja (" + juce::String(peakDb, 1) + " dB)";
                    if (hasTarget) msg += ", target " + juce::String(peakTarget, 1) + " dBFS";
                    msg += ". \xC2\xBFSubiste el fader?";
                    respondWithContext(msg, trackName, MentorMessage::Type::Info);
                }
            }
            else {
                state.wasClipping  = false;
                state.wasLowSignal = false;
            }

            if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) state.lastCrestFactor = telem.crestFactor;

            state.lastPeakDb = peakDb;
            state.lastRmsDb  = telem.rmsLeft;
        });

        if (maxGlobalPeak > -6.0f && maxGlobalPeak < -0.5f) {
            if (now - lastHeadroomWarningUs_ > kWarningCooldownUs) {
                lastHeadroomWarningUs_ = now;
                float headroom         = -maxGlobalPeak;
                respondWith(
                "\xF0\x9F\x93\x8A **Headroom: " + juce::String(headroom, 1) + " dB** \xE2\x80\x94 "
                "la pista con m\xC3\xA1s nivel alcanza **" + juce::String(maxGlobalPeak, 1)
                + " dB**. El rango ideal es -6 dB a -3 dB de pico en el master.",
                MentorMessage::Type::Tip);
            }
        }
        else if (maxGlobalPeak < -18.0f && registry.activeCount() >= 3) {
            if (now - lastHeadroomWarningUs_ > kWarningCooldownUs) {
                lastHeadroomWarningUs_ = now;
                respondWith(
                "\xF0\x9F\x93\x8A Las pistas est\xC3\xA1n muy bajas (pico m\xC3\xA1ximo: **" + juce::String(maxGlobalPeak, 1)
                + " dB**). Sube los faders de gain hasta que el master marque "
                "entre -12 dB y -6 dB.",
                MentorMessage::Type::Info);
            }
        }

        if (clippingCount > 0 || lowSignalCount > 0) {
            LogHelper::writeToLog("[CoachEngine] GainStaging: "
                              + juce::String(clippingCount) + " clipping, "
                              + juce::String(lowSignalCount) + " baja se\xC3\xB1" "al, "
                              + "pico global=" + juce::String(maxGlobalPeak, 1) + " dB");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  ORGANIZACI\xC3\x93N
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeOrganisationReal()
    {
        auto& registry = sharedData_.getSlotRegistry();
        int active     = registry.activeCount();
        if (active == 0) return;

        int bussedCount = 0;
        registry.forEachActive([&](const SlotInfo& info) {
            if (info.bus != BusType::None) bussedCount++;
        });

        int unnamedCount = 0;
        registry.forEachActive([&](const SlotInfo& info) {
            juce::String name = juce::String(info.trackName).trim();
            if (name.isEmpty() || name.startsWith("Pista")) unnamedCount++;
        });

        if (unnamedCount > 0)
            respondWith("\xF0\x9F\x93\x9D **" + juce::String(unnamedCount) + " pista(s)** sin nombre. "
                    "Nombrar cada pista ayuda a mantener la mezcla organizada.",
                    MentorMessage::Type::Info);

        if (active >= 3 && bussedCount < active / 2)
            respondWith("\xF0\x9F\x94\x97 Solo **" + juce::String(bussedCount) + "/" + juce::String(active)
                    + "** pistas tienen bus asignado. Agrupar por familias "
                    "(bater\xC3\xAD" "a, bajo, voces) facilita el procesamiento por grupos.",
                    MentorMessage::Type::Tip);

        LogHelper::writeToLog("[CoachEngine] Organizaci\xC3\xB3n: " + juce::String(active) + " activas, "
                              + juce::String(bussedCount) + " con bus, " + juce::String(unnamedCount) + " sin nombre");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  BALANCE TONAL
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeTonalBalanceReal()
    {
        auto now       = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        bool hasSpectrumData = false;
        float avgSubBass     = -100.0f;
        float avgLowBass     = -100.0f;
        float avgBass        = -100.0f;
        float avgLowMids     = -100.0f;
        float avgMids        = -100.0f;
        float avgHighMids    = -100.0f;
        float avgPresence    = -100.0f;
        float avgHighs       = -100.0f;
        float avgAir         = -100.0f;
        int trackCount       = 0;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;

            float spectrumSum = 0.0f;
            for (int fi = 0; fi < kNumSpectrumBins; ++fi) spectrumSum += telem.spectrum[fi];
            if (spectrumSum < 0.01f) return;
            hasSpectrumData = true;
            trackCount++;

            auto bandAvg = [&](int start, int end) -> float {
                float s = 0.0f;
                for (int fi = start; fi < end && fi < kNumSpectrumBins; ++fi) s += telem.spectrum[fi];
                int n = juce::jmin(end - start, kNumSpectrumBins - start);
                return (n > 0) ? (s / n) : -100.0f;
            };
            auto accumMax = [](float& acc, float v) {
                if (v > acc) acc = v;
            };

            accumMax(avgSubBass, bandAvg(0, 1));
            accumMax(avgLowBass, bandAvg(1, 3));
            accumMax(avgBass, bandAvg(3, 6));
            accumMax(avgLowMids, bandAvg(6, 13));
            accumMax(avgMids, bandAvg(13, 25));
            accumMax(avgHighMids, bandAvg(25, 53));
            accumMax(avgPresence, bandAvg(53, 106));
            accumMax(avgHighs, bandAvg(106, 213));
            accumMax(avgAir, bandAvg(213, 426));
        });

        if (!hasSpectrumData || trackCount == 0) return;

        auto toDb = [](float v) -> float { return (v > 0.001f) ? juce::Decibels::gainToDecibels(v) : -60.0f; };

        if (now - lastTonalWarningUs_ > kWarningCooldownUs) {
            bool warned      = false;
            float subBassDb  = toDb(avgSubBass);
            float lowBassDb  = toDb(avgLowBass);
            float bassDb     = toDb(avgBass);
            float lowMidsDb  = toDb(avgLowMids);
            float midsDb     = toDb(avgMids);
            float highMidsDb = toDb(avgHighMids);
            float presenceDb = toDb(avgPresence);
            float highsDb    = toDb(avgHighs);
            float airDb      = toDb(avgAir);

            LogHelper::writeToLog("[CoachEngine] Espectro (dBFS): sub=" + juce::String(subBassDb, 1)
                                  + " bass=" + juce::String(bassDb, 1) + " mids=" + juce::String(midsDb, 1)
                                  + " presence=" + juce::String(presenceDb, 1) + " air=" + juce::String(airDb, 1));

            if ((subBassDb > -20.0f || lowBassDb > -15.0f) && subBassDb > midsDb + 10.0f) {
                respondWith(
                    "\xF0\x9F\x8E\x9B\xEF\xB8\x8F **Exceso de graves**: el sub-bass domina. Prueba HPF en bajo 40-60 "
                    "Hz.",
                    MentorMessage::Type::Tip);
                warned = true;
            }
            else if (bassDb < -35.0f && lowBassDb < -30.0f && midsDb > -25.0f) {
                respondWith("\xF0\x9F\x8E\x9B\xEF\xB8\x8F **Faltan graves**: Revisa que bajo y bombo tengan presencia.",
                            MentorMessage::Type::Info);
                warned = true;
            }
            else if (presenceDb < -35.0f && highsDb < -40.0f && airDb < -45.0f && midsDb > -25.0f) {
                respondWith("\xF0\x9F\x8E\x9B\xEF\xB8\x8F **Mezcla opaca**: Prueba realce shelving en 8-12 kHz.",
                            MentorMessage::Type::Tip);
                warned = true;
            }
            else if (presenceDb > -15.0f && presenceDb > midsDb + 8.0f) {
                respondWith("\xF0\x9F\x8E\x9B\xEF\xB8\x8F **Exceso de agudos**: Prueba low-pass suave en 12-14 kHz.",
                            MentorMessage::Type::Tip);
                warned = true;
            }
            else if (airDb > -15.0f && airDb > highMidsDb + 6.0f && presenceDb > -20.0f) {
                respondWith(
                    "\xF0\x9F\x8E\x9B\xEF\xB8\x8F **Exceso de aire**: Reduce shelving HF o aplica low-pass en 16-18 "
                    "kHz.",
                    MentorMessage::Type::Tip);
                warned = true;
            }

            if (warned) lastTonalWarningUs_ = now;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DIN\xC3\x81MICA
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeDynamicsReal()
    {
        auto now       = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        float avgCrestFactor = 0.0f;
        int crestCount = 0, lowCrestCount = 0, highCrestCount = 0;
        float minLufsMomentary = -100.0f, maxLufsMomentary = -100.0f;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;

            auto& state            = trackStates_[info.slotIndex];
            juce::String trackName = juce::String(info.trackName).trim();
            if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

            if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) {
                avgCrestFactor += telem.crestFactor;
                crestCount++;
                state.lastCrestFactor = telem.crestFactor;

                if (telem.crestFactor < 6.0f) {
                    lowCrestCount++;
                    if (now - lastCrestWarningUs_ > kWarningCooldownUs
                        && now - state.lastWarningUs > kTrackCooldownUs) {
                        state.lastWarningUs = now;
                        lastCrestWarningUs_ = now;
                        respondWithContext("\xE2\x9A\xA1 **" + trackName
                                               + "** poca din\xC3\xA1mica (crest: " + juce::String(telem.crestFactor, 1)
                                               + " dB). Prueba ratio 2:1 o sube threshold 2-3 dB.",
                                           trackName,
                                           MentorMessage::Type::Tip);
                    }
                }
                else if (telem.crestFactor > 24.0f) {
                    highCrestCount++;
                    if (now - lastCrestWarningUs_ > kWarningCooldownUs
                        && now - state.lastWarningUs > kTrackCooldownUs) {
                        state.lastWarningUs = now;
                        lastCrestWarningUs_ = now;
                        respondWithContext("\xE2\x9A\xA1 **" + trackName + "** mucha din\xC3\xA1mica (crest: "
                                               + juce::String(telem.crestFactor, 1)
                                               + " dB). Compresor 4:1 con attack 10ms puede ayudar.",
                                           trackName,
                                           MentorMessage::Type::Tip);
                    }
                }
            }

            if (telem.lufsMomentary > -80.0f) {
                if (telem.lufsMomentary < minLufsMomentary || minLufsMomentary == -100.0f)
                    minLufsMomentary = telem.lufsMomentary;
                if (telem.lufsMomentary > maxLufsMomentary || maxLufsMomentary == -100.0f)
                    maxLufsMomentary = telem.lufsMomentary;
            }
            state.lastLufsShort = telem.lufsShortTerm;
            state.lastRmsDb     = telem.rmsLeft;
        });

        if (crestCount > 0) {
            float globalCrest = avgCrestFactor / crestCount;
            if (now - lastDynamicWarningUs_ > kWarningCooldownUs) {
                if (globalCrest < 8.0f && lowCrestCount > crestCount / 2) {
                    lastDynamicWarningUs_ = now;
                    respondWith("\xF0\x9F\x93\x88 **Mezcla comprimida**: crest promedio " + juce::String(globalCrest, 1)
                                    + " dB. Revisa compresores.",
                                MentorMessage::Type::Warning);
                }
                else if (globalCrest > 18.0f && highCrestCount > crestCount / 3) {
                    lastDynamicWarningUs_ = now;
                    respondWith("\xF0\x9F\x93\x88 **Mezcla muy din\xC3\xA1mica**: crest promedio "
                                    + juce::String(globalCrest, 1) + " dB. Considera compresores suaves.",
                                MentorMessage::Type::Info);
                }
            }
            if (maxLufsMomentary > -30.0f && now - lastLoudnessWarningUs_ > kWarningCooldownUs) {
                lastLoudnessWarningUs_ = now;
                respondWith("\xF0\x9F\x94\x8A **Rango de loudness**: "
                                + juce::String(maxLufsMomentary - minLufsMomentary, 1)
                                + " LU. Busca m\xC3\xA1ximo 8-10 LU de diferencia.",
                            MentorMessage::Type::Info);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  FASE / ESPACIAL
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzePhaseReal()
    {
        auto now       = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        int phaseIssueTracks = 0;
        juce::String phaseTracks;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;

            juce::String trackName = juce::String(info.trackName).trim();
            if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

            auto& state           = trackStates_[info.slotIndex];
            float prevCorrelation = state.lastCorrelation;
            state.lastCorrelation = telem.correlation;

            if (telem.correlation < 0.3f && telem.rmsLeft > -30.0f) {
                phaseIssueTracks++;
                if (!phaseTracks.isEmpty()) phaseTracks += ", ";
                phaseTracks += trackName;

                if ((telem.correlation < prevCorrelation - 0.2f || telem.correlation < 0.0f)
                    && now - lastPhaseWarningUs_ > kWarningCooldownUs) {
                    lastPhaseWarningUs_ = now;
                    juce::String msg;
                    if (telem.correlation < 0.0f)
                        msg = "\xF0\x9F\x94\xAE **" + trackName + "** correlaci\xC3\xB3n negativa (" + juce::String(telem.correlation, 2) + ") \xE2\x80\x94 fases invertidas. Revisa micr\xC3\xB3" "fonos o procesado est\xC3\xA9reo.";
                    else
                        msg = "\xF0\x9F\x94\xAE **" + trackName + "** baja correlaci\xC3\xB3n ("
                              + juce::String(telem.correlation, 2) + "). Revisa panorama o efectos est\xC3\xA9reo.";
                    respondWithContext(msg, trackName, MentorMessage::Type::Warning);
                }
            }
        });

        if (phaseIssueTracks >= 3)
            respondWith("\xF0\x9F\x94\xAE Se detectaron problemas de fase en **" + juce::String(phaseIssueTracks)
                            + " pistas**. Revisa correlaci\xC3\xB3n en el panel de an\xC3\xA1lisis.",
                        MentorMessage::Type::Info);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  AN\xC3\x81LISIS GLOBAL
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeOverallMixReal()
    {
        auto& registry = sharedData_.getSlotRegistry();
        int active     = registry.activeCount();
        if (active == 0) return;

        int clippingCount = 0, nearClipCount = 0, noSignalCount = 0;
        float masterPeakEstimate = -100.0f;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;
            float peak         = juce::jmax(telem.peakLeft, telem.peakRight);
            masterPeakEstimate = juce::jmax(masterPeakEstimate, peak);
            if (peak > -0.5f) clippingCount++;
            else if (peak > -3.0f)
                nearClipCount++;
            if (peak < -60.0f) noSignalCount++;
        });

        LogHelper::writeToLog("[CoachEngine] OverallMix: " + juce::String(active) + " tracks, "
                              + juce::String(clippingCount) + " clipping, " + juce::String(nearClipCount)
                              + " near-clip, " + juce::String(noSignalCount)
                              + " silent, peak=" + juce::String(masterPeakEstimate, 1) + " dB");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  ENMASCARAMIENTO ESPECTRAL
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeSpectralMaskingReal()
    {
        auto now       = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();
        if (registry.activeCount() < 2) return;

        struct TrackSpec
        {
            int slotIndex;
            juce::String name;
            float spectrum[kNumSpectrumBins];
            float rmsDb;
        };

        std::vector<TrackSpec> tracks;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;
            float sum = 0.0f;
            for (int fi = 0; fi < kNumSpectrumBins; ++fi) sum += telem.spectrum[fi];
            if (sum < 0.01f || telem.rmsLeft < -50.0f) return;
            TrackSpec ts;
            ts.slotIndex = info.slotIndex;
            ts.name      = juce::String(info.trackName).trim();
            if (ts.name.isEmpty()) ts.name = "Pista " + juce::String(info.slotIndex + 1);
            for (int fi = 0; fi < kNumSpectrumBins; ++fi) ts.spectrum[fi] = telem.spectrum[fi];
            ts.rmsDb = telem.rmsLeft;
            tracks.push_back(ts);
        });

        if (tracks.size() < 2) return;

        struct MaskBand
        {
            const char* name;
            int binStart, binEnd;
        };

        const MaskBand bands[] = {
            {"sub-graves (20-70 Hz)", 0, 1},
            {"graves bajos (70-150 Hz)", 1, 3},
            {"graves (150-300 Hz)", 3, 6},
            {"medios bajos (300-600 Hz)", 6, 13},
            {"medios (600-1.2 kHz)", 13, 26},
            {"medios altos (1.2-2.5 kHz)", 26, 53},
            {"presencia (2.5-5 kHz)", 53, 106},
            {"presencia alta (5-10 kHz)", 106, 213},
            {"agudos (10-16 kHz)", 213, 341},
            {"aire (16-20 kHz)", 341, 426},
        };
        constexpr int kNumBands = sizeof(bands) / sizeof(bands[0]);

        struct MaskPair
        {
            int idxA, idxB;
            float overlapScore;
            int worstBand;
            float energyA, energyB;
            juce::String bandName;
        };

        std::vector<MaskPair> pairs;
        auto toDb = [](float v) -> float { return (v > 0.001f) ? juce::Decibels::gainToDecibels(v) : -80.0f; };

        for (size_t i = 0; i < tracks.size(); ++i) {
            for (size_t j = i + 1; j < tracks.size(); ++j) {
                float totalOverlap = 0.0f;
                int worstBand      = -1;
                float worstDiff    = 0.0f;
                float worstEnergyA = 0.0f, worstEnergyB = 0.0f;
                juce::String worstBandName;
                for (int b = 0; b < kNumBands; ++b) {
                    float energyA = 0.0f, energyB = 0.0f;
                    int binStart = bands[b].binStart, binEnd = juce::jmin(bands[b].binEnd, kNumSpectrumBins);
                    int count = binEnd - binStart;
                    for (int bi = binStart; bi < binEnd; ++bi) {
                        energyA += tracks[i].spectrum[bi];
                        energyB += tracks[j].spectrum[bi];
                    }
                    if (count > 0) {
                        energyA /= (float)count;
                        energyB /= (float)count;
                    }
                    if (energyA > 0.01f && energyB > 0.01f) {
                        float bandOverlap = juce::jmin(energyA, energyB) * juce::jmax(energyA, energyB) * 10.0f;
                        totalOverlap += bandOverlap;
                        float diffDb = std::fabs(toDb(energyA) - toDb(energyB));
                        if (diffDb > worstDiff) {
                            worstDiff     = diffDb;
                            worstBand     = b;
                            worstEnergyA  = energyA;
                            worstEnergyB  = energyB;
                            worstBandName = bands[b].name;
                        }
                    }
                }
                if (totalOverlap > 0.03f && worstBand >= 0)
                    pairs.push_back(
                        {(int)i, (int)j, totalOverlap, worstBand, worstEnergyA, worstEnergyB, worstBandName});
            }
        }

        if (pairs.empty()) return;
        std::sort(pairs.begin(), pairs.end(), [](const MaskPair& a, const MaskPair& b) {
            return a.overlapScore > b.overlapScore;
        });

        if (now - lastMaskingWarningUs_ > kWarningCooldownUs) {
            lastMaskingWarningUs_ = now;
            int numToReport       = juce::jmin((int)pairs.size(), 3);
            for (int p = 0; p < numToReport; ++p) {
                auto& mp = pairs[p];
                auto &tA = tracks[mp.idxA], &tB = tracks[mp.idxB];
                float aDb = toDb(mp.energyA), bDb = toDb(mp.energyB);
                juce::String ctx = tA.name + " / " + tB.name;
                juce::String advice;
                if (aDb > bDb + 6.0f)
                    advice = "\xF0\x9F\x94\x8A **" + tA.name + "** enmascara a **" + tB.name + "** en " + mp.bandName;
                else if (bDb > aDb + 6.0f)
                    advice = "\xF0\x9F\x94\x8A **" + tB.name + "** enmascara a **" + tA.name + "** en " + mp.bandName;
                else
                    advice = "\xF0\x9F\x94\x8A **" + tA.name + "** y **" + tB.name + "** compiten en " + mp.bandName;
                advice += ". Considera EQ carving o panoramas opuestos.";
                respondWithContext(advice, ctx, MentorMessage::Type::Tip);
            }
            if ((int)pairs.size() > numToReport)
                respondWith("\xF0\x9F\x94\x8A **" + juce::String((int)pairs.size() - numToReport)
                                + " par(es) adicional(es)** con enmascaramiento.",
                            MentorMessage::Type::Info);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  LEGACY wrappers
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeGainStaging()
    {
        analyzeGainStagingReal();
    }

    void CoachEngine::analyzeOrganisation()
    {
        analyzeOrganisationReal();
    }

    void CoachEngine::analyzeTonalBalance()
    {
        analyzeTonalBalanceReal();
    }

    void CoachEngine::analyzeDynamics()
    {
        analyzeDynamicsReal();
    }

    void CoachEngine::analyzeSpatial()
    {
        analyzePhaseReal();
    }

} // namespace mixcoach

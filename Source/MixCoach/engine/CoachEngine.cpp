#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>
#include <cmath>

namespace mixcoach {

CoachEngine::CoachEngine(PhaseManager& phaseManager, SharedData& sharedData)
    : phaseManager_(phaseManager)
    , sharedData_(sharedData)
{
    // Inicializar estados de pista
    for (auto& state : trackStates_) {
        state = TrackAnalysisState{};
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════

TrackTelemetry CoachEngine::getLatestTelemetry(int slotIndex) const
{
    auto& registry = sharedData_.getSlotRegistry();
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return TrackTelemetry{};
    return registry.getTelemetry(slotIndex).latest();
}

void CoachEngine::respondWith(const juce::String& text, MentorMessage::Type type)
{
    MentorMessage msg;
    msg.type      = type;
    msg.text      = text.toStdString();
    msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
    msg.context   = "MixCoach";
    sharedData_.pushMessage(msg);
    LogHelper::writeToLog("[CoachEngine] Mensaje: " + text.substring(0, 80));
}

void CoachEngine::respondWithContext(const juce::String& text,
                                     const juce::String& context,
                                     MentorMessage::Type type)
{
    MentorMessage msg;
    msg.type      = type;
    msg.text      = text.toStdString();
    msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
    msg.context   = context.toStdString();
    sharedData_.pushMessage(msg);
    LogHelper::writeToLog("[CoachEngine] Mensaje (" + context + "): "
                          + text.substring(0, 80));
}

// ═══════════════════════════════════════════════════════════════════════════
//  ANÁLISIS PERIÓDICO — Llama a los análisis según la fase actual
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::periodicAnalysis()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;

    // Throttle: solo cada ~8s
    if (now - lastPeriodicAnalysisUs_ < kAnalysisIntervalUs)
        return;
    lastPeriodicAnalysisUs_ = now;

    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0)
        return;

    LogHelper::writeToLog("[CoachEngine] Análisis periódico iniciado ("
                          + juce::String(registry.activeCount()) + " pistas activas)");

    // Siempre ejecutar análisis global independientemente de la fase
    // Siempre ejecutar análisis de enmascaramiento espectral (independiente de fase)
    analyzeSpectralMaskingReal();

    analyzeOverallMixReal();

    // Análisis específico según la fase actual
    auto phase = phaseManager_.getCurrentPhase();
    switch (phase) {
        case MentorPhase::GainStaging:
            analyzeGainStagingReal();
            break;
        case MentorPhase::Organisation:
            analyzeOrganisationReal();
            break;
        case MentorPhase::TonalBalance:
            analyzeTonalBalanceReal();
            break;
        case MentorPhase::Dynamics:
            analyzeDynamicsReal();
            break;
        case MentorPhase::Spatial:
            analyzePhaseReal();
            break;
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  GAIN STAGING — Picos, clipping, headroom
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeGainStagingReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    float maxGlobalPeak = -100.0f;
    int clippingCount = 0;
    int lowSignalCount = 0;
    juce::String clippingTracks;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
        maxGlobalPeak = juce::jmax(maxGlobalPeak, peakDb);

        auto& state = trackStates_[info.slotIndex];
        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

        // ─── Clipping detection (peak > -0.5dB) ───────────────────────────
        if (peakDb > -0.5f) {
            clippingCount++;
            if (!state.wasClipping) {
                state.wasClipping = true;
                if (now - lastPeakWarningUs_ > kWarningCooldownUs) {
                    lastPeakWarningUs_ = now;
                    respondWithContext(
                        "🔴 **" + trackName + "** está haciendo clipping: **"
                        + juce::String(peakDb, 1) + " dB**. "
                        "Reduce el gain inmediatamente — el audio se está distorsionando.",
                        trackName,
                        MentorMessage::Type::Warning);
                }
            }
        }
        // ─── Pre-clipping warning (peak > -3dB) ───────────────────────────
        else if (peakDb > -3.0f && peakDb > state.lastPeakDb) {
            // Solo advertir si va en aumento (se acerca al clipping)
            if (now - state.lastWarningUs > kTrackCooldownUs) {
                state.lastWarningUs = now;
                respondWithContext(
                    "⚠️ **" + trackName + "** tiene picos de **" + juce::String(peakDb, 1)
                    + " dB**. Considera reducir el gain unos 3-6 dB para tener headroom.",
                    trackName,
                    MentorMessage::Type::Tip);
            }
        }
        // ─── Señal muy baja ───────────────────────────────────────────────
        else if (peakDb < -30.0f) {
            lowSignalCount++;
            if (!state.wasLowSignal && registry.activeCount() > 1) {
                // Solo advertir si hay otras pistas con señal normal
                state.wasLowSignal = true;
                respondWithContext(
                    "🔇 **" + trackName + "** tiene señal muy baja (" + juce::String(peakDb, 1)
                    + " dB). ¿Olvidaste subir el fader o abrir el plugin?",
                    trackName,
                    MentorMessage::Type::Info);
            }
        } else {
            state.wasClipping = false;
            state.wasLowSignal = false;
        }

        // ─── Crest factor analysis ────────────────────────────────────────
        if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) {
            state.lastCrestFactor = telem.crestFactor;
        }

        state.lastPeakDb = peakDb;
        state.lastRmsDb  = telem.rmsLeft;
    });

    // ─── Resumen global de headroom ───────────────────────────────────────
    if (maxGlobalPeak > -6.0f && maxGlobalPeak < -0.5f) {
        if (now - lastHeadroomWarningUs_ > kWarningCooldownUs) {
            lastHeadroomWarningUs_ = now;
            float headroom = -maxGlobalPeak;
            respondWith(
                "📊 **Headroom: " + juce::String(headroom, 1) + " dB** — "
                "la pista con más nivel alcanza **" + juce::String(maxGlobalPeak, 1)
                + " dB**. El rango ideal es -6 dB a -3 dB de pico en el master.",
                MentorMessage::Type::Tip);
        }
    } else if (maxGlobalPeak < -18.0f && registry.activeCount() >= 3) {
        if (now - lastHeadroomWarningUs_ > kWarningCooldownUs) {
            lastHeadroomWarningUs_ = now;
            respondWith(
                "📊 Las pistas están muy bajas (pico máximo: **" + juce::String(maxGlobalPeak, 1)
                + " dB**). Sube los faders de gain hasta que el master marque "
                "entre -12 dB y -6 dB.",
                MentorMessage::Type::Info);
        }
    }

    // Log de diagnóstico
    if (clippingCount > 0 || lowSignalCount > 0) {
        LogHelper::writeToLog("[CoachEngine] GainStaging: "
                              + juce::String(clippingCount) + " clipping, "
                              + juce::String(lowSignalCount) + " baja señal, "
                              + "pico global=" + juce::String(maxGlobalPeak, 1) + " dB");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ORGANIZACIÓN — Conteo de pistas, buses, colores
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeOrganisationReal()
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) return;

    int bussedCount = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        if (info.bus != BusType::None)
            bussedCount++;
    });

    int unnamedCount = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty() || name.startsWith("Pista"))
            unnamedCount++;
    });

    if (unnamedCount > 0) {
        respondWith(
            "📝 **" + juce::String(unnamedCount) + " pista(s)** sin nombre. "
            "Nombrar cada pista ayuda a mantener la mezcla organizada.",
            MentorMessage::Type::Info);
    }

    if (active >= 3 && bussedCount < active / 2) {
        respondWith(
            "🔗 Solo **" + juce::String(bussedCount) + "/" + juce::String(active)
            + "** pistas tienen bus asignado. Agrupar por familias "
            "(batería, bajo, voces) facilita el procesamiento por grupos.",
            MentorMessage::Type::Tip);
    }

    LogHelper::writeToLog("[CoachEngine] Organización: "
                          + juce::String(active) + " activas, "
                          + juce::String(bussedCount) + " con bus, "
                          + juce::String(unnamedCount) + " sin nombre");
}

// ═══════════════════════════════════════════════════════════════════════════
//  BALANCE TONAL — Espectro, comparación de bandas
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeTonalBalanceReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    bool hasSpectrumData = false;
    float avgSubBass   = -100.0f; // bins 0-1:   ~20-47Hz    (deep sub)
    float avgLowBass   = -100.0f; // bins 1-3:   ~47-141Hz   (sub-bass)
    float avgBass      = -100.0f; // bins 3-6:   ~141-281Hz  (bass fundamental)
    float avgLowMids   = -100.0f; // bins 6-13:  ~281-609Hz  (low mids)
    float avgMids      = -100.0f; // bins 13-25: ~609-1172Hz (mids)
    float avgHighMids  = -100.0f; // bins 25-53: ~1.17-2.48kHz
    float avgPresence  = -100.0f; // bins 53-106:~2.48-4.97kHz
    float avgHighs     = -100.0f; // bins 106-213:~4.97-9.98kHz
    float avgAir       = -100.0f; // bins 213-426:~9.98-19.97kHz
    int trackCount = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        // Verificar si hay datos espectrales válidos
        float spectrumSum = 0.0f;
        for (int fi = 0; fi < kNumSpectrumBins; ++fi)
            spectrumSum += telem.spectrum[fi];

        if (spectrumSum < 0.01f) return; // Sin datos espectrales recientes
        hasSpectrumData = true;
        trackCount++;

        // Promedio por bandas
        auto bandAvg = [&](int start, int end) -> float {
            float s = 0.0f;
            for (int fi = start; fi < end && fi < kNumSpectrumBins; ++fi)
                s += telem.spectrum[fi];
            int n = juce::jmin(end - start, kNumSpectrumBins - start);
            return (n > 0) ? (s / n) : -100.0f;
        };

        float subBass  = bandAvg(0, 1);
        float lowBass  = bandAvg(1, 3);
        float bass     = bandAvg(3, 6);
        float lowMids  = bandAvg(6, 13);
        float mids     = bandAvg(13, 25);
        float highMids = bandAvg(25, 53);
        float presence = bandAvg(53, 106);
        float highs    = bandAvg(106, 213);
        float air      = bandAvg(213, 426);

        auto accumMax = [](float& acc, float v) {
            if (v > acc) acc = v;
        };

        accumMax(avgSubBass,  subBass);
        accumMax(avgLowBass,  lowBass);
        accumMax(avgBass,     bass);
        accumMax(avgLowMids,  lowMids);
        accumMax(avgMids,     mids);
        accumMax(avgHighMids, highMids);
        accumMax(avgPresence, presence);
        accumMax(avgHighs,    highs);
        accumMax(avgAir,      air);
    });

    if (!hasSpectrumData || trackCount == 0)
        return;

    // ─── Detectar desbalances espectrales ─────────────────────────────────
    // Convertir a dBFS (los bins están normalizados 0..1, convertir a dB)
    auto toDb = [](float v) -> float {
        return (v > 0.001f) ? juce::Decibels::gainToDecibels(v) : -60.0f;
    };

    float subBassDb  = toDb(avgSubBass);
    float lowBassDb  = toDb(avgLowBass);
    float bassDb     = toDb(avgBass);
    float lowMidsDb  = toDb(avgLowMids);
    float midsDb     = toDb(avgMids);
    float highMidsDb = toDb(avgHighMids);
    float presenceDb = toDb(avgPresence);
    float highsDb    = toDb(avgHighs);
    float airDb      = toDb(avgAir);

    // Log de diagnóstico
    LogHelper::writeToLog("[CoachEngine] Espectro (dBFS): sub="
                          + juce::String(subBassDb, 1) + " lbass="
                          + juce::String(lowBassDb, 1) + " bass="
                          + juce::String(bassDb, 1) + " lmids="
                          + juce::String(lowMidsDb, 1) + " mids="
                          + juce::String(midsDb, 1) + " hmids="
                          + juce::String(highMidsDb, 1) + " pres="
                          + juce::String(presenceDb, 1) + " highs="
                          + juce::String(highsDb, 1) + " air="
                          + juce::String(airDb, 1));

    // Chequeos tonales
    if (now - lastTonalWarningUs_ > kWarningCooldownUs) {
        bool warned = false;

        // ═══ Demasiados graves (sub-bass mucho más alto que mids) ══════════
        if ((subBassDb > -20.0f || lowBassDb > -15.0f)
            && subBassDb > midsDb + 10.0f) {
            warned = true;
            respondWith(
                "🎛️ **Exceso de graves**: el sub-bass ("
                + juce::String(subBassDb, 1) + " dBFS) domina sobre los medios ("
                + juce::String(midsDb, 1) + " dBFS). Prueba un HPF en el bajo "
                "alrededor de 40-60 Hz o reduce el nivel del sub-bass.",
                MentorMessage::Type::Tip);
        }
        // ═══ Faltan graves (bajo mucho más bajo que mids) ══════════════════
        else if (bassDb < -35.0f && lowBassDb < -30.0f && midsDb > -25.0f) {
            warned = true;
            respondWith(
                "🎛️ **Faltan graves**: el rango de bajos ("
                + juce::String(bassDb, 1) + " dBFS) es muy bajo comparado con los medios ("
                + juce::String(midsDb, 1) + " dBFS). "
                "Revisa que el bajo y el bombo tengan presencia en el mezclador.",
                MentorMessage::Type::Info);
        }
        // ═══ Mezcla opaca (poca presencia en highs) ════════════════════════
        else if (presenceDb < -35.0f && highsDb < -40.0f && airDb < -45.0f && midsDb > -25.0f) {
            warned = true;
            respondWith(
                "🎛️ **Mezcla opaca**: hay poca energía en frecuencias altas ("
                + juce::String(highsDb, 1) + " dBFS). "
                "Prueba un realce suave de EQ shelving en 8-12 kHz o agrega "
                "aire con un excitador armónico.",
                MentorMessage::Type::Tip);
        }
        // ═══ Demasiados agudos (presencia domina) ══════════════════════════
        else if (presenceDb > -15.0f && presenceDb > midsDb + 8.0f) {
            warned = true;
            respondWith(
                "🎛️ **Exceso de agudos**: la presencia ("
                + juce::String(presenceDb, 1) + " dBFS) domina la mezcla. "
                "Prueba un filtro low-pass suave en 12-14 kHz o reduce "
                "el nivel de las pistas agudas.",
                MentorMessage::Type::Tip);
        }
        // ═══ Nuevo: Exceso de aire (demasiada energía >10kHz) ══════════════
        else if (airDb > -15.0f && airDb > highMidsDb + 6.0f && presenceDb > -20.0f) {
            warned = true;
            respondWith(
                "🎛️ **Exceso de aire**: la banda de 10-20 kHz ("
                + juce::String(airDb, 1) + " dBFS) es muy prominente. "
                "Reduce el shelving de alta frecuencia o aplica un low-pass "
                "suave en 16-18 kHz para evitar fatiga auditiva.",
                MentorMessage::Type::Tip);
        }

        if (warned)
            lastTonalWarningUs_ = now;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  DINÁMICA — Crest factor, LUFS, loudness range
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeDynamicsReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    float avgCrestFactor = 0.0f;
    int crestCount = 0;
    int lowCrestCount = 0;
    int highCrestCount = 0;
    float minLufsMomentary = -100.0f;
    float maxLufsMomentary = -100.0f;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        auto& state = trackStates_[info.slotIndex];
        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

        // ─── Crest Factor ─────────────────────────────────────────────────
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
                    respondWithContext(
                        "⚡ **" + trackName + "** tiene poca dinámica (crest factor: "
                        + juce::String(telem.crestFactor, 1) + " dB). "
                        "Si está comprimida, prueba con un ratio más bajo (2:1) "
                        "o reduce el threshold 2-3 dB.",
                        trackName,
                        MentorMessage::Type::Tip);
                }
            } else if (telem.crestFactor > 24.0f) {
                highCrestCount++;
                if (now - lastCrestWarningUs_ > kWarningCooldownUs
                    && now - state.lastWarningUs > kTrackCooldownUs) {
                    state.lastWarningUs = now;
                    lastCrestWarningUs_ = now;
                    respondWithContext(
                        "⚡ **" + trackName + "** tiene mucha dinámica (crest factor: "
                        + juce::String(telem.crestFactor, 1) + " dB). "
                        "Un compresor con ratio 4:1 y attack rápido (~10ms) "
                        "puede ayudar a controlar los picos.",
                        trackName,
                        MentorMessage::Type::Tip);
                }
            }
        }

        // ─── LUFS ─────────────────────────────────────────────────────────
        if (telem.lufsMomentary > -80.0f) {
            if (telem.lufsMomentary < minLufsMomentary || minLufsMomentary == -100.0f)
                minLufsMomentary = telem.lufsMomentary;
            if (telem.lufsMomentary > maxLufsMomentary || maxLufsMomentary == -100.0f)
                maxLufsMomentary = telem.lufsMomentary;
        }

        state.lastLufsShort = telem.lufsShortTerm;
        state.lastRmsDb = telem.rmsLeft;
    });

    // ─── Reporte de dinámica global ───────────────────────────────────────
    if (crestCount > 0) {
        float globalCrest = avgCrestFactor / crestCount;

        if (now - lastDynamicWarningUs_ > kWarningCooldownUs) {
            if (globalCrest < 8.0f && lowCrestCount > crestCount / 2) {
                lastDynamicWarningUs_ = now;
                respondWith(
                    "📈 **Mezcla comprimida**: crest factor promedio de "
                    + juce::String(globalCrest, 1) + " dB ("
                    + juce::String(lowCrestCount) + "/" + juce::String(crestCount)
                    + " pistas con poca dinámica). "
                    "Revisa los compresores — podrías estar sobre-comprimiendo.",
                    MentorMessage::Type::Warning);
            } else if (globalCrest > 18.0f && highCrestCount > crestCount / 3) {
                lastDynamicWarningUs_ = now;
                respondWith(
                    "📈 **Mezcla muy dinámica**: crest factor promedio de "
                    + juce::String(globalCrest, 1) + " dB. "
                    "Considera compresores suaves en las pistas más dinámicas "
                    "para nivelar la mezcla.",
                    MentorMessage::Type::Info);
            }
        }

        // ─── Reporte de LUFS (si hay datos) ──────────────────────────────
        if (maxLufsMomentary > -30.0f && now - lastLoudnessWarningUs_ > kWarningCooldownUs) {
            lastLoudnessWarningUs_ = now;
            float range = maxLufsMomentary - minLufsMomentary;
            respondWith(
                "🔊 **Rango de loudness**: " + juce::String(range, 1)
                + " LU (de " + juce::String(minLufsMomentary, 1)
                + " a " + juce::String(maxLufsMomentary, 1)
                + " LUFS). Para mezcla balanceada, busca que las pistas "
                "tengan loudness similar, con máximo 8-10 LU de diferencia.",
                MentorMessage::Type::Info);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  FASE / ESPACIAL — Correlación, panoramas, fase estéreo
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzePhaseReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    int phaseIssueTracks = 0;
    juce::String phaseTracks;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

        // ─── Correlación (mono compatibility check) ───────────────────────
        auto& state = trackStates_[info.slotIndex];
        float prevCorrelation = state.lastCorrelation;
        state.lastCorrelation = telem.correlation;

        if (telem.correlation < 0.3f && telem.rmsLeft > -30.0f) {
            phaseIssueTracks++;
            if (!phaseTracks.isEmpty()) phaseTracks += ", ";
            phaseTracks += trackName;

            // Solo advertir si empeora significativamente (respecto al valor ANTERIOR)
            if ((telem.correlation < prevCorrelation - 0.2f || telem.correlation < 0.0f)
                && now - lastPhaseWarningUs_ > kWarningCooldownUs) {
                lastPhaseWarningUs_ = now;
                juce::String msg;
                if (telem.correlation < 0.0f) {
                    msg = "🔮 **" + trackName + "** tiene correlación negativa ("
                        + juce::String(telem.correlation, 2) + ") — "
                        "las fases están invertidas. Revisa los micrófonos "
                        "o el procesado estéreo.";
                } else {
                    msg = "🔮 **" + trackName + "** tiene baja correlación ("
                        + juce::String(telem.correlation, 2) + "). "
                        "Si la pista suena hueca al monitorear en mono, "
                        "revisa el panorama o efectos estéreo.";
                }
                respondWithContext(msg, trackName, MentorMessage::Type::Warning);
            }
        }
    });

    // Resumen global de fase
    if (phaseIssueTracks >= 3) {
        respondWith(
            "🔮 Se detectaron problemas de fase en **" + juce::String(phaseIssueTracks)
            + " pistas**. Revisa la correlación en el panel de análisis "
            "y considera usar correladores de fase.",
            MentorMessage::Type::Info);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ANÁLISIS GLOBAL — Resumen del estado general de la mezcla
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeOverallMixReal()
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) return;

    // Recolectar métricas globales
    int clippingCount = 0;
    int nearClipCount = 0;
    int noSignalCount = 0;
    int negativePhaseCount = 0;
    float masterPeakEstimate = -100.0f;
    float masterRmsSum = 0.0f;
    int rmsCount = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float peak = juce::jmax(telem.peakLeft, telem.peakRight);
        masterPeakEstimate = juce::jmax(masterPeakEstimate, peak);

        if (peak > -0.5f) clippingCount++;
        else if (peak > -3.0f) nearClipCount++;

        if (peak < -60.0f) noSignalCount++;

        if (telem.correlation < 0.0f) negativePhaseCount++;

        if (telem.rmsLeft > -60.0f) {
            masterRmsSum += (telem.rmsLeft + telem.rmsRight) * 0.5f;
            rmsCount++;
        }
    });

    // Log de diagnóstico periódico
    LogHelper::writeToLog("[CoachEngine] OverallMix: "
                          + juce::String(active) + " tracks, "
                          + juce::String(clippingCount) + " clipping, "
                          + juce::String(nearClipCount) + " near-clip, "
                          + juce::String(noSignalCount) + " silent, "
                          + "peak=" + juce::String(masterPeakEstimate, 1) + " dB");
}

// ═══════════════════════════════════════════════════════════════════════════
//  MANEJO DE MENSAJES DEL USUARIO
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::handleUserMessage(const juce::String& message)
{
    auto lower = message.toLowerCase();

    // Comandos de sistema
    if (lower.startsWith("/")) {
        executeCommand(message);
        return;
    }

    // ─── Respuesta contextual según la fase ───────────────────────────────
    auto phase = phaseManager_.getCurrentPhase();

    // Si hay telemetría y pistas activas, responder con datos reales
    auto& registry = sharedData_.getSlotRegistry();
    bool hasTelemetry = (registry.activeCount() > 0);

    switch (phase) {
        case MentorPhase::Welcome:
            respondWith(
                "🎧 ¡Bienvenido a MixCoach! Estoy aquí para guiarte en tu mezcla.\n\n"
                "Actualmente **" + juce::String(registry.activeCount())
                + " pista(s) activa(s)** detectada(s).\n\n"
                "Puedes preguntarme:\n"
                "• \"Cómo va la mezcla?\" — análisis completo\n"
                "• \"Revisa gain staging\" — niveles y clipping\n"
                "• \"Dame un tip\" — consejo proactivo\n"
                "• /next — avanzar a la siguiente fase",
                MentorMessage::Type::Question);
            break;

        case MentorPhase::GainStaging:
            if (hasTelemetry) {
                analyzeGainStagingReal();
                respondWith(
                    "📊 Puedes preguntar \"cómo están los niveles\" o "
                    "\"hay clipping?\" para un análisis detallado.",
                    MentorMessage::Type::Info);
            } else {
                respondWith(
                    "Aún no detecto pistas activas. Asegúrate de tener "
                    "Messengers cargados en tus pistas.",
                    MentorMessage::Type::Info);
            }
            break;

        case MentorPhase::Organisation:
            if (hasTelemetry) {
                analyzeOrganisationReal();
            } else {
                respondWith(
                    "En fase de Organización. Recomiendo:\n"
                    "1. Nombra cada pista descriptivamente\n"
                    "2. Asigna colores por familia\n"
                    "3. Agrupa en buses virtuales",
                    MentorMessage::Type::Tip);
            }
            break;

        case MentorPhase::TonalBalance:
            if (hasTelemetry) {
                analyzeTonalBalanceReal();
            } else {
                respondWith(
                    "En fase de Balance Tonal. Revisa el espectro y "
                    "compáralo con referencias de tu género.",
                    MentorMessage::Type::Info);
            }
            break;

        case MentorPhase::Dynamics:
            if (hasTelemetry) {
                analyzeDynamicsReal();
            } else {
                respondWith(
                    "En fase de Dinámica. Considera compresores en buses "
                    "y limitador en el master (solo 1-2 dB).",
                    MentorMessage::Type::Tip);
            }
            break;

        case MentorPhase::Spatial:
            if (hasTelemetry) {
                analyzePhaseReal();
            }
            respondWith(
                "En fase de Espacialidad. Trabaja panoramas, reverbs, "
                "y efectos de profundidad.",
                MentorMessage::Type::Tip);
            break;
    }

    // Verificar logros
    auto activeCount = registry.activeCount();
    if (activeCount >= 1)  phaseManager_.unlockAchievement(Achievement::FirstTrack);
    if (activeCount >= 5)  phaseManager_.unlockAchievement(Achievement::FiveTracks);
    if (activeCount >= 10) phaseManager_.unlockAchievement(Achievement::TenTracks);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TIP PROACTIVO — Genera un tip basado en la fase y datos actuales
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::generateProactiveTip()
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) {
        respondWith(
            "💡 Aún no hay pistas activas. Carga Messengers en tus pistas "
            "para empezar a recibir análisis en tiempo real.",
            MentorMessage::Type::Tip);
        return;
    }

    // Si hay datos, dar tip basado en la fase y métricas reales
    auto phase = phaseManager_.getCurrentPhase();
    switch (phase) {
        case MentorPhase::GainStaging:
            analyzeGainStagingReal();
            if (juce::Time::getMillisecondCounter() * 1000 - lastPeakWarningUs_
                > kWarningCooldownUs) {
                respondWith(
                    "💡 Tip rápido: revisa que el fader de ganancia de cada pista "
                    "permita picos de -18 dB a -12 dB en el submix antes de "
                    "tocar el fader de volumen.",
                    MentorMessage::Type::Tip);
            }
            break;

        case MentorPhase::Organisation:
            analyzeOrganisationReal();
            break;

        case MentorPhase::TonalBalance:
            respondWith(
                "💡 Para evaluar el balance tonal, revisa el Analyzer "
                "(pestaña 2). Busca una curva suave de menos de 3 dB/octava "
                "de diferencia entre bandas adyacentes.",
                MentorMessage::Type::Tip);
            analyzeTonalBalanceReal();
            break;

        case MentorPhase::Dynamics:
            analyzeDynamicsReal();
            break;

        case MentorPhase::Spatial:
            analyzePhaseReal();
            break;

        default:
            respondWith(
                "💡 Escribe /help para ver comandos disponibles o pregúntame "
                "\"cómo va la mezcla?\" para un análisis completo.",
                MentorMessage::Type::Tip);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PROGRESO Y LOGROS
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::checkProgress()
{
    auto phase = phaseManager_.getCurrentPhase();
    auto progress = phaseManager_.getPhaseProgress(phase);
    auto& registry = sharedData_.getSlotRegistry();

    respondWith(
        juce::String("📈 **Progreso** en fase '")
        + phaseManager_.phaseDescription(phase) + "': "
        + juce::String(static_cast<int>(progress * 100.0f)) + "%\n"
        + "Pistas activas: " + juce::String(registry.activeCount()) + "\n"
        + "Logros: " + juce::String(phaseManager_.getAchievementCount()),
        MentorMessage::Type::Info);

    if (phaseManager_.isPhaseComplete(phase)) {
        respondWith(
            "🎉 ¡Has completado esta fase! Escribe **/next** para avanzar "
            "a la siguiente.",
            MentorMessage::Type::Achievement);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  COMANDOS
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::executeCommand(const juce::String& command)
{
    auto lower = command.toLowerCase();

    if (lower == "/next" || lower == "/avanzar") {
        phaseManager_.advanceToNextPhase();
        auto phase = phaseManager_.getCurrentPhase();
        respondWith("✅ **Avanzando a fase:** " + juce::String(phaseNames[static_cast<int>(phase)]),
                    MentorMessage::Type::Achievement);
        respondWith("📋 " + juce::String(phaseManager_.phaseDescription(phase)),
                    MentorMessage::Type::Tip);
    }
    else if (lower == "/status" || lower == "/progreso") {
        checkProgress();
    }
    else if (lower == "/analisis" || lower == "/analyze") {
        respondWith(
            "🔍 Ejecutando análisis completo de la mezcla...",
            MentorMessage::Type::Info);
        analyzeGainStagingReal();
        analyzeTonalBalanceReal();
        analyzeDynamicsReal();
        analyzePhaseReal();
        analyzePhaseReal();
        analyzeSpectralMaskingReal();
        analyzeOverallMixReal();
    }
    else if (lower == "/help" || lower == "/ayuda") {
        respondWith(
            "**Comandos disponibles:**\n"
            "/next — Avanzar a la siguiente fase\n"
            "/status — Ver progreso actual\n"
            "/analisis — Análisis completo de la mezcla\n"
            "/help — Mostrar esta ayuda\n\n"
            "También puedes preguntar:\n"
            "• \"Cómo va la mezcla?\"\n"
            "• \"Hay clipping?\"\n"
            "• \"Revisa dinámica\"\n"
            "• \"Problemas de fase?\"",
            MentorMessage::Type::Info);
    }
    else {
        respondWith(
            "❓ Comando no reconocido. Escribe **/help** para ver los disponibles.",
            MentorMessage::Type::Warning);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ANUNCIO DE NUEVA PISTA (desde Messenger)
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::announceNewTrack(int slotIndex, const juce::String& trackName,
                                   const juce::Colour& colour)
{
    juce::ignoreUnused(slotIndex, colour);

    auto& registry = sharedData_.getSlotRegistry();
    auto telem = getLatestTelemetry(slotIndex);

    juce::String msg = "🎛️ **Nuevo Messenger detectado:** " + trackName.trim() + "\n\n";

    // Si aún no hay telemetría, mostrar mensaje genérico
    if (telem.timestamp == 0) {
        msg += "Esperando datos de telemetría... (aparecerán en unos segundos)\n";
    } else {
        msg += "Niveles actuales:\n";
        msg += "• Peak: **" + juce::String(telem.peakLeft, 1) + " dB**\n";
        msg += "• RMS: **" + juce::String(telem.rmsLeft, 1) + " dB**\n";
    }

    if (telem.crestFactor > 0.0f)
        msg += "• Crest factor: **" + juce::String(telem.crestFactor, 1) + " dB**\n";

    if (telem.correlation < 1.0f)
        msg += "• Correlación: **" + juce::String(telem.correlation, 2) + "**\n";

    if (registry.activeCount() >= 3) {
        msg += "\n📊 Ya tienes **" + juce::String(registry.activeCount())
               + " pistas** activas — la mezcla empieza a tomar forma.";
    }

    respondWith(msg, MentorMessage::Type::Info);

    // Si estamos en Welcome, avanzar automáticamente a GainStaging
    if (phaseManager_.getCurrentPhase() == MentorPhase::Welcome) {
        respondWith(
            "🚀 ¡Excelente! Como ya tienes pistas, pasamos directo a "
            "**Gain Staging** para ajustar niveles.",
            MentorMessage::Type::Tip);
        phaseManager_.advanceToNextPhase();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENMASCARAMIENTO ESPECTRAL — Pares de pistas que compiten en frecuencia
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeSpectralMaskingReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    if (registry.activeCount() < 2)
        return;

    // ─── Recolectar espectro de todas las pistas activas ──────────────────
    struct TrackSpec {
        int slotIndex;
        juce::String name;
        float spectrum[kNumSpectrumBins];
        float rmsDb;
    };

    std::vector<TrackSpec> tracks;
    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float sum = 0.0f;
        for (int fi = 0; fi < kNumSpectrumBins; ++fi)
            sum += telem.spectrum[fi];
        if (sum < 0.01f || telem.rmsLeft < -50.0f) return;

        TrackSpec ts;
        ts.slotIndex = info.slotIndex;
        ts.name = juce::String(info.trackName).trim();
        if (ts.name.isEmpty())
            ts.name = "Pista " + juce::String(info.slotIndex + 1);
        for (int fi = 0; fi < kNumSpectrumBins; ++fi)
            ts.spectrum[fi] = telem.spectrum[fi];
        ts.rmsDb = telem.rmsLeft;
        tracks.push_back(ts);
    });

    if (tracks.size() < 2) return;

    // ─── Bandas críticas (escala Bark simplificada para 1024-FFT @ 48kHz) ──
    // Cada bin del FFT = 48000/1024 = ~46.875 Hz
    struct MaskBand {
        const char* name;
        int binStart; // inclusive
        int binEnd;   // exclusive
    };

    const MaskBand bands[] = {
        { "sub-graves (20-70 Hz)",       0,   1 },
        { "graves bajos (70-150 Hz)",    1,   3 },
        { "graves (150-300 Hz)",         3,   6 },
        { "medios bajos (300-600 Hz)",   6,  13 },
        { "medios (600-1.2 kHz)",       13,  26 },
        { "medios altos (1.2-2.5 kHz)", 26,  53 },
        { "presencia (2.5-5 kHz)",      53, 106 },
        { "presencia alta (5-10 kHz)", 106, 213 },
        { "agudos (10-16 kHz)",        213, 341 },
        { "aire (16-20 kHz)",          341, 426 },
    };
    constexpr int kNumBands = sizeof(bands) / sizeof(bands[0]);

    // ─── Comparación pairwise ─────────────────────────────────────────────
    struct MaskPair {
        int idxA, idxB;
        float overlapScore;
        int worstBand;
        float energyA, energyB;
        juce::String bandName;
    };

    std::vector<MaskPair> pairs;

    auto toDb = [](float v) -> float {
        return (v > 0.001f) ? juce::Decibels::gainToDecibels(v) : -80.0f;
    };

    for (size_t i = 0; i < tracks.size(); ++i) {
        for (size_t j = i + 1; j < tracks.size(); ++j) {
            float totalOverlap = 0.0f;
            int worstBand = -1;
            float worstDiff = 0.0f;
            float worstEnergyA = 0.0f, worstEnergyB = 0.0f;
            juce::String worstBandName;

            for (int b = 0; b < kNumBands; ++b) {
                // Energía promedio de cada pista en esta banda
                float energyA = 0.0f, energyB = 0.0f;
                int binStart = bands[b].binStart;
                int binEnd = juce::jmin(bands[b].binEnd, kNumSpectrumBins);
                int count = binEnd - binStart;

                for (int bi = binStart; bi < binEnd; ++bi) {
                    energyA += tracks[i].spectrum[bi];
                    energyB += tracks[j].spectrum[bi];
                }
                if (count > 0) {
                    energyA /= (float)count;
                    energyB /= (float)count;
                }

                // Ambas deben tener energía significativa (> -40 dBFS)
                if (energyA > 0.01f && energyB > 0.01f) {
                    float bandOverlap = juce::jmin(energyA, energyB)
                                      * juce::jmax(energyA, energyB) * 10.0f;
                    totalOverlap += bandOverlap;

                    float diffDb = std::fabs(toDb(energyA) - toDb(energyB));
                    if (diffDb > worstDiff) {
                        worstDiff = diffDb;
                        worstBand = b;
                        worstEnergyA = energyA;
                        worstEnergyB = energyB;
                        worstBandName = bands[b].name;
                    }
                }
            }

            // Umbral: overlap suficiente para considerar enmascaramiento
            if (totalOverlap > 0.03f && worstBand >= 0) {
                MaskPair mp;
                mp.idxA = (int)i;
                mp.idxB = (int)j;
                mp.overlapScore = totalOverlap;
                mp.worstBand = worstBand;
                mp.energyA = worstEnergyA;
                mp.energyB = worstEnergyB;
                mp.bandName = worstBandName;
                pairs.push_back(mp);
            }
        }
    }

    if (pairs.empty())
        return;

    // ─── Ordenar por overlap (más crítico primero) ────────────────────────
    std::sort(pairs.begin(), pairs.end(), [](const MaskPair& a, const MaskPair& b) {
        return a.overlapScore > b.overlapScore;
    });

    // ─── Reportar top maskings ────────────────────────────────────────────
    if (now - lastMaskingWarningUs_ > kWarningCooldownUs) {
        lastMaskingWarningUs_ = now;

        int numToReport = juce::jmin((int)pairs.size(), 3);
        for (int p = 0; p < numToReport; ++p) {
            auto& mp = pairs[p];
            auto& trackA = tracks[mp.idxA];
            auto& trackB = tracks[mp.idxB];

            float aDb = toDb(mp.energyA);
            float bDb = toDb(mp.energyB);
            juce::String context = trackA.name + " / " + trackB.name;
            juce::String advice;

            if (aDb > bDb + 6.0f) {
                // A enmascara a B
                advice = "🔊 **" + trackA.name + "** enmascara a **" + trackB.name
                       + "** en " + mp.bandName + "\n"
                       + "    " + trackA.name + ": **" + juce::String(aDb, 1) + " dBFS**, "
                       + trackB.name + ": **" + juce::String(bDb, 1) + " dBFS**\n"
                       + "    💡 Prueba: reducir " + juce::String(3) + " dB en " + mp.bandName
                       + " de " + trackB.name + ", o aplica sidechain dinámico.";
            } else if (bDb > aDb + 6.0f) {
                advice = "🔊 **" + trackB.name + "** enmascara a **" + trackA.name
                       + "** en " + mp.bandName + "\n"
                       + "    " + trackB.name + ": **" + juce::String(bDb, 1) + " dBFS**, "
                       + trackA.name + ": **" + juce::String(aDb, 1) + " dBFS**\n"
                       + "    💡 Prueba: reducir " + juce::String(3) + " dB en " + mp.bandName
                       + " de " + trackA.name + ", o ajusta panoramas para separarlas.";
            } else {
                advice = "🔊 **" + trackA.name + "** y **" + trackB.name
                       + "** compiten en " + mp.bandName + "\n"
                       + "    " + trackA.name + ": **" + juce::String(aDb, 1) + " dBFS**, "
                       + trackB.name + ": **" + juce::String(bDb, 1) + " dBFS**\n"
                       + "    💡 Considera EQ carving (" + mp.bandName + ") "
                       + "o panoramas opuestos para separarlas.";
            }

            respondWithContext(advice, context, MentorMessage::Type::Tip);
        }

        if ((int)pairs.size() > numToReport) {
            respondWith(
                "🔊 **" + juce::String((int)pairs.size() - numToReport)
                + " par(es) adicional(es)** con enmascaramiento. "
                "Usa analizador espectral (pestaña 2) para identificar conflictos.",
                MentorMessage::Type::Info);
        }
    }

    // Log de diagnóstico
    LogHelper::writeToLog("[CoachEngine] SpectralMasking: "
                          + juce::String((int)pairs.size()) + " pares, "
                          + juce::String((int)tracks.size()) + " tracks");
}

// ═══════════════════════════════════════════════════════════════════════════
//  LEGACY (mantenidos por compatibilidad con llamadas existentes)
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

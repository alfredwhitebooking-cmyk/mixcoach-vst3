#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include "../../Common/audio/LoudnessAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <map>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers internos de análisis espectral
    // ═══════════════════════════════════════════════════════════════════════════

    /** Convierte magnitud FFT (0..1) a dBFS. */
    static float fftMagToDb(float mag) noexcept
    {
        return (mag > 1e-10f) ? juce::Decibels::gainToDecibels(mag) : -100.0f;
    }

    /** Energía promedio en un rango de bins espectrales. */
    static float spectrumBandEnergy(const float* spectrum, int startBin, int endBin) noexcept
    {
        if (spectrum == nullptr || startBin < 0 || endBin <= startBin || startBin >= kNumSpectrumBins) return -100.0f;

        endBin     = std::min(endBin, kNumSpectrumBins);
        double sum = 0.0;
        int count  = 0;
        for (int b = startBin; b < endBin; ++b) {
            float mag = spectrum[b];
            if (mag > 1e-10f) {
                sum += static_cast<double>(fftMagToDb(mag));
                ++count;
            }
        }
        return (count > 0) ? static_cast<float>(sum / count) : -100.0f;
    }

    /** Computa similitud coseno entre dos vectores espectrales. */
    static float spectralCosineSimilarity(const float a[30], const float b[30])
    {
        double dot = 0.0, normA = 0.0, normB = 0.0;
        int active = 0;

        for (int i = 0; i < 30; ++i) {
            if (a[i] > -80.0f && b[i] > -80.0f) {
                double va = static_cast<double>(a[i] + 100.0);
                double vb = static_cast<double>(b[i] + 100.0);
                dot += va * vb;
                normA += va * va;
                normB += vb * vb;
                ++active;
            }
        }

        if (active < 3 || normA < 1e-10 || normB < 1e-10) return 0.0f;

        return static_cast<float>(dot / (std::sqrt(normA) * std::sqrt(normB)));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeReferenceBuffer — Procesa un buffer de audio completo y extrae
    //  el fingerprint espectral + loudness de la referencia.
    //
    //  Esta función toma el buffer que ReferenceAudioPlayer ya cargó en memoria,
    //  evitando una segunda lectura de disco. Procesa el audio por bloques a
    //  través de AudioAnalysis (FFT 16384) y LoudnessAnalyzer (EBU R128).
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::analyzeReferenceBuffer(const float* bufferL,
                                             const float* bufferR,
                                             int64_t numSamples,
                                             int numChannels,
                                             double sampleRate,
                                             const juce::String& filePath)
    {
        if (bufferL == nullptr || numSamples <= 0 || sampleRate <= 0.0) {
            LogHelper::writeToLog("[CoachEngine] analyzeReferenceBuffer: parametros invalidos");
            return;
        }

        LogHelper::writeToLog("[CoachEngine] Analizando referencia: \"" + juce::File(filePath).getFileName() + "\" ("
                              + juce::String(static_cast<int>(numChannels)) + "ch, "
                              + juce::String(static_cast<int>(sampleRate)) + "Hz, "
                              + juce::String(static_cast<double>(numSamples) / sampleRate, 1) + "s)");

        // ─── Normalización según modo (V4 Dual Mode) ──────────────────────────
        // En Mix Mode: la referencia se normaliza a -6 dBFS para evitar que el
        // usuario persiga loudness demasiado pronto (objetivo: balance, no volumen).
        // En Master Mode: se usa el volumen real para comparación exacta de LUFS/True Peak.
        std::vector<float> normalizedL, normalizedR;
        const float* procL = bufferL;
        const float* procR = bufferR;

        if (shouldNormalizeReference()) {
            float peak = 0.0f;
            for (int64_t i = 0; i < numSamples; ++i) {
                float absL = std::abs(bufferL[i]);
                if (absL > peak) peak = absL;
                if (bufferR && numChannels >= 2) {
                    float absR = std::abs(bufferR[i]);
                    if (absR > peak) peak = absR;
                }
            }

            if (peak > 1e-10f) {
                const float targetGain = juce::Decibels::decibelsToGain(-6.0f);
                const float scale      = targetGain / peak;

                normalizedL.assign(bufferL, bufferL + static_cast<ptrdiff_t>(numSamples));
                for (auto& s : normalizedL) s *= scale;
                procL = normalizedL.data();

                if (bufferR && numChannels >= 2) {
                    normalizedR.assign(bufferR, bufferR + static_cast<ptrdiff_t>(numSamples));
                    for (auto& s : normalizedR) s *= scale;
                    procR = normalizedR.data();
                }

                LogHelper::writeToLog(
                    "[CoachEngine] Referencia normalizada a -6 dBFS "
                    "(peak="
                    + juce::String(juce::Decibels::gainToDecibels(peak), 1) + " dBFS, scale=" + juce::String(scale, 3)
                    + ")");
            }
            else {
                LogHelper::writeToLog("[CoachEngine] Referencia: peak muy bajo, sin normalizar");
            }
        }

        // ─── 1. Preparar analizadores ─────────────────────────────────────────
        constexpr int kBlockSize = 1024;
        AudioAnalysis analysis;
        analysis.prepare(sampleRate, kBlockSize);

        LoudnessAnalyzer loudness;
        loudness.prepare(sampleRate, kBlockSize);

        // ─── 2. Procesar audio por bloques ────────────────────────────────────
        // Usamos bloques pequeños para mantener el FFT actualizado constantemente.
        // Al final, analysis_ tendrá el espectro acumulado y loudness_ las métricas LUFS.
        int totalSamples = static_cast<int>(numSamples);

        // Buffer para acumular picos L/R (para true peak y crest)
        float peakL = -100.0f, peakR = -100.0f;
        float rmsSum = 0.0f;
        int rmsCount = 0;

        for (int pos = 0; pos < totalSamples; pos += kBlockSize) {
            int thisBlock = std::min(kBlockSize, totalSamples - pos);

            // Sumar a mono para el análisis espectral
            juce::AudioBuffer<float> mono(1, thisBlock);
            auto* monoData = mono.getWritePointer(0);

            if (numChannels >= 2 && procR != nullptr) {
                for (int i = 0; i < thisBlock; ++i) monoData[i] = (procL[pos + i] + procR[pos + i]) * 0.5f;
            }
            else {
                std::copy(procL + pos, procL + pos + thisBlock, monoData);
            }

            analysis.process(monoData, thisBlock);

            // LUFS con canales L/R separados
            if (numChannels >= 2 && procR != nullptr) loudness.processBlock(procL + pos, procR + pos, thisBlock);
            else
                loudness.processBlock(procL + pos, procL + pos, thisBlock);

            // RMS acumulado
            double blockRms = 0.0;
            for (int i = 0; i < thisBlock; ++i) blockRms += static_cast<double>(monoData[i] * monoData[i]);
            blockRms = std::sqrt(blockRms / thisBlock);

            float rmsDb = fftMagToDb(static_cast<float>(blockRms));
            if (rmsDb > -90.0f) {
                rmsSum += rmsDb;
                ++rmsCount;
            }

            // Peak tracking
            for (int i = 0; i < thisBlock; ++i) {
                if (procL[pos + i] > peakL) peakL = procL[pos + i];
                if (numChannels >= 2 && procR != nullptr) {
                    if (procR[pos + i] > peakR) peakR = procR[pos + i];
                }
            }
        }

        // ─── 3. Extraer métricas del análisis ─────────────────────────────────
        const float* fftSpectrum = analysis.getSpectrum();
        if (fftSpectrum == nullptr) {
            LogHelper::writeToLog("[CoachEngine] analyzeReferenceBuffer: FFT spectrum es nulo");
            return;
        }

        // Poblar ReferenceFingerprint
        ReferenceFingerprint fp;

        // 30-band spectral energies (mismas bandas que el mix actual)
        for (int b = 0; b < kNumSpectralBands && b < 30; ++b) {
            int startBin       = kSpectralBandBins[b][0];
            int endBin         = std::min(kSpectralBandBins[b][1], kNumSpectrumBins);
            fp.bandEnergies[b] = spectrumBandEnergy(fftSpectrum, startBin, endBin);
        }

        // LUFS
        fp.lufsMomentary  = loudness.getMomentary();
        fp.lufsShortTerm  = loudness.getShortTerm();
        fp.lufsIntegrated = loudness.getIntegrated();
        fp.lufsRange      = loudness.getRange();

        // Crest factor (peak - RMS promedio)
        if (rmsCount > 0) {
            float avgRms = rmsSum / rmsCount;
            float peakDb = juce::jmax(fftMagToDb(peakL), fftMagToDb(peakR));
            if (avgRms > -80.0f && peakDb > -80.0f) fp.crestFactor = peakDb - avgRms;
        }

        // True Peak
        fp.truePeakDBTP = loudness.getTruePeak();

        // Correlación estéreo promedio (estimada del análisis)
        // Media de correlation en bloques procesados
        // Para un análisis offline, usamos una estimación
        // basada en la similitud L/R de los buffers
        if (numChannels >= 2 && procR != nullptr) {
            double corrSum = 0.0;
            int corrCount  = 0;
            for (int pos = 0; pos < totalSamples - kBlockSize; pos += kBlockSize) {
                double sumL = 0.0, sumR = 0.0, sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
                for (int i = 0; i < kBlockSize; ++i) {
                    double l = static_cast<double>(procL[pos + i]);
                    double r = static_cast<double>(procR[pos + i]);
                    sumL += l;
                    sumR += r;
                    sumL2 += l * l;
                    sumR2 += r * r;
                    sumLR += l * r;
                }
                int n       = kBlockSize;
                double cov  = (n * sumLR - sumL * sumR);
                double varL = (n * sumL2 - sumL * sumL);
                double varR = (n * sumR2 - sumR * sumR);
                if (varL > 1e-10 && varR > 1e-10) {
                    corrSum += cov / (std::sqrt(varL) * std::sqrt(varR));
                    ++corrCount;
                }
            }
            if (corrCount > 0) fp.correlation = static_cast<float>(corrSum / corrCount);
        }

        // Calcular centroide espectral desde las 30 bandas de energía
        // (se usa como expected centroid cuando hay referencia cargada)
        {
            double weightedSum = 0.0;
            double totalEnergy = 0.0;

            for (int b = 0; b < 30; ++b) {
                float energyDb = fp.bandEnergies[b];
                if (energyDb > -80.0f) {
                    // Convertir dB a magnitud lineal para promediar
                    float linearEnergy = juce::Decibels::decibelsToGain(energyDb);

                    // Centro de banda: media geométrica de low-high
                    float lowFreq    = kSpectralBandFreqs[b][0];
                    float highFreq   = kSpectralBandFreqs[b][1];
                    float centerFreq = std::sqrt(lowFreq * highFreq);
                    if (lowFreq < 1.0f) centerFreq = 30.0f; // Banda 0: floor en ~30 Hz

                    weightedSum += static_cast<double>(linearEnergy) * centerFreq;
                    totalEnergy += static_cast<double>(linearEnergy);
                }
            }

            if (totalEnergy > 0.0) fp.spectralCentroidHz = static_cast<float>(weightedSum / totalEnergy);

            LogHelper::writeToLog(
                "[CoachEngine] Referencia centroide espectral: " + juce::String(fp.spectralCentroidHz, 0) + " Hz");
        }

        fp.valid              = true;
        referenceFingerprint_ = fp;

        // ─── 4. Análisis de secciones ─────────────────────────────────────────
        // Cargar el buffer mono completo para el análisis estructural
        juce::AudioBuffer<float> fullBuffer;
        fullBuffer.setSize(numChannels, totalSamples);
        fullBuffer.copyFrom(0, 0, procL, totalSamples);
        if (numChannels >= 2 && procR != nullptr) fullBuffer.copyFrom(1, 0, procR, totalSamples);

        computeReferenceSections(fullBuffer, sampleRate, numChannels);

        // ─── 5. Notificar al ReferencePanel vía computeAndSendMatchData() ─────
        //     Envía TODOS los datos espectrales (6 regiones, LUFS, crest, correlation)
        //     para que el ReferenceMatchPanel muestre la comparación completa.
        computeAndSendMatchData();

        LogHelper::writeToLog("[CoachEngine] Referencia analizada: " + juce::String(static_cast<int>(sampleRate))
                              + "Hz, " + juce::String(numSamples) + "samples, " + "LUFS_I="
                              + juce::String(fp.lufsIntegrated, 1) + ", " + "Crest=" + juce::String(fp.crestFactor, 1)
                              + "dB, " + "Corr=" + juce::String(fp.correlation, 2));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeReferenceFile — Carga un archivo de referencia desde disco y lo
    //  analiza para extraer el fingerprint espectral + secciones.
    //
    //  Es una alternativa a analyzeReferenceBuffer para cuando ReferenceAudioPlayer
    //  aún no ha cargado el archivo en memoria (fallback).
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::analyzeReferenceFile(const juce::String& filePath)
    {
        juce::File file(filePath);
        if (!file.existsAsFile()) {
            LogHelper::writeToLog("[CoachEngine] analyzeReferenceFile: archivo no encontrado: " + filePath);
            return;
        }

        // Cargar el archivo
        juce::AudioFormatManager formatMgr;
        formatMgr.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(file));
        if (reader == nullptr) {
            LogHelper::writeToLog("[CoachEngine] analyzeReferenceFile: formato no soportado: " + filePath);
            return;
        }

        int numChannels      = reader->numChannels;
        double sampleRate    = reader->sampleRate;
        int64_t totalSamples = reader->lengthInSamples;

        // Limitar a 120s para evitar RAM excesiva
        constexpr int64_t kMaxSamples = 120 * 48000;
        int64_t readSamples           = std::min(totalSamples, kMaxSamples);
        int readSamplesInt            = static_cast<int>(readSamples);

        // Leer el buffer completo
        juce::AudioBuffer<float> buffer(numChannels, readSamplesInt);
        reader->read(&buffer, 0, readSamplesInt, 0, true, true);
        // BUG #3: reader es unique_ptr, se destruye automáticamente al salir del ámbito

        // Extraer pointers L/R
        const float* bufL = buffer.getReadPointer(0);
        const float* bufR = (numChannels >= 2) ? buffer.getReadPointer(1) : nullptr;

        // Delegar a analyzeReferenceBuffer
        analyzeReferenceBuffer(bufL, bufR, readSamples, numChannels, sampleRate, filePath);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  applyReferenceAnalysis — Versión con SEH protection (desde path)
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::applyReferenceAnalysis(const juce::String& filePath)
    {
        try {
            analyzeReferenceFile(filePath);
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[CoachEngine] SEH en analyzeReferenceFile: " + juce::String(e.what()));
        }
        catch (...) {
            MIXCOACH_LOG_CATCH("CoachEngine applyReferenceFile");
            LogHelper::writeToLog("[CoachEngine] SEH desconocido en analyzeReferenceFile");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  applyReferenceAnalysis — Versión con SEH protection (desde buffer)
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::applyReferenceAnalysis(const float* bufferL,
                                             const float* bufferR,
                                             int64_t numSamples,
                                             int numChannels,
                                             double sampleRate,
                                             const juce::String& filePath)
    {
        try {
            analyzeReferenceBuffer(bufferL, bufferR, numSamples, numChannels, sampleRate, filePath);
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[CoachEngine] SEH en analyzeReferenceBuffer: " + juce::String(e.what()));
        }
        catch (...) {
            MIXCOACH_LOG_CATCH("CoachEngine applyReferenceBuffer");
            LogHelper::writeToLog("[CoachEngine] SEH desconocido en analyzeReferenceBuffer");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeAndSendMatchData — Extrae 6 regiones desde el fingerprint activo
    //  y el master mix, y dispara el callback para actualizar la UI.
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::computeAndSendMatchData()
    {
        if (!matchDataCallback_) return;

        const auto& fp = getActiveFingerprint();
        if (!fp.valid) return;

        // ─── Extraer 6 regiones desde el fingerprint de 30 bandas ─────────────
        // Sub:    bandas 0-1   (0-86 Hz)
        // Bass:   bandas 2-4   (86-301 Hz)
        // LoMid:  bandas 5-9   (301-1076 Hz)
        // HiMid:  bandas 10-17 (1076-3532 Hz)
        // Pres:   bandas 18-24 (3532-8355 Hz)
        // Air:    bandas 25-29 (8355-16458 Hz)

        auto avgBands = [&](int start, int end) -> float {
            float sum = 0.0f;
            int count = 0;
            for (int b = start; b < end && b < 30; ++b) {
                if (fp.bandEnergies[b] > -90.0f) {
                    sum += fp.bandEnergies[b];
                    ++count;
                }
            }
            return (count > 0) ? (sum / count) : -100.0f;
        };

        DifferenceProfile dp;
        dp.valid             = true;
        dp.referenceName     = getReferenceName();
        dp.refIntegratedLUFS = fp.lufsIntegrated;
        dp.refCrestFactor    = fp.crestFactor;
        dp.refCorrelation    = fp.correlation;

        dp.refRegionEnergy[0] = avgBands(0, 2);   // Sub
        dp.refRegionEnergy[1] = avgBands(2, 5);   // Bass
        dp.refRegionEnergy[2] = avgBands(5, 10);  // LoMid
        dp.refRegionEnergy[3] = avgBands(10, 18); // HiMid
        dp.refRegionEnergy[4] = avgBands(18, 25); // Pres
        dp.refRegionEnergy[5] = avgBands(25, 30); // Air

        // Master mix regions (desde AudioAnalyzer)
        {
            const auto& master    = audioAnalyzer_.getMasterAnalysis();
            const float* spectrum = master.getSpectrum();
            if (spectrum != nullptr && master.getLastUpdateTime() > 0) {
                for (int r = 0; r < 6; ++r) {
                    static const int kRegionBands[6][2] = {{0, 2}, {2, 5}, {5, 10}, {10, 18}, {18, 25}, {25, 30}};
                    float sum                           = 0.0f;
                    int count                           = 0;
                    for (int b = kRegionBands[r][0]; b < kRegionBands[r][1] && b < kNumSpectralBands; ++b) {
                        int startBin = kSpectralBandBins[b][0];
                        int endBin   = std::min(kSpectralBandBins[b][1], kNumSpectrumBins);
                        float energy = spectrumBandEnergy(spectrum, startBin, endBin);
                        if (energy > -90.0f) {
                            sum += energy;
                            ++count;
                        }
                    }
                    dp.mixRegionEnergy[r] = (count > 0) ? (sum / count) : -100.0f;
                }

                dp.mixIntegratedLUFS = audioAnalyzer_.getIntegratedLUFS();
                dp.mixCrestFactor =
                    audioAnalyzer_.getMasterAnalysis().getPeak() - audioAnalyzer_.getMasterAnalysis().getRMS();
                dp.mixCorrelation = master.getCorrelation();
            }
        }

        // ─── Computar deltas (ref - mix) para cada región espectral ──────────
        for (int r = 0; r < 6; ++r) {
            if (dp.mixRegionEnergy[r] > -90.0f && dp.refRegionEnergy[r] > -90.0f)
                dp.deltaRegionEnergy[r] = dp.refRegionEnergy[r] - dp.mixRegionEnergy[r];
        }

        matchDataCallback_(dp);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeReferenceSections — Análisis ESTRUCTURAL de la referencia que
    //  detecta automáticamente coro, verso, puente, intro/outro basado en el
    //  contenido de audio.
    //
    //  Algoritmo:
    //    1. Ventanas deslizantes de ~2s → fingerprint espectral + energía
    //    2. Matriz de auto-similitud entre todas las ventanas
    //    3. Detección de boundaries: puntos donde cambia el espectro significativamente
    //    4. Agrupación de secciones similares (clustering por similitud espectral)
    //    5. Labeling heurístico:
    //       - Intro: primera sección si es de baja energía
    //       - Chorus: sección con más repeticiones + mayor energía
    //       - Verse: sección repetida entre chorus
    //       - Bridge: sección única en el medio
    //       - Outro: última sección si es de baja energía
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::computeReferenceSections(const juce::AudioBuffer<float>& fileBuffer,
                                               double sampleRate,
                                               int numChannels)
    {
        referenceSections_.clear();
        activeSectionIndex_ = -1;

        int totalSamples = fileBuffer.getNumSamples();
        if (totalSamples < sampleRate * 10) // Mínimo 10s para análisis estructural
        {
            LogHelper::writeToLog("[CoachEngine] Secciones: archivo muy corto (<10s), fingerprint global solo");
            return;
        }

        // ═══ FASE 1: Ventanas deslizantes ────────────────────────────────────
        // Cada ventana = ~2 segundos, solapamiento 50%
        constexpr int kWindowSeconds = 2;
        int windowSize               = static_cast<int>(sampleRate * kWindowSeconds);
        int hopSize                  = windowSize / 2; // 50% overlap

        if (windowSize <= 0) return;

        struct WindowFingerprint
        {
            float energy;           // RMS energy in dB
            float bandEnergies[30]; // 30-band spectral profile
            float positionSeconds;  // Center position in seconds
            int windowIndex;
        };

        std::vector<WindowFingerprint> windows;
        int numWindows = (totalSamples - windowSize) / hopSize + 1;

        if (numWindows < 4) {
            LogHelper::writeToLog("[CoachEngine] Secciones: muy pocas ventanas para analisis");
            return;
        }

        for (int w = 0; w < numWindows; ++w) {
            int startSample = w * hopSize;
            int endSample   = std::min(startSample + windowSize, totalSamples);
            int thisSize    = endSample - startSample;

            // Mezclar a mono
            std::vector<float> monoBuf(static_cast<size_t>(thisSize), 0.0f);
            for (int i = 0; i < thisSize; ++i) {
                float s = fileBuffer.getSample(0, startSample + i);
                if (numChannels >= 2) s = (s + fileBuffer.getSample(1, startSample + i)) * 0.5f;
                monoBuf[static_cast<size_t>(i)] = s;
            }

            // Procesar ventana con un AudioAnalysis FRESCO (nueva instancia por ventana
            // para evitar acumulación espectral entre ventanas).
            AudioAnalysis windowFFT;
            windowFFT.prepare(sampleRate, 1024);
            {
                int blockSize = 1024;
                auto* data    = monoBuf.data();
                int processed = 0;
                while (processed < thisSize) {
                    int block = std::min(blockSize, thisSize - processed);
                    windowFFT.process(data + processed, block);
                    processed += block;
                }
            }

            // Extraer 30-bandas del espectro acumulado de ESTA ventana
            const float* spectrum = windowFFT.getSpectrum();
            WindowFingerprint wf;
            wf.windowIndex     = w;
            wf.positionSeconds = static_cast<float>(startSample + thisSize / 2) / static_cast<float>(sampleRate);

            // Energía RMS
            double rms = 0.0;
            for (int i = 0; i < thisSize; ++i)
                rms += static_cast<double>(monoBuf[static_cast<size_t>(i)] * monoBuf[static_cast<size_t>(i)]);
            rms       = std::sqrt(rms / thisSize);
            wf.energy = fftMagToDb(static_cast<float>(rms));

            if (spectrum != nullptr) {
                for (int b = 0; b < kNumSpectralBands && b < 30; ++b) {
                    int startBin       = kSpectralBandBins[b][0];
                    int endBin         = std::min(kSpectralBandBins[b][1], kNumSpectrumBins);
                    wf.bandEnergies[b] = spectrumBandEnergy(spectrum, startBin, endBin);
                }
            }
            else {
                for (int b = 0; b < 30; ++b) wf.bandEnergies[b] = -100.0f;
            }

            windows.push_back(wf);
        }

        // ═══ FASE 2: Detección de boundaries ─────────────────────────────────
        // Busca puntos donde la similitud entre ventanas adyacentes baja
        // significativamente, indicando un cambio de sección.
        std::vector<int> boundaryIndices;
        boundaryIndices.push_back(0); // Siempre empezar en la primera ventana

        constexpr float kBoundaryThreshold = 0.65f; // Similitud por debajo de esto = boundary

        for (size_t i = 1; i < windows.size(); ++i) {
            float sim = spectralCosineSimilarity(windows[i - 1].bandEnergies, windows[i].bandEnergies);

            // También considerar cambio de energía > 6dB
            float energyDelta = std::abs(windows[i].energy - windows[i - 1].energy);

            if (sim < kBoundaryThreshold || energyDelta > 6.0f) {
                boundaryIndices.push_back(static_cast<int>(i));
            }
        }

        // Agregar el final
        if (boundaryIndices.back() != static_cast<int>(windows.size()) - 1)
            boundaryIndices.push_back(static_cast<int>(windows.size()) - 1);

        // Filtrar boundaries muy cercanos (< 4 ventanas = ~4s)
        {
            std::vector<int> filtered;
            filtered.push_back(boundaryIndices[0]);
            for (size_t i = 1; i < boundaryIndices.size(); ++i) {
                if (boundaryIndices[i] - filtered.back() >= 4) filtered.push_back(boundaryIndices[i]);
            }
            if (filtered.back() != static_cast<int>(windows.size()) - 1)
                filtered.push_back(static_cast<int>(windows.size()) - 1);
            boundaryIndices = filtered;
        }

        // ═══ FASE 3: Agrupar secciones por similitud ─────────────────────────
        struct DetectedSection
        {
            int startWindow;
            int endWindow;
            float startSeconds;
            float endSeconds;
            float avgEnergy;
            float avgBandEnergies[30];
            std::vector<WindowFingerprint> memberWindows;
        };

        std::vector<DetectedSection> sections;

        for (size_t b = 0; b < boundaryIndices.size() - 1; ++b) {
            DetectedSection sec;
            sec.startWindow  = boundaryIndices[b];
            sec.endWindow    = boundaryIndices[b + 1];
            sec.startSeconds = windows[sec.startWindow].positionSeconds;
            sec.endSeconds   = windows[sec.endWindow].positionSeconds;

            // Promediar fingerprints de las ventanas de esta sección
            float energySum = 0.0f;
            int energyCount = 0;
            std::fill(std::begin(sec.avgBandEnergies), std::end(sec.avgBandEnergies), 0.0f);
            int bandCounts[30] = {};

            for (int w = sec.startWindow; w <= sec.endWindow && w < (int)windows.size(); ++w) {
                if (windows[w].energy > -90.0f) {
                    energySum += windows[w].energy;
                    ++energyCount;
                }
                for (int band = 0; band < 30; ++band) {
                    if (windows[w].bandEnergies[band] > -90.0f) {
                        sec.avgBandEnergies[band] += windows[w].bandEnergies[band];
                        ++bandCounts[band];
                    }
                }
                sec.memberWindows.push_back(windows[w]);
            }

            sec.avgEnergy = (energyCount > 0) ? (energySum / energyCount) : -100.0f;
            for (int band = 0; band < 30; ++band) {
                if (bandCounts[band] > 0) sec.avgBandEnergies[band] /= bandCounts[band];
                else
                    sec.avgBandEnergies[band] = -100.0f;
            }

            sections.push_back(sec);
        }

        if (sections.empty()) {
            LogHelper::writeToLog("[CoachEngine] Secciones: no se detectaron secciones");
            return;
        }

        // ═══ FASE 4: Clustering — agrupar secciones similares ────────────────
        // Para cada par de secciones, computar similitud espectral.
        // Si sim >= 0.75, considerar que son la MISMA sección (e.g., verso 1 = verso 2).
        constexpr float kClusterSimilarity = 0.75f;

        struct SectionCluster
        {
            std::vector<int> memberSectionIndices;
            float avgEnergy           = -100.0f;
            float avgBandEnergies[30] = {};
            bool labeled              = false;
            juce::String label;
        };

        std::vector<SectionCluster> clusters;
        std::vector<bool> assigned(sections.size(), false);

        for (size_t i = 0; i < sections.size(); ++i) {
            if (assigned[i]) continue;

            SectionCluster cluster;
            cluster.memberSectionIndices.push_back(static_cast<int>(i));
            assigned[i] = true;

            // Buscar secciones similares
            for (size_t j = i + 1; j < sections.size(); ++j) {
                if (assigned[j]) continue;

                float sim = spectralCosineSimilarity(sections[i].avgBandEnergies, sections[j].avgBandEnergies);

                // Bonus de similitud si tienen energía similar
                float energyDiff  = std::abs(sections[i].avgEnergy - sections[j].avgEnergy);
                float adjustedSim = sim;
                if (energyDiff < 3.0f) adjustedSim += 0.05f; // Pequeño bonus por energía similar

                if (adjustedSim >= kClusterSimilarity) {
                    cluster.memberSectionIndices.push_back(static_cast<int>(j));
                    assigned[j] = true;
                }
            }

            // Computar perfil promedio del cluster
            float energySum = 0.0f;
            int energyCount = 0;
            std::fill(std::begin(cluster.avgBandEnergies), std::end(cluster.avgBandEnergies), 0.0f);
            int bandCounts[30] = {};

            for (int idx : cluster.memberSectionIndices) {
                if (sections[idx].avgEnergy > -90.0f) {
                    energySum += sections[idx].avgEnergy;
                    ++energyCount;
                }
                for (int b = 0; b < 30; ++b) {
                    if (sections[idx].avgBandEnergies[b] > -90.0f) {
                        cluster.avgBandEnergies[b] += sections[idx].avgBandEnergies[b];
                        ++bandCounts[b];
                    }
                }
            }

            cluster.avgEnergy = (energyCount > 0) ? (energySum / energyCount) : -100.0f;
            for (int b = 0; b < 30; ++b)
                if (bandCounts[b] > 0) cluster.avgBandEnergies[b] /= bandCounts[b];

            clusters.push_back(cluster);
        }

        // ═══ FASE 5: Labeling heurístico ─────────────────────────────────────
        // Ordenar clusters por número de miembros (más repetido primero)
        std::sort(clusters.begin(), clusters.end(), [](const SectionCluster& a, const SectionCluster& b) {
            return a.memberSectionIndices.size() > b.memberSectionIndices.size();
        });

        // Clasificar:
        // - El cluster más repetido → Chorus
        // - Si el primer cluster está al inicio → podría ser Intro
        // - El segundo más repetido → Verse
        // - Clusters únicos en el medio → Bridge
        // - Último cluster si es único y de baja energía → Outro

        for (auto& cluster : clusters) {
            // Ordenar miembros por posición
            std::sort(cluster.memberSectionIndices.begin(), cluster.memberSectionIndices.end());

            int firstMember     = cluster.memberSectionIndices.front();
            int lastMember      = cluster.memberSectionIndices.back();
            int numMembers      = static_cast<int>(cluster.memberSectionIndices.size());
            bool isFirstSection = (firstMember == 0);
            bool isLastSection  = (lastMember == static_cast<int>(sections.size()) - 1);

            if (numMembers >= 2 && !cluster.labeled) {
                // Sección que se repite
                if (cluster.avgEnergy > -20.0f) cluster.label = "Chorus";
                else
                    cluster.label = "Verse";
                cluster.labeled = true;
            }
        }

        // Asignar Chorus al cluster más repetido que aún no esté etiquetado
        for (auto& cluster : clusters) {
            if (!cluster.labeled && cluster.memberSectionIndices.size() >= 1) {
                int firstMember     = cluster.memberSectionIndices.front();
                bool isFirstSection = (firstMember == 0);
                bool isLastSection  = (cluster.memberSectionIndices.back() == static_cast<int>(sections.size()) - 1);

                if (isFirstSection && cluster.avgEnergy < -25.0f) cluster.label = "Intro";
                else if (isLastSection && cluster.avgEnergy < -25.0f)
                    cluster.label = "Outro";
                else if (cluster.memberSectionIndices.size() == 1)
                    cluster.label = "Bridge";
                else
                    cluster.label = (cluster.avgEnergy > -20.0f) ? "Chorus" : "Verse";

                cluster.labeled = true;
            }
        }

        // ═══ FASE 6: Generar SectionFingerprints ─────────────────────────────
        // Calcular fingerprint completo (con LUFS, crest, etc.) para cada sección
        for (const auto& cluster : clusters) {
            for (int secIdx : cluster.memberSectionIndices) {
                const auto& sec = sections[secIdx];

                SectionFingerprint sf;
                sf.startSeconds = sec.startSeconds;
                sf.endSeconds   = sec.endSeconds;
                sf.label        = cluster.label;

                // Poblar el fingerprint con los datos promedio de la sección
                auto& fp = sf.fingerprint;
                for (int b = 0; b < 30; ++b) fp.bandEnergies[b] = sec.avgBandEnergies[b];

                // Calcular LUFS aproximado desde la energía promedio
                // (estimación: energía RMS → LUFS con offset)
                if (sec.avgEnergy > -90.0f) {
                    // Offset típico entre RMS y LUFS en música masterizada
                    fp.lufsMomentary  = sec.avgEnergy + 3.0f;
                    fp.lufsShortTerm  = sec.avgEnergy + 3.0f;
                    fp.lufsIntegrated = sec.avgEnergy + 3.0f;
                }

                fp.crestFactor  = referenceFingerprint_.crestFactor; // Usar global
                fp.correlation  = referenceFingerprint_.correlation;
                fp.truePeakDBTP = referenceFingerprint_.truePeakDBTP;
                fp.valid        = true;

                referenceSections_.push_back(sf);
            }
        }

        // ═══ FASE 7: Ordenar secciones por tiempo ────────────────────────────
        std::sort(referenceSections_.begin(),
                  referenceSections_.end(),
                  [](const SectionFingerprint& a, const SectionFingerprint& b) {
                      return a.startSeconds < b.startSeconds;
                  });

        // Agregar sección \"Full\" al inicio (fingerprint global)
        // La UI espera que la primera sección sea \"Full\" con el fingerprint completo
        {
            SectionFingerprint fullSec;
            fullSec.startSeconds = 0.0f;
            fullSec.endSeconds   = static_cast<float>(totalSamples) / static_cast<float>(sampleRate);
            fullSec.label        = "Full";
            fullSec.fingerprint  = referenceFingerprint_;
            referenceSections_.insert(referenceSections_.begin(), fullSec);
        }

        // ═══ LOG ─────────────────────────────────────────────────────────────
        {
            juce::String logMsg = "[CoachEngine] Secciones estructurales detectadas: "
                                  + juce::String(static_cast<int>(referenceSections_.size())) + "\n";
            for (size_t i = 0; i < referenceSections_.size(); ++i) {
                const auto& s = referenceSections_[i];
                logMsg += "  [" + juce::String(static_cast<int>(i)) + "] " + s.label + " "
                          + juce::String(s.startSeconds, 1) + "s-" + juce::String(s.endSeconds, 1) + "s\n";
            }
            LogHelper::writeToLog(logMsg);

            // También enviar al chat del coach
            juce::String chatMsg = "[CHART] **Secciones detectadas en la referencia:**\n";
            for (const auto& s : referenceSections_) {
                if (s.label == "Full") continue; // No mostrar Full en el chat
                chatMsg += "  \xE2\x97\x8F **" + s.label + "** " + juce::String(s.startSeconds, 1) + "s - "
                           + juce::String(s.endSeconds, 1) + "s (" + juce::String(s.endSeconds - s.startSeconds, 1)
                           + "s)\n";
            }
            respondWith(chatMsg, MentorMessage::Type::Info);
        }
    }

} // namespace mixcoach

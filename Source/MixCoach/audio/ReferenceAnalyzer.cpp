#include "ReferenceAnalyzer.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  loadFile — Carga un archivo de audio y lo analiza
    // ═══════════════════════════════════════════════════════════════════════════
    bool ReferenceAnalyzer::loadFile(const juce::String& filePath)
    {
        clear();

        juce::File file(filePath);
        if (!file.existsAsFile()) {
            LogHelper::writeToLog("[ReferenceAnalyzer] Archivo no encontrado: " + filePath);
            return false;
        }

        fileName_ = file.getFileName();
        filePath_ = filePath;

        // ─── Crear el lector de audio ──────────────────────────────────────────
        juce::AudioFormatManager formatMgr;
        formatMgr.registerBasicFormats();

        auto* reader = formatMgr.createReaderFor(file);
        if (reader == nullptr) {
            LogHelper::writeToLog("[ReferenceAnalyzer] No se pudo leer: " + filePath + " (formato no soportado)");
            return false;
        }

        sampleRate_       = static_cast<int>(reader->sampleRate);
        duration_         = reader->lengthInSamples / reader->sampleRate;
        auto numChannels  = reader->numChannels;
        auto totalSamples = reader->lengthInSamples;

        // ─── Leer TODO el archivo en un buffer ─────────────────────────────────
        // Limitamos a 60 segundos para evitar RAM excesiva en referencias largas
        constexpr int64_t kMaxReferenceSamples = 60 * 44100; // ~60s a 44.1kHz
        int64_t samplesToRead                  = std::min<int64_t>(totalSamples, kMaxReferenceSamples);

        audioBuffer_.clear();
        audioBuffer_.setSize(static_cast<int>(numChannels), static_cast<int>(samplesToRead));

        reader->read(&audioBuffer_, 0, static_cast<int>(samplesToRead), 0, true, true);
        delete reader;

        // ─── Analizar el buffer completo ───────────────────────────────────────
        analyzeAudioBuffer(audioBuffer_, sampleRate_);

        loaded_ = true;

        LogHelper::writeToLog("[ReferenceAnalyzer] Referencia cargada: " + fileName_ + " ("
                              + juce::String(sampleRate_ / 1000) + " kHz, " + juce::String(duration_, 1) + "s, "
                              + juce::String(numChannels) + " canales)");

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  loadFromBuffer — Carga una referencia desde un buffer ya en memoria
    // ═══════════════════════════════════════════════════════════════════════════
    bool ReferenceAnalyzer::loadFromBuffer(const float* bufferL,
                                           const float* bufferR,
                                           int64_t numSamples,
                                           int numChannels,
                                           double sampleRate,
                                           const juce::String& filePath)
    {
        clear();

        if (bufferL == nullptr || numSamples <= 0 || numChannels < 1) {
            LogHelper::writeToLog("[ReferenceAnalyzer] loadFromBuffer: buffer inválido");
            return false;
        }

        fileName_   = juce::File(filePath).getFileName();
        filePath_   = filePath;
        sampleRate_ = static_cast<int>(sampleRate);
        duration_   = static_cast<double>(numSamples) / sampleRate;

        // ─── Limitar a 60 segundos para evitar RAM excesiva ─────────────────────
        constexpr int64_t kMaxReferenceSamples = 60 * 44100;
        int64_t samplesToCopy                  = std::min<int64_t>(numSamples, kMaxReferenceSamples);

        audioBuffer_.clear();
        audioBuffer_.setSize(numChannels, static_cast<int>(samplesToCopy));

        if (numChannels >= 2) {
            audioBuffer_.copyFrom(0, 0, bufferL, static_cast<int>(samplesToCopy));
            audioBuffer_.copyFrom(1, 0, bufferR, static_cast<int>(samplesToCopy));
        }
        else {
            audioBuffer_.copyFrom(0, 0, bufferL, static_cast<int>(samplesToCopy));
        }

        // ─── Analizar el buffer ─────────────────────────────────────────────────
        analyzeAudioBuffer(audioBuffer_, sampleRate_);

        loaded_ = true;

        LogHelper::writeToLog("[ReferenceAnalyzer] Referencia cargada desde buffer: " + fileName_ + " ("
                              + juce::String(sampleRate_ / 1000) + " kHz, " + juce::String(duration_, 1) + "s, "
                              + juce::String(numChannels) + " canales)");

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  clear — Limpia la referencia cargada
    // ═══════════════════════════════════════════════════════════════════════════
    void ReferenceAnalyzer::clear()
    {
        loaded_ = false;
        filePath_.clear();
        fileName_.clear();
        sampleRate_ = 0;
        duration_   = 0.0;
        audioBuffer_.clear();
        // NOTA: analysis_ y loudness_ NO se resetean via copy assignment
        // porque contienen unique_ptr y juce::dsp::filters (no copiables).
        // loadFile() -> analyzeAudioBuffer() las prepara con el nuevo sample rate.
        for (auto& e : bandEnergies_) e = -100.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeAudioBuffer — Procesa el buffer por bloques a través de los
    //  analizadores (FFT + LUFS) para generar las métricas de la referencia.
    // ═══════════════════════════════════════════════════════════════════════════
    void ReferenceAnalyzer::analyzeAudioBuffer(const juce::AudioBuffer<float>& buffer, int sampleRate)
    {
        if (buffer.getNumSamples() == 0) return;

        int numSamples = buffer.getNumSamples();

        // ─── Preparar los analizadores con la tasa de sample de la referencia ──
        constexpr int kBlockSize = 1024;
        analysis_.prepare(static_cast<double>(sampleRate), kBlockSize);
        loudness_.prepare(static_cast<double>(sampleRate), kBlockSize);

        // ─── Procesar por bloques ──────────────────────────────────────────────
        int blockSize = kBlockSize;
        for (int pos = 0; pos < numSamples; pos += blockSize) {
            int thisBlock = std::min(blockSize, numSamples - pos);

            // Sum L+R a mono para análisis (como procesa el master)
            // Si es mono, usar el mismo canal
            juce::AudioBuffer<float> mono(1, thisBlock);
            auto* monoData = mono.getWritePointer(0);

            if (buffer.getNumChannels() >= 2) {
                auto* left  = buffer.getReadPointer(0, pos);
                auto* right = buffer.getReadPointer(1, pos);
                for (int i = 0; i < thisBlock; ++i) monoData[i] = (left[i] + right[i]) * 0.5f;
            }
            else {
                auto* src = buffer.getReadPointer(0, pos);
                std::copy(src, src + thisBlock, monoData);
            }

            // Alimentar el análisis (FFT + RMS + LUFS)
            analysis_.process(monoData, thisBlock);

            // LUFS necesita L/R separados — usar mono para ambos
            if (buffer.getNumChannels() >= 2) {
                loudness_.processBlock(buffer.getReadPointer(0, pos), buffer.getReadPointer(1, pos), thisBlock);
            }
            else {
                loudness_.processBlock(buffer.getReadPointer(0, pos), buffer.getReadPointer(0, pos), thisBlock);
            }
        }

        // ─── Calcular energías de banda desde el espectro final ────────────────
        computeBandEnergies();

        LogHelper::writeToLog(
            "[ReferenceAnalyzer] Análisis completado: "
            "Spectrum bins="
            + juce::String(kNumSpectrumBins) + " | LUFS M=" + juce::String(getMomentaryLUFS(), 1)
            + " ST=" + juce::String(getShortTermLUFS(), 1) + " I=" + juce::String(getIntegratedLUFS(), 1));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Band energy helpers (mismas bandas que CoachEngine::analyzeTonalBalanceReal)
    // ═══════════════════════════════════════════════════════════════════════════
    void ReferenceAnalyzer::computeBandEnergies()
    {
        const float* spectrum = analysis_.getSpectrum();
        if (spectrum == nullptr) {
            for (auto& e : bandEnergies_) e = -100.0f;
            return;
        }

        for (int b = 0; b < kNumBands; ++b) {
            bandEnergies_[b] = spectrumBandEnergy(spectrum, kBandBins[b][0], kBandBins[b][1]);
        }
    }

    float ReferenceAnalyzer::spectrumBandEnergy(const float* spectrum, int startBin, int endBin) const noexcept
    {
        if (spectrum == nullptr || startBin < 0 || endBin <= startBin || startBin >= kNumSpectrumBins) return -100.0f;

        endBin = std::min(endBin, kNumSpectrumBins);

        double sum = 0.0;
        int count  = 0;
        for (int b = startBin; b < endBin; ++b) {
            sum += static_cast<double>(spectrum[b]);
            ++count;
        }

        if (count == 0) return -100.0f;

        return static_cast<float>(sum / count);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Getters de LUFS
    // ═══════════════════════════════════════════════════════════════════════════
    float ReferenceAnalyzer::getMomentaryLUFS() const noexcept
    {
        return loaded_ ? loudness_.getMomentary() : -100.0f;
    }

    float ReferenceAnalyzer::getShortTermLUFS() const noexcept
    {
        return loaded_ ? loudness_.getShortTerm() : -100.0f;
    }

    float ReferenceAnalyzer::getIntegratedLUFS() const noexcept
    {
        return loaded_ ? loudness_.getIntegrated() : -100.0f;
    }

    float ReferenceAnalyzer::getBandEnergy(int band) const noexcept
    {
        if (!loaded_ || band < 0 || band >= kNumBands) return -100.0f;
        return bandEnergies_[band];
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  compareWith — Compara la referencia contra el AudioAnalyzer del master
    // ═══════════════════════════════════════════════════════════════════════════
    ReferenceComparison ReferenceAnalyzer::compareWith(const AudioAnalyzer& master) const
    {
        ReferenceComparison result;

        if (!loaded_) return result;

        // ─── Obtener análisis del master ───────────────────────────────────────
        const auto& masterAnalysis  = master.getMasterAnalysis();
        const float* masterSpectrum = masterAnalysis.getSpectrum();
        if (masterSpectrum == nullptr || masterAnalysis.getLastUpdateTime() == 0) return result;

        // ─── Calcular energías de banda del master ────────────────────────────
        float masterBands[kNumBands];
        for (int b = 0; b < kNumBands; ++b) {
            masterBands[b] = spectrumBandEnergy(masterSpectrum, kBandBins[b][0], kBandBins[b][1]);
        }

        // ─── Diferencia por banda (positivo = referencia tiene más energía) ────
        // Normalizamos para que la comparación sea significativa: restamos el
        // offset promedio para alinear los niveles generales.
        float refAvg = 0.0f, masterAvg = 0.0f;
        for (int b = 0; b < kNumBands; ++b) {
            if (bandEnergies_[b] > -80.0f) refAvg += bandEnergies_[b];
            if (masterBands[b] > -80.0f) masterAvg += masterBands[b];
        }
        refAvg /= kNumBands;
        masterAvg /= kNumBands;
        float levelOffset = refAvg - masterAvg; // cuanto más fuerte es la ref

        result.subDiff      = (bandEnergies_[0] - masterBands[0]) - levelOffset;
        result.bassDiff     = (bandEnergies_[1] - masterBands[1]) - levelOffset;
        result.lowMidDiff   = (bandEnergies_[2] - masterBands[2]) - levelOffset;
        result.highMidDiff  = (bandEnergies_[3] - masterBands[3]) - levelOffset;
        result.presenceDiff = (bandEnergies_[4] - masterBands[4]) - levelOffset;
        result.highDiff     = (bandEnergies_[5] - masterBands[5]) - levelOffset;
        result.airDiff      = (bandEnergies_[6] - masterBands[6]) - levelOffset;

        // ─── Diferencia de LUFS ───────────────────────────────────────────────
        result.refMomentaryLUFS  = getMomentaryLUFS();
        result.refShortTermLUFS  = getShortTermLUFS();
        result.refIntegratedLUFS = getIntegratedLUFS();

        float masterShortTerm  = master.getShortTermLUFS();
        float masterIntegrated = master.getIntegratedLUFS();

        result.lufsShortTermDiff  = getShortTermLUFS() - masterShortTerm;
        result.lufsIntegratedDiff = getIntegratedLUFS() - masterIntegrated;

        // ─── Similitud espectral (0-1) ────────────────────────────────────────
        // Computamos la diferencia RMS entre los perfiles normalizados
        float totalDiffSq     = 0.0f;
        float maxPossibleDiff = 20.0f; // 20 dB de diferencia = 0 similitud
        int validBands        = 0;

        for (int b = 0; b < kNumBands; ++b) {
            if (bandEnergies_[b] > -80.0f && masterBands[b] > -80.0f) {
                float refNorm = bandEnergies_[b] - refAvg;
                float masNorm = masterBands[b] - masterAvg;
                float diff    = refNorm - masNorm;
                totalDiffSq += diff * diff;
                ++validBands;
            }
        }

        if (validBands > 0) {
            float rmsDiff             = std::sqrt(totalDiffSq / validBands);
            result.spectralSimilarity = std::max(0.0f, 1.0f - (rmsDiff / maxPossibleDiff));
        }

        // ─── Generar resumen textual ──────────────────────────────────────────
        {
            juce::String summary;

            // Si la similitud es alta, felicitar
            if (result.spectralSimilarity > 0.7f) {
                summary += "🎯 **Balance tonal muy cercano a la referencia!**\n";
                summary += "Similitud espectral: " + juce::String(static_cast<int>(result.spectralSimilarity * 100.0f))
                           + "%\n";
            }
            else if (result.spectralSimilarity > 0.4f) {
                summary += "📊 **Balance tonal aceptable** (similitud: "
                           + juce::String(static_cast<int>(result.spectralSimilarity * 100.0f)) + "%)\n";
            }
            else {
                summary += "🔊 **Balance tonal diferente a la referencia**\n";
            }

            // Detectar las bandas con mayor diferencia
            struct BandDiff
            {
                float diff;
                const char* name;
            };

            BandDiff bandDiffs[kNumBands] = {{result.subDiff, "Sub"},
                                             {result.bassDiff, "Bass"},
                                             {result.lowMidDiff, "Low-Mid"},
                                             {result.highMidDiff, "High-Mid"},
                                             {result.presenceDiff, "Presence"},
                                             {result.highDiff, "High"},
                                             {result.airDiff, "Air"}};
            // Ordenar por diferencia absoluta (más grande primero) — simple burbuja
            for (int i = 0; i < kNumBands - 1; ++i) {
                for (int j = i + 1; j < kNumBands; ++j) {
                    if (std::abs(bandDiffs[i].diff) < std::abs(bandDiffs[j].diff)) {
                        std::swap(bandDiffs[i], bandDiffs[j]);
                    }
                }
            }

            // Mostrar las 3 bandas con más diferencia
            summary += "\n**Diferencias principales vs referencia:**\n";
            int shown = std::min(kNumBands, 3);
            for (int i = 0; i < shown; ++i) {
                const auto& bd = bandDiffs[i];
                if (std::abs(bd.diff) < 1.0f) {
                    summary += "  ✅ " + juce::String(bd.name) + " — similar\n";
                }
                else if (bd.diff > 0) {
                    summary += "  🔴 " + juce::String(bd.name) + " +" + juce::String(std::abs(bd.diff), 1)
                               + " dB (ref tiene más)\n";
                }
                else {
                    summary += "  🔵 " + juce::String(bd.name) + " -" + juce::String(std::abs(bd.diff), 1)
                               + " dB (ref tiene menos)\n";
                }
            }

            // LUFS comparison
            summary += "\n**Loudness:**\n";
            summary += "  Referencia: " + juce::String(getIntegratedLUFS(), 1) + " LUFS I\n";
            if (masterIntegrated > -60.0f) {
                summary += "  Tu mezcla:  " + juce::String(masterIntegrated, 1) + " LUFS I\n";
                float lufsGap = getIntegratedLUFS() - masterIntegrated;
                if (lufsGap > 2.0f) summary += "  ⬆️ Sube " + juce::String(lufsGap, 1) + " dB para igualar loudness\n";
                else if (lufsGap < -2.0f)
                    summary += "  ⬇️ Baja " + juce::String(-lufsGap, 1) + " dB para igualar loudness\n";
                else
                    summary += "  ✅ Nivel de loudness similar a la referencia\n";
            }

            result.summary = summary;
        }

        result.valid = true;
        return result;
    }

} // namespace mixcoach

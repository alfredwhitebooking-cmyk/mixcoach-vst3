#include "TelemetryCollector.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  LoudnessMeter — EBU R128 / ITU BS.1770 simplificado
// ═══════════════════════════════════════════════════════════════════════════

LoudnessMeter::LoudnessMeter() = default;

void LoudnessMeter::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();

    // ─── Coeficientes K-weighting ────────────────────────────────────────
    // Stage 1: High-pass filter 20Hz (2nd order Butterworth)
    {
        double w0 = 2.0 * juce::MathConstants<double>::pi * 20.0 / sampleRate;
        double cosW0 = std::cos(w0);
        double sinW0 = std::sin(w0);
        double alpha = sinW0 / std::sqrt(2.0); // Q = 0.707

        double b0 = (1.0 - cosW0) / 2.0;
        double b1 = 1.0 - cosW0;
        double b2 = (1.0 - cosW0) / 2.0;
        double a0 = 1.0 + alpha;
        double a1 = -2.0 * cosW0;
        double a2 = 1.0 - alpha;

        // Normalizar
        hpB0_ = b0 / a0;
        hpB1_ = b1 / a0;
        hpB2_ = b2 / a0;
        hpA1_ = a1 / a0;
        hpA2_ = a2 / a0;
    }

    // Stage 2: Shelving filter +4dB @ 1.5kHz (1st order)
    {
        double gain = std::pow(10.0, 4.0 / 20.0); // +4 dB
        double w0 = 2.0 * juce::MathConstants<double>::pi * 1500.0 / sampleRate;
        double cosW0 = std::cos(w0);
        double sinW0 = std::sin(w0);
        double A = std::sqrt(gain);
        double alpha = sinW0 / (2.0 * std::sqrt(A));

        double b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + 2.0 * std::sqrt(A) * alpha);
        double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
        double b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - 2.0 * std::sqrt(A) * alpha);
        double a0 = (A + 1.0) - (A - 1.0) * cosW0 + 2.0 * std::sqrt(A) * alpha;
        double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
        double a2 = (A + 1.0) - (A - 1.0) * cosW0 - 2.0 * std::sqrt(A) * alpha;

        shB0_ = b0 / a0;
        shB1_ = b1 / a0;
        shB2_ = b2 / a0;
        shA1_ = a1 / a0;
        shA2_ = a2 / a0;
    }

    // Pre-calcular buffers por tiempo
    int momentarySamples = static_cast<int>(sampleRate * 0.4);   // 400ms
    int shortTermSamples = static_cast<int>(sampleRate * 3.0);   // 3s
    int blockSize = 512; // tamaño de bloque típico

    // Pre-llenar colas con espacio esperado
    momentaryQueue_.resize(momentarySamples / blockSize + 1, 0.0f);
    shortTermQueue_.resize(shortTermSamples / blockSize + 1, 0.0f);
}

void LoudnessMeter::reset()
{
    hpState_ = {};
    shState_ = {};
    momentaryQueue_.clear();
    shortTermQueue_.clear();
    integratedQueue_.clear();
    momentaryLUFS_ = -100.0f;
    shortTermLUFS_ = -100.0f;
    integratedLUFS_ = -100.0f;
    loudnessRange_ = 0.0f;
    blocksSinceLastCalc_ = 0;
}

void LoudnessMeter::process(const float* left, const float* right, int numSamples)
{
    if (numSamples <= 0 || sampleRate_ <= 0.0) return;

    // Asegurar buffers pre-asignados (evita heap allocations en audio thread)
    if ((int)monoBuffer_.size() < numSamples) {
        monoBuffer_.resize(numSamples, 0.0f);
        filterBuffer_.resize(numSamples, 0.0f);
    }

    // Mezclar a mono para medición de loudness (EBU R128 usa suma)
    for (int i = 0; i < numSamples; ++i)
        monoBuffer_[i] = (left[i] + right[i]) * 0.5f;

    // Aplicar K-weighting filter
    applyKFilter(monoBuffer_.data(), filterBuffer_.data(), numSamples);

    // Calcular mean square de este bloque
    float meanSquare = computeMeanSquare(filterBuffer_.data(), numSamples);

    // Encolar para cada ventana temporal
    momentaryQueue_.push_back(meanSquare);
    shortTermQueue_.push_back(meanSquare);
    integratedQueue_.push_back(meanSquare);

    // Limitar tamaño de colas
    int momentaryMax = static_cast<int>(sampleRate_ * 0.4 / numSamples) + 1;
    int shortTermMax = static_cast<int>(sampleRate_ * 3.0 / numSamples) + 1;

    while ((int)momentaryQueue_.size() > momentaryMax)
        momentaryQueue_.pop_front();
    while ((int)shortTermQueue_.size() > shortTermMax)
        shortTermQueue_.pop_front();

    // Recalcular LUFS periódicamente (no cada bloque para no saturar CPU)
    blocksSinceLastCalc_++;
    if (blocksSinceLastCalc_ >= kCalcInterval) {
        blocksSinceLastCalc_ = 0;

        // Promediar mean squares de cada ventana
        double momentarySum = 0.0;
        for (auto ms : momentaryQueue_) momentarySum += ms;
        if (!momentaryQueue_.empty())
            momentaryLUFS_ = toLUFS((float)(momentarySum / momentaryQueue_.size()));

        double shortTermSum = 0.0;
        for (auto ms : shortTermQueue_) shortTermSum += ms;
        if (!shortTermQueue_.empty())
            shortTermLUFS_ = toLUFS((float)(shortTermSum / shortTermQueue_.size()));

        // Integrated: incluir solo bloques por encima del gate absoluto (-70 LUFS)
        double integratedSum = 0.0;
        int integratedCount = 0;
        constexpr float kAbsoluteGate = -70.0f;
        for (auto ms : integratedQueue_) {
            float lufs = toLUFS(ms);
            if (lufs > kAbsoluteGate) {
                integratedSum += ms;
                integratedCount++;
            }
        }
        if (integratedCount > 0)
            integratedLUFS_ = toLUFS((float)(integratedSum / integratedCount));

        // Loudness Range simplificado: percentil 10-95
        // Usa lrBuffer_ pre-asignado (evita heap allocation en audio thread)
        if (!momentaryQueue_.empty() && momentaryQueue_.size() > 10) {
            auto qSize = momentaryQueue_.size();
            if (lrBuffer_.size() < qSize)
                lrBuffer_.resize(qSize, 0.0f);

            int idx = 0;
            for (auto ms : momentaryQueue_)
                lrBuffer_[idx++] = toLUFS(ms);

            std::sort(lrBuffer_.begin(), lrBuffer_.begin() + idx);
            size_t p10 = idx / 10;
            size_t p95 = idx * 95 / 100;
            if (p95 > p10 && p95 < (size_t)idx)
                loudnessRange_ = lrBuffer_[p95] - lrBuffer_[p10];
        }
    }
}

void LoudnessMeter::applyKFilter(const float* input, float* output, int numSamples)
{
    // Stage 1: High-pass 20Hz
    for (int i = 0; i < numSamples; ++i) {
        double in = input[i];
        double out = hpB0_ * in + hpB1_ * hpState_.x1 + hpB2_ * hpState_.x2
                   - hpA1_ * hpState_.y1 - hpA2_ * hpState_.y2;
        hpState_.x2 = hpState_.x1;
        hpState_.x1 = in;
        hpState_.y2 = hpState_.y1;
        hpState_.y1 = out;
        output[i] = (float)out;
    }

    // Stage 2: Shelving +4dB @ 1.5kHz
    for (int i = 0; i < numSamples; ++i) {
        double in = output[i];
        double out = shB0_ * in + shB1_ * shState_.x1 + shB2_ * shState_.x2
                   - shA1_ * shState_.y1 - shA2_ * shState_.y2;
        shState_.x2 = shState_.x1;
        shState_.x1 = in;
        shState_.y2 = shState_.y1;
        shState_.y1 = out;
        output[i] = (float)out;
    }
}

float LoudnessMeter::computeMeanSquare(const float* data, int numSamples) const
{
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sum += (double)data[i] * data[i];
    return (float)(sum / numSamples);
}

float LoudnessMeter::toLUFS(float meanSquare)
{
    if (meanSquare <= 1e-12f) return -100.0f;
    return -0.691f + 10.0f * std::log10(meanSquare);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TelemetryCollector — DSP en tiempo real
// ═══════════════════════════════════════════════════════════════════════════

// ─── Constructor ─────────────────────────────────────────────────────────
// NOTA CRÍTICA: No creamos el FFT aquí porque juce::dsp::FFT aloca memoria
// interna que puede crashear durante el escaneo VST3 en FL Studio (sandbox).
// Tampoco hacemos heap allocations (resize de vectores).
// Todo se inicializa lazy en prepare().
TelemetryCollector::TelemetryCollector() = default;

void TelemetryCollector::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = sampleRate;
    blockCount_ = 0;

    // Crear FFT (lazy — no en constructor)
    if (!fft_)
        fft_ = std::make_unique<juce::dsp::FFT>(kFFTOrder);

    // Ventana Hann
    window_.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / kFFTSize));

    fftBuffer_.assign(kFFTSize, 0.0f);
    fftWritePos_ = 0;

    loudnessMeter_.prepare(sampleRate);
}

TrackTelemetry TelemetryCollector::collect(const juce::AudioBuffer<float>& buffer)
{
    TrackTelemetry telemetry;
    telemetry.timestamp = juce::Time::getMillisecondCounter() * 1000;
    telemetry.slotIndex = -1;
    telemetry.active    = true;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    if (numSamples > 0) {
        if (numChannels >= 2) {
            auto left  = buffer.getReadPointer(0);
            auto right = buffer.getReadPointer(1);

            // DSP básico
            telemetry.peakLeft     = computePeak(left, numSamples);
            telemetry.peakRight    = computePeak(right, numSamples);
            telemetry.rmsLeft      = computeRMS(left, numSamples);
            telemetry.rmsRight     = computeRMS(right, numSamples);
            telemetry.correlation  = computeCorrelation(left, right, numSamples);

            // Crest Factor (Peak - RMS en dB)
            telemetry.crestFactor = computeCrestFactor(
                juce::jmax(telemetry.peakLeft, telemetry.peakRight),
                (telemetry.rmsLeft + telemetry.rmsRight) * 0.5f);

            // Últimas muestras para vectorscope (último par del buffer)
            telemetry.sampleL = left[numSamples - 1];
            telemetry.sampleR = right[numSamples - 1];

            // FFT Spectrum (cada kFFTInterval bloques)
            if (blockCount_ % kFFTInterval == 0) {
                computeSpectrum(left, numSamples, telemetry.spectrum, kSpectrumBins);
            }

            // LUFS (EBU R128 via LoudnessMeter)
            loudnessMeter_.process(left, right, numSamples);
        } else if (numChannels == 1) {
            auto mono = buffer.getReadPointer(0);

            telemetry.peakLeft     = computePeak(mono, numSamples);
            telemetry.peakRight    = telemetry.peakLeft;
            telemetry.rmsLeft      = computeRMS(mono, numSamples);
            telemetry.rmsRight     = telemetry.rmsLeft;
            telemetry.correlation  = 1.0f;
            telemetry.crestFactor  = computeCrestFactor(telemetry.peakLeft, telemetry.rmsLeft);
            telemetry.sampleL      = mono[numSamples - 1];
            telemetry.sampleR      = 0.0f;

            if (blockCount_ % kFFTInterval == 0)
                computeSpectrum(mono, numSamples, telemetry.spectrum, kSpectrumBins);
        }
    }

    // Poblar LUFS desde el LoudnessMeter (se actualiza internamente)
    telemetry.lufsIntegrated = loudnessMeter_.getIntegratedLUFS();
    telemetry.lufsShortTerm  = loudnessMeter_.getShortTermLUFS();
    telemetry.lufsMomentary  = loudnessMeter_.getMomentaryLUFS();
    telemetry.lufsTruePeak   = juce::jmax(telemetry.peakLeft, telemetry.peakRight);
    telemetry.loudnessRange  = loudnessMeter_.getLoudnessRange();

    blockCount_++;
    return telemetry;
}

float TelemetryCollector::computePeak(const float* data, int numSamples) const
{
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max(peak, std::abs(data[i]));

    return (peak > 0.0f)
        ? juce::Decibels::gainToDecibels(peak)
        : -100.0f;
}

float TelemetryCollector::computeRMS(const float* data, int numSamples) const
{
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sumSq += static_cast<double>(data[i]) * data[i];

    return (sumSq > 0.0)
        ? static_cast<float>(juce::Decibels::gainToDecibels(
              static_cast<float>(std::sqrt(sumSq / numSamples))))
        : -100.0f;
}

float TelemetryCollector::computeCorrelation(const float* left, const float* right, int numSamples) const
{
    double sumProduct = 0.0;
    double sumLeftSq  = 0.0;
    double sumRightSq = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        sumProduct += static_cast<double>(left[i]) * right[i];
        sumLeftSq  += static_cast<double>(left[i]) * left[i];
        sumRightSq += static_cast<double>(right[i]) * right[i];
    }

    auto denom = std::sqrt(sumLeftSq * sumRightSq);
    return (denom > 1e-12)
        ? static_cast<float>(sumProduct / denom)
        : 1.0f;
}

float TelemetryCollector::computeCrestFactor(float peakDb, float rmsDb) const
{
    if (rmsDb < -99.0f || peakDb < -99.0f)
        return 0.0f;
    return peakDb - rmsDb;
}

void TelemetryCollector::computeSpectrum(const float* data, int numSamples,
                                          float* outSpectrum, int numBins)
{
    if (!fft_ || fftBuffer_.size() < (size_t)kFFTSize || window_.size() < (size_t)kFFTSize)
        return;

    // Acumular en rolling buffer
    int toCopy = std::min(numSamples, kFFTSize);
    for (int i = 0; i < toCopy; ++i) {
        fftBuffer_[fftWritePos_] = data[i];
        fftWritePos_ = (fftWritePos_ + 1) % kFFTSize;
    }

    // Copiar buffer ordenado, aplicar ventana, y hacer FFT.
    // JUCE requiere 2 * fftSize floats para la transformacion real in-place.
    std::array<float, kFFTSize * 2> fftInput{};
    for (int i = 0; i < kFFTSize; ++i) {
        int idx = (fftWritePos_ + i) % kFFTSize;
        fftInput[i] = fftBuffer_[idx] * window_[i];
    }

    // Real FFT: transforma in-place
    fft_->performRealOnlyForwardTransform(fftInput.data());

    // Extraer magnitudes normalizadas
    int usableBins = std::min(kFFTSize / 2, numBins);
    for (int i = 0; i < usableBins; ++i) {
        float real = fftInput[i * 2];
        float imag = fftInput[i * 2 + 1];
        float mag = std::sqrt(real * real + imag * imag);
        // Normalizar
        float normalized = mag / (float)kFFTSize;
        // Escalar logarítmicamente para mejor visualización
        outSpectrum[i] = juce::jlimit(0.0f, 1.0f,
            (20.0f * std::log10(normalized + 1e-6f) + 80.0f) / 80.0f);
    }

    // Rellenar bins restantes
    for (int i = usableBins; i < numBins; ++i)
        outSpectrum[i] = 0.0f;
}

} // namespace mixcoach

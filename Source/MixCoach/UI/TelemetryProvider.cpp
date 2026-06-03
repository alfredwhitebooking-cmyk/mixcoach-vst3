#include "TelemetryProvider.h"

namespace mixcoach {

namespace {

bool telemetryHasSpectrum(const TrackTelemetry& telem)
{
    for (int fi = 0; fi < 256; ++fi)
    {
        if (telem.spectrum[fi] > 0.0001f)
            return true;
    }
    return false;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
//  TelemetryProvider::getLatest — Punto único de entrada para analizadores
//
//  Los componentes UI (meter, spectrograph, etc.) llaman a este método
//  y reciben datos sin importar el modo. No saben si es Single o Master.
// ═══════════════════════════════════════════════════════════════════════════
TrackTelemetry TelemetryProvider::getLatest()
{
    if (registry_ == nullptr)
        return TrackTelemetry{};

    if (mode_ == Mode::Single && selectedSlot_ >= 0)
    {
        auto& buf = registry_->getTelemetry(selectedSlot_);
        auto latest = buf.latest();
        // Preservar slotIndex para que el resto del sistema sepa qué track es
        if (latest.slotIndex < 0)
            latest.slotIndex = selectedSlot_;
        return latest;
    }

    if (mode_ == Mode::Bus)
        return buildBus(selectedBus_);

    return buildMaster();
}

// ═══════════════════════════════════════════════════════════════════════════
//  TelemetryProvider::buildMaster — UNA SOLA PASADA por todos los tracks
//
//  ═══ OPTIMIZACIÓN PERFORMANCE: Single-pass ═════════════════════════════════
//  Antes: 3 forEachActive() separados (peaks/RMS, spectrum, loudest track)
//  = 3 × N iteraciones por llamada. Con 100 tracks y 60fps: 18,000 it/s.
//  AHORA: 1 sola pasada que recolecta peaks, RMS, correlation, spectrum,
//  y loudest track simultáneamente. 1 × N = 6,000 it/s.
//
//  Estrategia de composición:
//  • peaks → MAX
//  • spectrum → MAX por bin (en la misma pasada que peaks)
//  • correlation → PROMEDIO
//  • samples/crest/lufs → del LOUDEST track
//  • colour → violeta accent
// ═══════════════════════════════════════════════════════════════════════════
TrackTelemetry TelemetryProvider::buildMaster()
{
    TrackTelemetry result;
    result.active = false;
    result.slotIndex = -1;

    if (registry_ == nullptr)
        return result;

    int activeCount = 0;
    float sumRmsL = 0.0f, sumRmsR = 0.0f;
    float sumCorr = 0.0f;
    float maxPeak = -100.0f;
    int loudestSlot = -1;

    // ─── Pre-inicializar spectrum (evita memset en cada pasada) ─────────
    std::fill(std::begin(result.spectrum), std::end(result.spectrum), 0.0f);

    // ─── PASADA ÚNICA: recolectar todo simultáneamente ─────────────────
    registry_->forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0) return;
        if (info.stale) return;

        auto& buf = registry_->getTelemetry(idx);
        auto t = buf.latest();

        result.active = true;

        // Peaks: máximo de todos
        if (t.peakLeft > result.peakLeft)   result.peakLeft  = t.peakLeft;
        if (t.peakRight > result.peakRight) result.peakRight = t.peakRight;

        // RMS: sumamos para promediar después
        sumRmsL += t.rmsLeft;
        sumRmsR += t.rmsRight;

        // Correlation: sumamos para promediar después
        sumCorr += t.correlation;

        // Spectrum: MAX por bin (en la misma pasada)
        for (int j = 0; j < 256; ++j)
        {
            if (t.spectrum[j] > result.spectrum[j])
                result.spectrum[j] = t.spectrum[j];
        }

        // Loudest track (para crest, samples, LUFS)
        float slotMaxPeak = juce::jmax(t.peakLeft, t.peakRight);
        if (slotMaxPeak > maxPeak) {
            maxPeak = slotMaxPeak;
            loudestSlot = idx;
        }

        ++activeCount;
    });

    if (activeCount == 0 || !result.active)
        return result;

    // ─── Promediar RMS y correlation ─────────────────────────────────────
    float invCount = 1.0f / static_cast<float>(activeCount);
    result.rmsLeft     = sumRmsL * invCount;
    result.rmsRight    = sumRmsR * invCount;
    result.correlation = sumCorr * invCount;

    // ─── Datos del LOUDEST track (para vectorscope, crest, etc.) ────────
    if (loudestSlot >= 0)
    {
        auto& buf = registry_->getTelemetry(loudestSlot);
        auto t = buf.latest();

        result.rmsLeft     = t.rmsLeft;
        result.rmsRight    = t.rmsRight;
        result.sampleL     = t.sampleL;
        result.sampleR     = t.sampleR;

        float loudestAvgRms = (t.rmsLeft + t.rmsRight) * 0.5f;
        if (result.peakLeft > -99.0f) {
            result.crestFactor = juce::jmax(0.0f, result.peakLeft - loudestAvgRms);
        }
        result.lufsIntegrated = t.lufsIntegrated;
        result.lufsShortTerm  = t.lufsShortTerm;
        result.lufsMomentary  = t.lufsMomentary;
        result.lufsTruePeak   = t.lufsTruePeak;
        result.loudnessRange  = t.loudnessRange;
        result.slotIndex = loudestSlot;
    }

    // ─── Color neutro para modo Master ──────────────────────────────────
    result.colour = juce::Colour(0xFF7C3AED);

    result.timestamp = static_cast<int64_t>(
        juce::Time::getMillisecondCounter()) * 1000;

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
//  TelemetryProvider::buildBus — Single-pass, filtrado por bus
// ═══════════════════════════════════════════════════════════════════════════
TrackTelemetry TelemetryProvider::buildBus(BusType bus)
{
    TrackTelemetry result;
    result.active = false;
    result.slotIndex = -1;

    if (registry_ == nullptr)
        return result;

    int activeCount = 0;
    float sumRmsL = 0.0f, sumRmsR = 0.0f;
    float sumCorr = 0.0f;
    float maxPeak = -100.0f;
    int loudestSlot = -1;

    std::fill(std::begin(result.spectrum), std::end(result.spectrum), 0.0f);

    // ─── PASADA ÚNICA: todo en 1 forEachActive ──────────────────────────
    registry_->forEachActive([&](const SlotInfo& info) {
        if (info.bus != bus) return;
        if (info.stale) return;

        int idx = info.slotIndex;
        if (idx < 0) return;

        auto& buf = registry_->getTelemetry(idx);
        auto t = buf.latest();

        result.active = true;

        if (t.peakLeft > result.peakLeft)   result.peakLeft  = t.peakLeft;
        if (t.peakRight > result.peakRight) result.peakRight = t.peakRight;

        sumRmsL += t.rmsLeft;
        sumRmsR += t.rmsRight;
        sumCorr += t.correlation;

        // Spectrum MAX en la misma pasada
        for (int j = 0; j < 256; ++j)
        {
            if (t.spectrum[j] > result.spectrum[j])
                result.spectrum[j] = t.spectrum[j];
        }

        float slotMaxPeak = juce::jmax(t.peakLeft, t.peakRight);
        if (slotMaxPeak > maxPeak) {
            maxPeak = slotMaxPeak;
            loudestSlot = idx;
        }

        ++activeCount;
    });

    if (activeCount == 0 || !result.active)
        return result;

    float invCount = 1.0f / static_cast<float>(activeCount);
    result.rmsLeft     = sumRmsL * invCount;
    result.rmsRight    = sumRmsR * invCount;
    result.correlation = sumCorr * invCount;

    if (loudestSlot >= 0)
    {
        auto& buf = registry_->getTelemetry(loudestSlot);
        auto t = buf.latest();

        result.rmsLeft     = t.rmsLeft;
        result.rmsRight    = t.rmsRight;
        result.sampleL     = t.sampleL;
        result.sampleR     = t.sampleR;

        float loudestAvgRms = (t.rmsLeft + t.rmsRight) * 0.5f;
        if (result.peakLeft > -99.0f) {
            result.crestFactor = juce::jmax(0.0f, result.peakLeft - loudestAvgRms);
        }
        result.lufsIntegrated = t.lufsIntegrated;
        result.lufsShortTerm  = t.lufsShortTerm;
        result.lufsMomentary  = t.lufsMomentary;
        result.lufsTruePeak   = t.lufsTruePeak;
        result.loudnessRange  = t.loudnessRange;
        result.slotIndex = loudestSlot;
    }

    result.colour = getBusColour(static_cast<int>(bus));
    result.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    return result;
}

} // namespace mixcoach

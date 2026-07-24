#pragma once
#include <juce_core/juce_core.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzerType — Qué tipo de analizador abrir como evidencia.
//
//  El CoachEngine selecciona un tipo según el problema detectado:
//    LevelMeter    → problemas de gain staging / balance
//    PhaseMeter    → problemas de fase / correlación estéreo
//    Spectrum      → problemas de EQ / balance tonal
//    TargetMarkers → comparación contra referencia
// ═══════════════════════════════════════════════════════════════════════════
enum class AnalyzerType {
    None = 0,
    LevelMeter,
    PhaseMeter,
    Spectrum,
    TargetMarkers
};

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzerParams — Datos que cada analizador necesita para renderizarse.
//
//  Solo los campos relevantes para el AnalyzerType activo se usan.
//  Ejemplo: highlightFreq solo se usa cuando type == Spectrum.
// ═══════════════════════════════════════════════════════════════════════════
struct AnalyzerParams {
    juce::String trackName;            // Pista a analizar (ej: "Kick")
    float levelDb           = -60.0f;  // LevelMeter: nivel en dB
    float correlation       = 1.0f;    // PhaseMeter: correlación -1..+1
    float highlightFreq     = 0.0f;    // Spectrum: frecuencia a resaltar (Hz)
    float highlightGain     = 0.0f;    // Spectrum: ganancia en esa frecuencia (dB)
    juce::String title;                // Título contextual (ej: "Kick vs 808 en 60Hz")

    // TargetMarkers: pares (label, targetValue, actualValue)
    std::vector<std::tuple<juce::String, float, float>> targets;
    juce::String summary;              // Texto resumen para TargetMarkers

    /** Crea parámetros para abrir un LevelMeter. */
    static AnalyzerParams forLevel(const juce::String& track, float db) noexcept
    {
        AnalyzerParams p;
        p.trackName = track;
        p.levelDb = db;
        p.title = track + " — Nivel";
        return p;
    }

    /** Crea parámetros para abrir un PhaseMeter. */
    static AnalyzerParams forPhase(const juce::String& track, float corr) noexcept
    {
        AnalyzerParams p;
        p.trackName = track;
        p.correlation = corr;
        p.title = track + " — Fase";
        return p;
    }

    /** Crea parámetros para abrir un SpectrumAnalyzer. */
    static AnalyzerParams forSpectrum(const juce::String& track,
                                       float freqHz, float gainDb) noexcept
    {
        AnalyzerParams p;
        p.trackName = track;
        p.highlightFreq = freqHz;
        p.highlightGain = gainDb;
        p.title = track + " — Espectro";
        return p;
    }

    /** Crea parámetros para abrir TargetMarkers. */
    static AnalyzerParams forTargets(const juce::String& title_,
                                      const juce::String& summary_,
                                      std::vector<std::tuple<juce::String, float, float>> targets_) noexcept
    {
        AnalyzerParams p;
        p.title = title_;
        p.summary = summary_;
        p.targets = std::move(targets_);
        return p;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzerManager — Fuente de verdad para qué analizador está abierto.
//
//  El CoachEngine escribe (open/close). La UI lee (isOpen/getParams).
//  Cuando cambia el estado, onChanged se dispara para que la UI reaccione.
//
//  Uso (CoachEngine):
//    analyzerManager_.open(AnalyzerType::Spectrum,
//        AnalyzerParams::forSpectrum("Kick", 60.0f, -4.5f));
//
//  Uso (UI):
//    if (analyzerManager_.isOpen()) {
//        auto type = analyzerManager_.getCurrentType();
//        auto& params = analyzerManager_.getCurrentParams();
//        // Mostrar componente correspondiente
//    }
// ═══════════════════════════════════════════════════════════════════════════
class AnalyzerManager {
public:
    AnalyzerManager() = default;
    ~AnalyzerManager() = default;

    // Non-copyable
    AnalyzerManager(const AnalyzerManager&) = delete;
    AnalyzerManager& operator=(const AnalyzerManager&) = delete;

    /** Abre un analizador con los parámetros especificados. */
    void open(AnalyzerType type, const AnalyzerParams& params) noexcept
    {
        if (type == AnalyzerType::None) { close(); return; }
        currentType_ = type;
        currentParams_ = params;
        if (onChanged) onChanged();
    }

    /** Cierra el analizador actual. */
    void close() noexcept
    {
        if (currentType_ == AnalyzerType::None) return;
        currentType_ = AnalyzerType::None;
        currentParams_ = AnalyzerParams{};
        if (onChanged) onChanged();
    }

    /** ¿Hay un analizador abierto? */
    bool isOpen() const noexcept { return currentType_ != AnalyzerType::None; }

    /** Tipo del analizador abierto (None si no hay). */
    AnalyzerType getCurrentType() const noexcept { return currentType_; }

    /** Parámetros del analizador abierto. */
    const AnalyzerParams& getCurrentParams() const noexcept { return currentParams_; }

    /** Callback: se dispara cuando open() o close() cambian el estado. */
    std::function<void()> onChanged;

    /** Devuelve nombre legible del tipo de analizador. */
    static juce::String getDisplayName(AnalyzerType type) noexcept
    {
        switch (type) {
            case AnalyzerType::LevelMeter:    return "Level Meter";
            case AnalyzerType::PhaseMeter:    return "Phase Meter";
            case AnalyzerType::Spectrum:      return "Spectrum";
            case AnalyzerType::TargetMarkers: return "Targets";
            default:                          return {};
        }
    }

private:
    AnalyzerType currentType_ = AnalyzerType::None;
    AnalyzerParams currentParams_;
};

} // namespace mixcoach

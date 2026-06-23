#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <mutex>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  BandDiagnostic — Diagnóstico espectral por banda de frecuencia
    //  Describe un problema o acierto en una región de frecuencia específica.
    //  Se produce desde SemanticComparator / CoachEngine y se consume en
    //  SpectrographComponent para el overlay visual de diagnóstico.
    // ═══════════════════════════════════════════════════════════════════════════
    struct BandDiagnostic
    {
        float lowFreqHz  = 0.0f;  // Frecuencia inferior de la banda (Hz)
        float highFreqHz = 0.0f;  // Frecuencia superior de la banda (Hz)
        float severity   = 0.0f;  // 0.0 (información) a 1.0 (crítico)
        bool isCritical  = false; // Si es un problema que requiere atención
        bool isPraise    = false; // Si es un acierto (algo que está bien)
        juce::String description; // Descripción legible del diagnóstico
        juce::String trackName;   // Nombre de la pista asociada
        juce::String trackRole;   // Rol de la pista (Kick, Bass, etc.)

        [[nodiscard]] juce::Colour getDisplayColour() const noexcept;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  PhaseDiagnostic — Diagnóstico de fase / correlación estéreo
    //  Describe problemas en el vectorscope o correlation meter.
    //  Se produce desde AnalyzerInterpreter y se consume en
    //  VectorscopeComponent / PhaseCorrelationMeter para overlay visual.
    // ═══════════════════════════════════════════════════════════════════════════
    struct PhaseDiagnostic
    {
        float severity    = 0.0f;  // 0.0 (info) a 1.0 (crítico)
        bool isWarning    = false; // Si es una advertencia (fuera de fase)
        bool isPraise     = false; // Si es un acierto (fase saludable)
        float correlation = 1.0f;  // Correlación estéreo (-1.0 a +1.0)
        juce::String description;  // Descripción legible del diagnóstico

        [[nodiscard]] juce::Colour getDisplayColour() const noexcept;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  DiagnosticBridge — Puente entre el análisis semántico y la UI visual
    //
    //  Recibe diagnósticos desde el motor de análisis (CoachEngine /
    //  SemanticComparator) y los pone a disposición de los componentes UI
    //  (SpectrographComponent, PhaseScopePanel, etc.) mediante ChangeBroadcaster.
    //
    //  Thread-safe: todos los setters/getters usan mutex.
    //  Uso típico:
    //    - Engine thread/timer: pushDiagnostics(DiagnosticBridge&, CoachEngine&)
    //    - UI thread (ChangeListener): bridge.getDiagnostics()
    // ═══════════════════════════════════════════════════════════════════════════
    class DiagnosticBridge : public juce::ChangeBroadcaster
    {
    public:
        DiagnosticBridge()           = default;
        ~DiagnosticBridge() override = default;

        // ═══ BandDiagnostics (espectral / overlay en Spectrograph) ═══════════

        /** Reemplaza todos los diagnósticos activos con una nueva lista.
            Dispara ChangeBroadcaster para notificar a los listeners de UI. */
        void setDiagnostics(const std::vector<BandDiagnostic>& diagnostics);

        /** Retorna los diagnósticos activos (copia thread-safe). */
        std::vector<BandDiagnostic> getDiagnostics() const;

        /** Limpia todos los diagnósticos activos. */
        void clearDiagnostics();

        /** Retorna true si hay al menos un diagnóstico activo. */
        [[nodiscard]] bool hasDiagnostics() const noexcept;

        // ═══ PhaseDiagnostics (fase / overlay en Vectorscope + PhaseMeter) ═══

        /** Reemplaza todos los diagnósticos de fase activos. */
        void setPhaseDiagnostics(const std::vector<PhaseDiagnostic>& diagnostics);

        /** Retorna los diagnósticos de fase activos (copia thread-safe). */
        std::vector<PhaseDiagnostic> getPhaseDiagnostics() const;

        /** Limpia todos los diagnósticos de fase. */
        void clearPhaseDiagnostics();

        /** Retorna true si hay al menos un diagnóstico de fase activo. */
        [[nodiscard]] bool hasPhaseDiagnostics() const noexcept;

        /** Helper: setea ambos tipos de diagnóstico y dispara una sola notificación. */
        void setAllDiagnostics(const std::vector<BandDiagnostic>& bandDiags,
                               const std::vector<PhaseDiagnostic>& phaseDiags);

    private:
        std::vector<BandDiagnostic> diagnostics_;
        std::vector<PhaseDiagnostic> phaseDiagnostics_;
        mutable std::mutex mutex_;
    };

} // namespace mixcoach

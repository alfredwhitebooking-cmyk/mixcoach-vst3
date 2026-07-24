#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  DecayGraphComponent — Mini visualización de decay de reverb
    //
    //  Muestra en un área compacta (160×50):
    //    • Línea base (dry signal)
    //    • Línea de pre-delay vertical punteada
    //    • Curva de decay exponencial
    //    • Anotación de high-cut (si aplica)
    //    • Porcentaje de mezcla indicado
    //
    //  Uso:
    //    DecayGraphComponent graph;
    //    graph.setReverbParams(40.0f, 1.8f, 7000.0f, 18.0f);  // preDelayMs, decaySec, highCutHz, mixPct
    //    graph.setGroupColour(MixCoachTheme::accent());  // Color del grupo
    // ═══════════════════════════════════════════════════════════════════════════
    class DecayGraphComponent : public juce::Component
    {
    public:
        DecayGraphComponent();
        ~DecayGraphComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Configura los parámetros de reverb a visualizar.
            @param preDelayMs  Pre-delay en milisegundos
            @param decaySec    Tiempo de decay en segundos
            @param highCutHz   Frecuencia de high-cut (0 = sin high-cut)
            @param mixPct      Porcentaje de mezcla (0-100) */
        void setReverbParams(float preDelayMs, float decaySec,
                             float highCutHz, float mixPct) noexcept;

        /** Configura el color del grupo (para la curva). */
        void setGroupColour(juce::Colour colour) noexcept { groupColour_ = colour; }

        /** Altura preferida para layout. */
        static constexpr int kPreferredHeight = 50;
        static constexpr int kPreferredWidth = 180;

    private:
        float preDelayMs_ = 40.0f;
        float decaySec_ = 1.8f;
        float highCutHz_ = 7000.0f;
        float mixPct_ = 18.0f;
        juce::Colour groupColour_ = MixCoachTheme::accent();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecayGraphComponent)
    };

} // namespace mixcoach

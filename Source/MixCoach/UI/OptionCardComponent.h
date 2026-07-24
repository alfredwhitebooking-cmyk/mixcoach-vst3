#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../engine/PluginSuggestion.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  OptionCardData — Datos para renderizar una tarjeta de opción de plugin
    // ═══════════════════════════════════════════════════════════════════════════
    struct OptionCardData
    {
        PluginTier tier = PluginTier::Native;
        juce::String pluginName;
        juce::String actionText;    // Parámetros visibles (ej: "60Hz, Q=2, -3dB")
        juce::String extraInfo;     // Desarrollador
        juce::String description;   // Descripción corta

        [[nodiscard]] bool isValid() const noexcept { return pluginName.isNotEmpty(); }

        /** Retorna el color representativo del tier. */
        [[nodiscard]] juce::Colour getColour() const noexcept
        {
            return OptionCardData::tierColour(tier);
        }

        /** Color para cada tier. */
        static juce::Colour tierColour(PluginTier tier) noexcept
        {
            switch (tier) {
                case PluginTier::Native:  return MixCoachTheme::success();     // Verde
                case PluginTier::Free:    return MixCoachTheme::accentCyan();  // Cian
                case PluginTier::Premium: return MixCoachTheme::accent();      // Púrpura
                case PluginTier::UserHas: return MixCoachTheme::info();        // Azul info
                default:                  return MixCoachTheme::textMuted();
            }
        }

        /** Icono emoji para cada tier. */
        static const char* tierIcon(PluginTier tier) noexcept
        {
            switch (tier) {
                case PluginTier::Native:  return "[COACH]";  // 🎛
                case PluginTier::Free:    return "\xF0\x9F\x9F\xA2";  // 🟢
                case PluginTier::Premium: return "\xE2\xAD\x90";      // ⭐
                case PluginTier::UserHas: return "[BOLT]";      // ⚡
                default:                  return "[QUESTION]";      // ❓
            }
        }

        /** Label en español para cada tier. */
        static const char* tierLabel(PluginTier tier) noexcept
        {
            switch (tier) {
                case PluginTier::Native:  return "Nativo";
                case PluginTier::Free:    return "Gratis";
                case PluginTier::Premium: return "Profesional";
                case PluginTier::UserHas: return "Ya tienes";
                default:                  return "Desconocido";
            }
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  OptionCardComponent — Componente de tarjeta interactiva para plugin
    //
    //  Layout visual (120×76):
    //    ┌──────────────────────┐
    //    │ [🎛 Nativo]    badge │
    //    │ Fruity Parametric    │ ← nombre bold
    //    │ EQ 2                 │
    //    │ 60Hz  -3dB  Q=2      │ ← parámetros small
    //    └──────────────────────┘
    //
    //  Efectos:
    //    • Hover: elevación + sombra 3D (translateY -3px, shadow intensify)
    //    • Click: callback onSelected
    //    • Animación suave usando SmoothValue (via Timer 60fps)
    // ═══════════════════════════════════════════════════════════════════════════
    class OptionCardComponent : public juce::Component,
                                 private juce::Timer
    {
    public:
        OptionCardComponent();
        ~OptionCardComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseEnter(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;

        /** Establece los datos de la tarjeta. */
        void setCardData(const OptionCardData& data);

        /** Retorna los datos actuales. */
        const OptionCardData& getCardData() const noexcept { return data_; }

        /** Marca/desmarca como seleccionada. */
        void setSelected(bool selected);
        bool isSelected() const noexcept { return selected_; }

        /** Callback al hacer clic. */
        std::function<void()> onClick;

        // ═══ Constantes de tamaño ═══════════════════════════════════════════
        static constexpr float kCardWidth  = 118.0f;
        static constexpr float kCardHeight = 74.0f;
        static constexpr float kCornerRadius = 6.0f;
        static constexpr float kHoverLift  = 3.0f;  // px de elevación al hover

        // ═══ Static drawing helper para inline cards ═══════════════════════
        /** Dibuja una tarjeta de opción en el contexto gráfico dado.
            Útil para renderizar dentro de ChatMessagesComponent::paint()
            sin necesidad de un componente hijo. */
        static void drawOptionCard(juce::Graphics& g,
                                    juce::Rectangle<float> bounds,
                                    const OptionCardData& data,
                                    bool hovered = false,
                                    float animLift = 0.0f);

    private:
        OptionCardData data_;
        bool selected_ = false;
        bool hovered_ = false;
        float liftAmount_ = 0.0f;  // 0=normal, 1=full hover lift
        float liftSmooth_ = 0.0f;

        void timerCallback() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OptionCardComponent)
    };

} // namespace mixcoach

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../engine/PluginSuggestion.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginConfirmationPanel — Panel flotante para confirmación manual de
    //  plugins insertados en cada pista.
    //
    //  Incremento 3d: Muestra una lista de pistas activas, cada una con un
    //  ComboBox de plugins detectados por PluginScanner. El usuario selecciona
    //  qué plugin (si alguno) está insertado en cada pista y confirma.
    //
    //  Layout:
    //    ┌───────────────────────────────────────┐
    //    │  🎛 Insertos por Pista          [✕]   │  ← Header + close
    //    ├───────────────────────────────────────┤
    //    │  🥁 Kick     [Fruity Balance    ▼]   │  ← Track row with ComboBox
    //    │  🔊 Snare    [Fruity Compressor ▼]   │
    //    │  🎸 Guitar   [Ninguno           ▼]   │
    //    │  ───                                │  ← Separator
    //    │       [✅ Confirmar insertos]        │  ← Confirm button
    //    └───────────────────────────────────────┘
    // ═══════════════════════════════════════════════════════════════════════════
    class PluginConfirmationPanel : public juce::Component
    {
    public:
        PluginConfirmationPanel();
        ~PluginConfirmationPanel() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Puebla el panel con datos de pistas activas + plugins detectados.
            @param slotNames  Vector de (slotIndex, trackName, roleEmoji) por pista activa
            @param pluginNames  Lista de nombres de plugins detectados (de PluginScanner) */
        void populate(const std::vector<std::tuple<int, juce::String, juce::String>>& slotData,
                      const std::vector<juce::String>& pluginNames);

        /** Retorna el mapa de slotIndex → pluginName seleccionado por el usuario. */
        [[nodiscard]] std::unordered_map<int, juce::String> getConfirmedInserts() const;

        /** Callback cuando el usuario hace clic en Confirmar. */
        std::function<void(const std::unordered_map<int, juce::String>&)> onConfirm;

        /** Callback cuando el usuario cierra el panel sin confirmar. */
        std::function<void()> onDismiss;

    private:
        // ─── Una fila de track en el panel ────────────────────────────────
        struct TrackRow {
            int slotIndex = -1;
            juce::Label trackLabel;       // Nombre + emoji del track
            juce::ComboBox pluginCombo;   // Selector de plugin
            juce::Rectangle<int> bounds;  // Layout bounds (actualizado en resized)
        };

        std::vector<std::unique_ptr<TrackRow>> rows_;
        juce::TextButton confirmButton_;
        juce::Label titleLabel_;
        juce::Label closeButton_;
        bool closeHovered_ = false;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginConfirmationPanel)
    };

} // namespace mixcoach

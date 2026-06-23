#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Constants.h"
#include "../../Common/types/Types.h"
#include "MixCoachTheme.h"
#include <functional>

namespace mixcoach {

    // ─── Buses Virtuales — Clickable bus selector ────────────────────────────────
    class VirtualBusesComponent : public juce::Component
    {
    public:
        VirtualBusesComponent();
        ~VirtualBusesComponent() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;

        /** Callback cuando el usuario selecciona un bus. */
        std::function<void(BusType bus)> onBusSelected;

        /** Resalta visualmente el bus seleccionado. */
        void setSelectedBus(BusType bus) noexcept
        {
            selectedBus_ = bus;
            repaint();
        }

        /** Actualiza el level y trackCount de un bus. */
        void setBusLevel(BusType bus, float levelDb, int trackCount);

    private:
        juce::Label titleLabel_;
        juce::Label infoLabel_;
        BusType selectedBus_ = BusType::None;

        struct BusInfo
        {
            BusType type = BusType::None;
            juce::String name;
            juce::Colour colour;
            float level    = -60.0f;
            int trackCount = 0;
        };

        std::vector<BusInfo> buses_;

        /** Retorna el índice del bus en las coordenadas dadas, o -1. */
        int hitTestBus(juce::Point<int> pos) const;
    };

} // namespace mixcoach

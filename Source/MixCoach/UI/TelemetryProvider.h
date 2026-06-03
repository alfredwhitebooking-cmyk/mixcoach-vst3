#pragma once
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  TelemetryProvider — Capa de selección de fuente de telemetría
//
//  Permite que los analizadores (meter, spectrograph, phase scope, VU meters)
//  reciban datos de:
//    - UN track específico (Mode::Single)
//    - COMPUESTOS de TODOS los tracks activos (Mode::Master)
//    - COMPUESTOS de UN BUS específico (Mode::Bus)
//  sin cambiar NADA en los componentes analizadores.
//
//  Uso:
//    provider.setRegistry(&registry);
//    provider.setMode(Mode::Master);         // ← modo "ALL"
//    provider.selectBus(BusType::Drums);     // ← modo "Bus: Drums"
//    provider.selectSlot(3);                 // ← modo "Single: slot 3"
//    auto latest = provider.getLatest();     // ← siempre funciona
// ═══════════════════════════════════════════════════════════════════════════
class TelemetryProvider {
public:
    enum class Mode {
        Single,  // Muestra UN track específico
        Bus,     // Muestra COMPOSITE de tracks en un bus específico
        Master   // Muestra COMPOSITE de TODOS los tracks activos
    };

    void setRegistry(SlotRegistry* reg) noexcept { registry_ = reg; }
    
    void setMode(Mode mode) noexcept { mode_ = mode; }
    [[nodiscard]] Mode getMode() const noexcept { return mode_; }
    [[nodiscard]] bool isMaster() const noexcept { return mode_ == Mode::Master; }
    [[nodiscard]] bool isBus() const noexcept { return mode_ == Mode::Bus; }
    [[nodiscard]] bool isSingle() const noexcept { return mode_ == Mode::Single; }

    /** Cambia a Single mode y selecciona un slot específico. */
    void selectSlot(int slotIndex) noexcept {
        selectedSlot_ = slotIndex;
        mode_ = Mode::Single;
    }
    [[nodiscard]] int getSelectedSlot() const noexcept { return selectedSlot_; }

    /** Cambia a Bus mode y selecciona un bus específico. */
    void selectBus(BusType bus) noexcept {
        selectedBus_ = bus;
        mode_ = Mode::Bus;
    }
    [[nodiscard]] BusType getSelectedBus() const noexcept { return selectedBus_; }
    [[nodiscard]] juce::String getSelectedBusName() const noexcept {
        if (selectedBus_ >= BusType::Drums && selectedBus_ <= BusType::FX)
            return busNames[static_cast<int>(selectedBus_)];
        return "BUS";
    }
    [[nodiscard]] juce::Colour getSelectedBusColour() const noexcept {
        if (selectedBus_ >= BusType::Drums && selectedBus_ <= BusType::FX)
            return getBusColour(static_cast<int>(selectedBus_));
        return juce::Colours::grey;
    }

    /** Retorna el TrackTelemetry que corresponda según el modo actual.
        En Single: devuelve latest() del slot seleccionado.
        En Bus: devuelve composite de los tracks en el bus seleccionado.
        En Master: devuelve composite de todos los slots activos. */
    TrackTelemetry getLatest();

private:
    SlotRegistry* registry_ = nullptr;
    Mode mode_ = Mode::Single;
    int selectedSlot_ = -1;
    BusType selectedBus_ = BusType::Drums;

    /** Construye un TrackTelemetry compuesto de todos los slots activos.
        Usa MAX para spectrum, loudest track para crest/RMS/samples. */
    TrackTelemetry buildMaster();

    /** Construye un TrackTelemetry compuesto de los tracks en un bus.
        Misma estrategia que buildMaster(), pero filtrado por bus. */
    TrackTelemetry buildBus(BusType bus);
};

} // namespace mixcoach

#pragma once
#include <juce_graphics/juce_graphics.h>
#include "../../Common/types/Types.h"

namespace mixcoach {

    // ─── Tipo de instrumento/pista (el "alma" del Messenger) ───────────────────
    // Cada Messenger se identifica con un tipo. Esto define su color por defecto
    // y el ruteo sugerido. El usuario puede sobrescribir ambos.
    enum class TrackType : int
    {
        None = -1,
        // Bateria (Drum Bus)
        Kick          = 0,
        Snare         = 1,
        HiHat         = 2,
        Tom           = 3,
        Percussion    = 4,
        Overheads     = 5,
        Room          = 6,
        ReggaetonKick = 23,
        // Bajo (Bass Bus)
        BassDI  = 7,
        BassMic = 8,
        Bass808 = 9,
        Sub     = 10,
        // Melodia (Melody Bus)
        Piano     = 11,
        Guitar    = 12,
        SynthLead = 13,
        SynthPad  = 14,
        Strings   = 15,
        // Voz (Vocal Bus)
        LeadVocal   = 16,
        DoubleVocal = 17,
        Adlibs      = 18,
        Chorus      = 19,
        // FX (FX Bus)
        Risers   = 20,
        Impacts  = 21,
        Ambience = 22
    };

    inline constexpr int kNumTrackTypes = 24;

    // ─── Tabla de mapeo: Tipo → Color → Ruteo Sugerido ─────────────────────────
    struct TrackTypeInfo
    {
        TrackType type;
        const char* name;     // Nombre para mostrar en ComboBox
        uint32_t colourARGB;  // Color sugerido
        BusType suggestedBus; // Ruteo sugerido
    };

    inline constexpr TrackTypeInfo kTrackTypeTable[] = {
        // Bateria → Drum Bus
        {TrackType::Kick, "Kick", 0xFFEF4444, BusType::Drums},
        {TrackType::Snare, "Snare", 0xFFEF4444, BusType::Drums},
        {TrackType::HiHat, "HiHat", 0xFFEF4444, BusType::Drums},
        {TrackType::Tom, "Tom", 0xFFEF4444, BusType::Drums},
        {TrackType::Percussion, "Percusion", 0xFFF97316, BusType::Drums},
        {TrackType::Overheads, "Overheads", 0xFFEF4444, BusType::Drums},
        {TrackType::Room, "Room", 0xFFEF4444, BusType::Drums},
        // Reggaeton
        {TrackType::ReggaetonKick, "Reggaeton Kick", 0xFFF97316, BusType::Drums},
        // Bajo → Bass Bus
        {TrackType::BassDI, "Bajo DI", 0xFF3B82F6, BusType::Bass},
        {TrackType::BassMic, "Bajo Mic", 0xFF3B82F6, BusType::Bass},
        {TrackType::Bass808, "808", 0xFF3B82F6, BusType::Bass},
        {TrackType::Sub, "Sub", 0xFF3B82F6, BusType::Bass},
        // Melodia → Melody Bus
        {TrackType::Piano, "Piano", 0xFF22C55E, BusType::Melody},
        {TrackType::Guitar, "Guitarra", 0xFF22C55E, BusType::Melody},
        {TrackType::SynthLead, "Synth Lead", 0xFF22C55E, BusType::Melody},
        {TrackType::SynthPad, "Synth Pad", 0xFF22C55E, BusType::Melody},
        {TrackType::Strings, "Cuerdas", 0xFF22C55E, BusType::Melody},
        // Voz → Vocal Bus
        {TrackType::LeadVocal, "Voz Principal", 0xFFEAB308, BusType::Vocals},
        {TrackType::DoubleVocal, "Voz Doble", 0xFFEAB308, BusType::Vocals},
        {TrackType::Adlibs, "Adlibs", 0xFFEAB308, BusType::Vocals},
        {TrackType::Chorus, "Coros", 0xFFEAB308, BusType::Vocals},
        // FX → FX Bus
        {TrackType::Risers, "Risers", 0xFFA78BFA, BusType::FX},
        {TrackType::Impacts, "Impacts", 0xFFA78BFA, BusType::FX},
        {TrackType::Ambience, "Ambientes", 0xFFA78BFA, BusType::FX},
    };

    // ─── Helpers inline ─────────────────────────────────────────────────────────

    // Obtener info de un TrackType por su valor enum
    inline const TrackTypeInfo& getTrackTypeInfo(TrackType type)
    {
        static const TrackTypeInfo defaultInfo = {TrackType::None, "—", 0xFF888888, BusType::None};
        for (auto& info : kTrackTypeTable) {
            if (info.type == type) return info;
        }
        return defaultInfo;
    }

    // Obtener el índice de un TrackType en la tabla (para ComboBox, V2: por tabla)
    // Usa búsqueda en kTrackTypeTable para desacoplar el orden visual del ComboBox
    // de los valores numéricos del enum. Permite insertar nuevos tipos en cualquier
    // posición sin romper el mapeo.
    inline int trackTypeToComboIndex(TrackType type)
    {
        if (type == TrackType::None) return 0;
        for (int i = 0; i < kNumTrackTypes; ++i)
            if (kTrackTypeTable[i].type == type) return i + 1;
        return 0;
    }

    // Obtener TrackType desde un índice de ComboBox (V2: por tabla)
    inline TrackType comboIndexToTrackType(int comboIndex)
    {
        if (comboIndex <= 0 || comboIndex > kNumTrackTypes) return TrackType::None;
        return kTrackTypeTable[comboIndex - 1].type;
    }

    // Obtener el nombre del tipo para mostrar en la UI
    inline const char* getTrackTypeName(TrackType type)
    {
        return getTrackTypeInfo(type).name;
    }

    // Obtener el color sugerido para un tipo
    inline juce::Colour getTrackTypeColour(TrackType type)
    {
        return juce::Colour(getTrackTypeInfo(type).colourARGB);
    }

    // Obtener el bus sugerido para un tipo
    inline BusType getTrackTypeBus(TrackType type)
    {
        return getTrackTypeInfo(type).suggestedBus;
    }

    // ═══ IDENTITY LAYER V7: Mapeo TrackType → TrackRole ───────────────────────
    // Declaración adelantada: el mapeo completo está en CoachEngine.cpp
    // donde TrackRole.h ya está incluido con sus valores enum completos.
    // Esto evita dependencias circulares y casts con números mágicos.

    /** Forward-declara el mapeo TrackType → TrackRole. La implementación
        completa está en CoachEngine.cpp con los valores reales del enum. */
    // (Implementado en CoachEngine.cpp)

} // namespace mixcoach

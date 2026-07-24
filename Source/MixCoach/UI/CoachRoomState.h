#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachRoomState — Estados de Progressive Disclosure para la UI (UX 2.0)
    //
    //  15 fases que siguen el flujo exacto de SESSION_FLOW.md.
    //  El Coach dirige la progresión — el usuario nunca navega, el Coach revela.
    //
    //  FASES 0-5 (Setup — solo Chat, TabBar oculta):
    //    STATE 0  (Welcome):         Solo chat + avatar. Nada más existe.
    //    STATE 1  (Intention):       Chat + botones [Mezclar] [Masterizar]
    //    STATE 2  (Genre):           Chat + chips de género
    //    STATE 3  (Reference):       Chat + DropZone referencia inline
    //    STATE 4  (Messengers):      Chat + referencia + track list inline
    //    STATE 5  (Mapping):         Chat + referencia + messengers + mixmap
    //                                (Session se desbloquea en DeepAnalysis, no aquí)
    //
    //  FASES 6-11 (Coaching — TabBar visible, FullUI):
    //    STATE 6  (GainStaging):     TabBar visible. Coach guía gain staging.
    //    STATE 7  (Balance):         Balance de faders
    //    STATE 8  (EQ):              Corrección tonal (Tools ▶ se desbloquea)
    //    STATE 9  (Compression):     Dinámica
    //    STATE 10 (Space):           Profundidad y ambiente
    //    STATE 11 (Refinement):      Refinamiento artístico (MixScore ≥ 70)
    //    STATE 12 (MasterCheck):     Revisión final vs referencia
    //
    //  FASE 13 (Report):
    //    STATE 13 (Report):          Overlay que reemplaza el chat
    // ═══════════════════════════════════════════════════════════════════════════
    enum class CoachRoomState : uint8_t
    {
        // ─── Setup (Chat-only, progressive reveal) ─────────────────────────
        Welcome,        // STATE 0:  solo chat + avatar
        Intention,      // STATE 1:  + botones Mix/Master
        Genre,          // STATE 2:  + chips de género
        ReferenceStage, // STATE 3:  + DropZone referencia inline
        MessengerStage, // STATE 4:  + track list inline
        SessionPrep,    // STATE 4.5: + checklist premium (organiza sesión → Messengers → MixMap)
        MixMapStage,    // STATE 5:  + mixmap inline (Session unlock en DeepAnalysis)

        // ─── Coaching (FullUI with TabBar) ─────────────────────────────────
        GainStaging,    // STATE 6:  TabBar visible, coaching activo
        Balance,        // STATE 7:
        EQ,             // STATE 8:  Tools tab unlock
        Compression,    // STATE 9:
        Space,          // STATE 10:
        Automation,     // STATE 11: Automatización y dinámica de secciones (GAP #8)
        Refinement,     // STATE 12: Refinamiento artístico (MixScore >= 70)
        MasterCheck,    // STATE 13: revisión final

        // ─── Report (overlay) ──────────────────────────────────────────────
        Report,         // STATE 14: overlay reemplaza chat

        Count           // Total states
    };

    /** Retorna un label textual para el estado (debugging). */
    inline const char* coachRoomStateLabel(CoachRoomState state) noexcept
    {
        switch (state) {
            case CoachRoomState::Welcome:        return "WELCOME";
            case CoachRoomState::Intention:      return "INTENTION";
            case CoachRoomState::Genre:          return "GENRE";
            case CoachRoomState::ReferenceStage: return "REFERENCE";
            case CoachRoomState::MessengerStage: return "MESSENGER";
            case CoachRoomState::SessionPrep:   return "SESSION_PREP";
            case CoachRoomState::MixMapStage:    return "MIXMAP";
            case CoachRoomState::GainStaging:    return "GAIN_STAGING";
            case CoachRoomState::Balance:        return "BALANCE";
            case CoachRoomState::EQ:             return "EQ";
            case CoachRoomState::Compression:    return "COMPRESSION";
            case CoachRoomState::Space:          return "SPACE";
            case CoachRoomState::Automation:     return "AUTOMATION";
            case CoachRoomState::Refinement:     return "REFINEMENT";
            case CoachRoomState::MasterCheck:    return "MASTER_CHECK";
            case CoachRoomState::Report:         return "REPORT";
            default:                             return "UNKNOWN";
        }
    }

    /** Retorna true si el estado es pre-FullUI (chat-only, setup). */
    inline bool isPreFullUI(CoachRoomState state) noexcept
    {
        return state < CoachRoomState::GainStaging;
    }

    /** Retorna true si el estado es de coaching (FullUI con TabBar). */
    inline bool isCoachingState(CoachRoomState state) noexcept
    {
        return state >= CoachRoomState::GainStaging && state < CoachRoomState::Report;
    }

    /** Retorna el progreso base 0.0-1.0 según el estado. */
    inline float coachRoomStateProgress(CoachRoomState state) noexcept
    {
        int idx = static_cast<int>(state);
        int total = static_cast<int>(CoachRoomState::Count);
        if (idx < 0 || idx >= total) return 0.0f;
        return static_cast<float>(idx) / static_cast<float>(total - 1);
    }

} // namespace mixcoach

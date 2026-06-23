#include "MessengerListComponent.h"
#include "MessengerListRoles.h"
#include "../engine/CoachEngine.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

// ═══ Role colour helper ═══════════════════════════════════════════════════
juce::Colour roleColourForCategory(RoleCategory cat) noexcept
{
    return MixCoachTheme::roleColourForCategory(static_cast<int>(cat));
}

// ═══ Role menu ID mapping ═════════════════════════════════════════════════
TrackRole roleFromMenuId(int menuId)
{
    if (menuId <= 0 || menuId == 1)
        return TrackRole::Unknown;

    static const std::vector<std::vector<TrackRole>> kCatRoles = {
        { TrackRole::Kick, TrackRole::Kick808, TrackRole::Snare, TrackRole::SnareTrap,
          TrackRole::HiHat, TrackRole::HiHatOpen, TrackRole::Ride, TrackRole::Crash,
          TrackRole::Tom, TrackRole::TomFloor, TrackRole::Percussion, TrackRole::Clap,
          TrackRole::DrumBus },
        { TrackRole::BassSub, TrackRole::Bass808, TrackRole::BassPick, TrackRole::BassFinger,
          TrackRole::BassSynth, TrackRole::BassBus },
        { TrackRole::GuitarAcoustic, TrackRole::GuitarElectric, TrackRole::GuitarRhythm,
          TrackRole::GuitarLead, TrackRole::GuitarBus },
        { TrackRole::SynthLead, TrackRole::SynthPad, TrackRole::SynthPluck,
          TrackRole::KeysPiano, TrackRole::KeysElectric, TrackRole::KeysOrgan,
          TrackRole::KeysBus, TrackRole::MelodyBus },
        { TrackRole::VozPrincipal, TrackRole::VozFondo, TrackRole::VozDouble, TrackRole::Adlibs,
          TrackRole::VozBus },
        { TrackRole::FxRiser, TrackRole::FxImpact, TrackRole::FxAmbience, TrackRole::FxNoise },
        { TrackRole::Strings, TrackRole::Brass, TrackRole::Winds }
    };

    int catIdx = (menuId / 100) - 1;
    int itemIdx = menuId % 100;

    if (catIdx >= 0 && catIdx < (int)kCatRoles.size()
        && itemIdx >= 0 && itemIdx < (int)kCatRoles[catIdx].size())
    {
        return kCatRoles[catIdx][itemIdx];
    }

    return TrackRole::Unknown;
}

// ═══ Build role popup menu ════════════════════════════════════════════════
juce::PopupMenu buildRoleMenuStatic()
{
    struct CatDef {
        RoleCategory cat;
        const char* name;
        std::vector<std::pair<TrackRole, const char*>> items;
    };

    std::vector<CatDef> cats = {
        { RoleCategory::Drums, "Drums", {
            { TrackRole::Kick, "Kick" }, { TrackRole::Kick808, "Kick 808" },
            { TrackRole::Snare, "Snare" }, { TrackRole::SnareTrap, "Snare Trap" },
            { TrackRole::HiHat, "Hi-Hat" }, { TrackRole::HiHatOpen, "Hi-Hat Open" },
            { TrackRole::Ride, "Ride" }, { TrackRole::Crash, "Crash" },
            { TrackRole::Tom, "Tom" }, { TrackRole::TomFloor, "Tom Floor" },
            { TrackRole::Percussion, "Percussion" }, { TrackRole::Clap, "Clap" },
            { TrackRole::DrumBus, "Drum Bus" }
        }},
        { RoleCategory::Bass, "Bass", {
            { TrackRole::BassSub, "Sub Bass" }, { TrackRole::Bass808, "808 Bass" },
            { TrackRole::BassPick, "Bass (Pick)" }, { TrackRole::BassFinger, "Bass (Finger)" },
            { TrackRole::BassSynth, "Synth Bass" }, { TrackRole::BassBus, "Bass Bus" }
        }},
        { RoleCategory::Guitars, "Guitars", {
            { TrackRole::GuitarAcoustic, "Acoustic Guitar" },
            { TrackRole::GuitarElectric, "Electric Guitar" },
            { TrackRole::GuitarRhythm, "Rhythm Guitar" },
            { TrackRole::GuitarLead, "Lead Guitar" }, { TrackRole::GuitarBus, "Guitar Bus" }
        }},
        { RoleCategory::Keys, "Keys / Synths", {
            { TrackRole::SynthLead, "Synth Lead" }, { TrackRole::SynthPad, "Synth Pad" },
            { TrackRole::SynthPluck, "Synth Pluck" }, { TrackRole::KeysPiano, "Piano" },
            { TrackRole::KeysElectric, "Electric Piano" }, { TrackRole::KeysOrgan, "Organ" },
            { TrackRole::KeysBus, "Keys Bus" }, { TrackRole::MelodyBus, "Melody Bus" }
        }},
        { RoleCategory::Vocals, "Vocals", {
            { TrackRole::VozPrincipal, "Lead Vocal" }, { TrackRole::VozFondo, "Backing Vocal" },
            { TrackRole::VozDouble, "Vocal Double" }, { TrackRole::Adlibs, "Ad-libs" },
            { TrackRole::VozBus, "Vocal Bus" }
        }},
        { RoleCategory::FX, "FX / Ambience", {
            { TrackRole::FxRiser, "Riser" }, { TrackRole::FxImpact, "Impact" },
            { TrackRole::FxAmbience, "Ambience" }, { TrackRole::FxNoise, "Noise" }
        }},
        { RoleCategory::Melody, "Melodic", {
            { TrackRole::Strings, "Strings" }, { TrackRole::Brass, "Brass" },
            { TrackRole::Winds, "Winds" }
        }}
    };

    juce::PopupMenu menu;
    menu.addItem(1, "No role (Unknown)");
    menu.addSeparator();

    int catIdx = 0;
    for (auto& cat : cats) {
        juce::PopupMenu sub;
        for (size_t i = 0; i < cat.items.size(); ++i) {
            int id = (catIdx + 1) * 100 + static_cast<int>(i);
            sub.addItem(id, cat.items[i].second);
        }
        menu.addSubMenu(cat.name, sub);
        ++catIdx;
    }

    return menu;
}

// ═══ syncRolesFromCoach — Member function: sync from CoachEngine ═════════
void MessengerListComponent::syncRolesFromCoach()
{
    if (coachEngine_ == nullptr)
        return;

    for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
        messengers_[i].trackRole = coachEngine_->getTrackRole(i);
        // Sync signal order pending state
        messengers_[i].signalOrderPending = coachEngine_->isSignalOrderInference(i);

        // Sprint 1: sync generalised inferred-pending state (name/espectral).
        // Covers all 3 inference origins: signal-order, name, spectral.
        messengers_[i].roleInferredPending = coachEngine_->isInferredRole(i);

        // Compute role confidence (Sprint 1: unified logic):
        // 0.00 — Unknown / Master
        // 0.55 — inferred (any origin) and NOT confirmed
        // 0.85 — confirmed (manual, signal-order confirmed, or Sprint 1 confirmRole)
        if (messengers_[i].trackRole == TrackRole::Unknown
            || messengers_[i].trackRole == TrackRole::Master)
            messengers_[i].roleConfidence = 0.0f;
        else if (coachEngine_->isRoleConfirmed(i))
            messengers_[i].roleConfidence = 0.85f;
        else
            messengers_[i].roleConfidence = 0.55f;
    }
}

} // namespace mixcoach

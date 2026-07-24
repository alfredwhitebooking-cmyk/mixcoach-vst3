#include "CoachEngine.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  GenreReverbTarget — Metadatos de reverb por género musical
    //
    //  Define los parámetros de reverb recomendados para cada género,
    //  usados por analyzeSpaceReal() para generar sugerencias específicas.
    // ═══════════════════════════════════════════════════════════════════════════
    struct GenreReverbTarget
    {
        float preDelayMs;    // Pre-delay en ms
        float decaySec;      // Tiempo de decay en segundos
        float highCutHz;     // Frecuencia de high-cut
        float mixPct;        // Porcentaje de mezcla (wet)
        const char* preset;  // Preset recomendado (vacío = genérico)
        const char* algorithm; // Algoritmo preferido ("Hall", "Room", "Plate", etc.)
    };

    /** Retorna el perfil de reverb para un género.
        Si no se encuentra el género, retorna perfil genérico balanceado. */
    static GenreReverbTarget getGenreReverbProfile(const juce::String& genre) noexcept
    {
        auto g = genre.trim().toLowerCase();

        if (g == "afrobeat" || g == "afrobeats" || g == "world")
            return { 35.0f, 1.5f, 7500.0f, 15.0f, "Plate", "Plate" };
        if (g == "reggaeton" || g == "reggaeton/latin" || g == "latin" || g == "dembow")
            return { 30.0f, 1.2f, 8000.0f, 12.0f, "Gate Reverb", "Gate" };
        if (g == "trap")
            return { 40.0f, 2.0f, 6000.0f, 18.0f, "Hall", "Hall" };
        if (g == "hiphop" || g == "hip-hop" || g == "rap")
            return { 35.0f, 1.6f, 7000.0f, 15.0f, "Room", "Room" };
        if (g == "pop")
            return { 40.0f, 1.8f, 8500.0f, 18.0f, "Hall", "Hall" };
        if (g == "rock")
            return { 25.0f, 1.6f, 6500.0f, 20.0f, "Room", "Room" };
        if (g == "edm" || g == "electronic" || g == "house" || g == "techno"
            || g == "trance" || g == "dubstep")
            return { 45.0f, 2.5f, 5000.0f, 22.0f, "Hall", "Hall" };
        if (g == "jazz")
            return { 50.0f, 2.2f, 5500.0f, 25.0f, "Hall", "Hall" };
        if (g == "rnb" || g == "r&b" || g == "rb" || g == "soul")
            return { 45.0f, 2.0f, 7500.0f, 20.0f, "Plate", "Plate" };
        return { 40.0f, 1.8f, 7000.0f, 18.0f, "", "" };
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeSpaceReal — Detecta mezcla "seca" y genera sugerencias de reverb
    //
    //  Busca tracks con correlacion alta (>0.92) y senal presente,
    //  y genera sugerencias con parametros especificos por genero.
    //
    //  Se llama desde periodicAnalysis() durante fase Espacio o desde
    //  comandos de usuario (/reverb, /espacio).
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::analyzeSpaceReal()
    {
        auto now = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        if (registry.activeCount() < 2) return;

        // Obtener perfil de reverb para el genero actual
        GenreReverbTarget reverbProfile = getGenreReverbProfile(setupGenre_);

        // Buscar tracks candidatas para reverb
        // Criterios de "mezcla seca":
        //   - Correlacion > 0.92 (senal casi mono, sin procesamiento estereo)
        //   - Senal presente (rms > -40 dB)
        //   - No es percusion (drums suelen ir secas a proposito)

        struct DryTrack
        {
            int slotIndex;
            juce::String trackName;
            float correlation;
            float rmsLeft;
            TrackRole role;
        };

        std::vector<DryTrack> dryCandidates;

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive_ && !info.soloed)) return;

            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;

            float corr = telem.correlation;
            float rms = (telem.rmsLeft + telem.rmsRight) * 0.5f;

            if (rms < -50.0f) return;

            TrackRole role = trackRoles_[info.slotIndex];

            // Descartar percusion (drums) — suelen ir secas a proposito
            auto category = getRoleCategory(role);
            if (category == RoleCategory::Drums) return;

            // Candidata si tiene correlacion alta y senal presente
            if (corr > 0.92f && rms > -35.0f) {
                juce::String name = juce::String(info.trackName).trim();
                if (name.isEmpty()) name = "Pista " + juce::String(info.slotIndex + 1);
                dryCandidates.push_back({info.slotIndex, name, corr, rms, role});
            }
        });

        if (dryCandidates.empty()) return;

        // Cooldown
        if (now - lastSpaceWarningUs_ < kWarningCooldownUs) return;
        lastSpaceWarningUs_ = now;

        // Ordenar por correlacion mas alta primero
        std::sort(dryCandidates.begin(), dryCandidates.end(),
                  [](const DryTrack& a, const DryTrack& b) {
                      return a.correlation > b.correlation;
                  });
        
        // Construir datos para la tarjeta inline de reverb
        int maxShow = juce::jmin(3, (int)dryCandidates.size());

        std::vector<juce::String> trackNames;
        std::vector<juce::String> trackRoles;
        for (int i = 0; i < maxShow; ++i) {
            auto& dt = dryCandidates[i];
            trackNames.push_back(dt.trackName);
            juce::String roleName = dt.role != TrackRole::Unknown
                                        ? juce::String(getRoleName(dt.role))
                                        : "";
            trackRoles.push_back(roleName);
        }

        // Disparar callback a la UI para que muestre la tarjeta inline con curva
        if (reverbSuggestedCb_) {
            reverbSuggestedCb_(reverbProfile.preDelayMs,
                               reverbProfile.decaySec,
                               reverbProfile.highCutHz,
                               reverbProfile.mixPct,
                               setupGenre_,
                               juce::String(reverbProfile.algorithm),
                               trackNames,
                               trackRoles);
        } else {
            // Fallback: mensaje de texto si no hay callback cableado
            juce::String msg;
            msg += "\xF0\x9F\x8C\x8A **Quieres anadir espacio a tu mezcla?**\n";
            msg += "Pistas secas detectadas. Prueba reverb con ";
            msg += "PreDelay=" + juce::String((int)reverbProfile.preDelayMs) + "ms, ";
            msg += "Decay=" + juce::String(reverbProfile.decaySec, 1) + "s, ";
            msg += "Mix=" + juce::String((int)reverbProfile.mixPct) + "%";
            respondWithPremium(msg, MentorMessage::Type::Tip);
        }
    }

} // namespace mixcoach

#pragma once
#include <juce_core/juce_core.h>
#include "CoachEngine.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ConfidenceScore — Evalúa la confianza del sistema en los datos disponibles
    //
    //  Cada sub-score mide un aspecto de completitud y calidad de datos.
    //  El overall es un promedio ponderado que indica cuánto puede confiar
    //  el LLM (y el usuario) en las recomendaciones del coach.
    //
    //  Uso:
    //    auto cs = ConfidenceScore::compute(coachEngine, sharedData);
    //    respondWith(cs.toTextSummary());
    //
    //  Sub-scores (0-100):
    //    trackIdentification:  % de pistas activas con TrackRole != Unknown
    //    roleConfirmation:     % de roles identificados que están confirmados por el usuario
    //    signalPresence:       % de pistas activas con señal audible (> -60dB)
    //    referenceLoaded:      100 si hay referencia cargada, 0 si no
    //    genreSelected:        100 si hay género configurado, 0 si no
    //    engineerNamed:        100 si hay nombre de ingeniero, 0 si no
    //    correctionsMade:      % basado en la proporción de correcciones aplicadas vs totales
    //    sessionEngagement:    Basado en duración de sesión y número de cambios realizados
    //
    //  Pesos para overall:
    //    trackIdentification:  25%
    //    roleConfirmation:     20%
    //    signalPresence:       15%
    //    referenceLoaded:      15%
    //    genreSelected:        10%
    //    engineerNamed:         5%
    //    correctionsMade:       5%
    //    sessionEngagement:     5%
    // ═══════════════════════════════════════════════════════════════════════════
    struct ConfidenceScore
    {
        // ═══ Sub-scores 0-100 ═════════════════════════════════════════════════
        int trackIdentification = 0;
        int roleConfirmation    = 0;
        int signalPresence      = 0;
        int referenceLoaded     = 0;
        int genreSelected       = 0;
        int engineerNamed       = 0;
        int correctionsMade     = 0;
        int sessionEngagement   = 0;

        // ═══ Overall 0-100 ════════════════════════════════════════════════════
        int overall = 0;

        // ═══ Label descriptivo ═════════════════════════════════════════════════
        juce::String statusLabel; // "Alta", "Media", "Baja", "Minima"

        // ═══ Breakdown de motivos (para mostrar al usuario) ═══════════════════
        juce::String reasons; // Texto con los factores que bajan la confianza

        // ═══ Compute ═══════════════════════════════════════════════════════════
        /** Computa el ConfidenceScore a partir de los datos actuales del motor.
            @param coach    CoachEngine con trackRoles, correction history, etc.
            @param shared   SharedData con slot registry + track audio results
            @param sessionDurationUs  Duración de la sesión en microsegundos (0 si no disponible) */
        static ConfidenceScore compute(const CoachEngine& coach,
                                       SharedData& shared,
                                       int64_t sessionDurationUs = 0) noexcept
        {
            ConfidenceScore cs;
            auto& registry = shared.getSlotRegistry();
            int active     = registry.activeCount();

            if (active == 0) {
                cs.statusLabel = "Minima";
                cs.reasons     = "No active tracks detected.";
                return cs;
            }

            // ─── 1. Track Identification ─────────────────────────────────
            int identified = 0;
            int confirmed  = 0;
            int withSignal = 0;
            int namedTracks = 0;
            int badNames    = 0;

            const auto& trackRoles = coach.getTrackRoles();

            registry.forEachActive([&](const SlotInfo& info) {
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

                // Identification
                TrackRole role = trackRoles[idx];
                if (role != TrackRole::Unknown && role != TrackRole::Master)
                    identified++;

                // Confirmation
                if (role != TrackRole::Unknown && role != TrackRole::Master && coach.isRoleConfirmed(idx))
                    confirmed++;

                // Signal presence
                auto result = shared.getTrackAudioResult(idx);
                float peak  = juce::jmax(result.peakLeft, result.peakRight);
                if (peak > -60.0f) withSignal++;

                // Naming quality
                juce::String name = juce::String(info.trackName).trim();
                if (name.isNotEmpty()) {
                    // Generic names like "Track 1", "Audio 1", "Slot 5" lower confidence
                    bool isGeneric = name.startsWithIgnoreCase("Track")
                                     || name.startsWithIgnoreCase("Audio")
                                     || name.startsWithIgnoreCase("Slot")
                                     || name.startsWithIgnoreCase("Pista")
                                     || name.startsWithIgnoreCase("Channel")
                                     || name.containsOnly("0123456789 -_");
                    if (isGeneric) badNames++;
                    else
                        namedTracks++;
                }
            });

            // Weight naming quality into identification
            cs.trackIdentification = (active > 0) ? (identified * 100 / active) : 0;
            // Bonus for meaningful names (up to +15)
            if (namedTracks > 0 && active > 0) {
                float nameRatio = (float)namedTracks / active;
                cs.trackIdentification = juce::jmin(100, cs.trackIdentification + (int)(nameRatio * 15.0f));
            }

            // ─── 2. Role Confirmation ────────────────────────────────────
            cs.roleConfirmation = (identified > 0) ? (confirmed * 100 / identified) : 0;

            // ─── 3. Signal Presence ──────────────────────────────────────
            cs.signalPresence = (active > 0) ? (withSignal * 100 / active) : 0;

            // ─── 4. Reference Loaded ─────────────────────────────────────
            cs.referenceLoaded = coach.hasReference() ? 100 : 0;

            // ─── 5. Genre Selected ───────────────────────────────────────
            juce::String genre = coach.getSetupGenre();
            cs.genreSelected = (genre.isNotEmpty() && genre != "Unknown" && genre != "Desconocido") ? 100 : 0;

            // ─── 6. Engineer Named ───────────────────────────────────────
            cs.engineerNamed = coach.hasEngineerName() ? 100 : 0;

            // ─── 7. Corrections Made ─────────────────────────────────────
            const auto& corrHistory = coach.getCorrectionHistory();
            int totalCorrections    = (int)corrHistory.size();
            if (totalCorrections > 0) {
                int applied = 0;
                for (const auto& entry : corrHistory) {
                    if (entry.finalStatus == TrackRecommendation::Status::Applied)
                        applied++;
                }
                // At least 3 corrections for full score, ratio of applied
                int baseScore = juce::jmin(100, totalCorrections * 20); // 5 corrections = 100 base
                float applyRatio = (totalCorrections > 0) ? (float)applied / totalCorrections : 0.0f;
                cs.correctionsMade = (int)(baseScore * 0.5f + applyRatio * 100.0f * 0.5f);
                cs.correctionsMade = juce::jmin(100, cs.correctionsMade);
            }
            else {
                cs.correctionsMade = 0;
            }

            // ─── 8. Session Engagement ───────────────────────────────────
            if (sessionDurationUs > 0) {
                int64_t minutes = sessionDurationUs / (60 * 1000 * 1000);
                if (minutes >= 30) cs.sessionEngagement = 100;
                else if (minutes >= 15)
                    cs.sessionEngagement = 80;
                else if (minutes >= 10)
                    cs.sessionEngagement = 60;
                else if (minutes >= 5)
                    cs.sessionEngagement = 40;
                else if (minutes >= 2)
                    cs.sessionEngagement = 20;
                else
                    cs.sessionEngagement = 10;
            }
            else {
                // Without session duration, use mix history count as proxy
                auto mixHistory = coach.getMixHistory(50);
                int histCount   = (int)mixHistory.size();
                cs.sessionEngagement = juce::jmin(100, histCount * 5); // 20 events = 100
            }

            // ═══ Overall: weighted average ════════════════════════════════
            static constexpr float weights[8] = {
                0.25f, // trackIdentification
                0.20f, // roleConfirmation
                0.15f, // signalPresence
                0.15f, // referenceLoaded
                0.10f, // genreSelected
                0.05f, // engineerNamed
                0.05f, // correctionsMade
                0.05f  // sessionEngagement
            };

            int subScores[8] = {
                cs.trackIdentification,
                cs.roleConfirmation,
                cs.signalPresence,
                cs.referenceLoaded,
                cs.genreSelected,
                cs.engineerNamed,
                cs.correctionsMade,
                cs.sessionEngagement
            };

            float total = 0.0f;
            for (int i = 0; i < 8; ++i) {
                total += subScores[i] * weights[i];
            }
            cs.overall = juce::jlimit(0, 100, (int)(total + 0.5f));

            // ═══ Status label ════════════════════════════════════════════════
            if (cs.overall >= 80) cs.statusLabel = "Alta";
            else if (cs.overall >= 60)
                cs.statusLabel = "Media-Alta";
            else if (cs.overall >= 40)
                cs.statusLabel = "Media";
            else if (cs.overall >= 20)
                cs.statusLabel = "Baja";
            else
                cs.statusLabel = "Minima";

            // ═══ Build reasons breakdown ═══════════════════════════════════════
            juce::StringArray reasons;
            if (cs.trackIdentification < 50)
                reasons.add("Pocas pistas tienen rol asignado (" + juce::String(identified)
                            + "/" + juce::String(active) + ")");
            if (cs.roleConfirmation < 50 && identified > 0)
                reasons.add("Roles no confirmados por el usuario (" + juce::String(confirmed)
                            + "/" + juce::String(identified) + " confirmados)");
            if (cs.signalPresence < 50)
                reasons.add("Varias pistas sin senal detectable");
            if (cs.referenceLoaded == 0)
                reasons.add("Sin referencia cargada");
            if (cs.genreSelected == 0)
                reasons.add("Género no seleccionado");
            if (cs.engineerNamed == 0)
                reasons.add("Nombre de ingeniero no establecido");
            if (cs.correctionsMade < 20)
                reasons.add("Pocas correcciones registradas (necesitas mas interaccion)");
            if (cs.sessionEngagement < 30)
                reasons.add("Sesión recién iniciada, pocos datos de actividad");

            cs.reasons = reasons.joinIntoString("; ");

            return cs;
        }

        /** Retorna un emoji representativo del nivel de confianza. */
        [[nodiscard]] const char* emoji() const noexcept
        {
            if (overall >= 80) return "[DONE]";       // ✅
            if (overall >= 60) return "\xF0\x9F\x9F\xA1";  // 🟡
            if (overall >= 40) return "[WARN]"; // ⚠️
            return "\xE2\x9D\x8C";                           // ❌
        }

        /** Texto formateado para el contexto del LLM. */
        [[nodiscard]] juce::String toTextSummary() const
        {
            juce::String s;
            s += juce::String("[CONFIDENCE SCORE: ") + juce::String(overall) + "/100 - " + statusLabel + "]\n";
            s += juce::String("  ") + juce::String(emoji()) + juce::String(" ");
            if (overall >= 60) {
                s += "Confianza suficiente. ";
            }
            else {
                s += "Confianza limitada. ";
            }
            s += juce::String(reasons) + "\n";
            s += "  Breakdown: Roles=" + juce::String(trackIdentification)
                 + "% Conf=" + juce::String(roleConfirmation)
                 + "% Signal=" + juce::String(signalPresence)
                 + "% Ref=" + juce::String(referenceLoaded)
                 + "% Genre=" + juce::String(genreSelected)
                 + "% Eng=" + juce::String(sessionEngagement) + "%\n";
            return s;
        }

        /** Texto detallado para mostrar en UI (EndOfSessionComponent). */
        [[nodiscard]] juce::String toDetailedString() const
        {
            juce::String s;
            s += juce::String("=== CONFIDENCE SCORE: ") + juce::String(overall) + "/100 - " + statusLabel + " ===\n\n";
            s += juce::String("  ") + juce::String(emoji()) + juce::String("  Reasons: ") + reasons + "\n\n";
            s += "  Track Identification:  " + juce::String(trackIdentification) + "/100\n";
            s += "  Role Confirmation:     " + juce::String(roleConfirmation) + "/100\n";
            s += "  Signal Presence:       " + juce::String(signalPresence) + "/100\n";
            s += "  Reference Loaded:      " + juce::String(referenceLoaded) + "/100\n";
            s += "  Genre Selected:        " + juce::String(genreSelected) + "/100\n";
            s += "  Engineer Named:        " + juce::String(engineerNamed) + "/100\n";
            s += "  Corrections Made:      " + juce::String(correctionsMade) + "/100\n";
            s += "  Session Engagement:    " + juce::String(sessionEngagement) + "/100\n";
            return s;
        }
    };

} // namespace mixcoach

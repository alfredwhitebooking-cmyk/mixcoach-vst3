#include "CoachEngine.h"
#include "TrackRole.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionContext::toChatMessage — Resumen formateado para el chat
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String SessionContext::toChatMessage() const
    {
        if (!hasData()) return "\\xF0\\x9F\\x97\\xBA No hay pistas en la sesi\\xC3\\xB3n a\\xC3\\xBAn.";

        juce::String msg;
        msg =
            "\\xF0\\x9F\\x93\\x8B **RESUMEN DE SESI\\xC3\\x93N**\\n"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\n\\n";

        // ─── Cabecera: modo + género + fase ──────────────────────────────────
        msg += "\\xF0\\x9F\\x8E\\xAF **Modo:** ";
        msg += (coachMode == CoachMode::Mix) ? "Mix" : "Master";
        if (genre.isNotEmpty()) msg += " | **G\\xC3\\xA9nero:** " + genre;
        msg += "\\n";
        msg += "\\xF0\\x9F\\x97\\xBA **Fase:** " + juce::String(phaseNames[static_cast<int>(currentPhase)]);
        msg += " (" + juce::String(static_cast<int>(phaseProgress * 100.0f)) + "%)";
        if (achievementCount > 0) msg += " | \\xF0\\x9F\\x8F\\x86 " + juce::String(achievementCount) + " logros";
        msg += "\\n\\n";

        // ─── Mapa de pistas por categoría ───────────────────────────────────
        msg += "\\xF0\\x9F\\x93\\x8A **PISTAS (" + juce::String(totalTracks) + "):**\\n";
        if (drumTracks > 0) msg += "  \\xF0\\x9F\\xA5\\x81 Drums: " + juce::String(drumTracks) + "\\n";
        if (bassTracks > 0) msg += "  \\xF0\\x9F\\x8E\\xB8 Bass: " + juce::String(bassTracks) + "\\n";
        if (guitarTracks > 0) msg += "  \\xF0\\x9F\\x8E\\xB8 Guitars: " + juce::String(guitarTracks) + "\\n";
        if (keysTracks > 0) msg += "  \\xF0\\x9F\\x8E\\xB9 Keys: " + juce::String(keysTracks) + "\\n";
        if (vocalTracks > 0) msg += "  \\xF0\\x9F\\x8E\\xA4 Vocals: " + juce::String(vocalTracks) + "\\n";
        if (fxTracks > 0) msg += "  \\xF0\\x9F\\x8E\\x9B FX: " + juce::String(fxTracks) + "\\n";
        if (melodyTracks > 0) msg += "  \\xF0\\x9F\\x8E\\xB5 Melodic: " + juce::String(melodyTracks) + "\\n";
        if (unknownTracks > 0) msg += "  \\xE2\\x9D\\x93 Unknown: " + juce::String(unknownTracks) + "\\n";

        // ─── Organización ───────────────────────────────────────────────────
        msg += "\\n\\xF0\\x9F\\x94\\x97 **ORGANIZACI\\xC3\\x93N:**\\n";
        msg += "  Con bus: " + juce::String(bussedTracks) + "/" + juce::String(totalTracks) + "\\n";
        msg += "  Con nombre: " + juce::String(namedTracks) + "/" + juce::String(totalTracks) + "\\n";
        msg += "  Roles asignados: " + juce::String(rolesAssigned) + "/" + juce::String(totalTracks) + "\\n";

        // ─── Gain staging ───────────────────────────────────────────────────
        if (clippingTracks > 0 || lowSignalTracks > 0) {
            msg += "\\n\\xE2\\x9A\\xA0 **GANANCIA:**\\n";
            if (clippingTracks > 0) msg += "  \\xF0\\x9F\\x94\\xB4 Clipping: " + juce::String(clippingTracks) + "\\n";
            if (nearClipTracks > 0) msg += "  \\xF0\\x9F\\x9F\\xA1 Near-clip: " + juce::String(nearClipTracks) + "\\n";
            if (lowSignalTracks > 0)
                msg += "  \\xE2\\x9A\\xAA Se\\xC3\\xB1al baja: " + juce::String(lowSignalTracks) + "\\n";
        }
        if (healthyTracks > 0) msg += "  \\xF0\\x9F\\x9F\\xA2 Saludables: " + juce::String(healthyTracks) + "\\n";

        // ─── Master ─────────────────────────────────────────────────────────
        if (masterPeak > -90.0f) {
            msg += "\\n\\xF0\\x9F\\x93\\x8A **MASTER:**\\n";
            msg += "  Peak: " + juce::String(masterPeak, 1) + " dBFS\\n";
            msg += "  RMS: " + juce::String(masterRMS, 1) + " dBFS\\n";
            msg += "  Crest: " + juce::String(masterCrest, 1) + " dB\\n";
            msg += "  Correlaci\\xC3\\xB3n: " + juce::String(masterCorrelation, 2) + "\\n";
            if (masterLUFS > -80.0f) msg += "  LUFS (ST): " + juce::String(masterLUFS, 1) + "\\n";
        }

        // ─── Referencia ─────────────────────────────────────────────────────
        if (referenceLoaded) {
            msg += "\\n\\xF0\\x9F\\x93\\x9A **REFERENCIA:**\\n";
            if (referenceName.isNotEmpty()) msg += "  " + referenceName + "\\n";
            if (matchScore > 0.0f) msg += "  Match: " + juce::String(static_cast<int>(matchScore * 100.0f)) + "%\\n";
            if (referenceGaps > 0) msg += "  Gaps pendientes: " + juce::String(referenceGaps) + "\\n";
        }

        // ─── Issues ─────────────────────────────────────────────────────────
        if (criticalIssues > 0 || warningIssues > 0) {
            msg += "\\n\\xE2\\x9A\\xA0 **ISSUES:**\\n";
            if (criticalIssues > 0)
                msg += "  \\xF0\\x9F\\x94\\xB4 Cr\\xC3\\xADticos: " + juce::String(criticalIssues) + "\\n";
            if (warningIssues > 0) msg += "  \\xF0\\x9F\\x9F\\xA1 Advertencias: " + juce::String(warningIssues) + "\\n";
        }

        // ─── Correcciones ───────────────────────────────────────────────────
        if (pendingCorrections > 0 || appliedCorrections > 0) {
            msg += "\\n\\xF0\\x9F\\x94\\xA7 **CORRECCIONES:**\\n";
            if (pendingCorrections > 0) msg += "  Pendientes: " + juce::String(pendingCorrections) + "\\n";
            if (appliedCorrections > 0) msg += "  Aplicadas: " + juce::String(appliedCorrections) + "\\n";
        }

        msg +=
            "\\n\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x"
            "90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x9"
            "0\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90"
            "\\xE2\\x95\\x90\\xE2\\x95\\x90\\xE2\\x95\\x90\\n"
            "Escribe **/help** para ver los comandos disponibles.";
        return msg;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionContext::toLLMContext — Contexto estructurado para el LLM
    //  Este bloque se pre-adjunta al prompt cuando el LLM va a generar una
    //  respuesta, para que tenga toda la información de la sesión sin tener
    //  que "investigar" internamente.
    //
    //  MEJORAS V2:
    //  - Añadidas restricciones por fase (qu\xC3\xA9 NO hablar en cada fase)
    //  - Añadido resumen de workflow events (\xC3\xBAltimas acciones del usuario)
    //  - Añadido target por g\xC3\xA9nero (LUFS, crest, headroom)
    //  - Añadido resumen de gaps contra referencia (diferencias por regi\xC3\xB3n espectral)
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String SessionContext::toLLMContext() const
    {
        juce::String ctx;
        ctx = "=== SESSION CONTEXT ===\\n";

        // ─── Modo y g\xC3\xA9nero ───────────────────────────────────────────────
        ctx += "Mode: " + juce::String((coachMode == CoachMode::Mix) ? "Mix" : "Master") + "\\n";
        if (genre.isNotEmpty()) ctx += "Genre: " + genre + "\\n";
        ctx += "Phase: " + juce::String(phaseNames[static_cast<int>(currentPhase)]) + "\\n";
        ctx += "PhaseProgress: " + juce::String(static_cast<int>(phaseProgress * 100.0f)) + "%\\n";
        ctx += "Achievements: " + juce::String(achievementCount) + "\\n";

        // ─── RESTRICCIONES POR FASE — Qu\xC3\xA9 NO debe hacer el coach en esta fase ─
        ctx += "\\nPhaseRestrictions:\\n";
        switch (currentPhase) {
            case MentorPhase::Organizacion:
                ctx += "  ENFOQUE: Organizaci\xC3\xB3n de la sesi\xC3\xB3n. Roles, nombres, colores, buses.\\n";
                ctx +=
                    "  PROHIBIDO: EQ, compresi\xC3\xB3n, reverb, saturaci\xC3\xB3n, efectos, procesamiento del "
                    "master.\\n";
                ctx += "  PERMITIDO: Sugerencias de ganancia inicial, paneo, agrupaci\xC3\xB3n en buses.\\n";
                break;
            case MentorPhase::GainStaging:
                ctx += "  ENFOQUE: Solo gain staging. Revisa niveles, clipping, headroom.\\n";
                ctx += "  PROHIBIDO: EQ, compresi\xC3\xB3n, reverb, saturaci\xC3\xB3n, ensanchar est\xC3\xA9reo.\\n";
                ctx += "  PROHIBIDO: Hablar de referencias, balance tonal o masterizaci\xC3\xB3n.\\n";
                break;
            case MentorPhase::Balance:
                ctx += "  ENFOQUE: Balance de faders y paneo. Niveles relativos entre instrumentos.\\n";
                ctx += "  PROHIBIDO: Compresi\xC3\xB3n, reverb, saturaci\xC3\xB3n, cambios de EQ.\\n";
                ctx += "  PERMITIDO: Ajustes de ganancia, paneo, agrupaci\xC3\xB3n, volumen relativo.\\n";
                break;
            case MentorPhase::EQ:
                ctx +=
                    "  ENFOQUE: Balance tonal con EQ. Carving espectral, filtros, correcci\xC3\xB3n de frecuencias.\\n";
                ctx += "  SECUENCIA: Primero ganancia, LUEGO balance tonal, DESPU\xC3\xA9S din\xC3\xA1mica.\\n";
                ctx += "  PERMITIDO: EQ, filtros pasa altos/bajos, an\xC3\xA1lisis espectral.\\n";
                ctx += "  PERMITIDO: Mencionar compresi\xC3\xB3n si es relevante, pero no es el foco.\\n";
                ctx += "  PROHIBIDO: Masterizaci\xC3\xB3n, limitaci\xC3\xB3n final, exportaci\xC3\xB3n.\\n";
                break;
            case MentorPhase::Compresion:
                ctx += "  ENFOQUE: Control de din\xC3\xA1mica. Compresores, saturaci\xC3\xB3n, crest factor.\\n";
                ctx += "  PERMITIDO: Compresi\xC3\xB3n, saturaci\xC3\xB3n, limitaci\xC3\xB3n suave, paralela.\\n";
                ctx += "  PERMITIDO: Ajustes finos de EQ si es necesario para la din\xC3\xA1mica.\\n";
                ctx +=
                    "  PROHIBIDO: Masterizaci\xC3\xB3n final, reverb/delay pesados, expansi\xC3\xB3n "
                    "est\xC3\xA9reo.\\n";
                break;
            case MentorPhase::Espacio:
                ctx += "  ENFOQUE: Profundidad, est\xC3\xA9reo, automatizaci\xC3\xB3n, \xC3\xBAltimos ajustes.\\n";
                ctx += "  PERMITIDO: Reverb, delay, ensanchar est\xC3\xA9reo, automatizaci\xC3\xB3n, panoramas.\\n";
                ctx += "  PROHIBIDO: Cambios grandes de EQ o compresi\xC3\xB3n que afecten el balance.\\n";
                break;
            case MentorPhase::MasterCheck:
                ctx += "  ENFOQUE: Verificaci\xC3\xB3n final contra referencia y targets.\\n";
                ctx += "  PERMITIDO: Comparaci\xC3\xB3n con referencia, ajustes de LUFS, limitaci\xC3\xB3n suave.\\n";
                ctx += "  PERMITIDO: Verificaci\xC3\xB3n de compatibilidad mono y balance final.\\n";
                ctx += "  PROHIBIDO: Cambios estructurales de mezcla, reasignaci\xC3\xB3n de roles.\\n";
                break;
            default:
                break;
        }

        // ─── Track count by category ───────────────────────────────────────────
        ctx += "\\nTracks (total: " + juce::String(totalTracks) + "):\\n";
        if (drumTracks > 0) ctx += "  Drums: " + juce::String(drumTracks) + "\\n";
        if (bassTracks > 0) ctx += "  Bass: " + juce::String(bassTracks) + "\\n";
        if (guitarTracks > 0) ctx += "  Guitars: " + juce::String(guitarTracks) + "\\n";
        if (keysTracks > 0) ctx += "  Keys: " + juce::String(keysTracks) + "\\n";
        if (vocalTracks > 0) ctx += "  Vocals: " + juce::String(vocalTracks) + "\\n";
        if (fxTracks > 0) ctx += "  FX: " + juce::String(fxTracks) + "\\n";
        if (unknownTracks > 0) ctx += "  Unknown: " + juce::String(unknownTracks) + "\\n";

        // ─── Organization ─────────────────────────────────────────────────────
        ctx += "\\nOrganization:\\n";
        ctx += "  Bussed: " + juce::String(bussedTracks) + "/" + juce::String(totalTracks) + "\\n";
        ctx += "  Named: " + juce::String(namedTracks) + "/" + juce::String(totalTracks) + "\\n";
        ctx += "  RolesAssigned: " + juce::String(rolesAssigned) + "/" + juce::String(totalTracks) + "\\n";

        // ─── Gain staging ─────────────────────────────────────────────────────
        ctx += "\\nGainStaging:\\n";
        ctx += "  Clipping: " + juce::String(clippingTracks) + "\\n";
        ctx += "  NearClip: " + juce::String(nearClipTracks) + "\\n";
        ctx += "  LowSignal: " + juce::String(lowSignalTracks) + "\\n";
        ctx += "  Healthy: " + juce::String(healthyTracks) + "\\n";

        // ─── Master metrics ──────────────────────────────────────────────────
        if (masterPeak > -90.0f) {
            ctx += "\\nMaster:\\n";
            ctx += "  Peak: " + juce::String(masterPeak, 1) + " dBFS\\n";
            ctx += "  RMS: " + juce::String(masterRMS, 1) + " dBFS\\n";
            ctx += "  Crest: " + juce::String(masterCrest, 1) + " dB\\n";
            ctx += "  Correlation: " + juce::String(masterCorrelation, 2) + "\\n";
            if (masterLUFS > -80.0f) {
                ctx += "  LUFS_ShortTerm: " + juce::String(masterLUFS, 1) + "\\n";
                ctx += "  LUFS_Integrated: " + juce::String(masterIntegratedLUFS, 1) + "\\n";
            }
            if (masterTruePeak > -90.0f) ctx += "  TruePeak: " + juce::String(masterTruePeak, 1) + " dBTP\\n";
            ctx += "  StereoWidth: " + juce::String(masterStereoWidth, 2) + "\\n";
        }

        // ─── Target por g\xC3\xA9nero ──────────────────────────────────────────────
        if (genre.isNotEmpty()) {
            auto& profile = CoachEngine::getGenreProfile(genre);
            ctx += "\\nGenreTargets:\\n";
            ctx += "  Description: " + juce::String(profile.description) + "\\n";
            ctx += "  TargetLUFS: " + juce::String(profile.targetIntegratedLUFS, 1) + "\\n";
            ctx += "  TargetCrest: " + juce::String(profile.targetCrestFactor, 1) + " dB\\n";
            ctx += "  TargetHeadroom: " + juce::String(profile.targetHeadroomDb, 1) + " dB\\n";
        }

        // ─── Reference ───────────────────────────────────────────────────────
        ctx += "\\nReference:\\n";
        ctx += "  Loaded: " + juce::String(referenceLoaded ? "yes" : "no") + "\\n";
        if (referenceLoaded) {
            if (referenceName.isNotEmpty()) ctx += "  Name: " + referenceName + "\\n";
            ctx += "  AudioFile: " + juce::String(referenceAudio ? "yes" : "no") + "\\n";
            ctx += "  MatchScore: " + juce::String(matchScore, 2) + "\\n";
            ctx += "  Gaps: " + juce::String(referenceGaps) + "\\n";
        }

        // ─── Issues ───────────────────────────────────────────────────────────
        ctx += "\\nIssues:\\n";
        ctx += "  Critical: " + juce::String(criticalIssues) + "\\n";
        ctx += "  Warnings: " + juce::String(warningIssues) + "\\n";
        ctx += "  Info: " + juce::String(infoIssues) + "\\n";

        // ─── Corrections (active recommendations) ─────────────────────────────
        ctx += "\\nCorrections:\\n";
        ctx += "  Pending: " + juce::String(pendingCorrections) + "\\n";
        ctx += "  Applied: " + juce::String(appliedCorrections) + "\\n";

        // ─── Workflow events (\xC3\xBAltimas acciones del usuario) ────────────────
        if (recentUserActions > 0) ctx += "RecentUserActions: " + juce::String(recentUserActions) + "\\n";

        // ─── Session duration ─────────────────────────────────────────────────
        if (sessionDurationUs > 0) ctx += "SessionDuration: " + juce::String(sessionDurationUs / 1000000) + "s\\n";

        ctx += "=== END SESSION CONTEXT ===";
        return ctx;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachEngine::buildSessionContext — Agrega todo el estado de la sesión
    // ═══════════════════════════════════════════════════════════════════════════
    SessionContext CoachEngine::buildSessionContext(const std::vector<TrackIssue>* issues) const
    {
        SessionContext ctx;

        // ═══ Modo y género ═════════════════════════════════════════════════════
        ctx.coachMode        = coachMode_;
        ctx.genre            = setupGenre_;
        ctx.currentPhase     = phaseManager_.getCurrentPhase();
        ctx.phaseProgress    = phaseManager_.getPhaseProgress(ctx.currentPhase);
        ctx.achievementCount = phaseManager_.getAchievementCount();

        // ═══ Referencia ════════════════════════════════════════════════════════
        ctx.referenceLoaded = referenceMetadata_.valid();
        ctx.referenceAudio  = referenceFingerprint_.valid;
        ctx.referenceName   = referenceMetadata_.name;
        ctx.referenceGenre  = referenceMetadata_.genre;
        if (lastReferenceComparison_.valid) {
            ctx.matchScore    = lastReferenceComparison_.spectralSimilarity;
            auto gaps         = getReferenceGaps();
            ctx.referenceGaps = static_cast<int>(gaps.size());
        }

        // ═══ SlotRegistry: contar pistas, roles, organización ═════════════════
        auto& registry = sharedData_.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

            ctx.totalTracks++;

            // Rol
            TrackRole role = trackRoles_[idx];
            if (role != TrackRole::Unknown) ctx.rolesAssigned++;

            // Categoría
            switch (getRoleCategory(role)) {
                case RoleCategory::Drums:
                    ctx.drumTracks++;
                    break;
                case RoleCategory::Bass:
                    ctx.bassTracks++;
                    break;
                case RoleCategory::Guitars:
                    ctx.guitarTracks++;
                    break;
                case RoleCategory::Keys:
                    ctx.keysTracks++;
                    break;
                case RoleCategory::Vocals:
                    ctx.vocalTracks++;
                    break;
                case RoleCategory::FX:
                    ctx.fxTracks++;
                    break;
                case RoleCategory::Melody:
                    ctx.melodyTracks++;
                    break;
                default:
                    ctx.unknownTracks++;
                    break;
            }

            // Bus asignado
            if (info.bus != BusType::None) ctx.bussedTracks++;

            // Nombre personalizado
            juce::String name = juce::String(info.trackName).trim();
            if (name.isNotEmpty() && !name.startsWith("Pista")) ctx.namedTracks++;

            // Gain staging — cubre TODOS los rangos, sin huecos
            // Escala dBFS: 0=max dig. | -0.5=clipping | -3=muy fuerte | -6=ideal | -24=bajo | -60=sin señal
            auto telem = getLatestTelemetry(idx);
            if (telem.timestamp != 0) {
                float peak = juce::jmax(telem.peakLeft, telem.peakRight);
                if (peak > -0.5f) ctx.clippingTracks++;
                else if (peak > -3.0f)
                    ctx.nearClipTracks++; // -3 a -0.5: muy fuerte, casi clipping
                else if (peak > -30.0f)
                    ctx.healthyTracks++; // -30 a -3: rango normal (incluye fuerte -6 a -3)
                else
                    ctx.lowSignalTracks++; // < -30: señal baja o silencio
            }
        });

        // ═══ AudioAnalyzer (master) ═══════════════════════════════════════════
        {
            const auto& master = audioAnalyzer_.getMasterAnalysis();
            float peak         = master.getPeak();
            if (peak > -100.0f) {
                ctx.masterPeak = peak;
                ctx.masterRMS  = master.getRMS();
                float crest    = peak - master.getRMS();
                if (crest < 0.0f) crest = 0.0f;
                ctx.masterCrest = crest;
            }

            ctx.masterCorrelation    = master.getCorrelation();
            ctx.masterLUFS           = audioAnalyzer_.getShortTermLUFS();
            ctx.masterIntegratedLUFS = audioAnalyzer_.getIntegratedLUFS();
            ctx.masterTruePeak       = audioAnalyzer_.getTruePeakDBTP();
            ctx.masterStereoWidth    = audioAnalyzer_.getAvgStereoWidth();
        }

        // ═══ Recommendations / Corrections ════════════════════════════════════
        for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
            const auto& rec = recommendations_[i];
            if (rec.slotIndex >= 0) {
                switch (rec.status) {
                    case TrackRecommendation::Status::Pending:
                        ctx.pendingCorrections++;
                        break;
                    case TrackRecommendation::Status::Applied:
                        ctx.appliedCorrections++;
                        break;
                    default:
                        break;
                }
            }
        }

        // ═══ Issues (si se proveyeron) ═══════════════════════════════════════
        if (issues != nullptr) {
            for (const auto& issue : *issues) {
                if (issue.severity >= 0.8f) ctx.criticalIssues++;
                else if (issue.severity >= 0.4f)
                    ctx.warningIssues++;
                else
                    ctx.infoIssues++;
            }
        }

        // ═══ Semantic Analysis: incluir issues por rol ════════════════════════
        {
            auto semanticDiffs = runSemanticAnalysis();
            for (const auto& diff : semanticDiffs) {
                for (const auto& semIssue : diff.issues) {
                    switch (semIssue.severity) {
                        case SemanticIssue::Severity::Critical:
                            ctx.criticalIssues++;
                            break;
                        case SemanticIssue::Severity::Warning:
                            ctx.warningIssues++;
                            break;
                        default:
                            ctx.infoIssues++;
                            break;
                    }
                }
            }
        }

        // ═══ WorkflowDetector: acciones recientes ═════════════════════════════
        auto recentEvents     = workflowDetector_.getRecentEvents(16);
        ctx.recentUserActions = static_cast<int>(recentEvents.size());

        // ═══ Session duration ═════════════════════════════════════════════════
        int64_t nowUs = juce::Time::getMillisecondCounter() * 1000;
        if (lastUserInteractionTimeUs_ > 0) ctx.sessionDurationUs = nowUs - lastUserInteractionTimeUs_;
        else if (lastPeriodicAnalysisUs_ > 0)
            ctx.sessionDurationUs = nowUs - lastPeriodicAnalysisUs_;

        return ctx;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachEngine::buildSessionContextText — Versión textual para el chat
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String CoachEngine::buildSessionContextText(const std::vector<TrackIssue>* issues) const
    {
        auto ctx = buildSessionContext(issues);
        return ctx.toChatMessage();
    }

} // namespace mixcoach

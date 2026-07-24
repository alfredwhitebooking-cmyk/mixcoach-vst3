#include "CoachEngine.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MANEJO DE MENSAJES DEL USUARIO
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::handleUserMessage(const juce::String& message)
    {
        auto lower = message.toLowerCase();

        // ═══ CoachingStageManager: detectar respuestas de aprobación ═══
        // Solo cuando el stage manager está esperando aprobación explícita.
        // Usamos split por espacios para evitar falsos positivos ("si" dentro de "silla").
        if (stageManager_.isInitialized() && stageManager_.isAwaitingApproval()) {
            auto tokens = juce::StringArray::fromTokens(lower, " \t\r\n", "");
            // Separar palabras compuestas ("sigue así" → "sigue", "así") y frases comunes
            static const juce::StringArray approvalTokens = {
                "listo", "listos", "preparado", "preparados", "ready",
                "sí", "si", "yes", "dale", "vamos", "ok", "okay",
                "confirmar", "confirmo", "adelante", "avanzar", "continue",
                "siguiente", "next", "continuar", "claro"};

            bool matched = false;
            for (const auto& token : tokens) {
                auto t = token.trim().toLowerCase();
                if (t.isEmpty()) continue;
                // Solo hacer match exacto, no contains, para evitar falsos positivos
                for (const auto& at : approvalTokens) {
                    if (t == at) {
                        matched = true;
                        break;
                    }
                }
                if (matched) break;
            }

            if (matched) {
                bool advanced = stageManager_.approveAdvance();
                if (advanced) {
                    setUserInteracted();
                    return;
                }
            }
            // ═══ No rechazar automáticamente — solo ignorar el mensaje
            // y dejar que el stage manager siga esperando.
            // El usuario puede estar escribiendo algo normal ("¿cómo va la mezcla?")
            // mientras el coach espera aprobación. No debemos penalizar eso.
            setUserInteracted();
            return;
        }

        // Comandos de sistema
        if (lower.startsWith("/")) {
            executeCommand(message);
            return;
        }

        // ═══ RUTEO AL LLM (lenguaje natural) ═══════════════════════════════
        if (llmEnabled_ && setupStep_ == SetupStep::Complete && llmStreamingCallback_) {
            bool accepted = llmStreamingCallback_(
                message,
                [this](const juce::String& token) {
                    if (streamTokenCb_) streamTokenCb_(token);
                },
                [this](const juce::String& response) {
                    if (streamEndedCb_) streamEndedCb_();
                    if (llmResponseCompleteCb_) llmResponseCompleteCb_(response);
                    LogHelper::writeToLog("[CoachEngine] LLM streaming completado (" + juce::String(response.length())
                                          + " chars)");
                });

            if (accepted) {
                if (streamStartedCb_) streamStartedCb_();
                setUserInteracted();
                return;
            }
        }

        // ═══ Fallback: LLM sin streaming ═════
        if (llmEnabled_ && setupStep_ == SetupStep::Complete && llmResponseCallback_) {
            bool accepted =
                llmResponseCallback_(message, [this](const juce::String& response) { respondWithLLM(response); });

            if (accepted) {
                setUserInteracted();
                return;
            }
        }

        // ─── FALLBACK: Respuesta contextual según la fase ─────
        auto phase        = phaseManager_.getCurrentPhase();
        auto& registry    = sharedData_.getSlotRegistry();
        bool hasTelemetry = (registry.activeCount() > 0);

        switch (phase) {
            case MentorPhase::GainStaging:
                if (hasTelemetry) {
                    analyzeGainStagingReal();
                    respondWith(
                        "[CHART] Puedes preguntar \"c\xC3\xB3mo est\xC3\xA1n los niveles\" o "
                        "\"\xC2\xBFhay clipping?\" para un an\xC3\xA1lisis detallado.",
                        MentorMessage::Type::Info);
                }
                else {
                    respondWith(
                        "A\xC3\xBAn no detecto pistas activas. Aseg\xC3\xBArate de tener "
                        "Messengers cargados en tus pistas.",
                        MentorMessage::Type::Info);
                }
                break;

            case MentorPhase::Organizacion:
                if (hasTelemetry) {
                    analyzeOrganisationReal();
                }
                else {
                    respondWith(
                        "En fase de Organizaci\xC3\xB3n. Recomiendo:\n"
                        "1. Nombra cada pista descriptivamente\n"
                        "2. Asigna colores por familia\n"
                        "3. Agrupa en buses virtuales",
                        MentorMessage::Type::Tip);
                }
                break;

            case MentorPhase::Balance:
                if (hasTelemetry) {
                    analyzeTonalBalanceReal();
                }
                else {
                    respondWith(
                        "En fase de Balance Tonal. Revisa el espectro y "
                        "comp\xC3\xA1ralo con referencias de tu g\xC3\xA9nero.",
                        MentorMessage::Type::Info);
                }
                break;

            case MentorPhase::Compresion:
                if (hasTelemetry) {
                    analyzeDynamicsReal();
                }
                else {
                    respondWith(
                        "En fase de Din\xC3\xA1mica. Considera compresores en buses "
                        "y limitador en el master (solo 1-2 dB).",
                        MentorMessage::Type::Tip);
                }
                break;

            case MentorPhase::Espacio:
                if (hasTelemetry) {
                    analyzePhaseReal();
                }
                respondWith(
                    "En fase de Espacialidad. Trabaja panoramas, reverbs, "
                    "y efectos de profundidad.",
                    MentorMessage::Type::Tip);
                break;
        }

        // Verificar logros
        auto activeCount = registry.activeCount();
        if (activeCount >= 1) phaseManager_.unlockAchievement(Achievement::FirstTrack);
        if (activeCount >= 5) phaseManager_.unlockAchievement(Achievement::FiveTracks);
        if (activeCount >= 10) phaseManager_.unlockAchievement(Achievement::TenTracks);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TIP PROACTIVO
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::generateProactiveTip()
    {
        auto& registry = sharedData_.getSlotRegistry();
        int active     = registry.activeCount();

        if (active == 0) {
            respondWith(
                "\xF0\x9F\x92\xA1 A\xC3\xBAn no hay pistas activas. Carga Messengers en tus pistas "
                "para empezar a recibir an\xC3\xA1lisis en tiempo real.",
                MentorMessage::Type::Tip);
            return;
        }

        auto phase = phaseManager_.getCurrentPhase();
        switch (phase) {
            case MentorPhase::GainStaging:
                analyzeGainStagingReal();
                if (juce::Time::getMillisecondCounter() * 1000 - lastPeakWarningUs_ > kWarningCooldownUs) {
                    respondWith(
                        "\xF0\x9F\x92\xA1 Tip r\xC3\xA1pido: revisa que el fader de ganancia de cada pista "
                        "permita picos de -18 dB a -12 dB en el submix antes de "
                        "tocar el fader de volumen.",
                        MentorMessage::Type::Tip);
                }
                break;

            case MentorPhase::Organizacion:
                analyzeOrganisationReal();
                break;

            case MentorPhase::Balance:
                respondWith(
                    "\xF0\x9F\x92\xA1 Para evaluar el balance tonal, revisa el Analyzer "
                    "(pesta\xC3\xB1"
                    "a 2). Busca una curva suave de menos de 3 dB/octava "
                    "de diferencia entre bandas adyacentes.",
                    MentorMessage::Type::Tip);
                analyzeTonalBalanceReal();
                break;

            case MentorPhase::Compresion:
                analyzeDynamicsReal();
                break;

            case MentorPhase::Espacio:
                analyzePhaseReal();
                break;

            default:
                respondWith(
                    "\xF0\x9F\x92\xA1 Escribe /help para ver comandos disponibles o preg\xC3\xBAntame "
                    "\"\xC2\xBF"
                    "c\xC3\xB3mo va la mezcla?\" para un an\xC3\xA1lisis completo.",
                    MentorMessage::Type::Tip);
                break;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PROGRESO Y LOGROS
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::checkProgress()
    {
        auto phase     = phaseManager_.getCurrentPhase();
        auto progress  = phaseManager_.getPhaseProgress(phase);
        auto& registry = sharedData_.getSlotRegistry();

        respondWith(juce::String("[TREND] **Progreso** en '") + phaseManager_.phaseDescription(phase)
                        + "': " + juce::String(static_cast<int>(progress * 100.0f)) + "%\n"
                        + "Pistas activas: " + juce::String(registry.activeCount()) + "\n"
                        + "Logros: " + juce::String(phaseManager_.getAchievementCount()),
                    MentorMessage::Type::Info);

        if (phaseManager_.isPhaseComplete(phase)) {
            respondWith(
                "\xF0\x9F\x91\x8F Bien hecho. Esta fase est\xC3\xA1 completa. "
                "Escribe **/next** y seguimos con la siguiente.",
                MentorMessage::Type::Achievement);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  ANUNCIO DE NUEVA PISTA
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::announceNewTrack(int slotIndex, const juce::String& trackName, const juce::Colour& colour)
    {
        juce::ignoreUnused(slotIndex, colour);

        auto& registry = sharedData_.getSlotRegistry();
        auto telem     = getLatestTelemetry(slotIndex);

        juce::String name = trackName.trim();
        if (name.isEmpty()) name = "una pista nueva";

        juce::String msg = "\xF0\x9F\x91\x82 Escucha... " + name + " acaba de aparecer.";

        if (telem.timestamp != 0) {
            float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
            msg += " La veo a " + juce::String(peakDb, 1) + " dB pico";
            if (peakDb > -0.5f)
                msg += " \xF0\x9F\x9A\xA8 **est\xC3\xA1 clipeando!** Bajemos eso ya.";
            else if (peakDb > -3.0f)
                msg += " — un poco caliente, vigilala.";
            else if (peakDb > -12.0f)
                msg += " — nivel saludable.";
            else
                msg += " — un poco baja, quiz\xC3\xA1s necesite gain.";
        }
        else {
            msg += " Dame un segundo para escucharla...";
        }

        if (registry.activeCount() >= 3) {
            msg += " Ya son " + juce::String(registry.activeCount())
                   + ". La mezcla empieza a tomar forma.";
        }

        respondWith(msg, MentorMessage::Type::Info);

        if (phaseManager_.getCurrentPhase() == MentorPhase::Organizacion) {
            respondWith(
                "\xF0\x9F\x9A\x80 Bien, ya tenemos pistas. Pasamos a "
                "**Gain Staging** — vamos a ajustar los niveles de entrada.",
                MentorMessage::Type::Tip);
            phaseManager_.advanceToNextPhase();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  respondWithLLM — Responde con mensaje premium (burbuja completa)
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::respondWithLLM(const juce::String& text)
    {
        respondWithPremium(text, MentorMessage::Type::Info);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildPluginSuggestionBlock — Genera bloque 3-tier de sugerencias de plugins
    //  para incrustar en mensajes del coach durante el análisis activo.
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String CoachEngine::buildPluginSuggestionBlock(const juce::String& domain,
                                                          const juce::String& issueType,
                                                          const juce::String& trackName,
                                                          float delta,
                                                          float frequencyHz) const
    {
        if (!pluginSuggestionsProvider_.isReady())
            return {};

        auto problemType = PluginSuggestionsProvider::domainToProblemType(domain, issueType);
        if (problemType == ProblemType::Unknown)
            return {};

        auto suggestions = pluginSuggestionsProvider_.getSuggestionsForProblem(problemType, delta, frequencyHz);
        if (suggestions.empty())
            return {};

        // ─── Emoji de cabecera según el dominio ────────────────────────────
        const char* headerEmoji = "\xF0\x9F\x92\xA1"; // 💡
        if (domain == "gain" || problemType == ProblemType::Clipping)
            headerEmoji = "[COACH]"; // 🎛
        else if (domain == "tonal" || domain == "eq" || domain == "masking")
            headerEmoji = "[COACH]"; // 🎛️
        else if (domain == "dynamics" || domain == "dynamic")
            headerEmoji = "[TREND]"; // 📈
        else if (domain == "phase" || domain == "spatial" || domain == "stereo")
            headerEmoji = "\xF0\x9F\x94\xAE"; // 🔮

        juce::String block;
        block += "\n\n" + juce::String(headerEmoji) + " **Plugins sugeridos**";
        if (trackName.isNotEmpty())
            block += " para " + trackName;
        block += ":\n\n";

        for (const auto& sug : suggestions) {
            if (!sug.isValid() || sug.plugin == nullptr)
                continue;

            // ─── Icono y tier ──────────────────────────────────────────────
            const char* tierIcon;
            juce::String tierLabel;
            switch (sug.plugin->tier) {
                case PluginTier::Native:
                    tierIcon = "[COACH]"; tierLabel = "NATIVO"; break;
                case PluginTier::Free:
                    tierIcon = "\xF0\x9F\x9F\xA2"; tierLabel = "GRATIS"; break;
                case PluginTier::Premium:
                    tierIcon = "\xE2\xAD\x90"; tierLabel = "PROFESIONAL"; break;
                case PluginTier::UserHas:
                    tierIcon = "[BOLT]"; tierLabel = "YA TIENES"; break;
                default:
                    continue;
            }

            // ─── Interpolar actionText con valores reales ───────────────────
            juce::String action = PluginSuggestionsProvider::interpolateAction(
                sug.config->actionText, sug.delta, sug.frequencyHz);

            // ─── Formatear línea del plugin ────────────────────────────────
            juce::String line = juce::String(tierIcon) + " " + tierLabel + ": **" + sug.plugin->name + "**";

            if (action.isNotEmpty())
                line += " — " + action;

            // ═══ Rating ═══════════════════════════════════════════════════
            int fullStars = static_cast<int>(sug.plugin->rating);
            bool halfStar = (sug.plugin->rating - fullStars >= 0.3f);
            juce::String stars = juce::String::repeatedString(juce::String("\xE2\x98\x85"), fullStars);
            if (halfStar) stars += "\xE2\x9C\x86";
            if (stars.isNotEmpty())
                line += " " + stars;

            block += line + "\n";
        }

        // ─── Footer: tip para elegir tier ──────────────────────────────────
        block += "\n\xF0\x9F\x92\xA1 Elige el tier que mejor se adapte a tu flujo de trabajo.";

        return block;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getGenreCharacteristicsMessage — Mensaje rico con características del género
    //  Cuando el usuario selecciona un género, el Coach responde con:
    //    "El Afrobeat necesita:"
    //    ✓ Groove
    //    ✓ Punch
    //    ✓ Mucho movimiento estéreo
    //    ...
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String CoachEngine::getGenreCharacteristicsMessage(const juce::String& genreKey,
                                                              const juce::String& genreLabel)
    {
        auto g = genreKey.trim().toLowerCase();
        juce::String title;
        juce::StringArray characteristics;
        juce::String sonicSignature;
        bool hasReferenceTip = false;

        // ─── AFROBEAT: percusivo, cálido, bajos bailables ────────────────
        if (g == "afrobeat" || g == "afrobeats" || g == "world") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Afrobeat";
            characteristics.addArray({"Groove rítmico constante",
                                      "Punch en la percusión",
                                      "Movimiento estéreo amplio",
                                      "Graves controlados y con cuerpo",
                                      "Voces presentes con aire"});
            sonicSignature = "C\xC3\xA1lido, percusivo, con mucha energ\xC3\xAD" "a r\xC3\xADtmica";
            hasReferenceTip = true;
        }
        // ─── REGGAETON: 808 dominante, dembow, voz al frente ─────────────
        else if (g == "reggaeton" || g == "reggaeton/latin" || g == "latin" || g == "dembow") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Reggaet\xC3\xB3n";
            characteristics.addArray({"Dembow groove constante (kick-808 alternado)",
                                      "808 profundo y presente",
                                      "Voz principal al frente y clara",
                                      "Hi-hats brillantes con rol",
                                      "Percusi\xC3\xB3n con din\xC3\xA1mica controlada"});
            sonicSignature = "Urbano, 808-heavy, vocal-forward";
            hasReferenceTip = true;
        }
        // ─── TRAP: 808 pesado, hi-hats brillantes, batería agresiva ──────
        else if (g == "trap") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Trap";
            characteristics.addArray({"808s profundos y sostenidos",
                                      "Hi-hats r\xC3\xA1pidos y brillantes",
                                      "Snare seco con click agudo",
                                      "Sub-graves masivos y controlados",
                                      "Rango din\xC3\xA1mico amplio (crest alto)"});
            sonicSignature = "Agresivo, sub-heavy, din\xC3\xA1mico";
            hasReferenceTip = true;
        }
        // ─── HIP HOP: boom bap, sampleo, voces con peso ──────────────────
        else if (g == "hiphop" || g == "hip-hop" || g == "rap") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Hip Hop";
            characteristics.addArray({"Kick y snare con peso (boom-bap)",
                                      "Bajo presente pero no dominante",
                                      "Voz centrada con presencia",
                                      "Sampleo con calidez anal\xC3\xB3gica",
                                      "Rango din\xC3\xA1mico natural"});
            sonicSignature = "Groove pesado, vocal-centric, cl\xC3\xA1sico";
        }
        // ─── POP: claridad, brillo, gancho vocal ─────────────────────────
        else if (g == "pop") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Pop";
            characteristics.addArray({"Voz principal cristalina y al frente",
                                      "Compresi\xC3\xB3n suave y consistente",
                                      "Brillo en agudos (presencia + aire)",
                                      "Balance espectral equilibrado",
                                      "Est\xC3\xA9reo amplio pero controlado"});
            sonicSignature = "Limpio, brillante, vocal-forward";
            hasReferenceTip = true;
        }
        // ─── ROCK: guitarras, batería potente, energía ───────────────────
        else if (g == "rock") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Rock";
            characteristics.addArray({"Guitarras con presencia en medios",
                                      "Bater\xC3\xAD" "a potente y din\xC3\xA1mica",
                                      "Compresi\xC3\xB3n natural (crest alto)",
                                      "Voz con character y actitud",
                                      "Bajo que sostiene sin dominar"});
            sonicSignature = "Energ\xC3\xA9tico, din\xC3\xA1mico, guitarrero";
        }
        // ─── EDM: sub masivo, drops, presencia extrema ───────────────────
        else if (g == "edm" || g == "electronic" || g == "house" || g == "techno"
                 || g == "trance" || g == "dubstep") {
            title = genreLabel.isNotEmpty() ? genreLabel : "EDM";
            characteristics.addArray({"Sub-graves masivos y controlados",
                                      "Compresi\xC3\xB3n fuerte (crest bajo)",
                                      "Presencia extrema en agudos",
                                      "Drops con impacto y tensi\xC3\xB3n",
                                      "Est\xC3\xA9reo muy amplio y efectos"});
            sonicSignature = "Grande, comprimido, sub-heavy";
            hasReferenceTip = true;
        }
        // ─── JAZZ: dinámica amplia, calidez, acústico ────────────────────
        else if (g == "jazz") {
            title = genreLabel.isNotEmpty() ? genreLabel : "Jazz";
            characteristics.addArray({"Din\xC3\xA1mica amplia y natural",
                                      "Calidez anal\xC3\xB3gica en medios",
                                      "Instrumentos ac\xC3\BAsticos con espacio",
                                      "Poco o nada de compresi\xC3\xB3n",
                                      "Est\xC3\xA9reo natural y profundo"});
            sonicSignature = "C\xC3\xA1lido, din\xC3\xA1mico, ac\xC3\BAstico";
        }
        // ─── R&B: cuerdas, voces sedosas, groove ─────────────────────────
        else if (g == "rnb" || g == "r&b" || g == "rb" || g == "soul") {
            title = genreLabel.isNotEmpty() ? genreLabel : "R&B";
            characteristics.addArray({"Voces sedosas con capas arm\xC3\B3nicas",
                                      "Cuerdas y pads que envuelven",
                                      "Groove suave con bajo presente",
                                      "Compresi\xC3\xB3n suave y musical",
                                      "Est\xC3\xA9reo amplio y profundo"});
            sonicSignature = "Sedoso, atmosf\xC3\xA9rico, vocal-forward";
        }
        // ─── DEFAULT (Otro, géneros sin perfil específico) ──────────────
        else {
            title = genreLabel.isNotEmpty() ? genreLabel : "Tu g\xC3\xA9nero";
            characteristics.addArray({"Definamos juntos el sonido que buscas",
                                      "Usar\xC3\xA9 un perfil balanceado por defecto",
                                      "Ajustaremos seg\xC3\xAn lo que escuche"});
            sonicSignature = "Perfil neutral — adaptable";
        }

        // ─── Construir mensaje formateado ─────────────────────────────────
        juce::String msg;

        if (sonicSignature.isNotEmpty()) {
            msg += "\xF0\x9F\x93\x8C **" + title + "** \xE2\x80\x94 " + sonicSignature + "\n\n";
        } else {
            msg += "\xF0\x9F\x93\x8C **" + title + "**\n\n";
        }

        msg += "**" + title + " necesita:**\n";
        for (const auto& c : characteristics) {
            msg += "[OK] " + c + "\n";
        }

        msg += "\n";

        // ═══ INCLUIR TARGETS DE MEZCLA DESDE getGenreProfile() ════════════════
        {
            const auto& profile = getGenreProfile(genreKey);
            msg += "[TARGET] **Targets de mezcla para " + title + "**\n";
            msg += "  \xE2\x80\xA2 LUFS integrado: **" + juce::String(profile.targetIntegratedLUFS, 1) + " LUFS** (nivel objetivo)\n";
            msg += "  \xE2\x80\xA2 Crest Factor: **" + juce::String(profile.targetCrestFactor, 1) + " dB** (rango din\xC3\xA1mico)\n";
            msg += "  \xE2\x80\xA2 Headroom: **" + juce::String(profile.targetHeadroomDb, 1) + " dB** (margen al pico)\n\n";

            // ═══ ÉNFASIS ESPECTRAL ═══════════════════════════════════════
            // Los offsets NEGATIVOS = la región SUBE de nivel (más presencia)
            // Los offsets POSITIVOS = la región BAJA de nivel (menos presencia)
            struct RegionInfo { const char* name; float offset; };
            RegionInfo regions[6] = {
                {"Sub   (20-86 Hz)",     profile.subBassOffset},
                {"Bass  (86-301 Hz)",    profile.bassOffset},
                {"LoMid (301-1076 Hz)",  profile.lowMidOffset},
                {"HiMid (1076-3532 Hz)", profile.highMidOffset},
                {"Pres  (3532-8355 Hz)", profile.presenceOffset},
                {"Air   (8355-16458 Hz)", profile.airOffset}
            };

            juce::String emphasisLine;
            for (auto& r : regions) {
                if (std::fabs(r.offset) > 0.5f) {
                    if (emphasisLine.isNotEmpty()) emphasisLine += " | ";
                    // offset < 0 = SUBE => ▲, offset > 0 = BAJA => ▼
                    emphasisLine += juce::String(r.name) + " " + (r.offset < 0 ? "[EXPAND]" : "[COLLAPSE]");
                }
            }

            if (emphasisLine.isNotEmpty()) {
                msg += "[CHART] **\xC3\x89nfasis espectral:**\n";
                msg += "  " + emphasisLine + "\n\n";
            }
        }

        // ═══ Gap #4: FRECUENCIAS CLAVE POR ROL SEG\xC3\x9AN EL G\xC3\x89NERO ═══════════
        {
            // Cada g\xC3\xA9nero tiene 5 roles clave con su rango de frecuencia \xC3\xB3ptimo
            struct RoleFreq { const char* icon; const char* role; const char* range; const char* note; };
            std::vector<RoleFreq> roleFreqs;

            if (g == "reggaeton" || g == "reggaeton/latin" || g == "latin" || g == "dembow") {
                roleFreqs = {
                    {"[DRUM]", "808 / Sub",    "40\xE2\x80\x93" "60 Hz",   "Fundamental del 808"},
                    {"[DRUM]", "Kick",         "60\xE2\x80\x93" "100 Hz",  "Ataque punchy"},
                    {"[FIRE]", "Hi-Hat",       "8\xE2\x80\x93" "12 kHz",   "Brillo y roll"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Presencia y claridad"},
                    {"[DRUM]", "Snare",        "200\xE2\x80\x93" "400 Hz", "Cuerpo del golpe"}
                };
            } else if (g == "trap") {
                roleFreqs = {
                    {"[FIRE]", "808 Sub",      "30\xE2\x80\x93" "50 Hz",   "Subgrave masivo"},
                    {"[DRUM]", "Kick",         "60\xE2\x80\x93" "100 Hz",  "Ataque y peso"},
                    {"[FIRE]", "Hi-Hat",       "8\xE2\x80\x93" "12 kHz",   "Rolls r\xC3\xA1pidos"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Presencia con actitud"},
                    {"[TREND]", "Snare/Clap",   "200\xE2\x80\x93" "400 Hz", "Golpe seco y agudo"}
                };
            } else if (g == "pop") {
                roleFreqs = {
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Claridad vocal"},
                    {"[DRUM]", "Kick",         "50\xE2\x80\x93" "100 Hz",  "Base r\xC3\xADtmica"},
                    {"[MUSIC]", "Bajo",         "60\xE2\x80\x93" "120 Hz",  "Cuerpo y groove"},
                    {"[MUSIC]", "Guitarra",     "1\xE2\x80\x93" "3 kHz",    "Relleno arm\xC3\xB3nico"},
                    {"[DRUM]", "Snare",        "200\xE2\x80\x93" "400 Hz", "Cuerpo del golpe"}
                };
            } else if (g == "rock") {
                roleFreqs = {
                    {"[MUSIC]", "Guitarra",     "500\xE2\x80\x93" "2 kHz",  "Presencia y car\xC3\xA1" "cter"},
                    {"[DRUM]", "Kick",         "50\xE2\x80\x93" "100 Hz",  "Golpe potente"},
                    {"\xF0\x9F\x8E\x99", "Bajo",         "80\xE2\x80\x93" "200 Hz",  "Sost\xC3\xA9n sin opacar"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "5 kHz",    "Presencia con actitud"},
                    {"\xF0\x9F\xA4\x98", "Snare",        "200\xE2\x80\x93" "400 Hz", "Golpe natural"}
                };
            } else if (g == "edm" || g == "electronic" || g == "house" || g == "techno"
                       || g == "trance" || g == "dubstep") {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "100\xE2\x80\x93" "120 Hz", "Golpe contundente"},
                    {"\xF0\x9F\x94\x8B", "Sub Bass",    "40\xE2\x80\x93" "60 Hz",   "Subgrave masivo"},
                    {"[FIRE]", "Hi-Hat",       "8\xE2\x80\x93" "12 kHz",   "Brillo extremo"},
                    {"[MUSIC]", "Lead Synth",  "2\xE2\x80\x93" "4 kHz",    "Presencia del drop"},
                    {"\xF0\x9F\x91\x8F", "Clap/Snare",  "1\xE2\x80\x93" "3 kHz",    "Ataque del clap"}
                };
            } else if (g == "hiphop" || g == "hip-hop" || g == "rap") {
                roleFreqs = {
                    {"\xF0\x9F\x94\x8B", "808 / Sub",    "30\xE2\x80\x93" "50 Hz",   "Alma del beat"},
                    {"[DRUM]", "Kick",         "50\xE2\x80\x93" "80 Hz",   "Pegada boom-bap"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Claridad narrativa"},
                    {"[FIRE]", "Hi-Hat",       "8\xE2\x80\x93" "12 kHz",   "Groove r\xC3\xADtmico"},
                    {"[TREND]", "Snare",        "200\xE2\x80\x93" "400 Hz", "Peso del golpe"}
                };
            } else if (g == "afrobeat" || g == "afrobeats" || g == "world") {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "60\xE2\x80\x93" "100 Hz",  "Base del groove"},
                    {"\xF0\x9F\x8E\xB6", "Bajo",         "80\xE2\x80\x93" "150 Hz",  "Groove c\xC3\xA1lido"},
                    {"\xF0\x9F\x94\x8A", "Percusi\xC3\xB3n", "1\xE2\x80\x93" "3 kHz",  "Punch percusivo"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Presencia mel\xC3\xB3" "dica"},
                    {"[MUSIC]", "Guitarra",     "1\xE2\x80\x93" "3 kHz",    "Ritmo y textura"}
                };
            } else if (g == "jazz") {
                roleFreqs = {
                    {"\xF0\x9F\x8E\xB7", "Bater\xC3\xAD" "a",   "50\xE2\x80\x93" "200 Hz",  "Base r\xC3\xADtmica natural"},
                    {"\xF0\x9F\x8E\x99", "Contrabajo",  "60\xE2\x80\x93" "120 Hz",  "Cuerpo del walking"},
                    {"[MUSIC]", "Piano",       "100\xE2\x80\x93" "3 kHz",  "Armon\xC3\xAD" "a completa"},
                    {"\xF0\x9F\x8E\xB7", "Saxof\xC3\xB3n",  "200\xE2\x80\x93" "2 kHz",  "Calidez mel\xC3\xB3" "dica"},
                    {"\xF0\x9F\x94\x8A", "Platillos",   "5\xE2\x80\x93" "10 kHz",   "Brillo sutil"}
                };
            } else if (g == "rnb" || g == "r&b" || g == "rb" || g == "soul") {
                roleFreqs = {
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Sedosa y presente"},
                    {"[DRUM]", "Kick",         "50\xE2\x80\x93" "100 Hz",  "Base relajada"},
                    {"[MUSIC]", "Bajo",         "60\xE2\x80\x93" "120 Hz",  "Groove mel\xC3\xB3" "dico"},
                    {"[MUSIC]", "Teclados",     "100\xE2\x80\x93" "3 kHz",  "Atm\xC3\xB3sfera"},
                    {"[TREND]", "Snare",        "200\xE2\x80\x93" "400 Hz", "Cuerpo suave"}
                };
            } else if (g == "country") {
                roleFreqs = {
                    {"[MUSIC]", "Guitarra",     "1\xE2\x80\x93" "3 kHz",    "Presencia ac\xC3\xBAstica"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Narrativa clara"},
                    {"[DRUM]", "Kick",         "50\xE2\x80\x93" "100 Hz",  "Base natural"},
                    {"\xF0\x9F\x8E\x99", "Bajo",         "80\xE2\x80\x93" "200 Hz",  "Groove constante"},
                    {"\xF0\x9F\x8E\xB7", "Fiddle",       "500\xE2\x80\x93" "2 kHz",  "Car\xC3\xA1" "cter country"}
                };
            } else if (g == "metal") {
                roleFreqs = {
                    {"\xF0\x9F\xA4\x98", "Guitarra",     "500\xE2\x80\x93" "2 kHz",  "Distorsi\xC3\xB3n masiva"},
                    {"[DRUM]", "Kick",         "100 Hz",               "Golpe ultrafr\xC3\xA1gil"},
                    {"\xF0\x9F\x8E\x99", "Bajo",         "80\xE2\x80\x93" "200 Hz",  "Sigue a la guitarra"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Agresividad vocal"},
                    {"\xF0\x9F\xA4\x98", "Snare",        "400 Hz",               "Golpe explosivo"}
                };
            } else if (g == "house") {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "100\xE2\x80\x93" "120 Hz", "Four-on-the-floor"},
                    {"[MUSIC]", "Bajo",         "60\xE2\x80\x93" "80 Hz",   "L\xC3\xADnea hipn\xC3\xB3tica"},
                    {"[FIRE]", "Hi-Hat",       "8\xE2\x80\x93" "12 kHz",   "Off-beat shuffle"},
                    {"\xF0\x9F\x91\x8F", "Clap",         "1\xE2\x80\x93" "3 kHz",    "Ataque en 2 y 4"},
                    {"[MUSIC]", "Synth/Pad",   "200\xE2\x80\x93" "2 kHz",  "Atm\xC3\xB3sfera"}
                };
            } else if (g == "techno") {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "40\xE2\x80\x93" "60 Hz",   "Motor hipn\xC3\xB3tico"},
                    {"[MUSIC]", "Bajo",         "60\xE2\x80\x93" "100 Hz",  "Subgrave profundo"},
                    {"[FIRE]", "Hi-Hat",       "8\xE2\x80\x93" "12 kHz",   "Textura percusiva"},
                    {"\xF0\x9F\x91\x8F", "Clap",         "1\xE2\x80\x93" "3 kHz",    "Percusi\xC3\xB3n m\xC3\xADnima"},
                    {"[COACH]", "Synth",        "200\xE2\x80\x93" "2 kHz",  "Textura oscura"}
                };
            } else if (g == "lofi") {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "60\xE2\x80\x93" "100 Hz",  "Groove relajado"},
                    {"[MUSIC]", "Bajo",         "60\xE2\x80\x93" "120 Hz",  "Cuerpo c\xC3\xA1lido"},
                    {"\xF0\x9F\x93\xA3", "Voz/Sample",   "2\xE2\x80\x93" "4 kHz",    "Textura nost\xC3\xA1lgica"},
                    {"[MUSIC]", "Teclados",     "200\xE2\x80\x93" "3 kHz",  "Atm\xC3\xB3sfera chill"},
                    {"\xF0\x9F\x93\xBC", "Vinilo Ruido", "100\xE2\x80\x93" "500 Hz",  "Warmth y textura"}
                };
            } else if (g == "classical") {
                roleFreqs = {
                    {"\xF0\x9F\x8E\xBC", "Cuerdas",      "200\xE2\x80\x93" "3 kHz",  "Calidez orquestal"},
                    {"\xF0\x9F\x8E\xB7", "Vientos",      "500\xE2\x80\x93" "2 kHz",  "Timbre mel\xC3\xB3" "dico"},
                    {"\xF0\x9F\x8E\xBA", "Bronces",      "2\xE2\x80\x93" "4 kHz",    "Presencia majestuosa"},
                    {"[DRUM]", "Percusi\xC3\xB3n", "50\xE2\x80\x93" "200 Hz",  "Base r\xC3\xADtmica"},
                    {"[MUSIC]", "Piano",        "100\xE2\x80\x93" "3 kHz",  "Armon\xC3\xAD" "a completa"}
                };
            } else if (g == "latino" || g == "latin") {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "60\xE2\x80\x93" "100 Hz",  "Base r\xC3\xADtmica"},
                    {"\xF0\x9F\x8E\xB6", "Bajo",         "100\xE2\x80\x93" "200 Hz", "Cuerpo latino"},
                    {"\xF0\x9F\x94\x8A", "Percusi\xC3\xB3n", "1\xE2\x80\x93" "3 kHz",  "Congas, bong\xC3\xB3s"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Presencia r\xC3\xADtmica"},
                    {"[MUSIC]", "Guitarra",     "1\xE2\x80\x93" "3 kHz",    "Ritmo y armon\xC3\xAD" "a"}
                };
            }

            // Si no hay datos para este g\xC3\xA9nero, usar perfil balanceado
            if (roleFreqs.empty()) {
                roleFreqs = {
                    {"[DRUM]", "Kick",         "50\xE2\x80\x93" "100 Hz",  "Fundamental del ritmo"},
                    {"[MUSIC]", "Bajo",         "60\xE2\x80\x93" "120 Hz",  "Base arm\xC3\xB3nica"},
                    {"\xF0\x9F\x93\xA3", "Voz",          "2\xE2\x80\x93" "4 kHz",    "Presencia principal"},
                    {"[MUSIC]", "Instrumentos", "200\xE2\x80\x93" "2 kHz",  "Relleno mel\xC3\xB3" "dico"},
                    {"[FIRE]", "Agudos",       "8\xE2\x80\x93" "12 kHz",   "Brillo y aire"}
                };
            }

            msg += "[TARGET] **Frecuencias clave por rol:**\n";
            for (const auto& rf : roleFreqs) {
                msg += juce::String(rf.icon) + " **" + juce::String(rf.role) + "** [RIGHT] "
                       + juce::String(rf.range) + " (" + juce::String(rf.note) + ")\n";
            }
            msg += "\n";
        }

        // ─── Tips adicionales según el género ─────────────────────────────
        if (hasReferenceTip) {
            msg += "\xF0\x9F\x92\xA1 Te recomiendo cargar una referencia de " + title
                   + " para afinar los detalles.\n";
        }

        msg += "[FIRE] \xC2\xA1" "Empecemos!\n";

        return msg;
    }

} // namespace mixcoach

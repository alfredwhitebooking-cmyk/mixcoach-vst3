#include "AiCoachAdapter.h"
#include "../../Common/types/LogHelper.h"
#include "../engine/MixScore.h"

namespace mixcoach {

    // ===========================================================================
    //  Constructor
    // ===========================================================================
    AiCoachAdapter::AiCoachAdapter(SharedData& sharedData,
                                   AudioAnalyzer& audioAnalyzer,
                                   CoachEngine& coachEngine,
                                   PhaseManager& phaseManager,
                                   LlmClient* /*llmClient*/) :
        sharedData_(sharedData),
        audioAnalyzer_(audioAnalyzer),
        coachEngine_(coachEngine),
        phaseManager_(phaseManager)
    {
        LogHelper::writeToLog("[AiCoachAdapter] Constructor");
        autoLoad();

        // ═══ Propagar nombre del ingeniero al CoachEngine después de cargar sesión ═══
        // Esto permite que startSetupDialogue() detecte usuarios recurrentes
        // y salte el onboarding antes de que se llame al timer.
        if (userProfile_.engineerName.isNotEmpty()) {
            coachEngine_.setEngineerName(userProfile_.engineerName);
            LogHelper::writeToLog("[AiCoachAdapter] Nombre propagado al CoachEngine: " + userProfile_.engineerName);
        }
    }

    // ===========================================================================
    //  saveSessionMemory - Save session state to JSON file
    // ===========================================================================
    void AiCoachAdapter::saveSessionMemory(const juce::File& file) const
    {
        try {
            auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
            root->setProperty("version", 2);
            root->setProperty("timestamp", juce::Time::getCurrentTime().toString(true, true, true, true));
            root->setProperty("genre", genre_);
            root->setProperty("experienceLevel", static_cast<int>(experienceLevel_));
            root->setProperty("engineerName", userProfile_.engineerName);
            root->setProperty("engineerNameDisplay", userProfile_.engineerName);
            root->setProperty("coachMode", static_cast<int>(coachEngine_.getCoachMode()));
            root->setProperty("masterDestination", static_cast<int>(coachEngine_.getMasterDestination()));
            root->setProperty("mentorPhase", static_cast<int>(phaseManager_.getCurrentPhase()));

            // Conversation history
            juce::Array<juce::var> convArr;
            for (const auto& turn : conversationHistory_) {
                auto turnObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                turnObj->setProperty("role", static_cast<int>(turn.role));
                turnObj->setProperty("timestampUs", static_cast<int64_t>(turn.timestampUs));
                turnObj->setProperty("message", turn.message);
                convArr.add(juce::var(turnObj));
            }
            root->setProperty("conversationHistory", convArr);

            // Session history
            juce::Array<juce::var> historyArr;
            for (const auto& change : sessionHistory_) {
                auto changeObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                changeObj->setProperty("timestampUs", static_cast<int64_t>(change.timestampUs));
                changeObj->setProperty("slotIndex", change.slotIndex);
                changeObj->setProperty("trackName", change.trackName);
                changeObj->setProperty("description", change.description);
                changeObj->setProperty("beforeValue", static_cast<double>(change.beforeValue));
                changeObj->setProperty("afterValue", static_cast<double>(change.afterValue));
                historyArr.add(juce::var(changeObj));
            }
            root->setProperty("sessionHistory", historyArr);

            // Track intents
            juce::Array<juce::var> intentsArr;
            for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
                if (trackIntents_[i].valid()) {
                    auto intentObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                    intentObj->setProperty("slotIndex", i);
                    intentObj->setProperty("role", trackIntents_[i].role);
                    intentObj->setProperty("style", trackIntents_[i].style);
                    intentsArr.add(juce::var(intentObj));
                }
            }
            root->setProperty("trackIntents", intentsArr);

            // SessionMap — mapa jerárquico persistente de la sesión
            auto sessionMapObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            coachEngine_.saveSessionMapToJson(*sessionMapObj);
            root->setProperty("sessionMap", juce::var(sessionMapObj));

            // DifferenceProfile — comparación unificada mix vs referencia
            if (coachEngine_.hasReferenceAudio()) {
                auto diffProfileObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                coachEngine_.saveDifferenceProfileToJson(*diffProfileObj);
                root->setProperty("differenceProfile", juce::var(diffProfileObj));
            }

            juce::var json(root);
            juce::String jsonStr = juce::JSON::toString(json, true);

            juce::FileOutputStream fos(file);
            if (fos.openedOk()) {
                fos.writeText(jsonStr, false, false, nullptr);
                fos.flush();
                LogHelper::writeToLog("[AiCoachAdapter] Session saved: " + file.getFullPathName());
            }
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[AiCoachAdapter] Error saving session: " + juce::String(e.what()));
        }
    }

    // ===========================================================================
    //  loadSessionMemory - Load session state from JSON file
    // ===========================================================================
    void AiCoachAdapter::loadSessionMemory(const juce::File& file)
    {
        try {
            if (!file.existsAsFile()) {
                LogHelper::writeToLog("[AiCoachAdapter] No previous session file: " + file.getFullPathName());
                return;
            }

            juce::FileInputStream fis(file);
            if (!fis.openedOk()) return;

            juce::String jsonStr = fis.readEntireStreamAsString();
            if (jsonStr.isEmpty()) return;

            auto json = juce::JSON::parse(jsonStr);
            if (!json.isObject()) return;

            auto root = json.getDynamicObject();
            if (root == nullptr) return;

            // Restore basic session config
            if (root->hasProperty("genre")) genre_ = root->getProperty("genre").toString();

            if (root->hasProperty("engineerName"))
                userProfile_.engineerName = root->getProperty("engineerName").toString();
            else if (root->hasProperty("engineerNameDisplay"))
                userProfile_.engineerName = root->getProperty("engineerNameDisplay").toString();

            // Restore experience level
            if (root->hasProperty("experienceLevel"))
                experienceLevel_ = static_cast<ExperienceLevel>(static_cast<int>(root->getProperty("experienceLevel")));

            // Restore coachMode and masterDestination (version >= 2)
            if (root->hasProperty("coachMode")) {
                auto mode = static_cast<CoachMode>(static_cast<int>(root->getProperty("coachMode")));
                coachEngine_.setCoachMode(mode);
            }
            if (root->hasProperty("masterDestination")) {
                auto dest = static_cast<MasterDestination>(static_cast<int>(root->getProperty("masterDestination")));
                coachEngine_.setMasterDestination(dest);
            }

            // Restore mentor phase (version >= 2)
            if (root->hasProperty("mentorPhase")) {
                auto phase = static_cast<MentorPhase>(static_cast<int>(root->getProperty("mentorPhase")));
                phaseManager_.setPhase(phase);
            }

            // Restore conversation history
            conversationHistory_.clear();
            if (root->hasProperty("conversationHistory")) {
                auto convArr = root->getProperty("conversationHistory").getArray();
                if (convArr != nullptr) {
                    for (int i = 0; i < convArr->size(); ++i) {
                        auto turnObj = (*convArr)[i].getDynamicObject();
                        if (turnObj == nullptr) continue;
                        ConversationTurn turn;
                        turn.role = static_cast<ConversationTurn::Role>(static_cast<int>(turnObj->getProperty("role")));
                        turn.timestampUs = turnObj->getProperty("timestampUs");
                        turn.message     = turnObj->getProperty("message").toString();
                        if ((int)conversationHistory_.size() < kMaxConversationTurns)
                            conversationHistory_.push_back(turn);
                    }
                }
            }

            // Restore session history
            sessionHistory_.clear();
            if (root->hasProperty("sessionHistory")) {
                auto historyArr = root->getProperty("sessionHistory").getArray();
                if (historyArr != nullptr) {
                    for (int i = 0; i < historyArr->size(); ++i) {
                        auto changeObj = (*historyArr)[i].getDynamicObject();
                        if (changeObj == nullptr) continue;
                        SessionChange change;
                        change.timestampUs = changeObj->getProperty("timestampUs");
                        change.slotIndex   = changeObj->getProperty("slotIndex");
                        change.trackName   = changeObj->getProperty("trackName").toString();
                        change.description = changeObj->getProperty("description").toString();
                        change.beforeValue =
                            static_cast<float>(static_cast<double>(changeObj->getProperty("beforeValue")));
                        change.afterValue =
                            static_cast<float>(static_cast<double>(changeObj->getProperty("afterValue")));
                        if ((int)sessionHistory_.size() < kMaxSessionChanges) sessionHistory_.push_back(change);
                    }
                }
            }

            // Restore SessionMap
            if (root->hasProperty("sessionMap")) {
                auto mapObj = root->getProperty("sessionMap").getDynamicObject();
                if (mapObj != nullptr) coachEngine_.loadSessionMapFromJson(*mapObj);
            }

            // Restore DifferenceProfile
            if (root->hasProperty("differenceProfile")) {
                auto dpObj = root->getProperty("differenceProfile").getDynamicObject();
                if (dpObj != nullptr) coachEngine_.loadDifferenceProfileFromJson(*dpObj);
            }

            // Restore track intents
            if (root->hasProperty("trackIntents")) {
                auto intentsArr = root->getProperty("trackIntents").getArray();
                if (intentsArr != nullptr) {
                    for (int i = 0; i < intentsArr->size(); ++i) {
                        auto intentObj = (*intentsArr)[i].getDynamicObject();
                        if (intentObj == nullptr) continue;
                        int slotIndex = intentObj->getProperty("slotIndex");
                        TrackIntent intent;
                        intent.role  = intentObj->getProperty("role").toString();
                        intent.style = intentObj->getProperty("style").toString();
                        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) trackIntents_[slotIndex] = intent;
                    }
                }
            }

            LogHelper::writeToLog("[AiCoachAdapter] Session loaded: " + file.getFullPathName());
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[AiCoachAdapter] Error loading session: " + juce::String(e.what()));
        }
    }

    // ===========================================================================
    //  Helper: directorio base para persistencia
    //  Windows: %LOCALAPPDATA%/MixCoach/ (no roam, ideal para cache de sesión)
    //  macOS:   ~/Library/Application Support/MixCoach/
    //  Linux:   ~/.local/share/MixCoach/
    // ===========================================================================
    static juce::File getMixCoachBaseDir()
    {
#ifdef _WIN32
        juce::String localAppData = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", "");
        if (localAppData.isNotEmpty()) return juce::File(localAppData).getChildFile("MixCoach");
#endif
        // Cross-platform fallback (JUCE mapea correctamente en cada SO)
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MixCoach");
    }

    // ===========================================================================
    //  Default file paths (static helpers)
    // ===========================================================================
    juce::File AiCoachAdapter::getDefaultSessionFile()
    {
        return getMixCoachBaseDir().getChildFile("session_state.json");
    }

    juce::File AiCoachAdapter::getDefaultProfileFile()
    {
        // ═══ V2: Guardar en Documents/MixCoach/ (visible para el usuario)
        // para que pueda ver/editar manualmente su perfil.
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach")
            .getChildFile("user_profile.json");
    }

    // ===========================================================================
    //  autoSave / autoLoad
    // ===========================================================================
    void AiCoachAdapter::autoSave()
    {
        // Crear directorios de persistencia
        getMixCoachBaseDir().createDirectory(); // %LOCALAPPDATA%/MixCoach/ (session state)
        getDefaultProfileFile().getParentDirectory().createDirectory(); // Documents/MixCoach/ (user profile)

        saveSessionMemory(getDefaultSessionFile());
        updateUserProfile();
        saveUserProfile(getDefaultProfileFile());
    }

    void AiCoachAdapter::autoLoad()
    {
        loadSessionMemory(getDefaultSessionFile());
        loadUserProfile(getDefaultProfileFile());

        // ═══ Sincronizar experienceLevel desde perfil cargado ═══════
        // Si el usuario editó user_profile.json manualmente, el cambio
        // en experienceLevel debe propagarse a AiCoachAdapter::experienceLevel_.
        if (userProfile_.valid && userProfile_.experienceLevel >= 1 && userProfile_.experienceLevel <= 4) {
            ExperienceLevel profileLevel = static_cast<ExperienceLevel>(userProfile_.experienceLevel - 1);
            if (profileLevel != experienceLevel_) {
                LogHelper::writeToLog("[AiCoachAdapter] Syncing experienceLevel from profile: "
                                      + juce::String(static_cast<int>(profileLevel))
                                      + " (was " + juce::String(static_cast<int>(experienceLevel_)) + ")");
                experienceLevel_ = profileLevel;
            }
        }
    }

    // ===========================================================================
    //  askLlm - Send a prompt to the LLM asynchronously
    // ===========================================================================
    void AiCoachAdapter::askLlm(const juce::String& userMessage,
                                std::function<void(bool, const juce::String&)> callback)
    {
        if (llmClient_ == nullptr) {
            if (callback) callback(false, "LLM not available");
            return;
        }

        // ═══ NO verificar isAvailable() aquí — el background thread del LlmClient
        // hará checkAvailability() automáticamente (processRequest hace retry
        // si available_ es false). Si retornáramos early acá, el request nunca
        // se encolaría y nunca se conectaría.

        // ═══ Usar buildChatContext() para modelos locales (Ollama, phi3, qwen)
        // buildChatContext() envía ~3KB de datos esenciales: session context,
        // snapshot del master, workflow events, track intents, reference gaps.
        // buildFullContext() (~8-12KB) es mejor para modelos cloud con gran
        // ventana de contexto (DeepSeek, GPT-4, Claude).
        juce::String system      = buildSystemPrompt();
        juce::String context     = buildChatContext();
        juce::String fullUserMsg = context + "\n[USER]\n" + userMessage;

        addConversationTurn(ConversationTurn::Role::User, userMessage);

        llmClient_->sendPrompt(
            system,
            fullUserMsg,
            [this, callback](bool success, const juce::String& response, const juce::String& error) {
                juce::String cleanedResponse = response;
                if (success) {
                    // Procesar comandos JSON del LLM (switch_tab, highlight_track, etc.)
                    if (commandInterpreter_ != nullptr) {
                        cleanedResponse = commandInterpreter_->processResponse(response);
                    }
                    addConversationTurn(ConversationTurn::Role::Assistant, cleanedResponse);
                }
                if (callback) callback(success, success ? cleanedResponse : error);
            });
    }

    // ===========================================================================
    //  askLlmStream - Send a prompt with streaming response
    // ===========================================================================
    void AiCoachAdapter::askLlmStream(const juce::String& userMessage,
                                      std::function<void(const juce::String& token)> onToken,
                                      std::function<void(bool success, const juce::String& response)> onComplete)
    {
        if (llmClient_ == nullptr) {
            if (onComplete) onComplete(false, "LLM not available");
            return;
        }

        // ═══ NO verificar isAvailable() aquí — el background thread del LlmClient
        // hará checkAvailability() automáticamente si no está disponible.

        // ═══ Usar buildChatContext() para modelos locales (Ollama, phi3, qwen)
        juce::String system      = buildSystemPrompt();
        juce::String context     = buildChatContext();
        juce::String fullUserMsg = context + "\n[USER]\n" + userMessage;

        addConversationTurn(ConversationTurn::Role::User, userMessage);

        llmClient_->sendPromptStream(system,
                                     fullUserMsg,
                                     onToken,
                                     [this, onComplete](const juce::String& fullResponse, const juce::String& error) {
                                         juce::String cleanedResponse = fullResponse;
                                         if (error.isEmpty()) {
                                             // Procesar comandos JSON del LLM
                                             if (commandInterpreter_ != nullptr) {
                                                 cleanedResponse = commandInterpreter_->processResponse(fullResponse);
                                             }
                                             addConversationTurn(ConversationTurn::Role::Assistant, cleanedResponse);
                                         }
                                         if (onComplete)
                                             onComplete(error.isEmpty(),
                                                        error.isEmpty() ? cleanedResponse : error);
                                     });
    }

    // ===========================================================================
    //  Knowledge Base - Load markdown knowledge files
    // ===========================================================================
    juce::String AiCoachAdapter::loadKnowledgeFile(const juce::String& relativePath)
    {
        if (relativePath.isEmpty()) return {};

        static juce::String s_knowledgeBasePath;
        static std::map<juce::String, juce::String> s_fileCache;

        if (s_knowledgeBasePath.isEmpty()) {
            static const juce::String kLocations[] = {
                juce::File::getCurrentWorkingDirectory().getChildFile("knowledge").getFullPathName(),
                juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                    .getParentDirectory()
                    .getChildFile("knowledge")
                    .getFullPathName(),
                "C:/Proyectos/MixCoach/knowledge"};

            for (auto& base : kLocations) {
                if (juce::File(base).isDirectory()) {
                    s_knowledgeBasePath = base;
                    break;
                }
            }
        }

        if (s_knowledgeBasePath.isEmpty()) return {};

        auto it = s_fileCache.find(relativePath);
        if (it != s_fileCache.end()) return it->second;

        juce::File file = juce::File(s_knowledgeBasePath).getChildFile(relativePath);
        if (!file.existsAsFile()) {
            s_fileCache[relativePath] = {};
            return {};
        }

        juce::FileInputStream fis(file);
        if (!fis.openedOk()) return {};

        juce::String content = fis.readEntireStreamAsString();
        LogHelper::writeToLog("[Knowledge] Loaded: " + relativePath);
        s_fileCache[relativePath] = content;
        return content;
    }

    juce::String AiCoachAdapter::loadKnowledgeSection(const juce::String& relativePath,
                                                      const juce::String& sectionLabel)
    {
        juce::String fullContent = loadKnowledgeFile(relativePath);
        if (fullContent.isEmpty() || sectionLabel.isEmpty()) return fullContent;

        int startIdx = fullContent.indexOf(sectionLabel);
        if (startIdx < 0) return fullContent;

        int lineStart = startIdx;
        while (lineStart > 0 && fullContent[lineStart - 1] != '\n') --lineStart;

        // Find next section header ("## ") manually to avoid indexOf overload ambiguity
        int searchFrom  = startIdx + sectionLabel.length();
        int nextSection = -1;
        for (int si = searchFrom; si < fullContent.length() - 2; ++si) {
            if (fullContent[si] == '#' && fullContent[si + 1] == '#' && fullContent[si + 2] == ' ') {
                nextSection = si;
                break;
            }
        }
        if (nextSection > lineStart) return fullContent.substring(lineStart, nextSection);

        return fullContent.substring(lineStart);
    }

    // ===========================================================================
    //  UserProfile methods
    // ===========================================================================
    juce::String AiCoachAdapter::UserProfile::toProfileContext() const
    {
        if (!valid) return {};

        juce::String s;
        s += "[USER PROFILE]\n";
        s += "  Sessions: " + juce::String(sessionCount) + "\n";

        // Nombre del ingeniero
        if (engineerName.isNotEmpty()) {
            s += "  Name: " + engineerName + "\n";
        }

        // Género más usado
        if (mostUsedGenre.isNotEmpty() && mostUsedGenre != "Unknown") {
            s += "  Most used genre: " + mostUsedGenre + "\n";
        }

        // Tono preferido
        if (preferredTone.isNotEmpty()) {
            s += "  Preferred tone: " + preferredTone + "\n";
        }

        // Géneros favoritos
        if (favoriteGenres.size() > 0) {
            s += "  Favorite genres: " + favoriteGenres.joinIntoString(", ") + "\n";
        }

        // Plugins favoritos
        if (favoritePlugins.size() > 0) {
            s += "  Favorite plugins: " + favoritePlugins.joinIntoString(", ") + "\n";
        }

        // Nivel de experiencia
        if (experienceLevel >= 1 && experienceLevel <= 4) {
            static const char* levelNames[] = {"Novice", "Intermediate", "Advanced", "Expert"};
            s += "  Level: " + juce::String(levelNames[experienceLevel - 1]) + "\n";
        }

        // Última sesión
        if (lastSessionDate.isNotEmpty()) {
            s += "  Last session: " + lastSessionDate + "\n";
        }

        // ═══ Session History — datos de la última sesión ═══════════════════
        if (lastSessionDurationS > 0 || lastSessionProblemsDetected > 0
            || lastSessionReferenceMatchPct > 0 || lastSessionGenre.isNotEmpty()) {
            s += "  [LAST SESSION SUMMARY]\n";
            if (lastSessionDurationS > 0) {
                int hours = lastSessionDurationS / 3600;
                int mins  = (lastSessionDurationS % 3600) / 60;
                if (hours > 0)
                    s += "    Duration: " + juce::String(hours) + "h " + juce::String(mins) + "m\n";
                else
                    s += "    Duration: " + juce::String(mins) + "m\n";
            }
            if (lastSessionGenre.isNotEmpty())
                s += "    Genre: " + lastSessionGenre + "\n";
            if (lastSessionProblemsDetected > 0) {
                s += "    Problems: " + juce::String(lastSessionProblemsDetected) + " detected, "
                     + juce::String(lastSessionProblemsResolved) + " resolved ("
                     + juce::String(lastSessionProblemsResolved * 100 / lastSessionProblemsDetected)
                     + "% resolution rate)\n";
            }
            if (lastSessionReferenceMatchPct > 0) {
                s += "    Reference match: " + juce::String(lastSessionReferenceMatchPct) + "%\n";
            }
            if (lastSessionMixScore > 0) {
                s += "    MixScore: " + juce::String(lastSessionMixScore) + "/100\n";
            }
            s += "\n";
        }

        // ═══ Accumulated Statistics ═══════════════════════════════════════
        if (sessionCount >= 3) {
            s += "  [ACCUMULATED STATISTICS]\n";
            s += "    Total sessions completed: " + juce::String(sessionCount) + "\n";
            if (totalProblemsResolved > 0)
                s += "    Total problems resolved across all sessions: " + juce::String(totalProblemsResolved) + "\n";
            if (bestReferenceMatchPct > 0)
                s += "    Best reference match: " + juce::String(bestReferenceMatchPct) + "%\n";
            if (mostUsedGenre.isNotEmpty())
                s += "    Favorite genre to mix: " + mostUsedGenre + "\n";
            s += "\n";
        }

        // Perfil de comportamiento (solo si hay suficientes datos)
        if (behavior.totalSessions >= 3) {
            s += "  Behavior:\n";
            if (behavior.tendsToOverApply)
                s += "    - Tends to over-apply corrections (applies more than recommended)\n";
            if (behavior.tendsToIgnore)
                s += "    - Tends to ignore some recommendations\n";
            if (behavior.averageCorrectionTime > 0.0f) {
                int secs = static_cast<int>(behavior.averageCorrectionTime);
                if (secs < 60)
                    s += "    - Average apply time: " + juce::String(secs) + "s\n";
                else
                    s += "    - Average apply time: " + juce::String(secs / 60) + "min " + juce::String(secs % 60) + "s\n";
            }
        }

        return s;
    }

    juce::String AiCoachAdapter::buildUserProfileContext() const
    {
        return userProfile_.toProfileContext();
    }

    /** Formatea duración de sesión en "Xh Ym" o "Ym Zs" según duración. */
    static juce::String formatSessionDuration(int totalSeconds) noexcept
    {
        if (totalSeconds <= 0) return "<1min";
        int hours   = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;
        if (hours > 0)
            return juce::String(hours) + "h " + juce::String(minutes) + "m";
        if (minutes > 0)
            return juce::String(minutes) + "m " + juce::String(seconds) + "s";
        return juce::String(seconds) + "s";
    }

    void AiCoachAdapter::updateUserProfile()
    {
        // Sincronizar nivel de experiencia desde el adapter
        userProfile_.experienceLevel = static_cast<int>(experienceLevel_) + 1;

        // Tono preferido por defecto si no se ha configurado
        if (userProfile_.preferredTone.isEmpty()) {
            // Detectar según nivel de experiencia: principiantes reciben tono motivador,
            // expertos reciben tono técnico
            switch (experienceLevel_) {
                case ExperienceLevel::Novice:
                case ExperienceLevel::Intermediate:
                    userProfile_.preferredTone = "motivador";
                    break;
                case ExperienceLevel::Advanced:
                    userProfile_.preferredTone = "t\xC3\xA9" "cnico";
                    break;
                case ExperienceLevel::Expert:
                    userProfile_.preferredTone = "directo";
                    break;
            }
        }

        userProfile_.sessionCount++;
        userProfile_.valid = true;

        // Timestamp de última sesión
        userProfile_.lastSessionDate = juce::Time::getCurrentTime().toString(false, false, false, false);

        // ═══ Capturar métricas de la sesión desde CoachEngine ═══════════════
        {
            // Duración de la sesión (desde SessionContext)
            auto ctx = coachEngine_.buildSessionContext();
            userProfile_.lastSessionDurationS = static_cast<int>(ctx.sessionDurationUs / 1000000);

            // Problemas detectados vs resueltos
            userProfile_.lastSessionProblemsDetected = ctx.pendingCorrections + ctx.appliedCorrections;
            userProfile_.lastSessionProblemsResolved = ctx.appliedCorrections;
            userProfile_.totalProblemsResolved += ctx.appliedCorrections;

            // Reference match %
            if (coachEngine_.hasReferenceAudio()) {
                auto refProgress = coachEngine_.getReferenceProgress();
                int matchPct = static_cast<int>(refProgress.currentMatch * 100.0f);
                userProfile_.lastSessionReferenceMatchPct = matchPct;
                if (matchPct > userProfile_.bestReferenceMatchPct)
                    userProfile_.bestReferenceMatchPct = matchPct;
            }

            // Género trabajado
            if (genre_.isNotEmpty() && genre_ != "Unknown")
                userProfile_.lastSessionGenre = genre_;

            // MixScore — si está disponible, cómputo estático
            // MixScore solo disponible en ciertas fases, no hardcodear
            {
                auto ms = MixScore::compute(coachEngine_, audioAnalyzer_, genre_);
                if (ms.overall > 0)
                    userProfile_.lastSessionMixScore = ms.overall;
            }

            // Resumen textual de la sesión
            {
                juce::String summary;
                summary += "Sesi\xC3\xB3n #" + juce::String(userProfile_.sessionCount);
                summary += " | Duraci\xC3\xB3n: " + formatSessionDuration(userProfile_.lastSessionDurationS);
                if (userProfile_.lastSessionGenre.isNotEmpty())
                    summary += " | G\xC3\xA9" "nero: " + userProfile_.lastSessionGenre;
                summary += " | Problemas: " + juce::String(userProfile_.lastSessionProblemsDetected)
                           + " detectados, " + juce::String(userProfile_.lastSessionProblemsResolved) + " resueltos";
                if (userProfile_.lastSessionReferenceMatchPct > 0)
                    summary += " | Match ref: " + juce::String(userProfile_.lastSessionReferenceMatchPct) + "%";
                if (userProfile_.lastSessionMixScore > 0)
                    summary += " | MixScore: " + juce::String(userProfile_.lastSessionMixScore);
                userProfile_.lastSessionSummary = summary;
            }
        }

        if (genre_.isNotEmpty() && genre_ != "Unknown") {
            userProfile_.genreFrequency[genre_]++;
            int maxCount = 0;
            for (const auto& [genre, count] : userProfile_.genreFrequency) {
                if (count > maxCount) {
                    maxCount                   = count;
                    userProfile_.mostUsedGenre = genre;
                }
            }

            // Agregar a favoriteGenres si no está ya
            if (userProfile_.favoriteGenres.size() < 5) {
                bool found = false;
                for (const auto& g : userProfile_.favoriteGenres) {
                    if (g == genre_) { found = true; break; }
                }
                if (!found && genre_ != "Unknown")
                    userProfile_.favoriteGenres.add(genre_);
            }
        }

        // Sincronizar totalSessions del behavior con sessionCount
        userProfile_.behavior.totalSessions = userProfile_.sessionCount;

        LogHelper::writeToLog("[AiCoachAdapter] User profile updated: " + juce::String(userProfile_.sessionCount)
                              + " sessions, level=" + juce::String(userProfile_.experienceLevel)
                              + ", lastSession='" + userProfile_.lastSessionSummary.substring(0, 80) + "'");
    }

    void AiCoachAdapter::saveUserProfile(const juce::File& file) const
    {
        try {
            auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
            root->setProperty("version", 2);
            root->setProperty("engineerName", userProfile_.engineerName);
            root->setProperty("preferredTone", userProfile_.preferredTone);
            root->setProperty("sessionCount", userProfile_.sessionCount);
            root->setProperty("lastSessionDate", userProfile_.lastSessionDate);
            root->setProperty("mostUsedGenre", userProfile_.mostUsedGenre);
            root->setProperty("valid", userProfile_.valid);
            root->setProperty("experienceLevel", userProfile_.experienceLevel);

            // Genre frequency
            juce::Array<juce::var> genreEntries;
            for (const auto& [genre, count] : userProfile_.genreFrequency) {
                auto entry = juce::DynamicObject::Ptr(new juce::DynamicObject());
                entry->setProperty("genre", genre);
                entry->setProperty("count", count);
                genreEntries.add(juce::var(entry));
            }
            root->setProperty("genreFrequency", genreEntries);

            // Favorite genres (V2)
            if (userProfile_.favoriteGenres.size() > 0) {
                juce::Array<juce::var> favGenres;
                for (const auto& g : userProfile_.favoriteGenres)
                    favGenres.add(juce::var(g));
                root->setProperty("favoriteGenres", favGenres);
            }

            // Favorite plugins (V2)
            if (userProfile_.favoritePlugins.size() > 0) {
                juce::Array<juce::var> favPlugins;
                for (const auto& p : userProfile_.favoritePlugins)
                    favPlugins.add(juce::var(p));
                root->setProperty("favoritePlugins", favPlugins);
            }

            // Behavior profile (V2)
            {
                auto behaviorObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                behaviorObj->setProperty("tendsToOverApply", userProfile_.behavior.tendsToOverApply);
                behaviorObj->setProperty("tendsToIgnore", userProfile_.behavior.tendsToIgnore);
                behaviorObj->setProperty("averageCorrectionTime", static_cast<double>(userProfile_.behavior.averageCorrectionTime));
                behaviorObj->setProperty("totalSessions", userProfile_.behavior.totalSessions);
                root->setProperty("behaviorProfile", juce::var(behaviorObj));
            }

            // ═══ AdaptiveThresholds (V4) — persistir thresholds adaptativos para restaurar entre sesiones ═══
            {
                auto at = coachEngine_.getAdaptiveThresholds();
                auto atObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
                atObj->setProperty("underApplyRatio", static_cast<double>(at.underApplyRatio));
                atObj->setProperty("goodStartRatio", static_cast<double>(at.goodStartRatio));
                atObj->setProperty("underApplyTarget", static_cast<double>(at.underApplyTarget));
                atObj->setProperty("overApplyTarget", static_cast<double>(at.overApplyTarget));
                atObj->setProperty("maxRetriesBeforeIgnore", at.maxRetriesBeforeIgnore);
                root->setProperty("adaptiveThresholds", juce::var(atObj));
            }

            // ═══ Session History fields (V3) ═══════════════════════════════
            root->setProperty("lastSessionDurationS", userProfile_.lastSessionDurationS);
            root->setProperty("lastSessionProblemsDetected", userProfile_.lastSessionProblemsDetected);
            root->setProperty("lastSessionProblemsResolved", userProfile_.lastSessionProblemsResolved);
            root->setProperty("lastSessionReferenceMatchPct", userProfile_.lastSessionReferenceMatchPct);
            root->setProperty("lastSessionMixScore", userProfile_.lastSessionMixScore);
            if (userProfile_.lastSessionGenre.isNotEmpty())
                root->setProperty("lastSessionGenre", userProfile_.lastSessionGenre);
            root->setProperty("totalProblemsResolved", userProfile_.totalProblemsResolved);
            root->setProperty("bestReferenceMatchPct", userProfile_.bestReferenceMatchPct);
            if (userProfile_.lastSessionSummary.isNotEmpty())
                root->setProperty("lastSessionSummary", userProfile_.lastSessionSummary);

            juce::var json(root);
            juce::String jsonStr = juce::JSON::toString(json, true);

            juce::FileOutputStream fos(file);
            if (fos.openedOk()) {
                fos.writeText(jsonStr, false, false, nullptr);
                fos.flush();
            }
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[AiCoachAdapter] Error saving profile: " + juce::String(e.what()));
        }
    }

    void AiCoachAdapter::loadUserProfile(const juce::File& file)
    {
        try {
            if (!file.existsAsFile()) return;

            juce::FileInputStream fis(file);
            if (!fis.openedOk()) return;

            juce::String jsonStr = fis.readEntireStreamAsString();
            if (jsonStr.isEmpty()) return;

            auto json = juce::JSON::parse(jsonStr);
            if (!json.isObject()) return;

            auto root = json.getDynamicObject();
            if (root == nullptr) return;

            // ─── Common fields (V1 + V2) ───────────────────────────────────
            if (root->hasProperty("engineerName"))
                userProfile_.engineerName = root->getProperty("engineerName").toString();
            if (root->hasProperty("sessionCount")) userProfile_.sessionCount = root->getProperty("sessionCount");
            if (root->hasProperty("mostUsedGenre"))
                userProfile_.mostUsedGenre = root->getProperty("mostUsedGenre").toString();
            if (root->hasProperty("valid")) userProfile_.valid = root->getProperty("valid");
            if (root->hasProperty("lastSessionDate"))
                userProfile_.lastSessionDate = root->getProperty("lastSessionDate").toString();

            // ═══ V2: New rich profile fields ─────────────────────────────────
            if (root->hasProperty("preferredTone"))
                userProfile_.preferredTone = root->getProperty("preferredTone").toString();
            if (root->hasProperty("experienceLevel"))
                userProfile_.experienceLevel = static_cast<int>(root->getProperty("experienceLevel"));

            // Favorite genres (V2)
            if (root->hasProperty("favoriteGenres")) {
                auto arr = root->getProperty("favoriteGenres").getArray();
                if (arr != nullptr) {
                    userProfile_.favoriteGenres.clear();
                    for (int i = 0; i < arr->size(); ++i) {
                        userProfile_.favoriteGenres.add((*arr)[i].toString());
                    }
                }
            }

            // Favorite plugins (V2)
            if (root->hasProperty("favoritePlugins")) {
                auto arr = root->getProperty("favoritePlugins").getArray();
                if (arr != nullptr) {
                    userProfile_.favoritePlugins.clear();
                    for (int i = 0; i < arr->size(); ++i) {
                        userProfile_.favoritePlugins.add((*arr)[i].toString());
                    }
                }
            }

            // Behavior profile (V2)
            if (root->hasProperty("behaviorProfile")) {
                auto bpObj = root->getProperty("behaviorProfile").getDynamicObject();
                if (bpObj != nullptr) {
                    if (bpObj->hasProperty("tendsToOverApply"))
                        userProfile_.behavior.tendsToOverApply = bpObj->getProperty("tendsToOverApply");
                    if (bpObj->hasProperty("tendsToIgnore"))
                        userProfile_.behavior.tendsToIgnore = bpObj->getProperty("tendsToIgnore");
                    if (bpObj->hasProperty("averageCorrectionTime"))
                        userProfile_.behavior.averageCorrectionTime =
                            static_cast<float>(static_cast<double>(bpObj->getProperty("averageCorrectionTime")));
                    if (bpObj->hasProperty("totalSessions"))
                        userProfile_.behavior.totalSessions = bpObj->getProperty("totalSessions");
                }
            }

            // ═══ AdaptiveThresholds (V4) — restaurar thresholds adaptativos ═══
            if (root->hasProperty("adaptiveThresholds")) {
                auto atObj = root->getProperty("adaptiveThresholds").getDynamicObject();
                if (atObj != nullptr) {
                    CoachEngine::AdaptiveThresholds at;
                    if (atObj->hasProperty("underApplyRatio"))
                        at.underApplyRatio = static_cast<float>(static_cast<double>(atObj->getProperty("underApplyRatio")));
                    if (atObj->hasProperty("goodStartRatio"))
                        at.goodStartRatio = static_cast<float>(static_cast<double>(atObj->getProperty("goodStartRatio")));
                    if (atObj->hasProperty("underApplyTarget"))
                        at.underApplyTarget = static_cast<float>(static_cast<double>(atObj->getProperty("underApplyTarget")));
                    if (atObj->hasProperty("overApplyTarget"))
                        at.overApplyTarget = static_cast<float>(static_cast<double>(atObj->getProperty("overApplyTarget")));
                    if (atObj->hasProperty("maxRetriesBeforeIgnore"))
                        at.maxRetriesBeforeIgnore = static_cast<int>(atObj->getProperty("maxRetriesBeforeIgnore"));
                    coachEngine_.restoreAdaptiveThresholds(at);
                    LogHelper::writeToLog("[AiCoachAdapter] AdaptiveThresholds restored from profile");
                }
            }

            // ═══ Session History fields (V3) ═══════════════════════════════
            if (root->hasProperty("lastSessionDurationS"))
                userProfile_.lastSessionDurationS = static_cast<int>(root->getProperty("lastSessionDurationS"));
            if (root->hasProperty("lastSessionProblemsDetected"))
                userProfile_.lastSessionProblemsDetected = static_cast<int>(root->getProperty("lastSessionProblemsDetected"));
            if (root->hasProperty("lastSessionProblemsResolved"))
                userProfile_.lastSessionProblemsResolved = static_cast<int>(root->getProperty("lastSessionProblemsResolved"));
            if (root->hasProperty("lastSessionReferenceMatchPct"))
                userProfile_.lastSessionReferenceMatchPct = static_cast<int>(root->getProperty("lastSessionReferenceMatchPct"));
            if (root->hasProperty("lastSessionMixScore"))
                userProfile_.lastSessionMixScore = static_cast<int>(root->getProperty("lastSessionMixScore"));
            if (root->hasProperty("lastSessionGenre"))
                userProfile_.lastSessionGenre = root->getProperty("lastSessionGenre").toString();
            if (root->hasProperty("totalProblemsResolved"))
                userProfile_.totalProblemsResolved = static_cast<int>(root->getProperty("totalProblemsResolved"));
            if (root->hasProperty("bestReferenceMatchPct"))
                userProfile_.bestReferenceMatchPct = static_cast<int>(root->getProperty("bestReferenceMatchPct"));
            if (root->hasProperty("lastSessionSummary"))
                userProfile_.lastSessionSummary = root->getProperty("lastSessionSummary").toString();

            // Genre frequency
            if (root->hasProperty("genreFrequency")) {
                auto arr = root->getProperty("genreFrequency").getArray();
                if (arr != nullptr) {
                    for (int i = 0; i < arr->size(); ++i) {
                        auto entry = (*arr)[i].getDynamicObject();
                        if (entry == nullptr) continue;
                        juce::String genre = entry->getProperty("genre").toString();
                        int count = entry->getProperty("count");
                        userProfile_.genreFrequency[genre] = count;
                    }
                }
            }

            LogHelper::writeToLog("[AiCoachAdapter] User profile loaded: " + file.getFullPathName()
                                  + " (" + juce::String(userProfile_.sessionCount) + " sessions"
                                  + ", V2 fields: " + juce::String(userProfile_.preferredTone.isNotEmpty() ? "tone " : "")
                                  + juce::String(userProfile_.favoriteGenres.size() > 0 ? "genres " : "")
                                  + ")");
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[AiCoachAdapter] Error loading profile: " + juce::String(e.what()));
        }
    }

    // ===========================================================================
    //  Utility functions
    // ===========================================================================
    juce::String AiCoachAdapter::formatDb(float value) noexcept
    {
        if (value < -90.0f) return "-inf";
        return juce::String(value, 1) + " dB";
    }

    juce::String AiCoachAdapter::formatLUFS(float value) noexcept
    {
        if (value < -90.0f) return "-inf";
        return juce::String(value, 1) + " LUFS";
    }

    juce::String AiCoachAdapter::bandLabel(int bandIndex) noexcept
    {
        static const char* labels[] = {"Sub (0-86Hz)",
                                       "Bass (86-301Hz)",
                                       "Low-Mid (301-1076Hz)",
                                       "High-Mid (1076-3532Hz)",
                                       "Presence (3532-8355Hz)",
                                       "Air (8355-16458Hz)"};
        if (bandIndex >= 0 && bandIndex < 6) return juce::String(labels[bandIndex]);
        return "Band " + juce::String(bandIndex);
    }

    juce::String AiCoachAdapter::busLabel(BusType bus) noexcept
    {
        switch (bus) {
            case BusType::Drums:
                return "Drums";
            case BusType::Bass:
                return "Bass";
            case BusType::Guitars:
                return "Guitars";
            case BusType::Keys:
                return "Keys";
            case BusType::Vocals:
                return "Vocals";
            case BusType::FX:
                return "FX";
            case BusType::Melody:
                return "Melody";
            default:
                return "None";
        }
    }

    juce::String AiCoachAdapter::phaseLabel(MentorPhase phase) noexcept
    {
        switch (phase) {
            case MentorPhase::Organizacion:
                return "Organización";
            case MentorPhase::GainStaging:
                return "Gain Staging";
            case MentorPhase::Balance:
                return "Balance";
            case MentorPhase::EQ:
                return "EQ";
            case MentorPhase::Compresion:
                return "Compresión";
            case MentorPhase::Espacio:
                return "Espacio";
            case MentorPhase::MasterCheck:
                return "Master Check";
            default:
                return "Desconocida";
        }
    }

    juce::String AiCoachAdapter::suggestionStatusEmoji(SuggestionStatus status) noexcept
    {
        switch (status) {
            case SuggestionStatus::Green:
                return "\xf0\x9f\x9f\xa2"; // green
            case SuggestionStatus::Yellow:
                return "\xf0\x9f\x9f\xa1"; // yellow
            case SuggestionStatus::Red:
                return "\xf0\x9f\x94\xb4"; // red
            case SuggestionStatus::White:
                return "\xe2\x9a\xaa"; // white
            default:
                return "\xe2\x9a\xab"; // gray
        }
    }

    const char* AiCoachAdapter::experienceLevelName(ExperienceLevel level) noexcept
    {
        switch (level) {
            case ExperienceLevel::Novice:
                return "Novice";
            case ExperienceLevel::Intermediate:
                return "Intermediate";
            case ExperienceLevel::Advanced:
                return "Advanced";
            case ExperienceLevel::Expert:
                return "Expert";
            default:
                return "Unknown";
        }
    }

    const AiCoachAdapter::TrackIntent& AiCoachAdapter::getTrackIntent(int slotIndex) const
    {
        static TrackIntent defaultIntent;
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) return trackIntents_[slotIndex];
        return defaultIntent;
    }

    // ===========================================================================
    //  Core setters & state management
    // ===========================================================================

    void AiCoachAdapter::setGenre(const juce::String& genre)
    {
        if (genre.isNotEmpty()) genre_ = genre;
    }

    void AiCoachAdapter::setTrackIntent(int slotIndex, const TrackIntent& intent)
    {
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) trackIntents_[slotIndex] = intent;
    }

    void AiCoachAdapter::addConversationTurn(ConversationTurn::Role role, const juce::String& message)
    {
        ConversationTurn turn;
        turn.role        = role;
        turn.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        turn.message     = message;

        conversationHistory_.push_back(turn);

        // Prune to max turns
        while ((int)conversationHistory_.size() > kMaxConversationTurns)
            conversationHistory_.erase(conversationHistory_.begin());
    }

    juce::String AiCoachAdapter::buildConversationHistory() const
    {
        if (conversationHistory_.empty()) return {};

        juce::String s;
        s += "[CONVERSATION HISTORY]\n";
        int start = std::max(0, (int)conversationHistory_.size() - 20);

        for (int i = start; i < (int)conversationHistory_.size(); ++i) {
            const auto& turn    = conversationHistory_[i];
            const char* roleStr = (turn.role == ConversationTurn::Role::User) ? "User" : "Assistant";
            s += "  [" + juce::String(roleStr) + "] " + turn.message + "\n";
        }
        s += "\n";
        return s;
    }

    juce::String AiCoachAdapter::buildSessionHistoryString() const
    {
        if (sessionHistory_.empty()) return {};

        juce::String s;
        s += "[SESSION HISTORY]\n";
        int count = 0;
        for (const auto& change : sessionHistory_) {
            if (count >= 20) break;
            s += "  " + change.trackName + ": " + change.description + "\n";
            ++count;
        }
        s += "\n";
        return s;
    }

    void AiCoachAdapter::recordChange(int slotIndex,
                                      const juce::String& trackName,
                                      const juce::String& description,
                                      float beforeValue,
                                      float afterValue)
    {
        SessionChange change;
        change.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        change.slotIndex   = slotIndex;
        change.trackName   = trackName;
        change.description = description;
        change.beforeValue = beforeValue;
        change.afterValue  = afterValue;

        sessionHistory_.push_back(change);

        while ((int)sessionHistory_.size() > kMaxSessionChanges) sessionHistory_.erase(sessionHistory_.begin());
    }

    void AiCoachAdapter::resetSession()
    {
        sessionHistory_.clear();
        conversationHistory_.clear();
        genre_ = "Unknown";
        trackIntents_.fill(TrackIntent{});
    }

    // ===========================================================================
    //  loadSessionHistory - Load multi-session history from persistent file
    // ===========================================================================
    std::vector<AiCoachAdapter::SessionSnapshotEntry> AiCoachAdapter::loadSessionHistory()
    {
        std::vector<SessionSnapshotEntry> result;

        auto file = getDefaultSessionFile();
        if (!file.existsAsFile()) return result;

        try {
            juce::FileInputStream fis(file);
            if (!fis.openedOk()) return result;

            juce::String jsonStr = fis.readEntireStreamAsString();
            if (jsonStr.isEmpty()) return result;

            auto json = juce::JSON::parse(jsonStr);
            if (!json.isObject()) return result;

            auto root = json.getDynamicObject();
            if (root == nullptr) return result;

            // Try to extract session snapshot data
            // Check for differenceProfile which may contain session scores
            if (root->hasProperty("differenceProfile")) {
                auto dpObj = root->getProperty("differenceProfile").getDynamicObject();
                if (dpObj != nullptr) {
                    SessionSnapshotEntry snap;
                    snap.timestampUs = juce::Time::currentTimeMillis() * 1000;
                    snap.sessionNumber = 1;

                    if (dpObj->hasProperty("mixScoreOverall"))
                        snap.mixScoreOverall = static_cast<int>(dpObj->getProperty("mixScoreOverall"));
                    if (dpObj->hasProperty("domainGain"))
                        snap.domainGain = static_cast<int>(dpObj->getProperty("domainGain"));
                    if (dpObj->hasProperty("domainTonal"))
                        snap.domainTonal = static_cast<int>(dpObj->getProperty("domainTonal"));
                    if (dpObj->hasProperty("domainDynamics"))
                        snap.domainDynamics = static_cast<int>(dpObj->getProperty("domainDynamics"));
                    if (dpObj->hasProperty("domainSpatial"))
                        snap.domainSpatial = static_cast<int>(dpObj->getProperty("domainSpatial"));
                    if (dpObj->hasProperty("domainReference"))
                        snap.domainReference = static_cast<int>(dpObj->getProperty("domainReference"));

                    result.push_back(snap);
                }
            }

            // If no differenceProfile data, create a simple entry from the session metadata
            if (result.empty()) {
                SessionSnapshotEntry snap;
                snap.timestampUs = juce::Time::currentTimeMillis() * 1000;
                snap.sessionNumber = 1;
                result.push_back(snap);
            }
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[AiCoachAdapter] Error loading session history: " + juce::String(e.what()));
        }

        return result;
    }

} // namespace mixcoach

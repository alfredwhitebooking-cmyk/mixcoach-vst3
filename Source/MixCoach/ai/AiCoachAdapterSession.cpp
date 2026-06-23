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
        return getMixCoachBaseDir().getChildFile("user_profile.json");
    }

    // ===========================================================================
    //  autoSave / autoLoad
    // ===========================================================================
    void AiCoachAdapter::autoSave()
    {
        // Crear directorio una sola vez (la primera llamada a createDirectory
        // crea la carpeta; las siguientes son no-op rápidas del OS).
        getMixCoachBaseDir().createDirectory();

        saveSessionMemory(getDefaultSessionFile());
        updateUserProfile();
        saveUserProfile(getDefaultProfileFile());
    }

    void AiCoachAdapter::autoLoad()
    {
        loadSessionMemory(getDefaultSessionFile());
        loadUserProfile(getDefaultProfileFile());
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
            [this, callback, userMessage](bool success, const juce::String& response, const juce::String& error) {
                if (success) {
                    addConversationTurn(ConversationTurn::Role::Assistant, response);
                }
                if (callback) callback(success, success ? response : error);
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
                                         if (error.isEmpty()) {
                                             addConversationTurn(ConversationTurn::Role::Assistant, fullResponse);
                                         }
                                         if (onComplete)
                                             onComplete(error.isEmpty(), error.isEmpty() ? fullResponse : error);
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
        s += "  Most used genre: " + mostUsedGenre + "\n";
        return s;
    }

    juce::String AiCoachAdapter::buildUserProfileContext() const
    {
        return userProfile_.toProfileContext();
    }

    void AiCoachAdapter::updateUserProfile()
    {
        userProfile_.sessionCount++;
        userProfile_.valid = true;

        if (genre_.isNotEmpty() && genre_ != "Unknown") {
            userProfile_.genreFrequency[genre_]++;
            int maxCount = 0;
            for (const auto& [genre, count] : userProfile_.genreFrequency) {
                if (count > maxCount) {
                    maxCount                   = count;
                    userProfile_.mostUsedGenre = genre;
                }
            }
        }

        LogHelper::writeToLog("[AiCoachAdapter] User profile updated: " + juce::String(userProfile_.sessionCount)
                              + " sessions");
    }

    void AiCoachAdapter::saveUserProfile(const juce::File& file) const
    {
        try {
            auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
            root->setProperty("sessionCount", userProfile_.sessionCount);
            root->setProperty("mostUsedGenre", userProfile_.mostUsedGenre);
            root->setProperty("valid", userProfile_.valid);
            root->setProperty("engineerName", userProfile_.engineerName);

            juce::Array<juce::var> genreEntries;
            for (const auto& [genre, count] : userProfile_.genreFrequency) {
                auto entry = juce::DynamicObject::Ptr(new juce::DynamicObject());
                entry->setProperty("genre", genre);
                entry->setProperty("count", count);
                genreEntries.add(juce::var(entry));
            }
            root->setProperty("genreFrequency", genreEntries);

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

            if (root->hasProperty("sessionCount")) userProfile_.sessionCount = root->getProperty("sessionCount");
            if (root->hasProperty("mostUsedGenre"))
                userProfile_.mostUsedGenre = root->getProperty("mostUsedGenre").toString();
            if (root->hasProperty("valid")) userProfile_.valid = root->getProperty("valid");
            if (root->hasProperty("engineerName"))
                userProfile_.engineerName = root->getProperty("engineerName").toString();

            if (root->hasProperty("genreFrequency")) {
                auto arr = root->getProperty("genreFrequency").getArray();
                if (arr != nullptr) {
                    for (int i = 0; i < arr->size(); ++i) {
                        auto entry = (*arr)[i].getDynamicObject();
                        if (entry == nullptr) continue;
                        juce::String genre                 = entry->getProperty("genre").toString();
                        int count                          = entry->getProperty("count");
                        userProfile_.genreFrequency[genre] = count;
                    }
                }
            }

            LogHelper::writeToLog("[AiCoachAdapter] User profile loaded: " + file.getFullPathName());
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

} // namespace mixcoach

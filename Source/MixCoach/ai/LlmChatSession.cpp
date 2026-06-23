#include "LlmChatSession.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ===========================================================================
    //  Constructor
    // ===========================================================================
    LlmChatSession::LlmChatSession(LlmClient* llmClient) :
        llmClient_(llmClient)
    {
        LogHelper::writeToLog("[LlmChatSession] Constructor");
    }

    // ===========================================================================
    //  experienceLevelName - Returns string representation of experience level
    // ===========================================================================
    const char* LlmChatSession::experienceLevelName(ExperienceLevel level) noexcept
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

    // ===========================================================================
    //  addConversationTurn - Adds a conversation turn to the history
    // ===========================================================================
    void LlmChatSession::addConversationTurn(ConversationTurn::Role role, const juce::String& message)
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

    // ===========================================================================
    //  buildConversationHistory - Builds formatted conversation history string
    // ===========================================================================
    juce::String LlmChatSession::buildConversationHistory() const
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

    // ===========================================================================
    //  askLlm - Send a prompt to the LLM asynchronously
    // ===========================================================================
    void LlmChatSession::askLlm(const juce::String& systemPrompt,
                                const juce::String& userMessage,
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

        addConversationTurn(ConversationTurn::Role::User, userMessage);

        llmClient_->sendPrompt(
            systemPrompt,
            userMessage,
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
    void LlmChatSession::askLlmStream(const juce::String& systemPrompt,
                                      const juce::String& userMessage,
                                      std::function<void(const juce::String& token)> onToken,
                                      std::function<void(bool success, const juce::String& response)> onComplete)
    {
        if (llmClient_ == nullptr) {
            if (onComplete) onComplete(false, "LLM not available");
            return;
        }

        // ═══ NO verificar isAvailable() aquí — el background thread del LlmClient
        // hará checkAvailability() automáticamente si no está disponible.

        addConversationTurn(ConversationTurn::Role::User, userMessage);

        llmClient_->sendPromptStream(systemPrompt,
                                     userMessage,
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
    //  buildSystemPromptBase - Builds the base system prompt with experience level
    //  and DAW context. This is called by AiCoachAdapter which adds its own context.
    // ===========================================================================
    juce::String LlmChatSession::buildSystemPromptBase() const
    {
        juce::String prompt;

        // ── Experience level-specific instructions ──
        prompt += "NIVEL DEL USUARIO: " + juce::String(experienceLevelName(experienceLevel_)) + "\n";
        prompt += "USER EXPERIENCE LEVEL: " + juce::String(experienceLevelName(experienceLevel_)) + "\n";
        prompt += "Ajusta tu profundidad tecnica segun su nivel.\n\n";

        switch (experienceLevel_) {
            case ExperienceLevel::Novice:
                prompt += "EXPERIENCE LEVEL: Novice\n";
                prompt += "Explain audio concepts simply and avoid jargon.\n";
                prompt += "You are teaching a beginner. Be encouraging and patient.\n";
                prompt += "Provide step-by-step guidance. Avoid assumed knowledge.\n";
                prompt += "Use analogies. Celebrate small wins.\n\n";
                break;
            case ExperienceLevel::Intermediate:
                prompt += "EXPERIENCE LEVEL: Intermediate\n";
                prompt += "User knows basic mixing: threshold, ratio, Q, attack, release.\n";
                prompt += "You can use standard terminology without explanation.\n";
                prompt += "Focus on practical application and ear training.\n";
                prompt += "Suggest specific frequencies and ratios to try.\n\n";
                break;
            case ExperienceLevel::Advanced:
                prompt += "EXPERIENCE LEVEL: Advanced\n";
                prompt += "User knows advanced mixing: pre-ring, phase coherence, transient shaping.\n";
                prompt += "Skip basic explanations. Give exact numbers and frequencies.\n";
                prompt += "Discuss technical trade-offs and advanced techniques.\n";
                prompt += "You can reference specific gear and plugin characteristics.\n";
                prompt += "Focus on refinement and professional polish.\n\n";
                break;
            case ExperienceLevel::Expert:
                prompt += "EXPERIENCE LEVEL: Expert\n";
                prompt += "User is a professional engineer. Be direct. no fluff, no emojis.\n";
                prompt += "Assume deep technical knowledge of every concept.\n";
                prompt += "Give exact numbers: frequencies, ratios, attack/release, LUFS targets.\n";
                prompt += "No explanations, just the exact solution.\n";
                prompt += "Treat the user as a peer.\n\n";
                break;
        }

        // ── DAW name for plugin suggestions ──
        if (dawName_.isNotEmpty()) {
            prompt += "DAW: " + dawName_ + "\n";
            prompt += "When suggesting plugins, prioritize native " + dawName_ + " plugins.\n\n";
        }

        return prompt;
    }

    // ===========================================================================
    //  buildChatContextWithHistory - Builds chat context with conversation history
    //  This is called by AiCoachAdapter to include conversation history in the chat context.
    // ===========================================================================
    juce::String LlmChatSession::buildChatContextWithHistory() const
    {
        juce::String ctx;

        // ─── CONVERSATION HISTORY: Last 6 turns ──
        if (!conversationHistory_.empty()) {
            ctx += "[CONVERSATION HISTORY]\n";
            int start = std::max(0, (int)conversationHistory_.size() - 6);
            for (int i = start; i < (int)conversationHistory_.size(); ++i) {
                const auto& turn   = conversationHistory_[i];
                const char* prefix = (turn.role == ConversationTurn::Role::User) ? "[User] " : "[Coach] ";
                juce::String msg   = turn.message.substring(0, 120);
                if (turn.message.length() > 120) msg += "...";
                ctx += juce::String("  ") + juce::String(prefix) + msg + "\n";
            }
            ctx += "[END HISTORY]\n\n";
        }

        return ctx;
    }

} // namespace mixcoach

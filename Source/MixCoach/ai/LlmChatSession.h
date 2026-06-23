#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>
#include "LlmClient.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  LlmChatSession — Encapsulates LLM client and conversation history management
//
//  This class handles:
//    • LLM client interaction (sendPrompt, sendPromptStream)
//    • Conversation history management
//    • Experience level and DAW name for context
//    • System prompt and chat context building
//
//  It is used by AiCoachAdapter to separate LLM concerns from coaching logic.
// ═══════════════════════════════════════════════════════════════════════════
class LlmChatSession
{
public:
    // ─── Conversation Turn — represents a single interaction with the LLM ─────
    struct ConversationTurn {
        enum class Role : uint8_t { User, Assistant };
        Role         role;
        int64_t      timestampUs;
        juce::String message;    // Text of the message
    };

    // ─── Experience Level — determines the depth of LLM responses ───────────
    enum class ExperienceLevel : uint8_t {
        Novice,       // Beginner: detailed explanations, friendly tone
        Intermediate, // Intermediate: balance between detail and conciseness
        Advanced,     // Advanced: technical jargon, fewer explanations
        Expert        // Expert: only numbers, direct, no fluff
    };

    // ─── Constructor / Destructor ───────────────────────────────────────────
    LlmChatSession(LlmClient* llmClient);
    ~LlmChatSession() = default;

    // ─── LLM Client Management ───────────────────────────────────────────────
    void setLlmClient(LlmClient* client) noexcept { llmClient_ = client; }
    [[nodiscard]] LlmClient* getLlmClient() const noexcept { return llmClient_; }

    // ─── LLM Availability ────────────────────────────────────────────────────
    [[nodiscard]] bool isLlmAvailable() const noexcept
    {
        return llmClient_ != nullptr && llmClient_->isAvailable();
    }

    // ─── LLM Enable/Disable ─────────────────────────────────────────────────
    void setLlmEnabled(bool enabled) noexcept { llmEnabled_ = enabled; }
    [[nodiscard]] bool isLlmEnabled() const noexcept { return llmEnabled_; }

    // ─── Experience Level ───────────────────────────────────────────────────
    void setExperienceLevel(ExperienceLevel level) noexcept { experienceLevel_ = level; }
    [[nodiscard]] ExperienceLevel getExperienceLevel() const noexcept { return experienceLevel_; }
    [[nodiscard]] static const char* experienceLevelName(ExperienceLevel level) noexcept;

    // ─── DAW Name (for context in prompts) ───────────────────────────────────
    void setHostName(const juce::String& name) noexcept { dawName_ = name; }
    [[nodiscard]] const juce::String& getHostName() const noexcept { return dawName_; }

    // ─── Conversation History Management ────────────────────────────────────
    /** Adds a conversation turn to the history.
        Automatically prunes to kMaxConversationTurns. */
    void addConversationTurn(ConversationTurn::Role role, const juce::String& message);

    /** Builds a formatted string with the conversation history for LLM context. */
    [[nodiscard]] juce::String buildConversationHistory() const;

    /** Clears the conversation history. */
    void clearConversationHistory() noexcept { conversationHistory_.clear(); }

    /** Returns the complete conversation history (for debugging). */
    [[nodiscard]] const std::vector<ConversationTurn>& getConversationHistory() const noexcept
    {
        return conversationHistory_;
    }

    // ─── LLM Interaction Methods ───────────────────────────────────────────
    /** Sends a user message to the LLM and delivers the complete response via callback.
        The conversation history is updated automatically. */
    void askLlm(const juce::String& systemPrompt,
                const juce::String& userMessage,
                std::function<void(bool success, const juce::String& response)> callback);

    /** Sends a user message to the LLM with streaming response.
        @param systemPrompt  System prompt for the LLM
        @param userMessage   User message
        @param onToken       Called for each token (message thread)
        @param onComplete    Called on completion: (success, fullResponse)
        The conversation history is updated automatically on completion. */
    void askLlmStream(const juce::String& systemPrompt,
                      const juce::String& userMessage,
                      std::function<void(const juce::String& token)> onToken,
                      std::function<void(bool success, const juce::String& response)> onComplete);

    // ─── Prompt Building Methods (to be called by AiCoachAdapter) ───────────
    /** Builds the system prompt with experience level and DAW context.
        This method should be called by AiCoachAdapter to get the base system prompt,
        then AiCoachAdapter will add its own context on top. */
    [[nodiscard]] juce::String buildSystemPromptBase() const;

    /** Builds the chat context with conversation history.
        This method should be called by AiCoachAdapter to include conversation history
        in the chat context. */
    [[nodiscard]] juce::String buildChatContextWithHistory() const;

private:
    // ─── Dependencies ───────────────────────────────────────────────────────
    LlmClient* llmClient_{nullptr};

    // ─── State ───────────────────────────────────────────────────────────────
    bool llmEnabled_{false};
    ExperienceLevel experienceLevel_{ExperienceLevel::Intermediate};
    juce::String dawName_ = "FL Studio";
    std::vector<ConversationTurn> conversationHistory_;

    static constexpr int kMaxConversationTurns = 50;   // Maximum conversation turns stored
};

} // namespace mixcoach

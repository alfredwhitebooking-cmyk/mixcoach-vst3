#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  LlmClient — Cliente HTTP asíncrono para LLMs (Ollama local + cloud APIs)
    //
    //  Soporta dos proveedores:
    //    - Ollama (local): POST /api/chat, sin API key
    //    - OpenAI-compatible (DeepSeek, Groq, OpenRouter): POST /v1/chat/completions, Bearer token
    //
    //  Uso:
    //    1. Configurar provider, endpoint URL, modelo, y apiKey si aplica
    //    2. Llamar sendPrompt() con system + user message
    //    3. Recibir respuesta en el callback (message thread)
    //
    //  Nota: para OpenAI-compatible, la URL base DEBE incluir el prefijo de versión
    //  (ej: /v1, /v1beta/openai). El cliente agrega /chat/completions al path base.
    //
    //  Ejemplos:
    //    Ollama:  endpoint="http://localhost:11434", model="phi3:mini"
    //    DeepSeek: endpoint="https://api.deepseek.com/v1", model="deepseek-v4-flash", apiKey="sk-..."
    //    Groq:    endpoint="https://api.groq.com/openai/v1", model="llama3-70b-8192", apiKey="gsk_..."
    //    OpenRouter: endpoint="https://openrouter.ai/api/v1", model="deepseek/deepseek-v4-flash", apiKey="sk-..."
    //    Gemini:  endpoint="https://generativelanguage.googleapis.com/v1beta/openai", model="gemini-2.5-flash",
    //    apiKey="..."
    // ═══════════════════════════════════════════════════════════════════════════
    class LlmClient : private juce::Thread
    {
    public:
        // ─── Proveedor de API ─────────────────────────────────────────────────
        enum class Provider : uint8_t
        {
            Ollama,          // API local: POST /api/chat
            OpenAICompatible // Cloud: POST /v1/chat/completions (DeepSeek, Groq, OpenRouter, etc.)
        };

        // ─── Configuración ────────────────────────────────────────────────────
        struct Config
        {
            // ─── Provider primario ──────────────────────────────────────────
            Provider provider        = Provider::Ollama;
            juce::String endpointUrl = "http://localhost:11434";
            juce::String apiKey      = {}; // Para OpenAI-compatible: "sk-..." o "gsk_..."
            juce::String model       = "qwen2.5:7b";
            float temperature        = 0.7f;
            int maxTokens            = 800;
            int timeoutMs            = 30000;

            // ─── Fallback provider (se activa si el primario falla) ─────────
            // Cuando el primario no responde (timeout, HTTP error, conexión),
            // la request se reenvía automáticamente al fallback.
            // Si el fallback tiene éxito, la config global se cambia al fallback
            // para las siguientes requests (evita reintentar el primario caído).
            bool useFallback                 = false;
            Provider fallbackProvider        = Provider::OpenAICompatible;
            juce::String fallbackEndpointUrl = "https://openrouter.ai/api/v1";
            juce::String fallbackApiKey      = {};
            juce::String fallbackModel       = "meta-llama/llama-3.2-3b-instruct:free";
            float fallbackTemperature        = 0.7f;
            int fallbackMaxTokens            = 800;
            int fallbackTimeoutMs            = 15000; // 15s para el fallback
        };

        // ─── Helpers para construir Config de fallback conocidos ─────────────
        /** Construye una Config para OpenRouter con modelo gratis. */
        static Config makeOpenRouterFallback(const juce::String& apiKey,
                                             const juce::String& model = "meta-llama/llama-3.2-3b-instruct:free")
        {
            Config cfg;
            cfg.provider    = Provider::OpenAICompatible;
            cfg.endpointUrl = "https://openrouter.ai/api/v1";
            cfg.apiKey      = apiKey;
            cfg.model       = model;
            cfg.temperature = 0.7f;
            cfg.maxTokens   = 800;
            cfg.timeoutMs   = 15000;
            return cfg;
        }

        /** Construye una Config para DeepSeek local via Ollama. */
        static Config makeDeepSeekOllamaConfig()
        {
            Config cfg;
            cfg.provider    = Provider::Ollama;
            cfg.endpointUrl = "http://localhost:11434";
            cfg.model       = "deepseek-r1:7b";
            cfg.temperature = 0.7f;
            cfg.maxTokens   = 1024;
            cfg.timeoutMs   = 60000;
            return cfg;
        }

        /** Construye una Config para Qwen2.5 local via Ollama. */
        static Config makeQwen25Config()
        {
            Config cfg;
            cfg.provider    = Provider::Ollama;
            cfg.endpointUrl = "http://localhost:11434";
            cfg.model       = "qwen2.5:7b";
            cfg.temperature = 0.7f;
            cfg.maxTokens   = 1024;
            cfg.timeoutMs   = 60000;
            return cfg;
        }

        /** Construye una Config para NVIDIA AI Foundation API.
            Usa el endpoint OpenAI-compatible de NVIDIA.
            Modelos recomendados:
              - meta/llama-3.1-70b-instruct (potente, ~70B params)
              - meta/llama-3.1-8b-instruct  (rápido, ~8B params)
              - nvidia/nemotron-4-340b-instruct (máxima capacidad)
              - mistralai/mixtral-8x22b-v0.1 (buen balance calidad/velocidad)
            @param apiKey  API key con prefijo "nvapi-"
            @param model   Model ID (default: meta/llama-3.1-70b-instruct) */
        static Config makeNvidiaConfig(const juce::String& apiKey,
                                       const juce::String& model = "meta/llama-3.1-70b-instruct")
        {
            Config cfg;
            cfg.provider    = Provider::OpenAICompatible;
            cfg.endpointUrl = "https://integrate.api.nvidia.com/v1";
            cfg.apiKey      = apiKey;
            cfg.model       = model;
            cfg.temperature = 0.7f;
            cfg.maxTokens   = 1024;
            cfg.timeoutMs   = 30000;
            return cfg;
        }

        // ─── Callback para respuestas ─────────────────────────────────────────
        using ResponseCallback =
            std::function<void(bool success, const juce::String& response, const juce::String& error)>;

        // ─── Streaming callbacks ──────────────────────────────────────────────
        /** Called for each token as it arrives from the LLM. */
        using StreamCallback = std::function<void(const juce::String& token)>;
        /** Called when streaming completes (success or error).
            @param fullResponse  The entire accumulated response text on success.
            @param error         Error message on failure (empty on success). */
        using StreamCompletedCallback =
            std::function<void(const juce::String& fullResponse, const juce::String& error)>;

        LlmClient();
        ~LlmClient() override;

        // ─── Config ───────────────────────────────────────────────────────────
        void setConfig(const Config& config);

        [[nodiscard]] const Config& getConfig() const noexcept { return config_; }

        // ─── Estado ───────────────────────────────────────────────────────────
        [[nodiscard]] bool isBusy() const noexcept { return busy_.load(); }

        [[nodiscard]] bool isAvailable() const noexcept { return available_.load(); }

        void setAvailability(bool available) noexcept { available_.store(available); }

        // ─── Enviar prompt (asíncrono) ────────────────────────────────────────
        /** Envía un prompt al LLM. La respuesta completa llega vía callback en el message thread. */
        void sendPrompt(const juce::String& systemPrompt, const juce::String& userPrompt, ResponseCallback callback);

        /** Envía un prompt al LLM con respuesta en streaming.
            Los tokens llegan UNO POR UNO vía onToken (en el message thread).
            Cuando termina, se llama a onComplete con el texto completo.
            @param systemPrompt  Instrucciones del sistema
            @param userPrompt    Mensaje del usuario
            @param onToken       Se llama por cada token (message thread)
            @param onComplete    Se llama al finalizar (message thread) */
        void sendPromptStream(const juce::String& systemPrompt,
                              const juce::String& userPrompt,
                              StreamCallback onToken,
                              StreamCompletedCallback onComplete);

        /** Verifica si el proveedor primario está disponible (GET /api/tags o /models). */
        void checkAvailability();

        /** Verifica AMBOS proveedores (primario + fallback). Retorna true si ALGUNO está disponible.
            Cambia la config activa al que responda primero. */
        bool checkAllProviders();

        /** Obtiene el nombre del proveedor actual para logging/UI. */
        juce::String getProviderName() const noexcept
        {
            switch (config_.provider) {
                case Provider::Ollama:
                    return "Ollama";
                case Provider::OpenAICompatible:
                    return "OpenAI";
                default:
                    return "Unknown";
            }
        }

        /** Obtiene un nombre de proveedor legible para la UI,
            identificando servicios específicos por su endpoint URL. */
        juce::String getProviderDisplayName() const noexcept
        {
            switch (config_.provider) {
                case Provider::Ollama:
                    return "Ollama";
                case Provider::OpenAICompatible: {
                    juce::String url = config_.endpointUrl.toLowerCase();
                    if (url.contains("nvidia"))     return "NVIDIA";
                    if (url.contains("groq"))        return "Groq";
                    if (url.contains("openrouter"))  return "OpenRouter";
                    if (url.contains("deepseek"))    return "DeepSeek";
                    if (url.contains("googleapis") || url.contains("generativelanguage"))
                        return "Gemini";
                    return "Cloud AI";
                }
                default:
                    return "Unknown";
            }
        }

        /** Obtiene un nombre de modelo legible para la UI.
            Convierte IDs técnicos como "meta/llama-3.1-70b-instruct"
            en "Llama 3.1 70B" y "qwen2.5:7b" en "Qwen 2.5 7B". */
        juce::String getModelDisplayName() const noexcept
        {
            juce::String raw  = config_.model;
            juce::String name = raw;

            // Quitar prefijo "org/" (ej: "meta/llama-..." → "llama-...")
            int slashPos = name.indexOfChar('/');
            if (slashPos >= 0) name = name.substring(slashPos + 1);

            // Quitar sufijos comunes de modelos instruct/chat
            name = name.replace("-instruct", "", true);
            name = name.replace("-chat", "", true);
            name = name.replace("-it", "", true);
            name = name.replace("-vision", "", true);

            // Separar palabras: convertir "llama-3.1-70b" → "llama 3.1 70b"
            name = name.replace("-", " ", true);
            name = name.replace(":", " ", true);
            name = name.replace("_", " ", true);

            // Capitalizar "b" después de dígitos (70b → 70B, 7b → 7B)
            for (int i = 0; i < name.length() - 1; ++i) {
                if (juce::CharacterFunctions::isDigit(name[i]) && name[i + 1] == 'b') {
                    name = name.substring(0, i + 1) + 'B' + name.substring(i + 2);
                }
            }

            // Capitalizar primera letra de cada palabra
            juce::String result;
            bool newWord = true;
            for (int i = 0; i < name.length(); ++i) {
                juce::juce_wchar c = name[i];
                if (c == ' ') {
                    newWord = true;
                    result += ' ';
                }
                else if (newWord) {
                    result += (juce::juce_wchar)juce::CharacterFunctions::toUpperCase(c);
                    newWord = false;
                }
                else {
                    result += c;
                }
            }

            return result;
        }

        /** Obtiene un string combinado "Proveedor: Modelo" para mostrar en UI. */
        juce::String getProviderModelLabel() const noexcept
        {
            return getProviderDisplayName() + ": " + getModelDisplayName();
        }

    private:
        void run() override;

        // ─── Trabajo pendiente encolado ───────────────────────────────────────
        struct PendingRequest
        {
            juce::String systemPrompt;
            juce::String userPrompt;
            ResponseCallback callback;
            StreamCallback onToken;                   // nullptr = no streaming
            StreamCompletedCallback onStreamComplete; // nullptr = no streaming

            bool isStreaming() const noexcept { return onToken != nullptr; }
        };

        /** Intenta procesar un request con una config específica.
            Retorna true si tuvo éxito, false si falló (para probar fallback).
            Si succeedCallback no es nullptr, se llama con la respuesta en el message thread. */
        bool processRequestWithConfig(const PendingRequest& request, const Config& cfg, bool fireCallback);
        bool processStreamWithConfig(const PendingRequest& request, const Config& cfg);
        void switchToFallbackConfig();

        void processRequest(const PendingRequest& request);
        void processStreamRequest(const PendingRequest& request);
        juce::String makeRequest(const juce::String& jsonBody);
        juce::String makeRequestWithConfig(const juce::String& jsonBody, const Config& cfg, int& statusCode);

        Config config_;
        std::atomic<bool> busy_{false};
        std::atomic<bool> available_{false};

        juce::CriticalSection mutex_;
        std::vector<PendingRequest> pendingRequests_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LlmClient)
    };

} // namespace mixcoach

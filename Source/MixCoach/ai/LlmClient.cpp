#include "LlmClient.h"
#include "../../Common/types/LogHelper.h"
#include <juce_events/juce_events.h>

#ifdef _WIN32
#include <objbase.h>  // CoInitializeEx / CoUninitialize
#endif

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════
LlmClient::LlmClient()
    : juce::Thread("LlmClient")
{
    startThread();
}

LlmClient::~LlmClient()
{
    stopThread(5000);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Config
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::setConfig(const Config& config)
{
    const juce::ScopedLock lock(mutex_);
    config_ = config;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Enviar prompt (asíncrono)
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::sendPrompt(const juce::String& systemPrompt,
                            const juce::String& userPrompt,
                            ResponseCallback callback)
{
    {
        const juce::ScopedLock lock(mutex_);
        if (pendingRequests_.size() < 10) // Máximo 10 requests encolados
        {
            pendingRequests_.push_back({systemPrompt, userPrompt, std::move(callback), nullptr, nullptr});
        }
        else
        {
            if (callback)
                callback(false, {}, "[LlmClient] Cola llena, request ignorado.");
            return;
        }
    }
    notify(); // Despertar el thread
}

// ═══════════════════════════════════════════════════════════════════════════
//  sendPromptStream — Enviar prompt con respuesta en streaming
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::sendPromptStream(const juce::String& systemPrompt,
                                  const juce::String& userPrompt,
                                  StreamCallback onToken,
                                  StreamCompletedCallback onComplete)
{
    {
        const juce::ScopedLock lock(mutex_);
        if (pendingRequests_.size() < 10)
        {
            pendingRequests_.push_back({systemPrompt, userPrompt, nullptr,
                                        std::move(onToken), std::move(onComplete)});
        }
        else
        {
            if (onComplete)
                juce::MessageManager::callAsync(
                    [cb = std::move(onComplete)]()
                    {
                        cb({}, "[LlmClient] Cola llena, request ignorado.");
                    });
            return;
        }
    }
    notify();
}

// Forward declaration para checkEndpoint (definida más abajo)
static bool checkEndpoint(LlmClient::Provider provider,
                           const juce::String& endpointUrl,
                           const juce::String& apiKey);

// ═══════════════════════════════════════════════════════════════════════════
//  checkAvailability — Verifica disponibilidad del proveedor PRIMARIO
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::checkAvailability()
{
    bool available = checkEndpoint(config_.provider, config_.endpointUrl, config_.apiKey);
    available_.store(available);

    if (available)
        LogHelper::writeToLog("[LlmClient] API disponible: " + getProviderName()
                              + " @ " + config_.endpointUrl + " (modelo: " + config_.model + ")");
    else
        LogHelper::writeToLog("[LlmClient] API NO disponible: " + getProviderName()
                              + " @ " + config_.endpointUrl);
}

// ═══════════════════════════════════════════════════════════════════════════
//  checkAllProviders — Verifica PRIMARIO + FALLBACK
//  Retorna true si ALGUNO está disponible.
//  Si el fallback responde y el primario no, cambia la config activa al fallback.
// ═══════════════════════════════════════════════════════════════════════════
bool LlmClient::checkAllProviders()
{
    // Probar primario
    bool primaryAvailable = checkEndpoint(config_.provider, config_.endpointUrl, config_.apiKey);
    if (primaryAvailable)
    {
        available_.store(true);
        LogHelper::writeToLog("[LlmClient] Primario disponible: " + getProviderName()
                              + " @ " + config_.endpointUrl);
        return true;
    }

    // Probar fallback si está configurado
    if (config_.useFallback)
    {
        bool fallbackAvailable = checkEndpoint(config_.fallbackProvider,
                                                config_.fallbackEndpointUrl,
                                                config_.fallbackApiKey);
        if (fallbackAvailable)
        {
            // ═══ Conmutar al fallback permanentemente ═══
            LogHelper::writeToLog("[LlmClient] Primario NO disponible, fallback disponible."
                                  " Conmutando a " + config_.fallbackEndpointUrl
                                  + " (modelo: " + config_.fallbackModel + ")");
            switchToFallbackConfig();
            available_.store(true);
            return true;
        }

        LogHelper::writeToLog("[LlmClient] Ambos proveedores NO disponibles");
    }

    available_.store(false);
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  checkEndpoint — Helper: GET /api/tags (Ollama) o GET /models (OpenAI)
// ═══════════════════════════════════════════════════════════════════════════
static bool checkEndpoint(LlmClient::Provider provider,
                           const juce::String& endpointUrl,
                           const juce::String& apiKey)
{
    try
    {
        juce::String endpoint;
        juce::StringPairArray requestHeaders;

        if (provider == LlmClient::Provider::OpenAICompatible)
        {
            endpoint = endpointUrl;
            if (!endpoint.endsWithChar('/'))
                endpoint += "/";
            if (!endpoint.endsWith("models"))
                endpoint += "models";

            if (apiKey.isNotEmpty())
                requestHeaders.set("Authorization", "Bearer " + apiKey);
        }
        else
        {
            // Ollama: GET /api/tags
            endpoint = endpointUrl;
            if (!endpoint.endsWithChar('/'))
                endpoint += "/";
            endpoint += "api/tags";
        }

        juce::URL url(endpoint);
        int statusCode = 0;
        juce::StringPairArray responseHeaders;

        auto inputStream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders(requestHeaders.getDescription())
                .withConnectionTimeoutMs(5000)
                .withResponseHeaders(&responseHeaders)
                .withStatusCode(&statusCode));

        return (inputStream != nullptr && statusCode == 200);
    }
    catch (...)
    {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Background thread — Procesa requests encolados
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::run()
{
#ifdef _WIN32
    // ═══ IMPORTANTE: Inicializar COM para este thread ════════════════
    // JUCE usa WinHTTP/WinInet para HTTP en Windows, lo que requiere
    // COM inicializado en el thread. Si no se hace, createInputStream()
    // puede causar Access Violation en VCRUNTIME140.dll.
    // La elección de COINIT_MULTITHREADED evita conflictos con el
    // COINIT_APARTMENTTHREADED del message thread.
    ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

    while (!threadShouldExit())
    {
        PendingRequest request;
        {
            const juce::ScopedLock lock(mutex_);
            if (!pendingRequests_.empty())
            {
                request = std::move(pendingRequests_.front());
                pendingRequests_.erase(pendingRequests_.begin());
            }
        }

        if (request.callback || request.isStreaming())
        {
            busy_.store(true);
            if (request.isStreaming())
                processStreamRequest(request);
            else
                processRequest(request);
            busy_.store(false);
        }
        else
        {
            wait(100); // Sleep 100ms esperando trabajo
        }
    }

#ifdef _WIN32
    ::CoUninitialize();
#endif
}
// ═══════════════════════════════════════════════════════════════════════════
//  switchToFallbackConfig — Conmuta la config activa al fallback
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::switchToFallbackConfig()
{
    if (!config_.useFallback)
        return;

    Config fb;
    fb.provider     = config_.fallbackProvider;
    fb.endpointUrl  = config_.fallbackEndpointUrl;
    fb.apiKey       = config_.fallbackApiKey;
    fb.model        = config_.fallbackModel;
    fb.temperature  = config_.fallbackTemperature;
    fb.maxTokens    = config_.fallbackMaxTokens;
    fb.timeoutMs    = config_.fallbackTimeoutMs;
    // Preservar config de fallback por si el fallback también falla (para futuros reintentos)
    fb.useFallback  = config_.useFallback;
    fb.fallbackProvider       = config_.fallbackProvider;
    fb.fallbackEndpointUrl    = config_.fallbackEndpointUrl;
    fb.fallbackApiKey         = config_.fallbackApiKey;
    fb.fallbackModel          = config_.fallbackModel;
    fb.fallbackTemperature    = config_.fallbackTemperature;
    fb.fallbackMaxTokens      = config_.fallbackMaxTokens;
    fb.fallbackTimeoutMs      = config_.fallbackTimeoutMs;

    {
        const juce::ScopedLock lock(mutex_);
        config_ = fb;
    }

    LogHelper::writeToLog("[LlmClient] Conmutado a fallback: "
                          + config_.endpointUrl + " (modelo: " + config_.model + ")");
}

//  Procesar un request (en background thread) — con fallback automático
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::processRequest(const PendingRequest& request)
{
    // ═══ Intentar con config actual (primario) ═══════════════════════════
    bool success = processRequestWithConfig(request, config_, true);

    // ═══ Si falló Y hay fallback configurado Y no estamos ya en fallback ══
    if (!success && config_.useFallback
        && (config_.provider != config_.fallbackProvider
            || config_.endpointUrl != config_.fallbackEndpointUrl))
    {
        LogHelper::writeToLog("[LlmClient] Request falló con primario."
                              " Reintentando con fallback...");

        // Construir config de fallback temporal
        Config fbConfig;
        fbConfig.provider     = config_.fallbackProvider;
        fbConfig.endpointUrl  = config_.fallbackEndpointUrl;
        fbConfig.apiKey       = config_.fallbackApiKey;
        fbConfig.model        = config_.fallbackModel;
        fbConfig.temperature  = config_.fallbackTemperature;
        fbConfig.maxTokens    = config_.fallbackMaxTokens;
        fbConfig.timeoutMs    = config_.fallbackTimeoutMs;

        // Intentar con fallback (SIN fireCallback — el callback del primario ya falló)
        bool fbSuccess = processRequestWithConfig(request, fbConfig, true);

        if (fbSuccess)
        {
            // ═══ El fallback funcionó: conmutar permanentemente ═══
            switchToFallbackConfig();
            available_.store(true);
            LogHelper::writeToLog("[LlmClient] Fallback exitoso. Conmutado a: "
                                  + config_.model + " @ " + config_.endpointUrl);
        }
        else
        {
            LogHelper::writeToLog("[LlmClient] Fallback también falló.");
            available_.store(false);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  processRequestWithConfig — Intenta procesar un request con una Config dada
//  Retorna true si tuvo éxito. Si fireCallback es true, invoca el callback
//  con la respuesta. Si es false, el callback ya fue invocado por el primario.
// ═══════════════════════════════════════════════════════════════════════════
bool LlmClient::processRequestWithConfig(const PendingRequest& request,
                                          const Config& cfg,
                                          bool fireCallback)
{
    try
    {
        // ═══ Construir payload JSON según cfg (NO config_) ═══════════════
        bool isOpenAI = (cfg.provider == Provider::OpenAICompatible);

        // Messages array (compartido por ambos formatos)
        juce::Array<juce::var> messagesArr;

        auto sysMsg = juce::DynamicObject::Ptr(new juce::DynamicObject());
        sysMsg->setProperty("role", "system");
        sysMsg->setProperty("content", request.systemPrompt);
        messagesArr.add(juce::var(sysMsg));

        auto userMsg = juce::DynamicObject::Ptr(new juce::DynamicObject());
        userMsg->setProperty("role", "user");
        userMsg->setProperty("content", request.userPrompt);
        messagesArr.add(juce::var(userMsg));

        juce::String jsonBody;

        if (isOpenAI)
        {
            // OpenAI-compatible (DeepSeek, Groq, OpenRouter):
            // POST /v1/chat/completions
            // { "model": "...", "messages": [...], "temperature": ...,
            //   "max_tokens": ..., "stream": false }
            auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
            root->setProperty("model", cfg.model);
            root->setProperty("messages", messagesArr);
            root->setProperty("temperature", static_cast<double>(cfg.temperature));
            root->setProperty("max_tokens", cfg.maxTokens);
            root->setProperty("stream", false);
            jsonBody = juce::JSON::toString(juce::var(root), false);
        }
        else
        {
            // Ollama local:
            // POST /api/chat
            // { "model": "...", "messages": [...], "stream": false,
            //   "options": { "temperature": ..., "num_predict": ... } }
            auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
            root->setProperty("model", cfg.model);
            root->setProperty("messages", messagesArr);
            root->setProperty("stream", false);

            auto options = juce::DynamicObject::Ptr(new juce::DynamicObject());
            options->setProperty("temperature", static_cast<double>(cfg.temperature));
            options->setProperty("num_predict", cfg.maxTokens);
            root->setProperty("options", juce::var(options));

            jsonBody = juce::JSON::toString(juce::var(root), false);
        }

        // ─── Enviar request HTTP POST ────────────────────────────────────
        LogHelper::writeToLog(juce::String("[LlmClient] Enviando prompt a ")
                              + (isOpenAI ? "cloud" : "local")
                              + " (" + cfg.model + ", "
                              + juce::String(cfg.maxTokens) + " tokens)...");

        int statusCode = 0;
        juce::String response = makeRequestWithConfig(jsonBody, cfg, statusCode);

        if (response.isEmpty())
        {
            if (fireCallback && request.callback)
            {
                juce::MessageManager::callAsync(
                    [cb = request.callback, isOpenAI]()
                    {
                        juce::String msg = isOpenAI
                            ? "[LlmClient] No hubo respuesta del proveedor cloud. Verifica tu API key y conexión a internet."
                            : "[LlmClient] No hubo respuesta de Ollama. Verifica que Ollama esté corriendo en http://localhost:11434";
                        cb(false, {}, msg);
                    });
            }
            return false;
        }

        // ─── Parsear respuesta JSON según el proveedor ═══════════════
        auto resultJson = juce::JSON::parse(response);
        if (!resultJson.isObject())
        {
            if (fireCallback && request.callback)
            {
                juce::MessageManager::callAsync(
                    [cb = request.callback, response]()
                    {
                        cb(false, {}, "[LlmClient] Respuesta invalida: " + response.substring(0, 200));
                    });
            }
            return false;
        }

        auto rootObj = resultJson.getDynamicObject();
        if (rootObj == nullptr)
        {
            if (fireCallback && request.callback)
            {
                juce::MessageManager::callAsync(
                    [cb = request.callback]()
                    {
                        cb(false, {}, "[LlmClient] Error parseando respuesta JSON");
                    });
            }
            return false;
        }

        // Extraer mensaje del asistente
        juce::String content;

        if (isOpenAI)
        {
            // OpenAI format: { "choices": [ { "message": { "content": "..." } } ] }
            auto choices = rootObj->getProperty("choices");
            if (choices.isArray() && choices.getArray()->size() > 0)
            {
                auto firstChoice = (*choices.getArray())[0];
                auto choiceObj = firstChoice.getDynamicObject();
                if (choiceObj != nullptr)
                {
                    auto msgObj = choiceObj->getProperty("message").getDynamicObject();
                    if (msgObj != nullptr)
                        content = msgObj->getProperty("content").toString().trim();
                }
            }
        }
        else
        {
            // Ollama format: { "message": { "content": "..." } }
            //             or: { "response": "..." } (legacy /api/generate)
            if (rootObj->hasProperty("message"))
            {
                auto msgObj = rootObj->getProperty("message").getDynamicObject();
                if (msgObj != nullptr)
                    content = msgObj->getProperty("content").toString().trim();
            }
            else if (rootObj->hasProperty("response"))
            {
                content = rootObj->getProperty("response").toString().trim();
            }
        }

        if (content.isEmpty())
        {
            if (fireCallback && request.callback)
            {
                juce::String preview = response.substring(0, 300);
                juce::MessageManager::callAsync(
                    [cb = request.callback, preview]()
                    {
                        cb(false, {}, "[LlmClient] No se pudo extraer contenido de la respuesta: "
                           + preview);
                    });
            }
            return false;
        }

        // Marcar como disponible si tuvimos éxito (solo si es el primario)
        if (fireCallback)
            available_.store(true);

        // ─── Entregar respuesta al message thread ────────────────────────
        if (fireCallback && request.callback)
        {
            juce::MessageManager::callAsync(
                [cb = request.callback, content]()
                {
                    cb(true, content, {});
                });
        }

        LogHelper::writeToLog("[LlmClient] Respuesta recibida ("
                              + juce::String(content.length()) + " chars)");
        return true;
    }
    catch (const std::exception& e)
    {
        LogHelper::writeToLog("[LlmClient] Excepción: " + juce::String(e.what()));
        if (fireCallback && request.callback)
        {
            juce::MessageManager::callAsync(
                [cb = request.callback, msg = juce::String(e.what())]()
                {
                    cb(false, {}, "[LlmClient] Error: " + msg);
                });
        }
        return false;
    }
    catch (...)
    {
        LogHelper::writeToLog("[LlmClient] Excepción desconocida");
        if (fireCallback && request.callback)
        {
            juce::MessageManager::callAsync(
                [cb = request.callback]()
                {
                    cb(false, {}, "[LlmClient] Error desconocido");
                });
        }
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  makeRequest — Envía HTTP POST con la CONFIG ACTUAL
// ═══════════════════════════════════════════════════════════════════════════
juce::String LlmClient::makeRequest(const juce::String& jsonBody)
{
    int unusedStatus = 0;
    return makeRequestWithConfig(jsonBody, config_, unusedStatus);
}

// ═══════════════════════════════════════════════════════════════════════════
//  makeRequestWithConfig — Envía HTTP POST con una Config específica
// ═══════════════════════════════════════════════════════════════════════════
juce::String LlmClient::makeRequestWithConfig(const juce::String& jsonBody,
                                                const Config& cfg,
                                                int& statusCode)
{
    try
    {
        // ─── Determinar endpoint según el proveedor ─────────────────────
        juce::String endpoint = cfg.endpointUrl;
        if (endpoint.endsWithChar('/'))
            endpoint = endpoint.dropLastCharacters(1);

        if (cfg.provider == Provider::OpenAICompatible)
            endpoint += "/chat/completions";
        else
            endpoint += "/api/chat";

        juce::URL url(endpoint);
        juce::StringPairArray responseHeaders;

        // ─── Construir headers HTTP ─────────────────────────────────────
        juce::String extraHeaders = "Content-Type: application/json\r\n";

        if (cfg.provider == Provider::OpenAICompatible && cfg.apiKey.isNotEmpty())
            extraHeaders += "Authorization: Bearer " + cfg.apiKey + "\r\n";

        auto inputStream = url.withPOSTData(jsonBody)
            .createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                   .withExtraHeaders(extraHeaders)
                                   .withConnectionTimeoutMs(cfg.timeoutMs)
                                   .withResponseHeaders(&responseHeaders)
                                   .withStatusCode(&statusCode));

        if (inputStream == nullptr)
        {
            LogHelper::writeToLog("[LlmClient] Error de conexión a " + endpoint);
            statusCode = 0;
            return {};
        }

        juce::String response = inputStream->readEntireStreamAsString();

        if (statusCode != 200)
        {
            LogHelper::writeToLog("[LlmClient] HTTP " + juce::String(statusCode)
                                  + ": " + response.substring(0, 200));
            return {};
        }

        return response;
    }
    catch (const std::exception& e)
    {
        LogHelper::writeToLog("[LlmClient] Error HTTP: " + juce::String(e.what()));
        return {};
    }
    catch (...)
    {
        LogHelper::writeToLog("[LlmClient] Error HTTP desconocido");
        return {};
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  processStreamRequest — Procesa un request con respuesta en streaming
//  Lee el InputStream línea por línea (SSE), parsea tokens y los envía
//  al message thread via callAsync.
//  Incluye fallback automático si el primario falla.
// ═══════════════════════════════════════════════════════════════════════════
void LlmClient::processStreamRequest(const PendingRequest& request)
{
    // ═══ Intentar con config actual ═══════════════════════════════════
    bool streamOk = processStreamWithConfig(request, config_);

    // ═══ Si falló Y hay fallback configurado ─────────────────────────
    if (!streamOk && config_.useFallback
        && (config_.provider != config_.fallbackProvider
            || config_.endpointUrl != config_.fallbackEndpointUrl))
    {
        LogHelper::writeToLog("[LlmClient] Stream falló con primario."
                              " Reintentando streaming con fallback...");

        Config fbConfig;
        fbConfig.provider     = config_.fallbackProvider;
        fbConfig.endpointUrl  = config_.fallbackEndpointUrl;
        fbConfig.apiKey       = config_.fallbackApiKey;
        fbConfig.model        = config_.fallbackModel;
        fbConfig.temperature  = config_.fallbackTemperature;
        fbConfig.maxTokens    = config_.fallbackMaxTokens;
        fbConfig.timeoutMs    = config_.fallbackTimeoutMs;

        bool fbStreamOk = processStreamWithConfig(request, fbConfig);

        if (fbStreamOk)
        {
            switchToFallbackConfig();
            available_.store(true);
            LogHelper::writeToLog("[LlmClient] Fallback streaming exitoso. Conmutado a: "
                                  + config_.model + " @ " + config_.endpointUrl);
        }
        else
        {
            LogHelper::writeToLog("[LlmClient] Fallback streaming también falló.");
            available_.store(false);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  processStreamWithConfig — Intenta streaming con una Config específica
// ═══════════════════════════════════════════════════════════════════════════
bool LlmClient::processStreamWithConfig(const PendingRequest& request,
                                         const Config& cfg)
{
    try
    {
        // ─── Construir payload JSON con stream: true ────────────────────
        bool isOpenAI = (cfg.provider == Provider::OpenAICompatible);

        juce::Array<juce::var> messagesArr;

        auto sysMsg = juce::DynamicObject::Ptr(new juce::DynamicObject());
        sysMsg->setProperty("role", "system");
        sysMsg->setProperty("content", request.systemPrompt);
        messagesArr.add(juce::var(sysMsg));

        auto userMsg = juce::DynamicObject::Ptr(new juce::DynamicObject());
        userMsg->setProperty("role", "user");
        userMsg->setProperty("content", request.userPrompt);
        messagesArr.add(juce::var(userMsg));

        juce::String jsonBody;
        auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
        root->setProperty("model", cfg.model);
        root->setProperty("messages", messagesArr);
        root->setProperty("stream", true);

        if (isOpenAI)
        {
            root->setProperty("temperature", static_cast<double>(cfg.temperature));
            root->setProperty("max_tokens", cfg.maxTokens);
            jsonBody = juce::JSON::toString(juce::var(root), false);
        }
        else
        {
            // Ollama: stream: true + options
            auto options = juce::DynamicObject::Ptr(new juce::DynamicObject());
            options->setProperty("temperature", static_cast<double>(cfg.temperature));
            options->setProperty("num_predict", cfg.maxTokens);
            root->setProperty("options", juce::var(options));
            jsonBody = juce::JSON::toString(juce::var(root), false);
        }

        // ─── Determinar endpoint ───────────────────────────────────────
        juce::String endpoint = cfg.endpointUrl;
        if (endpoint.endsWithChar('/'))
            endpoint = endpoint.dropLastCharacters(1);

        if (isOpenAI)
            endpoint += "/chat/completions";
        else
            endpoint += "/api/chat";

        juce::URL url(endpoint);
        int statusCode = 0;
        juce::StringPairArray responseHeaders;

        // ─── Headers HTTP ───────────────────────────────────────────────
        juce::String extraHeaders = "Content-Type: application/json\r\n";
        if (isOpenAI && cfg.apiKey.isNotEmpty())
            extraHeaders += "Accept: text/event-stream\r\n"
                          + juce::String("Authorization: Bearer ") + cfg.apiKey + "\r\n";
        else
            extraHeaders += "Accept: text/event-stream\r\n";

        auto inputStream = url.withPOSTData(jsonBody)
            .createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                   .withExtraHeaders(extraHeaders)
                                   .withConnectionTimeoutMs(cfg.timeoutMs)
                                   .withResponseHeaders(&responseHeaders)
                                   .withStatusCode(&statusCode));

        if (inputStream == nullptr || statusCode != 200)
        {
            juce::String errMsg = (inputStream == nullptr)
                ? "[LlmClient] Error de conexión a " + endpoint
                : "[LlmClient] HTTP " + juce::String(statusCode);
            LogHelper::writeToLog(errMsg);
            if (request.onStreamComplete)
            {
                juce::MessageManager::callAsync(
                    [cb = request.onStreamComplete, errMsg]()
                    {
                        cb({}, errMsg);
                    });
            }
            return false;
        }

        // ─── Marcar como disponible ─────────────────────────────────────
        available_.store(true);

        // ─── Leer stream línea por línea ────────────────────────────────
        // Acumulamos el texto completo para el callback de completion
        juce::String fullResponse;
        bool hasError = false;
        juce::String errorMsg;

        while (!inputStream->isExhausted() && !threadShouldExit())
        {
            juce::String line = inputStream->readNextLine();
            if (line.isEmpty())
                continue;

            juce::String contentToken;
            bool isDone = false;

            if (isOpenAI)
            {
                // OpenAI: "data: {\"choices\":[{\"delta\":{\"content\":\"token\"}}]}"
                if (line.startsWith("data: "))
                {
                    juce::String data = line.substring(6).trim();
                    if (data == "[DONE]")
                    {
                        isDone = true;
                    }
                    else
                    {
                        auto json = juce::JSON::parse(data);
                        if (json.isObject())
                        {
                            auto* obj = json.getDynamicObject();
                            if (obj != nullptr)
                            {
                                auto choices = obj->getProperty("choices");
                                if (choices.isArray() && choices.getArray()->size() > 0)
                                {
                                    auto delta = (*choices.getArray())[0].getDynamicObject();
                                    if (delta != nullptr)
                                    {
                                        auto deltaContent = delta->getProperty("delta");
                                        if (deltaContent.isObject())
                                        {
                                            auto* deltaObj = deltaContent.getDynamicObject();
                                            if (deltaObj != nullptr)
                                                contentToken = deltaObj->getProperty("content").toString();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                // Ollama: {"message":{"content":"token"},"done":false}
                auto json = juce::JSON::parse(line);
                if (json.isObject())
                {
                    auto* obj = json.getDynamicObject();
                    if (obj != nullptr)
                    {
                        // Extract content
                        auto msgVal = obj->getProperty("message");
                        if (msgVal.isObject())
                        {
                            auto* msgObj = msgVal.getDynamicObject();
                            if (msgObj != nullptr)
                                contentToken = msgObj->getProperty("content").toString();
                        }

                        // Check done flag
                        auto doneVal = obj->getProperty("done");
                        if (doneVal.isBool() && static_cast<bool>(doneVal))
                            isDone = true;
                    }
                }
            }

            if (contentToken.isNotEmpty())
            {
                fullResponse += contentToken;
                // Enviar token al message thread
                if (request.onToken)
                {
                    juce::String tokenCopy = contentToken;
                    auto tokenCb = request.onToken;
                    juce::MessageManager::callAsync([tokenCb, tokenCopy]()
                    {
                        tokenCb(tokenCopy);
                    });
                }
            }

            if (isDone)
                break;
        }

        // ─── Limpiar ────────────────────────────────────────────────────
        inputStream.reset();

        // ─── Entregar resultado ────────────────────────────────────────
        if (hasError)
        {
            LogHelper::writeToLog("[LlmClient] Streaming error: " + errorMsg);
            if (request.onStreamComplete)
            {
                juce::MessageManager::callAsync(
                    [cb = request.onStreamComplete, errorMsg]()
                    {
                        cb({}, errorMsg);
                    });
            }
        }
        else
        {
            LogHelper::writeToLog("[LlmClient] Streaming completado ("
                                  + juce::String(fullResponse.length()) + " chars)");
            if (request.onStreamComplete)
            {
                juce::String responseCopy = fullResponse;
                auto completeCb = request.onStreamComplete;
                juce::MessageManager::callAsync([completeCb, responseCopy]()
                {
                    completeCb(responseCopy, {});
                });
            }
        }

        return !hasError;
    }
    catch (const std::exception& e)
    {
        LogHelper::writeToLog("[LlmClient] Stream exception: " + juce::String(e.what()));
        if (request.onStreamComplete)
        {
            juce::String errMsg = "[LlmClient] Error: " + juce::String(e.what());
            juce::MessageManager::callAsync(
                [cb = request.onStreamComplete, errMsg]()
                {
                    cb({}, errMsg);
                });
        }
        return false;
    }
    catch (...)
    {
        LogHelper::writeToLog("[LlmClient] Stream unknown exception");
        if (request.onStreamComplete)
        {
            juce::MessageManager::callAsync(
                [cb = request.onStreamComplete]()
                {
                    cb({}, "[LlmClient] Error desconocido en streaming");
                });
        }
        return false;
    }
}

} // namespace mixcoach

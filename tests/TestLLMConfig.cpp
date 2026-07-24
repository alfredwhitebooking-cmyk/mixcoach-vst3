// ═══════════════════════════════════════════════════════════════════════════
//  TestLLMConfig — Verifica que los helpers de configuración del LlmClient
//  devuelvan URLs, modelos y parámetros correctos.
//
//  Helpers verificados:
//    • makeQwen25Config()          → Ollama, qwen2.5:7b, 60s timeout
//    • makeLightweightConfig()     → Ollama, llama3.2:3b, 30s timeout
//    • makeDeepSeekOllamaConfig()  → Ollama, deepseek-r1:7b, 60s timeout
//    • makeOpenRouterFallback()    → OpenAI, openrouter.ai, meta-llama/llama-3.2-3b-instruct:free
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>

#include "Source/MixCoach/ai/LlmClient.h"

using namespace mixcoach;
using Provider = LlmClient::Provider;

// ─── Test framework minimalista ─────────────────────────────────────────────
static int g_pass = 0, g_fail = 0;

static void CHECK(bool condition, const char* expr, const char* msg) {
    if (condition) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("  FAIL: %s — %s\n", expr, msg);
    }
}

static void CHECK_STREQ(const juce::String& actual, const char* expected,
                         const char* field) {
    bool ok = (actual == juce::String(expected));
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("  FAIL: %s — expected \"%s\", got \"%s\"\n",
               field, expected, actual.toRawUTF8());
    }
}

static void CHECK_FEQ(float actual, float expected, float tol,
                       const char* field) {
    bool ok = std::abs(actual - expected) <= tol;
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("  FAIL: %s — expected %.4f, got %.4f\n",
               field, expected, actual);
    }
}

static void CHECK_INTEQ(int actual, int expected, const char* field) {
    if (actual == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("  FAIL: %s — expected %d, got %d\n",
               field, expected, actual);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 1: makeQwen25Config — Configuración para Qwen2.5 7B local
// ═══════════════════════════════════════════════════════════════════════════
static void testMakeQwen25Config() {
    printf("── TEST 1: makeQwen25Config() ──\n");

    auto cfg = LlmClient::makeQwen25Config();

    CHECK(cfg.provider == Provider::Ollama,
          "provider == Ollama",
          "Qwen2.5 7B debe usar Ollama como provider");

    CHECK_STREQ(cfg.endpointUrl, "http://localhost:11434", "endpointUrl");
    CHECK_STREQ(cfg.model, "qwen2.5:7b", "model");
    CHECK_FEQ(cfg.temperature, 0.7f, 0.001f, "temperature");
    CHECK_INTEQ(cfg.maxTokens, 1024, "maxTokens");
    CHECK_INTEQ(cfg.timeoutMs, 60000, "timeoutMs");

    // Debe tener API key vacía (Ollama no requiere key)
    CHECK(cfg.apiKey.isEmpty(), "apiKey.isEmpty()",
          "Ollama no necesita API key");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 2: makeLightweightConfig — Configuración para modelo ligero 3B
// ═══════════════════════════════════════════════════════════════════════════
static void testMakeLightweightConfig() {
    printf("\n── TEST 2: makeLightweightConfig() ──\n");

    auto cfg = LlmClient::makeLightweightConfig();

    CHECK(cfg.provider == Provider::Ollama,
          "provider == Ollama",
          "Modelo ligero debe usar Ollama como provider");

    CHECK_STREQ(cfg.endpointUrl, "http://localhost:11434", "endpointUrl");
    CHECK_STREQ(cfg.model, "llama3.2:3b", "model");
    CHECK_FEQ(cfg.temperature, 0.7f, 0.001f, "temperature");
    CHECK_INTEQ(cfg.maxTokens, 1024, "maxTokens");
    CHECK_INTEQ(cfg.timeoutMs, 30000, "timeoutMs (30s para modelo ligero)");

    CHECK(cfg.apiKey.isEmpty(), "apiKey.isEmpty()",
          "Ollama no necesita API key");

    // Verificar que el timeout es MENOR que el de makeQwen25Config (modelo
    // ligero debe ser más rápido y por tanto tener timeout menor)
    auto qwenCfg = LlmClient::makeQwen25Config();
    CHECK(cfg.timeoutMs < qwenCfg.timeoutMs,
          "lightweight timeout < qwen timeout",
          "Modelo 3B debe tener timeout menor que 7B (30s < 60s)");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 3: makeDeepSeekOllamaConfig — Configuración para DeepSeek R1 7B
// ═══════════════════════════════════════════════════════════════════════════
static void testMakeDeepSeekOllamaConfig() {
    printf("\n── TEST 3: makeDeepSeekOllamaConfig() ──\n");

    auto cfg = LlmClient::makeDeepSeekOllamaConfig();

    CHECK(cfg.provider == Provider::Ollama,
          "provider == Ollama",
          "DeepSeek R1 via Ollama usa provider Ollama");

    CHECK_STREQ(cfg.endpointUrl, "http://localhost:11434", "endpointUrl");
    CHECK_STREQ(cfg.model, "deepseek-r1:7b", "model");
    CHECK_FEQ(cfg.temperature, 0.7f, 0.001f, "temperature");
    CHECK_INTEQ(cfg.maxTokens, 1024, "maxTokens");
    CHECK_INTEQ(cfg.timeoutMs, 60000, "timeoutMs (60s para modelo 7B)");

    CHECK(cfg.apiKey.isEmpty(), "apiKey.isEmpty()",
          "Ollama no necesita API key");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 4: makeOpenRouterFallback — Configuración para OpenRouter cloud
// ═══════════════════════════════════════════════════════════════════════════
static void testMakeOpenRouterFallback() {
    printf("\n── TEST 4: makeOpenRouterFallback() ──\n");

    juce::String testKey = "sk-test-key-12345";
    auto cfg = LlmClient::makeOpenRouterFallback(testKey);

    CHECK(cfg.provider == Provider::OpenAICompatible,
          "provider == OpenAICompatible",
          "OpenRouter usa provider OpenAICompatible");

    CHECK_STREQ(cfg.endpointUrl, "https://openrouter.ai/api/v1", "endpointUrl");
    CHECK_STREQ(cfg.model, "meta-llama/llama-3.2-3b-instruct:free", "model (default)");
    CHECK_STREQ(cfg.apiKey, testKey.toRawUTF8(), "apiKey");
    CHECK_FEQ(cfg.temperature, 0.7f, 0.001f, "temperature");
    CHECK_INTEQ(cfg.maxTokens, 800, "maxTokens (800 para OpenRouter)");
    CHECK_INTEQ(cfg.timeoutMs, 15000, "timeoutMs (15s para fallback)");

    // Verificar con modelo personalizado
    auto cfgCustom = LlmClient::makeOpenRouterFallback(
        "sk-custom",
        "deepseek/deepseek-v4-flash");

    CHECK_STREQ(cfgCustom.model, "deepseek/deepseek-v4-flash", "model (custom)");
    CHECK_STREQ(cfgCustom.apiKey, "sk-custom", "apiKey (custom)");
    CHECK(cfgCustom.provider == Provider::OpenAICompatible,
          "provider == OpenAICompatible",
          "OpenRouter custom también usa OpenAICompatible");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 5: All Ollama configs share the same endpoint URL
// ═══════════════════════════════════════════════════════════════════════════
static void testEndpointConsistency() {
    printf("\n── TEST 5: Endpoint URL Consistency ──\n");

    auto qwen   = LlmClient::makeQwen25Config();
    auto light  = LlmClient::makeLightweightConfig();
    auto deep   = LlmClient::makeDeepSeekOllamaConfig();

    CHECK_STREQ(qwen.endpointUrl, "http://localhost:11434", "qwen endpoint");
    CHECK_STREQ(light.endpointUrl, "http://localhost:11434", "light endpoint");
    CHECK_STREQ(deep.endpointUrl, "http://localhost:11434", "deep endpoint");

    // Todos los Ollama configs deben tener API key vacía
    CHECK(qwen.apiKey.isEmpty(),  "qwen apiKey vacía",  "Ollama no necesita key");
    CHECK(light.apiKey.isEmpty(), "light apiKey vacía", "Ollama no necesita key");
    CHECK(deep.apiKey.isEmpty(),  "deep apiKey vacía",  "Ollama no necesita key");
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════
int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  TestLLMConfig — LlmClient Configuration Helper Tests          ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    testMakeQwen25Config();
    testMakeLightweightConfig();
    testMakeDeepSeekOllamaConfig();
    testMakeOpenRouterFallback();
    testEndpointConsistency();

    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("════════════════════════════════════════════════════════════════\n\n");

    return (g_fail > 0) ? 1 : 0;
}

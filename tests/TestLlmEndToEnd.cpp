// ═══════════════════════════════════════════════════════════════════════════
//  TestLlmEndToEnd.cpp — Pipeline end-to-end con LLM real (DeepSeek API)
//
//  Verifica que el LLM real, al recibir el system prompt de MixCoach
//  con instrucciones de UI commands, emita comandos JSON estructurados
//  correctamente en sus respuestas.
//
//  Pipeline completo:
//    buildSystemPrompt() + buildChatContext() → HTTP POST → LLM →
//    processResponse() → comandos UI ejecutados via callbacks
//
//  Requisitos:
//    - DEEPSEEK_API_KEY en environment (o pasar por argumento)
//    - Conexión a internet
//
//  Build:
//    cmake --build build --config Release --target TestLlmEndToEnd
//  Run:
//    DEEPSEEK_API_KEY=sk-xxx build/tests/Release/TestLlmEndToEnd.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../Source/MixCoach/ai/AiCoachAdapter.h"
#include "../Source/MixCoach/engine/LlmCommandInterpreter.h"

// ─── Test framework ─────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  \xE2\x9D\x8C FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        gTestsFailed++; \
    } else { \
        std::printf("  \xE2\x9C\x85 PASS: %s\n", name); \
        gTestsPassed++; \
    } \
} while(0)

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════
//  Helper: Llama a API compatible OpenAI vía HTTP POST
//  Por defecto usa OpenRouter + deepseek/deepseek-v4-flash.
//  Retorna la respuesta JSON completa como string (vacío si error).
// ═══════════════════════════════════════════════════════════════════════════
static juce::String callLlmApi(const juce::String& systemPrompt,
                                const juce::String& userPrompt,
                                const juce::String& apiKey,
                                const juce::String& endpoint = "https://openrouter.ai/api/v1/chat/completions",
                                const char* model = "deepseek/deepseek-v4-flash")
{
    if (apiKey.isEmpty()) {
        std::printf("  [SKIP] No API key disponible\n");
        return {};
    }

    // ─── Construir payload ───────────────────────────────────────────────
    juce::Array<juce::var> messagesArr;

    auto sysMsg = juce::DynamicObject::Ptr(new juce::DynamicObject());
    sysMsg->setProperty("role", "system");
    sysMsg->setProperty("content", systemPrompt);
    messagesArr.add(juce::var(sysMsg));

    auto usrMsg = juce::DynamicObject::Ptr(new juce::DynamicObject());
    usrMsg->setProperty("role", "user");
    usrMsg->setProperty("content", userPrompt);
    messagesArr.add(juce::var(usrMsg));

    auto root = juce::DynamicObject::Ptr(new juce::DynamicObject());
    root->setProperty("model", model);
    root->setProperty("messages", messagesArr);
    root->setProperty("temperature", 0.7);
    root->setProperty("max_tokens", 800);
    root->setProperty("stream", false);

    juce::String jsonBody = juce::JSON::toString(juce::var(root), false);

    // ─── Enviar HTTP POST ───────────────────────────────────────────────
    juce::String extraHeaders = "Content-Type: application/json\r\n"
                                "Authorization: Bearer " + apiKey + "\r\n";

    juce::URL url(endpoint);
    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    auto inputStream = url.withPOSTData(jsonBody).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
            .withExtraHeaders(extraHeaders)
            .withConnectionTimeoutMs(30000)
            .withResponseHeaders(&responseHeaders)
            .withStatusCode(&statusCode));

    if (inputStream == nullptr) {
        std::printf("  [ERROR] Error de conexión a %s\n", endpoint.toRawUTF8());
        return {};
    }

    juce::String response = inputStream->readEntireStreamAsString();

    if (statusCode != 200) {
        std::printf("  [ERROR] HTTP %d: %s\n", statusCode,
                    response.substring(0, 200).toRawUTF8());
        return {};
    }

    // ─── Parsear respuesta ──────────────────────────────────────────────
    auto resultJson = juce::JSON::parse(response);
    if (!resultJson.isObject()) {
        std::printf("  [ERROR] Respuesta no es JSON válido\n");
        return {};
    }

    auto* obj = resultJson.getDynamicObject();
    if (obj == nullptr) {
        std::printf("  [ERROR] No se pudo parsear objeto JSON\n");
        return {};
    }

    // Extraer content del mensaje
    auto choices = obj->getProperty("choices");
    if (!choices.isArray() || choices.getArray()->size() == 0) {
        std::printf("  [ERROR] No choices en respuesta\n");
        return {};
    }

    auto firstChoice = (*choices.getArray())[0];
    auto* choiceObj = firstChoice.getDynamicObject();
    if (choiceObj == nullptr) {
        std::printf("  [ERROR] choice no es objeto\n");
        return {};
    }

    auto* msgObj = choiceObj->getProperty("message").getDynamicObject();
    if (msgObj == nullptr) {
        std::printf("  [ERROR] message no es objeto\n");
        return {};
    }

    return msgObj->getProperty("content").toString().trim();
}

// ═══════════════════════════════════════════════════════════════════════════
//  CallbackRecorder — Captura comandos UI ejecutados
// ═══════════════════════════════════════════════════════════════════════════
struct CallbackRecorder {
    int revealPanelCount = 0;
    int setCoachStateCount = 0;
    int switchTabCount = 0;
    int highlightTrackCount = 0;
    int celebrateCount = 0;
    int setModeCount = 0;
    int returnToCoachCount = 0;
    int advancePhaseCount = 0;
    int showReportCount = 0;
    int showSuggestionsCount = 0;

    juce::String lastPanel;
    juce::String lastState;
    juce::String lastTab;
    juce::String lastTrack;
    int lastDomain = -1;
    juce::String lastCelebrationMessage;

    void wireTo(LlmCommandInterpreter& interp) {
        interp.onRevealPanel = [this](const juce::String& panel) {
            revealPanelCount++; lastPanel = panel;
            std::printf("    >> Callback: reveal_panel(%s)\n", panel.toRawUTF8());
        };
        interp.onSetCoachState = [this](const juce::String& state) {
            setCoachStateCount++; lastState = state;
            std::printf("    >> Callback: set_coach_state(%s)\n", state.toRawUTF8());
        };
        interp.onSwitchTab = [this](const juce::String& tab) {
            switchTabCount++; lastTab = tab;
            std::printf("    >> Callback: switch_tab(%s)\n", tab.toRawUTF8());
        };
        interp.onHighlightTrack = [this](const juce::String& track, int domain) {
            highlightTrackCount++; lastTrack = track; lastDomain = domain;
            std::printf("    >> Callback: highlight_track(%s, domain=%d)\n",
                       track.toRawUTF8(), domain);
        };
        interp.onCelebrate = [this](const juce::String& message) {
            celebrateCount++; lastCelebrationMessage = message;
            std::printf("    >> Callback: celebrate(%s)\n", message.toRawUTF8());
        };
        interp.onSetMode = [this](bool isMixMode) {
            setModeCount++;
            std::printf("    >> Callback: set_mode(mix=%d)\n", isMixMode);
        };
        interp.onReturnToCoach = [this]() {
            returnToCoachCount++;
            std::printf("    >> Callback: return_to_coach()\n");
        };
        interp.onAdvancePhase = [this]() {
            advancePhaseCount++;
            std::printf("    >> Callback: advance_phase()\n");
        };
        interp.onShowReport = [this]() {
            showReportCount++;
            std::printf("    >> Callback: show_report()\n");
        };
        interp.onShowSuggestions = [this](const std::vector<juce::String>& suggestions) {
            showSuggestionsCount++;
            std::printf("    >> Callback: show_suggestions(%zu items)\n", suggestions.size());
        };
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 1: DeepSeek API connectivity
// ═══════════════════════════════════════════════════════════════════════════
static void test_api_connectivity(const juce::String& apiKey) {
    std::printf("\n── Test 1: API Connectivity ──\n");

    bool hasKey = apiKey.isNotEmpty();
    TEST("DEEPSEEK_API_KEY configurada", hasKey);

    if (!hasKey) {
        std::printf("  Saltando tests de API (no hay API key)\n");
        return;
    }

    // Probar endpoint de modelos para verificar conectividad
    // OpenRouter: GET /api/v1/models (con API key en header)
    juce::String modelsEndpoint = "https://openrouter.ai/api/v1/models";
    juce::String extraHeaders = "Authorization: Bearer " + apiKey + "\r\n";

    juce::URL url(modelsEndpoint);
    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    auto inputStream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders(extraHeaders)
            .withConnectionTimeoutMs(10000)
            .withResponseHeaders(&responseHeaders)
            .withStatusCode(&statusCode));

    bool connected = (inputStream != nullptr && statusCode == 200);
    TEST("OpenRouter API responde (HTTP 200)", connected);

    if (!connected) {
        std::printf("  Status: HTTP %d — verifica API key y conexión\n", statusCode);
        if (inputStream != nullptr) {
            juce::String body = inputStream->readEntireStreamAsString();
            if (body.isNotEmpty()) std::printf("  Response: %s\n", body.substring(0, 300).toRawUTF8());
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 2: LLM recibe instrucciones UI y emite comandos JSON
//
//  Escenario: El usuario pide consejo sobre un track (kick).
//  El LLM debe emitir highlight_track + posiblemente otros comandos.
// ═══════════════════════════════════════════════════════════════════════════
static void test_llm_emits_highlight_track(const juce::String& apiKey) {
    std::printf("\n── Test 2: LLM emite highlight_track al hablar de un track ──\n");

    if (apiKey.isEmpty()) {
        std::printf("  [SKIP] No API key\n");
        return;
    }

    // ─── Crear dependencias ─────────────────────────────────────────────
    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // Registrar un track de ejemplo (kick)
    sd->getSlotRegistry().registerSlot("kick",
        juce::Colours::blue, BusType::Drums);
    adapter.setTrackIntent(0, { "Kick", "Punchy" });

    // ─── Cablear LlmCommandInterpreter ──────────────────────────────────
    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    // ─── Construir prompts ──────────────────────────────────────────────
    juce::String systemPrompt = adapter.buildSystemPrompt();
    juce::String userMsg = "Oye, el kick suena como que esta recortando. Que deberia hacer?";
    juce::String fullPrompt = adapter.buildChatContext() + "\n[USER]\n" + userMsg;

    std::printf("  System prompt: %d chars\n", systemPrompt.length());
    std::printf("  Full user prompt: %d chars\n", fullPrompt.length());

    // ─── Enviar a OpenRouter ────────────────────────────────────────────
    std::printf("  Enviando a OpenRouter (deepseek/deepseek-v4-flash)...\n");
    juce::String llmResponse = callLlmApi(systemPrompt, fullPrompt, apiKey);
    TEST("2.1 API devolvió respuesta", llmResponse.isNotEmpty());

    if (llmResponse.isEmpty()) {
        std::printf("  Saltando resto del test (no hay respuesta)\n");
        return;
    }

    std::printf("  Respuesta del LLM (%d chars):\n", llmResponse.length());
    std::printf("  ──────────────────────────────────────────────────────────\n");
    std::printf("  %s\n", llmResponse.toRawUTF8());
    std::printf("  ──────────────────────────────────────────────────────────\n");

    // ─── Procesar respuesta ─────────────────────────────────────────────
    juce::String cleanText = interp.processResponse(llmResponse);
    std::printf("\n  Texto limpio (%d chars):\n", cleanText.length());
    std::printf("  %s\n", cleanText.toRawUTF8());

    // ─── Verificar comandos ─────────────────────────────────────────────
    int totalCallbacks = rec.revealPanelCount + rec.setCoachStateCount +
                         rec.switchTabCount + rec.highlightTrackCount +
                         rec.celebrateCount + rec.setModeCount +
                         rec.returnToCoachCount + rec.advancePhaseCount +
                         rec.showReportCount + rec.showSuggestionsCount;

    std::printf("\n  Comandos ejecutados: %d\n", totalCallbacks);
    std::printf("    reveal_panel:      %d\n", rec.revealPanelCount);
    std::printf("    set_coach_state:   %d\n", rec.setCoachStateCount);
    std::printf("    switch_tab:        %d\n", rec.switchTabCount);
    std::printf("    highlight_track:   %d\n", rec.highlightTrackCount);
    std::printf("    celebrate:         %d\n", rec.celebrateCount);
    std::printf("    return_to_coach:   %d\n", rec.returnToCoachCount);
    std::printf("    advance_phase:     %d\n", rec.advancePhaseCount);
    std::printf("    show_report:       %d\n", rec.showReportCount);
    std::printf("    show_suggestions:  %d\n", rec.showSuggestionsCount);

    // El test pasa si el LLM emitió AL MENOS 1 comando UI.
    // highlight_track es el esperado (habla del kick),
    // pero cualquier comando UI válido es aceptable.
    TEST("2.2 LLM emitió al menos 1 comando UI", totalCallbacks >= 1);

    // Registrar qué comando(s) se ejecutaron
    if (rec.highlightTrackCount > 0) {
        TEST("2.3 LLM emitió highlight_track con track correcto",
             rec.lastTrack == "kick");
        TEST("2.4 Domain es valido (0-3)",
             rec.lastDomain >= 0 && rec.lastDomain <= 3);
    }
    if (rec.switchTabCount > 0) {
        TEST("2.5 switch_tab a tab valido",
             rec.lastTab == "tools" || rec.lastTab == "coach" || rec.lastTab == "session");
    }
    if (rec.celebrateCount > 0) {
        TEST("2.6 Mensaje de celebrate no vacío",
             rec.lastCelebrationMessage.isNotEmpty());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 3: LLM puede responder sin comandos UI (modo default)
//
//  Escenario: El usuario hace una pregunta general que NO requiere UI.
//  El LLM debe responder sin comandos UI.
// ═══════════════════════════════════════════════════════════════════════════
static void test_llm_no_ui_commands_needed(const juce::String& apiKey) {
    std::printf("\n── Test 3: LLM responde sin UI commands (pregunta general) ──\n");

    if (apiKey.isEmpty()) {
        std::printf("  [SKIP] No API key\n");
        return;
    }

    // ─── Crear dependencias ─────────────────────────────────────────────
    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    // ─── Pregunta conceptual que NO debería requerir UI ──────────────────
    juce::String systemPrompt = adapter.buildSystemPrompt();
    juce::String userMsg = "Cual es la diferencia entre compresor y limiter?";
    juce::String fullPrompt = adapter.buildChatContext() + "\n[USER]\n" + userMsg;

    std::printf("  Enviando pregunta conceptual a OpenRouter...\n");
    juce::String llmResponse = callLlmApi(systemPrompt, fullPrompt, apiKey);
    TEST("3.1 API devolvió respuesta", llmResponse.isNotEmpty());

    if (llmResponse.isEmpty()) return;

    std::printf("  Respuesta del LLM (%d chars):\n", llmResponse.length());
    std::printf("  %s\n", llmResponse.toRawUTF8());

    // ─── Procesar respuesta ─────────────────────────────────────────────
    juce::String cleanText = interp.processResponse(llmResponse);

    int totalCallbacks = rec.revealPanelCount + rec.setCoachStateCount +
                         rec.switchTabCount + rec.highlightTrackCount +
                         rec.celebrateCount + rec.setModeCount +
                         rec.returnToCoachCount + rec.advancePhaseCount +
                         rec.showReportCount + rec.showSuggestionsCount;

    std::printf("\n  Comandos UI ejecutados: %d\n", totalCallbacks);

    // El LLM podría o no emitir comandos. No forzamos un resultado,
    // solo registramos. El test no falla si hay o no comandos.
    std::printf("  [INFO] El LLM emitió %d comandos UI\n", totalCallbacks);
    if (totalCallbacks > 0) {
        std::printf("  [INFO] Comandos: highlight_track=%d switch_tab=%d celebrate=%d\n",
                   rec.highlightTrackCount, rec.switchTabCount, rec.celebrateCount);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 4: LLM en Master Mode — comandos relevantes
//
//  Escenario: Modo Master, el usuario reporta que terminó.
//  El LLM debe emitir celebrate + show_report.
// ═══════════════════════════════════════════════════════════════════════════
static void test_llm_master_mode(const juce::String& apiKey) {
    std::printf("\n── Test 4: LLM en Master Mode ──\n");

    if (apiKey.isEmpty()) {
        std::printf("  [SKIP] No API key\n");
        return;
    }

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // Cambiar a Master Mode
    engine.setCoachMode(CoachMode::Master);
    engine.setMasterDestination(MasterDestination::StreamingGeneral);

    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    juce::String systemPrompt = adapter.buildSystemPrompt();
    juce::String userMsg = "Listo! Termine el master check. Como vamos?";
    juce::String fullPrompt = adapter.buildChatContext() + "\n[USER]\n" + userMsg;

    std::printf("  Enviando a OpenRouter (Master Mode)...\n");
    juce::String llmResponse = callLlmApi(systemPrompt, fullPrompt, apiKey);
    TEST("4.1 API devolvió respuesta (Master)", llmResponse.isNotEmpty());

    if (llmResponse.isEmpty()) return;

    std::printf("  Respuesta del LLM (%d chars):\n", llmResponse.length());
    std::printf("  %s\n", llmResponse.toRawUTF8());

    juce::String cleanText = interp.processResponse(llmResponse);

    int totalCallbacks = rec.revealPanelCount + rec.switchTabCount +
                         rec.celebrateCount + rec.showReportCount +
                         rec.advancePhaseCount + rec.returnToCoachCount;
    std::printf("\n  Comandos en Master Mode: %d\n", totalCallbacks);
    std::printf("    celebrate:      %d\n", rec.celebrateCount);
    std::printf("    show_report:    %d\n", rec.showReportCount);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 5: JSON commands syntax check
//
//  Verifica que el LLM emite JSON con la sintaxis correcta
//  (```json ... ```) y que el parser puede extraerlo.
// ═══════════════════════════════════════════════════════════════════════════
static void test_json_syntax(const juce::String& apiKey) {
    std::printf("\n── Test 5: Sintaxis JSON de los comandos ──\n");

    if (apiKey.isEmpty()) {
        std::printf("  [SKIP] No API key\n");
        return;
    }

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // Registrar algunos tracks
    sd->getSlotRegistry().registerSlot("kick", juce::Colours::blue, BusType::Drums);
    sd->getSlotRegistry().registerSlot("bajo", juce::Colours::red, BusType::Bass);
    adapter.setTrackIntent(0, { "Kick", "Punchy" });
    adapter.setTrackIntent(1, { "Bajo", "Deep" });

    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    // Pedir consejo específico que debería provocar highlight_track + switch_tab
    juce::String systemPrompt = adapter.buildSystemPrompt();
    juce::String userMsg = "El kick y el bajo estan peleando en el sub. Que hago?";
    juce::String fullPrompt = adapter.buildChatContext() + "\n[USER]\n" + userMsg;

    std::printf("  Enviando a OpenRouter...\n");
    juce::String llmResponse = callLlmApi(systemPrompt, fullPrompt, apiKey);
    TEST("5.1 API devolvió respuesta", llmResponse.isNotEmpty());

    if (llmResponse.isEmpty()) return;

    // Verificar sintaxis JSON
    bool hasJsonBlock = llmResponse.contains("```json");
    TEST("5.2 Respuesta contiene bloque ```json", hasJsonBlock);

    // Parsear manualmente usando raw C strings (evita overload ambiguity de indexOf en MSVC)
    if (hasJsonBlock) {
        const char* raw = llmResponse.toRawUTF8();
        const char* jsonStartPtr = std::strstr(raw, "```json");
        TEST("5.3 Bloque JSON encontrado", jsonStartPtr != nullptr);

        if (jsonStartPtr != nullptr) {
            const char* jsonEndPtr = std::strstr(jsonStartPtr + 7, "```");
            bool hasClosingTag = (jsonEndPtr != nullptr);
            TEST("5.3b Bloque JSON tiene cierre ```", hasClosingTag);

            if (hasClosingTag) {
                juce::String jsonBlock = llmResponse.substring(
                    static_cast<int>(jsonStartPtr - raw + 7),
                    static_cast<int>(jsonEndPtr - raw));
                jsonBlock = jsonBlock.trim();
                TEST("5.4 Bloque JSON no vacío", jsonBlock.isNotEmpty());

                // Verificar que contiene "ui" array
                bool hasUiArray = (std::strstr(jsonBlock.toRawUTF8(), "\"ui\"") != nullptr);
                TEST("5.5 JSON contiene array \"ui\"", hasUiArray);

                // Verificar que cada objeto tiene "action"
                const char* blockRaw = jsonBlock.toRawUTF8();
                int actionCount = 0;
                const char* ap = blockRaw;
                while (true) {
                    ap = std::strstr(ap, "\"action\"");
                    if (ap == nullptr) break;
                    actionCount++;
                    ap += 8;
                }
                TEST("5.6 Al menos 1 accion definida", actionCount >= 1);
                std::printf("    Acciones encontradas en JSON: %d\n", actionCount);
            }
        }
    }

    // Procesar y mostrar qué comandos se ejecutaron
    interp.processResponse(llmResponse);
    std::printf("  Comandos ejecutados tras procesar:\n");
    std::printf("    highlight_track:   %d\n", rec.highlightTrackCount);
    std::printf("    switch_tab:        %d\n", rec.switchTabCount);
    std::printf("    reveal_panel:      %d\n", rec.revealPanelCount);
    std::printf("    return_to_coach:   %d\n", rec.returnToCoachCount);
    std::printf("    celebrate:         %d\n", rec.celebrateCount);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════════════════╗\n");
    std::printf("║  TestLlmEndToEnd — Pipeline LLM real (DeepSeek API)       ║\n");
    std::printf("╚══════════════════════════════════════════════════════════════╝\n");

    // Leer API key de environment o argumento
    juce::String apiKey;
    if (argc >= 2 && std::strlen(argv[1]) > 0) {
        apiKey = argv[1];
    } else {
        // Usar JUCE para leer environment (evita C4996 de MSVC con std::getenv)
        apiKey = juce::SystemStats::getEnvironmentVariable("DEEPSEEK_API_KEY", {});
    }

    if (apiKey.isEmpty()) {
        std::printf("\n⚠  No API key encontrada.\n");
        std::printf("  Pasa la key como argumento: %s sk-or-v1-...\n\n", argv[0]);
    }

    // ─── Ejecutar tests ──────────────────────────────────────────────
    test_api_connectivity(apiKey);
    test_llm_emits_highlight_track(apiKey);
    test_llm_no_ui_commands_needed(apiKey);
    test_llm_master_mode(apiKey);
    test_json_syntax(apiKey);

    // ─── Resultados ──────────────────────────────────────────────────
    std::printf("\n════════════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("════════════════════════════════════════════════════════════════\n");

    return gTestsFailed > 0 ? 1 : 0;
}

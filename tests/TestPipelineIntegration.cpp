// ═══════════════════════════════════════════════════════════════════════════
//  TestPipelineIntegration.cpp — Tests de integración del pipeline completo
//
//  Verifica que:
//    • buildSystemPrompt() + LlmCommandInterpreter cableado funcionan juntos
//    • buildFullContext() produce contexto válido sin crashear
//    • buildChatContext() produce contexto válido sin crashear
//    • Una respuesta mock del LLM con JSON commands se procesa correctamente
//      a través del pipeline: adapter → commandInterpreter → callbacks
//
//  Build: cmake --build build --config Release --target TestPipelineIntegration
//  Run:   build/tests/Release/TestPipelineIntegration.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../Source/MixCoach/ai/AiCoachAdapter.h"
#include "../Source/MixCoach/engine/LlmCommandInterpreter.h"

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        gTestsFailed++; \
    } else { \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name); \
        gTestsPassed++; \
    } \
} while(0)

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════
//  Helper: setupTrackAudioResult — Inyecta datos de telemetría en SharedData
// ═══════════════════════════════════════════════════════════════════════════
static void setupTrackAudioResult(mixcoach::SharedData& sd, int slotIndex,
                                   float peakDb, float rmsDb)
{
    mixcoach::TrackAudioResult result;
    result.peakLeft     = peakDb;
    result.peakRight    = peakDb;
    result.rmsLeft      = rmsDb;
    result.rmsRight     = rmsDb;
    result.correlation  = 0.8f;
    result.timestampUs  = juce::Time::getMillisecondCounter() * 1000;
    for (int b = 0; b < 30; ++b)
        result.bandEnergies[b] = -50.0f + (b * 1.5f);
    for (int b = 0; b < 6; ++b) {
        result.crestPerBand[b]       = 10.0f;
        result.stereoWidthPerBand[b] = 0.3f;
    }
    sd.updateTrackAudioResult(slotIndex, result);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Helper: CallbackRecorder — same as TestLlmCommandInterpreter
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
    bool lastIsMixMode = true;
    std::vector<juce::String> lastSuggestions;

    void wireTo(LlmCommandInterpreter& interp) {
        interp.onRevealPanel = [this](const juce::String& panel) {
            revealPanelCount++;
            lastPanel = panel;
        };
        interp.onSetCoachState = [this](const juce::String& state) {
            setCoachStateCount++;
            lastState = state;
        };
        interp.onSwitchTab = [this](const juce::String& tab) {
            switchTabCount++;
            lastTab = tab;
        };
        interp.onHighlightTrack = [this](const juce::String& track, int domain) {
            highlightTrackCount++;
            lastTrack = track;
            lastDomain = domain;
        };
        interp.onCelebrate = [this](const juce::String& message) {
            celebrateCount++;
            lastCelebrationMessage = message;
        };
        interp.onSetMode = [this](bool isMixMode) {
            setModeCount++;
            lastIsMixMode = isMixMode;
        };
        interp.onReturnToCoach = [this]() {
            returnToCoachCount++;
        };
        interp.onAdvancePhase = [this]() {
            advancePhaseCount++;
        };
        interp.onShowReport = [this]() {
            showReportCount++;
        };
        interp.onShowSuggestions = [this](const std::vector<juce::String>& suggestions) {
            showSuggestionsCount++;
            lastSuggestions = suggestions;
        };
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  1. Full Pipeline: buildSystemPrompt + buildChatContext + mock LLM → processResponse
//
//  Simula el flujo COMPLETO del AiCoachAdapter cuando recibe una respuesta
//  del LLM que contiene comandos JSON estructurados.
//  Verifica que los callbacks se disparen correctamente.
// ═══════════════════════════════════════════════════════════════════════════

static void test_full_pipeline_mock_response() {
    std::printf("\n── Pipeline Completo: systemPrompt → mock LLM → processResponse ──\n");

    // ─── Crear dependencias ─────────────────────────────────────────
    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // ─── Cablear LlmCommandInterpreter con callbacks ───────────────
    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    // ─── PASO 1: buildSystemPrompt() → verificar que tiene UI commands ──
    juce::String systemPrompt = adapter.buildSystemPrompt();
    TEST("1.1 buildSystemPrompt() no vacío", systemPrompt.isNotEmpty());
    TEST("1.2 System prompt tiene [UI COMMANDS]",
         systemPrompt.contains("[UI COMMANDS"));
    TEST("1.3 System prompt tiene CUANDO USAR sections",
         systemPrompt.contains("CUANDO USAR"));

    // ─── PASO 2: buildChatContext() → contexto ligero ───────────────
    // buildChatContext() NO incluye DBFS SCALE GUIDE (solo buildFullContext).
    juce::String chatContext = adapter.buildChatContext();
    TEST("2.1 buildChatContext() no vacío", chatContext.isNotEmpty());
    TEST("2.2 Chat context tiene SNAPSHOT", chatContext.contains("SNAPSHOT"));
    TEST("2.3 Chat context tiene Mode:",
         chatContext.contains("Mode:"));

    // ─── PASO 3: Simular respuesta del LLM con JSON commands ──────
    // Esto es lo mismo que hace askLlam() internamente:
    //   1. buildSystemPrompt() + buildChatContext() se envían al LLM
    //   2. La respuesta del LLM incluye ```json { "ui": [...] } ```
    //   3. processResponse() extrae los comandos y los ejecuta
    juce::String mockLlmResponse =
        "Oye, escucha el kick, esta recortando. Prueba bajarle 3dB.\n"
        "```json\n"
        "{\n"
        "  \"ui\": [\n"
        "    { \"action\": \"highlight_track\", \"track\": \"kick\", \"domain\": 0 },\n"
        "    { \"action\": \"switch_tab\", \"tab\": \"tools\" },\n"
        "    { \"action\": \"celebrate\", \"message\": \"Gain staging listo!\" },\n"
        "    { \"action\": \"advance_phase\" }\n"
        "  ]\n"
        "}\n"
        "```\n";

    juce::String cleanText = interp.processResponse(mockLlmResponse);

    // ─── PASO 4: Verificar texto limpio ────────────────────────────
    TEST("4.1 Texto limpio no contiene ```json", !cleanText.contains("```json"));
    TEST("4.2 Texto contiene el mensaje del coach",
         cleanText.contains("kick") && cleanText.contains("recortando"));

    // ─── PASO 5: Verificar que los 4 comandos se ejecutaron ───────
    TEST("5.1 highlight_track ejecutado", rec.highlightTrackCount == 1);
    TEST("5.2 Track es 'kick'", rec.lastTrack == "kick");
    TEST("5.3 Domain es 0 (gain)", rec.lastDomain == 0);
    TEST("5.4 switch_tab ejecutado", rec.switchTabCount == 1);
    TEST("5.5 Tab es 'tools'", rec.lastTab == "tools");
    TEST("5.6 celebrate ejecutado", rec.celebrateCount == 1);
    TEST("5.7 Mensaje de celebrate correcto",
         rec.lastCelebrationMessage == "Gain staging listo!");
    TEST("5.8 advance_phase ejecutado", rec.advancePhaseCount == 1);

    std::printf("    Pipeline completo verificado: 4 comandos ejecutados\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  2. Full Pipeline: Mock con patrón CUANDO USAR → show + return_to_coach
//
//  Simula el escenario más común: mostrar evidencia + volver al chat.
//  highlight_track + switch_tab + return_to_coach
// ═══════════════════════════════════════════════════════════════════════════

static void test_pipeline_show_evidence_and_return() {
    std::printf("\n── Pipeline: mostrar evidencia + volver al chat ──\n");

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    // Confirmar que el interpreter está cableado
    TEST("CommandInterpreter cableado al adapter",
         adapter.getCommandInterpreter() == &interp);
    TEST("CommandInterpreter tiene callbacks", interp.hasAnyCallbacks());

    // Mock LLM response con patrón "mostrar evidencia + volver"
    juce::String mockResponse =
        "Mira el espectro del bajo, esta peleando con el kick.\n"
        "```json\n"
        "{\n"
        "  \"ui\": [\n"
        "    { \"action\": \"highlight_track\", \"track\": \"bajo\", \"domain\": 1 },\n"
        "    { \"action\": \"switch_tab\", \"tab\": \"tools\" },\n"
        "    { \"action\": \"reveal_panel\", \"panel\": \"messengers\" },\n"
        "    { \"action\": \"return_to_coach\" }\n"
        "  ]\n"
        "}\n"
        "```\n";

    juce::String cleanText = interp.processResponse(mockResponse);

    TEST("Texto limpio preserva mensaje", cleanText.contains("espectro del bajo"));
    TEST("highlight_track ejecutado con track=bajo",
         rec.highlightTrackCount == 1 && rec.lastTrack == "bajo");
    TEST("domain=1 (tonal)", rec.lastDomain == 1);
    TEST("switch_tab ejecutado", rec.switchTabCount == 1);
    TEST("Tab es tools", rec.lastTab == "tools");
    TEST("reveal_panel ejecutado", rec.revealPanelCount == 1);
    TEST("Panel es messengers", rec.lastPanel == "messengers");
    TEST("return_to_coach ejecutado", rec.returnToCoachCount == 1);

    std::printf("    Patron 'evidencia + retorno' verificado: 4 comandos ejecutados\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  3. buildFullContext() — Contexto completo
//
//  Verifica que buildFullContext() produce un string válido con todas
//  las secciones esperadas. No requiere tracks activos — prueba en vacío.
// ═══════════════════════════════════════════════════════════════════════════

static void test_build_full_context_output() {
    std::printf("\n── buildFullContext: contexto completo ──\n");

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    juce::String ctx = adapter.buildFullContext();

    TEST("3.1 buildFullContext() no vacío", ctx.isNotEmpty());
    TEST("3.2 Tiene DBFS SCALE GUIDE", ctx.contains("DBFS SCALE GUIDE"));
    TEST("3.3 Tiene CONFIDENCE section", ctx.contains("[CONFIDENCE]"));
    TEST("3.4 Tiene PHASE section", ctx.contains("[PHASE]"));
    TEST("3.5 Tiene TRACKS (con 'No active' o datos)", ctx.contains("TRACKS"));
    // buildFullContext(): REFERENCE solo aparece si hay referencia cargada (ninguna aqui)
    // CONVERSATION HISTORY solo aparece si hay historial (no hay conversacion aqui)
    // [END SNAPSHOT] solo aparece en buildChatContext(), no en buildFullContext
    TEST("3.6 NO tiene REFERENCE (sin referencia cargada)",
         !ctx.contains("REFERENCE"));
    TEST("3.7 NO tiene CONVERSATION HISTORY (sin conversacion)",
         !ctx.contains("CONVERSATION HISTORY"));
    TEST("3.8 NO tiene END SNAPSHOT (solo buildChatContext)",
         !ctx.contains("END SNAPSHOT"));

    std::printf("    buildFullContext() length: %d chars\n", ctx.length());
}

// ═══════════════════════════════════════════════════════════════════════════
//  4. buildChatContext() — Contexto ligero (~3KB)
//
//  buildChatContext() es el contexto que se envía en cada request al LLM.
//  Debe incluir SNAPSHOT con datos del master + TRACKS + sesión.
// ═══════════════════════════════════════════════════════════════════════════

static void test_build_chat_context_output() {
    std::printf("\n── buildChatContext: contexto ligero ──\n");

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    juce::String ctx = adapter.buildChatContext();

    // buildChatContext() NO incluye DBFS SCALE GUIDE (solo buildFullContext)
    TEST("4.1 buildChatContext() no vacío", ctx.isNotEmpty());
    TEST("4.2 Tiene SNAPSHOT header", ctx.contains("[SNAPSHOT"));
    TEST("4.3 Tiene Mode:", ctx.contains("Mode:"));
    TEST("4.4 Tiene Mode: MIX o MASTER",
         ctx.contains("Mode: MIX") || ctx.contains("Mode: MASTER"));
    TEST("4.5 Tiene Progress:", ctx.contains("Progress:"));
    TEST("4.6 Tiene [END SNAPSHOT]", ctx.contains("[END SNAPSHOT]"));

    std::printf("    buildChatContext() length: %d chars\n", ctx.length());
}

// ═══════════════════════════════════════════════════════════════════════════
//  5. Master Mode Pipeline — buildSystemPrompt + processResponse en Master
//
//  Verifica que el pipeline también funciona en Master Mode.
//  Master Mode no debería tener acciones de mezcla (highlight_track, etc.).
// ═══════════════════════════════════════════════════════════════════════════

static void test_pipeline_master_mode() {
    std::printf("\n── Pipeline: Master Mode ──\n");

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

    // Verificar system prompt en Master Mode
    juce::String systemPrompt = adapter.buildSystemPrompt();
    TEST("5.1 Master Mode prompt tiene [UI COMMANDS]",
         systemPrompt.contains("[UI COMMANDS"));
    TEST("5.2 Master Mode prompt tiene MASTER MODE identity",
         systemPrompt.contains("MASTER MODE"));
    TEST("5.3 Master Mode: UI commands antes de TUS SENTIDOS",
         systemPrompt.indexOf("[UI COMMANDS") < systemPrompt.indexOf("TUS SENTIDOS"));

    // Mock respuesta en Master Mode: solo comandos relevantes
    juce::String mockResponse =
        "Listo para el master check.\n"
        "```json\n"
        "{\n"
        "  \"ui\": [\n"
        "    { \"action\": \"celebrate\", \"message\": \"Master check completado!\" },\n"
        "    { \"action\": \"show_report\" }\n"
        "  ]\n"
        "}\n"
        "```\n";

    juce::String cleanText = interp.processResponse(mockResponse);
    TEST("5.4 Texto limpio preserva mensaje", cleanText.contains("master check"));
    TEST("5.5 celebrate ejecutado", rec.celebrateCount == 1);
    TEST("5.6 Mensaje de celebrate correcto",
         rec.lastCelebrationMessage == "Master check completado!");
    TEST("5.7 show_report ejecutado", rec.showReportCount == 1);

    std::printf("    Pipeline en Master Mode verificado\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  6. Pipeline: Sin comandos UI (solo texto) — no hay cambios
//
//  Verifica que cuando el LLM NO incluye JSON commands, el texto pasa
//  sin cambios y no se disparan callbacks.
// ═══════════════════════════════════════════════════════════════════════════

static void test_pipeline_no_ui_commands() {
    std::printf("\n── Pipeline: sin comandos UI (solo texto) ──\n");

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    LlmCommandInterpreter interp;
    CallbackRecorder rec;
    rec.wireTo(interp);
    adapter.setCommandInterpreter(&interp);

    // Respuesta del LLM sin JSON commands
    juce::String response = "La mezcla suena bien. El balance de frecuencias esta correcto.";
    juce::String result = interp.processResponse(response);

    TEST("6.1 Texto sin cambios", result == response);
    TEST("6.2 Ningun callback disparado",
         rec.revealPanelCount == 0 &&
         rec.setCoachStateCount == 0 &&
         rec.switchTabCount == 0 &&
         rec.highlightTrackCount == 0 &&
         rec.celebrateCount == 0 &&
         rec.setModeCount == 0 &&
         rec.returnToCoachCount == 0 &&
         rec.advancePhaseCount == 0 &&
         rec.showReportCount == 0 &&
         rec.showSuggestionsCount == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  7. buildFullContext + buildChatContext con 3 tracks + referencia
//
//  Escenario de producción: 3 tracks (kick, bass, vocal) con audio data,
//  intents, género, referencia cargada, sesión configurada.
//  Verifica que todos los bloques del contexto se generen correctamente.
// ═══════════════════════════════════════════════════════════════════════════

static void test_build_full_context_with_tracks() {
    std::printf("\n── buildFullContext + buildChatContext con 3 tracks + referencia ──\n");

    // ─── Crear dependencias ───────────────────────────────────────────────
    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // ─── Configurar escenario realista ────────────────────────────────────
    adapter.setGenre("Reggaeton");
    adapter.setEngineerName("DJ Mix");
    adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Intermediate);

    // ─── Registrar 3 tracks con audio data ────────────────────────────────
    // Track 0: Kick (Drums, clipping!)
    sd->getSlotRegistry().registerSlot("kick",
        juce::Colours::red, BusType::Drums);
    setupTrackAudioResult(*sd, 0, -0.1f, -8.0f);
    adapter.setTrackIntent(0, { "Kick", "Punchy" });
    engine.setTrackRole(0, TrackRole::Kick);

    // Track 1: Bajo (Bass, healthy level)
    sd->getSlotRegistry().registerSlot("bajo",
        juce::Colours::blue, BusType::Bass);
    setupTrackAudioResult(*sd, 1, -8.0f, -16.0f);
    adapter.setTrackIntent(1, { "Bajo", "Deep" });
    engine.setTrackRole(1, TrackRole::BassSub);

    // Track 2: Voz (Vocals, moderate level)
    sd->getSlotRegistry().registerSlot("voz",
        juce::Colours::purple, BusType::Vocals);
    setupTrackAudioResult(*sd, 2, -12.0f, -20.0f);
    adapter.setTrackIntent(2, { "Voz Principal", "Clean" });
    engine.setTrackRole(2, TrackRole::VozPrincipal);

    // ─── Cargar referencia (URL sin descarga, solo nombre) ────────────────
    engine.setReferenceURL("Mi Referencia", "https://example.com/ref.wav");

    // ─── Avanzar fase para simular progreso ───────────────────────────────
    pm.advanceToNextPhase(); // Organizacion → GainStaging
    engine.periodicAnalysis();

    // ═══════════════════════════════════════════════════════════════════════
    //  PARTE A: buildSystemPrompt()
    // ═══════════════════════════════════════════════════════════════════════
    {
        juce::String sys = adapter.buildSystemPrompt();
        TEST("A1 System prompt no vacío", sys.isNotEmpty());
        TEST("A2 Tiene UI COMMANDS", sys.contains("[UI COMMANDS"));
        TEST("A3 Tiene nombre del ingeniero", sys.contains("DJ Mix"));
        TEST("A4 Tiene género Reggaeton", sys.contains("Reggaeton"));
        TEST("A5 Tiene INTERMEDIATE MODE", sys.contains("INTERMEDIATE MODE"));
        TEST("A6 Tiene REASONING PROTOCOL step 6", sys.contains("QUE COMANDOS UI"));
        std::printf("    System prompt: %d chars\n", sys.length());
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  PARTE B: buildFullContext() — Contexto completo con datos reales
    // ═══════════════════════════════════════════════════════════════════════
    juce::String ctx;
    {
        ctx = adapter.buildFullContext();
        std::printf("    Full context: %d chars\n", ctx.length());

        // Secciones esperadas
        TEST("B1 No vacío", ctx.isNotEmpty());
        TEST("B2 Tiene DBFS SCALE GUIDE", ctx.contains("DBFS SCALE GUIDE"));
        TEST("B3 Tiene SNAPSHOT del master", ctx.contains("[SNAPSHOT"));
        TEST("B4 Tiene Mode: MIX", ctx.contains("Mode: MIX"));
        TEST("B5 Tiene 3 tracks activos", ctx.contains("3 tracks active") || ctx.contains("3 active"));
        TEST("B6 Tiene CONFIDENCE section", ctx.contains("[CONFIDENCE]"));
        TEST("B7 Tiene PHASE section", ctx.contains("[PHASE]"));

        // Track data should be present
        TEST("B8 Tiene tracks en contexto", ctx.contains("TRACKS"));
        TEST("B9 Menciona kick", ctx.contains("kick"));
        TEST("B10 Menciona bajo", ctx.contains("bajo"));
        TEST("B11 Menciona voz", ctx.contains("voz"));

        // Reference section (loaded)
        TEST("B12 Tiene REFERENCE section", ctx.contains("[REFERENCE]"));
        TEST("B13 Menciona nombre de referencia", ctx.contains("Mi Referencia"));

        // Spectral / bus data
        TEST("B14 Tiene buses", ctx.contains("Drums") || ctx.contains("Bass") || ctx.contains("Vocals"));
        TEST("B15 Tiene Progress", ctx.contains("Progress:"));

        // No conversation yet
        TEST("B16 NO tiene CONVERSATION HISTORY", !ctx.contains("CONVERSATION HISTORY"));
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  PARTE C: buildChatContext() — Contexto ligero con datos
    // ═══════════════════════════════════════════════════════════════════════
    {
        juce::String chatCtx = adapter.buildChatContext();
        std::printf("    Chat context: %d chars\n", chatCtx.length());

        TEST("C1 No vacío", chatCtx.isNotEmpty());
        TEST("C2 Tiene SNAPSHOT", chatCtx.contains("[SNAPSHOT"));
        TEST("C3 Tiene Mode: MIX o MASTER",
             chatCtx.contains("Mode: MIX") || chatCtx.contains("Mode: MASTER"));
        TEST("C4 Tiene Progress:", chatCtx.contains("Progress:"));
        TEST("C5 Tiene END SNAPSHOT", chatCtx.contains("[END SNAPSHOT]"));
        TEST("C6 Menciona kick", chatCtx.contains("kick"));
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  PARTE D: Pipeline con tracks registrados + respuesta mock
    //  Verifica que los callbacks funcionan con nombres de tracks reales
    // ═══════════════════════════════════════════════════════════════════════
    {
        LlmCommandInterpreter interp;
        CallbackRecorder rec;
        rec.wireTo(interp);
        adapter.setCommandInterpreter(&interp);

        juce::String mockResponse =
            "El bajo necesita mas cuerpo.\n"
            "```json\n"
            "{\n"
            "  \"ui\": [\n"
            "    { \"action\": \"highlight_track\", \"track\": \"bajo\", \"domain\": 1 },\n"
            "    { \"action\": \"switch_tab\", \"tab\": \"tools\" },\n"
            "    { \"action\": \"return_to_coach\" }\n"
            "  ]\n"
            "}\n"
            "```\n";

        juce::String cleanText = interp.processResponse(mockResponse);
        TEST("D1 Texto limpio preserva mensaje", cleanText.contains("bajo necesita mas cuerpo"));
        TEST("D2 highlight_track ejecutado", rec.highlightTrackCount == 1);
        TEST("D3 Track es bajo", rec.lastTrack == "bajo");
        TEST("D4 Domain=1 (tonal)", rec.lastDomain == 1);
        TEST("D5 switch_tab ejecutado", rec.switchTabCount == 1);
        TEST("D6 Tab es tools", rec.lastTab == "tools");
        TEST("D7 return_to_coach ejecutado", rec.returnToCoachCount == 1);

        std::printf("    Pipeline con 3 tracks verificado\n");
    }

    std::printf("    buildFullContext en escenario realista verificado\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::printf("\n====================================================");
    std::printf("\n  TestPipelineIntegration — Full Pipeline Verification");
    std::printf("\n====================================================\n");

    // ─── Pipeline completo ───────────────────────────────────
    test_full_pipeline_mock_response();

    // ─── Patrón evidencia + retorno ──────────────────────────
    test_pipeline_show_evidence_and_return();

    // ─── buildFullContext ────────────────────────────────────
    test_build_full_context_output();

    // ─── buildChatContext ────────────────────────────────────
    test_build_chat_context_output();

    // ─── Master Mode ─────────────────────────────────────────
    test_pipeline_master_mode();

    // ─── Sin comandos UI ─────────────────────────────────────
    test_pipeline_no_ui_commands();

    // ─── 3 tracks + referencia (producción) ──────────────────
    test_build_full_context_with_tracks();

    // ─── Results ─────────────────────────────────────────────
    std::printf("\n====================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("====================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}

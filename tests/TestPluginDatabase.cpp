// ═══════════════════════════════════════════════════════════════════════════
//  TestPluginDatabase.cpp — Unit tests para PluginDatabase problemCache_
//
//  Verifica:
//    • rebuildCache() puebla problemCache_ correctamente
//    • findByProblem() retorna resultados del cache ordenados por rating
//    • findByProblem() retorna vacío para tipos sin plugins compatibles
//    • findByProblem() es estable (mismos resultados en múltiples llamadas)
//    • findByProblem() con maxResults respeta el límite
//
//  Build: cmake --build build --config Release --target TestPluginDatabase
//  Run:   build/tests/Release/TestPluginDatabase.exe
//
//  Dependencias: juce_core + juce_graphics + juce_data_structures (JSON)
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_data_structures/juce_data_structures.h>

#include "../Source/MixCoach/engine/PluginDatabase.h"

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

// ═══════════════════════════════════════════════════════════════════════════
//  Fixture: carga plugin_db.json desde CWD (build/tests/Release/ → ../../../)
// ═══════════════════════════════════════════════════════════════════════════

/** Retorna la ruta a plugin_db.json relativa al directorio del test. */
static juce::String findDatabasePath()
{
    juce::String paths[] = {
        // Desde build/tests/Release/
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("../../../plugins/plugin_db.json").getFullPathName(),
        // Desde build/tests/
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("../../plugins/plugin_db.json").getFullPathName(),
        // Desde CWD raíz del proyecto
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("plugins/plugin_db.json").getFullPathName(),
    };

    for (auto& p : paths) {
        if (juce::File(p).existsAsFile())
            return p;
    }

    return {};
}

/** Inicializa el singleton PluginDatabase con la DB real.
    @return true si se cargó correctamente. */
static bool loadTestDatabase()
{
    auto& db = mixcoach::PluginDatabase::getInstance();

    // Si ya está cargada de una llamada anterior, está bien
    if (db.isLoaded())
        return true;

    juce::String dbPath = findDatabasePath();
    if (dbPath.isEmpty()) {
        std::printf("  \xe2\x9a\xa0 SKIP: plugin_db.json not found\n");
        return false;
    }

    return db.load(dbPath);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: isLoaded y estado del singleton
// ═══════════════════════════════════════════════════════════════════════════

static void test_is_loaded()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 isLoaded and singleton state \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();

    // El singleton debe existir incluso sin load()
    TEST("getInstance returns valid reference", true);

    // Después de load() debe estar cargada
    if (!db.isLoaded()) {
        bool loaded = loadTestDatabase();
        TEST("database loaded successfully", loaded);
    } else {
        TEST("database was already loaded", true);
    }

    TEST("isLoaded returns true after load", db.isLoaded());
    TEST("getAllPlugins is not empty after load",
         !db.getAllPlugins().empty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: findByProblem — cache lookup y orden por rating
// ═══════════════════════════════════════════════════════════════════════════

static void test_find_by_problem_gain()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: Gain \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    auto results = db.findByProblem(mixcoach::ProblemType::Gain);

    TEST("Gain problem returns non-empty results", !results.empty());

    if (!results.empty()) {
        // Gain tiene: Fruity Balance (4.5), Melda MUtility (4.0), FabFilter Pro-G (4.5)
        // Todos deben ser compatibles con Gain
        TEST("first result is compatible with Gain", results[0].isCompatible(mixcoach::ProblemType::Gain));
        TEST("first result has non-empty name", results[0].name.isNotEmpty());
        TEST("first result has positive rating", results[0].rating > 0.0f);

        // Verificar que el primero tiene un tier válido
        bool hasValidTier = (results[0].tier != mixcoach::PluginTier::Unknown);
        TEST("first result has valid tier", hasValidTier);

        // Verificar que Fruity Balance está en los resultados
        bool foundBalance = false;
        bool foundProG = false;
        for (auto& r : results) {
            if (r.name == "Fruity Balance") foundBalance = true;
            if (r.name == "FabFilter Pro-G") foundProG = true;
        }
        TEST("Fruity Balance is in Gain results", foundBalance);
        TEST("FabFilter Pro-G is in Gain results", foundProG);
    }
}

static void test_find_by_problem_clipping()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: Clipping \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    auto results = db.findByProblem(mixcoach::ProblemType::Clipping);

    TEST("Clipping problem returns non-empty results", !results.empty());

    if (!results.empty()) {
        // Clipping tiene: Fruity Limiter (4.0), Ozone 11 (5.0), Fruity Balance (4.5 via config)
        // La DB debería tenerlos ordenados por rating descendente
        for (size_t i = 1; i < results.size(); ++i) {
            TEST("results sorted by rating descending",
                 results[i-1].rating >= results[i].rating - 0.01f);
        }

        // Verificar que Fruity Limiter está (si no lo filtra por compatibilidad)
        bool foundLimiter = false;
        for (auto& r : results) {
            if (r.id == "fruity_limiter") foundLimiter = true;
        }
        TEST("Fruity Limiter is in Clipping results", foundLimiter);
    }
}

static void test_find_by_problem_tonal_excess()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: TonalExcess (multi-plugin, order) \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // Usar maxResults=10 para obtener TODOS los plugins de TonalExcess
    // (4 plugins: Pro-Q4 5.0, Ozone 11 5.0, ParamEQ2 4.5, TDR Nova 4.5)
    auto results = db.findByProblem(mixcoach::ProblemType::TonalExcess, 10);

    TEST("TonalExcess returns all 4 plugins", (int)results.size() == 4);

    if (!results.empty()) {
        // El primero debe ser 5.0 (FabFilter Pro-Q 4 o Ozone 11)
        TEST("first result has best rating (5.0)",
             results[0].rating >= 4.99f);

        // Orden descendente
        for (size_t i = 1; i < results.size(); ++i) {
            TEST("TonalExcess sorted by rating descending",
                 results[i-1].rating >= results[i].rating - 0.01f);
        }

        // Verificar plugins específicos
        bool foundProQ4 = false;
        bool foundParamEQ2 = false;
        bool foundNova = false;
        bool foundOzone = false;
        for (auto& r : results) {
            if (r.id == "fabfilter_pro_q4") foundProQ4 = true;
            if (r.id == "fruity_param_eq2") foundParamEQ2 = true;
            if (r.id == "tdr_nova") foundNova = true;
            if (r.id == "ozone_11") foundOzone = true;
        }
        TEST("FabFilter Pro-Q 4 is in results", foundProQ4);
        TEST("Fruity Param EQ 2 is in results", foundParamEQ2);
        TEST("TDR Nova is in results", foundNova);
        TEST("Ozone 11 is in results", foundOzone);
    }
}

static void test_find_by_problem_reverb()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: Reverb \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // Usar maxResults=10 para obtener TODOS los plugins de Reverb
    // (5 plugins: Valhalla VintageVerb 5.0, Seventh Heaven 5.0,
    //  Valhalla Supermassive 4.5, Fruity Reeverb 2 4.0, Dragonfly Reverb 4.0)
    auto results = db.findByProblem(mixcoach::ProblemType::Reverb, 10);

    TEST("Reverb problem returns all 5 plugins", (int)results.size() == 5);

    if (!results.empty()) {
        for (size_t i = 1; i < results.size(); ++i) {
            TEST("Reverb sorted by rating descending",
                 results[i-1].rating >= results[i].rating - 0.01f);
        }

        // Verificar multi-tier
        bool foundNative = false;
        bool foundFree = false;
        bool foundPremium = false;
        for (auto& r : results) {
            if (r.tier == mixcoach::PluginTier::Native)  foundNative = true;
            if (r.tier == mixcoach::PluginTier::Free)    foundFree = true;
            if (r.tier == mixcoach::PluginTier::Premium) foundPremium = true;
        }
        TEST("Reverb has Native plugins", foundNative);
        TEST("Reverb has Free plugins", foundFree);
        TEST("Reverb has Premium plugins", foundPremium);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Edge cases — tipos sin plugins, Unknown, límite maxResults
// ═══════════════════════════════════════════════════════════════════════════

static void test_find_by_problem_unknown()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: Unknown (edge case) \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    auto results = db.findByProblem(mixcoach::ProblemType::Unknown);
    TEST("Unknown problem returns empty", results.empty());
}

static void test_find_by_problem_saturation()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: Saturation (not in DB) \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // Saturation no está en plugin_db.json → debe retornar vacío
    auto results = db.findByProblem(mixcoach::ProblemType::Saturation);
    TEST("Saturation problem returns empty (no plugins in DB)", results.empty());
}

static void test_find_by_problem_max_results()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: maxResults limit \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // TonalExcess tiene 4 plugins, pedimos maxResults=2
    auto results = db.findByProblem(mixcoach::ProblemType::TonalExcess, 2);

    TEST("maxResults=2 returns at most 2 results", (int)results.size() <= 2);
    TEST("maxResults=2 returns at least 1 result", !results.empty());
}

static void test_find_by_problem_default_max()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: default maxResults (3) \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // Default es maxResults=3
    auto results = db.findByProblem(mixcoach::ProblemType::TonalExcess);
    TEST("default maxResults returns at most 3 results", (int)results.size() <= 3);
}

static void test_find_by_problem_zero_max()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 findByProblem: maxResults=0 \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    auto results = db.findByProblem(mixcoach::ProblemType::Gain, 0);
    TEST("maxResults=0 returns empty", results.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Estabilidad del cache — múltiples llamadas retornan lo mismo
// ═══════════════════════════════════════════════════════════════════════════

static void test_cache_stability()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 Cache stability: same results across calls \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // Llamar 3 veces a findByProblem con Gain y verificar mismo tamaño
    auto r1 = db.findByProblem(mixcoach::ProblemType::Gain);
    auto r2 = db.findByProblem(mixcoach::ProblemType::Gain);
    auto r3 = db.findByProblem(mixcoach::ProblemType::Gain);

    TEST("first call returns results", !r1.empty());
    TEST("second call returns same count", (int)r1.size() == (int)r2.size());
    TEST("third call returns same count", (int)r1.size() == (int)r3.size());

    // Verificar nombres consistentes
    if (!r1.empty() && !r2.empty() && !r3.empty()) {
        TEST("first result name is stable across calls",
             r1[0].name == r2[0].name && r2[0].name == r3[0].name);
    }
}

static void test_cache_stability_multiple_types()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 Cache stability: multiple problem types \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    using PT = mixcoach::ProblemType;

    // Todos los tipos que DEBERÍAN tener plugins en la DB
    PT knownTypes[] = {
        PT::Gain, PT::Clipping, PT::Masking,
        PT::TonalExcess, PT::TonalDeficit,
        PT::DynamicsOvercompressed, PT::DynamicsTooDynamic,
        PT::Phase, PT::Spatial, PT::Reverb,
        PT::Limiting
    };

    for (auto t : knownTypes) {
        auto first  = db.findByProblem(t);
        auto second = db.findByProblem(t);

        std::printf("    problem type: %s\n", mixcoach::problemTypeToString(t));
        TEST("stable count matches across calls",
             (int)first.size() == (int)second.size());

        if (!first.empty() && !second.empty()) {
            TEST("stable first result matches across calls",
                 first[0].name == second[0].name);
        }
    }

    // Tipos que NO tienen plugins
    PT emptyTypes[] = { PT::Unknown, PT::Saturation };
    for (auto t : emptyTypes) {
        auto results = db.findByProblem(t);
        std::printf("    empty type: %s\n", mixcoach::problemTypeToString(t));
        TEST("empty result for problem type", results.empty());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getSuggestionsByTier — usa internamente plugin_ directamente
//  (verificación de que el fix de dangling pointers no regresiona)
// ═══════════════════════════════════════════════════════════════════════════

static void test_get_suggestions_by_tier()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 getSuggestionsByTier: correct tier distribution \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    auto sugs = db.getSuggestionsByTier(mixcoach::ProblemType::Gain);

    TEST("Gain suggestions returns results", !sugs.empty());
    TEST("Gain suggestions has at most 3 entries (1 per tier)",
         (int)sugs.size() <= 3);

    if (!sugs.empty()) {
        // Todas deben ser válidas
        for (auto& s : sugs) {
            TEST("suggestion is valid", s.isValid());
            if (s.plugin != nullptr) {
                TEST("suggestion has non-empty plugin name", s.plugin->name.isNotEmpty());
                TEST("suggestion has valid tier",
                     s.plugin->tier != mixcoach::PluginTier::Unknown);
            }
        }
    }
}

static void test_get_suggestions_by_tier_all_types()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 getSuggestionsByTier: all known types \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    using PT = mixcoach::ProblemType;
    PT knownTypes[] = {
        PT::Gain, PT::Clipping, PT::Masking,
        PT::TonalExcess, PT::TonalDeficit,
        PT::DynamicsOvercompressed, PT::DynamicsTooDynamic,
        PT::Phase, PT::Spatial, PT::Reverb, PT::Limiting
    };

    for (auto t : knownTypes) {
        auto sugs = db.getSuggestionsByTier(t);

        if (sugs.empty()) {
            std::printf("  \xe2\x9a\xa0  %s returned 0 suggestions\n",
                         mixcoach::problemTypeToString(t));
        }

        std::printf("    suggestions for: %s (count=%zu)\n",
                     mixcoach::problemTypeToString(t), sugs.size());
        for (auto& s : sugs) {
            TEST("suggestion valid for problem type", s.isValid());
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getBestSuggestion — mejor plugin global
// ═══════════════════════════════════════════════════════════════════════════

static void test_get_best_suggestion()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 getBestSuggestion: best plugin per problem \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // TonalExcess → FabFilter Pro-Q 4 (5.0) o Ozone 11 (5.0)
    auto best = db.getBestSuggestion(mixcoach::ProblemType::TonalExcess);
    TEST("best suggestion for TonalExcess is valid", best.isValid());
    if (best.plugin != nullptr) {
        TEST("best suggestion has max rating (5.0)", best.plugin->rating >= 4.99f);
    }

    // Clipping → Ozone 11 (5.0) o Fruity Limiter (4.0)
    auto bestClip = db.getBestSuggestion(mixcoach::ProblemType::Clipping);
    TEST("best suggestion for Clipping is valid", bestClip.isValid());
    if (bestClip.plugin != nullptr) {
        TEST("best Clipping has rating >= 4.0", bestClip.plugin->rating >= 4.0f);
    }

    // Saturation (no existe) → inválida
    auto bestSat = db.getBestSuggestion(mixcoach::ProblemType::Saturation);
    TEST("best suggestion for Saturation is invalid", !bestSat.isValid());

    // Unknown → inválida
    auto bestUnk = db.getBestSuggestion(mixcoach::ProblemType::Unknown);
    TEST("best suggestion for Unknown is invalid", !bestUnk.isValid());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getById — lookup individual de plugin
// ═══════════════════════════════════════════════════════════════════════════

static void test_get_by_id()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 getById: individual plugin lookup \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    // IDs conocidos
    auto balance = db.getById("fruity_balance");
    TEST("getById finds fruity_balance", balance != nullptr);
    if (balance != nullptr) {
        TEST("fruity_balance name is 'Fruity Balance'", balance->name == "Fruity Balance");
        TEST("fruity_balance tier is Native",
             balance->tier == mixcoach::PluginTier::Native);
        TEST("fruity_balance rating is 4.5", balance->rating == 4.5f);
    }

    auto proQ4 = db.getById("fabfilter_pro_q4");
    TEST("getById finds fabfilter_pro_q4", proQ4 != nullptr);
    if (proQ4 != nullptr) {
        TEST("fabfilter_pro_q4 tier is Premium",
             proQ4->tier == mixcoach::PluginTier::Premium);
        TEST("fabfilter_pro_q4 rating is 5.0", proQ4->rating == 5.0f);
    }

    // ID inexistente
    auto nonexistent = db.getById("nonexistent_plugin_xyz");
    TEST("getById returns nullptr for nonexistent ID", nonexistent == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: formatSuggestions — texto formateado con datos reales
// ═══════════════════════════════════════════════════════════════════════════

static void test_format_suggestions()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 formatSuggestions: formatted text output \xe2\x94\x80\xe2\x94\x80\n");

    auto& db = mixcoach::PluginDatabase::getInstance();
    if (!db.isLoaded()) { std::printf("  \xe2\x9a\xa0 SKIP: DB not loaded\n"); return; }

    auto sugs = db.getSuggestionsByTier(mixcoach::ProblemType::Gain);
    TEST("formatSuggestions with suggestions returns non-empty",
         !db.formatSuggestions(sugs).isEmpty());

    TEST("formatSuggestions with track name returns non-empty",
         !db.formatSuggestions(sugs, "Kick").isEmpty());

    // Empty suggestions
    TEST("formatSuggestions with empty suggestions returns empty",
         db.formatSuggestions({}).isEmpty());

    // Empty suggestions with track name
    TEST("formatSuggestions with empty + track name returns empty",
         db.formatSuggestions({}, "Test").isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: interpolateAction — reemplazo de placeholders
// ═══════════════════════════════════════════════════════════════════════════

static void test_interpolate_action()
{
    std::printf("\n\xe2\x94\x80\xe2\x94\x80 interpolateAction: placeholder replacement \xe2\x94\x80\xe2\x94\x80\n");

    using DB = mixcoach::PluginDatabase;

    {
        auto result = DB::interpolateAction("reduce {delta} dB", -1.5f, 0.0f);
        std::printf("    delta placeholder: '%s'\n", result.toRawUTF8());
        TEST("delta placeholder", result == "reduce -1.5 dB");
    }
    {
        auto result = DB::interpolateAction("cut at {frequencyHz}", 0.0f, 100.0f);
        std::printf("    frequency Hz: '%s'\n", result.toRawUTF8());
        TEST("frequency Hz", result == "cut at 100 Hz");
    }
    {
        auto result = DB::interpolateAction("cut at {frequencyHz}", 0.0f, 2500.0f);
        std::printf("    frequency kHz: '%s'\n", result.toRawUTF8());
        TEST("frequency kHz", result == "cut at 2.5 kHz");
    }
    {
        auto result = DB::interpolateAction("cut at {freq}", 0.0f, 100.0f);
        std::printf("    freq alias: '%s'\n", result.toRawUTF8());
        TEST("freq alias", result == "cut at 100 Hz");
    }
    {
        auto result = DB::interpolateAction("adjust {channel}", 0.0f, 0.0f);
        std::printf("    channel: '%s'\n", result.toRawUTF8());
        TEST("channel", result == "adjust izquierdo");
    }
    {
        auto result = DB::interpolateAction("{delta} dB at {frequencyHz}", -1.5f, 100.0f);
        std::printf("    multiple placeholders: '%s'\n", result.toRawUTF8());
        TEST("multiple placeholders", result == "-1.5 dB at 100 Hz");
    }
    {
        auto result = DB::interpolateAction("", 0.0f, 0.0f);
        TEST("empty text", result.isEmpty());
    }
    {
        auto result = DB::interpolateAction("reduce gain", -1.5f, 0.0f);
        TEST("no placeholders", result == "reduce gain");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("====================================================================\n");
    std::printf("  PluginDatabase Unit Tests — problemCache_\n");
    std::printf("  findByProblem | getSuggestionsByTier | getBestSuggestion | getById\n");
    std::printf("====================================================================\n\n");

    // ─── Load database ──────────────────────────────────────────────────────
    if (!loadTestDatabase()) {
        std::printf("\n\xe2\x9d\x8c FATAL: Could not load plugin_db.json\n");
        std::printf("  Expected at: %s\n",
                     findDatabasePath().toRawUTF8());
        std::printf("  CWD: %s\n",
                     juce::File::getCurrentWorkingDirectory().getFullPathName().toRawUTF8());
        return 1;
    }

    // Print summary of loaded data
    {
        auto& db = mixcoach::PluginDatabase::getInstance();
        auto all = db.getAllPlugins();
        std::printf("  Database: %zu plugins loaded\n", all.size());
        int native=0, free=0, premium=0;
        for (auto& p : all) {
            if (p.tier == mixcoach::PluginTier::Native)  native++;
            if (p.tier == mixcoach::PluginTier::Free)    free++;
            if (p.tier == mixcoach::PluginTier::Premium) premium++;
        }
        std::printf("  Tiers: %d Native, %d Free, %d Premium\n", native, free, premium);
    }

    // ─── isLoaded and singleton state ───────────────────────────────────────
    test_is_loaded();

    // ─── findByProblem — cache lookup ───────────────────────────────────────
    test_find_by_problem_gain();
    test_find_by_problem_clipping();
    test_find_by_problem_tonal_excess();
    test_find_by_problem_reverb();

    // ─── Edge cases ─────────────────────────────────────────────────────────
    test_find_by_problem_unknown();
    test_find_by_problem_saturation();
    test_find_by_problem_max_results();
    test_find_by_problem_default_max();
    test_find_by_problem_zero_max();

    // ─── Cache stability ────────────────────────────────────────────────────
    test_cache_stability();
    test_cache_stability_multiple_types();

    // ─── getSuggestionsByTier ───────────────────────────────────────────────
    test_get_suggestions_by_tier();
    test_get_suggestions_by_tier_all_types();

    // ─── getBestSuggestion ──────────────────────────────────────────────────
    test_get_best_suggestion();

    // ─── getById ────────────────────────────────────────────────────────────
    test_get_by_id();

    // ─── formatSuggestions ──────────────────────────────────────────────────
    test_format_suggestions();

    // ─── interpolateAction ──────────────────────────────────────────────────
    test_interpolate_action();

    // ─── Results ────────────────────────────────────────────────────────────
    std::printf("\n====================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("====================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}

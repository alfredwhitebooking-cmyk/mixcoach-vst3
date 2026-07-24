// ═══════════════════════════════════════════════════════════════════════════
//  TestChatInlineCards.cpp — Unit tests para ChatMessagesComponent_InlineCards
//
//  Verifica:
//    • drawTrackGroupCard() — free function que renderiza la tarjeta inline
//    • addTrackGroupCard() — añade burbuja al ChatMessagesComponent
//
//  Build: cmake --build build --config Release --target TestChatInlineCards
//  Run:   build/tests/Release/TestChatInlineCards.exe
//
//  Dependencias: juce_core + juce_graphics + juce_gui_basics
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../Source/MixCoach/UI/CoachChatComponent.h"
#include "../Source/MixCoach/UI/TrackProblemCard.h"
#include "../Source/MixCoach/engine/PluginSuggestionsProvider.h"

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

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ─── Helpers: crear datos de prueba ──────────────────────────────────────
// drawTrackGroupCard() usa MixCoachTheme::accent() real durante el renderizado.
// Los tests verifican que el contenido es visible (píxeles no-transparentes)
// sin depender de colores específicos de la paleta.

// ═══════════════════════════════════════════════════════════════════════════
//  Helpers: crear datos de prueba
// ═══════════════════════════════════════════════════════════════════════════

/** Crea un TrackProblemGroup de prueba con N tracks. */
static mixcoach::TrackProblemGroup makeTestGroup(int numTracks = 3)
{
    mixcoach::TrackProblemGroup group;
    group.groupName = "Bateria";
    group.icon      = "\xF0\x9F\xA5\x81"; // 🥁
    group.colour    = juce::Colour(0xFFA855F7); // Purple (MixCoachTheme::accent())

    for (int i = 0; i < numTracks; ++i) {
        mixcoach::TrackProblemData track;
        track.slotIndex   = i;
        track.trackName   = "Track_" + juce::String(i + 1);
        track.roleName    = (i == 0) ? "Kick"
                          : (i == 1) ? "Snare"
                          : "HiHat";
        track.problemType = (i == 0) ? "Exceso subgraves"
                          : (i == 1) ? "Poco impacto"
                          : "Enmascaramiento";
        track.severity    = (i == 0) ? 0.9f
                          : (i == 1) ? 0.6f
                          : 0.3f;

        // Añadir sugerencias de plugin al primer track
        if (i == 0) {
            mixcoach::TrackPluginSuggestion native;
            native.tier       = mixcoach::TrackPluginSuggestion::Tier::Native;
            native.pluginName = "Fruity Balance";
            native.actionText = "-1.5 dB";

            mixcoach::TrackPluginSuggestion free;
            free.tier       = mixcoach::TrackPluginSuggestion::Tier::Free;
            free.pluginName = "TDR Nova";
            free.actionText = "-3dB Q2";

            mixcoach::TrackPluginSuggestion premium;
            premium.tier       = mixcoach::TrackPluginSuggestion::Tier::Premium;
            premium.pluginName = "Pro-Q 3";
            premium.actionText = "€169";

            track.pluginSuggestions.push_back(native);
            track.pluginSuggestions.push_back(free);
            track.pluginSuggestions.push_back(premium);
        }

        group.tracks.push_back(track);
    }

    return group;
}

/** Renderiza un Component a una imagen y retorna la imagen. */
static juce::Image renderComponent(juce::Component& comp, int w, int h)
{
    comp.setSize(w, h);
    juce::Image img(juce::Image::ARGB, w, h, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    juce::Graphics g(img);
    comp.paint(g);
    return img;
}

/** Retorna true si la imagen tiene contenido visible en al menos un píxel. */
static bool imageHasContent(const juce::Image& img)
{
    auto bounds = img.getBounds();
    int step = std::max(1, bounds.getWidth() / 20);
    for (int y = 0; y < bounds.getHeight(); y += step)
        for (int x = 0; x < bounds.getWidth(); x += step)
            if (img.getPixelAt(x, y).getARGB() != 0)
                return true;
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: drawTrackGroupCard — Free function rendering
// ═══════════════════════════════════════════════════════════════════════════

static void test_draw_track_group_card_basic()
{
    std::printf("\n── drawTrackGroupCard: Basic rendering ──\n");

    auto group = makeTestGroup(3);

    // Crear ChatBubble con datos
    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.trackGroup = group;

    // Renderizar a imagen
    juce::Image img(juce::Image::ARGB, 350, 200, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    {
        juce::Graphics g(img);
        float h = mixcoach::drawTrackGroupCard(g, {0, 0, 350, 200}, bubble, false);
        TEST("drawTrackGroupCard returns positive height", h > 0.0f);
        TEST("drawTrackGroupCard returns expected bounds height", h >= 120.0f && h <= 220.0f);
    }

    TEST("rendered image has visual content", imageHasContent(img));
}

static void test_draw_track_group_card_empty_group()
{
    std::printf("\n── drawTrackGroupCard: Empty group ──\n");

    auto group = makeTestGroup(0); // Cero tracks

    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.trackGroup = group;

    juce::Image img(juce::Image::ARGB, 300, 100, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    {
        juce::Graphics g(img);
        float h = mixcoach::drawTrackGroupCard(g, {0, 0, 300, 100}, bubble, false);
        // Con tracks vacío, retorna bounds.getHeight()
        TEST("empty group returns bounds height", h == 100.0f);
    }
}

static void test_draw_track_group_card_single_track()
{
    std::printf("\n── drawTrackGroupCard: Single track ──\n");

    auto group = makeTestGroup(1); // Un solo track

    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.trackGroup = group;

    juce::Image img(juce::Image::ARGB, 300, 120, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    {
        juce::Graphics g(img);
        float h = mixcoach::drawTrackGroupCard(g, {0, 0, 300, 120}, bubble, false);
        TEST("single track returns positive height", h > 0.0f);
    }

    TEST("single track renders content", imageHasContent(img));
}

static void test_draw_track_group_card_many_tracks()
{
    std::printf("\n── drawTrackGroupCard: Many tracks (5) ──\n");

    auto group = makeTestGroup(5); // Cinco tracks

    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.trackGroup = group;

    juce::Image img(juce::Image::ARGB, 350, 280, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    {
        juce::Graphics g(img);
        float h = mixcoach::drawTrackGroupCard(g, {0, 0, 350, 280}, bubble, false);
        TEST("many tracks returns positive height", h > 0.0f);
        // Debe ser más alto que el mínimo (5 tracks + header + footer ≈ 160px)
        TEST("many tracks produces taller render", h >= 160.0f);
    }

    TEST("many tracks renders content", imageHasContent(img));
}

static void test_draw_track_group_card_plugin_suggestions()
{
    std::printf("\n── drawTrackGroupCard: With plugin suggestions ──\n");

    auto group = makeTestGroup(1); // 1 track with 3 plugin suggestions

    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.trackGroup = group;

    juce::Image img(juce::Image::ARGB, 350, 160, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    {
        juce::Graphics g(img);
        float h = mixcoach::drawTrackGroupCard(g, {0, 0, 350, 160}, bubble, false);
        TEST("with plugin suggestions returns positive height", h > 0.0f);
    }

    TEST("with plugin suggestions renders content", imageHasContent(img));
}

static void test_draw_track_group_card_tier_icons()
{
    std::printf("\n── TrackPluginSuggestion: tierIcon and tierLabel ──\n");

    using Tier = mixcoach::TrackPluginSuggestion::Tier;

    // Verificar que los íconos no están vacíos
    TEST("Native tierIcon is not empty",
         std::strlen(mixcoach::TrackPluginSuggestion::tierIcon(Tier::Native)) > 0);
    TEST("Free tierIcon is not empty",
         std::strlen(mixcoach::TrackPluginSuggestion::tierIcon(Tier::Free)) > 0);
    TEST("Premium tierIcon is not empty",
         std::strlen(mixcoach::TrackPluginSuggestion::tierIcon(Tier::Premium)) > 0);
    TEST("UserHas tierIcon is not empty",
         std::strlen(mixcoach::TrackPluginSuggestion::tierIcon(Tier::UserHas)) > 0);

    // Verificar que los labels son distintos
    TEST("Native and Free tierLabels are different",
         std::strcmp(mixcoach::TrackPluginSuggestion::tierLabel(Tier::Native),
                     mixcoach::TrackPluginSuggestion::tierLabel(Tier::Free)) != 0);
    TEST("Premium and UserHas tierLabels are different",
         std::strcmp(mixcoach::TrackPluginSuggestion::tierLabel(Tier::Premium),
                     mixcoach::TrackPluginSuggestion::tierLabel(Tier::UserHas)) != 0);

    // Verificar contenido de labels
    TEST("Native tierLabel contains 'Nativo'",
         std::strstr(mixcoach::TrackPluginSuggestion::tierLabel(Tier::Native), "Nativo") != nullptr);
    TEST("Free tierLabel contains 'Gratis'",
         std::strstr(mixcoach::TrackPluginSuggestion::tierLabel(Tier::Free), "Gratis") != nullptr);
}

static void test_draw_track_group_card_various_widths()
{
    std::printf("\n── drawTrackGroupCard: Various widths ──\n");

    auto group = makeTestGroup(3);
    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.trackGroup = group;

    int widths[] = { 200, 300, 400, 500 };
    int idx = 0;
    for (int w : widths) {
        juce::Image img(juce::Image::ARGB, w, 180, true);
        img.clear(img.getBounds(), juce::Colour(0x00000000));
        {
            juce::Graphics g(img);
            float h = mixcoach::drawTrackGroupCard(g, {0, 0, (float)w, 180.0f}, bubble, false);
            TEST("width variation renders with positive height", h > 0.0f);
        }
        TEST("width variation produces visible content", imageHasContent(img));
        ++idx;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: PluginSuggestion struct helpers
// ═══════════════════════════════════════════════════════════════════════════

static void test_plugin_suggestion_struct()
{
    std::printf("\n── TrackPluginSuggestion: Struct construction ──\n");

    using Tier = mixcoach::TrackPluginSuggestion::Tier;

    mixcoach::TrackPluginSuggestion sug;
    sug.tier       = Tier::Native;
    sug.pluginName = "Fruity Balance";
    sug.actionText = "-1.5 dB";

    TEST("pluginName stored correctly", sug.pluginName == "Fruity Balance");
    TEST("actionText stored correctly", sug.actionText == "-1.5 dB");
    TEST("tier is Native", sug.tier == Tier::Native);
}

static void test_plugin_suggestion_all_tiers()
{
    std::printf("\n── TrackPluginSuggestion: All tier values ──\n");

    using Tier = mixcoach::TrackPluginSuggestion::Tier;

    // Verificar que todos los valores del enum existen y tienen icon+label
    Tier tiers[] = { Tier::Native, Tier::Free, Tier::Premium, Tier::UserHas };
    for (auto t : tiers) {
        auto icon  = mixcoach::TrackPluginSuggestion::tierIcon(t);
        auto label = mixcoach::TrackPluginSuggestion::tierLabel(t);
        TEST("tier has non-empty icon", std::strlen(icon) > 0);
        TEST("tier has non-empty label", std::strlen(label) > 0);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackProblemGroup / TrackProblemData structs
// ═══════════════════════════════════════════════════════════════════════════

static void test_track_problem_group_construction()
{
    std::printf("\n── TrackProblemGroup: Construction and data ──\n");

    auto group = makeTestGroup(3);

    TEST("group name is 'Bateria'", group.groupName == "Bateria");
    TEST("group icon is non-empty", group.icon.isNotEmpty());
    TEST("group has 3 tracks", (int)group.tracks.size() == 3);

    // Verificar track data
    TEST("first track is Kick", group.tracks[0].roleName == "Kick");
    TEST("first track has high severity", group.tracks[0].severity >= 0.8f);
    TEST("first track has plugin suggestions",
         (int)group.tracks[0].pluginSuggestions.size() == 3);

    TEST("second track is Snare", group.tracks[1].roleName == "Snare");
    TEST("second track has medium severity",
         group.tracks[1].severity >= 0.4f && group.tracks[1].severity < 0.8f);

    TEST("third track is HiHat", group.tracks[2].roleName == "HiHat");
    TEST("third track has low severity", group.tracks[2].severity < 0.4f);
}

static void test_track_problem_group_empty()
{
    std::printf("\n── TrackProblemGroup: Empty group ──\n");

    auto group = makeTestGroup(0);
    TEST("empty group has 0 tracks", group.tracks.empty());
    TEST("empty group still has name", group.groupName.isNotEmpty());
}

static void test_track_problem_data_defaults()
{
    std::printf("\n── TrackProblemData: Default values ──\n");

    mixcoach::TrackProblemData track;

    TEST("default slotIndex is -1", track.slotIndex == -1);
    TEST("default severity is 0.5f", track.severity == 0.5f);
    TEST("default priorityScore is 0.0f", track.priorityScore == 0.0f);
    TEST("default trackName is empty", track.trackName.isEmpty());
    TEST("default roleName is empty", track.roleName.isEmpty());
    TEST("default problemType is empty", track.problemType.isEmpty());
    TEST("default maskingInfo is empty", track.maskingInfo.isEmpty());
    TEST("default pluginSuggestions is empty",
         track.pluginSuggestions.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: ChatBubble track group fields
// ═══════════════════════════════════════════════════════════════════════════

static void test_chat_bubble_track_group_fields()
{
    std::printf("\n── ChatBubble: Track group fields ──\n");

    auto group = makeTestGroup(2);

    mixcoach::ChatBubble bubble;
    bubble.isTrackGroupCard = true;
    bubble.isUser = false;
    bubble.isSystem = false;
    bubble.trackGroup = group;
    bubble.trackProblems = group.tracks;

    TEST("isTrackGroupCard set to true", bubble.isTrackGroupCard == true);
    TEST("trackGroup has tracks", (int)bubble.trackGroup.tracks.size() == 2);
    TEST("trackProblems has tracks",
         (int)bubble.trackProblems.size() == 2);
    TEST("trackGroup name matches", bubble.trackGroup.groupName == "Bateria");
    TEST("bubble is not user", bubble.isUser == false);
    TEST("bubble is not system", bubble.isSystem == false);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: PluginSuggestionsProvider::domainToProblemType
// ═══════════════════════════════════════════════════════════════════════════

static void test_domain_to_problem_type()
{
    std::printf("\n── PluginSuggestionsProvider::domainToProblemType ──\n");

    using juce::String;
    using PT = mixcoach::ProblemType;

    // ─── Gain domain ────────────────────────────────────────────────────
    TEST("gain+CLIPPING → Clipping",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("gain", "CLIPPING")
         == PT::Clipping);
    TEST("gain+HIGH → Gain",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("gain", "HIGH")
         == PT::Gain);
    TEST("level+CLIP → Clipping",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("level", "CLIP")
         == PT::Clipping);
    TEST("gain+empty → Gain (default)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("gain", "")
         == PT::Gain);

    // ─── Tonal domain ───────────────────────────────────────────────────
    TEST("tonal+EXCESS → TonalExcess",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("tonal", "EXCESS")
         == PT::TonalExcess);
    TEST("tonal+DEFICIT → TonalDeficit",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("tonal", "DEFICIT")
         == PT::TonalDeficit);
    TEST("spectral+EXCESS → TonalExcess",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("spectral", "EXCESS")
         == PT::TonalExcess);
    TEST("eq+EXCESS → TonalExcess",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("eq", "EXCESS")
         == PT::TonalExcess);
    TEST("tonal+LOW → TonalExcess (default tonal)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("tonal", "LOW")
         == PT::TonalExcess);

    // ─── Dynamics domain ────────────────────────────────────────────────
    TEST("dynamics+OVERCOMPRESS → DynamicsOvercompressed",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("dynamics", "OVERCOMPRESS")
         == PT::DynamicsOvercompressed);
    TEST("dynamics+LOW_CREST → DynamicsOvercompressed",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("dynamics", "LOW_CREST")
         == PT::DynamicsOvercompressed);
    TEST("dynamics+HIGH_CREST → DynamicsTooDynamic",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("dynamics", "HIGH_CREST")
         == PT::DynamicsTooDynamic);
    TEST("dynamics+TOO_DYNAMIC → DynamicsTooDynamic",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("dynamics", "TOO_DYNAMIC")
         == PT::DynamicsTooDynamic);
    TEST("dynamic+OVERCOMPRESS → DynamicsOvercompressed",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("dynamic", "OVERCOMPRESS")
         == PT::DynamicsOvercompressed);
    TEST("dynamics+OTHER → DynamicsTooDynamic (default dynamics)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("dynamics", "OTHER")
         == PT::DynamicsTooDynamic);

    // ─── Phase / Spatial domain ─────────────────────────────────────────
    TEST("phase+PHASE → Phase",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("phase", "PHASE")
         == PT::Phase);
    TEST("phase+CORRELATION → Phase",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("phase", "CORRELATION")
         == PT::Phase);
    TEST("spatial+WIDTH → Spatial",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("spatial", "WIDTH")
         == PT::Spatial);
    TEST("spatial+STEREO → Spatial",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("spatial", "STEREO")
         == PT::Spatial);
    TEST("stereo+PHASE → Phase",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("stereo", "PHASE")
         == PT::Phase);
    TEST("phase+OTHER → Phase (default phase)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("phase", "OTHER")
         == PT::Phase);

    // ─── Other domains ──────────────────────────────────────────────────
    TEST("masking+empty → Masking",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("masking", "")
         == PT::Masking);
    TEST("limiting+empty → Limiting",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("limiting", "")
         == PT::Limiting);
    TEST("master+empty → Limiting",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("master", "")
         == PT::Limiting);

    // ─── Unknown domain ─────────────────────────────────────────────────
    TEST("unknown+empty → Unknown",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("unknown", "")
         == PT::Unknown);
    TEST("empty+empty → Unknown",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("", "")
         == PT::Unknown);

    // ─── Case insensitivity ─────────────────────────────────────────────
    TEST("GAIN+clipping → Clipping (lowercase issueType)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("GAIN", "clipping")
         == PT::Clipping);
    TEST("Gain+Clipping → Clipping (mixed case)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("Gain", "Clipping")
         == PT::Clipping);
    TEST("  gain  +  CLIPPING  → Clipping (trimmed)",
         mixcoach::PluginSuggestionsProvider::domainToProblemType("  gain  ", "  CLIPPING  ")
         == PT::Clipping);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: PluginSuggestionsProvider::interpolateAction
// ═══════════════════════════════════════════════════════════════════════════

static void test_interpolate_action()
{
    std::printf("\n── PluginSuggestionsProvider::interpolateAction ──\n");

    using PSP = mixcoach::PluginSuggestionsProvider;

    // ─── Delta placeholder ───────────────────────────────────────────────
    TEST("{delta} replaced with -1.5",
         PSP::interpolateAction("reduce {delta} dB", -1.5f, 0.0f)
         == "reduce -1.5 dB");
    TEST("{delta} replaced with 3.0",
         PSP::interpolateAction("boost {delta} dB", 3.0f, 0.0f)
         == "boost 3.0 dB");
    TEST("{delta} with zero delta",
         PSP::interpolateAction("{delta} dB", 0.0f, 0.0f)
         == "0.0 dB");

    // ─── Frequency placeholders ──────────────────────────────────────────
    TEST("{frequencyHz} → Hz for low freq",
         PSP::interpolateAction("cut at {frequencyHz}", 0.0f, 58.0f)
         == "cut at 58 Hz");
    TEST("{frequencyHz} → kHz for high freq",
         PSP::interpolateAction("cut at {frequencyHz}", 0.0f, 2500.0f)
         == "cut at 2.5 kHz");
    TEST("{freq} alias → Hz",
         PSP::interpolateAction("cut at {freq}", 0.0f, 100.0f)
         == "cut at 100 Hz");
    TEST("{freq} alias → kHz",
         PSP::interpolateAction("cut at {freq}", 0.0f, 12000.0f)
         == "cut at 12.0 kHz");

    // ─── Channel placeholder ─────────────────────────────────────────────
    TEST("{channel} replaced",
         PSP::interpolateAction("adjust {channel} channel", 0.0f, 0.0f)
         == "adjust izquierdo channel");

    // ─── Multiple placeholders ───────────────────────────────────────────
    TEST("multiple placeholders",
         PSP::interpolateAction("{delta} dB at {frequencyHz}", -1.5f, 100.0f)
         == "-1.5 dB at 100 Hz");
    TEST("all placeholders",
         PSP::interpolateAction("{delta}|{frequencyHz}|{channel}", 2.0f, 1000.0f)
         == "2.0|1.0 kHz|izquierdo");

    // ─── Edge cases ─────────────────────────────────────────────────────
    TEST("empty action text returns empty", PSP::interpolateAction("", 0.0f, 0.0f).isEmpty());
    TEST("no placeholders returns original",
         PSP::interpolateAction("reduce gain", -1.5f, 0.0f) == "reduce gain");
    TEST("only text with no braces unchanged",
         PSP::interpolateAction("hello world", 0.0f, 0.0f) == "hello world");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getSuggestionsForProblem — with PluginDatabase loaded
// ═══════════════════════════════════════════════════════════════════════════
// Inicializa PluginSuggestionsProvider con plugin_db.json real.
// Verifica que getSuggestionsForProblem retorna sugerencias reales.

static void test_get_suggestions_for_problem_with_db()
{
    std::printf("\n── getSuggestionsForProblem: With database loaded ──\n");

    // Ruta a plugin_db.json relativa al ejecutable del test
    // (desde build/tests/Release/ -> ../../../plugins/plugin_db.json)
    juce::File dbFile = juce::File::getCurrentWorkingDirectory()
                            .getChildFile("../../../plugins/plugin_db.json");
    if (!dbFile.existsAsFile()) {
        std::printf("  ⚠ SKIP: plugin_db.json not found at %s\n",
                     dbFile.getFullPathName().toRawUTF8());
        return;
    }

    mixcoach::PluginSuggestionsProvider provider;
    provider.setDatabasePath(dbFile.getFullPathName());
    provider.initialize();
    TEST("provider is ready after initialize", provider.isReady());

    // ─── getSuggestionsForProblem con Gain ───────────────────────────────
    {
        auto sugs = provider.getSuggestionsForProblem(mixcoach::ProblemType::Gain);
        TEST("Gain problem returns suggestions", !sugs.empty());
        if (!sugs.empty()) {
            TEST("first Gain suggestion has plugin", sugs[0].plugin != nullptr);
            TEST("first Gain suggestion has config", sugs[0].config != nullptr);
            if (sugs[0].plugin != nullptr)
                TEST("first Gain suggestion name is not empty",
                     sugs[0].plugin->name.isNotEmpty());
        }
    }

    // ─── getSuggestionsForProblem con Clipping ───────────────────────────
    {
        auto sugs = provider.getSuggestionsForProblem(mixcoach::ProblemType::Clipping);
        TEST("Clipping problem returns suggestions", !sugs.empty());
        if (!sugs.empty()) {
            TEST("Clipping suggestion has delta set", sugs[0].delta == 0.0f);
            // Verificar que el plugin existe en la DB
            bool foundBalance = false;
            for (auto& s : sugs) {
                if (s.plugin && s.plugin->name == "Fruity Balance")
                    foundBalance = true;
            }
            TEST("Fruity Balance is in Clipping suggestions", foundBalance);
        }
    }

    // ─── getSuggestionsForProblem con delta/frequencyHz ──────────────────
    {
        auto sugs = provider.getSuggestionsForProblem(
            mixcoach::ProblemType::TonalExcess, -2.5f, 2500.0f);
        TEST("TonalExcess returns suggestions", !sugs.empty());
        if (!sugs.empty()) {
            TEST("delta passed through to suggestion", sugs[0].delta == -2.5f);
            TEST("frequencyHz passed through to suggestion", sugs[0].frequencyHz == 2500.0f);
        }
    }

    // ─── getSuggestionsForProblem con todos los ProblemType válidos ──────
    using PT = mixcoach::ProblemType;
    PT allTypes[] = {
        PT::Gain, PT::Clipping, PT::Masking,
        PT::TonalExcess, PT::TonalDeficit,
        PT::DynamicsOvercompressed, PT::DynamicsTooDynamic,
        PT::Phase, PT::Spatial, PT::Reverb,
        PT::Limiting
    };
    for (auto t : allTypes) {
        auto sugs = provider.getSuggestionsForProblem(t);
        TEST("problem type returns 1 suggestion per tier",
             !sugs.empty() && (int)sugs.size() <= 3);
    }

    // ─── getSuggestionsForProblem con Unknown → vacío ────────────────────
    {
        auto sugs = provider.getSuggestionsForProblem(PT::Unknown);
        TEST("Unknown problem returns empty", sugs.empty());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: populatePluginSuggestions — full integration
// ═══════════════════════════════════════════════════════════════════════════
// Inicializa PluginSuggestionsProvider con DB real, crea TrackProblemData,
// y llama populatePluginSuggestions con varios dominios.

static void test_populate_plugin_suggestions_with_db()
{
    std::printf("\n── populatePluginSuggestions: Full integration ──\n");

    juce::File dbFile = juce::File::getCurrentWorkingDirectory()
                            .getChildFile("../../../plugins/plugin_db.json");
    if (!dbFile.existsAsFile()) {
        std::printf("  ⚠ SKIP: plugin_db.json not found at %s\n",
                     dbFile.getFullPathName().toRawUTF8());
        return;
    }

    mixcoach::PluginSuggestionsProvider provider;
    provider.setDatabasePath(dbFile.getFullPathName());
    provider.initialize();
    if (!provider.isReady()) {
        std::printf("  ⚠ SKIP: Provider not ready after initialization\n");
        return;
    }

    // ─── Gain domain + CLIPPING issueType ────────────────────────────────
    {
        mixcoach::TrackProblemData track;
        track.slotIndex   = 0;
        track.trackName   = "Kick";
        track.roleName    = "Kick";
        track.severity    = 0.9f;
        track.problemType = "Picos de clip";

        TEST("track starts with empty pluginSuggestions",
             track.pluginSuggestions.empty());

        // populatePluginSuggestions(provider, track, domain, issueType, delta, freq)
        populatePluginSuggestions(provider, track, "gain", "CLIPPING", -1.5f, 0.0f);

        TEST("gain+CLIPPING populates suggestions",
             !track.pluginSuggestions.empty());

        if (!track.pluginSuggestions.empty()) {
            // Should have at least one suggestion
            TEST("first suggestion has non-empty pluginName",
                 track.pluginSuggestions[0].pluginName.isNotEmpty());
            // First suggestion should be Native tier
            TEST("first suggestion is Native tier",
                 track.pluginSuggestions[0].tier == mixcoach::TrackPluginSuggestion::Tier::Native);
        }
    }

    // ─── Dynamics domain + HIGH_CREST → compressores ─────────────────────
    {
        mixcoach::TrackProblemData track;
        track.trackName   = "Bass";
        track.roleName    = "Bass";
        track.severity    = 0.6f;

        populatePluginSuggestions(provider, track, "dynamics", "HIGH_CREST", 2.0f, 0.0f);
        TEST("dynamics+HIGH_CREST populates suggestions",
             !track.pluginSuggestions.empty());
    }

    // ─── Tonal domain + EXCESS → EQ plugins ──────────────────────────────
    {
        mixcoach::TrackProblemData track;
        track.trackName   = "Vocal";
        track.roleName    = "Vocal";
        track.severity    = 0.7f;

        populatePluginSuggestions(provider, track, "tonal", "EXCESS", -3.0f, 2500.0f);
        TEST("tonal+EXCESS populates suggestions",
             !track.pluginSuggestions.empty());

        if (!track.pluginSuggestions.empty()) {
            // VERIFY: actionText includes frequency
            bool hasFreqInText = track.pluginSuggestions[0].actionText.contains("2.5 kHz")
                                 || track.pluginSuggestions[0].actionText.contains("2500");
            TEST("suggestion actionText contains frequency", hasFreqInText);
        }
    }

    // ─── Phase domain → stereo/phase plugins ─────────────────────────────
    {
        mixcoach::TrackProblemData track;
        track.trackName   = "Synth";
        track.roleName    = "Synth";
        track.severity    = 0.5f;

        populatePluginSuggestions(provider, track, "phase", "PHASE_ISSUE", 0.0f, 0.0f);
        TEST("phase+PHASE_ISSUE populates suggestions",
             !track.pluginSuggestions.empty());
    }

    // ─── Unknown domain → no suggestions added ──────────────────────────
    {
        mixcoach::TrackProblemData track;
        track.trackName   = "Test";
        track.roleName    = "Test";

        populatePluginSuggestions(provider, track, "nonexistent", "", 0.0f, 0.0f);
        TEST("nonexistent domain adds no suggestions",
             track.pluginSuggestions.empty());
    }

    // ─── Empty domain → no suggestions added ─────────────────────────────
    {
        mixcoach::TrackProblemData track;
        track.trackName   = "Test2";
        track.roleName    = "Test2";

        populatePluginSuggestions(provider, track, "", "", 0.0f, 0.0f);
        TEST("empty domain adds no suggestions",
             track.pluginSuggestions.empty());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: addTrackGroupCard
// ═══════════════════════════════════════════════════════════════════════════
// writeChatLog is now provided by ChatMessagesComponent_InlineCards.cpp
// (included as a source in the test target via CMakeLists.txt).
// No need for a mock here.

static void test_add_track_group_card_basic()
{
    std::printf("\n── addTrackGroupCard: Basic ──\n");

    mixcoach::ChatMessagesComponent chat;
    chat.setSize(350, 400);

    // Should be empty initially
    TEST("chat starts empty", chat.isEmpty());

    // Add a track group card
    auto group = makeTestGroup(3);
    chat.addTrackGroupCard(group);

    // Should not be empty after adding
    TEST("chat not empty after addTrackGroupCard", !chat.isEmpty());

    // Add another card — should still not be empty
    auto group2 = makeTestGroup(2);
    chat.addTrackGroupCard(group2);
    TEST("chat still not empty after second card", !chat.isEmpty());
}

static void test_add_track_group_card_multiple()
{
    std::printf("\n── addTrackGroupCard: Multiple cards ──\n");

    mixcoach::ChatMessagesComponent chat;
    chat.setSize(350, 400);

    auto group1 = makeTestGroup(2);
    auto group2 = makeTestGroup(4);

    chat.addTrackGroupCard(group1);
    TEST("chat not empty after first card", !chat.isEmpty());

    chat.addTrackGroupCard(group2);
    TEST("chat still not empty after second card", !chat.isEmpty());

    // Clear and verify empty again
    chat.clear();
    TEST("chat empty after clear", chat.isEmpty());
}

static void test_add_track_group_card_empty_group()
{
    std::printf("\n── addTrackGroupCard: Empty group ──\n");

    mixcoach::ChatMessagesComponent chat;
    chat.setSize(350, 400);

    auto emptyGroup = makeTestGroup(0);
    chat.addTrackGroupCard(emptyGroup);

    // Even empty groups should add a bubble
    TEST("chat not empty after empty group", !chat.isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("====================================================================\n");
    std::printf("  Chat Inline Cards Unit Tests\n");
    std::printf("  drawTrackGroupCard | TrackProblemGroup | PluginSuggestion | ChatBubble\n");
    std::printf("====================================================================\n\n");

    // ─── drawTrackGroupCard rendering tests ───────────────────────────────
    test_draw_track_group_card_basic();
    test_draw_track_group_card_empty_group();
    test_draw_track_group_card_single_track();
    test_draw_track_group_card_many_tracks();
    test_draw_track_group_card_plugin_suggestions();
    test_draw_track_group_card_various_widths();

    // ─── PluginSuggestion tests ──────────────────────────────────────────
    test_draw_track_group_card_tier_icons();
    test_plugin_suggestion_struct();
    test_plugin_suggestion_all_tiers();

    // ─── TrackProblemGroup / TrackProblemData tests ──────────────────────
    test_track_problem_group_construction();
    test_track_problem_group_empty();
    test_track_problem_data_defaults();

    // ─── ChatBubble track group fields tests ─────────────────────────────
    test_chat_bubble_track_group_fields();

    // ─── PluginSuggestionsProvider static tests ──────────────────────────
    test_domain_to_problem_type();
    test_interpolate_action();
    test_get_suggestions_for_problem_with_db();
    test_populate_plugin_suggestions_with_db();

    // ─── Results ─────────────────────────────────────────────────────────
    std::printf("\n====================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("====================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}

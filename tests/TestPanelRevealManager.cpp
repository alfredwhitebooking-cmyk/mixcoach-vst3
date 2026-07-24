// ═══════════════════════════════════════════════════════════════════════════
//  TestPanelRevealManager.cpp — Unit tests para PanelRevealManager
//  (keyword detection via processMessage, state tracking via isPanelRevealed,
//   callback on first reveal, track name matching)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestPanelRevealManager
//  Run:   build/tests/Release/TestPanelRevealManager.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <set>
#include "../Source/MixCoach/engine/PanelRevealManager.h"

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
//  1. Constructor — Coach siempre visible
// ═══════════════════════════════════════════════════════════════════════════

static void test_constructor() {
    std::printf("\n── Constructor ──\n");
    PanelRevealManager mgr;

    TEST("Coach panel is revealed by default",
         mgr.isPanelRevealed(PanelId::Coach));

    TEST("Reference panel is NOT revealed initially",
         !mgr.isPanelRevealed(PanelId::Reference));

    TEST("Messengers panel is NOT revealed initially",
         !mgr.isPanelRevealed(PanelId::Messengers));

    TEST("MixMap panel is NOT revealed initially",
         !mgr.isPanelRevealed(PanelId::MixMap));

    TEST("Tools panel is NOT revealed initially",
         !mgr.isPanelRevealed(PanelId::Tools));

    TEST("Session panel is NOT revealed initially",
         !mgr.isPanelRevealed(PanelId::Session));

    TEST("Report panel is NOT revealed initially",
         !mgr.isPanelRevealed(PanelId::Report));

    auto& revealed = mgr.getRevealedPanels();
    TEST("getRevealedPanels contains exactly 1 panel (Coach)",
         revealed.size() == 1);
    TEST("getRevealedPanels contains Coach",
         revealed.find(PanelId::Coach) != revealed.end());
}

// ═══════════════════════════════════════════════════════════════════════════
//  2. processMessage — Keyword detection → panelsToReveal
// ═══════════════════════════════════════════════════════════════════════════

static void test_keyword_reference_spanish() {
    std::printf("\n── processMessage: 'referencia' (ES) → Reference panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Carga tu referencia para empezar");
    TEST("panelsToReveal contains Reference",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Reference) != result.panelsToReveal.end());
    TEST("panelsToReveal has exactly 1 panel (Reference)",
         result.panelsToReveal.size() == 1);
}

static void test_keyword_reference_english() {
    std::printf("\n── processMessage: 'reference' (EN) → Reference panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Load your reference track");
    TEST("panelsToReveal contains Reference (english)",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Reference) != result.panelsToReveal.end());
}

static void test_keyword_messenger_spanish() {
    std::printf("\n── processMessage: 'pista' → Messengers panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Analicemos la pista de bajo");
    TEST("panelsToReveal contains Messengers",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Messengers) != result.panelsToReveal.end());
}

static void test_keyword_messenger_english() {
    std::printf("\n── processMessage: 'track' (EN) → Messengers panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Let's look at the kick track");
    TEST("panelsToReveal contains Messengers (english track)",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Messengers) != result.panelsToReveal.end());
}

static void test_keyword_mixmap() {
    std::printf("\n── processMessage: 'mapa' → MixMap panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Mira el mapa de ruteo");
    TEST("panelsToReveal contains MixMap",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::MixMap) != result.panelsToReveal.end());
}

static void test_keyword_tools() {
    std::printf("\n── processMessage: 'analizador' → Tools panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Abre el analizador de espectro");
    TEST("panelsToReveal contains Tools",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Tools) != result.panelsToReveal.end());
}

static void test_keyword_session() {
    std::printf("\n── processMessage: 'progreso' → Session panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Veamos tu progreso");
    TEST("panelsToReveal contains Session",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Session) != result.panelsToReveal.end());
}

static void test_keyword_report() {
    std::printf("\n── processMessage: 'reporte' → Report panel ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Genera el reporte final");
    TEST("panelsToReveal contains Report",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Report) != result.panelsToReveal.end());
}

static void test_keyword_no_match() {
    std::printf("\n── processMessage: sin keywords → panels vacío ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Hola, soy tu coach de mezcla");
    TEST("panelsToReveal is empty when no keywords match",
         result.panelsToReveal.empty());
    TEST("tracksToHighlight is empty when no track keywords match",
         result.tracksToHighlight.empty());
}

static void test_keyword_multiple_panels() {
    std::printf("\n── processMessage: múltiples keywords → múltiples paneles ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("Carga tu referencia y mira el analizador de espectro");
    TEST("panelsToReveal contains Reference",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Reference) != result.panelsToReveal.end());
    TEST("panelsToReveal contains Tools",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Tools) != result.panelsToReveal.end());
    TEST("panelsToReveal has exactly 2 panels",
         result.panelsToReveal.size() == 2);
}

static void test_keyword_duplicate_panels() {
    std::printf("\n── processMessage: keywords duplicados → un solo panel ──\n");
    PanelRevealManager mgr;

    // "referencia" and "carga tu" both map to Reference
    auto result = mgr.processMessage("Carga tu referencia ahora");
    TEST("panelsToReveal contains Reference (deduplicated)",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Reference) != result.panelsToReveal.end());
    TEST("panelsToReveal has exactly 1 panel despite 2 keyword matches",
         result.panelsToReveal.size() == 1);
}

// ═══════════════════════════════════════════════════════════════════════════
//  3. processMessage — Solo paneles NO revelados aparecen en result
// ═══════════════════════════════════════════════════════════════════════════

static void test_keyword_already_revealed() {
    std::printf("\n── processMessage: panel ya revelado → no aparece en result ──\n");
    PanelRevealManager mgr;

    // Mark Reference as already revealed
    mgr.markPanelRevealed(PanelId::Reference);

    // Now process a message that would trigger Reference
    auto result = mgr.processMessage("Carga tu referencia");
    TEST("panelsToReveal is empty when Reference already revealed",
         result.panelsToReveal.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  4. markPanelRevealed + isPanelRevealed
// ═══════════════════════════════════════════════════════════════════════════

static void test_mark_panel() {
    std::printf("\n── markPanelRevealed + isPanelRevealed ──\n");
    PanelRevealManager mgr;

    // Initially not revealed
    TEST("Reference is NOT revealed before markPanelRevealed",
         !mgr.isPanelRevealed(PanelId::Reference));

    mgr.markPanelRevealed(PanelId::Reference);

    // Now it should be revealed
    TEST("Reference IS revealed after markPanelRevealed",
         mgr.isPanelRevealed(PanelId::Reference));

    // Coach should still be revealed
    TEST("Coach is still revealed after marking Reference",
         mgr.isPanelRevealed(PanelId::Coach));

    // Messengers still not revealed
    TEST("Messengers is still NOT revealed",
         !mgr.isPanelRevealed(PanelId::Messengers));
}

static void test_mark_multiple_panels() {
    std::printf("\n── markPanelRevealed: múltiples paneles ──\n");
    PanelRevealManager mgr;

    mgr.markPanelRevealed(PanelId::Reference);
    mgr.markPanelRevealed(PanelId::Messengers);
    mgr.markPanelRevealed(PanelId::MixMap);

    TEST("Reference is revealed",
         mgr.isPanelRevealed(PanelId::Reference));
    TEST("Messengers is revealed",
         mgr.isPanelRevealed(PanelId::Messengers));
    TEST("MixMap is revealed",
         mgr.isPanelRevealed(PanelId::MixMap));
    TEST("getRevealedPanels size = 4 (Coach + 3)",
         mgr.getRevealedPanels().size() == 4);

    auto& revealed = mgr.getRevealedPanels();
    TEST("set contains Coach",
         revealed.find(PanelId::Coach) != revealed.end());
    TEST("set contains Reference",
         revealed.find(PanelId::Reference) != revealed.end());
    TEST("set contains Messengers",
         revealed.find(PanelId::Messengers) != revealed.end());
    TEST("set contains MixMap",
         revealed.find(PanelId::MixMap) != revealed.end());
}

// ═══════════════════════════════════════════════════════════════════════════
//  5. onPanelRevealed callback
// ═══════════════════════════════════════════════════════════════════════════

static void test_callback_fires_on_first_reveal() {
    std::printf("\n── onPanelRevealed: se dispara en el primer markPanelRevealed ──\n");
    PanelRevealManager mgr;

    int callCount = 0;
    PanelId lastPanel = PanelId::Count;
    bool lastIsFirst = false;

    mgr.onPanelRevealed = [&](PanelId panel, bool isFirst) {
        callCount++;
        lastPanel = panel;
        lastIsFirst = isFirst;
    };

    mgr.markPanelRevealed(PanelId::Messengers);

    TEST("callback was called exactly once", callCount == 1);
    TEST("callback received PanelId::Messengers",
         lastPanel == PanelId::Messengers);
    TEST("callback received isFirstReveal = true", lastIsFirst);
}

static void test_callback_not_fired_on_second_reveal() {
    std::printf("\n── onPanelRevealed: NO se dispara la segunda vez ──\n");
    PanelRevealManager mgr;

    int callCount = 0;
    mgr.onPanelRevealed = [&](PanelId, bool) { callCount++; };

    mgr.markPanelRevealed(PanelId::Tools);  // First → fires
    mgr.markPanelRevealed(PanelId::Tools);  // Second → should NOT fire

    TEST("callback fired exactly once (not on second call)",
         callCount == 1);
}

static void test_callback_no_callback_set() {
    std::printf("\n── onPanelRevealed: sin callback → no crash ──\n");
    PanelRevealManager mgr;

    // Should not crash when onPanelRevealed is not set
    mgr.markPanelRevealed(PanelId::Reference);
    TEST("markPanelRevealed without callback completes without crash", true);
    TEST("Reference is revealed even without callback",
         mgr.isPanelRevealed(PanelId::Reference));
}

static void test_callback_multiple_panels() {
    std::printf("\n── onPanelRevealed: múltiples paneles → llamado N veces ──\n");
    PanelRevealManager mgr;

    int callCount = 0;
    std::set<PanelId> revealedPanels;
    mgr.onPanelRevealed = [&](PanelId panel, bool) {
        callCount++;
        revealedPanels.insert(panel);
    };

    mgr.markPanelRevealed(PanelId::Reference);
    mgr.markPanelRevealed(PanelId::Messengers);
    mgr.markPanelRevealed(PanelId::MixMap);

    TEST("callback called 3 times for 3 panels", callCount == 3);
    TEST("callback received Reference",
         revealedPanels.find(PanelId::Reference) != revealedPanels.end());
    TEST("callback received Messengers",
         revealedPanels.find(PanelId::Messengers) != revealedPanels.end());
    TEST("callback received MixMap",
         revealedPanels.find(PanelId::MixMap) != revealedPanels.end());
}

// ═══════════════════════════════════════════════════════════════════════════
//  6. resetAllPanels
// ═══════════════════════════════════════════════════════════════════════════

static void test_reset_all_panels() {
    std::printf("\n── resetAllPanels ──\n");
    PanelRevealManager mgr;

    // Reveal several panels
    mgr.markPanelRevealed(PanelId::Reference);
    mgr.markPanelRevealed(PanelId::Messengers);
    mgr.markPanelRevealed(PanelId::Tools);

    TEST("3 panels + Coach are revealed before reset",
         mgr.getRevealedPanels().size() == 4);

    mgr.resetAllPanels();

    TEST("After reset, only Coach is revealed",
         mgr.getRevealedPanels().size() == 1);
    TEST("Coach is still revealed after reset",
         mgr.isPanelRevealed(PanelId::Coach));
    TEST("Reference is NOT revealed after reset",
         !mgr.isPanelRevealed(PanelId::Reference));
    TEST("Messengers is NOT revealed after reset",
         !mgr.isPanelRevealed(PanelId::Messengers));
    TEST("Tools is NOT revealed after reset",
         !mgr.isPanelRevealed(PanelId::Tools));
}

// ═══════════════════════════════════════════════════════════════════════════
//  7. findTrackMatches — track keyword detection
// ═══════════════════════════════════════════════════════════════════════════

static void test_track_keyword_kick() {
    std::printf("\n── track keyword: 'kick' → domain 0 (gain) ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("The kick is too loud");
    TEST("tracksToHighlight has at least 1 match",
         !result.tracksToHighlight.empty());

    bool foundKick = false;
    for (const auto& track : result.tracksToHighlight) {
        if (track.keyword == "kick") {
            foundKick = true;
            TEST("kick domain is 0 (gain)", track.domain == 0);
            break;
        }
    }
    TEST("kick was found in tracksToHighlight", foundKick);
}

static void test_track_keyword_vocal() {
    std::printf("\n── track keyword: 'vocal' → domain 1 (tonal) ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("La vocal necesita presencia");
    TEST("tracksToHighlight has at least 1 match",
         !result.tracksToHighlight.empty());

    bool foundVocal = false;
    for (const auto& track : result.tracksToHighlight) {
        if (track.keyword == "vocal") {
            foundVocal = true;
            TEST("vocal domain is 1 (tonal)", track.domain == 1);
            break;
        }
    }
    TEST("vocal was found in tracksToHighlight", foundVocal);
}

static void test_track_keyword_reverb() {
    std::printf("\n── track keyword: 'reverb' → domain 3 (spatial) ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("The reverb is too long");
    TEST("tracksToHighlight has at least 1 match",
         !result.tracksToHighlight.empty());

    bool foundReverb = false;
    for (const auto& track : result.tracksToHighlight) {
        if (track.keyword == "reverb") {
            foundReverb = true;
            TEST("reverb domain is 3 (spatial)", track.domain == 3);
            break;
        }
    }
    TEST("reverb was found in tracksToHighlight", foundReverb);
}

static void test_track_keyword_multiple() {
    std::printf("\n── track keywords: múltiples tracks en un mensaje ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("The kick and snare need work, and the bass is muddy");
    TEST("tracksToHighlight has multiple matches",
         result.tracksToHighlight.size() >= 2);

    bool foundKick = false, foundSnare = false, foundBass = false;
    for (const auto& track : result.tracksToHighlight) {
        if (track.keyword == "kick")  foundKick = true;
        if (track.keyword == "snare") foundSnare = true;
        if (track.keyword == "bass")  foundBass = true;
    }
    TEST("kick was found", foundKick);
    TEST("snare was found", foundSnare);
    TEST("bass was found", foundBass);
}

// ═══════════════════════════════════════════════════════════════════════════
//  8. updateTrackNames — track name matching
// ═══════════════════════════════════════════════════════════════════════════

static void test_track_name_matching() {
    std::printf("\n── updateTrackNames: match por nombre de track ──\n");
    PanelRevealManager mgr;

    mgr.updateTrackNames({ "Kick", "Snare", "Hi Hat", "Bass" });

    // Mention a track by name → should appear in tracksToHighlight
    auto result = mgr.processMessage("The Snare has too much ring");
    bool foundSnare = false;
    for (const auto& track : result.tracksToHighlight) {
        if (track.keyword == "snare") {
            foundSnare = true;
            break;
        }
    }
    TEST("snare found by track name match", foundSnare);

    // Should ALSO reveal Messengers panel if a track name is mentioned
    bool messengersFound = std::find(result.panelsToReveal.begin(),
                                     result.panelsToReveal.end(),
                                     PanelId::Messengers) != result.panelsToReveal.end();
    TEST("track name match also triggers Messengers panel reveal",
         messengersFound);
}

static void test_track_name_no_track_names_set() {
    std::printf("\n── updateTrackNames: sin track names → no match extra ──\n");
    PanelRevealManager mgr;

    // Without calling updateTrackNames, only keyword-based matching works
    auto result = mgr.processMessage("The Snare needs work");
    bool foundSnare = false;
    for (const auto& track : result.tracksToHighlight) {
        // "snare" is a keyword match even without track names
        if (track.keyword == "snare") foundSnare = true;
    }
    // "snare" should match as a keyword (kTrackKeywordRules includes it)
    TEST("snare found as built-in keyword", foundSnare);
}

static void test_track_name_does_not_trigger_panel_without_names() {
    std::printf("\n── updateTrackNames: sin track names, Messengers no se revela por nombre ──\n");
    PanelRevealManager mgr;

    // Without track names, mentioning a non-keyword track name shouldn't trigger Messengers
    auto result = mgr.processMessage("The customTrackName needs work");
    TEST("panelsToReveal is empty (no known keywords match)", result.panelsToReveal.empty());
    TEST("tracksToHighlight is empty (no keywords match)", result.tracksToHighlight.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  9. Edge cases
// ═══════════════════════════════════════════════════════════════════════════

static void test_empty_message() {
    std::printf("\n── processMessage: string vacío ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("");
    TEST("panelsToReveal empty for empty string",
         result.panelsToReveal.empty());
    TEST("tracksToHighlight empty for empty string",
         result.tracksToHighlight.empty());
}

static void test_whitespace_message() {
    std::printf("\n── processMessage: solo espacios ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("   ");
    TEST("panelsToReveal empty for whitespace",
         result.panelsToReveal.empty());
}

static void test_case_insensitivity() {
    std::printf("\n── processMessage: case insensitive ──\n");
    PanelRevealManager mgr;

    auto result = mgr.processMessage("CARGA TU REFERENCIA AQUI");
    TEST("Reference detected in UPPERCASE",
         std::find(result.panelsToReveal.begin(), result.panelsToReveal.end(),
                   PanelId::Reference) != result.panelsToReveal.end());
}

static void test_keyword_whole_word_only() {
    std::printf("\n── processMessage: containsWholeWord — no match dentro de otra palabra ──\n");
    PanelRevealManager mgr;

    // "analizador" in "analizadores" — containsWholeWord should NOT match
    // because "analizador" is followed by "es" (alphanumeric)
    auto result = mgr.processMessage("Los analizadores son útiles");
    TEST("analizador does NOT match within 'analizadores' (whole word only)",
         result.panelsToReveal.empty());

    // But "analizador" as a standalone word should match
    auto result2 = mgr.processMessage("Abre el analizador ahora");
    TEST("analizador as standalone word DOES match",
         std::find(result2.panelsToReveal.begin(), result2.panelsToReveal.end(),
                   PanelId::Tools) != result2.panelsToReveal.end());
}

static void test_count_sentinel() {
    std::printf("\n── PanelId::Count como sentinel — markPanelRevealed inserta si no existe ──\n");
    PanelRevealManager mgr;

    // Count is a valid uint8_t enum value; markPanelRevealed inserts it like any other
    mgr.markPanelRevealed(PanelId::Count);
    TEST("markPanelRevealed with Count does not crash", true);
    TEST("Count IS in revealed panels after markPanelRevealed",
         mgr.isPanelRevealed(PanelId::Count));

    // Calling again should NOT crash (already revealed)
    mgr.markPanelRevealed(PanelId::Count);
    TEST("markPanelRevealed with Count twice does not crash", true);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  PanelRevealManager Unit Tests\n");
    std::printf("  Constructor | processMessage Keywords | markPanelRevealed | Callback | resetAll | Track Matching\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    // ─── Constructor ───────────────────────────────────────────────────
    test_constructor();

    // ─── processMessage keywords ───────────────────────────────────────
    test_keyword_reference_spanish();
    test_keyword_reference_english();
    test_keyword_messenger_spanish();
    test_keyword_messenger_english();
    test_keyword_mixmap();
    test_keyword_tools();
    test_keyword_session();
    test_keyword_report();
    test_keyword_no_match();
    test_keyword_multiple_panels();
    test_keyword_duplicate_panels();

    // ─── Already revealed ──────────────────────────────────────────────
    test_keyword_already_revealed();

    // ─── markPanelRevealed + isPanelRevealed ───────────────────────────
    test_mark_panel();
    test_mark_multiple_panels();

    // ─── onPanelRevealed callback ──────────────────────────────────────
    test_callback_fires_on_first_reveal();
    test_callback_not_fired_on_second_reveal();
    test_callback_no_callback_set();
    test_callback_multiple_panels();

    // ─── resetAllPanels ─────────────────────────────────────────────────
    test_reset_all_panels();

    // ─── Track keyword matching ────────────────────────────────────────
    test_track_keyword_kick();
    test_track_keyword_vocal();
    test_track_keyword_reverb();
    test_track_keyword_multiple();

    // ─── Track name matching ───────────────────────────────────────────
    test_track_name_matching();
    test_track_name_no_track_names_set();
    test_track_name_does_not_trigger_panel_without_names();

    // ─── Edge cases ─────────────────────────────────────────────────────
    test_empty_message();
    test_whitespace_message();
    test_case_insensitivity();
    test_keyword_whole_word_only();
    test_count_sentinel();

    // ─── Results ───────────────────────────────────────────────────────
    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    return gTestsFailed > 0 ? 1 : 0;
}

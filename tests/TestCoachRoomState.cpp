// ═══════════════════════════════════════════════════════════════════════════
//  TestCoachRoomState.cpp — Unit tests para CoachRoomState
//  (isPreFullUI, isCoachingState, coachRoomStateProgress, coachRoomStateLabel,
//   enum ordering, progressive disclosure transition validation)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestCoachRoomState
//  Run:   build/tests/Release/TestCoachRoomState.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include "../Source/MixCoach/UI/CoachRoomState.h"

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
//  1. isPreFullUI — Estados pre-FullUI (welcome/setup, solo chat)
// ═══════════════════════════════════════════════════════════════════════════

static void test_is_pre_full_ui() {
    std::printf("\n── isPreFullUI ──\n");

    // Pre-FullUI states (Welcome → MixMapStage)
    TEST("Welcome is pre-FullUI",    isPreFullUI(CoachRoomState::Welcome));
    TEST("Intention is pre-FullUI",  isPreFullUI(CoachRoomState::Intention));
    TEST("Genre is pre-FullUI",      isPreFullUI(CoachRoomState::Genre));
    TEST("ReferenceStage is pre-FullUI", isPreFullUI(CoachRoomState::ReferenceStage));
    TEST("MessengerStage is pre-FullUI", isPreFullUI(CoachRoomState::MessengerStage));
    TEST("MixMapStage is pre-FullUI",    isPreFullUI(CoachRoomState::MixMapStage));

    // NOT pre-FullUI (coaching states)
    TEST("GainStaging is NOT pre-FullUI",   !isPreFullUI(CoachRoomState::GainStaging));
    TEST("Balance is NOT pre-FullUI",       !isPreFullUI(CoachRoomState::Balance));
    TEST("EQ is NOT pre-FullUI",            !isPreFullUI(CoachRoomState::EQ));
    TEST("Compression is NOT pre-FullUI",   !isPreFullUI(CoachRoomState::Compression));
    TEST("Space is NOT pre-FullUI",         !isPreFullUI(CoachRoomState::Space));
    TEST("Refinement is NOT pre-FullUI",    !isPreFullUI(CoachRoomState::Refinement));
    TEST("MasterCheck is NOT pre-FullUI",   !isPreFullUI(CoachRoomState::MasterCheck));

    // Report is NOT pre-FullUI (it's an overlay)
    TEST("Report is NOT pre-FullUI",        !isPreFullUI(CoachRoomState::Report));
}

// ═══════════════════════════════════════════════════════════════════════════
//  2. isCoachingState — Estados de coaching (FullUI con TabBar)
// ═══════════════════════════════════════════════════════════════════════════

static void test_is_coaching_state() {
    std::printf("\n── isCoachingState ──\n");

    // NOT coaching (pre-FullUI)
    TEST("Welcome is NOT coaching",         !isCoachingState(CoachRoomState::Welcome));
    TEST("Intention is NOT coaching",       !isCoachingState(CoachRoomState::Intention));
    TEST("Genre is NOT coaching",           !isCoachingState(CoachRoomState::Genre));
    TEST("ReferenceStage is NOT coaching",  !isCoachingState(CoachRoomState::ReferenceStage));
    TEST("MessengerStage is NOT coaching",  !isCoachingState(CoachRoomState::MessengerStage));
    TEST("MixMapStage is NOT coaching",     !isCoachingState(CoachRoomState::MixMapStage));

    // Coaching states (GainStaging → MasterCheck)
    TEST("GainStaging IS coaching",         isCoachingState(CoachRoomState::GainStaging));
    TEST("Balance IS coaching",             isCoachingState(CoachRoomState::Balance));
    TEST("EQ IS coaching",                  isCoachingState(CoachRoomState::EQ));
    TEST("Compression IS coaching",         isCoachingState(CoachRoomState::Compression));
    TEST("Space IS coaching",               isCoachingState(CoachRoomState::Space));
    TEST("Refinement IS coaching",          isCoachingState(CoachRoomState::Refinement));
    TEST("MasterCheck IS coaching",         isCoachingState(CoachRoomState::MasterCheck));

    // Report is NOT coaching (overlay)
    TEST("Report is NOT coaching",          !isCoachingState(CoachRoomState::Report));
}

// ═══════════════════════════════════════════════════════════════════════════
//  3. coachRoomStateProgress — Progreso lineal 0.0-1.0
// ═══════════════════════════════════════════════════════════════════════════

static void test_coach_room_state_progress() {
    std::printf("\n── coachRoomStateProgress ──\n");

    // Welcome should be 0/13 ≈ 0.0
    float progressWelcome = coachRoomStateProgress(CoachRoomState::Welcome);
    TEST("Welcome progress is 0.0 (or very close)",
         progressWelcome >= 0.0f && progressWelcome < 0.01f);

    // Report should be 13/13 = 1.0
    float progressReport = coachRoomStateProgress(CoachRoomState::Report);
    TEST("Report progress is 1.0 (or very close)",
         progressReport >= 0.99f && progressReport <= 1.0f);

    // Each consecutive state should have increasing progress
    CoachRoomState states[] = {
        CoachRoomState::Welcome,
        CoachRoomState::Intention,
        CoachRoomState::Genre,
        CoachRoomState::ReferenceStage,
        CoachRoomState::MessengerStage,
        CoachRoomState::MixMapStage,
        CoachRoomState::GainStaging,
        CoachRoomState::Balance,
        CoachRoomState::EQ,
        CoachRoomState::Compression,
        CoachRoomState::Space,
        CoachRoomState::Refinement,
        CoachRoomState::MasterCheck,
        CoachRoomState::Report
    };

    float lastProgress = -1.0f;
    bool allIncreasing = true;
    for (auto state : states) {
        float p = coachRoomStateProgress(state);
        if (p <= lastProgress) {
            allIncreasing = false;
            std::fprintf(stderr, "    Non-increasing at state %d: %f <= %f\n",
                         static_cast<int>(state), p, lastProgress);
        }
        lastProgress = p;
    }
    TEST("Progress increases monotonically across all 14 states", allIncreasing);

    // Count = all states have unique progress (14 unique values)
    float progressValues[14];
    for (int i = 0; i < 14; ++i) {
        progressValues[i] = coachRoomStateProgress(static_cast<CoachRoomState>(i));
    }
    bool allUnique = true;
    for (int i = 0; i < 14 && allUnique; ++i) {
        for (int j = i + 1; j < 14; ++j) {
            if (progressValues[i] == progressValues[j]) {
                allUnique = false;
                break;
            }
        }
    }
    TEST("All 14 states have unique progress values", allUnique);
}

// ═══════════════════════════════════════════════════════════════════════════
//  4. coachRoomStateLabel — Labels descriptivos para debug
// ═══════════════════════════════════════════════════════════════════════════

static void test_coach_room_state_label() {
    std::printf("\n── coachRoomStateLabel ──\n");

    TEST("Welcome label is not empty",
         coachRoomStateLabel(CoachRoomState::Welcome)[0] != '\0');
    TEST("Intention label is not empty",
         coachRoomStateLabel(CoachRoomState::Intention)[0] != '\0');
    TEST("Genre label is not empty",
         coachRoomStateLabel(CoachRoomState::Genre)[0] != '\0');
    TEST("ReferenceStage label is not empty",
         coachRoomStateLabel(CoachRoomState::ReferenceStage)[0] != '\0');
    TEST("MessengerStage label is not empty",
         coachRoomStateLabel(CoachRoomState::MessengerStage)[0] != '\0');
    TEST("MixMapStage label is not empty",
         coachRoomStateLabel(CoachRoomState::MixMapStage)[0] != '\0');
    TEST("GainStaging label is not empty",
         coachRoomStateLabel(CoachRoomState::GainStaging)[0] != '\0');
    TEST("Balance label is not empty",
         coachRoomStateLabel(CoachRoomState::Balance)[0] != '\0');
    TEST("EQ label is not empty",
         coachRoomStateLabel(CoachRoomState::EQ)[0] != '\0');
    TEST("Compression label is not empty",
         coachRoomStateLabel(CoachRoomState::Compression)[0] != '\0');
    TEST("Space label is not empty",
         coachRoomStateLabel(CoachRoomState::Space)[0] != '\0');
    TEST("Refinement label is not empty",          coachRoomStateLabel(CoachRoomState::Refinement)[0] != '\0');
    TEST("MasterCheck label is not empty",
         coachRoomStateLabel(CoachRoomState::MasterCheck)[0] != '\0');
    TEST("Report label is not empty",
         coachRoomStateLabel(CoachRoomState::Report)[0] != '\0');

    // Labels should be distinct
    const char* labels[14];
    for (int i = 0; i < 14; ++i) {
        labels[i] = coachRoomStateLabel(static_cast<CoachRoomState>(i));
    }
    bool allDistinct = true;
    for (int i = 0; i < 14 && allDistinct; ++i) {
        for (int j = i + 1; j < 14; ++j) {
            if (std::strcmp(labels[i], labels[j]) == 0) {
                allDistinct = false;
                std::fprintf(stderr, "    Duplicate label at indices %d and %d: '%s'\n",
                             i, j, labels[i]);
                break;
            }
        }
    }
    TEST("All 14 state labels are distinct", allDistinct);
}

// ═══════════════════════════════════════════════════════════════════════════
//  5. Enum ordering — Verificar progressive disclosure ordering
// ═══════════════════════════════════════════════════════════════════════════

static void test_enum_ordering() {
    std::printf("\n── Enum ordering (progressive disclosure sequence) ──\n");

    // Pre-FullUI comes before coaching
    TEST("Welcome < GainStaging",
         static_cast<int>(CoachRoomState::Welcome) <
         static_cast<int>(CoachRoomState::GainStaging));

    // Coaching comes before Report
    TEST("MasterCheck < Report",
         static_cast<int>(CoachRoomState::MasterCheck) <
         static_cast<int>(CoachRoomState::Report));

    // Pre-FullUI ordering: Welcome < Intention < Genre < Reference < Messenger < MixMap
    TEST("Welcome < Intention",
         static_cast<int>(CoachRoomState::Welcome) <
         static_cast<int>(CoachRoomState::Intention));
    TEST("Intention < Genre",
         static_cast<int>(CoachRoomState::Intention) <
         static_cast<int>(CoachRoomState::Genre));
    TEST("Genre < ReferenceStage",
         static_cast<int>(CoachRoomState::Genre) <
         static_cast<int>(CoachRoomState::ReferenceStage));
    TEST("ReferenceStage < MessengerStage",
         static_cast<int>(CoachRoomState::ReferenceStage) <
         static_cast<int>(CoachRoomState::MessengerStage));
    TEST("MessengerStage < MixMapStage",
         static_cast<int>(CoachRoomState::MessengerStage) <
         static_cast<int>(CoachRoomState::MixMapStage));

    // Coaching ordering: GainStaging < Balance < EQ < Compression < Space < Automation < MasterCheck
    TEST("GainStaging < Balance",
         static_cast<int>(CoachRoomState::GainStaging) <
         static_cast<int>(CoachRoomState::Balance));
    TEST("Balance < EQ",
         static_cast<int>(CoachRoomState::Balance) <
         static_cast<int>(CoachRoomState::EQ));
    TEST("EQ < Compression",
         static_cast<int>(CoachRoomState::EQ) <
         static_cast<int>(CoachRoomState::Compression));
    TEST("Compression < Space",
         static_cast<int>(CoachRoomState::Compression) <
         static_cast<int>(CoachRoomState::Space));
    TEST("Space < Refinement",
         static_cast<int>(CoachRoomState::Space) <
         static_cast<int>(CoachRoomState::Refinement));
    TEST("Refinement < MasterCheck",
         static_cast<int>(CoachRoomState::Refinement) <
         static_cast<int>(CoachRoomState::MasterCheck));
}

// ═══════════════════════════════════════════════════════════════════════════
//  6. Conversión — CoachRoomState ↔ int (count check)
// ═══════════════════════════════════════════════════════════════════════════

static void test_count_and_conversion() {
    std::printf("\n── Count y conversión ──\n");

    TEST("Count == 14", static_cast<int>(CoachRoomState::Count) == 14);

    // Valid states are 0..13
    TEST("Welcome int value is 0", static_cast<int>(CoachRoomState::Welcome) == 0);
    TEST("Intention int value is 1", static_cast<int>(CoachRoomState::Intention) == 1);
    TEST("Genre int value is 2", static_cast<int>(CoachRoomState::Genre) == 2);
    TEST("ReferenceStage int value is 3", static_cast<int>(CoachRoomState::ReferenceStage) == 3);
    TEST("MessengerStage int value is 4", static_cast<int>(CoachRoomState::MessengerStage) == 4);
    TEST("MixMapStage int value is 5", static_cast<int>(CoachRoomState::MixMapStage) == 5);
    TEST("GainStaging int value is 6", static_cast<int>(CoachRoomState::GainStaging) == 6);
    TEST("Balance int value is 7", static_cast<int>(CoachRoomState::Balance) == 7);
    TEST("EQ int value is 8", static_cast<int>(CoachRoomState::EQ) == 8);
    TEST("Compression int value is 9", static_cast<int>(CoachRoomState::Compression) == 9);
    TEST("Space int value is 10", static_cast<int>(CoachRoomState::Space) == 10);
    TEST("Refinement int value is 11", static_cast<int>(CoachRoomState::Refinement) == 11);
    TEST("MasterCheck int value is 12", static_cast<int>(CoachRoomState::MasterCheck) == 12);
    TEST("Report int value is 13", static_cast<int>(CoachRoomState::Report) == 13);

    // Invalid state should return UNKNOWN
    CoachRoomState invalid = static_cast<CoachRoomState>(99);
    TEST("Invalid state returns 'UNKNOWN' label",
         std::strcmp(coachRoomStateLabel(invalid), "UNKNOWN") == 0);

    // Invalid state progress
    TEST("Invalid state progress returns 0.0",
         coachRoomStateProgress(invalid) >= 0.0f && coachRoomStateProgress(invalid) < 0.01f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  CoachRoomState Unit Tests\n");
    std::printf("  isPreFullUI | isCoachingState | Progress | Labels | Enum Ordering | Count\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_is_pre_full_ui();
    test_is_coaching_state();
    test_coach_room_state_progress();
    test_coach_room_state_label();
    test_enum_ordering();
    test_count_and_conversion();

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

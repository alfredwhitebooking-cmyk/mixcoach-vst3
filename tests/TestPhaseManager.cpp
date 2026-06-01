// ═══════════════════════════════════════════════════════════════════════════
//  TestPhaseManager.cpp — Unit test para PhaseManager (fases + logros)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --target TestPhaseManager
//
//  Modo standalone (sin JUCE):
//    g++ -std=c++20 -DTEST_PHASEMANAGER_STANDALONE -I Source
//        tests/TestPhaseManager.cpp Source/MixCoach/engine/PhaseManager.cpp
//        Source/Common/memory/SlotRegistry.cpp Source/Common/memory/SharedMemory.cpp
//        -o test_phase
// ═══════════════════════════════════════════════════════════════════════════

// ─── Modo standalone: mock de tipos JUCE para probar lógica pura ──────────
#ifdef TEST_PHASEMANAGER_STANDALONE

#include <cmath>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <cstdint>

// ─── Mock mínimo de juce::Colour ──────────────────────────────────────────
namespace juce {

class Colour {
public:
    Colour() = default;
    Colour(uint32_t argb) : argb_(argb) {}
    static Colour fromARGB(uint8_t, uint8_t r, uint8_t g, uint8_t b) { return Colour(); }
    static Colour fromRGB(uint8_t r, uint8_t g, uint8_t b) { return Colour(); }
    uint8_t getRed() const { return 0; }
    uint8_t getGreen() const { return 0; }
    uint8_t getBlue() const { return 0; }
    uint32_t getARGB() const { return argb_; }
    juce::String toDisplayString(bool) const { return juce::String(); }
private:
    uint32_t argb_ = 0;
};

// ─── Mock mínimo de juce::String ──────────────────────────────────────────
class String {
public:
    String() = default;
    String(const char* s) : str_(s ? s : "") {}
    String(const std::string& s) : str_(s) {}
    bool isNotEmpty() const { return !str_.empty(); }
    const char* toRawUTF8() const { return str_.c_str(); }
    String operator+(const String& o) const { return String(str_ + o.str_); }
    std::string str_;
};

// ─── Mock mínimo de juce::File ────────────────────────────────────────────
class File {
public:
    static File getSpecialLocation(int) { return File(); }
    File getChildFile(const String&) const { return File(); }
    bool exists() const { return false; }
    bool existsAsFile() const { return false; }
    bool createDirectory() { return false; }
    bool deleteFile() { return false; }
    bool moveFileTo(const File&) { return false; }
    String getFullPathName() const { return String(); }
    int64_t getSize() const { return 0; }
};

// ─── Mock mínimo de juce::Time ────────────────────────────────────────────
class Time {
public:
    static uint32_t getMillisecondCounter() { return 0; }
    static double getMillisecondCounterHiRes() { return 0.0; }
};

// ─── Mock mínimo de juce::FileInputStream ─────────────────────────────────
class FileInputStream {
public:
    FileInputStream(const File&) {}
    bool openedOk() const { return false; }
    int readIntBigEndian() { return 0; }
    int read(void*, int) { return 0; }
    int64_t getNumBytesRemaining() { return 0; }
};

// ─── Mock mínimo de juce::FileOutputStream ────────────────────────────────
class FileOutputStream {
public:
    FileOutputStream(const File&) {}
    bool openedOk() const { return false; }
    bool setPosition(int64_t) { return false; }
    bool write(const void*, int) { return false; }
    bool writeByte(char) { return false; }
    bool writeIntBigEndian(int) { return false; }
    bool flush() { return false; }
};

// ─── Mock mínimo de juce::Array ───────────────────────────────────────────
template<typename T>
class Array {
public:
    void add(const T&) {}
    int size() const { return 0; }
    const T& operator[](int) const { static T t; return t; }
};

namespace FileHelpers {
    static constexpr int findFiles = 0;
}

namespace LogHelper {
    inline void writeToLog(const juce::String&) {}
}

} // namespace juce

// Necesario para compilar SlotRegistry.cpp en modo standalone
namespace juce {
    namespace Colours {
        const Colour grey(0xFF888888);
    }
}

#else
// ─── Modo normal: usar JUCE real ──────────────────────────────────────────
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#endif

// ─── Dependencias del proyecto ─────────────────────────────────────────────
#include "Common/types/Types.h"
#include "Common/memory/SlotRegistry.h"
#include "MixCoach/engine/PhaseManager.h"

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  ❌ FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        gTestsFailed++; \
    } else { \
        std::printf("  ✅ PASS: %s\n", name); \
        gTestsPassed++; \
    } \
} while(0)

// ============================================================================
//  Tests de Fases y Enums (modo standalone + JUCE)
// ============================================================================

static void test_enum_values() {
    std::printf("\n── Enum Values ──\n");

    TEST("Welcome = 0",
         static_cast<int>(mixcoach::MentorPhase::Welcome) == 0);
    TEST("GainStaging = 1",
         static_cast<int>(mixcoach::MentorPhase::GainStaging) == 1);
    TEST("Organisation = 2",
         static_cast<int>(mixcoach::MentorPhase::Organisation) == 2);
    TEST("TonalBalance = 3",
         static_cast<int>(mixcoach::MentorPhase::TonalBalance) == 3);
    TEST("Dynamics = 4",
         static_cast<int>(mixcoach::MentorPhase::Dynamics) == 4);
    TEST("Spatial = 5",
         static_cast<int>(mixcoach::MentorPhase::Spatial) == 5);

    TEST("FirstTrack = 0",
         static_cast<int>(mixcoach::Achievement::FirstTrack) == 0);
    TEST("FiveTracks = 1",
         static_cast<int>(mixcoach::Achievement::FiveTracks) == 1);
    TEST("TenTracks = 2",
         static_cast<int>(mixcoach::Achievement::TenTracks) == 2);
    TEST("FullMix = 3",
         static_cast<int>(mixcoach::Achievement::FullMix) == 3);
    TEST("PhaseMaster = 4",
         static_cast<int>(mixcoach::Achievement::PhaseMaster) == 4);
    TEST("DynamicControl = 5",
         static_cast<int>(mixcoach::Achievement::DynamicControl) == 5);
    TEST("GainGod = 6",
         static_cast<int>(mixcoach::Achievement::GainGod) == 6);
}

static void test_phase_descriptions() {
    std::printf("\n── Phase Descriptions ──\n");

    TEST("Welcome has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Welcome) != nullptr);
    TEST("GainStaging has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::GainStaging) != nullptr);
    TEST("Organisation has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Organisation) != nullptr);
    TEST("TonalBalance has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::TonalBalance) != nullptr);
    TEST("Dynamics has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Dynamics) != nullptr);
    TEST("Spatial has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Spatial) != nullptr);

    // All descriptions should be non-empty
    TEST("Welcome description non-empty",
         std::strlen(mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Welcome)) > 0);
    TEST("Spatial description non-empty",
         std::strlen(mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Spatial)) > 0);
}

// ============================================================================
//  Tests de minTracksForPhase (modo standalone + JUCE)
// ============================================================================

static void test_min_tracks() {
    std::printf("\n── minTracksForPhase ──\n");

    // Valores esperados
    TEST("Welcome → 0",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Welcome) == 0);
    TEST("GainStaging → 1",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::GainStaging) == 1);
    TEST("Organisation → 3",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Organisation) == 3);
    TEST("TonalBalance → 5",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::TonalBalance) == 5);
    TEST("Dynamics → 8",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Dynamics) == 8);
    TEST("Spatial → 10",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Spatial) == 10);

    // Edge cases: valores fuera de rango
    auto invalidLow  = static_cast<mixcoach::MentorPhase>(-1);
    auto invalidHigh = static_cast<mixcoach::MentorPhase>(99);
    TEST("Invalid low (-1) → 0",
         mixcoach::PhaseManager::minTracksForPhase(invalidLow) == 0);
    TEST("Invalid high (99) → 0",
         mixcoach::PhaseManager::minTracksForPhase(invalidHigh) == 0);

    // Monotonía estricta: cada fase requiere >= tracks que la anterior
    TEST("GainStaging >= Welcome",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::GainStaging) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Welcome));
    TEST("Organisation >= GainStaging",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Organisation) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::GainStaging));
    TEST("TonalBalance >= Organisation",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::TonalBalance) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Organisation));
    TEST("Dynamics >= TonalBalance",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Dynamics) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::TonalBalance));
    TEST("Spatial >= Dynamics",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Spatial) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Dynamics));

    // Valor máximo de tracks requerido
    int maxTracks = 0;
    for (int p = 0; p <= static_cast<int>(mixcoach::MentorPhase::Spatial); ++p) {
        int t = mixcoach::PhaseManager::minTracksForPhase(static_cast<mixcoach::MentorPhase>(p));
        if (t > maxTracks) maxTracks = t;
    }
    TEST("Spatial has max track requirement",
         maxTracks == mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Spatial));
    TEST("Max tracks <= SlotRegistry::kMaxSlots",
         maxTracks <= mixcoach::SlotRegistry::kMaxSlots);
}

// ============================================================================
//  Tests con instancia real de PhaseManager + SlotRegistry (solo JUCE)
// ============================================================================

#ifndef TEST_PHASEMANAGER_STANDALONE

// ─── NOTE: SlotRegistry es grande (~11MB por TelemetryBuffer).
// Siempre usar heap allocation (std::make_unique) para evitar
// stack overflow (Windows stack default = 1MB).

static void test_phase_transitions() {
    std::printf("\n── Phase Transitions ──\n");

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    mixcoach::PhaseManager pm(*registry);

    // Estado inicial
    TEST("Initial phase is Welcome",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Welcome);

    // Transiciones progresivas: Welcome → GainStaging → Organisation
    pm.advanceToNextPhase();
    TEST("After advance 1 → GainStaging",
         pm.getCurrentPhase() == mixcoach::MentorPhase::GainStaging);

    pm.advanceToNextPhase();
    TEST("After advance 2 → Organisation",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Organisation);

    pm.advanceToNextPhase();
    TEST("After advance 3 → TonalBalance",
         pm.getCurrentPhase() == mixcoach::MentorPhase::TonalBalance);

    pm.advanceToNextPhase();
    TEST("After advance 4 → Dynamics",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Dynamics);

    pm.advanceToNextPhase();
    TEST("After advance 5 → Spatial",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Spatial);

    // No debe avanzar más allá de Spatial
    pm.advanceToNextPhase();
    TEST("After advance 6 → still Spatial (no overflow)",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Spatial);

    pm.advanceToNextPhase();
    pm.advanceToNextPhase();
    TEST("After multiple advances → still Spatial",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Spatial);

    // setPhase() debe saltar a cualquier fase directamente
    pm.setPhase(mixcoach::MentorPhase::Welcome);
    TEST("setPhase(Welcome) works",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Welcome);

    pm.setPhase(mixcoach::MentorPhase::Dynamics);
    TEST("setPhase(Dynamics) works",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Dynamics);

    pm.setPhase(mixcoach::MentorPhase::Spatial);
    TEST("setPhase(Spatial) works",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Spatial);

    registry.reset();
}

static void test_achievement_unlocking() {
    std::printf("\n── Achievement Unlocking ──\n");

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    mixcoach::PhaseManager pm(*registry);

    // Estado inicial: sin logros
    TEST("Initial achievement count = 0",
         pm.getAchievementCount() == 0);

    // Desbloquear logros únicos
    TEST("Unlock FirstTrack → true (new)",
         pm.unlockAchievement(mixcoach::Achievement::FirstTrack) == true);
    TEST("Count = 1 after FirstTrack",
         pm.getAchievementCount() == 1);

    TEST("Unlock FiveTracks → true (new)",
         pm.unlockAchievement(mixcoach::Achievement::FiveTracks) == true);
    TEST("Count = 2 after FiveTracks",
         pm.getAchievementCount() == 2);

    TEST("Unlock TenTracks → true (new)",
         pm.unlockAchievement(mixcoach::Achievement::TenTracks) == true);
    TEST("Count = 3 after TenTracks",
         pm.getAchievementCount() == 3);

    // Desbloquear duplicado → false, count no cambia
    TEST("Unlock FirstTrack again → false (duplicate)",
         pm.unlockAchievement(mixcoach::Achievement::FirstTrack) == false);
    TEST("Count still = 3 after duplicate",
         pm.getAchievementCount() == 3);

    TEST("Unlock FiveTracks again → false (duplicate)",
         pm.unlockAchievement(mixcoach::Achievement::FiveTracks) == false);
    TEST("Count still = 3 after second duplicate",
         pm.getAchievementCount() == 3);

    // Desbloquear todos los 7 logros
    pm.unlockAchievement(mixcoach::Achievement::FullMix);
    pm.unlockAchievement(mixcoach::Achievement::PhaseMaster);
    pm.unlockAchievement(mixcoach::Achievement::DynamicControl);
    pm.unlockAchievement(mixcoach::Achievement::GainGod);
    TEST("Count = 7 after all achievements",
         pm.getAchievementCount() == 7);

    // Ya no se pueden desbloquear más
    TEST("Unlock after all unlocked → false",
         pm.unlockAchievement(mixcoach::Achievement::GainGod) == false);
    TEST("Count still = 7 after final duplicate",
         pm.getAchievementCount() == 7);

    registry.reset();
}

static void test_phase_completion() {
    std::printf("\n── Phase Completion ──\n");

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    mixcoach::PhaseManager pm(*registry);

    // Welcome siempre está completo
    TEST("Welcome is always complete",
         pm.isPhaseComplete(mixcoach::MentorPhase::Welcome) == true);

    // GainStaging: completa cuando activeCount > 0
    TEST("GainStaging incomplete with 0 tracks",
         pm.isPhaseComplete(mixcoach::MentorPhase::GainStaging) == false);

    // Registrar 1 pista → GainStaging se completa
    registry->registerSlot("Pista 1", juce::Colours::red, mixcoach::BusType::Drums);
    TEST("GainStaging complete with 1 track",
         pm.isPhaseComplete(mixcoach::MentorPhase::GainStaging) == true);

    // Organisation: necesita >= 3 tracks activos
    TEST("Organisation incomplete with 1 track",
         pm.isPhaseComplete(mixcoach::MentorPhase::Organisation) == false);

    registry->registerSlot("Pista 2", juce::Colours::blue, mixcoach::BusType::Bass);
    TEST("Organisation incomplete with 2 tracks",
         pm.isPhaseComplete(mixcoach::MentorPhase::Organisation) == false);

    registry->registerSlot("Pista 3", juce::Colours::green, mixcoach::BusType::Guitars);
    TEST("Organisation complete with 3 tracks",
         pm.isPhaseComplete(mixcoach::MentorPhase::Organisation) == true);

    // TonalBalance, Dynamics, Spatial: siempre completas (por diseño)
    TEST("TonalBalance always complete",
         pm.isPhaseComplete(mixcoach::MentorPhase::TonalBalance) == true);
    TEST("Dynamics always complete",
         pm.isPhaseComplete(mixcoach::MentorPhase::Dynamics) == true);
    TEST("Spatial always complete",
         pm.isPhaseComplete(mixcoach::MentorPhase::Spatial) == true);

    // Fase inválida → false
    auto invalidPhase = static_cast<mixcoach::MentorPhase>(99);
    TEST("Invalid phase is not complete",
         pm.isPhaseComplete(invalidPhase) == false);

    registry.reset();
}

static void test_phase_progress() {
    std::printf("\n── Phase Progress ──\n");

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    mixcoach::PhaseManager pm(*registry);

    // Welcome → 100%
    TEST("Welcome progress = 1.0",
         pm.getPhaseProgress(mixcoach::MentorPhase::Welcome) == 1.0f);

    // GainStaging: 0.5 si hay tracks, 0.0 si no
    TEST("GainStaging progress = 0.0 with 0 tracks",
         pm.getPhaseProgress(mixcoach::MentorPhase::GainStaging) == 0.0f);

    registry->registerSlot("Pista 1", juce::Colours::red, mixcoach::BusType::Drums);
    TEST("GainStaging progress = 0.5 with 1 track",
         pm.getPhaseProgress(mixcoach::MentorPhase::GainStaging) >= 0.49f);

    // Organisation: activeCount / 10.0, clamped to [0, 1]
    float prog3 = pm.getPhaseProgress(mixcoach::MentorPhase::Organisation);
    TEST("Organisation progress with 1 track ≈ 0.1",
         prog3 > 0.05f && prog3 < 0.15f);

    // Registrar 9 tracks más = 10 total → Organisation progress = 1.0
    for (int i = 2; i <= 10; ++i) {
        registry->registerSlot("Pista " + std::to_string(i),
                               juce::Colours::blue, mixcoach::BusType::Bass);
    }
    TEST("Organisation progress = 1.0 with 10 tracks",
         pm.getPhaseProgress(mixcoach::MentorPhase::Organisation) >= 0.99f);

    // TonalBalance, Dynamics, Spatial → 0.0 (no implementados)
    TEST("TonalBalance progress = 0.0",
         pm.getPhaseProgress(mixcoach::MentorPhase::TonalBalance) == 0.0f);
    TEST("Dynamics progress = 0.0",
         pm.getPhaseProgress(mixcoach::MentorPhase::Dynamics) == 0.0f);
    TEST("Spatial progress = 0.0",
         pm.getPhaseProgress(mixcoach::MentorPhase::Spatial) == 0.0f);

    // Fase inválida → 0.0
    auto invalidPhase = static_cast<mixcoach::MentorPhase>(99);
    TEST("Invalid phase progress = 0.0",
         pm.getPhaseProgress(invalidPhase) == 0.0f);

    registry.reset();
}

#endif // !TEST_PHASEMANAGER_STANDALONE

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("══════════════════════════════════════════════════════════\n");
    std::printf("  PhaseManager Unit Tests\n");
    std::printf("══════════════════════════════════════════════════════════\n");

    test_enum_values();
    test_phase_descriptions();
    test_min_tracks();

#ifndef TEST_PHASEMANAGER_STANDALONE
    test_phase_transitions();
    test_achievement_unlocking();
    test_phase_completion();
    test_phase_progress();
#endif

    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("══════════════════════════════════════════════════════════\n");

    return gTestsFailed > 0 ? 1 : 0;
}

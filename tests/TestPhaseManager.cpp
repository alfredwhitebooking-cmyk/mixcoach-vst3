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

    TEST("Organizacion = 0",
         static_cast<int>(mixcoach::MentorPhase::Organizacion) == 0);
    TEST("GainStaging = 1",
         static_cast<int>(mixcoach::MentorPhase::GainStaging) == 1);
    TEST("Balance = 2",
         static_cast<int>(mixcoach::MentorPhase::Balance) == 2);
    TEST("EQ = 3",
         static_cast<int>(mixcoach::MentorPhase::EQ) == 3);
    TEST("Compresion = 4",
         static_cast<int>(mixcoach::MentorPhase::Compresion) == 4);
    TEST("Espacio = 5",
         static_cast<int>(mixcoach::MentorPhase::Espacio) == 5);
    TEST("MasterCheck = 6",
         static_cast<int>(mixcoach::MentorPhase::MasterCheck) == 6);

    TEST("FirstTrack = 0",
         static_cast<int>(mixcoach::Achievement::FirstTrack) == 0);
    TEST("FiveTracks = 1",
         static_cast<int>(mixcoach::Achievement::FiveTracks) == 1);
    TEST("TenTracks = 2",
         static_cast<int>(mixcoach::Achievement::TenTracks) == 2);
    TEST("FullMap = 3",
         static_cast<int>(mixcoach::Achievement::FullMap) == 3);
    TEST("PhaseMaster = 4",
         static_cast<int>(mixcoach::Achievement::PhaseMaster) == 4);
    TEST("FirstReference = 5",
         static_cast<int>(mixcoach::Achievement::FirstReference) == 5);
    TEST("MixComplete = 6",
         static_cast<int>(mixcoach::Achievement::MixComplete) == 6);
}

static void test_phase_descriptions() {
    std::printf("\n── Phase Descriptions ──\n");

    TEST("Organizacion has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Organizacion) != nullptr);
    TEST("GainStaging has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::GainStaging) != nullptr);
    TEST("Balance has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Balance) != nullptr);
    TEST("EQ has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::EQ) != nullptr);
    TEST("Compresion has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Compresion) != nullptr);
    TEST("Espacio has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Espacio) != nullptr);
    TEST("MasterCheck has description",
         mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::MasterCheck) != nullptr);

    // All descriptions should be non-empty
    TEST("Organizacion description non-empty",
         std::strlen(mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::Organizacion)) > 0);
    TEST("MasterCheck description non-empty",
         std::strlen(mixcoach::PhaseManager::phaseDescription(mixcoach::MentorPhase::MasterCheck)) > 0);
}

// ============================================================================
//  Tests de minTracksForPhase (modo standalone + JUCE)
// ============================================================================

static void test_min_tracks() {
    std::printf("\n── minTracksForPhase ──\n");

    // Valores esperados
    TEST("Organizacion → 0",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Organizacion) == 0);
    TEST("GainStaging → 1",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::GainStaging) == 1);
    TEST("Balance → 3",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Balance) == 3);
    TEST("EQ → 3",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::EQ) == 3);
    TEST("Compresion → 5",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Compresion) == 5);
    TEST("Espacio → 3",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Espacio) == 3);
    TEST("MasterCheck → 3",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::MasterCheck) == 3);

    // Edge cases: valores fuera de rango
    auto invalidLow  = static_cast<mixcoach::MentorPhase>(-1);
    auto invalidHigh = static_cast<mixcoach::MentorPhase>(99);
    TEST("Invalid low (-1) → 0",
         mixcoach::PhaseManager::minTracksForPhase(invalidLow) == 0);
    TEST("Invalid high (99) → 0",
         mixcoach::PhaseManager::minTracksForPhase(invalidHigh) == 0);

    // Monotonía estricta: cada fase requiere >= tracks que la anterior
    TEST("GainStaging >= Organizacion",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::GainStaging) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Organizacion));
    TEST("Balance >= GainStaging",
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Balance) >=
         mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::GainStaging));

    // Valor máximo de tracks requerido
    int maxTracks = 0;
    for (int p = 0; p <= static_cast<int>(mixcoach::MentorPhase::MasterCheck); ++p) {
        int t = mixcoach::PhaseManager::minTracksForPhase(static_cast<mixcoach::MentorPhase>(p));
        if (t > maxTracks) maxTracks = t;
    }
    TEST("Compresion has max track requirement",
         maxTracks == mixcoach::PhaseManager::minTracksForPhase(mixcoach::MentorPhase::Compresion));
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
    TEST("Initial phase is Organizacion",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Organizacion);

    // Transiciones progresivas: Organizacion → GainStaging → Balance → EQ → Compresion → Espacio → MasterCheck
    pm.advanceToNextPhase();
    TEST("After advance 1 → GainStaging",
         pm.getCurrentPhase() == mixcoach::MentorPhase::GainStaging);

    pm.advanceToNextPhase();
    TEST("After advance 2 → Balance",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Balance);

    pm.advanceToNextPhase();
    TEST("After advance 3 → EQ",
         pm.getCurrentPhase() == mixcoach::MentorPhase::EQ);

    pm.advanceToNextPhase();
    TEST("After advance 4 → Compresion",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Compresion);

    pm.advanceToNextPhase();
    TEST("After advance 5 → Espacio",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Espacio);

    pm.advanceToNextPhase();
    TEST("After advance 6 → MasterCheck",
         pm.getCurrentPhase() == mixcoach::MentorPhase::MasterCheck);

    // No debe avanzar más allá de MasterCheck
    pm.advanceToNextPhase();
    TEST("After advance 7 → still MasterCheck (no overflow)",
         pm.getCurrentPhase() == mixcoach::MentorPhase::MasterCheck);

    pm.advanceToNextPhase();
    pm.advanceToNextPhase();
    TEST("After multiple advances → still MasterCheck",
         pm.getCurrentPhase() == mixcoach::MentorPhase::MasterCheck);

    // setPhase() debe saltar a cualquier fase directamente
    pm.setPhase(mixcoach::MentorPhase::Organizacion);
    TEST("setPhase(Organizacion) works",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Organizacion);

    pm.setPhase(mixcoach::MentorPhase::MasterCheck);
    TEST("setPhase(MasterCheck) works",
         pm.getCurrentPhase() == mixcoach::MentorPhase::MasterCheck);

    pm.setPhase(mixcoach::MentorPhase::Espacio);
    TEST("setPhase(Espacio) works",
         pm.getCurrentPhase() == mixcoach::MentorPhase::Espacio);

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
    pm.unlockAchievement(mixcoach::Achievement::FullMap);
    pm.unlockAchievement(mixcoach::Achievement::PhaseMaster);
    pm.unlockAchievement(mixcoach::Achievement::FirstReference);
    pm.unlockAchievement(mixcoach::Achievement::MixComplete);
    TEST("Count = 7 after all achievements",
         pm.getAchievementCount() == 7);

    // Ya no se pueden desbloquear más
    TEST("Unlock after all unlocked → false",
         pm.unlockAchievement(mixcoach::Achievement::MixComplete) == false);
    TEST("Count still = 7 after final duplicate",
         pm.getAchievementCount() == 7);

    registry.reset();
}

static void test_phase_completion() {
    std::printf("\n── Phase Completion ──\n");

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    mixcoach::PhaseManager pm(*registry);

    // Organizacion: necesita al menos 1 track activo
    TEST("Organizacion complete with 0 tracks",
         pm.isPhaseComplete(mixcoach::MentorPhase::Organizacion) == false);

    // Registrar 1 pista → Organizacion se completa
    registry->registerSlot("Pista 1", juce::Colours::red, mixcoach::BusType::Drums);
    TEST("Organizacion complete with 1 track",
         pm.isPhaseComplete(mixcoach::MentorPhase::Organizacion) == true);

    // GainStaging: depende de gainMetrics_.hasData
    TEST("GainStaging incomplete without metrics",
         pm.isPhaseComplete(mixcoach::MentorPhase::GainStaging) == false);

    // Balance: depende de balanceMetrics_.hasData
    TEST("Balance incomplete without metrics",
         pm.isPhaseComplete(mixcoach::MentorPhase::Balance) == false);

    // EQ: depends on tonalMetrics
    TEST("EQ incomplete without metrics",
         pm.isPhaseComplete(mixcoach::MentorPhase::EQ) == false);

    // Compresion: depends on dynamicsMetrics
    TEST("Compresion incomplete without metrics",
         pm.isPhaseComplete(mixcoach::MentorPhase::Compresion) == false);

    // Espacio: depends on espacioMetrics
    TEST("Espacio incomplete without metrics",
         pm.isPhaseComplete(mixcoach::MentorPhase::Espacio) == false);

    // MasterCheck: siempre completa (fase final)
    TEST("MasterCheck always complete",
         pm.isPhaseComplete(mixcoach::MentorPhase::MasterCheck) == true);

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

    // Organizacion → 100% siempre
    TEST("Organizacion progress = 1.0",
         pm.getPhaseProgress(mixcoach::MentorPhase::Organizacion) == 1.0f);
    TEST("Organizacion progress = 1.0",
         pm.getPhaseProgress(mixcoach::MentorPhase::Organizacion) == 1.0f);

    // GainStaging: 0.0 sin metrics
    TEST("GainStaging progress = 0.0 without metrics",
         pm.getPhaseProgress(mixcoach::MentorPhase::GainStaging) == 0.0f);

    // Balance: 0.0 sin metrics
    float progBal = pm.getPhaseProgress(mixcoach::MentorPhase::Balance);
    TEST("Balance progress = 0.0 without metrics",
         progBal == 0.0f);

    // Registrar 10 tracks para probar progress
    for (int i = 1; i <= 10; ++i) {
        registry->registerSlot("Track " + std::to_string(i),
                               juce::Colours::blue, mixcoach::BusType::Bass);
    }

    // EQ: 0.0 sin metrics
    TEST("EQ progress = 0.0 without metrics",
         pm.getPhaseProgress(mixcoach::MentorPhase::EQ) == 0.0f);

    // Compresion: 0.0 sin metrics
    TEST("Compresion progress = 0.0 without metrics",
         pm.getPhaseProgress(mixcoach::MentorPhase::Compresion) == 0.0f);

    // MasterCheck: 0.0 sin metrics
    TEST("MasterCheck progress = 0.0 without metrics",
         pm.getPhaseProgress(mixcoach::MentorPhase::MasterCheck) == 0.0f);

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

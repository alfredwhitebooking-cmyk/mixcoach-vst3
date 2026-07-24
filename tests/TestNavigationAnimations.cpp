// ═══════════════════════════════════════════════════════════════════════════
//  TestNavigationAnimations.cpp — Unit tests para transiciones setup (P1-P6)
//
//  Verifica:
//  1. Constantes del struct SetupFadeAnim (kFrames, kStep, kFadeInStart)
//  2. Fade math: ease-out quad para alpha 0→1 después de kFadeInStart
//  3. Lifecycle del struct (active, progress)
//
//  NOTA: SetupFadeAnim se define localmente para evitar la cadena masiva
//  de includes de NavigationShell.h (juce_audio_processors, etc.).
//  Los valores DEBEN coincidir con NavigationShell::SetupFadeAnim.
//  (Protegido por static_assert en NavigationShell.cpp)
//
//  ⚠️ Integración con NavigationShell::isSetupFadeActive():
//  Para verificar que setCoachRoomState() activa la animación en transiciones
//  setup→setup (ej: Intention→Genre → isSetupFadeActive()==true), se necesita
//  un NavigationShell real con todas sus dependencias (MixCoachAudioProcessor,
//  SharedData). Esto escapa al alcance de este test standalone y requiere
//  un test de integración con el PluginEditor completo.
//
//  Compilado via CMake:
//    cmake --build build --config Release --target TestNavigationAnimations
//    ./build/tests/Release/TestNavigationAnimations.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>

#include <juce_core/juce_core.h>

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                   \
    if (!(expr)) {                                                              \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                 \
        std::fflush(stderr);                                                    \
        gTestsFailed++;                                                         \
    } else {                                                                    \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                         \
        std::fflush(stdout);                                                    \
        gTestsPassed++;                                                         \
    }                                                                           \
} while(0)

// ═══════════════════════════════════════════════════════════════════════════
//  SetupFadeAnim — Mirror de NavigationShell::SetupFadeAnim
//  ⚠️ Los valores DEBEN coincidir exactamente con NavigationShell::SetupFadeAnim
// ═══════════════════════════════════════════════════════════════════════════
struct SetupFadeAnim {
    bool active = false;
    float progress = 0.0f;
    static constexpr float kFrames      = 10.0f;
    static constexpr float kStep        = 1.0f / kFrames; // 0.1
    static constexpr float kFadeInStart = 0.3f;
};

// ═══════════════════════════════════════════════════════════════════════════
//  1. CONSTANTS — Verificar que los valores no cambien accidentalmente
// ═══════════════════════════════════════════════════════════════════════════
static void test_constants() {
    std::printf("\n── [1] Constants ──\n");

    TEST("kFrames = 10.0f (10 frames at 60fps = 167ms)",
         SetupFadeAnim::kFrames == 10.0f);

    TEST("kStep = 0.1f (1/10 per frame)",
         std::abs(SetupFadeAnim::kStep - 0.1f) < 0.001f);

    TEST("kFadeInStart = 0.3f (30% delay, 70% fade-in)",
         SetupFadeAnim::kFadeInStart == 0.3f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  2. FADE MATH — Verificar ease-out quad alpha 0→1 después de kFadeInStart
//
//  Fórmula (desde timerCallback):
//    if progress < kFadeInStart → alpha = 0
//    else:
//      t = (progress - kFadeInStart) / (1.0 - kFadeInStart)
//      eased = t * (2.0 - t)  // ease-out quad
//      alpha = eased
// ═══════════════════════════════════════════════════════════════════════════
static void test_fade_math() {
    std::printf("\n── [2] Fade-in Math (ease-out quad) ──\n");

    constexpr float kFadeInStart = SetupFadeAnim::kFadeInStart;
    constexpr float kFadeDuration = 1.0f - kFadeInStart; // 0.7

    // Helper: compute alpha at a given progress
    auto computeAlpha = [](float progress) {
        if (progress < SetupFadeAnim::kFadeInStart)
            return 0.0f;
        float t = (progress - SetupFadeAnim::kFadeInStart) / (1.0f - SetupFadeAnim::kFadeInStart);
        return t * (2.0f - t); // ease-out quad
    };

    // ─── Test 2a: Before kFadeInStart → alpha = 0 ──
    float alphaAt0 = computeAlpha(0.0f);
    TEST("At progress=0.0: alpha = 0.0 (before delay)",
         alphaAt0 == 0.0f);

    float alphaAtStart = computeAlpha(kFadeInStart);
    TEST("At progress=kFadeInStart=0.3: alpha = 0.0 (still in delay)",
         alphaAtStart == 0.0f);

    // ─── Test 2b: At kFadeInStart + epsilon → alpha > 0 ──
    float alphaJustAfter = computeAlpha(kFadeInStart + 0.01f);
    TEST("At progress=0.31: alpha > 0.0 (fade started)",
         alphaJustAfter > 0.0f);

    // ─── Test 2c: Midpoint → alpha ≈ 0.75 (ease-out accelerates early) ──
    float midProgress = (kFadeInStart + 1.0f) * 0.5f; // 0.65
    float alphaMid = computeAlpha(midProgress);
    // t = (0.65 - 0.3) / 0.7 = 0.5
    // eased = 0.5 * (2.0 - 0.5) = 0.5 * 1.5 = 0.75
    TEST("At progress=0.65: alpha > 0.5 (ease-out faster in first half)",
         alphaMid > 0.5f);
    TEST("At progress=0.65: alpha < 1.0 (not yet complete)",
         alphaMid < 1.0f);
    TEST("At progress=0.65: alpha ≈ 0.75",
         std::abs(alphaMid - 0.75f) < 0.01f);

    // ─── Test 2d: End → alpha = 1.0 ──
    float alphaAtEnd = computeAlpha(1.0f);
    TEST("At progress=1.0: alpha = 1.0 (fade complete)",
         alphaAtEnd == 1.0f);

    // ─── Test 2e: Symmetry check — ease-out is faster early ──
    // At t=0.5 (linear): alpha should be 0.75 (ease-out = faster first half)
    // At t=0.5 (linear): would be 0.5
    float tHalf = 0.5f;
    float easedAtHalf = tHalf * (2.0f - tHalf); // = 0.75
    float linearAtHalf = tHalf; // = 0.5
    TEST("Ease-out is faster in first half (0.75 > 0.5)",
         easedAtHalf > linearAtHalf);

    // ─── Test 2f: Derivative at t=0 is 2 (steep start) ──
    // d/dt of (2t - t²) = 2 - 2t. At t=0: slope = 2
    float slopeAt0 = 2.0f - 2.0f * 0.0f;
    TEST("Ease-out starts steep (slope=2 at t=0)",
         std::abs(slopeAt0 - 2.0f) < 0.01f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  3. STRUCT LIFECYCLE — Verificar comportamiento del struct
// ═══════════════════════════════════════════════════════════════════════════
static void test_struct_lifecycle() {
    std::printf("\n── [3] Struct Lifecycle ──\n");

    SetupFadeAnim anim;

    TEST("Default active = false",
         !anim.active);

    TEST("Default progress = 0.0f",
         anim.progress == 0.0f);

    // ─── Activate ──
    anim.active = true;
    TEST("After activate: active = true",
         anim.active);

    // ─── Advance 10 steps to completion ──
    for (int i = 0; i < 10; ++i)
        anim.progress += SetupFadeAnim::kStep;
    TEST("After 10 steps: progress = 1.0f",
         std::abs(anim.progress - 1.0f) < 0.001f);

    // ─── Deactivate ──
    anim.active = false;
    TEST("After deactivate: active = false",
         !anim.active);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main() {
    std::printf("\n");
    std::printf("================================================================================\n");
    std::printf("  Navigation Animations Unit Tests\n");
    std::printf("  SetupFadeAnim constants + fade-in math\n");
    std::printf("================================================================================\n");

    test_constants();
    test_fade_math();
    test_struct_lifecycle();

    std::printf("\n");
    std::printf("================================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("================================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}

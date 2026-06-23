// ═══════════════════════════════════════════════════════════════════════════
//  TestExpectedProfiles — Calibración de la base de conocimiento de roles
//
//  Este test VALIDA que los 40+ ExpectedProfiles son coherentes entre sí
//  y están dentro de rangos razonables según ingeniería de audio.
//  Es documentación viva: si alguien edita un perfil, este test falla.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

#include "Source/MixCoach/engine/TrackRole.h"

using namespace mixcoach;

// ─── Helpers ────────────────────────────────────────────────────────────────
static int g_pass = 0, g_fail = 0;

static void CHECK(bool condition, const char* expr, const char* msg) {
    if (condition) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("  FAIL: %s — %s\n", expr, msg);
    }
}

static void CHECK_RANGE(float value, float lo, float hi, const char* name,
                         const char* profile) {
    if (value >= lo && value <= hi) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("  FAIL: %s [%s] = %.1f (expected %.1f..%.1f)\n",
               name, profile, value, lo, hi);
    }
}

// Print a compact profile table for visual inspection
static void printProfileTable() {
    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  EXPECTED PROFILES TABLE — Visual Calibration Reference                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ %-18s %-6s %-6s %-6s  Spectrum (dB offset from peak)                    ║\n", "Role", "Peak", "Crest", "Trgt");
    printf("║ %-18s %-6s %-6s %-6s  Sub  Bass  LoMid HiMid Pres  Air                  ║\n", "", "Tgt", "Tgt", "");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");

    // Iterate all roles except Unknown
    const TrackRole roles[] = {
        TrackRole::Kick, TrackRole::Kick808, TrackRole::Snare, TrackRole::SnareTrap,
        TrackRole::HiHat, TrackRole::HiHatOpen, TrackRole::Ride, TrackRole::Crash,
        TrackRole::Tom, TrackRole::TomFloor, TrackRole::Percussion, TrackRole::Clap,
        TrackRole::ReggaetonKick, TrackRole::DrumBus, TrackRole::DrumRoom,
        TrackRole::BassSub, TrackRole::Bass808, TrackRole::BassPick,
        TrackRole::BassFinger, TrackRole::BassSynth, TrackRole::BassBus,
        TrackRole::GuitarAcoustic, TrackRole::GuitarElectric,
        TrackRole::GuitarRhythm, TrackRole::GuitarLead, TrackRole::GuitarBus,
        TrackRole::SynthLead, TrackRole::SynthPad, TrackRole::SynthPluck,
        TrackRole::KeysPiano, TrackRole::KeysElectric, TrackRole::KeysOrgan,
        TrackRole::KeysBus, TrackRole::VozPrincipal, TrackRole::VozFondo,
        TrackRole::VozDouble, TrackRole::Adlibs, TrackRole::VozBus,
        TrackRole::FxRiser, TrackRole::FxImpact, TrackRole::FxAmbience, TrackRole::FxNoise,
        TrackRole::Strings, TrackRole::Brass, TrackRole::Winds, TrackRole::MelodyBus,
        TrackRole::Master
    };

    for (auto role : roles) {
        auto p = getExpectedProfile(role);
        printf("║ %-18s %5.1f   %5.1f %5.1f  %4.0f  %4.0f  %4.0f  %4.0f  %4.0f  %4.0f ║\n",
               p.name,
               p.peakTargetDb, p.crestTargetDb, p.transientRatioTarget,
               p.spectralOffset[0], p.spectralOffset[1], p.spectralOffset[2],
               p.spectralOffset[3], p.spectralOffset[4], p.spectralOffset[5]);
    }
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 1: Rangos razonables para TODOS los perfiles
// ═══════════════════════════════════════════════════════════════════════════
static void testUniversalRanges() {
    printf("── TEST 1: Universal Range Validation ──\n");

    const TrackRole roles[] = {
        TrackRole::Kick, TrackRole::Kick808, TrackRole::Snare, TrackRole::SnareTrap,
        TrackRole::HiHat, TrackRole::HiHatOpen, TrackRole::Tom, TrackRole::Percussion,
        TrackRole::Clap, TrackRole::ReggaetonKick, TrackRole::DrumBus, TrackRole::DrumRoom,
        TrackRole::BassSub, TrackRole::Bass808, TrackRole::BassPick,
        TrackRole::BassFinger, TrackRole::BassBus,
        TrackRole::GuitarAcoustic, TrackRole::GuitarElectric, TrackRole::GuitarLead,
        TrackRole::GuitarBus,
        TrackRole::SynthLead, TrackRole::SynthPad, TrackRole::SynthPluck,
        TrackRole::KeysPiano, TrackRole::KeysBus,
        TrackRole::VozPrincipal, TrackRole::VozFondo, TrackRole::Adlibs, TrackRole::VozBus,
        TrackRole::FxRiser, TrackRole::FxImpact, TrackRole::FxAmbience,
        TrackRole::Strings, TrackRole::Brass
    };

    for (auto role : roles) {
        auto p = getExpectedProfile(role);

        // Peak target debe estar entre -24 y 0 dBFS
        CHECK_RANGE(p.peakTargetDb, -24.0f, 0.0f, "peakTargetDb", p.name);

        // Tolerancia de peak: entre 2 y 12 dB
        CHECK_RANGE(p.peakTolerance, 2.0f, 12.0f, "peakTolerance", p.name);

        // Crest factor: entre 4 y 22 dB (nada tiene crest 0 o 30)
        CHECK_RANGE(p.crestTargetDb, 4.0f, 22.0f, "crestTargetDb", p.name);

        // Tolerancia de crest: entre 2 y 12 dB
        CHECK_RANGE(p.crestTolerance, 2.0f, 12.0f, "crestTolerance", p.name);

        // Spectral offsets deben ser negativos (por debajo del peak)
        for (int b = 0; b < 6; ++b) {
            char name[64];
            snprintf(name, sizeof(name), "spectralOffset[%d]", b);
            CHECK(p.spectralOffset[b] <= 0.0f, name,
                  (std::string("positive offset in ") + p.name).c_str());
            CHECK_RANGE(p.spectralOffset[b], -80.0f, 0.0f, name, p.name);
        }

        // Stereo width: entre 0 y 1
        for (int b = 0; b < 6; ++b) {
            char name[64];
            snprintf(name, sizeof(name), "stereoWidthMax[%d]", b);
            CHECK_RANGE(p.stereoWidthMax[b], 0.0f, 1.0f, name, p.name);
        }

        // Transient ratio: 0 a 8
        CHECK_RANGE(p.transientRatioTarget, 0.0f, 8.0f, "transientRatioTarget", p.name);

        // Fundamental: 20 Hz a 20 kHz
        CHECK(p.fundamentalMin <= p.fundamentalMax, "fundamentalMin <= Max", p.name);
        CHECK_RANGE(p.fundamentalMin, 20.0f, 20000.0f, "fundamentalMin", p.name);

        // Centroid: 20 Hz a 20 kHz
        CHECK(p.centroidMin <= p.centroidMax, "centroidMin <= Max", p.name);
        CHECK_RANGE(p.centroidMin, 20.0f, 20000.0f, "centroidMin", p.name);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 2: Coherencia entre roles relacionados
// ═══════════════════════════════════════════════════════════════════════════
static void testRoleCoherence() {
    printf("\n── TEST 2: Role Coherence ──\n");

    // Kick debe tener más crest que BassSub (bombo es transiente, sub es sostenido)
    auto kick = getExpectedProfile(TrackRole::Kick);
    auto bassSub = getExpectedProfile(TrackRole::BassSub);
    CHECK(kick.crestTargetDb > bassSub.crestTargetDb,
          "Kick.crest > BassSub.crest",
          "Kick should be more dynamic than sustained sub-bass");

    // BassSub debe ser casi mono (width <= 0.15 en sub)
    CHECK(bassSub.stereoWidthMax[0] <= 0.15f,
          "BassSub stereo width Sub <= 0.15",
          "Sub-bass should be nearly mono to avoid phase issues");

    // VozPrincipal centrada (stereo bajo en bajas frecuencias)
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal);
    CHECK(vocal.stereoWidthMax[0] <= 0.1f,
          "Vocal stereo width Sub <= 0.1",
          "Lead vocal should be centered in low frequencies");

    // VozPrincipal tiene target de peak en rango vocal (-8 a -4)
    CHECK(vocal.peakTargetDb >= -8.0f && vocal.peakTargetDb <= -4.0f,
          "Vocal peakTarget in [-8, -4]",
          "Lead vocal typically sits around -6 dBFS peak");

    // VozPrincipal tiene presencia en HiMid (spectralOffset[3] >= -12)
    CHECK(vocal.spectralOffset[3] >= -12.0f,
          "Vocal presence in HiMid",
          "Lead vocal should have energy in 2-4kHz presence region");

    // SynthPad debe tener crest bajo (sostenido)
    auto pad = getExpectedProfile(TrackRole::SynthPad);
    CHECK(pad.crestTargetDb <= 8.0f,
          "SynthPad crest <= 8",
          "Pads should be sustained with low crest factor");

    // DrumBus debe tener peak más alto que pistas individuales (es la suma)
    auto drumBus = getExpectedProfile(TrackRole::DrumBus);
    CHECK(drumBus.peakTargetDb >= kick.peakTargetDb - 2.0f,
          "DrumBus peak >= Kick peak - 2",
          "Drum bus peak should be near or above individual kick");

    // HiHat tiene energía en agudos (spectralOffset[5] >= -15)
    auto hihat = getExpectedProfile(TrackRole::HiHat);
    CHECK(hihat.spectralOffset[5] >= -15.0f,
          "HiHat energy in Air",
          "Hi-hats should have energy in 12kHz+ air region");

    // SnareTrap es más agudo que Snare (centroid más alto)
    auto snare = getExpectedProfile(TrackRole::Snare);
    auto snareTrap = getExpectedProfile(TrackRole::SnareTrap);
    CHECK(snareTrap.centroidMin > snare.centroidMin,
          "SnareTrap centroid > Snare centroid",
          "Trap snare should be brighter than regular snare");

    // 808 Kick tiene más sub que Kick (spectralOffset[0] más alto)
    auto kick808 = getExpectedProfile(TrackRole::Kick808);
    CHECK(kick808.spectralOffset[0] >= kick.spectralOffset[0],
          "808 Kick sub >= Kick sub",
          "808 kick should have more sub energy than acoustic kick");

    // Buses tienen crest menor que sus componentes (mezcla reduce dinámica)
    auto voxBus = getExpectedProfile(TrackRole::VozBus);
    CHECK(voxBus.crestTargetDb <= vocal.crestTargetDb,
          "VocalBus crest <= Vocal crest",
          "Bus should have lower crest than individual tracks");

    // Strings debe ser sostenido (crest bajo)
    auto strings = getExpectedProfile(TrackRole::Strings);
    CHECK(strings.crestTargetDb <= 10.0f,
          "Strings crest <= 10",
          "Strings sections should be moderately sustained");

    // FxAmbience es bajo nivel (peak target <= -15)
    auto ambience = getExpectedProfile(TrackRole::FxAmbience);
    CHECK(ambience.peakTargetDb <= -15.0f,
          "FxAmbience peak <= -15",
          "Ambience/reverb returns should be low level");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 3: Helpers de ExpectedProfile funcionan correctamente
// ═══════════════════════════════════════════════════════════════════════════
static void testProfileHelpers() {
    printf("\n── TEST 3: Profile Helper Methods ──\n");

    auto kick = getExpectedProfile(TrackRole::Kick);

    // isPeakInRange: peakTargetDb=-6, tolerance=4 → [-10, -2]
    CHECK(kick.isPeakInRange(-6.0f), "isPeakInRange(-6)", "Exact target");
    CHECK(kick.isPeakInRange(-10.0f), "isPeakInRange(-10)", "Lower bound");
    CHECK(kick.isPeakInRange(-2.0f), "isPeakInRange(-2)", "Upper bound");
    CHECK(!kick.isPeakInRange(-11.0f), "!isPeakInRange(-11)", "Below range");
    CHECK(!kick.isPeakInRange(-1.0f), "!isPeakInRange(-1)", "Above range");

    // isCrestInRange: crestTargetDb=14, tolerance=6 → [8, 20]
    CHECK(kick.isCrestInRange(14.0f), "isCrestInRange(14)", "Exact target");
    CHECK(kick.isCrestInRange(8.0f), "isCrestInRange(8)", "Lower bound");
    CHECK(kick.isCrestInRange(20.0f), "isCrestInRange(20)", "Upper bound");
    CHECK(!kick.isCrestInRange(7.0f), "!isCrestInRange(7)", "Below range");
    CHECK(!kick.isCrestInRange(21.0f), "!isCrestInRange(21)", "Above range");

    // getPeakDeviation
    CHECK(std::abs(kick.getPeakDeviation(-6.0f)) < 0.001f,
          "getPeakDeviation(peakTarget) == 0", "At target");
    CHECK(std::abs(kick.getPeakDeviation(-4.0f) - 2.0f) < 0.001f,
          "getPeakDeviation(-4) == 2", "2 dB above target");

    // getCrestDeviation
    CHECK(std::abs(kick.getCrestDeviation(14.0f)) < 0.001f,
          "getCrestDeviation(crestTarget) == 0", "At target");
    CHECK(std::abs(kick.getCrestDeviation(8.0f) - (-6.0f)) < 0.001f,
          "getCrestDeviation(8) == -6", "6 dB below target");

    // isWidthInBandOk
    CHECK(kick.isWidthInBandOk(0, 0.1f),
          "isWidthInBandOk(0, 0.1) true", "0.1 <= 0.1 (Sub max)");
    CHECK(!kick.isWidthInBandOk(0, 0.2f),
          "isWidthInBandOk(0, 0.2) false", "0.2 > 0.1 (Sub max)");
    CHECK(kick.isWidthInBandOk(-1, 0.5f),
          "isWidthInBandOk(-1, 0.5) true", "Out of range returns true");
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST 4: Spectral shape sanity (la forma del espectro tiene sentido)
// ═══════════════════════════════════════════════════════════════════════════
static void testSpectralShape() {
    printf("\n── TEST 4: Spectral Shape Sanity ──\n");

    // Kick: sub debe ser la banda con más energía (menos offset negativo)
    auto kick = getExpectedProfile(TrackRole::Kick);
    bool subIsLoudest = true;
    for (int b = 1; b < 6; ++b) {
        if (kick.spectralOffset[b] > kick.spectralOffset[0]) {
            subIsLoudest = false;
            break;
        }
    }
    CHECK(subIsLoudest, "Kick: Sub is loudest band",
          "Kick should have most energy in sub-bass region");

    // HiHat: Air debe ser la banda con más energía
    auto hihat = getExpectedProfile(TrackRole::HiHat);
    bool airIsLoudest = true;
    for (int b = 0; b < 5; ++b) {
        if (hihat.spectralOffset[b] > hihat.spectralOffset[5]) {
            airIsLoudest = false;
            break;
        }
    }
    CHECK(airIsLoudest, "HiHat: Air is loudest band",
          "Hi-hats should have most energy in air region");

    // Vocal: presencia en HiMid pero NO en Sub
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal);
    CHECK(vocal.spectralOffset[3] > vocal.spectralOffset[0],
          "Vocal: HiMid > Sub",
          "Vocal should have more presence than sub energy");
    CHECK(vocal.spectralOffset[0] < -20.0f,
          "Vocal: Sub offset < -20",
          "Vocals should have very little sub-bass energy");

    // BassSub: sub es dominante, decae rápidamente
    auto bassSub = getExpectedProfile(TrackRole::BassSub);
    CHECK(bassSub.spectralOffset[0] > bassSub.spectralOffset[1],
          "BassSub: Sub > Bass",
          "Sub-bass should have energy concentrated in lowest band");
    CHECK(bassSub.spectralOffset[2] < -20.0f,
          "BassSub: LoMid < -20",
          "Sub-bass should roll off sharply above bass region");
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════
int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║  TestExpectedProfiles — Audio Precision Brain Calibration       ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    printProfileTable();
    testUniversalRanges();
    testRoleCoherence();
    testProfileHelpers();
    testSpectralShape();

    printf("\n════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d PASS, %d FAIL\n", g_pass, g_fail);
    printf("════════════════════════════════════════════════════════════════\n\n");

    return (g_fail > 0) ? 1 : 0;
}

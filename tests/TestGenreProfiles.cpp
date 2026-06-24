// ═══════════════════════════════════════════════════════════════════════════
//  TestGenreProfiles.cpp — Genre-Aware Profile Unit Tests
//  Verifica que:
//    1. getGenreProfileDelta(genre, role) retorna deltas correctos por género
//    2. getExpectedProfile(role, genre) aplica deltas correctamente
//    3. Casos edge: género vacío, género desconocido, rol sin override
//    4. Combinaciones género + rol específico (Trap, Pop, Rock, Reggaeton, etc.)
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>

#include "Source/MixCoach/engine/TrackRole.h"

using namespace mixcoach;

// ─── Test framework ─────────────────────────────────────────────────────
static int g_pass = 0, g_fail = 0;

#define TEST(name, expr) do {                                               \
    if (!(expr)) {                                                          \
        std::printf("  FAIL  %s (%s:%d)\n", name, __FILE__, __LINE__);      \
        ++g_fail;                                                           \
    } else {                                                                \
        std::printf("  PASS  %s\n", name);                                  \
        ++g_pass;                                                           \
    }                                                                       \
} while(0)

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getGenreProfileDelta — Valores de delta específicos por género
// ═══════════════════════════════════════════════════════════════════════════

// ─── 1. Genre vacío → delta vacío ──────────────────────────────────────
static void testDeltaEmptyGenre()
{
    std::printf("\n── Test 1: getGenreProfileDelta — Empty Genre ──\n");

    auto delta = getGenreProfileDelta("", TrackRole::Kick);
    TEST("Empty genre returns Unknown role", delta.role == TrackRole::Unknown);
    TEST_NEAR("Empty genre peak delta = 0", delta.peakDeltaDb, 0.0f, 0.001f);
    TEST_NEAR("Empty genre crest delta = 0", delta.crestDeltaDb, 0.0f, 0.001f);
}

// ─── 2. Género desconocido → delta vacío ───────────────────────────────
static void testDeltaUnknownGenre()
{
    std::printf("\n── Test 2: getGenreProfileDelta — Unknown Genre ──\n");

    auto delta = getGenreProfileDelta("nonexistent_genre_xyz", TrackRole::Kick);
    TEST("Unknown genre returns Unknown role", delta.role == TrackRole::Unknown);
    TEST_NEAR("Unknown genre peak delta = 0", delta.peakDeltaDb, 0.0f, 0.001f);
    TEST_NEAR("Unknown genre crest delta = 0", delta.crestDeltaDb, 0.0f, 0.001f);
}

// ─── 3. Genre con nombre, pero rol sin override → delta vacío ──────────
static void testDeltaNoOverrideForRole()
{
    std::printf("\n── Test 3: getGenreProfileDelta — Genre exists but role has no override ──\n");

    // Trap has overrides for Kick, Bass808, etc. but NOT for Master
    auto delta = getGenreProfileDelta("trap", TrackRole::Master);
    TEST("Trap Master: Unknown role (no override)", delta.role == TrackRole::Unknown);

    // Pop has explicit entry for Kick with all-zero deltas (not Unknown role)
    auto delta2 = getGenreProfileDelta("pop", TrackRole::Kick);
    TEST("Pop Kick: role is set (explicit entry with zero deltas)", delta2.role == TrackRole::Kick);
    TEST_NEAR("Pop Kick: all deltas are zero", delta2.peakDeltaDb, 0.0f, 0.001f);
}

// ─── 4. Trap: deltas esperados ─────────────────────────────────────────
static void testDeltaTrap()
{
    std::printf("\n── Test 4: getGenreProfileDelta — Trap ──\n");

    // Trap Kick: crest +4dB (más agresivo)
    auto kick = getGenreProfileDelta("trap", TrackRole::Kick);
    TEST("Trap Kick role = Kick", kick.role == TrackRole::Kick);
    TEST_NEAR("Trap Kick crestDelta = 4.0", kick.crestDeltaDb, 4.0f, 0.001f);
    TEST_NEAR("Trap Kick peakDelta = 0", kick.peakDeltaDb, 0.0f, 0.001f);

    // Trap Bass808: crest +2dB, sub +2dB
    auto bass = getGenreProfileDelta("trap", TrackRole::Bass808);
    TEST("Trap Bass808 role = Bass808", bass.role == TrackRole::Bass808);
    TEST_NEAR("Trap Bass808 crestDelta = 2.0", bass.crestDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("Trap Bass808 subDelta = -2.0", bass.subDelta, -2.0f, 0.001f);

    // Trap SnareTrap: presencia +2dB, aire +2dB
    auto snare = getGenreProfileDelta("trap", TrackRole::SnareTrap);
    TEST("Trap SnareTrap role = SnareTrap", snare.role == TrackRole::SnareTrap);
    TEST_NEAR("Trap SnareTrap presDelta = 2.0", snare.presDelta, 2.0f, 0.001f);
    TEST_NEAR("Trap SnareTrap airDelta = 2.0", snare.airDelta, 2.0f, 0.001f);

    // Trap HiHat: aire +2dB (brillante)
    auto hihat = getGenreProfileDelta("trap", TrackRole::HiHat);
    TEST("Trap HiHat role = HiHat", hihat.role == TrackRole::HiHat);
    TEST_NEAR("Trap HiHat airDelta = 2.0", hihat.airDelta, 2.0f, 0.001f);

    // Trap VozPrincipal: peak -2dB, presencia +2dB
    auto vocal = getGenreProfileDelta("trap", TrackRole::VozPrincipal);
    TEST("Trap VozPrincipal role = VozPrincipal", vocal.role == TrackRole::VozPrincipal);
    TEST_NEAR("Trap VozPrincipal peakDelta = -2.0", vocal.peakDeltaDb, -2.0f, 0.001f);
    TEST_NEAR("Trap VozPrincipal presDelta = 2.0", vocal.presDelta, 2.0f, 0.001f);

    // Trap BassSub: peak +2dB, sub +3dB
    auto bassSub = getGenreProfileDelta("trap", TrackRole::BassSub);
    TEST("Trap BassSub role = BassSub", bassSub.role == TrackRole::BassSub);
    TEST_NEAR("Trap BassSub peakDelta = 2.0", bassSub.peakDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("Trap BassSub subDelta = -3.0", bassSub.subDelta, -3.0f, 0.001f);
}

// ─── 5. Pop: deltas esperados ──────────────────────────────────────────
static void testDeltaPop()
{
    std::printf("\n── Test 5: getGenreProfileDelta — Pop ──\n");

    // Pop VozPrincipal: crest -2dB (más comprimida), presencia +3dB
    auto vocal = getGenreProfileDelta("pop", TrackRole::VozPrincipal);
    TEST("Pop VozPrincipal role = VozPrincipal", vocal.role == TrackRole::VozPrincipal);
    TEST_NEAR("Pop VozPrincipal crestDelta = -2.0", vocal.crestDeltaDb, -2.0f, 0.001f);
    TEST_NEAR("Pop VozPrincipal presDelta = -3.0", vocal.presDelta, -3.0f, 0.001f);

    // Pop VozFondo: peak +2dB
    auto backing = getGenreProfileDelta("pop", TrackRole::VozFondo);
    TEST("Pop VozFondo role = VozFondo", backing.role == TrackRole::VozFondo);
    TEST_NEAR("Pop VozFondo peakDelta = 2.0", backing.peakDeltaDb, 2.0f, 0.001f);
}

// ─── 6. Rock: deltas esperados ─────────────────────────────────────────
static void testDeltaRock()
{
    std::printf("\n── Test 6: getGenreProfileDelta — Rock ──\n");

    // Rock Kick: peak +2dB, crest -2dB
    auto kick = getGenreProfileDelta("rock", TrackRole::Kick);
    TEST("Rock Kick role = Kick", kick.role == TrackRole::Kick);
    TEST_NEAR("Rock Kick peakDelta = 2.0", kick.peakDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("Rock Kick crestDelta = -2.0", kick.crestDeltaDb, -2.0f, 0.001f);

    // Rock GuitarElectric: hiMid +3dB, presencia +3dB
    auto gtr = getGenreProfileDelta("rock", TrackRole::GuitarElectric);
    TEST("Rock GuitarElectric role = GuitarElectric", gtr.role == TrackRole::GuitarElectric);
    TEST_NEAR("Rock GuitarElectric hiMidDelta = -3.0", gtr.hiMidDelta, -3.0f, 0.001f);
    TEST_NEAR("Rock GuitarElectric presDelta = -3.0", gtr.presDelta, -3.0f, 0.001f);

    // Rock VozPrincipal: crest +2dB (menos comprimida, más natural)
    auto vocal = getGenreProfileDelta("rock", TrackRole::VozPrincipal);
    TEST("Rock VozPrincipal role = VozPrincipal", vocal.role == TrackRole::VozPrincipal);
    TEST_NEAR("Rock VozPrincipal crestDelta = 2.0", vocal.crestDeltaDb, 2.0f, 0.001f);

    // Rock BassPick: crest +2dB (más dinámica)
    auto bass = getGenreProfileDelta("rock", TrackRole::BassPick);
    TEST("Rock BassPick role = BassPick", bass.role == TrackRole::BassPick);
    TEST_NEAR("Rock BassPick crestDelta = 2.0", bass.crestDeltaDb, 2.0f, 0.001f);
}

// ─── 7. Reggaeton: deltas esperados ────────────────────────────────────
static void testDeltaReggaeton()
{
    std::printf("\n── Test 7: getGenreProfileDelta — Reggaeton ──\n");

    // Reggaeton Bass808: peak +2dB, sub +3dB
    auto bass = getGenreProfileDelta("reggaeton", TrackRole::Bass808);
    TEST("Reggaeton Bass808 role = Bass808", bass.role == TrackRole::Bass808);
    TEST_NEAR("Reggaeton Bass808 peakDelta = 2.0", bass.peakDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("Reggaeton Bass808 subDelta = -3.0", bass.subDelta, -3.0f, 0.001f);

    // Reggaeton BassSub: peak +2dB, sub +4dB
    auto bassSub = getGenreProfileDelta("reggaeton", TrackRole::BassSub);
    TEST("Reggaeton BassSub role = BassSub", bassSub.role == TrackRole::BassSub);
    TEST_NEAR("Reggaeton BassSub peakDelta = 2.0", bassSub.peakDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("Reggaeton BassSub subDelta = -4.0", bassSub.subDelta, -4.0f, 0.001f);

    // Reggaeton VozPrincipal: presencia +2dB
    auto vocal = getGenreProfileDelta("reggaeton", TrackRole::VozPrincipal);
    TEST("Reggaeton VozPrincipal role = VozPrincipal", vocal.role == TrackRole::VozPrincipal);
    TEST_NEAR("Reggaeton VozPrincipal presDelta = -2.0", vocal.presDelta, -2.0f, 0.001f);

    // Reggaeton ReggaetonKick: role is set but all deltas are zero
    auto rkick = getGenreProfileDelta("reggaeton", TrackRole::ReggaetonKick);
    TEST("Reggaeton ReggaetonKick: role is set", rkick.role == TrackRole::ReggaetonKick);
    TEST_NEAR("Reggaeton ReggaetonKick peakDelta = 0 (all deltas zero)", rkick.peakDeltaDb, 0.0f, 0.001f);
}

// ─── 8. Afrobeat: deltas esperados ─────────────────────────────────────
static void testDeltaAfrobeat()
{
    std::printf("\n── Test 8: getGenreProfileDelta — Afrobeat ──\n");

    // Afrobeat Kick: peak +2dB
    auto kick = getGenreProfileDelta("afrobeat", TrackRole::Kick);
    TEST("Afrobeat Kick role = Kick", kick.role == TrackRole::Kick);
    TEST_NEAR("Afrobeat Kick peakDelta = 2.0", kick.peakDeltaDb, 2.0f, 0.001f);

    // Afrobeat BassSub: bass +2dB (cuerpo extra)
    auto bassSub = getGenreProfileDelta("afrobeat", TrackRole::BassSub);
    TEST("Afrobeat BassSub role = BassSub", bassSub.role == TrackRole::BassSub);
    TEST_NEAR("Afrobeat BassSub bassDelta = -2.0", bassSub.bassDelta, -2.0f, 0.001f);

    // Afrobeat Percussion: presencia +2dB
    auto perc = getGenreProfileDelta("afrobeat", TrackRole::Percussion);
    TEST("Afrobeat Percussion role = Percussion", perc.role == TrackRole::Percussion);
    TEST_NEAR("Afrobeat Percussion presDelta = -2.0", perc.presDelta, -2.0f, 0.001f);

    // Afrobeat VozPrincipal: aire +2dB
    auto vocal = getGenreProfileDelta("afrobeat", TrackRole::VozPrincipal);
    TEST("Afrobeat VozPrincipal role = VozPrincipal", vocal.role == TrackRole::VozPrincipal);
    TEST_NEAR("Afrobeat VozPrincipal airDelta = -2.0", vocal.airDelta, -2.0f, 0.001f);

    // Afrobeat BassFinger: bass +2dB (cálido)
    auto bassF = getGenreProfileDelta("afrobeat", TrackRole::BassFinger);
    TEST("Afrobeat BassFinger role = BassFinger", bassF.role == TrackRole::BassFinger);
    TEST_NEAR("Afrobeat BassFinger bassDelta = -2.0", bassF.bassDelta, -2.0f, 0.001f);
}

// ─── 9. EDM: deltas esperados ──────────────────────────────────────────
static void testDeltaEDM()
{
    std::printf("\n── Test 9: getGenreProfileDelta — EDM ──\n");

    // EDM Kick: peak +2dB, sub +2dB
    auto kick = getGenreProfileDelta("edm", TrackRole::Kick);
    TEST("EDM Kick role = Kick", kick.role == TrackRole::Kick);
    TEST_NEAR("EDM Kick peakDelta = 2.0", kick.peakDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("EDM Kick subDelta = -2.0", kick.subDelta, -2.0f, 0.001f);

    // EDM BassSub: peak +2dB, sub +4dB (sub masivo)
    auto bassSub = getGenreProfileDelta("edm", TrackRole::BassSub);
    TEST("EDM BassSub role = BassSub", bassSub.role == TrackRole::BassSub);
    TEST_NEAR("EDM BassSub peakDelta = 2.0", bassSub.peakDeltaDb, 2.0f, 0.001f);
    TEST_NEAR("EDM BassSub subDelta = -4.0", bassSub.subDelta, -4.0f, 0.001f);

    // EDM SynthLead: presencia +3dB, aire +3dB
    auto synth = getGenreProfileDelta("edm", TrackRole::SynthLead);
    TEST("EDM SynthLead role = SynthLead", synth.role == TrackRole::SynthLead);
    TEST_NEAR("EDM SynthLead presDelta = -3.0", synth.presDelta, -3.0f, 0.001f);
    TEST_NEAR("EDM SynthLead airDelta = -3.0", synth.airDelta, -3.0f, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getExpectedProfile(role, genre) — Aplicación de deltas
// ═══════════════════════════════════════════════════════════════════════════

// ─── 10. getExpectedProfile con género vacío = base ─────────────────────
static void testExpectedProfileEmptyGenre()
{
    std::printf("\n── Test 10: getExpectedProfile — Empty Genre = Base ──\n");

    // Sin género → debe ser idéntico a getExpectedProfile(role)
    auto base = getExpectedProfile(TrackRole::Kick);
    auto genre = getExpectedProfile(TrackRole::Kick, "");
    TEST_NEAR("Kick empty genre peak target = base", genre.peakTargetDb, base.peakTargetDb, 0.001f);
    TEST_NEAR("Kick empty genre crest target = base", genre.crestTargetDb, base.crestTargetDb, 0.001f);
    for (int b = 0; b < 6; ++b) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Kick spectralOffset[%d] = base", b);
        TEST_NEAR(buf, genre.spectralOffset[b], base.spectralOffset[b], 0.001f);
    }
}

// ─── 11. getExpectedProfile con género desconocido = base ───────────────
static void testExpectedProfileUnknownGenre()
{
    std::printf("\n── Test 11: getExpectedProfile — Unknown Genre = Base ──\n");

    auto base = getExpectedProfile(TrackRole::Kick);
    auto genre = getExpectedProfile(TrackRole::Kick, "unknown_genre_xyz");
    TEST_NEAR("Kick unknown genre peak target = base", genre.peakTargetDb, base.peakTargetDb, 0.001f);
    TEST_NEAR("Kick unknown genre crest target = base", genre.crestTargetDb, base.crestTargetDb, 0.001f);
}

// ─── 12. getExpectedProfile — Trap aplica deltas correctamente ──────────
static void testExpectedProfileTrap()
{
    std::printf("\n── Test 12: getExpectedProfile — Trap profile deltas applied ──\n");

    // Trap Kick: crest base 14.0 + delta 4.0 = 18.0
    auto kick = getExpectedProfile(TrackRole::Kick, "trap");
    TEST_NEAR("Trap Kick crestTarget = 18.0", kick.crestTargetDb, 18.0f, 0.001f);
    TEST_NEAR("Trap Kick peakTarget = -6.0 (unchanged)", kick.peakTargetDb, -6.0f, 0.001f);

    // Trap Bass808: crest base 10.0 + delta 2.0 = 12.0, sub offset base -6.0 + delta -2.0 = -8.0
    auto bass = getExpectedProfile(TrackRole::Bass808, "trap");
    TEST_NEAR("Trap Bass808 crestTarget = 12.0", bass.crestTargetDb, 12.0f, 0.001f);
    TEST_NEAR("Trap Bass808 sub offset = -8.0", bass.spectralOffset[0], -8.0f, 0.001f);

    // Trap VozPrincipal: peak base -6.0 + delta -2.0 = -8.0, presence base -10.0 + delta 2.0 = -8.0
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal, "trap");
    TEST_NEAR("Trap VozPrincipal peakTarget = -8.0", vocal.peakTargetDb, -8.0f, 0.001f);
    TEST_NEAR("Trap VozPrincipal presence offset = -8.0", vocal.spectralOffset[4], -8.0f, 0.001f);

    // Trap BassSub: peak -8.0 + 2.0 = -6.0, sub offset -6.0 + -3.0 = -9.0
    auto bassSub = getExpectedProfile(TrackRole::BassSub, "trap");
    TEST_NEAR("Trap BassSub peakTarget = -6.0", bassSub.peakTargetDb, -6.0f, 0.001f);
    TEST_NEAR("Trap BassSub sub offset = -9.0", bassSub.spectralOffset[0], -9.0f, 0.001f);

    // Trap SnareTrap: pres offset -10.0 + 2.0 = -8.0, air offset -8.0 + 2.0 = -6.0
    auto snare = getExpectedProfile(TrackRole::SnareTrap, "trap");
    TEST_NEAR("Trap SnareTrap presence offset = -8.0", snare.spectralOffset[4], -8.0f, 0.001f);
    TEST_NEAR("Trap SnareTrap air offset = -6.0", snare.spectralOffset[5], -6.0f, 0.001f);

    // Trap HiHat: air offset -8.0 + 2.0 = -6.0
    auto hihat = getExpectedProfile(TrackRole::HiHat, "trap");
    TEST_NEAR("Trap HiHat air offset = -6.0", hihat.spectralOffset[5], -6.0f, 0.001f);
}

// ─── 13. getExpectedProfile — Rock aplica deltas correctamente ──────────
static void testExpectedProfileRock()
{
    std::printf("\n── Test 13: getExpectedProfile — Rock profile deltas applied ──\n");

    // Rock Kick: peak -6.0 + 2.0 = -4.0, crest 14.0 + -2.0 = 12.0
    auto kick = getExpectedProfile(TrackRole::Kick, "rock");
    TEST_NEAR("Rock Kick peakTarget = -4.0", kick.peakTargetDb, -4.0f, 0.001f);
    TEST_NEAR("Rock Kick crestTarget = 12.0", kick.crestTargetDb, 12.0f, 0.001f);

    // Rock GuitarElectric: hiMid offset -8.0 + -3.0 = -11.0, pres offset -15.0 + -3.0 = -18.0
    auto gtr = getExpectedProfile(TrackRole::GuitarElectric, "rock");
    TEST_NEAR("Rock GuitarElectric hiMid offset = -11.0", gtr.spectralOffset[3], -11.0f, 0.001f);
    TEST_NEAR("Rock GuitarElectric pres offset = -18.0", gtr.spectralOffset[4], -18.0f, 0.001f);

    // Rock VozPrincipal: crest 10.0 + 2.0 = 12.0
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal, "rock");
    TEST_NEAR("Rock VozPrincipal crestTarget = 12.0", vocal.crestTargetDb, 12.0f, 0.001f);
}

// ─── 14. getExpectedProfile — Reggaeton aplica deltas correctamente ─────
static void testExpectedProfileReggaeton()
{
    std::printf("\n── Test 14: getExpectedProfile — Reggaeton profile deltas applied ──\n");

    // Reggaeton Bass808: peak -6.0 + 2.0 = -4.0, sub offset -6.0 + -3.0 = -9.0
    auto bass = getExpectedProfile(TrackRole::Bass808, "reggaeton");
    TEST_NEAR("Reggaeton Bass808 peakTarget = -4.0", bass.peakTargetDb, -4.0f, 0.001f);
    TEST_NEAR("Reggaeton Bass808 sub offset = -9.0", bass.spectralOffset[0], -9.0f, 0.001f);

    // Reggaeton BassSub: peak -8.0 + 2.0 = -6.0, sub offset -6.0 + -4.0 = -10.0
    auto bassSub = getExpectedProfile(TrackRole::BassSub, "reggaeton");
    TEST_NEAR("Reggaeton BassSub peakTarget = -6.0", bassSub.peakTargetDb, -6.0f, 0.001f);
    TEST_NEAR("Reggaeton BassSub sub offset = -10.0", bassSub.spectralOffset[0], -10.0f, 0.001f);

    // Reggaeton VozPrincipal: pres offset -10.0 + -2.0 = -12.0
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal, "reggaeton");
    TEST_NEAR("Reggaeton VozPrincipal pres offset = -12.0", vocal.spectralOffset[4], -12.0f, 0.001f);
}

// ─── 15. getExpectedProfile — EDM aplica deltas correctamente ───────────
static void testExpectedProfileEDM()
{
    std::printf("\n── Test 15: getExpectedProfile — EDM profile deltas applied ──\n");

    // EDM BassSub: peak -8.0 + 2.0 = -6.0, sub offset -6.0 + -4.0 = -10.0
    auto bassSub = getExpectedProfile(TrackRole::BassSub, "edm");
    TEST_NEAR("EDM BassSub peakTarget = -6.0", bassSub.peakTargetDb, -6.0f, 0.001f);
    TEST_NEAR("EDM BassSub sub offset = -10.0", bassSub.spectralOffset[0], -10.0f, 0.001f);

    // EDM SynthLead: pres -10.0 + -3.0 = -13.0, air -14.0 + -3.0 = -17.0
    auto synth = getExpectedProfile(TrackRole::SynthLead, "edm");
    TEST_NEAR("EDM SynthLead pres offset = -13.0", synth.spectralOffset[4], -13.0f, 0.001f);
    TEST_NEAR("EDM SynthLead air offset = -17.0", synth.spectralOffset[5], -17.0f, 0.001f);
}

// ─── 16. getExpectedProfile — Pop aplica deltas correctamente ───────────
static void testExpectedProfilePop()
{
    std::printf("\n── Test 16: getExpectedProfile — Pop profile deltas applied ──\n");

    // Pop VozPrincipal: crest 10.0 + -2.0 = 8.0, pres offset -10.0 + -3.0 = -13.0
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal, "pop");
    TEST_NEAR("Pop VozPrincipal crestTarget = 8.0", vocal.crestTargetDb, 8.0f, 0.001f);
    TEST_NEAR("Pop VozPrincipal pres offset = -13.0", vocal.spectralOffset[4], -13.0f, 0.001f);

    // Pop VozFondo: peak -10.0 + 2.0 = -8.0
    auto backing = getExpectedProfile(TrackRole::VozFondo, "pop");
    TEST_NEAR("Pop VozFondo peakTarget = -8.0", backing.peakTargetDb, -8.0f, 0.001f);
}

// ─── 17. getExpectedProfile — Afrobeat aplica deltas correctamente ──────
static void testExpectedProfileAfrobeat()
{
    std::printf("\n── Test 17: getExpectedProfile — Afrobeat profile deltas applied ──\n");

    // Afrobeat Kick: peak -6.0 + 2.0 = -4.0
    auto kick = getExpectedProfile(TrackRole::Kick, "afrobeat");
    TEST_NEAR("Afrobeat Kick peakTarget = -4.0", kick.peakTargetDb, -4.0f, 0.001f);

    // Afrobeat BassSub: bass offset -10.0 + -2.0 = -12.0
    auto bassSub = getExpectedProfile(TrackRole::BassSub, "afrobeat");
    TEST_NEAR("Afrobeat BassSub bass offset = -12.0", bassSub.spectralOffset[1], -12.0f, 0.001f);

    // Afrobeat VozPrincipal: air offset -18.0 + -2.0 = -20.0
    auto vocal = getExpectedProfile(TrackRole::VozPrincipal, "afrobeat");
    TEST_NEAR("Afrobeat VozPrincipal air offset = -20.0", vocal.spectralOffset[5], -20.0f, 0.001f);

    // Afrobeat Percussion: base pres offset -12.0 + delta -2.0 = -14.0
    auto perc = getExpectedProfile(TrackRole::Percussion, "afrobeat");
    TEST_NEAR("Afrobeat Percussion pres offset = -14.0", perc.spectralOffset[4], -14.0f, 0.001f);
}

// ─── 18. Rol sin override con género existente = base ───────────────────
static void testExpectedProfileNoOverride()
{
    std::printf("\n── Test 16: getExpectedProfile — Genre exists, role has no override ──\n");

    // Pop doesn't have overrides for Kick → should return base
    auto base = getExpectedProfile(TrackRole::Kick);
    auto pop = getExpectedProfile(TrackRole::Kick, "pop");
    TEST_NEAR("Pop Kick peak = base", pop.peakTargetDb, base.peakTargetDb, 0.001f);
    TEST_NEAR("Pop Kick crest = base", pop.crestTargetDb, base.crestTargetDb, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Cross-gender consistency (mismos resultados para alias de género)
// ═══════════════════════════════════════════════════════════════════════════

// ─── 19. Alias de género: "edm" == "electronic" ────────────────────────
static void testGenreAliases()
{
    std::printf("\n── Test 19: Genre aliases consistency ──\n");

    auto edmKick = getGenreProfileDelta("edm", TrackRole::Kick);
    auto electronicKick = getGenreProfileDelta("electronic", TrackRole::Kick);
    TEST_NEAR("EDM vs Electronic Kick peakDelta", edmKick.peakDeltaDb, electronicKick.peakDeltaDb, 0.001f);
    TEST_NEAR("EDM vs Electronic Kick crestDelta", edmKick.crestDeltaDb, electronicKick.crestDeltaDb, 0.001f);
    TEST_NEAR("EDM vs Electronic Kick subDelta", edmKick.subDelta, electronicKick.subDelta, 0.001f);

    // "afrobeat" == "afrobeats"
    auto afro1 = getGenreProfileDelta("afrobeat", TrackRole::Kick);
    auto afro2 = getGenreProfileDelta("afrobeats", TrackRole::Kick);
    TEST_NEAR("Afrobeat vs Afrobeats Kick peakDelta", afro1.peakDeltaDb, afro2.peakDeltaDb, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: jlimit boundaries (clamping en getExpectedProfile)
// ═══════════════════════════════════════════════════════════════════════════

// ─── 20. Clamping: valores extremos no rompen los límites ───────────────
static void testClampingBoundaries()
{
    std::printf("\n── Test 20: jlimit clamping boundaries ──\n");

    // peakTargetDb limitado a [-24, 0]
    auto bassSub = getExpectedProfile(TrackRole::BassSub, "edm"); // peak -8 + 2 = -6 (OK, dentro del rango)
    TEST("BassSub EDM peakTarget >= -24", bassSub.peakTargetDb >= -24.0f);
    TEST("BassSub EDM peakTarget <= 0", bassSub.peakTargetDb <= 0.0f);

    // spectralOffset[0] limitado a [-60, 0]
    TEST("BassSub EDM sub offset >= -60", bassSub.spectralOffset[0] >= -60.0f);
    TEST("BassSub EDM sub offset <= 0", bassSub.spectralOffset[0] <= 0.0f);

    // crestTargetDb limitado a [2, 24]
    auto hihat = getExpectedProfile(TrackRole::HiHat, "trap"); // base 18 + 0 = 18 (OK)
    TEST("HiHat Trap crestTarget >= 2", hihat.crestTargetDb >= 2.0f);
    TEST("HiHat Trap crestTarget <= 24", hihat.crestTargetDb <= 24.0f);

    // Trap Kick: crest 14 + 4 = 18 (OK, dentro de [2, 24])
    auto kick = getExpectedProfile(TrackRole::Kick, "trap");
    TEST("Kick Trap crestTarget >= 2", kick.crestTargetDb >= 2.0f);
    TEST("Kick Trap crestTarget <= 24", kick.crestTargetDb <= 24.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════════════════════╗\n");
    std::printf("║        GenreProfiles — Genre-Aware Profile Tests                ║\n");
    std::printf("╚══════════════════════════════════════════════════════════════════╝\n\n");

    // ─── getGenreProfileDelta tests ──────────────────────────────────
    testDeltaEmptyGenre();
    testDeltaUnknownGenre();
    testDeltaNoOverrideForRole();
    testDeltaTrap();
    testDeltaPop();
    testDeltaRock();
    testDeltaReggaeton();
    testDeltaAfrobeat();
    testDeltaEDM();

    // ─── getExpectedProfile(role, genre) tests ───────────────────────
    testExpectedProfileEmptyGenre();
    testExpectedProfileUnknownGenre();
    testExpectedProfileTrap();
    testExpectedProfileRock();
    testExpectedProfileReggaeton();
    testExpectedProfileEDM();
    testExpectedProfilePop();
    testExpectedProfileAfrobeat();
    testExpectedProfileNoOverride();

    // ─── Cross-gender tests ──────────────────────────────────────────
    testGenreAliases();
    testClampingBoundaries();

    std::printf("\n");
    std::printf("════════════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d PASS, %d FAIL\n", g_pass, g_fail);
    std::printf("════════════════════════════════════════════════════════════════\n\n");

    return g_fail > 0 ? 1 : 0;
}

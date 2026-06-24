#pragma once
#include <juce_core/juce_core.h>
#include "../../Common/types/Types.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackRole — Lo que la pista REPRESENTA en la mezcla
    //  Cada rol tiene expectativas de audio específicas (peak, crest, espectro)
    //  que el SemanticComparator usa para producir análisis semánticos.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class TrackRole : uint8_t
    {
        // ─── Batería ─────────────────────────────────────────────────
        Kick,          // Bombo: ataque grave, fundamental 50-100Hz
        Kick808,       // Bombo tipo 808: decay largo, sub 40-60Hz
        Snare,         // Redoblante: ataque 200Hz, cuerpo 400Hz, presencia 5kHz
        SnareTrap,     // Snare tipo trap: agudo, seco, con click 10kHz
        HiHat,         // Hi-hat: agudo 8-12kHz, corto
        HiHatOpen,     // Open hi-hat: decay más largo
        Ride,          // Ride: brillante 12-15kHz
        Crash,         // Crash: explosivo, ancho espectro
        Tom,           // Tom: medios-graves 100-300Hz
        TomFloor,      // Tom de piso: graves 80-200Hz
        Percussion,    // Percusión genérica (clap, shaker, etc.)
        Clap,          // Clap: 1-3kHz, ruidoso
        ReggaetonKick, // Reggaeton kick: punchy, mid-sub, crest alto
        DrumBus,       // Bus de batería (mezcla de todo)
        DrumRoom,      // Room mic: ambiente de batería, stereo amplio

        // ─── Bajo ────────────────────────────────────────────────────
        BassSub,    // Sub-bajo: 40-80Hz, puro seno
        Bass808,    // 808: 50-60Hz fundamental, armónicos hasta 200Hz
        BassPick,   // Bajo con púa: ataque 2-5kHz
        BassFinger, // Bajo con dedo: redondo, 80-200Hz
        BassSynth,  // Bajo sintético: varía mucho
        BassBus,    // Bus de bajo

        // ─── Guitarras ───────────────────────────────────────────────
        GuitarAcoustic, // Guitarra acústica: medios 1-5kHz
        GuitarElectric, // Guitarra eléctrica: medios 500Hz-2kHz
        GuitarRhythm,   // Guitarra rítmica: cuerpo 200-800Hz
        GuitarLead,     // Guitarra líder: presencia 2-5kHz
        GuitarBus,      // Bus de guitarras

        // ─── Teclados / Sintetizadores ──────────────────────────────
        SynthLead,    // Synth líder: presencia 2-4kHz
        SynthPad,     // Pad: sostenido, ancho, medios-graves
        SynthPluck,   // Pluck: ataque rápido, decay medio
        KeysPiano,    // Piano acústico: 80Hz-5kHz
        KeysElectric, // Piano eléctrico (Rhodes, Wurlitzer): 100Hz-3kHz
        KeysOrgan,    // Órgano: sostenido, 50Hz-5kHz
        KeysBus,      // Bus de teclados

        // ─── Voces ───────────────────────────────────────────────────
        VozPrincipal, // Voz principal: presencia 2-4kHz, centrada
        VozFondo,     // Coros: más abiertos, 3-6kHz
        VozDouble,    // Doblaje: stereo, complementa
        Adlibs,       // Ad-libs: efecto, espacio stereo
        VozBus,       // Bus de voces

        // ─── FX y Ambientales ─────────────────────────────────────────
        FxRiser,    // Riser: sube en frecuencia
        FxImpact,   // Impact: golpe grave + ruido
        FxAmbience, // Ambiente: colchón, ancho
        FxNoise,    // Ruido (viento, vinyl, etc.)

        // ─── Melódicos ───────────────────────────────────────────────
        Strings,   // Cuerdas: sostenido, 200Hz-3kHz
        Brass,     // Bronces: presencia 2-4kHz
        Winds,     // Vientos: medios 500Hz-2kHz
        MelodyBus, // Bus melódico

        // ─── Especial ────────────────────────────────────────────────
        Master,        // Master bus
        Unknown = 0xFF // No especificado / desconocido
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  RoleCategory — Grupo funcional (para análisis de bus resumido)
    // ═══════════════════════════════════════════════════════════════════════════
    enum class RoleCategory : uint8_t
    {
        Drums,
        Bass,
        Guitars,
        Keys,
        Vocals,
        FX,
        Melody,
        Other
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ExpectedProfile — Qué valores debería tener una pista de este rol
    //  en un género musical dado. Estos son TARGETS de ingeniería de audio.
    // ═══════════════════════════════════════════════════════════════════════════
    struct ExpectedProfile
    {
        // ─── Identidad ────────────────────────────────────────────────
        TrackRole role          = TrackRole::Unknown;
        const char* name        = "Unknown";
        const char* description = "";

        // ─── Nivel y dinámica ─────────────────────────────────────────
        float peakTargetDb   = -8.0f; // Peak objetivo (dBFS)
        float peakTolerance  = 4.0f;  // ± dB de tolerancia en peak
        float crestTargetDb  = 10.0f; // Crest factor objetivo (peak - RMS)
        float crestTolerance = 6.0f;  // ± dB de tolerancia en crest

        // ─── Espectro: dB offset esperado por banda (relativo al peak) ─
        // Bands: Sub(0), Bass(1), LoMid(2), HiMid(3), Pres(4), Air(5)
        float spectralOffset[6] = {-20.0f, -15.0f, -10.0f, -10.0f, -15.0f, -25.0f};

        // ─── Estéreo: ancho máximo esperado por banda (0-1) ──────────
        float stereoWidthMax[6] = {0.15f, 0.2f, 0.4f, 0.5f, 0.5f, 0.6f};

        // ─── Transientes ──────────────────────────────────────────────
        float transientRatioMin    = 0.0f; // Mínimo esperado
        float transientRatioMax    = 5.0f; // Máximo esperado
        float transientRatioTarget = 1.5f;

        // ─── Frecuencia fundamental estimada ──────────────────────────
        float fundamentalMin = 0.0f;     // Hz
        float fundamentalMax = 20000.0f; // Hz
        float centroidMin    = 0.0f;     // Hz (centroide espectral)
        float centroidMax    = 20000.0f;

        // ─── Helpers ──────────────────────────────────────────────────
        [[nodiscard]] bool isPeakInRange(float peakDb) const noexcept
        {
            return peakDb >= (peakTargetDb - peakTolerance) && peakDb <= (peakTargetDb + peakTolerance);
        }

        [[nodiscard]] bool isCrestInRange(float crestDb) const noexcept
        {
            return crestDb >= (crestTargetDb - crestTolerance) && crestDb <= (crestTargetDb + crestTolerance);
        }

        [[nodiscard]] float getPeakDeviation(float peakDb) const noexcept { return peakDb - peakTargetDb; }

        [[nodiscard]] float getCrestDeviation(float crestDb) const noexcept { return crestDb - crestTargetDb; }

        [[nodiscard]] bool isWidthInBandOk(int band, float width) const noexcept
        {
            if (band < 0 || band >= 6) return true;
            return width <= stereoWidthMax[band];
        }
    };

// ═══════════════════════════════════════════════════════════════════════════
//  EXPECTED PROFILE DATABASE — Conocimiento de ingeniería de audio
//  Cada rol tiene targets ajustados por experiencia en mezcla profesional.
//  Los valores representan lo que un ingeniero esperaría en una mezcla
//  equilibrada con headroom adecuado.
// ═══════════════════════════════════════════════════════════════════════════

// Helper para construir perfiles más fácilmente
#define PROFILE(role,                                                                          \
                name,                                                                          \
                desc,                                                                          \
                peak,                                                                          \
                peakTol,                                                                       \
                crest,                                                                         \
                crestTol,                                                                      \
                s0,                                                                            \
                s1,                                                                            \
                s2,                                                                            \
                s3,                                                                            \
                s4,                                                                            \
                s5,                                                                            \
                w0,                                                                            \
                w1,                                                                            \
                w2,                                                                            \
                w3,                                                                            \
                w4,                                                                            \
                w5,                                                                            \
                trMin,                                                                         \
                trMax,                                                                         \
                trTarget,                                                                      \
                fMin,                                                                          \
                fMax,                                                                          \
                cMin,                                                                          \
                cMax)                                                                          \
    ExpectedProfile                                                                            \
    {                                                                                          \
        TrackRole::role, name, desc, peak, peakTol, crest, crestTol, {s0, s1, s2, s3, s4, s5}, \
            {w0, w1, w2, w3, w4, w5}, trMin, trMax, trTarget, fMin, fMax, cMin, cMax           \
    }

    // Retorna el perfil esperado para un rol dado
    inline ExpectedProfile getExpectedProfile(TrackRole role) noexcept
    {
        switch (role) {
            // ═══ BATERÍA ═══════════════════════════════════════════════════
            case TrackRole::Kick:
                // Bombo: ataque grave, fundamental 50-100Hz, crest medio-alto
                return PROFILE(Kick,
                               "Kick",
                               "Bombo: ataque grave 50-100Hz",
                               -6.0f,
                               4.0f,
                               14.0f,
                               6.0f, // peak -6dB, crest 14dB
                               -8.0f,
                               -10.0f,
                               -20.0f,
                               -30.0f,
                               -40.0f,
                               -50.0f,
                               0.1f,
                               0.1f,
                               0.2f,
                               0.2f,
                               0.2f,
                               0.2f, // stereo: casi mono
                               0.5f,
                               5.0f,
                               2.0f,
                               50.0f,
                               100.0f,
                               80.0f,
                               200.0f);

            case TrackRole::Kick808:
                // 808 Kick: sub grave, decay largo, crest alto (ataque → sostenido)
                return PROFILE(Kick808,
                               "808 Kick",
                               "808 Kick: sub 40-60Hz, decay largo",
                               -5.0f,
                               4.0f,
                               16.0f,
                               8.0f,
                               -6.0f,
                               -10.0f,
                               -25.0f,
                               -35.0f,
                               -45.0f,
                               -55.0f,
                               0.05f,
                               0.05f,
                               0.1f,
                               0.1f,
                               0.1f,
                               0.1f, // cuasi-mono
                               1.0f,
                               6.0f,
                               2.5f,
                               40.0f,
                               60.0f,
                               40.0f,
                               80.0f);

            case TrackRole::Snare:
                // Snare: ataque 200Hz, cuerpo 400Hz, presencia 5kHz
                return PROFILE(Snare,
                               "Snare",
                               "Redoblante: cuerpo 400Hz, presencia 5kHz",
                               -8.0f,
                               4.0f,
                               16.0f,
                               6.0f,
                               -20.0f,
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -12.0f,
                               -20.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.3f,
                               0.3f,
                               0.3f,
                               0.5f,
                               4.0f,
                               2.0f,
                               150.0f,
                               400.0f,
                               300.0f,
                               800.0f);

            case TrackRole::SnareTrap:
                // Snare trap: agudo, seco, con click 10kHz
                return PROFILE(SnareTrap,
                               "Snare Trap",
                               "Snare trap: agudo seco 10kHz",
                               -6.0f,
                               4.0f,
                               18.0f,
                               6.0f,
                               -30.0f,
                               -25.0f,
                               -18.0f,
                               -12.0f,
                               -10.0f,
                               -8.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.5f,
                               0.5f,
                               0.5f,
                               1.0f,
                               5.0f,
                               3.0f,
                               200.0f,
                               500.0f,
                               500.0f,
                               2000.0f);

            case TrackRole::HiHat:
                // Hi-hat: agudo 8-12kHz, corto, crest alto
                return PROFILE(HiHat,
                               "HiHat",
                               "Hi-hat: agudo 8-12kHz, corto",
                               -12.0f,
                               6.0f,
                               18.0f,
                               6.0f,
                               -40.0f,
                               -35.0f,
                               -25.0f,
                               -15.0f,
                               -10.0f,
                               -8.0f,
                               0.4f,
                               0.4f,
                               0.6f,
                               0.6f,
                               0.7f,
                               0.7f, // puede ser stereo
                               1.0f,
                               6.0f,
                               3.0f,
                               5000.0f,
                               12000.0f,
                               8000.0f,
                               12000.0f);

            case TrackRole::HiHatOpen:
                return PROFILE(HiHatOpen,
                               "Open HH",
                               "Open hi-hat: decay largo",
                               -10.0f,
                               6.0f,
                               16.0f,
                               6.0f,
                               -35.0f,
                               -30.0f,
                               -20.0f,
                               -12.0f,
                               -10.0f,
                               -8.0f,
                               0.5f,
                               0.5f,
                               0.6f,
                               0.7f,
                               0.7f,
                               0.7f,
                               1.0f,
                               5.0f,
                               2.5f,
                               5000.0f,
                               12000.0f,
                               6000.0f,
                               10000.0f);

            case TrackRole::Tom:
                // Tom: medios-graves 100-300Hz
                return PROFILE(Tom,
                               "Tom",
                               "Tom: medios-graves 100-300Hz",
                               -10.0f,
                               4.0f,
                               14.0f,
                               6.0f,
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -15.0f,
                               -25.0f,
                               -35.0f,
                               0.1f,
                               0.1f,
                               0.2f,
                               0.2f,
                               0.2f,
                               0.2f,
                               0.5f,
                               4.0f,
                               2.0f,
                               80.0f,
                               300.0f,
                               100.0f,
                               400.0f);

            case TrackRole::Percussion:
            case TrackRole::Clap:
                // Percusión genérica: medios-altos
                return PROFILE(Percussion,
                               "Percusion",
                               "Percusión: medios-altos",
                               -10.0f,
                               6.0f,
                               16.0f,
                               6.0f,
                               -25.0f,
                               -20.0f,
                               -15.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.5f,
                               0.5f,
                               0.5f,
                               0.5f,
                               5.0f,
                               2.5f,
                               200.0f,
                               3000.0f,
                               500.0f,
                               3000.0f);

            case TrackRole::ReggaetonKick:
                // Reggaeton Kick: punchy, corto, mid-sub prominence, crest alto
                return PROFILE(ReggaetonKick,
                               "Reggaeton Kick",
                               "Reggaeton kick: punchy, mid-sub 60-100Hz, crest alto",
                               -5.0f,
                               4.0f,
                               15.0f,
                               6.0f, // peak -5dB, crest 15dB
                               -7.0f,
                               -9.0f,
                               -22.0f,
                               -35.0f,
                               -45.0f,
                               -55.0f,
                               0.05f,
                               0.05f,
                               0.1f,
                               0.1f,
                               0.1f,
                               0.1f, // casi mono
                               1.0f,
                               6.0f,
                               3.0f,
                               40.0f,
                               100.0f,
                               50.0f,
                               150.0f);

            case TrackRole::DrumBus:
                return PROFILE(DrumBus,
                               "Drum Bus",
                               "Bus de batería: mezcla completa",
                               -4.0f,
                               4.0f,
                               12.0f,
                               6.0f,
                               -10.0f,
                               -8.0f,
                               -12.0f,
                               -15.0f,
                               -20.0f,
                               -25.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               1.0f,
                               5.0f,
                               2.0f,
                               40.0f,
                               100.0f,
                               60.0f,
                               500.0f);

            case TrackRole::DrumRoom:
                return PROFILE(DrumRoom,
                               "Drum Room",
                               "Room mic: ambiente de batería, stereo amplio",
                               -18.0f,
                               8.0f,
                               8.0f,
                               6.0f,
                               -12.0f,
                               -10.0f,
                               -14.0f,
                               -18.0f,
                               -22.0f,
                               -28.0f,
                               0.3f,
                               0.3f,
                               0.5f,
                               0.6f,
                               0.7f,
                               0.8f, // stereo amplio
                               0.5f,
                               4.0f,
                               1.5f,
                               60.0f,
                               500.0f,
                               100.0f,
                               2000.0f);

            // ═══ BAJOS ════════════════════════════════════════════════════
            case TrackRole::BassSub:
                // Sub-bajo: 40-80Hz, casi mono, crest bajo (sostenido)
                return PROFILE(BassSub,
                               "Sub Bass",
                               "Sub-bajo: 40-80Hz, sostenido",
                               -8.0f,
                               4.0f,
                               6.0f,
                               4.0f, // crest bajo = sostenido
                               -6.0f,
                               -10.0f,
                               -25.0f,
                               -40.0f,
                               -50.0f,
                               -60.0f,
                               0.05f,
                               0.05f,
                               0.1f,
                               0.1f,
                               0.1f,
                               0.1f, // DEBE ser mono
                               0.0f,
                               2.0f,
                               1.0f,
                               30.0f,
                               80.0f,
                               30.0f,
                               80.0f);

            case TrackRole::Bass808:
                // 808: fundamental 50-60Hz, armónicos hasta 200Hz, crest medio
                return PROFILE(Bass808,
                               "808",
                               "808: fundamental 50-60Hz, armónicos 200Hz",
                               -6.0f,
                               4.0f,
                               10.0f,
                               6.0f,
                               -6.0f,
                               -8.0f,
                               -18.0f,
                               -30.0f,
                               -40.0f,
                               -50.0f,
                               0.05f,
                               0.05f,
                               0.1f,
                               0.1f,
                               0.15f,
                               0.15f,
                               0.0f,
                               3.0f,
                               1.5f,
                               40.0f,
                               70.0f,
                               40.0f,
                               100.0f);

            case TrackRole::BassPick:
                // Bajo con púa: ataque 2-5kHz, crest medio-alto
                return PROFILE(BassPick,
                               "Bass Pick",
                               "Bajo con púa: ataque 2-5kHz",
                               -8.0f,
                               4.0f,
                               12.0f,
                               6.0f,
                               -10.0f,
                               -8.0f,
                               -12.0f,
                               -18.0f,
                               -22.0f,
                               -30.0f,
                               0.1f,
                               0.1f,
                               0.15f,
                               0.2f,
                               0.2f,
                               0.2f,
                               0.5f,
                               4.0f,
                               2.0f,
                               60.0f,
                               120.0f,
                               80.0f,
                               500.0f);

            case TrackRole::BassFinger:
                // Bajo con dedo: redondo, 80-200Hz
                return PROFILE(BassFinger,
                               "Bass Finger",
                               "Bajo con dedo: redondo 80-200Hz",
                               -8.0f,
                               4.0f,
                               10.0f,
                               6.0f,
                               -12.0f,
                               -8.0f,
                               -10.0f,
                               -20.0f,
                               -28.0f,
                               -35.0f,
                               0.1f,
                               0.1f,
                               0.15f,
                               0.15f,
                               0.2f,
                               0.2f,
                               0.3f,
                               3.5f,
                               1.8f,
                               60.0f,
                               200.0f,
                               80.0f,
                               400.0f);

            case TrackRole::BassBus:
                return PROFILE(BassBus,
                               "Bass Bus",
                               "Bus de bajo: grupo completo",
                               -4.0f,
                               4.0f,
                               8.0f,
                               6.0f,
                               -8.0f,
                               -6.0f,
                               -15.0f,
                               -25.0f,
                               -35.0f,
                               -45.0f,
                               0.1f,
                               0.1f,
                               0.15f,
                               0.2f,
                               0.2f,
                               0.2f,
                               0.0f,
                               3.0f,
                               1.5f,
                               30.0f,
                               200.0f,
                               40.0f,
                               300.0f);

            // ═══ VOCES ════════════════════════════════════════════════════
            case TrackRole::VozPrincipal:
                // Voz principal: presencia 2-4kHz, centrada, crest moderado
                return PROFILE(VozPrincipal,
                               "Vocal",
                               "Voz principal: presencia 2-4kHz, centrada",
                               -6.0f,
                               4.0f,
                               10.0f,
                               6.0f,
                               -25.0f,
                               -18.0f,
                               -12.0f,
                               -8.0f,
                               -10.0f,
                               -18.0f,
                               0.05f,
                               0.05f,
                               0.1f,
                               0.1f,
                               0.15f,
                               0.2f, // centrada
                               0.3f,
                               4.0f,
                               2.0f,
                               80.0f,
                               500.0f,
                               300.0f,
                               2000.0f);

            case TrackRole::VozFondo:
                // Coros: más abiertos, 3-6kHz
                return PROFILE(VozFondo,
                               "Backing Vocal",
                               "Coros: más abiertos 3-6kHz",
                               -10.0f,
                               6.0f,
                               10.0f,
                               6.0f,
                               -30.0f,
                               -22.0f,
                               -15.0f,
                               -10.0f,
                               -10.0f,
                               -15.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.5f,
                               0.5f,
                               0.5f, // más stereo
                               0.3f,
                               4.0f,
                               2.0f,
                               100.0f,
                               800.0f,
                               400.0f,
                               3000.0f);

            case TrackRole::Adlibs:
                return PROFILE(Adlibs,
                               "Adlibs",
                               "Ad-libs: efecto, espacio stereo",
                               -14.0f,
                               6.0f,
                               12.0f,
                               6.0f,
                               -30.0f,
                               -22.0f,
                               -15.0f,
                               -10.0f,
                               -10.0f,
                               -15.0f,
                               0.5f,
                               0.5f,
                               0.6f,
                               0.7f,
                               0.7f,
                               0.7f, // stereo amplio
                               0.5f,
                               5.0f,
                               2.5f,
                               100.0f,
                               1000.0f,
                               500.0f,
                               4000.0f);

            case TrackRole::VozBus:
                return PROFILE(VozBus,
                               "Vocal Bus",
                               "Bus de voces: grupo completo",
                               -4.0f,
                               4.0f,
                               8.0f,
                               6.0f,
                               -22.0f,
                               -16.0f,
                               -10.0f,
                               -8.0f,
                               -12.0f,
                               -20.0f,
                               0.1f,
                               0.1f,
                               0.2f,
                               0.3f,
                               0.3f,
                               0.3f,
                               0.3f,
                               4.0f,
                               2.0f,
                               80.0f,
                               600.0f,
                               300.0f,
                               2000.0f);

            // ═══ GUITARRAS ═══════════════════════════════════════════════
            case TrackRole::GuitarAcoustic:
                return PROFILE(GuitarAcoustic,
                               "Acoustic Guitar",
                               "Guitarra acústica: medios 1-5kHz",
                               -10.0f,
                               4.0f,
                               12.0f,
                               6.0f,
                               -18.0f,
                               -15.0f,
                               -10.0f,
                               -8.0f,
                               -12.0f,
                               -18.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.5f,
                               4.0f,
                               2.0f,
                               80.0f,
                               500.0f,
                               200.0f,
                               2000.0f);

            case TrackRole::GuitarElectric:
                return PROFILE(GuitarElectric,
                               "Electric Guitar",
                               "Guitarra eléctrica: medios 500Hz-2kHz",
                               -8.0f,
                               4.0f,
                               12.0f,
                               6.0f,
                               -18.0f,
                               -15.0f,
                               -10.0f,
                               -8.0f,
                               -15.0f,
                               -22.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.5f,
                               4.0f,
                               2.0f,
                               80.0f,
                               300.0f,
                               200.0f,
                               1000.0f);

            case TrackRole::GuitarLead:
                return PROFILE(GuitarLead,
                               "Lead Guitar",
                               "Guitarra líder: presencia 2-5kHz",
                               -8.0f,
                               4.0f,
                               14.0f,
                               6.0f,
                               -22.0f,
                               -18.0f,
                               -12.0f,
                               -8.0f,
                               -10.0f,
                               -15.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.5f,
                               5.0f,
                               2.5f,
                               100.0f,
                               400.0f,
                               300.0f,
                               2000.0f);

            case TrackRole::GuitarBus:
                return PROFILE(GuitarBus,
                               "Guitar Bus",
                               "Bus de guitarras: grupo",
                               -4.0f,
                               4.0f,
                               10.0f,
                               6.0f,
                               -15.0f,
                               -12.0f,
                               -8.0f,
                               -10.0f,
                               -15.0f,
                               -22.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.5f,
                               4.0f,
                               2.0f,
                               80.0f,
                               400.0f,
                               200.0f,
                               1500.0f);

            // ═══ TECLADOS ═════════════════════════════════════════════════
            case TrackRole::SynthPad:
                // Pad: sostenido, crest bajo, stereo amplio
                return PROFILE(SynthPad,
                               "Synth Pad",
                               "Pad: sostenido, ancho, crest bajo",
                               -12.0f,
                               6.0f,
                               6.0f,
                               4.0f, // crest bajo = sostenido
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.3f,
                               0.3f,
                               0.5f,
                               0.6f,
                               0.6f,
                               0.6f,
                               0.0f,
                               2.0f,
                               1.0f,
                               50.0f,
                               500.0f,
                               100.0f,
                               2000.0f);

            case TrackRole::SynthLead:
                return PROFILE(SynthLead,
                               "Synth Lead",
                               "Synth líder: presencia 2-4kHz",
                               -8.0f,
                               4.0f,
                               10.0f,
                               6.0f,
                               -20.0f,
                               -16.0f,
                               -12.0f,
                               -8.0f,
                               -10.0f,
                               -14.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.3f,
                               4.0f,
                               2.0f,
                               100.0f,
                               2000.0f,
                               500.0f,
                               3000.0f);

            case TrackRole::SynthPluck:
                return PROFILE(SynthPluck,
                               "Synth Pluck",
                               "Pluck: ataque rápido, decay",
                               -8.0f,
                               4.0f,
                               14.0f,
                               6.0f,
                               -20.0f,
                               -16.0f,
                               -12.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               1.0f,
                               5.0f,
                               3.0f,
                               100.0f,
                               2000.0f,
                               500.0f,
                               4000.0f);

            case TrackRole::KeysPiano:
                return PROFILE(KeysPiano,
                               "Piano",
                               "Piano: 80Hz-5kHz, amplio espectro",
                               -10.0f,
                               4.0f,
                               12.0f,
                               6.0f,
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.5f,
                               0.5f,
                               0.3f,
                               4.0f,
                               2.0f,
                               50.0f,
                               500.0f,
                               100.0f,
                               3000.0f);

            case TrackRole::KeysBus:
                return PROFILE(KeysBus,
                               "Keys Bus",
                               "Bus de teclados: grupo",
                               -4.0f,
                               4.0f,
                               8.0f,
                               6.0f,
                               -14.0f,
                               -12.0f,
                               -10.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.0f,
                               3.0f,
                               1.5f,
                               50.0f,
                               2000.0f,
                               100.0f,
                               3000.0f);

            // ═══ EFECTOS ══════════════════════════════════════════════════
            case TrackRole::FxRiser:
                return PROFILE(FxRiser,
                               "Riser",
                               "Riser: sube en frecuencia, efecto",
                               -16.0f,
                               8.0f,
                               14.0f,
                               8.0f,
                               -20.0f,
                               -18.0f,
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -8.0f,
                               0.5f,
                               0.5f,
                               0.6f,
                               0.7f,
                               0.8f,
                               0.8f,
                               0.5f,
                               5.0f,
                               2.0f,
                               20.0f,
                               10000.0f,
                               500.0f,
                               8000.0f);

            case TrackRole::FxImpact:
                return PROFILE(FxImpact,
                               "Impact",
                               "Impact: golpe + ruido, efecto",
                               -10.0f,
                               6.0f,
                               16.0f,
                               8.0f,
                               -12.0f,
                               -10.0f,
                               -12.0f,
                               -12.0f,
                               -15.0f,
                               -20.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.5f,
                               0.5f,
                               0.5f,
                               1.0f,
                               6.0f,
                               3.0f,
                               20.0f,
                               200.0f,
                               30.0f,
                               500.0f);

            case TrackRole::FxAmbience:
                return PROFILE(FxAmbience,
                               "Ambience",
                               "Ambiente: colchón, ancho, bajo",
                               -20.0f,
                               10.0f,
                               8.0f,
                               6.0f,
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -12.0f,
                               -15.0f,
                               -20.0f,
                               0.5f,
                               0.5f,
                               0.6f,
                               0.7f,
                               0.8f,
                               0.8f,
                               0.0f,
                               3.0f,
                               1.0f,
                               20.0f,
                               200.0f,
                               50.0f,
                               500.0f);

            // ═══ MELÓDICOS ════════════════════════════════════════════════
            case TrackRole::Strings:
                return PROFILE(Strings,
                               "Strings",
                               "Cuerdas: sostenido 200Hz-3kHz",
                               -12.0f,
                               6.0f,
                               8.0f,
                               4.0f,
                               -18.0f,
                               -15.0f,
                               -10.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.3f,
                               0.3f,
                               0.4f,
                               0.5f,
                               0.5f,
                               0.5f,
                               0.0f,
                               3.0f,
                               1.5f,
                               100.0f,
                               1000.0f,
                               200.0f,
                               3000.0f);

            case TrackRole::Brass:
                return PROFILE(Brass,
                               "Brass",
                               "Bronces: presencia 2-4kHz",
                               -10.0f,
                               4.0f,
                               12.0f,
                               6.0f,
                               -22.0f,
                               -18.0f,
                               -12.0f,
                               -8.0f,
                               -10.0f,
                               -14.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.3f,
                               4.0f,
                               2.0f,
                               100.0f,
                               800.0f,
                               300.0f,
                               3000.0f);

            // ═══ DEFAULT / UNKNONW ════════════════════════════════════════
            default:
                return PROFILE(Unknown,
                               "Unknown",
                               "Rol no especificado",
                               -10.0f,
                               8.0f,
                               10.0f,
                               8.0f,
                               -20.0f,
                               -15.0f,
                               -12.0f,
                               -10.0f,
                               -12.0f,
                               -18.0f,
                               0.2f,
                               0.2f,
                               0.3f,
                               0.4f,
                               0.4f,
                               0.4f,
                               0.0f,
                               5.0f,
                               2.0f,
                               20.0f,
                               500.0f,
                               50.0f,
                               3000.0f);
        }
    }

#undef PROFILE

// ═══════════════════════════════════════════════════════════════════════════
//  GENRE-AWARE PROFILE OVERRIDES (Fase B)
//  Cada género tiene ajustes de target sobre la base (ExpectedProfile).
//  Los deltas se aplican solo a los roles más relevantes por género.
//  Para roles no especificados, se usa el perfil base sin cambios.
// ═══════════════════════════════════════════════════════════════════════════

/** Delta de ajuste sobre un ExpectedProfile base. */
struct GenreProfileDelta
{
    TrackRole role = TrackRole::Unknown;
    float peakDeltaDb   = 0.0f; // Ajuste al peak target (dB)
    float crestDeltaDb  = 0.0f; // Ajuste al crest target (dB)
    float subDelta      = 0.0f; // Ajuste al spectralOffset[0]
    float bassDelta     = 0.0f; // Ajuste al spectralOffset[1]
    float loMidDelta    = 0.0f; // Ajuste al spectralOffset[2]
    float hiMidDelta    = 0.0f; // Ajuste al spectralOffset[3]
    float presDelta     = 0.0f; // Ajuste al spectralOffset[4]
    float airDelta      = 0.0f; // Ajuste al spectralOffset[5]
};

/** Retorna los deltas de perfil para un género y rol específicos.
    Si el género no tiene overrides para el rol, retorna un delta vacío (0.0f). */
inline GenreProfileDelta getGenreProfileDelta(const juce::String& genre, TrackRole role) noexcept
{
    juce::String g = genre.trim().toLowerCase();

    // ═══════════════════════════════════════════════════════════════════
    //  TRAP — 808 dominante, agudos brillantes, batería agresiva
    //  Característica: sub-graves +8dB, presencia muy abierta, crest alto
    // ═══════════════════════════════════════════════════════════════════
    if (g == "trap") {
        switch (role) {
            case TrackRole::Kick:
                return {role, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // crest +4dB (más agresivo)
            case TrackRole::Kick808:
                return {role, 1.0f, -2.0f, -2.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +1dB, crest -2dB (más sostenido), sub +2dB
            case TrackRole::Bass808:
                return {role, 0.0f, 2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // crest +2dB, sub +2dB
            case TrackRole::SnareTrap:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 2.0f}; // presencia +2dB, aire +2dB
            case TrackRole::HiHat:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f}; // aire +2dB (brillante)
            case TrackRole::VozPrincipal:
                return {role, -2.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f}; // peak -2dB (más ajustada), presencia +2dB
            case TrackRole::BassSub:
                return {role, 2.0f, -2.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB, sub +3dB (más peso)
            case TrackRole::BassSynth:
                return {role, 1.0f, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // sub +2dB
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  POP — Voces cristalinas, mezcla balanceada, crest moderado
    //  Característica: presencia vocal +4dB, compresión suave, ancho stereo
    // ═══════════════════════════════════════════════════════════════════
    if (g == "pop") {
        switch (role) {
            case TrackRole::VozPrincipal:
                return {role, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, -3.0f, -2.0f}; // crest -2dB (más comprimida), presencia +3dB
            case TrackRole::VozFondo:
                return {role, 2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB (más presentes)
            case TrackRole::Kick:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::BassSub:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::BassFinger:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::KeysPiano:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  ROCK — Guitarras presentes, batería potente, crest natural
    //  Característica: guitarras +4dB en medios, rango dinámico amplio
    // ═══════════════════════════════════════════════════════════════════
    if (g == "rock") {
        switch (role) {
            case TrackRole::Kick:
                return {role, 2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB, crest -2dB
            case TrackRole::Snare:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::GuitarElectric:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -3.0f, -3.0f, 0.0f}; // hiMid +3dB, presencia +3dB
            case TrackRole::GuitarLead:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, -2.0f, 0.0f}; // hiMid +2dB, presencia +2dB
            case TrackRole::GuitarRhythm:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -3.0f, 0.0f, 0.0f}; // hiMid +3dB
            case TrackRole::VozPrincipal:
                return {role, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // crest +2dB (menos comprimida, más natural)
            case TrackRole::BassPick:
                return {role, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // crest +2dB (más dinámica)
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  REGGAETON — 808/Bass dominante, bombo punchy, rango dinámico
    //  Característica: sub-bass prominente, bombo mid-sub, presencia vocal
    // ═══════════════════════════════════════════════════════════════════
    if (g == "reggaeton" || g == "reggaeton/latin" || g == "latin" || g == "dembow") {
        switch (role) {
            case TrackRole::Kick:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::ReggaetonKick:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil ya es reagg
            case TrackRole::Bass808:
                return {role, 2.0f, -2.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB, sub +3dB (más presencia del 808)
            case TrackRole::BassSub:
                return {role, 2.0f, -2.0f, -4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB, sub +4dB
            case TrackRole::VozPrincipal:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 0.0f}; // presencia +2dB (voz clara sobre el beat)
            case TrackRole::Snare:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::HiHat:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  AFROBEAT — Percusión presente, bajos bailables, calidez
    //  Característica: percusión +4dB, bajos con cuerpo, voces aire
    // ═══════════════════════════════════════════════════════════════════
    if (g == "afrobeat" || g == "afrobeats" || g == "world") {
        switch (role) {
            case TrackRole::Kick:
                return {role, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB (kick más presente)
            case TrackRole::BassSub:
                return {role, 0.0f, 0.0f, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // bass +2dB (cuerpo extra)
            case TrackRole::BassFinger:
                return {role, 0.0f, 0.0f, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // bass +2dB (cálido)
            case TrackRole::Percussion:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -2.0f, 0.0f}; // presencia +2dB (percusión brillante)
            case TrackRole::Snare:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::VozPrincipal:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -2.0f}; // aire +2dB
            case TrackRole::KeysPiano:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    //  EDM — Sub masivo, crest comprimido, presencia extrema
    //  Característica: sub +6dB, compresión fuerte, presencia +4dB
    // ═══════════════════════════════════════════════════════════════════
    if (g == "edm" || g == "electronic" || g == "house" || g == "techno" || g == "trance" || g == "dubstep") {
        switch (role) {
            case TrackRole::Kick:
                return {role, 2.0f, -2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB, sub +2dB (kick enorme)
            case TrackRole::BassSub:
                return {role, 2.0f, -2.0f, -4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // peak +2dB, sub +4dB (sub masivo)
            case TrackRole::BassSynth:
                return {role, 0.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // sub +3dB
            case TrackRole::SynthLead:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -3.0f, -3.0f}; // presencia +3dB, aire +3dB
            case TrackRole::SynthPad:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::HiHat:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            case TrackRole::Snare:
                return {role, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // perfil base
            default:
                return {};
        }
    }

    // Si no hay coincidencia, retornar delta vacío (sin cambios)
    return {};
}

/** Retorna el ExpectedProfile para un rol en un género específico.
    Aplica los deltas del género sobre el perfil base.
    Si el género no tiene overrides, retorna el perfil base sin cambios.
    @param role  TrackRole a consultar
    @param genre  Género musical (vacío o desconocido = perfil base) */
inline ExpectedProfile getExpectedProfile(TrackRole role, const juce::String& genre) noexcept
{
    ExpectedProfile base = getExpectedProfile(role);

    // Si no hay género, retornar base sin cambios
    if (genre.isEmpty()) return base;

    // Obtener deltas específicos del género para este rol
    GenreProfileDelta delta = getGenreProfileDelta(genre, role);

    // Aplicar deltas (solo si hay cambios — detectamos por rol != Unknown)
    if (delta.role != TrackRole::Unknown) {
        base.peakTargetDb  = juce::jlimit(-24.0f, 0.0f, base.peakTargetDb + delta.peakDeltaDb);
        base.crestTargetDb = juce::jlimit(2.0f, 24.0f, base.crestTargetDb + delta.crestDeltaDb);

        base.spectralOffset[0] = juce::jlimit(-60.0f, 0.0f, base.spectralOffset[0] + delta.subDelta);
        base.spectralOffset[1] = juce::jlimit(-60.0f, 0.0f, base.spectralOffset[1] + delta.bassDelta);
        base.spectralOffset[2] = juce::jlimit(-60.0f, 0.0f, base.spectralOffset[2] + delta.loMidDelta);
        base.spectralOffset[3] = juce::jlimit(-60.0f, 0.0f, base.spectralOffset[3] + delta.hiMidDelta);
        base.spectralOffset[4] = juce::jlimit(-60.0f, 0.0f, base.spectralOffset[4] + delta.presDelta);
        base.spectralOffset[5] = juce::jlimit(-60.0f, 0.0f, base.spectralOffset[5] + delta.airDelta);

        // Ajustar descripción para reflejar que es genre-aware
        // (mantenemos el nombre pero indicamos brevemente)
    }

    return base;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Utility functions
// ═══════════════════════════════════════════════════════════════════════════

    inline BusType getBusForRole(TrackRole role) noexcept
    {
        switch (role) {
            case TrackRole::Kick:
            case TrackRole::Kick808:
            case TrackRole::Snare:
            case TrackRole::SnareTrap:
            case TrackRole::HiHat:
            case TrackRole::HiHatOpen:
            case TrackRole::Ride:
            case TrackRole::Crash:
            case TrackRole::Tom:
            case TrackRole::TomFloor:
            case TrackRole::Percussion:
            case TrackRole::Clap:
            case TrackRole::ReggaetonKick:
            case TrackRole::DrumBus:
            case TrackRole::DrumRoom:
                return BusType::Drums;

            case TrackRole::BassSub:
            case TrackRole::Bass808:
            case TrackRole::BassPick:
            case TrackRole::BassFinger:
            case TrackRole::BassSynth:
            case TrackRole::BassBus:
                return BusType::Bass;

            case TrackRole::GuitarAcoustic:
            case TrackRole::GuitarElectric:
            case TrackRole::GuitarRhythm:
            case TrackRole::GuitarLead:
            case TrackRole::GuitarBus:
                return BusType::Guitars;

            case TrackRole::SynthLead:
            case TrackRole::SynthPad:
            case TrackRole::SynthPluck:
            case TrackRole::KeysPiano:
            case TrackRole::KeysElectric:
            case TrackRole::KeysOrgan:
            case TrackRole::KeysBus:
            case TrackRole::MelodyBus:
                return BusType::Keys;

            case TrackRole::VozPrincipal:
            case TrackRole::VozFondo:
            case TrackRole::VozDouble:
            case TrackRole::Adlibs:
            case TrackRole::VozBus:
                return BusType::Vocals;

            case TrackRole::FxRiser:
            case TrackRole::FxImpact:
            case TrackRole::FxAmbience:
            case TrackRole::FxNoise:
                return BusType::FX;

            case TrackRole::Strings:
            case TrackRole::Brass:
            case TrackRole::Winds:
                return BusType::Melody;

            default:
                return BusType::None;
        }
    }

    inline const char* getRoleName(TrackRole role) noexcept
    {
        return getExpectedProfile(role).name;
    }

    inline RoleCategory getRoleCategory(TrackRole role) noexcept
    {
        switch (role) {
            case TrackRole::Kick:
            case TrackRole::Kick808:
            case TrackRole::Snare:
            case TrackRole::SnareTrap:
            case TrackRole::HiHat:
            case TrackRole::HiHatOpen:
            case TrackRole::Ride:
            case TrackRole::Crash:
            case TrackRole::Tom:
            case TrackRole::TomFloor:
            case TrackRole::Percussion:
            case TrackRole::Clap:
            case TrackRole::ReggaetonKick:
            case TrackRole::DrumBus:
            case TrackRole::DrumRoom:
                return RoleCategory::Drums;

            case TrackRole::BassSub:
            case TrackRole::Bass808:
            case TrackRole::BassPick:
            case TrackRole::BassFinger:
            case TrackRole::BassSynth:
            case TrackRole::BassBus:
                return RoleCategory::Bass;

            case TrackRole::GuitarAcoustic:
            case TrackRole::GuitarElectric:
            case TrackRole::GuitarRhythm:
            case TrackRole::GuitarLead:
            case TrackRole::GuitarBus:
                return RoleCategory::Guitars;

            case TrackRole::SynthLead:
            case TrackRole::SynthPad:
            case TrackRole::SynthPluck:
            case TrackRole::KeysPiano:
            case TrackRole::KeysElectric:
            case TrackRole::KeysOrgan:
            case TrackRole::KeysBus:
            case TrackRole::MelodyBus:
                return RoleCategory::Keys;

            case TrackRole::VozPrincipal:
            case TrackRole::VozFondo:
            case TrackRole::VozDouble:
            case TrackRole::Adlibs:
            case TrackRole::VozBus:
                return RoleCategory::Vocals;

            case TrackRole::FxRiser:
            case TrackRole::FxImpact:
            case TrackRole::FxAmbience:
            case TrackRole::FxNoise:
                return RoleCategory::FX;

            case TrackRole::Strings:
            case TrackRole::Brass:
            case TrackRole::Winds:
                return RoleCategory::Melody;

            default:
                return RoleCategory::Other;
        }
    }

} // namespace mixcoach

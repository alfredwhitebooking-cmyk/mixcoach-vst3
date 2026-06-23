#include "SpectralProfiler.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeProfile — Desde TrackAudioResult
    // ═══════════════════════════════════════════════════════════════════════════
    TrackSpectralProfile SpectralProfiler::computeProfile(const TrackAudioResult& result) noexcept
    {
        TrackSpectralProfile p;
        p.peakDb      = result.getPeakCombined();
        p.rmsDb       = result.getRmsCombined();
        p.correlation = result.correlation;

        // Crest (peak - RMS en dB)
        if (p.rmsDb > -90.0f && p.peakDb > -90.0f) p.crestDb = p.peakDb - p.rmsDb;

        // AudioDNA descriptors
        p.transientRatio = result.transientRatio;
        for (int b = 0; b < 6; ++b) {
            p.crestPerBand[b]       = result.crestPerBand[b];
            p.stereoWidthPerBand[b] = result.stereoWidthPerBand[b];
        }

        // ═══ Mapear bandEnergies[30] → bandLevelDb[6] (5 sub-bands por banda) ═══
        for (int b = 0; b < 6; ++b) {
            float sum = 0.0f;
            int count = 0;
            for (int s = 0; s < 5; ++s) {
                int idx = b * 5 + s;
                if (idx < 30 && result.bandEnergies[idx] > -90.0f) {
                    sum += result.bandEnergies[idx];
                    ++count;
                }
            }
            p.bandLevelDb[b] = (count > 0) ? (sum / (float)count) : -100.0f;
        }

        // Centroide espectral (promedio ponderado por energía real de banda)
        p.spectralCentroid = estimateCentroid(p.bandLevelDb);

        // Frecuencia fundamental estimada (primera banda con energía significativa)
        p.fundamentalEstimate = estimateFundamental(p.bandLevelDb);

        // ═══ Envelope descriptors ═══
        p.attackTimeMs   = result.attackTimeMs;
        p.releaseTimeMs  = result.releaseTimeMs;
        p.sustainLevelDb = result.sustainLevelDb;

        // ═══ Mid/Side ratio per region ═══
        for (int b = 0; b < 6; ++b) {
            float midE  = result.midEnergyPerBand[b];
            float sideE = result.sideEnergyPerBand[b];
            if (midE > -80.0f && sideE > -80.0f) {
                float midLin      = std::pow(10.0f, midE / 20.0f);
                float sideLin     = std::pow(10.0f, sideE / 20.0f);
                p.midSideRatio[b] = (midLin > 1e-10f) ? sideLin / midLin : 0.0f;
            }
        }

        // Ancho estereo promedio
        float totalWidth = 0.0f;
        for (int b = 0; b < 6; ++b) totalWidth += result.stereoWidthPerBand[b];
        p.avgStereoWidth = totalWidth / 6.0f;

        // Caracteristicas inferidas (orden: hasTransientCharacter antes que isPercussive)
        p.hasTransientCharacter = hasTransientChar(p.crestDb, p.transientRatio);
        p.hasSustainedCharacter = (p.crestDb > 0.0f && p.crestDb < 6.0f);
        p.isPercussive          = (p.attackTimeMs > 0.0f && p.attackTimeMs < 10.0f && p.hasTransientCharacter);
        p.isSustained           = (p.releaseTimeMs > 200.0f) || (p.crestDb < 6.0f && p.releaseTimeMs > 100.0f);
        p.isBassHeavy           = isBassHeavy(p.bandLevelDb);
        p.isBright              = (p.bandLevelDb[4] > p.bandLevelDb[2] || p.bandLevelDb[5] > p.bandLevelDb[2]);
        p.isMonoCompatible      = (p.avgStereoWidth < 0.3f);

        return p;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeProfile — Desde TrackTelemetry (para CoachEngine)
    // ═══════════════════════════════════════════════════════════════════════════
    TrackSpectralProfile SpectralProfiler::computeProfile(const TrackTelemetry& telem) noexcept
    {
        TrackSpectralProfile p;
        p.peakDb      = telem.peak;
        p.rmsDb       = telem.rms;
        p.correlation = telem.correlation;

        if (p.rmsDb > -90.0f && p.peakDb > -90.0f) p.crestDb = p.peakDb - p.rmsDb;

        p.transientRatio = telem.transientRatio;
        for (int b = 0; b < 6; ++b) {
            p.crestPerBand[b]       = telem.crestPerBand[b];
            p.stereoWidthPerBand[b] = telem.stereoWidthPerBand[b];
        }

        // ═══ Mapear bandEnergies[30] → bandLevelDb[6] ═══
        for (int b = 0; b < 6; ++b) {
            float sum = 0.0f;
            int count = 0;
            for (int s = 0; s < 5; ++s) {
                int idx = b * 5 + s;
                if (idx < 30 && telem.bandEnergies[idx] > -90.0f) {
                    sum += telem.bandEnergies[idx];
                    ++count;
                }
            }
            p.bandLevelDb[b] = (count > 0) ? (sum / (float)count) : -100.0f;
        }

        p.spectralCentroid    = estimateCentroid(p.bandLevelDb);
        p.fundamentalEstimate = estimateFundamental(p.bandLevelDb);

        // ═══ Envelope descriptors ═══
        p.attackTimeMs   = telem.attackTimeMs;
        p.releaseTimeMs  = telem.releaseTimeMs;
        p.sustainLevelDb = telem.sustainLevelDb;

        float totalWidth = 0.0f;
        for (int b = 0; b < 6; ++b) totalWidth += telem.stereoWidthPerBand[b];
        p.avgStereoWidth = totalWidth / 6.0f;

        // Caracteristicas inferidas (orden: hasTransientCharacter antes que isPercussive)
        p.hasTransientCharacter = hasTransientChar(p.crestDb, p.transientRatio);
        p.hasSustainedCharacter = (p.crestDb > 0.0f && p.crestDb < 6.0f);
        p.isPercussive          = (p.attackTimeMs > 0.0f && p.attackTimeMs < 10.0f && p.hasTransientCharacter);
        p.isSustained           = (p.releaseTimeMs > 200.0f) || (p.crestDb < 6.0f && p.releaseTimeMs > 100.0f);
        p.isBassHeavy           = isBassHeavy(p.bandLevelDb);
        p.isBright              = (p.bandLevelDb[4] > p.bandLevelDb[2] || p.bandLevelDb[5] > p.bandLevelDb[2]);
        p.isMonoCompatible      = (p.avgStereoWidth < 0.3f);

        return p;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  estimateCentroid — Centroide espectral ponderado por energía (dBFS→lineal)
    // ═══════════════════════════════════════════════════════════════════════════
    float SpectralProfiler::estimateCentroid(const float bandLevelDb[6]) noexcept
    {
        float weightedSum = 0.0f;
        float totalWeight = 0.0f;

        for (int b = 0; b < 6; ++b) {
            // Convertir dBFS a amplitud lineal para usar como peso energético real.
            // Bandas silentes (< -90dBFS) no aportan al centroide.
            if (bandLevelDb[b] <= -90.0f) continue;
            float weight = std::pow(10.0f, bandLevelDb[b] / 20.0f);
            weightedSum += kBandCenterFreqs[b] * weight;
            totalWeight += weight;
        }

        if (totalWeight > 1e-10f) return weightedSum / totalWeight;

        return 0.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  estimateFundamental — Banda más grave con energía significativa
    // ═══════════════════════════════════════════════════════════════════════════
    float SpectralProfiler::estimateFundamental(const float bandLevelDb[6]) noexcept
    {
        // -40 dBFS: umbral realista para "energía significativa".
        // Antes usaba crestPerBand > 3.0 (ratio peak/RMS), que ignoraba
        // sustained bass notes con crest bajo pero enorme energía.
        for (int b = 0; b < 6; ++b) {
            if (bandLevelDb[b] > -40.0f) return kBandCenterFreqs[b];
        }
        return 0.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  hasTransientChar
    // ═══════════════════════════════════════════════════════════════════════════
    bool SpectralProfiler::hasTransientChar(float crest, float transientRatio) noexcept
    {
        return (crest > 12.0f && transientRatio > 1.5f) || crest > 16.0f || transientRatio > 2.5f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  isBassHeavy — Usa energía espectral real (bandLevelDb), no crest factor
    // ═══════════════════════════════════════════════════════════════════════════
    bool SpectralProfiler::isBassHeavy(const float bandLevelDb[6]) noexcept
    {
        float bassEnergy = (bandLevelDb[0] + bandLevelDb[1]) * 0.5f;
        float highEnergy = (bandLevelDb[4] + bandLevelDb[5]) * 0.5f;
        return bassEnergy > highEnergy + 6.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  HELPERS DE ENERGIA ESPECTRAL
    // ═══════════════════════════════════════════════════════════════════════════

    float SpectralProfiler::subBassEnergy(const float bandLevelDb[6]) noexcept
    {
        float sum = 0.0f;
        int count = 0;
        for (int b = 0; b < 2; ++b) {
            if (bandLevelDb[b] > -90.0f) {
                sum += bandLevelDb[b];
                ++count;
            }
        }
        return (count > 0) ? sum / (float)count : -100.0f;
    }

    float SpectralProfiler::midEnergy(const float bandLevelDb[6]) noexcept
    {
        float sum = 0.0f;
        int count = 0;
        for (int b = 2; b < 4; ++b) {
            if (bandLevelDb[b] > -90.0f) {
                sum += bandLevelDb[b];
                ++count;
            }
        }
        return (count > 0) ? sum / (float)count : -100.0f;
    }

    float SpectralProfiler::highEnergy(const float bandLevelDb[6]) noexcept
    {
        float sum = 0.0f;
        int count = 0;
        for (int b = 4; b < 6; ++b) {
            if (bandLevelDb[b] > -90.0f) {
                sum += bandLevelDb[b];
                ++count;
            }
        }
        return (count > 0) ? sum / (float)count : -100.0f;
    }

    float SpectralProfiler::bassToMidRatio(const float bandLevelDb[6]) noexcept
    {
        float sub = subBassEnergy(bandLevelDb);
        float mid = midEnergy(bandLevelDb);
        if (mid < -80.0f || sub < -80.0f) return 0.0f;
        return sub - mid;
    }

    float SpectralProfiler::highToMidRatio(const float bandLevelDb[6]) noexcept
    {
        float high = highEnergy(bandLevelDb);
        float mid  = midEnergy(bandLevelDb);
        if (mid < -80.0f || high < -80.0f) return 0.0f;
        return high - mid;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  inferTrackRole — Arbol de decision V2 con correlacion
    // ═══════════════════════════════════════════════════════════════════════════
    //  Estrategia:
    //   1. Determinar si es transiente (percussivo) o sostenido
    //   2. Analizar distribucion espectral (sub-heavy, mid-heavy, bright)
    //   3. Usar ancho estereo y CORRELACION para refinar
    //   4. Mapear a TrackRole especifico
    //
    //  La correlacion ayuda a distinguir:
    //   - Kick/Bass (corr > 0.9, casi mono) vs Pad/Synth (corr < 0.6)
    //   - VozPrincipal (corr > 0.8) vs VozFondo/Adlibs (corr < 0.6)
    //   - Ride (corr > 0.7, brillante) vs Cymbals/Crash (corr < 0.4)
    //   - Snare (corr > 0.8) vs Percussion (corr < 0.6)
    // ═══════════════════════════════════════════════════════════════════════════
    TrackRole SpectralProfiler::inferTrackRole(const TrackSpectralProfile& p) noexcept
    {
        if (!p.hasData() || p.peakDb < -60.0f) return TrackRole::Unknown;

        const float* bands = p.bandLevelDb;
        float subE         = subBassEnergy(bands);
        float midE         = midEnergy(bands);
        float highE        = highEnergy(bands);
        float bToM         = bassToMidRatio(bands);
        float hToM         = highToMidRatio(bands);
        // Umbrales realistas: antes -60dBFS era demasiado generoso (cualquier
        // noise floor lo cumplía). Ahora exigimos energía genuina y que domine
        // sobre los medios, no solo "estar a 5dB de los medios".
        bool hasStrongSub  = (subE > -45.0f && (subE - midE) > -2.0f);
        bool hasStrongHigh = (highE > -50.0f && (highE - midE) > -1.0f);
        float corr         = p.correlation;

        // ═══════════════════════════════════════════════════════════════════════
        //  CATEGORIA: PERCUSSIVO (transientRatio > 2.0 y crest > 12dB)
        // ═══════════════════════════════════════════════════════════════════════
        if (p.hasTransientCharacter) {
            // ─── Kick: sub fuerte, crest muy alto, cuasi-mono, corr > 0.85 ──
            if (hasStrongSub && bToM > 5.0f && p.isMonoCompatible && corr > 0.85f) {
                if (p.transientRatio > 2.5f && p.attackTimeMs < 8.0f) return TrackRole::Kick;
                else
                    return TrackRole::Kick808;
            }

            // ─── HiHat: agudos dominantes, transientRatio extremo ─────────
            if (hasStrongHigh && hToM > 6.0f && p.transientRatio > 3.0f) {
                // HiHat abierto: menor correlacion (mas estereo) que cerrado
                if (p.avgStereoWidth > 0.3f || corr < 0.6f) return TrackRole::HiHatOpen;
                return TrackRole::HiHat;
            }

            // ─── Ride: brillante 12-15kHz, mas sostenido que hi-hat,
            //     correlacion mas alta que crash (ride es mas centrado)
            if (hasStrongHigh && hToM > 8.0f && p.transientRatio > 2.0f && corr > 0.7f && p.releaseTimeMs > 80.0f) {
                return TrackRole::Ride;
            }

            // ─── Snare: medios-altos, crest alto, mono, corr > 0.8 ────
            if (midE > -50.0f && (highE > midE - 5.0f) && p.isMonoCompatible && corr > 0.8f) {
                if (hToM > 3.0f) return TrackRole::SnareTrap;
                if (p.releaseTimeMs < 80.0f && p.releaseTimeMs > 0.0f) return TrackRole::Snare;
                return TrackRole::Snare;
            }

            // ─── Clap: medios, transientRatio muy alto, corr media ─────────
            if (midE > -50.0f && p.transientRatio > 3.5f && !hasStrongSub && corr < 0.8f) return TrackRole::Clap;

            // ─── Tom: medios-graves, mono, corr alta ─────────────────────
            if (midE > -40.0f && (midE - highE) > 3.0f && p.isMonoCompatible && corr > 0.8f) {
                // Floor tom: mas grave que tom normal
                if (bToM > 2.0f) return TrackRole::TomFloor;
                return TrackRole::Tom;
            }

            // ─── Crash: ancho espectro, baja correlacion (stereo amplio) ──
            if (!hasStrongSub && (p.avgStereoWidth > 0.3f || corr < 0.5f)) return TrackRole::Crash;

            // ─── Percusion generica ─────────────────────────────────────────
            return TrackRole::Percussion;
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  CATEGORIA: SOSTENIDO (crest < 8dB, transientRatio < 1.5)
        // ═══════════════════════════════════════════════════════════════════════
        if (p.hasSustainedCharacter || p.transientRatio < 1.5f) {
            // ─── Sub-bajo: graves dominantes, crest muy bajo, mono, corr alta ─
            if (hasStrongSub && bToM > 5.0f && p.crestDb < 6.0f && corr > 0.9f) {
                if (p.releaseTimeMs > 300.0f || p.isSustained) return TrackRole::Bass808;
                return TrackRole::BassSub;
            }

            // ─── Bajo 808: menos sub, mas armónicos, corr alta ────────────
            if (hasStrongSub && bToM > 3.0f && corr > 0.85f && p.crestDb < 10.0f) return TrackRole::Bass808;

            // ─── Bajo finger: sostenido pero no tan apretado ──────────────
            if (hasStrongSub && bToM > 2.0f && corr > 0.8f) return TrackRole::BassFinger;

            // ─── Pad: sostenido, stereo amplio, crest bajo, baja corr ─────
            if (p.avgStereoWidth > 0.35f && p.crestDb < 8.0f && midE > -50.0f) {
                // SynthPad: correlacion mas baja (estereo procesado)
                // Strings: correlacion mas alta (orquestal centrado)
                if (corr < 0.6f || hasStrongSub || bToM > 0.0f) return TrackRole::SynthPad;
                return TrackRole::Strings;
            }

            // ─── Ambiente / Ruido: stereo amplio, baja corr, silencioso ───
            if ((p.avgStereoWidth > 0.4f || corr < 0.3f) && p.peakDb < -14.0f) return TrackRole::FxAmbience;

            // ─── Organo: sostenido, mono, medios, poca dispersion stereo ──
            if (midE > -45.0f && p.isMonoCompatible && p.crestDb < 7.0f && corr > 0.8f) return TrackRole::KeysOrgan;

            // ─── Bajo synth: variado, crest bajo-medio ────────────────────
            if (hasStrongSub && bToM > 3.0f && p.crestDb < 12.0f) return TrackRole::BassSynth;
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  CATEGORIA: VOCAL (crest moderado 6-14dB, mid-high, sin sub)
        //  Correlacion distingue: Principal > 0.8, Fondo 0.4-0.8, Adlibs < 0.4
        // ═══════════════════════════════════════════════════════════════════════
        if (p.crestDb >= 6.0f && p.crestDb <= 14.0f && !hasStrongSub && midE > -45.0f && highE > midE - 8.0f) {
            // VozPrincipal: centrada, correlacion alta, mono
            if (corr > 0.8f && p.avgStereoWidth < 0.15f) return TrackRole::VozPrincipal;
            // VozFondo: moderadamente estereo
            if (corr > 0.4f || p.avgStereoWidth < 0.4f) return TrackRole::VozFondo;
            // Adlibs: ampliamente estereo, baja correlacion
            return TrackRole::Adlibs;
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  CATEGORIA: GUITARRA (crest 8-16dB, medios)
        // ═══════════════════════════════════════════════════════════════════════
        if (p.crestDb >= 8.0f && p.crestDb <= 16.0f && midE > -45.0f) {
            // Guitarra acustica: transiente medio, presencia, corr media-alta
            if (p.transientRatio > 1.5f && hasStrongHigh) return TrackRole::GuitarAcoustic;
            // Guitarra electrica: sostenida, crest medio, corr media
            if (p.transientRatio < 1.5f && bToM < 3.0f && corr < 0.85f) return TrackRole::GuitarElectric;
            // Guitarra lider: presencia, crest alto, mas brillante
            if (p.transientRatio > 1.8f && highE > midE - 3.0f) return TrackRole::GuitarLead;
            // Guitarra ritmica
            return TrackRole::GuitarRhythm;
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  CATEGORIA: TECLADOS (crest variado, espectro amplio)
        // ═══════════════════════════════════════════════════════════════════════
        if (midE > -45.0f) {
            // Piano: amplio espectro, stereo medio, corr 0.5-0.8
            if (p.crestDb >= 8.0f && p.crestDb <= 16.0f && hasStrongHigh) return TrackRole::KeysPiano;
            // Synth lead: presencia, crest medio-alto, corr variable
            if (p.transientRatio > 1.5f && highE > midE - 3.0f) return TrackRole::SynthLead;
            // Synth pluck: ataque rapido, decay, ancho
            if (p.transientRatio > 2.0f && p.avgStereoWidth > 0.2f) return TrackRole::SynthPluck;
            // Rhodes/Wurly: suave, crest bajo-medio, corr media
            if (p.crestDb < 10.0f && corr > 0.5f && corr < 0.85f) return TrackRole::KeysElectric;
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  CATEGORIA: MELODICOS / OTROS
        // ═══════════════════════════════════════════════════════════════════════
        // Brass: presencia fuerte, crest medio, monos mas centrados
        if (midE > -45.0f && hasStrongHigh && hToM > 3.0f && p.crestDb >= 8.0f && corr > 0.7f) return TrackRole::Brass;

        // Strings: sostenido, stereo, crest bajo
        if (midE > -45.0f && p.avgStereoWidth > 0.3f && p.crestDb < 10.0f) return TrackRole::Strings;

        // Vientos (Winds): crest medio, presencia sin ser brillante
        if (midE > -45.0f && hasStrongHigh && p.crestDb < 12.0f && corr > 0.7f) return TrackRole::Winds;

        // Riser: energia creciendo de graves a agudos (sweep up)
        // Detectado por: subE < midE < highE (energia aumenta con la frecuencia)
        if (subE < -50.0f && midE > subE + 6.0f && highE > midE + 3.0f && p.crestDb < 14.0f) return TrackRole::FxRiser;

        return TrackRole::Unknown;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  inferTrackRole — Version GENRE-AWARE
    //  Ejecuta la inferencia base y luego aplica ajustes segun el genero.
    //  Ejemplos:
    //    Reggaeton  → Kick → ReggaetonKick
    //    Trap       → Kick → Kick808, Snare → SnareTrap
    //    Rock       → favor BassPick sobre BassFinger
    //    Jazz       → favor GuitarAcoustic sobre GuitarElectric
    //    Electronic → favor BassSub, SynthPad
    // ═══════════════════════════════════════════════════════════════════════════
    TrackRole SpectralProfiler::inferTrackRole(const TrackSpectralProfile& p, const juce::String& genre) noexcept
    {
        // ─── 1. Inferencia base (sin genero) ─────────────────────────────────
        TrackRole role = inferTrackRole(p);
        if (role == TrackRole::Unknown) return role;

        // ─── 2. Ajustes por genero ───────────────────────────────────────────
        juce::String g = genre.trim().toLowerCase();
        if (g.isEmpty()) return role;

        // ─── Reggaeton / Latin / Dancehall ───────────────────────────────────
        // Sonido caracteristico: bombo corto y punchy, 808s melódicos
        if (g == "reggaeton" || g == "latin" || g == "dancehall" || g == "reggae") {
            if (role == TrackRole::Kick) return TrackRole::ReggaetonKick;
            if (role == TrackRole::BassSub || role == TrackRole::BassFinger) return TrackRole::Bass808;
            if (role == TrackRole::BassPick) return TrackRole::BassFinger; // Reggaeton usa finger style mas que pick
            return role;
        }

        // ─── Trap / Drill / Hip Hop ─────────────────────────────────────────
        // 808s, hi-hats rapidos, snares agudas con click
        if (g == "trap" || g == "drill" || g == "hip hop" || g == "hiphop") {
            if (role == TrackRole::Kick) return TrackRole::Kick808;
            if (role == TrackRole::Snare) return TrackRole::SnareTrap;
            if (role == TrackRole::BassFinger || role == TrackRole::BassPick) return TrackRole::Bass808;
            if (role == TrackRole::BassSub) return TrackRole::Bass808;
            if (role == TrackRole::HiHatOpen) return TrackRole::HiHat; // Trap usa hi-hats cerrados rapidos
            return role;
        }

        // ─── Rock / Metal / Punk ────────────────────────────────────────────
        // Bateria real, bajo con pua, guitarras electricas
        if (g == "rock" || g == "metal" || g == "punk" || g == "alternative" || g == "indie") {
            if (role == TrackRole::BassFinger) return TrackRole::BassPick;
            if (role == TrackRole::Bass808 || role == TrackRole::BassSub)
                return TrackRole::BassPick;                                          // Rock usa bajo real, no 808
            if (role == TrackRole::GuitarAcoustic) return TrackRole::GuitarElectric; // Rock prioriza electrica
            if (role == TrackRole::SynthPad || role == TrackRole::SynthLead)
                return TrackRole::KeysPiano; // Mas probable piano que synth
            if (role == TrackRole::Kick808) return TrackRole::Kick;
            return role;
        }

        // ─── Electronic / EDM / House ───────────────────────────────────────
        // Sintetizadores, sub-bajos, pads
        if (g == "electronic" || g == "edm" || g == "house" || g == "techno" || g == "trance") {
            if (role == TrackRole::BassPick || role == TrackRole::BassFinger) return TrackRole::BassSynth;
            if (role == TrackRole::Kick) return TrackRole::Kick808; // Kicks electronicos son mas "808 style"
            if (role == TrackRole::GuitarAcoustic || role == TrackRole::GuitarElectric)
                return TrackRole::SynthLead;                           // Synth reemplaza guitarra
            if (role == TrackRole::Brass) return TrackRole::SynthLead; // Brass sintetizado
            return role;
        }

        // ─── Jazz / Acoustic / Classical ────────────────────────────────────
        // Instrumentos acusticos, pianos reales, cuerdas
        if (g == "jazz" || g == "acoustic" || g == "classical" || g == "folk" || g == "blues") {
            if (role == TrackRole::GuitarElectric) return TrackRole::GuitarAcoustic;
            if (role == TrackRole::BassPick || role == TrackRole::BassSynth) return TrackRole::BassFinger;
            if (role == TrackRole::Kick808 || role == TrackRole::Bass808)
                return role; // Keep if specifically detected, but generally jazz has real bass
            if (role == TrackRole::SynthLead || role == TrackRole::SynthPad) return TrackRole::KeysPiano;
            if (role == TrackRole::SynthPluck) return TrackRole::KeysPiano;
            return role;
        }

        // ─── R&B / Soul / Funk ──────────────────────────────────────────────
        // Bajos con dedo, teclados electricos, voz centrada
        if (g == "r&b" || g == "rnb" || g == "soul" || g == "funk") {
            if (role == TrackRole::BassPick) return TrackRole::BassFinger;
            if (role == TrackRole::Bass808) return role;                      // R&B moderno usa 808s
            if (role == TrackRole::KeysOrgan) return role;                    // Hammond is common in soul/funk
            if (role == TrackRole::KeysPiano) return TrackRole::KeysElectric; // Rhodes/Wurly is more R&B
            return role;
        }

        // ─── Pop / Lo-fi ────────────────────────────────────────────────────
        // Mezcla de todo, pero favorece sintetizadores suaves y bajos con dedo
        if (g == "pop" || g == "lo-fi" || g == "lofi") {
            if (role == TrackRole::BassPick) return TrackRole::BassFinger;
            if (role == TrackRole::GuitarElectric) return TrackRole::GuitarAcoustic; // Lofi usa acustica
            return role;
        }

        // ─── Country / Bluegrass ───────────────────────────────────────────
        // Instrumentos acusticos, banjo, fiddle
        if (g == "country" || g == "bluegrass") {
            if (role == TrackRole::GuitarElectric || role == TrackRole::GuitarLead) return TrackRole::GuitarAcoustic;
            if (role == TrackRole::BassPick || role == TrackRole::BassSynth) return TrackRole::BassFinger;
            return role;
        }

        return role;
    }

} // namespace mixcoach

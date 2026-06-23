#include "SemanticComparator.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════════

juce::String SemanticComparator::formatDb(float value)
{
    if (value < -90.0f) return "-inf dB";
    return juce::String(value, 1) + " dB";
}

juce::String SemanticComparator::roleDisplayName(TrackRole role)
{
    return juce::String(getRoleName(role));
}

SemanticIssue SemanticComparator::makeIssue(
    SemanticIssue::Severity sev, SemanticIssue::Domain dom,
    const juce::String& trackName, TrackRole role,
    const juce::String& message, float actual, float expected,
    const juce::String& recommendation)
{
    SemanticIssue iss;
    iss.severity     = sev;
    iss.domain       = dom;
    iss.trackName    = trackName;
    iss.role         = role;
    iss.message      = message;
    iss.actualValue  = actual;
    iss.expectedValue = expected;
    iss.deviation    = actual - expected;
    iss.recommendation = recommendation;
    return iss;
}

SemanticIssue SemanticComparator::makePraise(
    SemanticIssue::Severity sev, SemanticIssue::Domain dom,
    const juce::String& trackName, TrackRole role,
    const juce::String& message)
{
    SemanticIssue iss;
    iss.severity  = sev;
    iss.domain    = dom;
    iss.trackName = trackName;
    iss.role      = role;
    iss.message   = message;
    return iss;
}

// ═══════════════════════════════════════════════════════════════════════════
//  toTextSummary — Resumen textual para prompts LLM
// ═══════════════════════════════════════════════════════════════════════════
juce::String SemanticDiff::toTextSummary() const
{
    if (!hasProblems() && praises.empty())
        return {};

    juce::String s;
    s += juce::String(getRoleName(role)) + " \"" + trackName + "\"";

    // Problemas (máximo 2)
    int shown = 0;
    for (const auto& iss : issues) {
        if (!iss.isProblem()) continue;
        if (shown >= 2) { s += "..."; break; }
        s += ": " + iss.message;
        shown++;
    }

    // Logros (máximo 1)
    if (!praises.empty()) {
        s += " | " + praises[0].message;
    }

    return s;
}

// ═══════════════════════════════════════════════════════════════════════════
//  compareTrack — Compara una pista contra su perfil esperado
// ═══════════════════════════════════════════════════════════════════════════
SemanticDiff SemanticComparator::compareTrack(int slotIndex,
                                               const juce::String& trackName,
                                               TrackRole role,
                                               const TrackSpectralProfile& profile)
{
    SemanticDiff diff;
    diff.slotIndex = slotIndex;
    diff.trackName = trackName;
    diff.role      = role;
    diff.profile   = profile;
    diff.expected  = getExpectedProfile(role);

    if (!profile.hasData())
        return diff;

    // Ejecutar todas las verificaciones
    checkGain(diff, profile, diff.expected);
    checkDynamics(diff, profile, diff.expected);
    checkSpectralBalance(diff, profile, diff.expected);
    checkStereoWidth(diff, profile, diff.expected);
    checkTransient(diff, profile, diff.expected);

    // ═══ Priorizar issues por severidad: Critical → Warning → Info → Praise ═══
    std::sort(diff.issues.begin(), diff.issues.end(),
        [](const SemanticIssue& a, const SemanticIssue& b) {
            // Map severity to numeric priority (lower = more urgent)
            auto prio = [](SemanticIssue::Severity s) -> int {
                switch (s) {
                    case SemanticIssue::Severity::Critical: return 0;
                    case SemanticIssue::Severity::Warning:  return 1;
                    case SemanticIssue::Severity::Info:     return 2;
                    case SemanticIssue::Severity::Praise:   return 3;
                    default: return 4;
                }
            };
            return prio(a.severity) < prio(b.severity);
        });

    return diff;
}

// ═══════════════════════════════════════════════════════════════════════════
//  compareAllTracks — Escanea todas las pistas activas
// ═══════════════════════════════════════════════════════════════════════════
std::vector<SemanticDiff> SemanticComparator::compareAllTracks(
    const SlotRegistry& registry, const SharedData& sharedData,
    const std::array<TrackRole, SlotRegistry::kMaxSlots>& roles)
{
    std::vector<SemanticDiff> results;
    results.reserve(32);

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = sharedData.getTrackAudioResult(info.slotIndex);
        if (telem.timestampUs == 0) return;

        auto profile = SpectralProfiler::computeProfile(telem);
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty())
            name = "Track " + juce::String(info.slotIndex + 1);

        TrackRole role = roles[info.slotIndex];
        if (role == TrackRole::Unknown) {
            // Inferir rol desde el bus asignado
            switch (info.bus) {
                case BusType::Drums:  role = TrackRole::DrumBus; break;
                case BusType::Bass:   role = TrackRole::BassBus; break;
                case BusType::Vocals: role = TrackRole::VozBus; break;
                case BusType::Guitars: role = TrackRole::GuitarBus; break;
                case BusType::Keys:   role = TrackRole::KeysBus; break;
                case BusType::Melody: role = TrackRole::Strings; break;
                default: break;
            }
        }

        auto diff = compareTrack(info.slotIndex, name, role, profile);
        if (diff.hasProblems() || !diff.praises.empty())
            results.push_back(std::move(diff));
    });

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
//  CHECK: Gain
// ═══════════════════════════════════════════════════════════════════════════
void SemanticComparator::checkGain(SemanticDiff& diff,
                                    const TrackSpectralProfile& p,
                                    const ExpectedProfile& e)
{
    float dev = e.getPeakDeviation(p.peakDb);

    if (p.peakDb > -0.5f) {
        // Clipping
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Critical, SemanticIssue::Domain::Gain,
            diff.trackName, diff.role,
            "CLIPPING at " + formatDb(p.peakDb) + " — reduce gain inmediatamente",
            p.peakDb, e.peakTargetDb,
            "Reduce " + formatDb(p.peakDb - (-3.0f)) + " en el fader de ganancia"));
    }
    else if (dev > e.peakTolerance) {
        // Demasiado fuerte
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Warning, SemanticIssue::Domain::Gain,
            diff.trackName, diff.role,
            juce::String(dev, 1) + " dB above target peak of "
            + formatDb(e.peakTargetDb),
            p.peakDb, e.peakTargetDb,
            "Reduce gain " + juce::String(dev * 0.7f, 1)
            + " dB (target peak: " + formatDb(e.peakTargetDb) + ")"));
    }
    else if (dev < -e.peakTolerance && p.peakDb > -50.0f) {
        // Demasiado bajo
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Warning, SemanticIssue::Domain::Gain,
            diff.trackName, diff.role,
            juce::String(-dev, 1) + " dB below target peak of "
            + formatDb(e.peakTargetDb),
            p.peakDb, e.peakTargetDb,
            "Increase gain " + juce::String(-dev * 0.7f, 1)
            + " dB (target peak: " + formatDb(e.peakTargetDb) + ")"));
    }
    else if (p.peakDb > -90.0f && std::abs(dev) < e.peakTolerance * 0.5f) {
        // Bien!
        diff.praises.push_back(makePraise(
            SemanticIssue::Severity::Praise, SemanticIssue::Domain::Gain,
            diff.trackName, diff.role,
            "Nivel optimo: " + formatDb(p.peakDb)
            + " (target: " + formatDb(e.peakTargetDb) + ")"));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CHECK: Dynamics (Crest)
// ═══════════════════════════════════════════════════════════════════════════
void SemanticComparator::checkDynamics(SemanticDiff& diff,
                                        const TrackSpectralProfile& p,
                                        const ExpectedProfile& e)
{
    if (p.crestDb <= 0.0f) return; // Sin datos

    if (p.crestDb > (e.crestTargetDb + e.crestTolerance)) {
        // Demasiado dinámico
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Warning, SemanticIssue::Domain::Dynamics,
            diff.trackName, diff.role,
            "Crest " + juce::String(p.crestDb, 1) + " dB — demasiado din\xC3\xA1" "mico "
            "(target: " + juce::String(e.crestTargetDb, 1) + " dB). "
            "Considera compresi\xC3\xB3" "n suave",
            p.crestDb, e.crestTargetDb,
            "Prueba compresor 2:1, threshold -"
            + juce::String(std::abs(p.peakDb + 6.0f), 1) + " dB, attack 10ms, release 100ms"));
    }
    else if (p.crestDb < (e.crestTargetDb - e.crestTolerance) && p.crestDb > 1.0f) {
        // Demasiado comprimido
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Info, SemanticIssue::Domain::Dynamics,
            diff.trackName, diff.role,
            "Crest " + juce::String(p.crestDb, 1) + " dB — bajo para un "
            + roleDisplayName(diff.role) + " (target: "
            + juce::String(e.crestTargetDb, 1) + " dB). "
            "Puede estar sobre-comprimido",
            p.crestDb, e.crestTargetDb,
            "Reduce el ratio del compresor o usa attack mas rapido (5ms)"));
    }
    else if (p.crestDb > 0.0f) {
        diff.praises.push_back(makePraise(
            SemanticIssue::Severity::Praise, SemanticIssue::Domain::Dynamics,
            diff.trackName, diff.role,
            "Dinamica optima: crest " + juce::String(p.crestDb, 1)
            + " dB (target: " + juce::String(e.crestTargetDb, 1) + " dB)"));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CHECK: Spectral Balance — EQ quirúrgico con Hz + dB + Q precisos
//  Usa bandLevelDb[6] (energía absoluta por banda) comparado contra
//  spectralOffset[6] (target esperado) para generar recomendaciones EQ
//  con frecuencia exacta (Hz), cantidad (dB), Q, y tipo de filtro.
// ═══════════════════════════════════════════════════════════════════════════

// ─── Constantes de tolerancia espectral por banda ─────────────────────────
// Bandas críticas (LoMid, HiMid) tienen menos tolerancia porque afectan
// más la claridad y presencia. Extremos (Sub, Air) permiten más variación.
static constexpr float kBandTolerance[6] = {
    8.0f,  // Sub (94Hz):    más tolerante
    7.0f,  // Bass (281Hz):  tolerancia media
    5.0f,  // LoMid (752Hz): crítica — claridad vocal/instrumental
    5.0f,  // HiMid (2kHz):  crítica — presencia
    6.0f,  // Pres (5kHz):   media
    8.0f   // Air (12kHz):   más tolerante
};

// ─── Frecuencias de borde para cada banda espectral (Hz) ─────────────────
// Usadas para setear affectedLowHz/affectedHighHz en los diagnósticos
// de SemanticIssue, que se renderizan como overlay en el Spectrograph.
static constexpr float kBandLowFreq[6] = {
    20.0f,    // Sub
    94.0f,    // Bass
    281.0f,   // LoMid
    752.0f,   // HiMid
    2046.0f,  // Pres
    5144.0f   // Air
};
static constexpr float kBandHighFreq[6] = {
    94.0f,    // Sub
    281.0f,   // Bass
    752.0f,   // LoMid
    2046.0f,  // HiMid
    5144.0f,  // Pres
    20000.0f  // Air
};

// ─── Q recomendado por banda (más estrecho en mids, más ancho en extremos) ─
static constexpr float kBandQ[6] = {
    1.8f,  // Sub: estrecho para evitar resonancia
    1.4f,  // Bass: medio
    2.0f,  // LoMid: estrecho (quirúrgico)
    1.8f,  // HiMid: estrecho
    1.4f,  // Pres: medio
    0.7f   // Air: ancho (shelf-like)
};

// ─── Tipo de filtro recomendado según banda y polaridad ───────────────────
//  cut:    banda demasiado fuerte → reducir
//  boost:  banda demasiado débil   → aumentar
//  Retorna string corto como "Bell", "Low shelf", "High shelf", "HPF"
static const char* getFilterType(int band, bool isCut) noexcept
{
    if (band == 0 && isCut)  return "HPF";
    if (band <= 1)           return isCut ? "Low shelf" : "Bell";
    if (band >= 4)           return "High shelf";
    return "Bell";
}

void SemanticComparator::checkSpectralBalance(SemanticDiff& diff,
                                               const TrackSpectralProfile& p,
                                               const ExpectedProfile& e)
{
    // ─── Detectar si tenemos datos espectrales ────────────────────────────
    bool hasBandLevel = false;
    for (int b = 0; b < 6; ++b)
        if (p.bandLevelDb[b] > -90.0f) { hasBandLevel = true; break; }

    bool hasCrestData = false;
    for (int b = 0; b < 6; ++b)
        if (p.crestPerBand[b] > 0.5f) { hasCrestData = true; break; }

    if (!hasBandLevel && !hasCrestData) return;

    // ═══════════════════════════════════════════════════════════════════════
    //  FASE 1: Análisis por banda individual (EQ quirúrgico)
    //  Para cada banda espectral, compara energía real vs esperada y genera
    //  una recomendación EQ con Hz exacto, dB, Q, y tipo de filtro.
    // ═══════════════════════════════════════════════════════════════════════
    float actualPeak = p.peakDb;

    if (hasBandLevel && actualPeak > -90.0f)
    {
        for (int b = 0; b < 6; ++b)
        {
            if (p.bandLevelDb[b] <= -90.0f) continue;

            // Desviación: qué tan lejos está esta banda de lo esperado
            // actualRelative = bandLevelDb[b] - actualPeak  (dBFS relativo al peak)
            // expectedRelative = e.spectralOffset[b]       (dBFS relativo al peak esperado)
            float actualRelative = p.bandLevelDb[b] - actualPeak;
            float expectedRelative = e.spectralOffset[b];
            float deviation = actualRelative - expectedRelative; // >0 = too loud, <0 = too quiet

            float absDev = std::abs(deviation);
            if (absDev < kBandTolerance[b]) continue;

            float freq = SpectralProfiler::kBandCenterFreqs[b];
            const char* bandName = SpectralProfiler::kBandNames[b];
            float q = kBandQ[b];
            bool isCut = (deviation > 0.0f);
            const char* filterType = getFilterType(b, isCut);

            // Severidad según magnitud de desviación
            SemanticIssue::Severity sev;
            if (absDev > 12.0f)      sev = SemanticIssue::Severity::Critical;
            else if (absDev > 8.0f)  sev = SemanticIssue::Severity::Warning;
            else                     sev = SemanticIssue::Severity::Info;

            // Generar mensaje con parámetros EQ exactos
            // Setear rango de frecuencia afectado para overlay visual
            float freqLow  = kBandLowFreq[b];
            float freqHigh = kBandHighFreq[b];
            juce::String direction = isCut ? "Sobra" : "Falta";
            juce::String actionWord = isCut ? "Corta" : "Sube";
            float amount = std::floor(absDev * 10.0f) / 10.0f; // Redondear a 1 decimal

            // Calcular frecuencia exacta para el mensaje
            juce::String freqStr;
            if (freq >= 1000.0f)
                freqStr = juce::String(freq / 1000.0f, 1) + "kHz";
            else
                freqStr = juce::String((int)freq) + "Hz";

            juce::String message = bandName + juce::String(": ") + direction
                + " " + juce::String(amount, 1) + "dB vs target.";

            // Recomendación con parámetros EQ precisos
            juce::String recommendation;
            if (b == 0 && isCut && deviation > 10.0f)
            {
                // Sub demasiado fuerte → recomendar HPF
                recommendation = "Aplica HPF en 80-100Hz (24dB/oct) para limpiar sub-graves";
            }
            else if (b == 0 && !isCut && deviation < -10.0f)
            {
                // Sub demasiado débil → recomendar sub-armónico o boost
                recommendation = juce::String(actionWord) + " "
                    + juce::String(amount, 1) + "dB con Bell en " + freqStr
                    + " Q=" + juce::String(q, 1);
            }
            else if (b <= 1 && isCut)
            {
                // Graves demasiado fuertes → shelf o HPF
                recommendation = juce::String(actionWord) + " "
                    + juce::String(amount, 1) + "dB con " + juce::String(filterType)
                    + " en " + freqStr + " Q=" + juce::String(q, 1);
            }
            else if (b >= 4 && !isCut)
            {
                // Agudos demasiado débiles → shelf boost
                recommendation = juce::String(actionWord) + " "
                    + juce::String(amount, 1) + "dB con " + juce::String(filterType)
                    + " en " + freqStr + " Q=" + juce::String(q, 1);
            }
            else
            {
                // Mid bands → Bell quirúrgico
                recommendation = juce::String(actionWord) + " "
                    + juce::String(amount, 1) + "dB con " + juce::String(filterType)
                    + " en " + freqStr + " Q=" + juce::String(q, 1);
            }

            auto issue = makeIssue(
                sev, SemanticIssue::Domain::Spectral,
                diff.trackName, diff.role,
                message,
                actualRelative, expectedRelative,
                recommendation);
            issue.affectedLowHz  = freqLow;
            issue.affectedHighHz = freqHigh;
            diff.issues.push_back(std::move(issue));
        }

    }

    // ═══════════════════════════════════════════════════════════════════════
    //  FASE 2: Análisis de banda dominante (fallback si no hay bandLevelDb)
    //  También ejecuta siempre con crestPerBand como verificacion secundaria.
    // ═══════════════════════════════════════════════════════════════════════
    if (hasCrestData)
    {
        // Encontrar la banda dominante (la de mayor crest)
        float maxCrest = 0.0f;
        int dominantBand = -1;
        for (int b = 0; b < 6; ++b) {
            if (p.crestPerBand[b] > maxCrest) {
                maxCrest = p.crestPerBand[b];
                dominantBand = b;
            }
        }

        if (dominantBand >= 0 && maxCrest >= 1.0f)
        {
            // El offset esperado indica cuánto más baja debería ser cada banda
            int expectedDominant = 0;
            float maxOffset = e.spectralOffset[0];
            for (int b = 1; b < 6; ++b) {
                if (e.spectralOffset[b] > maxOffset) {
                    maxOffset = e.spectralOffset[b];
                    expectedDominant = b;
                }
            }

            // Si la banda dominante REAL no coincide con la esperada y la diferencia
            // es significativa (y no generamos ya un issue individual para esta banda)
            if (dominantBand != expectedDominant) {
                float actualRel = p.crestPerBand[expectedDominant] - maxCrest;
                if (actualRel < -8.0f) {
                    diff.issues.push_back(makeIssue(
                        SemanticIssue::Severity::Info, SemanticIssue::Domain::Spectral,
                        diff.trackName, diff.role,
                        "Banda dominante es "
                        + juce::String(SpectralProfiler::kBandNames[dominantBand])
                        + " pero se esperaba "
                        + juce::String(SpectralProfiler::kBandNames[expectedDominant])
                        + " para un " + roleDisplayName(diff.role),
                        p.crestPerBand[dominantBand], p.crestPerBand[expectedDominant],
                        "Revisa el balance espectral: "
                        + juce::String(SpectralProfiler::kBandNames[expectedDominant])
                        + " deberia ser la banda mas fuerte"));
                }
            }

            // ─── Verificar rango espectral anómalo ────────────────────────
            float highFreqEnergy = (p.crestPerBand[4] > 1.0f ? p.crestPerBand[4] : 0.0f)
                                 + (p.crestPerBand[5] > 1.0f ? p.crestPerBand[5] : 0.0f);
            float lowFreqEnergy = (p.crestPerBand[0] > 1.0f ? p.crestPerBand[0] : 0.0f)
                                + (p.crestPerBand[1] > 1.0f ? p.crestPerBand[1] : 0.0f);

            // Roles que NO deberían tener mucha energía en graves
            if (lowFreqEnergy > highFreqEnergy + 6.0f
                && diff.role != TrackRole::BassSub
                && diff.role != TrackRole::Bass808  && diff.role != TrackRole::BassFinger
                && diff.role != TrackRole::BassPick && diff.role != TrackRole::Kick
                && diff.role != TrackRole::Kick808  && diff.role != TrackRole::Tom
                && diff.role != TrackRole::TomFloor && diff.role != TrackRole::BassBus
                && diff.role != TrackRole::DrumBus)
            {
                diff.issues.push_back(makeIssue(
                    SemanticIssue::Severity::Info, SemanticIssue::Domain::Spectral,
                    diff.trackName, diff.role,
                    "Energia grave inusualmente alta para un "
                    + roleDisplayName(diff.role),
                    lowFreqEnergy, highFreqEnergy,
                    "Aplica un high-pass filter en 80-120Hz para limpiar frecuencias"));
            }

            // Roles que DEBERÍAN tener mucha energía en graves pero no la tienen
            if ((diff.role == TrackRole::BassSub || diff.role == TrackRole::Bass808)
                && lowFreqEnergy < 6.0f)
            {
                diff.issues.push_back(makeIssue(
                    SemanticIssue::Severity::Warning, SemanticIssue::Domain::Spectral,
                    diff.trackName, diff.role,
                    "Poca energia en sub-graves para un " + roleDisplayName(diff.role),
                    lowFreqEnergy, 10.0f,
                    "Verifica que el 808 tenga suficiente contenido sub-100Hz. "
                    "Prueba saturar a 60Hz con un waveshaper suave"));
            }

            // Roles que deberían tener presencia en agudos pero no la tienen
            if (diff.role == TrackRole::VozPrincipal || diff.role == TrackRole::HiHat
                || diff.role == TrackRole::SnareTrap || diff.role == TrackRole::SynthLead)
            {
                float highCrest = p.crestPerBand[4] + p.crestPerBand[5];
                if (highCrest < 4.0f && p.crestPerBand[3] < 3.0f) {
                    diff.issues.push_back(makeIssue(
                        SemanticIssue::Severity::Info, SemanticIssue::Domain::Spectral,
                        diff.trackName, diff.role,
                        "Falta presencia en agudos para un " + roleDisplayName(diff.role),
                        highCrest, 8.0f,
                        "Sube 3-4dB con High shelf en 5kHz Q=0.7 para recuperar presencia"));
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CHECK: Stereo Width
// ═══════════════════════════════════════════════════════════════════════════
void SemanticComparator::checkStereoWidth(SemanticDiff& diff,
                                           const TrackSpectralProfile& p,
                                           const ExpectedProfile& e)
{
    for (int b = 0; b < 6; ++b) {
        if (!e.isWidthInBandOk(b, p.stereoWidthPerBand[b])) {
            static const char* bandNames[6] = {
                "Sub (94Hz)", "Bass (281Hz)", "LoMid (752Hz)",
                "HiMid (2kHz)", "Pres (5kHz)", "Air (12kHz)"
            };

            if (b <= 1) {
                // Grave/sub-grave estéreo = problema crítico
                diff.issues.push_back(makeIssue(
                    SemanticIssue::Severity::Warning, SemanticIssue::Domain::StereoWidth,
                    diff.trackName, diff.role,
                    "Ancho estéreo en " + juce::String(bandNames[b])
                    + ": " + juce::String(p.stereoWidthPerBand[b], 2)
                    + " (max: " + juce::String(e.stereoWidthMax[b], 2) + "). "
                    "Los graves deben ser MONO",
                    p.stereoWidthPerBand[b], e.stereoWidthMax[b],
                    "Activa el filtro mono en < 150Hz o reduce el stereo widening"));
            } else {
                // Agudos muy abiertos
                diff.issues.push_back(makeIssue(
                    SemanticIssue::Severity::Info, SemanticIssue::Domain::StereoWidth,
                    diff.trackName, diff.role,
                    "Ancho estéreo amplio en " + juce::String(bandNames[b])
                    + ": " + juce::String(p.stereoWidthPerBand[b], 2),
                    p.stereoWidthPerBand[b], e.stereoWidthMax[b],
                    "Verifica compatibilidad mono — el stereo extremo puede causar "
                    "cancelacion de fase"));
            }
        }
    }

    if (!p.isMonoCompatible) {
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Info, SemanticIssue::Domain::MonoCompat,
            diff.trackName, diff.role,
            "Ancho estéreo promedio " + juce::String(p.avgStereoWidth, 2)
            + " — puede sonar debil en mono",
            p.avgStereoWidth, 0.3f,
            "Usa correlacion Mid-Side para mantener energia en mono"));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CHECK: Transient
// ═══════════════════════════════════════════════════════════════════════════
void SemanticComparator::checkTransient(SemanticDiff& diff,
                                         const TrackSpectralProfile& p,
                                         const ExpectedProfile& e)
{
    if (p.transientRatio <= 0.0f) return;

    if (p.transientRatio > e.transientRatioMax) {
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Info, SemanticIssue::Domain::Transient,
            diff.trackName, diff.role,
            "Transiente ratio " + juce::String(p.transientRatio, 2)
            + " — muy agresivo para un " + roleDisplayName(diff.role)
            + " (max: " + juce::String(e.transientRatioMax, 1) + ")",
            p.transientRatio, e.transientRatioMax,
            "Usa un transient shaper con attack mas lento (10-20ms) "
            "o compresor con attack rapido (1ms) para controlar el pico"));
    }
    else if (p.transientRatio > 0.0f && p.transientRatio < e.transientRatioMin) {
        diff.issues.push_back(makeIssue(
            SemanticIssue::Severity::Info, SemanticIssue::Domain::Transient,
            diff.trackName, diff.role,
            "Transiente ratio " + juce::String(p.transientRatio, 2)
            + " — muy bajo, el ataque se pierde "
            + "(target: > " + juce::String(e.transientRatioMin, 1) + ")",
            p.transientRatio, e.transientRatioMin,
            "Usa un transient shaper con attack mas rapido o "
            "reduce el release del compresor"));
    }
    else if (p.transientRatio >= e.transientRatioMin
             && p.transientRatio <= e.transientRatioMax) {
        diff.praises.push_back(makePraise(
            SemanticIssue::Severity::Praise, SemanticIssue::Domain::Transient,
            diff.trackName, diff.role,
            "Ataque natural: transient ratio "
            + juce::String(p.transientRatio, 2)));
    }
}

} // namespace mixcoach

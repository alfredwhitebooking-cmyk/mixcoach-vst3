#include "ReferenceDrivenEngine.h"
#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"

#include <algorithm>
#include <cmath>

namespace mixcoach {

    // Colores para severityColour() - constantes locales para evitar dependencia engine→UI (Ley 6)
    static const juce::Colour kMatchColours[4] = {
        juce::Colour(0xFFFF5252), // Red   - Critical
        juce::Colour(0xFFFFC107), // Amber - Warning
        juce::Colour(0xFF00B7FF), // Cyan  - Info
        juce::Colour(0xFF4CAF50), // Green - Praise
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Formats
    // ═══════════════════════════════════════════════════════════════════════════

    static juce::String fmtDb(float value)
    {
        if (value < -90.0f) return "-inf dB";
        return juce::String(value, 1) + " dB";
    }

    static juce::String fmtLUFS(float value)
    {
        if (value < -90.0f) return "-inf LUFS";
        return juce::String(value, 1) + " LUFS";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DomainGap helpers
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String DomainGap::toTextSummary() const
    {
        juce::String s;
        s += ReferenceDrivenEngine::severityLabel(severity);
        s += juce::String(" [") + ReferenceDrivenEngine::domainName(domain) + "] ";

        switch (domain) {
            case Domain::Gain:
                s += "Nivel: " + metric + " " + fmtLUFS(actualValue) + " vs ref " + fmtLUFS(targetValue) + " (gap "
                     + juce::String(std::abs(gap), 1) + " LUFS)";
                break;
            case Domain::Tonal:
                s += metric + ": " + fmtDb(actualValue) + " vs ref " + fmtDb(targetValue) + " (gap "
                     + juce::String(std::abs(gap), 1) + " dB)";
                break;
            case Domain::Dynamics:
                s += metric + ": " + juce::String(actualValue, 1) + " dB" + " vs ref " + juce::String(targetValue, 1)
                     + " dB" + " (gap " + juce::String(std::abs(gap), 1) + " dB)";
                break;
            case Domain::Spatial:
                s += metric + ": " + juce::String(actualValue, 2) + " vs ref " + juce::String(targetValue, 2);
                break;
            case Domain::Loudness:
                s += metric + ": " + juce::String(actualValue, 1) + " LU" + " vs ref " + juce::String(targetValue, 1)
                     + " LU" + " (gap " + juce::String(std::abs(gap), 1) + " LU)";
                break;
        }

        s += " — " + description;
        return s;
    }

    juce::String DomainGap::toShortLabel() const
    {
        juce::String s;
        switch (severity) {
            case GapSeverity::Critical:
                s += "\xF0\x9F\x94\xB4 ";
                break; // 🔴
            case GapSeverity::Warning:
                s += "\xF0\x9F\x9F\xA1 ";
                break; // 🟡
            case GapSeverity::Info:
                s += "\xF0\x9F\x94\xB5 ";
                break; // 🔵
            case GapSeverity::Praise:
                s += "\xE2\x9C\x85 ";
                break; // ✅
        }
        s += juce::String(ReferenceDrivenEngine::domainName(domain)) + ": " + description;
        return s;
    }

    juce::String DomainGap::toLLMContextGap() const
    {
        juce::String s;

        // Severidad como texto
        switch (severity) {
            case GapSeverity::Critical:
                s += "[CRITICAL] ";
                break;
            case GapSeverity::Warning:
                s += "[WARNING]  ";
                break;
            case GapSeverity::Info:
                s += "[INFO]     ";
                break;
            case GapSeverity::Praise:
                s += "[OK]       ";
                break;
        }

        s += juce::String(ReferenceDrivenEngine::domainName(domain)) + ": " + metric;

        // Valores numéricos con unidades según dominio
        switch (domain) {
            case Domain::Gain:
                s += " | actual=" + juce::String(actualValue, 1) + " LUFS" + " target=" + juce::String(targetValue, 1)
                     + " LUFS" + " gap=" + juce::String(gap, 1) + " LUFS";
                break;
            case Domain::Tonal:
                s += " | actual=" + juce::String(actualValue, 1) + " dB" + " target=" + juce::String(targetValue, 1)
                     + " dB" + " gap=" + juce::String(gap, 1) + " dB"
                     + (frequencyHint.isNotEmpty() ? (" @ " + frequencyHint) : "");
                break;
            case Domain::Dynamics:
                s += " | actual=" + juce::String(actualValue, 1) + " dB" + " target=" + juce::String(targetValue, 1)
                     + " dB" + " gap=" + juce::String(gap, 1) + " dB";
                break;
            case Domain::Spatial:
                s += " | actual=" + juce::String(actualValue, 2) + " target=" + juce::String(targetValue, 2)
                     + " gap=" + juce::String(gap, 2);
                break;
            case Domain::Loudness:
                s += " | actual=" + juce::String(actualValue, 1) + " LU" + " target=" + juce::String(targetValue, 1)
                     + " LU" + " gap=" + juce::String(gap, 1) + " LU";
                break;
        }

        s += "\n    " + description;
        if (suggestion.isNotEmpty()) s += "\n    -> " + suggestion;

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Static helpers
    // ═══════════════════════════════════════════════════════════════════════════

    const char* ReferenceDrivenEngine::domainName(Domain d) noexcept
    {
        switch (d) {
            case Domain::Gain:
                return "GANANCIA";
            case Domain::Tonal:
                return "TONAL";
            case Domain::Dynamics:
                return "DINAMICA";
            case Domain::Spatial:
                return "ESPACIAL";
            case Domain::Loudness:
                return "LOUDNESS";
        }
        return "?";
    }

    const char* ReferenceDrivenEngine::severityLabel(GapSeverity s) noexcept
    {
        switch (s) {
            case GapSeverity::Critical:
                return "\xF0\x9F\x94\xB4 CRITICAL "; // 🔴
            case GapSeverity::Warning:
                return "\xF0\x9F\x9F\xA1 WARNING  "; // 🟡
            case GapSeverity::Info:
                return "\xF0\x9F\x94\xB5 Info     "; // 🔵
            case GapSeverity::Praise:
                return "\xE2\x9C\x85 Praise    "; // ✅
        }
        return "?";
    }

    juce::Colour ReferenceDrivenEngine::severityColour(GapSeverity s) noexcept
    {
        switch (s) {
            case GapSeverity::Critical:
                return kMatchColours[0]; // Red
            case GapSeverity::Warning:
                return kMatchColours[1]; // Yellow
            case GapSeverity::Info:
                return kMatchColours[2]; // Cyan
            case GapSeverity::Praise:
                return kMatchColours[3]; // Green
        }
        return juce::Colours::grey;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Region helpers
    // ═══════════════════════════════════════════════════════════════════════════

    const char* ReferenceDrivenEngine::regionName(int regionIdx) noexcept
    {
        static const char* names[] = {"Sub", "Bass", "Low-Mid", "High-Mid", "Presence", "Air"};
        return (regionIdx >= 0 && regionIdx < 6) ? names[regionIdx] : "?";
    }

    const char* ReferenceDrivenEngine::regionCenterFreq(int regionIdx) noexcept
    {
        static const char* freq[] = {"60Hz", "200Hz", "600Hz", "2kHz", "5kHz", "12kHz"};
        return (regionIdx >= 0 && regionIdx < 6) ? freq[regionIdx] : "?";
    }

    const char* ReferenceDrivenEngine::regionFreqRange(int regionIdx) noexcept
    {
        static const char* ranges[] = {
            "0-86Hz", "86-301Hz", "301-1076Hz", "1076-3532Hz", "3532-8355Hz", "8355-16458Hz"};
        return (regionIdx >= 0 && regionIdx < 6) ? ranges[regionIdx] : "?";
    }

    float ReferenceDrivenEngine::regionEnergy(const float bandEnergies[30], int regionStartBand, int regionEndBand)
    {
        double sum = 0.0;
        int count  = 0;
        int end    = std::min(regionEndBand, 30);
        for (int b = regionStartBand; b < end; ++b) {
            if (bandEnergies[b] > -90.0f) {
                sum += static_cast<double>(bandEnergies[b]);
                ++count;
            }
        }
        return (count > 0) ? static_cast<float>(sum / count) : -100.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  New region-specific helpers for actionable suggestions
    // ═══════════════════════════════════════════════════════════════════════════

    const char* ReferenceDrivenEngine::regionFilterType(int regionIdx) noexcept
    {
        static const char* types[] = {
            "Low Shelf",  // Sub — shelving para graves profundos
            "Low Shelf",  // Bass — shelving para cuerpo
            "Bell",       // Low-Mid — campana para medios
            "Bell",       // High-Mid — campana precisa
            "High Shelf", // Presence — shelving para presencia
            "High Shelf"  // Air — shelving para brillo
        };
        return (regionIdx >= 0 && regionIdx < 6) ? types[regionIdx] : "Bell";
    }

    float ReferenceDrivenEngine::regionQ(int regionIdx) noexcept
    {
        // Q más amplio en extremos (Sub, Air), más preciso en medios
        static const float qValues[] = {
            0.7f, // Sub — ancho, para no crear resonancias
            0.8f, // Bass — semi-ancho
            1.2f, // Low-Mid — moderado
            1.8f, // High-Mid — preciso, evitar colorear vocales
            1.2f, // Presence — moderado
            0.7f  // Air — ancho, suave
        };
        return (regionIdx >= 0 && regionIdx < 6) ? qValues[regionIdx] : 1.0f;
    }

    const char* ReferenceDrivenEngine::regionInstrumentTarget(int regionIdx) noexcept
    {
        static const char* targets[] = {
            "Bombo/Sub-bass",           // Sub
            "Bajo/Guitarra Grave",      // Bass
            "Guitarras/Pads/Teclados",  // Low-Mid
            "Voces/Snares/Guitarras L", // High-Mid
            "Voces/Hi-Hats/Platos",     // Presence
            "Platos/Aire/Ambiente"      // Air
        };
        return (regionIdx >= 0 && regionIdx < 6) ? targets[regionIdx] : "?";
    }

    float ReferenceDrivenEngine::suggestedEqGainDb(float gapDb, int regionIdx) noexcept
    {
        float absGap = std::abs(gapDb);
        bool isBoost = gapDb > 0; // mix > ref, need to cut (bring down)

        // En Sub y Air: aplicar máximo 60% del gap para evitar artefactos
        float factor = (regionIdx == 0 || regionIdx == 5) ? 0.5f : 0.6f;

        // Para gaps muy grandes (> 8dB), aplicar en múltiplos de 3dB
        // El usuario debe escuchar y ajustar progresivamente
        if (absGap > 8.0f) factor = 0.4f;

        float suggested = absGap * factor;

        // Redondear al 0.5dB más cercano para que sea un número práctico
        suggested = std::round(suggested * 2.0f) / 2.0f;
        if (suggested < 0.5f && absGap >= 1.0f) suggested = 0.5f;
        if (suggested < 0.3f) suggested = 0.0f; // menos de 0.3dB no vale la pena

        return isBoost ? suggested : -suggested;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeGaps — Método principal
    //  Orden de prioridad: Gain (1) > Tonal (2) > Dynamics (3) > Spatial (4) > Loudness (5)
    // ═══════════════════════════════════════════════════════════════════════════

    std::vector<DomainGap> ReferenceDrivenEngine::computeGaps(const ReferenceFingerprint& fp,
                                                              const AudioAnalyzer& master,
                                                              const juce::String& genre)
    {
        juce::ignoreUnused(genre);
        std::vector<DomainGap> gaps;

        if (!fp.valid) return gaps; // No reference data — return empty

        // ─── 1. Gain domain (priority 1) ────────────────────────────────────
        {
            auto gap     = analyzeGainDomain(fp, master);
            gap.priority = 1;
            gaps.push_back(gap);
        }

        // ─── 2. Tonal domain (priority 2) — can produce multiple gaps ───────
        {
            auto tonalGaps = analyzeTonalDomain(fp, master);
            for (auto& g : tonalGaps) {
                g.priority = 2;
                gaps.push_back(g);
            }
        }

        // ─── 3. Dynamics domain (priority 3) ────────────────────────────────
        {
            auto gap     = analyzeDynamicsDomain(fp, master);
            gap.priority = 3;
            gaps.push_back(gap);
        }

        // ─── 4. Spatial domain (priority 4) ─────────────────────────────────
        {
            auto gap     = analyzeSpatialDomain(fp, master);
            gap.priority = 4;
            gaps.push_back(gap);
        }

        // ─── 5. Loudness Range domain (priority 5) ─────────────────────────
        {
            auto gap     = analyzeLoudnessRangeDomain(fp, master);
            gap.priority = 5;
            gaps.push_back(gap);
        }

        // ─── Ordenar: por prioridad ascendente, luego por severidad ─────────
        // Critical antes que Warning, etc.
        std::stable_sort(gaps.begin(), gaps.end(), [](const DomainGap& a, const DomainGap& b) {
            if (a.priority != b.priority) return a.priority < b.priority;
            // Misma prioridad — Critical primero
            return static_cast<int>(a.severity) < static_cast<int>(b.severity);
        });

        // Filtrar gaps con severity = Praise si hay problemas más graves
        // (no tiene sentido decir "el nivel está perfecto" si el Sub está 6dB mal)
        bool hasAnyProblem = false;
        for (const auto& g : gaps) {
            if (g.severity == GapSeverity::Critical || g.severity == GapSeverity::Warning) {
                hasAnyProblem = true;
                break;
            }
        }
        // Quitar Praise gaps solo si hay problemas activos y son en el mismo dominio
        // (son relevantes para dar feedback positivo al usuario)
        // En realidad — dejamos todos, el consumidor decide qué mostrar

        LogHelper::writeToLog("[ReferenceDrivenEngine] " + juce::String((int)gaps.size())
                              + " gaps generados (ref: " + juce::String(fp.lufsIntegrated, 1) + " LUFS)");

        return gaps;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeGainDomain — Compara LUFS Integrated + True Peak
    //  Pregunta: ¿Mi mezcla suena tan fuerte como la referencia?
    // ═══════════════════════════════════════════════════════════════════════════

    DomainGap ReferenceDrivenEngine::analyzeGainDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master)
    {
        DomainGap g;
        g.domain = Domain::Gain;

        float mixIntegrated = master.getIntegratedLUFS();
        float refIntegrated = fp.lufsIntegrated;
        float gapLUFS       = mixIntegrated - refIntegrated; // positivo = mix más fuerte

        g.metric      = "LUFS Integrated";
        g.actualValue = mixIntegrated;
        g.targetValue = refIntegrated;
        g.gap         = gapLUFS;

        // Normalised gap: cuánto del threshold crítico hemos alcanzado
        float absGap    = std::abs(gapLUFS);
        g.normalisedGap = std::min(1.0f, absGap / kLufsCriticalThreshold);

        // Severidad
        if (absGap >= kLufsCriticalThreshold) {
            g.severity = GapSeverity::Critical;
        }
        else if (absGap >= kLufsWarningThreshold) {
            g.severity = GapSeverity::Warning;
        }
        else if (absGap >= 0.5f) {
            g.severity = GapSeverity::Info;
        }
        else {
            g.severity = GapSeverity::Praise;
        }

        // Descripción y sugerencia
        if (gapLUFS > 0) {
            g.description = "La mezcla esta " + juce::String(absGap, 1) + " LUFS MAS FUERTE que la referencia";
            if (absGap >= 6.0f) {
                // Gap grande: sugerir reducción escalonada
                g.suggestion =
                    "Baja " + juce::String(absGap * 0.5f, 1) + " dB el fader master primero y vuelve a escuchar. "
                    + "Luego ajusta el limitador: reduce " + juce::String(absGap * 0.3f, 1)
                    + " dB de gain reduction. Objetivo: " + juce::String(fp.lufsIntegrated, 1) + " LUFS integrated.";
            }
            else {
                g.suggestion = "Baja " + juce::String(absGap, 1) + " dB el fader master y verifica con el LUFS meter. "
                               + "Objetivo: " + juce::String(fp.lufsIntegrated, 1) + " LUFS integrated.";
            }
            g.actionVerb = "reducir";
        }
        else {
            g.description = "La mezcla est\xC3\xA1" " " + juce::String(absGap, 1)
                        + " LUFS M\xC3\x81" "S QUIETA que la referencia";
            if (absGap >= 6.0f) {
                g.suggestion = "Sube " + juce::String(absGap * 0.6f, 1)
                           + " dB el fader master primero. Si el master clipea, "
                           + "reduce compresi\xC3\xB3" "n 2:1 threshold -20dB en el bus master. "
                           + "Objetivo: " + juce::String(fp.lufsIntegrated, 1) + " LUFS integrated.";
            }
            else {
                g.suggestion = "Sube " + juce::String(absGap, 1) + " dB el fader master o agrega "
                               + juce::String(absGap * 0.3f, 1) + " dB de makeup gain en el compresor del master. "
                               + "Objetivo: " + juce::String(fp.lufsIntegrated, 1) + " LUFS integrated.";
            }
            g.actionVerb = "subir";
        }

        // True Peak check
        float mixTP = master.getTruePeakDBTP();
        float refTP = fp.truePeakDBTP;

        if (mixTP > -1.0f && refTP < -3.0f) {
            // Mix has near-clipping but reference doesn't — upgrade severity
            if (g.severity == GapSeverity::Info) g.severity = GapSeverity::Warning;
            g.description += " y el True Peak (" + juce::String(mixTP, 1)
                         + " dBTP) est\xC3\xA1" " muy cerca del l\xC3\xAD" "mite digital vs ref "
                         + juce::String(refTP, 1) + " dBTP";
            g.suggestion +=
                ". Adem\xC3\xA1"
                "s, deja al menos 1dB de headroom en el master";
        }

        return g;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeTonalDomain — Compara 6 regiones espectrales
    //  Pregunta: ¿El balance de frecuencias de mi mezcla coincide con la ref?
    //  Produce hasta 2 gaps: la región con mayor exceso y la región con mayor defecto
    // ═══════════════════════════════════════════════════════════════════════════

    std::vector<DomainGap> ReferenceDrivenEngine::analyzeTonalDomain(const ReferenceFingerprint& fp,
                                                                     const AudioAnalyzer& master)
    {
        std::vector<DomainGap> gaps;

        // Extraer 30-band spectrum del master actual
        const auto& masterAnalysis = master.getMasterAnalysis();
        const float* spectrum      = masterAnalysis.getSpectrum();
        if (spectrum == nullptr || masterAnalysis.getLastUpdateTime() == 0) return gaps;

        float mixBands[30];
        for (int b = 0; b < 30; ++b) {
            double sum   = 0.0;
            int count    = 0;
            int startBin = kSpectralBandBins[b][0];
            int endBin   = std::min(kSpectralBandBins[b][1], kNumSpectrumBins);
            for (int i = startBin; i < endBin; ++i) {
                float mag = spectrum[i];
                if (mag > 1e-10f) sum += 20.0 * std::log10(static_cast<double>(mag));
                else
                    sum += -100.0;
                ++count;
            }
            mixBands[b] = (count > 0) ? static_cast<float>(sum / count) : -100.0f;
        }

        // Calcular offset de nivel general para alinear
        double refSum = 0.0, mixSum = 0.0;
        int refCount = 0, mixCount = 0;
        for (int b = 0; b < 30; ++b) {
            if (fp.bandEnergies[b] > -80.0f) {
                refSum += fp.bandEnergies[b];
                refCount++;
            }
            if (mixBands[b] > -80.0f) {
                mixSum += mixBands[b];
                mixCount++;
            }
        }
        float refAvg      = (refCount > 0) ? static_cast<float>(refSum / refCount) : -100.0f;
        float mixAvg      = (mixCount > 0) ? static_cast<float>(mixSum / mixCount) : -100.0f;
        float levelOffset = refAvg - mixAvg; // Cuanto más fuerte es la ref en general

        // Comparar cada región
        struct RegionDiff
        {
            int regionIdx;
            float diff; // mix - ref (positivo = mix tiene más energía)
            float absDiff;
        };

        std::vector<RegionDiff> regionDiffs;

        for (int r = 0; r < 6; ++r) {
            float refRegion = regionEnergy(fp.bandEnergies, kRegionBands[r][0], kRegionBands[r][1]);
            float mixRegion = regionEnergy(mixBands, kRegionBands[r][0], kRegionBands[r][1]);

            if (refRegion < -80.0f || mixRegion < -80.0f) continue; // Sin datos en esta región

            // Aplicar level offset para comparación significativa
            float mixNormalised = mixRegion + levelOffset;
            float diff          = mixNormalised - refRegion;

            regionDiffs.push_back({r, diff, std::abs(diff)});
        }

        if (regionDiffs.empty()) return gaps;

        // Ordenar por magnitud de diferencia descendente
        std::sort(regionDiffs.begin(), regionDiffs.end(), [](const RegionDiff& a, const RegionDiff& b) {
            return a.absDiff > b.absDiff;
        });

        // Tomar la región con MAYOR exceso (mix > ref) y la región con MAYOR defecto (mix < ref)
        RegionDiff* worstExcess  = nullptr;
        RegionDiff* worstDeficit = nullptr;

        for (auto& rd : regionDiffs) {
            if (rd.diff > 0 && rd.absDiff >= kSpectralWarningDb) {
                if (worstExcess == nullptr || rd.absDiff > worstExcess->absDiff) worstExcess = &rd;
            }
            else if (rd.diff < 0 && rd.absDiff >= kSpectralWarningDb) {
                if (worstDeficit == nullptr || rd.absDiff > worstDeficit->absDiff) worstDeficit = &rd;
            }
        }

        // Crear gap para exceso
        if (worstExcess != nullptr) {
            DomainGap g;
            g.domain = Domain::Tonal;
            g.metric =
                juce::String(regionName(worstExcess->regionIdx)) + " (" + regionFreqRange(worstExcess->regionIdx) + ")";

            float refRegion = regionEnergy(
                fp.bandEnergies, kRegionBands[worstExcess->regionIdx][0], kRegionBands[worstExcess->regionIdx][1]);
            float mixRegion = regionEnergy(
                mixBands, kRegionBands[worstExcess->regionIdx][0], kRegionBands[worstExcess->regionIdx][1]);

            g.actualValue = mixRegion;
            g.targetValue = refRegion;
            g.gap         = worstExcess->diff;

            float absGap    = worstExcess->absDiff;
            g.normalisedGap = std::min(1.0f, absGap / kSpectralCriticalDb);

            if (absGap >= kSpectralCriticalDb) g.severity = GapSeverity::Critical;
            else
                g.severity = GapSeverity::Warning;

            float eqGain  = suggestedEqGainDb(-absGap, worstExcess->regionIdx); // negativo = reducir
            g.description = juce::String(regionName(worstExcess->regionIdx)) + " esta " + juce::String(absGap, 1)
                            + " dB por ENCIMA de la referencia";
            if (std::abs(eqGain) >= 0.5f) {
                g.suggestion = "Reduce " + juce::String(eqGain, 1) + " dB en "
                               + juce::String(regionCenterFreq(worstExcess->regionIdx)) + " con un EQ "
                               + juce::String(regionFilterType(worstExcess->regionIdx))
                               + " (Q=" + juce::String(regionQ(worstExcess->regionIdx), 1) + ")" + " en los "
                               + juce::String(regionInstrumentTarget(worstExcess->regionIdx));
            }
            else {
                g.suggestion = "Atenuacion sutil (<0.5dB) en " + juce::String(regionCenterFreq(worstExcess->regionIdx))
                               + " con EQ " + juce::String(regionFilterType(worstExcess->regionIdx))
                               + " (Q=" + juce::String(regionQ(worstExcess->regionIdx), 1) + ")" + " en los "
                               + juce::String(regionInstrumentTarget(worstExcess->regionIdx));
            }
            g.frequencyHint = regionCenterFreq(worstExcess->regionIdx);
            g.actionVerb    = "reducir";
            gaps.push_back(g);
        }

        // Crear gap para defecto
        if (worstDeficit != nullptr) {
            DomainGap g;
            g.domain = Domain::Tonal;
            g.metric = juce::String(regionName(worstDeficit->regionIdx)) + " ("
                       + regionFreqRange(worstDeficit->regionIdx) + ")";

            float refRegion = regionEnergy(
                fp.bandEnergies, kRegionBands[worstDeficit->regionIdx][0], kRegionBands[worstDeficit->regionIdx][1]);
            float mixRegion = regionEnergy(
                mixBands, kRegionBands[worstDeficit->regionIdx][0], kRegionBands[worstDeficit->regionIdx][1]);

            g.actualValue = mixRegion;
            g.targetValue = refRegion;
            g.gap         = worstDeficit->diff;

            float absGap    = worstDeficit->absDiff;
            g.normalisedGap = std::min(1.0f, absGap / kSpectralCriticalDb);

            if (absGap >= kSpectralCriticalDb) g.severity = GapSeverity::Critical;
            else
                g.severity = GapSeverity::Warning;

            float eqGain  = suggestedEqGainDb(absGap, worstDeficit->regionIdx); // positivo = subir
            g.description = juce::String(regionName(worstDeficit->regionIdx)) + " esta " + juce::String(absGap, 1)
                            + " dB por DEBAJO de la referencia";
            if (std::abs(eqGain) >= 0.5f) {
                g.suggestion = "Sube " + juce::String(eqGain, 1) + " dB en "
                               + juce::String(regionCenterFreq(worstDeficit->regionIdx)) + " con un EQ "
                               + juce::String(regionFilterType(worstDeficit->regionIdx))
                               + " (Q=" + juce::String(regionQ(worstDeficit->regionIdx), 1) + ")" + " en "
                               + juce::String(regionInstrumentTarget(worstDeficit->regionIdx));
            }
            else {
                g.suggestion = "Aumento sutil (<0.5dB) en " + juce::String(regionCenterFreq(worstDeficit->regionIdx))
                               + " con EQ " + juce::String(regionFilterType(worstDeficit->regionIdx))
                               + " (Q=" + juce::String(regionQ(worstDeficit->regionIdx), 1) + ")" + " en "
                               + juce::String(regionInstrumentTarget(worstDeficit->regionIdx));
            }
            g.frequencyHint = regionCenterFreq(worstDeficit->regionIdx);
            g.actionVerb    = "subir";
            gaps.push_back(g);
        }

        // Si no hay gaps significativos, crear uno de Praise
        if (gaps.empty()) {
            DomainGap g;
            g.domain        = Domain::Tonal;
            g.severity      = GapSeverity::Praise;
            g.metric        = "Spectral Balance";
            g.actualValue   = 0;
            g.targetValue   = 0;
            g.gap           = 0;
            g.normalisedGap = 0;
            g.description   = "Balance tonal alineado con la referencia — todas las regiones dentro de ±"
                              + juce::String(kSpectralWarningDb, 1) + " dB";
            g.suggestion    = "El balance espectral ya coincide con la referencia. Pasa a ajustar dinámica.";
            g.actionVerb    = "mantener";
            gaps.push_back(g);
        }

        return gaps;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeDynamicsDomain — Compara Crest Factor
    //  Pregunta: ¿La dinámica de mi mezcla es similar a la referencia?
    //  Crest bajo = comprimido, Crest alto = dinámico
    // ═══════════════════════════════════════════════════════════════════════════

    DomainGap ReferenceDrivenEngine::analyzeDynamicsDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master)
    {
        DomainGap g;
        g.domain = Domain::Dynamics;

        // Calcular crest del master actual
        const auto& left  = master.getLeftAnalysis();
        const auto& right = master.getRightAnalysis();
        float peakL       = left.getPeak();
        float peakR       = right.getPeak();
        float rmsL        = left.getRMS();
        float rmsR        = right.getRMS();

        float masterPeak = juce::jmax(peakL, peakR);
        float masterRMS  = juce::jmax(rmsL, rmsR);

        float mixCrest = 0.0f;
        if (masterRMS > -60.0f && masterPeak > -60.0f) mixCrest = masterPeak - masterRMS;

        float refCrest = fp.crestFactor;
        float crestGap = mixCrest - refCrest; // positivo = mix más dinámico

        g.metric      = "Crest Factor";
        g.actualValue = mixCrest;
        g.targetValue = refCrest;
        g.gap         = crestGap;

        float absGap    = std::abs(crestGap);
        g.normalisedGap = (kCrestCriticalDb > 0.0f) ? std::min(1.0f, absGap / kCrestCriticalDb) : 0.0f;

        if (absGap >= kCrestCriticalDb) {
            g.severity = GapSeverity::Critical;
        }
        else if (absGap >= kCrestWarningDb) {
            g.severity = GapSeverity::Warning;
        }
        else if (absGap >= 1.0f) {
            g.severity = GapSeverity::Info;
        }
        else {
            g.severity = GapSeverity::Praise;
        }

        if (crestGap > 0) {
            g.description = "La mezcla es " + juce::String(absGap, 1)
                        + " dB M\xC3\x81" "S DIN\xC3\x81" "MICA que la referencia (crest "
                        + juce::String(mixCrest, 1) + "dB vs ref " + juce::String(refCrest, 1) + "dB)";
            if (absGap >= 6.0f) {
                g.suggestion =
                    "Agrega compresi\xC3\xB3"
                    "n: ratio 3:1, threshold -24dB, "
                    "attack 5ms, release 60ms en los buses de bater\xC3\xAD"
                    "a y bajo. "
                    "Objetivo: crest de "
                    + juce::String(refCrest, 1) + " dB.";
            }
            else {
                g.suggestion =
                    "Agrega compresi\xC3\xB3"
                    "n suave: ratio 2:1, threshold -18dB, "
                    "attack 10ms, release 80ms en los buses m\xC3\xA1"
                    "s din\xC3\xA1"
                    "micos. "
                    "Objetivo: crest de "
                    + juce::String(refCrest, 1) + " dB.";
            }
            g.actionVerb = "comprimir";
        }
        else {
            g.description = "La mezcla es " + juce::String(absGap, 1)
                        + " dB M\xC3\x81" "S COMPRIMIDA que la referencia (crest "
                        + juce::String(mixCrest, 1) + "dB vs ref " + juce::String(refCrest, 1) + "dB)";
            if (absGap >= 6.0f) {
                g.suggestion =
                    "Reduce compresi\xC3\xB3"
                    "n agresiva en buses: sube el threshold a -12dB, "
                    "baja el ratio a 1.5:1, attack 20ms, release 120ms. "
                    "Objetivo: crest de "
                    + juce::String(refCrest, 1) + " dB.";
            }
            else {
                g.suggestion =
                    "Afloja la compresi\xC3\xB3"
                    "n: prueba ratio 1.5:1, "
                    "threshold -20dB, attack 20ms, release 100ms. "
                    "Objetivo: crest de "
                    + juce::String(refCrest, 1) + " dB.";
            }
            g.actionVerb = "expandir";
        }

        return g;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeSpatialDomain — Compara correlación estéreo
    //  Pregunta: ¿El ancho estéreo de mi mezcla es similar a la referencia?
    // ═══════════════════════════════════════════════════════════════════════════

    DomainGap ReferenceDrivenEngine::analyzeSpatialDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master)
    {
        DomainGap g;
        g.domain = Domain::Spatial;

        float mixCorr = master.getMasterAnalysis().getCorrelation();
        float refCorr = fp.correlation;
        float corrGap = mixCorr - refCorr; // positivo = mix más mono

        g.metric      = "Stereo Correlation";
        g.actualValue = mixCorr;
        g.targetValue = refCorr;
        g.gap         = corrGap;

        float absGap    = std::abs(corrGap);
        g.normalisedGap = (kCorrelationThreshold > 0.0f) ? std::min(1.0f, absGap / kCorrelationThreshold) : 0.0f;

        // Para correlación, valores extremos (muy cerca de 1 o muy cerca de 0) son más graves
        bool mixExtreme = (mixCorr > 0.95f || mixCorr < 0.3f);

        if (absGap >= kCorrelationThreshold && mixExtreme) {
            g.severity = GapSeverity::Critical;
        }
        else if (absGap >= kCorrelationThreshold) {
            g.severity = GapSeverity::Warning;
        }
        else if (absGap >= kCorrelationThreshold * 0.5f) {
            g.severity = GapSeverity::Info;
        }
        else {
            g.severity = GapSeverity::Praise;
        }

        if (mixCorr > refCorr + kCorrelationThreshold) {
            // Mix más mono que la ref
            g.description = "La mezcla tiene correlaci\xC3\xB3" "n " + juce::String(absGap, 2)
                        + " M\xC3\x81" "S ALTA (m\xC3\xA1" "s mono) que la referencia ("
                        + juce::String(mixCorr, 2) + " vs " + juce::String(refCorr, 2) + ")";
            if (mixCorr > 0.85f) {
                g.suggestion =
                    "La mezcla es casi mono! Abre el est\xC3\xA9"
                    "reo: aplica un ensanchador "
                    "est\xC3\xA9"
                    "reo (mix 30-40%) en pads/keys. Agrega delay ping-pong "
                    "1/8 nota, feedback 15%, mix 20% en elementos secundarios. "
                    "Verifica que el bajo y bombo queden en mono (< 80Hz)."
                    "\nObjetivo: correlaci\xC3\xB3"
                    "n de "
                    + juce::String(refCorr, 2);
            }
            else {
                g.suggestion =
                    "Abre el est\xC3\xA9"
                    "reo suavemente: prueba ensanchador est\xC3\xA9"
                    "reo "
                    "(mix 20-30%) en pads/keys o delay ping-pong "
                    "1/8 nota, feedback 20%, mix 15% en acompa\xC3\xB1"
                    "amiento. "
                    "\nObjetivo: correlaci\xC3\xB3"
                    "n de "
                    + juce::String(refCorr, 2);
            }
            g.actionVerb = "ensanchar";
        }
        else if (mixCorr < refCorr - kCorrelationThreshold) {
            // Mix más ancho que la ref
            g.description = "La mezcla tiene correlaci\xC3\xB3" "n " + juce::String(absGap, 2)
                        + " M\xC3\x81" "S BAJA (m\xC3\xA1" "s ancha) que la referencia ("
                        + juce::String(mixCorr, 2) + " vs " + juce::String(refCorr, 2) + ")";
            if (mixCorr < 0.3f) {
                g.suggestion =
                    "PELIGRO: correlaci\xC3\xB3"
                    "n muy baja! Problemas de fase probables. "
                    "Revisa fase en kick y bass con un correlaci\xC3\xB3"
                    "nmetro. "
                    "Prueba M/S EQ: reduce ancho est\xC3\xA9"
                    "reo en 200-800Hz. "
                    "\nObjetivo: correlaci\xC3\xB3"
                    "n de "
                    + juce::String(refCorr, 2);
            }
            else {
                g.suggestion =
                    "Reduce est\xC3\xA9"
                    "reo widening en medios (400Hz-3kHz). "
                    "Verifica compatibilidad mono. "
                    "Prueba un correlaci\xC3\xB3"
                    "nmetro en el master para identificar "
                    "pistas con fase problem\xC3\xA1"
                    "tica."
                    "\nObjetivo: correlaci\xC3\xB3"
                    "n de "
                    + juce::String(refCorr, 2);
            }
            g.actionVerb = "centrar";
        }
        else {
            g.severity = GapSeverity::Praise;
            g.description =
                "Ancho est\xC3\xA9"
                "reo similar a la referencia ("
                + juce::String(mixCorr, 2) + " vs " + juce::String(refCorr, 2) + ")";
            g.suggestion =
                "El balance est\xC3\xA9"
                "reo ya est\xC3\xA1"
                " alineado con la referencia. "
                "Contin\xC3\xBA"
                "a con el siguiente paso.";
            g.actionVerb = "mantener";
        }

        return g;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeLoudnessRangeDomain — Compara Loudness Range (LRA)
    //  Pregunta: ¿La variación de volumen de mi mezcla es similar a la ref?
    // ═══════════════════════════════════════════════════════════════════════════

    DomainGap ReferenceDrivenEngine::analyzeLoudnessRangeDomain(const ReferenceFingerprint& fp,
                                                                const AudioAnalyzer& master)
    {
        DomainGap g;
        g.domain = Domain::Loudness;

        float mixRange = master.getLoudnessRange();
        float refRange = fp.lufsRange;
        float rangeGap = mixRange - refRange; // positivo = mix más variable

        g.metric      = "Loudness Range (LRA)";
        g.actualValue = mixRange;
        g.targetValue = refRange;
        g.gap         = rangeGap;

        float absGap    = std::abs(rangeGap);
        g.normalisedGap = (kLoudnessRangeCritical > 0.0f) ? std::min(1.0f, absGap / kLoudnessRangeCritical) : 0.0f;

        if (absGap >= kLoudnessRangeCritical) {
            g.severity = GapSeverity::Critical;
        }
        else if (absGap >= kLoudnessRangeWarning) {
            g.severity = GapSeverity::Warning;
        }
        else if (absGap >= 1.0f) {
            g.severity = GapSeverity::Info;
        }
        else {
            g.severity = GapSeverity::Praise;
        }

        if (rangeGap > 0) {
            g.description = "El rango de loudness es " + juce::String(absGap, 1)
                        + " LU M\xC3\x81" "S AMPLIO que la referencia ("
                        + juce::String(mixRange, 1) + " vs " + juce::String(refRange, 1) + " LU)";
            if (absGap >= 6.0f) {
                g.suggestion =
                    "Demasiada variaci\xC3\xB3"
                    "n din\xC3\xA1"
                    "mica. Aplica compresi\xC3\xB3"
                    "n 3:1, "
                    "threshold -28dB, attack 5ms, release 40ms en el bus master. "
                    "Luego compresi\xC3\xB3"
                    "n adicional 2:1, threshold -20dB en los buses "
                    "m\xC3\xA1"
                    "s din\xC3\xA1"
                    "micos. Objetivo: "
                    + juce::String(refRange, 1) + " LU.";
            }
            else {
                g.suggestion =
                    "Aplica compresi\xC3\xB3"
                    "n suave 2:1, threshold -24dB, "
                    "attack 10ms, release 60ms en el bus master. "
                    "Objetivo: "
                    + juce::String(refRange, 1) + " LU.";
            }
            g.actionVerb = "comprimir";
        }
        else {
            g.description = "El rango de loudness es " + juce::String(absGap, 1)
                        + " LU M\xC3\x81" "S ESTRECHO que la referencia ("
                        + juce::String(mixRange, 1) + " vs " + juce::String(refRange, 1) + " LU)";
            if (absGap >= 4.0f) {
                g.suggestion =
                    "Mezcla demasiado comprimida. Reduce compresi\xC3\xB3"
                    "n en el master: "
                    "sube threshold a -12dB, baja ratio a 1.5:1. "
                    "Si usas limitador agresivo, lib\xC3\xA9"
                    "ralo 2-3dB. "
                    "Objetivo: "
                    + juce::String(refRange, 1) + " LU.";
            }
            else {
                g.suggestion =
                    "Afloja la compresi\xC3\xB3"
                    "n del master: prueba threshold -16dB, "
                    "ratio 1.5:1 y libera 1-2dB de gain reduction. "
                    "Objetivo: "
                    + juce::String(refRange, 1) + " LU.";
            }
            g.actionVerb = "expandir";
        }

        return g;
    }

} // namespace mixcoach

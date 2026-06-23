#include "AnalyzerInterpreter.h"
#include "SpectralProfiler.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzerInterpretation
// ═══════════════════════════════════════════════════════════════════════════

juce::String AnalyzerInterpretation::toFullMessage() const
{
    juce::String msg;

    // Emoji según dominio
    switch (domain) {
        case Domain::Correlation:  msg += "\xF0\x9F\x94\xAE "; break; // 🔮
        case Domain::Crest:        msg += "\xE2\x9A\xA1 "; break;     // ⚡
        case Domain::Centroid:     msg += "\xF0\x9F\x8C\xA1\xEF\xB8\x8F "; break; // 🌡️
        case Domain::LUFS:         msg += "\xF0\x9F\x94\x8A "; break; // 🔊
        case Domain::PhaseScope:   msg += "\xF0\x9F\x94\x84 "; break; // 🔄
        case Domain::SpectralBand: msg += "\xF0\x9F\x8E\x9B\xEF\xB8\x8F "; break; // 🎛️
        case Domain::Gain:         msg += "\xF0\x9F\x93\x8A "; break; // 📊
        case Domain::MidSide:      msg += "\xF0\x9F\x94\x80 "; break; // 🔀
        case Domain::StereoWidth:  msg += "\xF0\x9F\x93\x90 "; break; // 📐
    }

    // Interpretación + consecuencia
    msg += "**" + interpretation + "**";
    if (reading.isNotEmpty())
        msg += " (" + reading + ")";
    msg += "\n";

    if (consequence.isNotEmpty())
        msg += "    → " + consequence + "\n";

    if (action.isNotEmpty())
        msg += "    \xF0\x9F\x92\xA1 " + action;

    return msg;
}

BandDiagnostic AnalyzerInterpretation::toBandDiagnostic() const
{
    BandDiagnostic diag;
    diag.lowFreqHz  = lowFreqHz;
    diag.highFreqHz = highFreqHz;
    diag.severity    = severity;
    diag.isCritical  = isCritical;
    diag.isPraise    = isPraise;
    diag.trackName   = trackName;
    diag.trackRole   = trackRole;

    // Construir descripción: "Interpretación: Consecuencia → Acción"
    diag.description = interpretation;
    if (reading.isNotEmpty())
        diag.description += " (" + reading + ")";
    if (consequence.isNotEmpty())
        diag.description += ": " + consequence;
    if (action.isNotEmpty())
        diag.description += " | \xF0\x9F\x92\xA1 " + action;

    return diag;
}

// ═══════════════════════════════════════════════════════════════════════════
//  CORRELATION METER
// ═══════════════════════════════════════════════════════════════════════════

const char* AnalyzerInterpreter::interpretCorrelationLabel(float correlation)
{
    if (correlation >= 0.98f) return "MONO PERFECTO";
    if (correlation >= 0.5f)  return "EST\xC3\x89REO SALUDABLE"; // "ESTÉREO SALUDABLE"
    if (correlation >= 0.3f)  return "EST\xC3\x89REO CONSERVADOR";
    if (correlation >= 0.0f)  return "BAJA CORRELACI\xC3\x93N";
    if (correlation > -0.8f)  return "FASE INVERTIDA";
    return "CANCELACI\xC3\x93N TOTAL"; // "CANCELACIÓN TOTAL"
}

float AnalyzerInterpreter::getSeverityFromCorrelation(float correlation)
{
    if (correlation >= 0.98f) return 0.1f; // Informational
    if (correlation >= 0.5f)  return 0.1f; // Praise / good
    if (correlation >= 0.3f)  return 0.2f; // Slight concern
    if (correlation >= 0.0f)  return 0.5f; // Warning
    if (correlation > -0.8f)  return 0.8f; // Critical
    return 1.0f;                            // Total cancelation
}

AnalyzerInterpretation AnalyzerInterpreter::interpretCorrelation(
    float correlation, const juce::String& genre)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::Correlation;
    correlation = juce::jlimit(-1.0f, 1.0f, correlation);

    interp.reading = "Correlation = " + juce::String(correlation, 2);
    interp.interpretation = interpretCorrelationLabel(correlation);
    interp.severity = getSeverityFromCorrelation(correlation);
    interp.isCritical = (correlation < 0.0f);
    interp.isPraise = (correlation >= 0.5f && correlation < 0.98f);

    // Asignar rango de frecuencia según el tipo de problema de fase
    if (correlation < 0.0f) {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;  // Fase invertida afecta todo el espectro
    } else if (correlation < 0.3f) {
        interp.lowFreqHz = 200.0f;
        interp.highFreqHz = 4000.0f;   // Baja correlación más audible en medios
    } else {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
    }

    // Asignar consecuencia y acción según el rango
    if (correlation >= 0.98f) {
        interp.consequence = "Señal idéntica en L/R — sin imagen estéreo, mezcla angosta";
        interp.action = "Revisa si hay elementos que deberían tener ancho estéreo (pads, reverbs)";
    } else if (correlation >= 0.5f) {
        interp.consequence = "Buena compatibilidad mono con imagen estéreo natural";
        interp.action = "—";
        // Ajustar contexto por género
        juce::String g = genre.toLowerCase().trim();
        if (g == "edm" || g == "electronic" || g == "trap") {
            interp.consequence += " (rango aceptable para " + genre + ")";
        }
    } else if (correlation >= 0.3f) {
        interp.consequence = "Mezcla puede sonar angosta en sistemas estéreo";
        interp.action = "Abre panoramas, agrega delays o reverbs en los lados";
    } else if (correlation >= 0.0f) {
        interp.consequence = "Posible pérdida de definición al reproducirse en mono";
        interp.action = "Revisa si hay wideners estéreo o phase flipping";
    } else if (correlation > -0.8f) {
        interp.consequence = "Cancelación severa en mono — partes de la mezcla desaparecen";
        interp.action = "Revisa polaridad de micrófonos, wideners estéreo, M/S processing";
    } else {
        interp.consequence = "Cancelación total en mono";
        interp.action = "Flip polarity en la pista problemática";
    }

    // Contexto por género para mensajes extra
    juce::String g = genre.toLowerCase().trim();
    if (g.isNotEmpty()) {
        float minCorr = 0.3f; // Default min
        float maxCorr = 0.9f; // Default max
        if (g == "edm" || g == "electronic" || g == "trap") {
            minCorr = 0.2f;
        } else if (g == "jazz" || g == "classical") {
            minCorr = 0.4f;
        } else if (g == "pop") {
            minCorr = 0.5f;
        } else if (g == "podcast" || g == "radio" || g == "speech") {
            minCorr = 0.7f;
        }

        if (correlation < minCorr && correlation >= 0.0f) {
            interp.action += " (para " + genre
                + " se recomienda correlaci\xC3\xB3n > " + juce::String(minCorr, 1) + ")";
        }
    }

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  CREST FACTOR
// ═══════════════════════════════════════════════════════════════════════════

const char* AnalyzerInterpreter::interpretCrestLabel(float crestDb, const juce::String& genre)
{
    juce::ignoreUnused(genre);
    if (crestDb < 4.0f)  return "SOBRECOMPRIMIDO";
    if (crestDb < 6.0f)  return "MUY COMPRIMIDO";
    if (crestDb < 10.0f) return "RANGO DIN\xC3\x81MICO SALUDABLE";
    if (crestDb < 16.0f) return "MUY DIN\xC3\x81MICO";
    return "EXTREMADAMENTE DIN\xC3\x81MICO";
}

float AnalyzerInterpreter::getSeverityFromCrest(float crestDb, const juce::String& genre)
{
    float target = getCrestTarget(genre);
    float diff = std::abs(crestDb - target);

    if (crestDb < 4.0f)  return 1.0f;
    if (crestDb < 6.0f)  return 0.7f;
    if (crestDb < 8.0f)  return diff > 3.0f ? 0.4f : 0.2f;
    if (crestDb < 12.0f) return 0.1f;
    if (crestDb < 16.0f) return 0.4f;
    return 0.6f;
}

bool AnalyzerInterpreter::isCrestCritical(float crestDb, const juce::String& genre)
{
    juce::ignoreUnused(genre);
    return crestDb < 4.0f;
}

float AnalyzerInterpreter::getCrestTarget(const juce::String& genre)
{
    juce::String g = genre.toLowerCase().trim();
    if (g == "pop" || g == "reggaeton")        return 7.0f;
    if (g == "edm" || g == "electronic")       return 5.0f;
    if (g == "hip-hop" || g == "trap" || g == "rap") return 6.0f;
    if (g == "rock")                           return 9.0f;
    if (g == "metal")                          return 5.0f;
    if (g == "r&b" || g == "rnb")              return 8.0f;
    if (g == "jazz")                           return 14.0f;
    if (g == "classical")                      return 16.0f;
    if (g == "latin")                          return 8.0f;
    return 10.0f; // Default
}

juce::String AnalyzerInterpreter::getCrestRangeString(const juce::String& genre)
{
    juce::String g = genre.toLowerCase().trim();
    if (g == "pop")         return "5-8 dB";
    if (g == "edm" || g == "electronic") return "4-7 dB";
    if (g == "trap" || g == "hip-hop")   return "5-8 dB";
    if (g == "rock")        return "8-12 dB";
    if (g == "metal")       return "3-6 dB";
    if (g == "jazz")        return "10-16 dB";
    if (g == "classical")   return "14-20 dB";
    if (g == "r&b")         return "7-10 dB";
    if (g == "latin")       return "6-10 dB";
    return "6-14 dB"; // Default range
}

AnalyzerInterpretation AnalyzerInterpreter::interpretCrest(
    float crestDb, const juce::String& genre)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::Crest;
    crestDb = juce::jlimit(0.0f, 30.0f, crestDb);

    float target = getCrestTarget(genre);
    interp.reading = "Crest = " + juce::String(crestDb, 1) + " dB (target " + juce::String(target, 1) + " dB)";
    interp.interpretation = interpretCrestLabel(crestDb, genre);
    interp.severity = getSeverityFromCrest(crestDb, genre);
    interp.isCritical = isCrestCritical(crestDb, genre);

    // Asignar rango de frecuencia según el crest (la compresión afecta más a ciertas regiones)
    if (crestDb < 4.0f) {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 500.0f;   // Sobre-compresión: más audible en graves
    } else if (crestDb > 18.0f) {
        interp.lowFreqHz = 1000.0f;
        interp.highFreqHz = 10000.0f; // Exceso de dinámica: más audible en presencia
    } else {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
    }

    if (crestDb < 4.0f) {
        interp.consequence = "La mezcla suena aplastada, sin vida ni transientes";
        interp.action = "Reduce ratios de compresi\xC3\xB3n, aumenta attack times";
    } else if (crestDb < 6.0f) {
        interp.consequence = "Fatiga auditiva r\xC3\xA1pida, falta de punch";
        interp.action = "Revisa cadenas de compresi\xC3\xB3n, busca reducir gain reduction";
    } else if (crestDb < 8.0f && crestDb < target - 2.0f) {
        interp.consequence = "Ligeramente comprimido para el g\xC3\xA9nero";
        interp.action = "Monitorea si hay fatiga auditiva despu\xC3\xA9s de 15-20 min";
    } else if (crestDb >= 8.0f && crestDb <= 12.0f) {
        interp.consequence = "Buen balance entre punch y control";
        interp.action = "\xE2\x80\x94";
        interp.isPraise = true;
    } else if (crestDb < 16.0f) {
        interp.consequence = "Dificultad para escuchar a bajo volumen, partes suaves se pierden";
        interp.action = "Agrega compresi\xC3\xB3n suave en tracks m\xC3\xA1s din\xC3\xA1micos";
    } else {
        interp.consequence = "Mezcla no traduce bien a streaming o club";
        interp.action = "Comprime las tracks m\xC3\xA1s din\xC3\xA1micas, usa limitador suave en master";
    }

    // Añadir contexto por género
    if (crestDb < 6.0f && genre.isNotEmpty()) {
        interp.consequence += ". Rango esperado para "
            + genre + ": " + getCrestRangeString(genre);
    }

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  SPECTRAL CENTROID
// ═══════════════════════════════════════════════════════════════════════════

const char* AnalyzerInterpreter::interpretCentroidLabel(float ratio, const juce::String& genre)
{
    juce::ignoreUnused(genre);
    if (ratio >= 1.2f)  return "MEZCLA M\xC3\x81S BRILLANTE";
    if (ratio >= 1.1f)  return "LIGERAMENTE M\xC3\x81S BRILLANTE";
    if (ratio <= 0.8f)  return "MEZCLA M\xC3\x81S OSCURA";
    if (ratio <= 0.9f)  return "LIGERAMENTE M\xC3\x81S OSCURA";
    return "BALANCE TONAL SIMILAR";
}

AnalyzerInterpretation AnalyzerInterpreter::interpretCentroid(
    float ratio, const juce::String& genre)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::Centroid;
    ratio = juce::jlimit(0.4f, 2.5f, ratio);

    interp.reading = "Centroid: " + juce::String((ratio - 1.0f) * 100.0f, 0)
        + "% vs referencia";
    interp.interpretation = interpretCentroidLabel(ratio, genre);

    if (ratio >= 1.2f) {
        interp.severity = 0.6f;
        interp.consequence = "Posible fatiga auditiva en frecuencias medias-altas (3-5 kHz)";
        interp.action = "Suaviza presencia en voces, revisa hi-hats y platillos";
    } else if (ratio >= 1.1f) {
        interp.severity = 0.3f;
        interp.consequence = "Puede sonar m\xC3\xA1s moderno, aceptable";
        interp.action = "Monitorea si hay fatiga despu\xC3\xA9s de 30 min";
    } else if (ratio <= 0.8f) {
        interp.severity = 0.6f;
        interp.consequence = "Falta de claridad y definici\xC3\xB3n, mezcla opaca";
        interp.action = "Agrega shelving en 8-12 kHz, revisa HPF agresivos";
    } else if (ratio <= 0.9f) {
        interp.severity = 0.3f;
        interp.consequence = "Puede sonar m\xC3\xA1s vintage, aceptable";
        interp.action = "Monitorea claridad general de la mezcla";
    } else {
        interp.severity = 0.1f;
        interp.consequence = "Buena traducci\xC3\xB3n espectral";
        interp.action = "\xE2\x80\x94";
        interp.isPraise = true;
    }

    // Añadir contexto de rango típico por género
    juce::String g = genre.toLowerCase().trim();
    if (g.isNotEmpty() && ratio > 1.1f) {
        interp.action += " (rango t\xC3\xADpico " + genre + ": 2-3.5 kHz)";
    }

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  LUFS / LOUDNESS
// ═══════════════════════════════════════════════════════════════════════════

float AnalyzerInterpreter::getLUFSTarget(const juce::String& genre)
{
    juce::String g = genre.toLowerCase().trim();
    if (g == "pop" || g == "r&b" || g == "rnb")     return -9.0f;
    if (g == "edm" || g == "electronic")             return -7.0f;
    if (g == "trap" || g == "hip-hop" || g == "rap") return -8.0f;
    if (g == "reggaeton")                            return -8.0f;
    if (g == "rock")                                 return -10.0f;
    if (g == "metal")                                return -8.0f;
    if (g == "jazz")                                 return -16.0f;
    if (g == "classical")                            return -18.0f;
    if (g == "latin")                                return -9.0f;
    return -14.0f; // Default
}

AnalyzerInterpretation AnalyzerInterpreter::interpretLUFS(
    float integratedLUFS, float truePeakDBTP, float lra,
    const juce::String& genre)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::LUFS;

    float target = getLUFSTarget(genre);
    float diff = integratedLUFS - target;

    interp.reading = "LUFS: " + juce::String(integratedLUFS, 1)
        + " (target " + juce::String(target, 1)
        + ") | TP: " + juce::String(truePeakDBTP, 1)
        + " dBTP | LRA: " + juce::String(lra, 1) + " LU";

    // LUFS afecta todo el espectro — rango completo para el overlay
    interp.lowFreqHz = 20.0f;
    interp.highFreqHz = 20000.0f;

    // Evaluar LUFS vs target
    if (diff > 2.0f) {
        interp.interpretation = "MEZCLA M\xC3\x81S FUERTE QUE EL TARGET";
        interp.severity = 0.6f;
        interp.consequence = "Posible distorsi\xC3\xB3n en streaming, mezcla sin headroom";
        interp.action = "Baja el master fader 2-3 dB, revisa limitadores";
    } else if (diff < -4.0f) {
        interp.interpretation = "MEZCLA M\xC3\x81S SILENCIOSA QUE EL TARGET";
        interp.severity = 0.4f;
        interp.consequence = "Sonar\xC3\xA1 m\xC3\xA1s bajito que otras canciones en playlists";
        interp.action = "Sube niveles, agrega compresi\xC3\xB3n/limitaci\xC3\xB3n suave";
    } else if (std::abs(diff) <= 2.0f) {
        interp.interpretation = "LOUDNESS ALINEADO CON EL G\xC3\x89NERO";
        interp.severity = 0.1f;
        interp.consequence = "Buena traducci\xC3\xB3n a plataformas de streaming";
        interp.action = "\xE2\x80\x94";
        interp.isPraise = true;
    } else {
        interp.interpretation = "LOUDNESS LIGERAMENTE DESVIADO";
        interp.severity = 0.2f;
        interp.consequence = "Diferencia de " + juce::String(std::abs(diff), 1)
            + " LU respecto al target de " + genre;
        interp.action = "Ajuste fino de 1-2 dB en master";
    }

    // True Peak check
    if (truePeakDBTP > -1.0f) {
        // This is critical - override to most severe interpretation
        interp.interpretation = "INTERSAMPLE PEAKS PELIGROSOS";
        interp.severity = 1.0f;
        interp.isCritical = true;
        interp.consequence = "Distorsión en conversión D/A y códecs con pérdida";
        interp.action = "Reduce output del limitador 1-2 dB, usa True Peak limiting";
        interp.reading = "True Peak = " + juce::String(truePeakDBTP, 1) + " dBTP (máximo seguro: -1 dBTP)";
    }

    // LRA check (only if not already critical from True Peak)
    if (!interp.isCritical) {
        if (lra < 3.0f && lra > 0.0f) {
            interp.interpretation = "RANGO DE LOUDNESS MUY ESTRECHO";
            interp.severity = juce::jmax(interp.severity, 0.6f);
            interp.consequence = "Mezcla monótona, fatiga auditiva";
            interp.action = "Revisa compresión excesiva, agrega variación dinámica";
            interp.reading = "LRA = " + juce::String(lra, 1) + " LU (mínimo recomendado: 3 LU)";
        } else if (lra > 12.0f) {
            // Warning for high LRA but not critical
            AnalyzerInterpretation lraInterp;
            lraInterp.domain = AnalyzerInterpretation::Domain::LUFS;
            lraInterp.interpretation = "RANGO DE LOUDNESS MUY AMPLIO";
            lraInterp.severity = 0.4f;
            lraInterp.consequence = "Partes suaves se pierden en ambientes ruidosos";
            lraInterp.action = "Comprime las secciones más dinámicas";
            lraInterp.reading = "LRA = " + juce::String(lra, 1) + " LU";
            return lraInterp;
        }
    }

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  PHASE SCOPE / VECTORSCOPE
// ═══════════════════════════════════════════════════════════════════════════

AnalyzerInterpretation AnalyzerInterpreter::interpretPhaseScope(
    float correlation, float stereoWidth)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::PhaseScope;

    interp.reading = "Corr: " + juce::String(correlation, 2)
        + " | Width: " + juce::String(stereoWidth, 2);

    // Asignar rango de frecuencia según el tipo de problema de fase/estéreo
    if (correlation < 0.0f || (stereoWidth > 0.8f && correlation < 0.4f)) {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;  // Phase cancellation spans full spectrum
    } else if (stereoWidth > 0.7f) {
        interp.lowFreqHz = 1000.0f;
        interp.highFreqHz = 20000.0f;  // Wide stereo mostly affects highs
    } else if (stereoWidth < 0.2f) {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 500.0f;   // Mono/center mostly affects low end
    } else {
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
    }

    // Determinar si el problema es exceso de side o mono
    if (stereoWidth > 0.8f && correlation < 0.4f) {
        interp.interpretation = "EXCESO DE INFORMACIÓN EN CANAL SIDE";
        interp.severity = 0.6f;
        interp.consequence = "Imagen estéreo exagerada, problemas al reproducirse en mono";
        interp.action = "Revisa elementos paneados extremos, reduce ancho de pads/reverbs";
    } else if (stereoWidth < 0.2f && correlation > 0.95f) {
        interp.interpretation = "MAYORÍA DE ENERGÍA EN MID";
        interp.severity = 0.3f;
        interp.consequence = "Mezcla angosta, falta de apertura estéreo";
        interp.action = "Agrega ancho estéreo en elementos secundarios";
    } else if (correlation < 0.0f) {
        interp.interpretation = "CANCELACIÓN DE FASE";
        interp.severity = 0.8f;
        interp.isCritical = true;
        interp.consequence = "Pérdida severa de información en mono";
        interp.action = "Usa el correlation meter para identificar frecuencias problemáticas";
    } else if (stereoWidth > 0.7f) {
        interp.interpretation = "ANCHO ESTÉREO ELEVADO";
        interp.severity = 0.3f;
        interp.consequence = "Puede sonar difuso en sistemas de baja fidelidad";
        interp.action = "Verifica compatibilidad mono en el phase correlation meter";
    } else {
        interp.interpretation = "IMAGEN ESTÉREO BALANCEADA";
        interp.severity = 0.1f;
        interp.isPraise = true;
        interp.consequence = "Buena distribución estéreo sin excesos";
        interp.action = "—";
    }

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  SPECTRAL BAND
// ═══════════════════════════════════════════════════════════════════════════

AnalyzerInterpretation AnalyzerInterpreter::interpretSpectralBand(
    float bandEnergyDb, int bandIndex, const juce::String& bandName,
    float referenceDb)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::SpectralBand;

    int idx = juce::jlimit(0, 6, bandIndex);
    interp.lowFreqHz = kBandFreqs[idx][0];
    interp.highFreqHz = kBandFreqs[idx][1];
    interp.reading = bandName + " (" + juce::String((int)kBandFreqs[idx][0])
        + "-" + juce::String((int)kBandFreqs[idx][1]) + " Hz): "
        + juce::String(bandEnergyDb, 1) + " dBFS";

    // Si hay referencia, comparar
    float diff = -100.0f;
    bool hasReference = (referenceDb > -90.0f);
    if (hasReference) {
        diff = bandEnergyDb - referenceDb;
        interp.reading += " vs ref " + juce::String(referenceDb, 1) + " dBFS (" + juce::String(diff, 1) + " dB)";
    }

    // Interpretar según la banda
    switch (idx) {
        case 0: // Sub
            if (diff > 6.0f) {
                interp.interpretation = "EXCESO DE SUB-GRAVES";
                interp.severity = 0.6f;
                interp.consequence = "Headroom consumido, mezcla turbia en sistemas sin sub";
                interp.action = "HPF en 25-30 Hz en elementos que no sean kick/bass";
            } else {
                interp.interpretation = "SUB-GRAVES EN RANGO";
                interp.severity = 0.1f;
                interp.isPraise = true;
                interp.consequence = "Presencia de sub-graves controlada";
                interp.action = "\xE2\x80\x94";
            }
            break;

        case 1: // Bass
            if (diff > 6.0f) {
                interp.interpretation = "EXCESO DE BAJOS";
                interp.severity = 0.5f;
                interp.consequence = "Mezcla turbia, enmascaramiento de medios";
                interp.action = "Reduce 2-4 dB en 86-301 Hz, revisa acumulaci\xC3\xB3n de bajos";
            } else if (diff < -6.0f && hasReference) {
                interp.interpretation = "FALTA DE BAJOS";
                interp.severity = 0.4f;
                interp.consequence = "Mezcla sin cuerpo ni solidez";
                interp.action = "Revisa niveles de kick y bajo, agrega compresi\xC3\xB3n en serie";
            } else {
                interp.interpretation = "BAJOS EN RANGO";
                interp.severity = 0.1f;
                interp.isPraise = true;
            }
            break;

        case 2: // Low-Mid
            if (diff > 4.0f) {
                interp.interpretation = "BARRO EN LOW-MID";
                interp.severity = 0.6f;
                interp.consequence = "Mezcla turbia, falta de claridad";
                interp.action = "Corte de 2-4 dB en 300-500 Hz en guitarras, teclados, voces";
            } else if (diff < -8.0f && hasReference) {
                interp.interpretation = "FALTA DE CUERPO EN LOW-MID";
                interp.severity = 0.3f;
                interp.consequence = "Mezcla hueca, sin calidez";
                interp.action = "Agrega cuerpo con EQ en 250-400 Hz en elementos principales";
            } else {
                interp.interpretation = "LOW-MID CONTROLADO";
                interp.severity = 0.1f;
                interp.isPraise = true;
            }
            break;

        case 3: // High-Mid
            if (diff > 4.0f) {
                interp.interpretation = "EXCESO EN HIGH-MID";
                interp.severity = 0.5f;
                interp.consequence = "Mezcla agresiva, posible fatiga auditiva";
                interp.action = "Reduce 1-3 dB en 1-3 kHz en instrumentos melódicos";
            } else if (diff < -6.0f && hasReference) {
                interp.interpretation = "FALTA DE HIGH-MID";
                interp.severity = 0.3f;
                interp.consequence = "Mezcla sin ataque ni definición";
                interp.action = "Agrega presencia en 2-4 kHz en elementos rítmicos";
            } else {
                interp.interpretation = "HIGH-MID BALANCEADO";
                interp.severity = 0.1f;
                interp.isPraise = true;
            }
            break;

        case 4: // Presence
            if (diff > 4.0f) {
                interp.interpretation = "EXCESO DE PRESENCIA";
                interp.severity = 0.5f;
                interp.consequence = "Posible fatiga auditiva en voces y elementos melódicos";
                interp.action = "Ajuste fino de 1-2 dB en 3-5 kHz, usa de-esser si hay sibilancia";
            } else if (diff < -6.0f && hasReference) {
                interp.interpretation = "FALTA DE PRESENCIA";
                interp.severity = 0.5f;
                interp.consequence = "Voces e instrumentos principales suenan lejanos, falta de foco";
                interp.action = "Boost shelving en 4-6 kHz de 1-3 dB en voces/pistas principales";
            } else {
                interp.interpretation = "PRESENCIA ADECUADA";
                interp.severity = 0.1f;
                interp.isPraise = true;
            }
            break;

        case 5: // High
            if (diff > 4.0f) {
                interp.interpretation = "EXCESO DE AGUDOS";
                interp.severity = 0.4f;
                interp.consequence = "Fatiga auditiva, sibilancia en voces";
                interp.action = "Shelving suave en 8-10 kHz, control con de-esser";
            } else if (diff < -8.0f && hasReference) {
                interp.interpretation = "FALTA DE AGUDOS";
                interp.severity = 0.3f;
                interp.consequence = "Mezcla opaca, sin aire ni brillo";
                interp.action = "Agrega shelving en 10-12 kHz de 1-2 dB";
            } else {
                interp.interpretation = "AGUDOS BALANCEADOS";
                interp.severity = 0.1f;
                interp.isPraise = true;
            }
            break;

        case 6: // Air
            if (diff > 4.0f) {
                interp.interpretation = "EXCESO DE AIRE";
                interp.severity = 0.3f;
                interp.consequence = "Posible sibilancia, fatiga en frecuencias muy altas";
                interp.action = "Control con shelving en 12-16 kHz, low-pass suave si es necesario";
            } else if (diff < -10.0f && hasReference) {
                interp.interpretation = "FALTA DE AIRE";
                interp.severity = 0.2f;
                interp.consequence = "Mezcla sin brillo ni apertura";
                interp.action = "Agrega shelving en 12-16 kHz de 1-2 dB en master";
            } else {
                interp.interpretation = "AIRE ADECUADO";
                interp.severity = 0.1f;
                interp.isPraise = true;
            }
            break;
    }

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  GAIN — Peak / RMS levels per channel
// ═══════════════════════════════════════════════════════════════════════════

AnalyzerInterpretation AnalyzerInterpreter::interpretGain(
    float peakLeft, float peakRight, float rmsLeft, float rmsRight)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::Gain;

    float peakCombined = juce::jmax(peakLeft, peakRight);
    float rmsCombined  = juce::jmax(rmsLeft, rmsRight);
    float crestDb = (rmsCombined > -90.0f && peakCombined > -90.0f)
        ? peakCombined - rmsCombined : 0.0f;
    float lrDiff = std::abs(peakLeft - peakRight);

    interp.reading = "Peak L: " + juce::String(peakLeft, 1) + " dB | R: "
        + juce::String(peakRight, 1) + " dB | RMS L: " + juce::String(rmsLeft, 1)
        + " dB | R: " + juce::String(rmsRight, 1) + " dB";

    // ═══ Priority 1: Clipping (Critical) ═════════════════════════════════
    if (peakCombined > -0.5f) {
        interp.interpretation = "CLIPPING";
        interp.severity = 1.0f;
        interp.isCritical = true;
        interp.consequence = "Distorsión digital audible — el audio se está recortando";
        interp.action = "Reduce el fader de gain INMEDIATAMENTE 3-6 dB. Revisa si hay plugins con output alto";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Priority 2: L/R severe imbalance ═══════════════════════════════
    if (lrDiff > 10.0f && peakLeft > -60.0f && peakRight > -60.0f) {
        interp.interpretation = "DESBALANCE SEVERO L/R";
        interp.severity = 0.7f;
        interp.consequence = "La imagen estéreo está inclinada " + juce::String(lrDiff, 1)
            + " dB hacia " + (peakLeft > peakRight ? "el canal izquierdo" : "el canal derecho");
        interp.action = "Ajusta el pan o el fader de la pista para balancear. Diferencia aceptable: < 3 dB";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Priority 3: Near-clipping ═══════════════════════════════════════
    if (peakCombined > -3.0f) {
        interp.interpretation = "CERCA DEL CLIPPING";
        interp.severity = 0.6f;
        interp.consequence = "Picos a " + juce::String(peakCombined, 1)
            + " dB — muy cerca de 0 dBFS, sin headroom";
        interp.action = "Reduce el gain 3-6 dB para dejar headroom. Objetivo: picos entre -12 dB y -6 dB";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Priority 4: Crest extremo (muy comprimido o muy dinámico) ═══════
    if (crestDb > 0.0f && rmsCombined > -50.0f) {
        if (crestDb < 4.0f) {
            interp.interpretation = "SOBRECOMPRIMIDO";
            interp.severity = 0.6f;
            interp.consequence = "Crest factor de " + juce::String(crestDb, 1)
                + " dB — la pista suena aplastada, sin transientes";
            interp.action = "Reduce la compresión: menor ratio, mayor threshold, o bypass compresores en cadena";
            interp.lowFreqHz = 20.0f;
            interp.highFreqHz = 500.0f;
            return interp;
        }
        if (crestDb > 20.0f) {
            interp.interpretation = "EXTREMADAMENTE DINÁMICO";
            interp.severity = 0.4f;
            interp.consequence = "Crest factor de " + juce::String(crestDb, 1)
                + " dB — partes suaves se pierden, partes fuertes dominan";
            interp.action = "Agrega compresión suave (ratio 3:1, threshold en picos) para nivelar la pista";
            interp.lowFreqHz = 1000.0f;
            interp.highFreqHz = 10000.0f;
            return interp;
        }
    }

    // ═══ Priority 5: L/R moderate imbalance ═══════════════════════════════
    if (lrDiff > 6.0f && peakLeft > -60.0f && peakRight > -60.0f) {
        interp.interpretation = "LIGERO DESBALANCE L/R";
        interp.severity = 0.3f;
        interp.consequence = "Diferencia de " + juce::String(lrDiff, 1)
            + " dB entre canales — perceptible en auriculares";
        interp.action = "Ajusta el pan o volumen del canal más fuerte para igualar";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Priority 6: Baja señal ═══════════════════════════════════════════
    if (peakCombined < -30.0f && peakCombined > -90.0f) {
        interp.interpretation = "SEÑAL MUY BAJA";
        interp.severity = 0.2f;
        interp.consequence = "La pista está muy silenciosa (peak " + juce::String(peakCombined, 1)
            + " dB) — puede perderse en la mezcla";
        interp.action = "Sube el fader de gain. Objetivo: picos entre -12 dB y -6 dB en el canal";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Priority 7: Saludable ════════════════════════════════════════════
    if (peakCombined >= -12.0f && peakCombined <= -3.0f && lrDiff < 3.0f) {
        interp.interpretation = "NIVEL SALUDABLE";
        interp.severity = 0.1f;
        interp.isPraise = true;
        interp.consequence = "Peak de " + juce::String(peakCombined, 1)
            + " dB con buen balance L/R — nivel ideal para mezcla";
        interp.action = "—";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso por defecto: sin señal o datos insuficientes ════════════════
    interp.interpretation = "SIN SEÑAL";
    interp.severity = 0.15f;
    if (peakCombined < -90.0f) {
        interp.consequence = "No se detecta señal — ¿la pista está muteada o sin contenido?";
        interp.action = "Verifica que la pista tenga audio y no esté silenciada";
    } else {
        interp.consequence = "Nivel por debajo del rango ideal (peak " + juce::String(peakCombined, 1) + " dB)";
        interp.action = "Sube el fader hasta que los picos estén entre -12 dB y -6 dB";
    }
    interp.lowFreqHz = 20.0f;
    interp.highFreqHz = 20000.0f;

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  STEREO WIDTH — Per-band stereo width analysis
// ═══════════════════════════════════════════════════════════════════════════

AnalyzerInterpretation AnalyzerInterpreter::interpretStereoWidth(
    const float stereoWidthPerBand[6], float avgStereoWidth)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::StereoWidth;

    // Nombres de bandas (6 bandas usadas en el sistema)
    static constexpr const char* kBand6Names[6] = {
        "Sub (94Hz)", "Bass (281Hz)", "LoMid (752Hz)",
        "HiMid (2kHz)", "Pres (5kHz)", "Air (12kHz)"
    };
    static constexpr float kBand6Freqs[6][2] = {
        { 20.0f, 86.0f },     // Sub
        { 86.0f, 301.0f },    // Bass
        { 301.0f, 1076.0f },  // LoMid
        { 1076.0f, 3532.0f }, // HiMid
        { 3532.0f, 8355.0f }, // Pres
        { 8355.0f, 20000.0f } // Air
    };

    // Construir lectura detallada
    juce::String reading = "Avg width: " + juce::String(avgStereoWidth, 2) + " | Per-band:";
    for (int b = 0; b < 6; ++b) {
        reading += " " + juce::String(kBand6Names[b]) + "=" + juce::String(stereoWidthPerBand[b], 2);
        if (b < 5) reading += ",";
    }
    interp.reading = reading;

    // ─── Detectar problemas específicos por banda ────────────────────────
    bool subTooWide    = (stereoWidthPerBand[0] > 0.3f);  // Sub estéreo = problema
    bool bassTooWide   = (stereoWidthPerBand[1] > 0.4f);  // Bass estéreo = problema
    bool lowMidWide    = (stereoWidthPerBand[2] > 0.5f);  // LoMid acepta algo de ancho
    bool hiMidNarrow   = (stereoWidthPerBand[3] < 0.1f);  // HiMid muy mono = falta apertura
    bool presNarrow    = (stereoWidthPerBand[4] < 0.1f);  // Presencia muy mono
    bool airNarrow     = (stereoWidthPerBand[5] < 0.1f);  // Aire muy mono
    bool allNarrow     = true;
    bool allWide       = true;
    for (int b = 0; b < 6; ++b) {
        if (stereoWidthPerBand[b] > 0.15f) allNarrow = false;
        if (stereoWidthPerBand[b] < 0.5f)  allWide   = false;
    }

    // ═══ Caso 1: Sub o Bass con ancho estéreo (problemático) ═══════════════
    if (subTooWide || bassTooWide) {
        juce::String bandName = subTooWide ? kBand6Names[0] : kBand6Names[1];
        float freq = subTooWide ? kBand6Freqs[0][0] : kBand6Freqs[1][0];
        interp.interpretation = "GRAVES ESTÉREO — PROBLEMA DE MONO";
        interp.severity = 0.7f;
        interp.isCritical = true;
        interp.consequence = "La banda " + juce::String(bandName)
            + " tiene ancho estéreo (" + juce::String(
                subTooWide ? stereoWidthPerBand[0] : stereoWidthPerBand[1], 2)
            + "). Los graves deben ser mono para evitar cancelación de fase y pérdida de pegada";
        interp.action = "Aplica un bass mono (procesado M/S o utility) hasta "
            + juce::String((int)freq) + " Hz. El bajo y el kick deben estar centrados";
        interp.lowFreqHz = subTooWide ? 20.0f : 86.0f;
        interp.highFreqHz = subTooWide ? 86.0f : 301.0f;
        return interp;
    }

    // ═══ Caso 2: Todo muy angosto (mono total) ════════════════════════════
    if (allNarrow && avgStereoWidth < 0.15f) {
        interp.interpretation = "MEZCLA MUY MONO";
        interp.severity = 0.4f;
        interp.consequence = "El ancho estéreo promedio es " + juce::String(avgStereoWidth, 2)
            + " — la mezcla suena angosta, sin apertura ni profundidad";
        interp.action = "Agrega ancho estéreo en HiMid/Pres/Air usando Stereo Shaper, "
            "delays en los laterales, o reverbs con width > 50%";
        interp.lowFreqHz = 1000.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso 3: Todo muy ancho (problemas de mono) ════════════════════════
    if (allWide && avgStereoWidth > 0.6f) {
        interp.interpretation = "MEZCLA MUY ANCHA";
        interp.severity = 0.5f;
        interp.consequence = "El ancho estéreo promedio es " + juce::String(avgStereoWidth, 2)
            + " — peligro de cancelación de fase en mono y falta de foco";
        interp.action = "Reduce el ancho de elementos secundarios (reverbs, pads) y "
            "mantén elementos principales centrados (kick, bajo, voz)";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso 4: HiMid/Pres/Air angostos (falta apertura en agudos) ═══════
    if ((hiMidNarrow || presNarrow || airNarrow) && avgStereoWidth < 0.3f) {
        interp.interpretation = "FALTA DE APERTURA ESTÉREO";
        interp.severity = 0.3f;
        juce::String narrowBands;
        if (hiMidNarrow) narrowBands += "HiMid";
        if (presNarrow)  narrowBands += (narrowBands.isEmpty() ? "" : ", ") + juce::String("Pres");
        if (airNarrow)   narrowBands += (narrowBands.isEmpty() ? "" : ", ") + juce::String("Air");
        interp.consequence = "Las bandas " + narrowBands
            + " tienen ancho estéreo muy bajo — la mezcla suena cerrada en agudos";
        interp.action = "Agrega ancho estéreo en高频 usando delays cortos (<15ms) o "
            "procesado M/S con boost en Side por encima de 3 kHz";
        interp.lowFreqHz = 2000.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso 5: Ancho saludable ═══════════════════════════════════════════
    if (avgStereoWidth >= 0.2f && avgStereoWidth <= 0.5f && !subTooWide && !bassTooWide) {
        interp.interpretation = "ANCHO ESTÉREO SALUDABLE";
        interp.severity = 0.1f;
        interp.isPraise = true;
        interp.consequence = "Ancho promedio de " + juce::String(avgStereoWidth, 2)
            + " — buena distribución estéreo con graves centrados";
        interp.action = "—";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso por defecto: ancho moderado ══════════════════════════════════
    interp.interpretation = "ANCHO ESTÉREO MODERADO";
    interp.severity = 0.2f;
    interp.consequence = "El ancho estéreo promedio es " + juce::String(avgStereoWidth, 2)
        + " — dentro del rango aceptable, pero revisa compatibilidad mono";
    interp.action = "Verifica en el correlation meter que no haya cancelación de fase";
    interp.lowFreqHz = 20.0f;
    interp.highFreqHz = 20000.0f;

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  MID-SIDE — Mid/Side balance per band
// ═══════════════════════════════════════════════════════════════════════════

AnalyzerInterpretation AnalyzerInterpreter::interpretMidSide(
    const float midEnergyPerBand[6], const float sideEnergyPerBand[6],
    float avgStereoWidth)
{
    AnalyzerInterpretation interp;
    interp.domain = AnalyzerInterpretation::Domain::MidSide;

    static constexpr const char* kBand6Names[6] = {
        "Sub (94Hz)", "Bass (281Hz)", "LoMid (752Hz)",
        "HiMid (2kHz)", "Pres (5kHz)", "Air (12kHz)"
    };
    static constexpr float kBand6Freqs[6][2] = {
        { 20.0f, 86.0f },     // Sub
        { 86.0f, 301.0f },    // Bass
        { 301.0f, 1076.0f },  // LoMid
        { 1076.0f, 3532.0f }, // HiMid
        { 3532.0f, 8355.0f }, // Pres
        { 8355.0f, 20000.0f } // Air
    };

    // ═══ Calcular ratios Mid/Side por banda ═══════════════════════════════
    struct BandMS {
        int index;
        float midDb;
        float sideDb;
        float ratio;  // sideEnergy / midEnergy (lineal, >0.5 = side significativo)
        bool valid;
    };
    BandMS bands[6];
    float maxRatio = 0.0f;
    int maxRatioBand = -1;
    int wideBands = 0;
    int narrowBands = 0;

    for (int b = 0; b < 6; ++b) {
        bands[b].index  = b;
        bands[b].midDb  = midEnergyPerBand[b];
        bands[b].sideDb = sideEnergyPerBand[b];
        bands[b].valid  = (midEnergyPerBand[b] > -80.0f && sideEnergyPerBand[b] > -80.0f);

        if (bands[b].valid) {
            float midLin  = std::pow(10.0f, midEnergyPerBand[b] / 20.0f);
            float sideLin = std::pow(10.0f, sideEnergyPerBand[b] / 20.0f);
            bands[b].ratio = (midLin > 1e-10f) ? sideLin / midLin : 0.0f;

            if (bands[b].ratio > maxRatio) {
                maxRatio = bands[b].ratio;
                maxRatioBand = b;
            }
            if (bands[b].ratio > 0.5f) wideBands++;
            if (bands[b].ratio < 0.1f) narrowBands++;
        } else {
            bands[b].ratio = 0.0f;
        }
    }

    // Construir lectura
    juce::String reading = "Mid/Side ratios (side/mid):";
    for (int b = 0; b < 6; ++b) {
        if (bands[b].valid)
            reading += " " + juce::String(kBand6Names[b]) + "=" + juce::String(bands[b].ratio, 2);
        else
            reading += " " + juce::String(kBand6Names[b]) + "=N/A";
        if (b < 5) reading += ",";
    }
    interp.reading = reading;

    // ═══ Caso 1: Side muy alto en Sub o Bass (problema grave) ═════════════
    if (maxRatioBand >= 0 && maxRatioBand <= 1 && maxRatio > 0.5f) {
        interp.interpretation = "SIDE EXCESIVO EN GRAVES";
        interp.severity = 0.7f;
        interp.isCritical = true;
        interp.consequence = "La banda " + juce::String(kBand6Names[maxRatioBand])
            + " tiene más energía Side que Mid (ratio " + juce::String(bands[maxRatioBand].ratio, 2)
            + "). Esto causa pérdida de pegada y problemas de fase en mono";
        interp.action = "Aplica procesado M/S para centrar los graves: pasa el contenido "
            "de " + juce::String((int)kBand6Freqs[maxRatioBand][0]) + "-"
            + juce::String((int)kBand6Freqs[maxRatioBand][1]) + " Hz a Mid";
        interp.lowFreqHz = kBand6Freqs[maxRatioBand][0];
        interp.highFreqHz = kBand6Freqs[maxRatioBand][1];
        return interp;
    }

    // ═══ Caso 2: Side dominante en general ═════════════════════════════════
    if (wideBands >= 4 && maxRatio > 0.8f) {
        interp.interpretation = "PREDOMINIO DE CANAL SIDE";
        interp.severity = 0.5f;
        interp.consequence = juce::String(wideBands)
            + " de 6 bandas tienen side significativo (>0.5 ratio). "
            + "La banda " + juce::String(kBand6Names[maxRatioBand])
            + " es la más extrema (ratio " + juce::String(bands[maxRatioBand].ratio, 2) + ")";
        interp.action = juce::String("Aplica procesado M/S: reduce el side en frecuencias medias (500 Hz-2 kHz) ")
            + "y mantén el centro fuerte. La mezcla debe sonar sólida en mono";
        interp.lowFreqHz = 20.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso 3: Todo muy mono (poco Side) ═════════════════════════════════
    if (narrowBands >= 4 && avgStereoWidth < 0.2f) {
        interp.interpretation = "PREDOMINIO DE CANAL MID";
        interp.severity = 0.3f;
        interp.consequence = juce::String(narrowBands)
            + " de 6 bandas tienen poco contenido Side (<0.1 ratio). "
            + "La mezcla suena muy centrada, sin apertura estéreo";
        interp.action = juce::String("Agrega ancho estéreo usando delays stereo, wideners, o ")
            + "procesado M/S con boost en Side por encima de 3 kHz";
        interp.lowFreqHz = 2000.0f;
        interp.highFreqHz = 20000.0f;
        return interp;
    }

    // ═══ Caso 4: Side alto en agudos (normal/deseable) ════════════════════
    if (maxRatioBand >= 4 && maxRatio > 0.5f && avgStereoWidth >= 0.2f && avgStereoWidth <= 0.5f) {
        interp.interpretation = "SIDE SALUDABLE EN AGUDOS";
        interp.severity = 0.1f;
        interp.isPraise = true;
        interp.consequence = "Las bandas " + juce::String(kBand6Names[maxRatioBand])
            + " tienen contenido Side (ratio " + juce::String(bands[maxRatioBand].ratio, 2)
            + "), esperado y deseable para dar apertura";
        interp.action = "—";
        interp.lowFreqHz = kBand6Freqs[maxRatioBand][0];
        interp.highFreqHz = kBand6Freqs[maxRatioBand][1];
        return interp;
    }

    // ═══ Caso 5: Balance M/S saludable ═════════════════════════════════════
    {
        int healthyBands = 0;
        for (int b = 0; b < 6; ++b) {
            if (bands[b].valid && bands[b].ratio >= 0.1f && bands[b].ratio <= 0.5f)
                healthyBands++;
        }
        if (healthyBands >= 4) {
            interp.interpretation = "BALANCE MID/SIDE SALUDABLE";
            interp.severity = 0.1f;
            interp.isPraise = true;
            interp.consequence = juce::String(healthyBands)
                + " de 6 bandas tienen balance M/S saludable. "
                + "Buena distribución entre centro y laterales";
            interp.action = "—";
            interp.lowFreqHz = 20.0f;
            interp.highFreqHz = 20000.0f;
            return interp;
        }
    }

    // ═══ Caso por defecto ═════════════════════════════════════════════════
    interp.interpretation = "MID/SIDE SIN DATOS SUFICIENTES";
    interp.severity = 0.1f;
    interp.consequence = "No hay suficientes datos de Mid/Side para una interpretación completa";
    interp.action = "—";
    interp.lowFreqHz = 20.0f;
    interp.highFreqHz = 20000.0f;

    return interp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  GENERACIÓN MASIVA
// ═══════════════════════════════════════════════════════════════════════════

std::vector<AnalyzerInterpretation> AnalyzerInterpreter::interpretAll(
    float correlation, float crestDb, float centroidRatio,
    float integratedLUFS, float truePeakDBTP, float lra,
    const juce::String& genre)
{
    std::vector<AnalyzerInterpretation> results;

    // Correlation (siempre)
    auto corrInterp = interpretCorrelation(correlation, genre);
    if (correlation > -1.0f && correlation < 1.0f) // Only if valid reading
        results.push_back(std::move(corrInterp));

    // Crest factor (si > 0)
    if (crestDb > 0.0f) {
        auto crestInterp = interpretCrest(crestDb, genre);
        results.push_back(std::move(crestInterp));
    }

    // Centroid (si ratio > 0)
    if (centroidRatio > 0.0f) {
        auto centroidInterp = interpretCentroid(centroidRatio, genre);
        results.push_back(std::move(centroidInterp));
    }

    // LUFS (si integrated > -90)
    if (integratedLUFS > -90.0f) {
        auto lufsInterp = interpretLUFS(integratedLUFS, truePeakDBTP, lra, genre);
        results.push_back(std::move(lufsInterp));
    }

    return results;
}

std::vector<BandDiagnostic> AnalyzerInterpreter::toBandDiagnostics(
    const std::vector<AnalyzerInterpretation>& interpretations)
{
    std::vector<BandDiagnostic> diagnostics;
    diagnostics.reserve(interpretations.size());

    for (const auto& interp : interpretations) {
        // Solo incluir si hay información útil
        if (interp.interpretation.isNotEmpty())
            diagnostics.push_back(interp.toBandDiagnostic());
    }

    return diagnostics;
}

juce::String AnalyzerInterpreter::formatCoachMessage(
    const std::vector<AnalyzerInterpretation>& interpretations,
    int maxEntries)
{
    if (interpretations.empty())
        return {};

    // Ordenar por severidad (más severo primero)
    std::vector<std::reference_wrapper<const AnalyzerInterpretation>> sorted;
    sorted.reserve(interpretations.size());
    for (const auto& interp : interpretations)
        sorted.push_back(std::cref(interp));

    std::sort(sorted.begin(), sorted.end(),
        [](const AnalyzerInterpretation& a, const AnalyzerInterpretation& b) {
            return a.severity > b.severity;
        });

    juce::String msg;
    msg = "\xF0\x9F\x93\x8A **INTERPRETACI\xC3\x93N DE ANALIZADORES**\n"
          "═══════════════════════════════════\n\n";

    int count = 0;
    for (const auto& ref : sorted) {
        if (count >= maxEntries) break;
        const auto& interp = ref.get();

        // Saltar praises si hay warnings críticos
        if (count > 0 && interp.isPraise)
            continue;

        msg += interp.toFullMessage() + "\n\n";
        count++;
    }

    if (sorted.size() > maxEntries) {
        msg += "... y " + juce::String((int)sorted.size() - maxEntries)
            + " interpretaciones adicionales.\n";
    }

    msg += "═══════════════════════════════════";

    return msg;
}

} // namespace mixcoach

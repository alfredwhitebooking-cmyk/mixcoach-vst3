#pragma once
#include <juce_core/juce_core.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  NameInferrer — Detección compartida de palabras clave en nombres de pista
//
//  Propósito: UNIFICAR la detección de palabras clave que existía duplicada
//  en MessengerAudioProcessor::suggestTrackTypeFromName() y
//  CoachEngine::inferTrackRoleFromName(). Ambos ahora usan detectKeywords().
//
//  Uso:
//    auto kw = NameInferrer::detectKeywords("Kick 808");
//    if (kw.hasKick && kw.has808) ...
// ═══════════════════════════════════════════════════════════════════════════

struct DetectedKeywords {
    // Nombre normalizado (después de limpieza)
    juce::String normalizedName;
    bool isEmpty = true;

    // ═══ Palabras clave base ═══
    bool hasKick     = false;
    bool hasSnare    = false;
    bool hasHiHat    = false;
    bool has808      = false;
    bool hasBass     = false;
    bool hasClap     = false;
    bool hasTom      = false;
    bool hasRide     = false;
    bool hasCrash    = false;
    bool hasPerc     = false;
    bool hasVocal    = false;
    bool hasGuitar   = false;
    bool hasSynth    = false;
    bool hasPad      = false;
    bool hasPiano    = false;
    bool hasOrgan    = false;
    bool hasStrings  = false;
    bool hasBrass    = false;
    bool hasRiser    = false;
    bool hasFx       = false;
    bool hasNoise    = false;
    bool hasAmbient  = false;
    bool hasMaster   = false;
    bool hasBus      = false;
    bool hasDrum     = false;
    bool hasSub      = false;

    // ═══ Calificadores ═══
    bool hasAcoustic = false;
    bool hasElectric = false;
    bool hasFinger   = false;
    bool hasPick     = false;
    bool has808Kick  = false;
    bool has808Bass  = false;
    bool hasTrap     = false;
    bool hasReggaeton = false;
    bool hasRim      = false;
    bool hasOpenHH   = false;
    bool hasClosedHH = false;
    bool hasPluck    = false;
    bool hasLead     = false;   // "lead" sin "guitar"
    bool hasVozPrincipal = false;
    bool hasVozFondo = false;
    bool hasAdlib    = false;
    bool hasRhythm   = false;

    // ═══ Patrones extendidos (V8) ═══
    bool hasWinds    = false;
    bool hasViolin   = false;
    bool hasSample   = false;
    bool hasIntro    = false;
    bool hasFill     = false;
    bool hasMelody   = false;
    bool hasArp      = false;
    bool hasClick    = false;
    bool hasRoom     = false;
    bool hasRoomMic  = false;   // "room" en contexto de batería (drum room mic)

    // ═══ Específicos del Messenger ═══
    bool hasImpact   = false;
    bool hasDrumBus  = false;
    bool hasLeadVocal = false;
    bool hasBackVocal = false;
};

class NameInferrer {
public:
    /** Detecta palabras clave en el nombre de una pista.
        @param rawName  Nombre crudo de la pista (ej: "Kick 808", "Voz Principal L")
        @return DetectedKeywords con flags booleans + normalizedName */
    static DetectedKeywords detectKeywords(const juce::String& rawName) noexcept
    {
        DetectedKeywords kw;

        // ─── Normalizar nombre ──────────────────────────────────────────
        juce::String name = rawName.trim().toLowerCase();
        if (name.isEmpty())
            return kw;

        // Eliminar números al final ("Kick 1" → "Kick", "Snare 2" → "Snare")
        while (name.length() > 0 && name.getLastCharacter() >= '0' && name.getLastCharacter() <= '9')
            name = name.dropLastCharacters(1);
        name = name.trim();

        // Eliminar sufijos: " L", " R", " left", " right", " der", " izq", etc.
        auto removeSuffix = [](const juce::String& n, const juce::String& suffix) -> juce::String {
            if (n.endsWith(suffix))
                return n.substring(0, n.length() - suffix.length()).trim();
            return n;
        };
        name = removeSuffix(name, " l");
        name = removeSuffix(name, " r");
        name = removeSuffix(name, " left");
        name = removeSuffix(name, " right");
        name = removeSuffix(name, " der");
        name = removeSuffix(name, " izq");
        name = removeSuffix(name, " top");
        name = removeSuffix(name, " bot");
        name = removeSuffix(name, " mono");
        name = removeSuffix(name, " stereo");
        name = name.trim();

        if (name.isEmpty())
            return kw;

        kw.normalizedName = name;
        kw.isEmpty = false;

        // ─── Detectar palabras clave ────────────────────────────────────
        kw.hasKick     = name.contains("kick") || name.contains("bombo") || name.contains("patada")
                         || name.contains("kik") || name.contains("kck");
        kw.hasSnare    = name.contains("snare") || name.contains("caja") || name.contains("redoblante");
        kw.hasHiHat    = name.contains("hihat") || name.contains("hi-hat") || name.contains("hi hat")
                         || name.contains("charles")
                         || (name.contains("hat") && !name.contains("that") && !name.contains("what"));
        kw.has808      = name.contains("808");
        kw.hasBass     = name.contains("bass") || name.contains("bajo") || name.contains("sub");
        kw.hasClap     = name.contains("clap") || name.contains("palmas");
        kw.hasTom      = name.contains("tom") || name.contains("toms");
        kw.hasRide     = name.contains("ride") && !name.contains("side");
        kw.hasCrash    = name.contains("crash");
        kw.hasPerc     = name.contains("perc") || name.contains("shaker") || name.contains("tambourine")
                         || name.contains("pandereta") || name.contains("cowbell");
        kw.hasVocal    = name.contains("voz") || name.contains("vocal") || name.contains("voice")
                         || name.contains("vox") || name.contains("voc");
        kw.hasGuitar   = name.contains("guitar") || name.contains("guitarra");
        kw.hasSynth    = name.contains("synth") || name.contains("sint");
        kw.hasPad      = name.contains("pad") && !name.contains("shred");
        kw.hasPiano    = name.contains("piano") || name.contains("keys") || name.contains("teclado")
                         || name.contains("rhodes") || name.contains("wurly");
        kw.hasOrgan    = name.contains("organ") || name.contains("organo") || name.contains("\xC3\xB3rgano");
        kw.hasStrings  = name.contains("string") || name.contains("cuerda");
        kw.hasBrass    = name.contains("brass") || name.contains("bronce") || name.contains("trompeta")
                         || name.contains("sax") || name.contains("horn");
        kw.hasRiser    = name.contains("riser") || name.contains("sweep") || name.contains("ascensor")
                         || name.contains("uplift");
        kw.hasFx       = name.contains("fx") || name.contains("efecto") || name.contains("sfx")
                         || name.contains("effect");
        kw.hasNoise    = name.contains("noise") || name.contains("ruido");
        kw.hasAmbient  = name.contains("ambient") || name.contains("ambiente") || name.contains("atmos")
                         || name.contains("atm\xC3\xB3sfera");
        kw.hasMaster   = name.contains("master") || name.contains("main") || name.contains("final");
        kw.hasBus      = name.contains("bus") || name.contains("grupo") || name.contains("submix")
                         || name.contains("group");
        kw.hasDrum     = name.contains("drum") || name.contains("bater");
        kw.hasSub      = name.contains("sub") && !name.contains("submix") && !name.contains("subgrupo");

        // ═══ Calificadores ═══════════════════════════════════════════════
        kw.hasAcoustic  = name.contains("acoustic") || name.contains("ac\xC3\xBAstica") || name.contains("acustica");
        kw.hasElectric  = name.contains("electric") || name.contains("el\xC3" "\xA9" "ctrica") || name.contains("electrica");
        kw.hasFinger    = name.contains("finger") || name.contains("dedo");
        kw.hasPick      = name.contains("pick") || name.contains("p\xC3" "\xBA" "a") || name.contains("pua");
        kw.has808Kick   = kw.has808 && (kw.hasKick || name.contains("kick 808") || name.contains("808 kick"));
        kw.has808Bass   = kw.has808 && (kw.hasBass || name.contains("808 bass") || name.contains("bass 808"));
        kw.hasTrap      = name.contains("trap");
        kw.hasReggaeton = name.contains("reggaeton") || name.contains("reggae") || name.contains("reguet")
                          || name.contains("dem bow") || name.contains("dembow");
        kw.hasRim       = name.contains("rim") || name.contains("side stick") || name.contains("sidestick")
                          || name.contains("cross stick");
        kw.hasOpenHH    = name.contains("open") && kw.hasHiHat;
        kw.hasClosedHH  = name.contains("closed") && kw.hasHiHat;
        kw.hasPluck     = name.contains("pluck") || name.contains("pizz");
        kw.hasLead      = name.contains("lead") && !kw.hasGuitar;
        kw.hasVozPrincipal = kw.hasVocal && (name.contains("principal") || name.contains("lead")
                               || name.contains("main") || name.contains("voz"));
        kw.hasVozFondo  = kw.hasVocal && (name.contains("fondo") || name.contains("back")
                           || name.contains("coro") || name.contains("backing"));
        kw.hasAdlib     = kw.hasVocal && (name.contains("adlib") || name.contains("double")
                           || name.contains("doblaje"));
        kw.hasRhythm    = name.contains("rhythm") || name.contains("ritmica")
                          || name.contains("r\xC3" "\xAD" "tmica") || name.contains("ritmica");

        // ═══ Patrones extendidos V8 ══════════════════════════════════════
        kw.hasWinds    = name.contains("flute") || name.contains("flauta") || name.contains("clarinet")
                         || name.contains("sax") || name.contains("saxofon") || name.contains("oboe")
                         || name.contains("trumpet") || name.contains("trompeta") || name.contains("trombon");
        kw.hasViolin   = name.contains("violin") || name.contains("cello") || name.contains("viola");
        kw.hasSample   = name.contains("sample") || name.contains("loop") || name.contains("oneshot")
                         || name.contains("clip");
        kw.hasIntro    = name.contains("intro") || name.contains("buildup");
        kw.hasFill     = name.contains("fill") || name.contains("llenado");
        kw.hasMelody   = name.contains("melody") || name.contains("melodia");
        kw.hasArp      = name.contains("arpegg") || name.contains("arpegio") || name.contains("arp");
        kw.hasClick    = name.contains("click") || name.contains("metronome") || name.contains("metronomo");
        kw.hasRoom     = name.contains("room") || name.contains("sala");
        // Drum room mic: "room" aparece junto a keywords de batería → es un
        // mic de ambiente de batería, no un pad ambient genérico.
        kw.hasRoomMic  = kw.hasRoom && (kw.hasDrum || kw.hasKick || kw.hasSnare
                                        || kw.hasHiHat || kw.hasTom
                                        || name.contains("overhead") || name.contains("oh"));

        // ═══ Específicos del Messenger ═══════════════════════════════════
        kw.hasImpact   = name.contains("impact") || name.contains("hit") || name.contains("stinger");
        kw.hasDrumBus  = (name.contains("drum") && name.contains("bus")) || name.contains("bateria");
        kw.hasLeadVocal = kw.hasVocal && (name.contains("principal") || name.contains("lead")
                           || name.contains("main") || name.contains("voz"));
        kw.hasBackVocal = kw.hasVocal && (name.contains("fondo") || name.contains("back")
                           || name.contains("coro") || name.contains("double"));

        return kw;
    }

private:
    NameInferrer() = default; // Static-only class
};

} // namespace mixcoach

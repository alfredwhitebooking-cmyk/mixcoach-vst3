#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  FASE 0 — SETUP: Diálogo interactivo de bienvenida (V4 Dual Mode)
//  Separado en su propio .cpp para mantener CoachEngine.cpp manejable.
//
//  V4 FLOW:
//    NotStarted → WaitingForMode → [Mix] WaitingForGenre → WaitingForConfirm → Complete
//                                  → [Master] WaitingForDestination → Complete
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::startSetupDialogue()
{
    // ═══ Marcar que ya enviamos saludo para evitar duplicado ═══
    setupGreetingSent_ = true;

    // ═══ USUARIO RECURRENTE: Ya conocemos el nombre → saltar onboarding ═══
    if (engineerName_.isNotEmpty())
    {
        LogHelper::writeToLog("[CoachEngineSetup] Usuario recurrente: " + engineerName_
                              + " — saltando onboarding");

        juce::String msg;
        msg += "\xF0\x9F\x91\x8B **\xC2\xA1" "Bienvenido de nuevo, " + engineerName_ + "!** \xF0\x9F\x94\xA5\n\n";
        msg += "Tu setup anterior est\xC3\xA1 listo. Cu\xC3\xA9ntame qu\xC3\xA9 vamos a hacer hoy...";

        respondWithPremium(msg, MentorMessage::Type::Question);

        // Marcar setup como completo y avanzar a la primera fase
        setupStep_ = SetupStep::Complete;
        phaseManager_.advanceToNextPhase();
        auto newPhase = phaseManager_.getCurrentPhase();
        sendPhaseGuidance(newPhase);
        return;
    }

    // ═══ NUEVO USUARIO: Iniciar onboarding hardcoded (SIN LLM) ═══
    setupStep_ = SetupStep::WaitingForName;
    sendFallbackWelcome();

    LogHelper::writeToLog("[CoachEngineSetup] Nuevo usuario — iniciando onboarding");
}

void CoachEngine::sendFallbackWelcome()
{
    juce::String msg;
    msg += "\xF0\x9F\x8E\xA7 **\xC2\xA1" "Hola! Bienvenido a MixCoach.**\n\n";
    msg += "Antes de empezar, \xC2\xBF" "C\xC3\xB3mo te llamas o tienes nombre art\xC3\xADstico?\n";
    msg += "As\xC3\xAD puedo dirigirme a ti durante la sesi\xC3\xB3n.";

    respondWithPremium(msg, MentorMessage::Type::Question);
}

// ═══ Detectar nombre del ingeniero — primer paso del setup ═══════════
void CoachEngine::detectAndSetEngineerName(const juce::String& message)
{
    auto lower = message.trim();
    if (lower.isEmpty())
    {
        respondWithPremium(
            "\xC2\xBFPodr\xC3\xAD" "as decirme tu nombre o nombre art\xC3\xADstico? "
            "As\xC3\xAD puedo dirigirme a ti durante la sesi\xC3\xB3n.",
            MentorMessage::Type::Question);
        return;
    }

    // Limpiar: quitar "soy ", "me llamo ", "mi nombre es ", etc.
    juce::String cleanName = lower;
    if (cleanName.startsWithIgnoreCase("soy "))
        cleanName = cleanName.substring(4).trim();
    else if (cleanName.startsWithIgnoreCase("me llamo "))
        cleanName = cleanName.substring(9).trim();
    else if (cleanName.startsWithIgnoreCase("mi nombre es "))
        cleanName = cleanName.substring(13).trim();
    else if (cleanName.startsWithIgnoreCase("yo soy "))
        cleanName = cleanName.substring(7).trim();

    // Capitalizar primera letra
    if (cleanName.isNotEmpty())
    {
        cleanName = cleanName.substring(0, 1).toUpperCase() + cleanName.substring(1);
    }

    if (cleanName.isEmpty() || cleanName.length() > 40)
    {
        respondWithPremium(
            "\xC2\xBFPodr\xC3\xAD" "as repetirlo? Un nombre corto est\xC3\xA1 bien.",
            MentorMessage::Type::Question);
        return;
    }

    // Guardar nombre localmente
    engineerName_ = cleanName;

    // Guardar nombre via callback (AiCoachAdapter)
    if (engineerNameCallback_)
        engineerNameCallback_(cleanName);

    LogHelper::writeToLog("[CoachEngine] Nombre del ingeniero: " + cleanName);

    // Confirmar y avanzar a modo (tono natural)
    respondWithPremium(
        "\xF0\x9F\x91\x8B **\xC2\xA1" "Encantado de conocerte, " + cleanName + "!** \xF0\x9F\x8E\xA7\n\n"
        "Ahora dime, \xC2\xBFqu\xC3\xA9 vamos a hacer hoy?\n\n"
        "  \xF0\x9F\x8E\x9B **MIX** — Mezclar una canci\xC3\xB3n desde cero\n"
        "  \xF0\x9F\x8E\xB1 **MASTER** — Masterizar una mezcla terminada\n\n"
        "\xC2\xBFSuelta **\"Mix\"** o **\"Master\"** y empezamos.",
        MentorMessage::Type::Question);

    // Avanzar al siguiente paso del setup
    setupStep_ = SetupStep::WaitingForMode;
}

// ═══ Detectar modo (Mix vs Master) y bifurcar al setup correspondiente ═══
void CoachEngine::detectAndSetMode(const juce::String& message)
{
    auto lower = message.toLowerCase().trim();

    bool isMix = lower.contains("mix") || lower.contains("mezcla")
                 || lower.contains("mezclar") || lower.contains("produc")
                 || lower.contains("track") || lower.contains("pista")
                 || lower.contains("instrumento") || lower.contains("beats")
                 || lower.contains("cancion") || lower.contains("canci");

    bool isMaster = lower.contains("master") || lower.contains("masterizar")
                    || lower.contains("mastering") || lower.contains("finalizar")
                    || lower.contains("final") || lower.contains("loudness");

    // Default: if has active Messengers, assume Mix Mode
    if (!isMix && !isMaster)
    {
        int active = sharedData_.getSlotRegistry().activeCount();
        if (active > 0)
            isMix = true;
        else
            isMix = true; // Default to Mix Mode
    }

    // ═══ MIX MODE ══════════════════════════════════════════════════════════
    if (isMix && !isMaster)
    {
        coachMode_ = CoachMode::Mix;
        setupStep_ = SetupStep::WaitingForGenre;

        respondWithPremium(
            "\xF0\x9F\x8E\x9B **\xC2\xA1Modo MIX activado!** \xF0\x9F\x94\xA5\n\n"
            "Perfecto. \xC2\xBFQu\xC3\xA9 g\xC3\xA9nero vamos a mezclar?\n"
            "Dime el estilo y me adapto a \xC3\xA9l:\n"
            "Por ejemplo: **House, Reggaeton, Pop, Rock, Hip-Hop, Trap, EDM...**\n"
            "O dime el tuyo si no est\xC3\xA1 en la lista.",
            MentorMessage::Type::Question);

        LogHelper::writeToLog("[CoachEngine] Setup: Mix Mode seleccionado");
        return;
    }

    // ═══ MASTER MODE ═══════════════════════════════════════════════════════
    if (isMaster && !isMix)
    {
        coachMode_ = CoachMode::Master;
        resetMasterCooldowns();
        workflowDetector_.resetAll();
        setupStep_ = SetupStep::WaitingForDestination;

        respondWithPremium(
            "\xF0\x9F\x8E\xB1 **Modo MASTER activado!**\n\n"
            "\xC2\xBF" "Cu\xC3\xA1l es el destino de esta masterizaci\xC3\xB3n?\n\n"
            "  \xF0\x9F\x8E\xB5 **Spotify**         — -14 LUFS, -1 dBTP\n"
            "  \xF0\x9F\x8D\x8E **Apple Music**      — -16 LUFS, -1 dBTP\n"
            "  \xF0\x9F\x96\xA5 **YouTube**          — -14 LUFS, -1 dBTP\n"
            "  \xF0\x9F\x92\x83 **Club**             — -8 LUFS, -0.5 dBTP\n"
            "  \xF0\x9F\x93\xA1 **Streaming General** — -14 LUFS (gen\xC3\xA9rico)\n"
            "  \xF0\x9F\x92\xBF **CD**               — -9 LUFS, -0.1 dBTP\n\n"
            "Escribe el nombre del destino, o **/skip** para Streaming General.",
            MentorMessage::Type::Question);

        LogHelper::writeToLog("[CoachEngine] Setup: Master Mode seleccionado");
        return;
    }

    // Ambiguo — preguntar de nuevo
    setupStep_ = SetupStep::WaitingForMode;
    respondWithPremium(
        "No me qued\xC3\xB3 claro. \xC2\xBFQuieres **MIX** (mezclar una canci\xC3\xB3n)\n"
        "o **MASTER** (masterizar una mezcla terminada)?",
        MentorMessage::Type::Question);
}

// ═══ Detectar destino de Master Mode ═══════════════════════════════════
void CoachEngine::detectAndSetDestination(const juce::String& message)
{
    auto lower = message.toLowerCase().trim();

    MasterDestination dest = MasterDestination::StreamingGeneral;

    if (lower.contains("spotify"))
        dest = MasterDestination::Spotify;
    else if (lower.contains("apple") || lower.contains("music"))
        dest = MasterDestination::AppleMusic;
    else if (lower.contains("youtube") || lower.contains("yt"))
        dest = MasterDestination::YouTube;
    else if (lower.contains("club") || lower.contains("club") || lower.contains("dj"))
        dest = MasterDestination::Club;
    else if (lower.contains("cd") || lower.contains("disco"))
        dest = MasterDestination::CD;
    else
        dest = MasterDestination::StreamingGeneral;

    masterDestination_ = dest;

    setupStep_ = SetupStep::WaitingForConfirm;

    juce::String msg;
    msg += "\xF0\x9F\x93\x8C **Destino seleccionado: " + juce::String(destinationNames[static_cast<int>(dest)]) + "**\n\n";
    msg += "\xF0\x9F\x8E\xAF Target LUFS: **" + juce::String(::mixcoach::getDestinationLUFS(dest), 1) + "**\n";
    msg += "\xE2\x9A\xA0 True Peak m\xC3\xA1ximo: **" + juce::String(::mixcoach::getDestinationTruePeak(dest), 1) + " dBTP**\n\n";
    msg += "Masterizar\xC3\xA9 con estos targets en mente.\n\n";
    msg += "\xE2\x9C\x85 **Confirma:**\n";
    msg += "  - Escribe **\"si\"** o **\"confirmar\"** para continuar\n";
    msg += "  - Escribe el nombre de otro destino para cambiarlo\n";
    msg += "  - Escribe **\"/skip\"** para saltar el setup\n";

    respondWithPremium(msg, MentorMessage::Type::Question);
}

void CoachEngine::detectAndSetGenre(const juce::String& message)
{
    // Lista de generos conocidos
    const juce::StringArray knownGenres = {
        "reggaeton", "pop", "rock", "hip-hop", "hip hop", "rap", "trap",
        "edm", "electronic", "house", "techno", "trance", "dubstep",
        "jazz", "blues", "classical", "clasica", "orchestra", "orquestal",
        "latin", "latina", "bachata", "salsa", "merengue",
        "rnb", "r&b", "soul", "funk",
        "country", "folk", "indie",
        "metal", "punk", "hardcore",
        "lo-fi", "lofi", "ambient", "drone",
        "k-pop", "kpop", "j-pop", "jpop",
        "reggae", "dancehall"
    };

    auto lower = message.toLowerCase().trim();

    // Intentar detectar genero por palabra clave
    juce::String detectedGenre;
    for (auto& g : knownGenres) {
        if (lower.contains(g)) {
            detectedGenre = g;
            if (detectedGenre.isNotEmpty())
                detectedGenre = detectedGenre.substring(0, 1).toUpperCase()
                              + detectedGenre.substring(1);
            break;
        }
    }

    if (detectedGenre.isEmpty()) {
        detectedGenre = message.trim();
        if (detectedGenre.length() > 30)
            detectedGenre = detectedGenre.substring(0, 30);
    }

    setupGenre_ = detectedGenre;
    setupStep_ = SetupStep::WaitingForConfirm;

    juce::String scanResult = scanAndShowResults();

    juce::String msg;
    msg += "\xF0\x9F\x93\x8C **G\xC3\xA9" "nero detectado: " + setupGenre_ + "**\n\n";
    msg += scanResult;
    msg += "\n\n\xE2\x9C\x85 **Confirma que est\xC3\xA1" " correcto:**\n";
    msg += "  - Escribe **\"si\"** o **\"confirmar\"** para continuar\n";
    msg += "  - Escribe el nombre del g\xC3\xA9" "nero para cambiarlo\n";
    msg += "  - Escribe **\"/skip\"** para saltar el setup\n";

    respondWithPremium(msg, MentorMessage::Type::Question);
}

juce::String CoachEngine::scanAndShowResults() const
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    juce::String result;
    result += "\xF0\x9F\x8E\x9B **Pistas detectadas: " + juce::String(active) + "**\n";

    if (active == 0) {
        result += "  (Aun no hay pistas activas. Los Messengers apareceran "
                  "automaticamente cuando los cargues en tus pistas.)";
        return result;
    }

    int bussedCount = 0;
    int unnamedCount = 0;

    result += "\n";
    registry.forEachActive([&](const SlotInfo& info) {
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty()) {
            name = "Pista " + juce::String(info.slotIndex + 1);
            unnamedCount++;
        }

        juce::String busName;
        if (info.bus >= BusType::Drums && info.bus <= BusType::Melody) {
            busName = juce::String(busNames[static_cast<int>(info.bus)]);
            bussedCount++;
        }

        result += "  \xF0\x9F\x94\xB9 " + name;
        if (busName.isNotEmpty())
            result += "  [" + getBusIcon(info.bus) + " " + busName + "]";
        result += "\n";
    });

    result += "\n";
    if (unnamedCount > 0)
        result += "  \xE2\x9A\xA0 " + juce::String(unnamedCount) + " pista(s) sin nombre\n";
    if (bussedCount < active)
        result += "  \xF0\x9F\x92\xA1 " + juce::String(active - bussedCount) + " pista(s) sin bus asignado\n";

    if (active >= 3)
        result += "\n\xF0\x9F\x93\x8A Ya tienes suficientes pistas para empezar a mezclar!";

    return result;
}

void CoachEngine::advanceFromSetup()
{
    setupStep_ = SetupStep::Complete;

    // Avanzar a la siguiente fase (la 1, sea Mix o Master)
    phaseManager_.advanceToNextPhase();
    auto newPhase = phaseManager_.getCurrentPhase();

    juce::String msg;
    msg += "\xE2\x9C\x85 **Setup completado!**\n\n";

    if (isMixMode())
    {
        msg += "\xF0\x9F\x93\x8C **Modo:** MIX\n";
        msg += "\xF0\x9F\x8E\xB5 **G\xC3\xA9nero:** " + setupGenre_ + "\n";
    }
    else
    {
        msg += "\xF0\x9F\x93\x8C **Modo:** MASTER\n";
        msg += "\xF0\x9F\x8E\xAF **Destino:** " + juce::String(destinationNames[static_cast<int>(masterDestination_)]) + "\n";
        msg += "\xF0\x9F\x8E\x9B" " Target: " + juce::String(getDestinationLUFS(), 1) + " LUFS\n";
    }

    // Registrar el setup como cambio de sesión
    if (trackChangeCallback_)
        trackChangeCallback_(-1, "Sesi\xC3\xB3" "n",
            "Setup completado. Modo: " + juce::String(isMixMode() ? "Mix" : "Master")
            + ", G\xC3\xA9" "nero: " + setupGenre_, 0.0f, 0.0f);

    respondWith(msg, MentorMessage::Type::Achievement);

    // ═══ Guía activa para la fase a la que acabamos de avanzar ═══
    sendPhaseGuidance(newPhase);

    LogHelper::writeToLog("[CoachEngine] Setup completado.");
    LogHelper::writeToLog("[CoachEngine] Setup completado. Modo: "
                          + juce::String(isMixMode() ? "Mix" : "Master")
                          + ". Genero: " + setupGenre_
                          + ". Avanzando a fase "
                          + juce::String(phaseNames[static_cast<int>(newPhase)]));

    // ═══ Identity Layer: ejecutar escaneo combinado (nombre + espectral) ═══
    // Primero la inferencia determinística por nombre, que es inmediata y no requiere LLM.
    if (isMixMode() && sharedData_.getSlotRegistry().activeCount() > 0) {
        // Forzar inferencia inmediata para todas las pistas (combinada nombre + espectral)
        // inferTrackRoles() auto-muestra resumen de identidad si detecta >=2 pistas nuevas
        // Tambien llama a requestLLMRoleSuggestions() para pistas que quedaron Unknown
        inferTrackRoles();
    }
}

void CoachEngine::forceSetupComplete()
{
    if (setupGenre_.isEmpty())
        setupGenre_ = "No especificado";
    if (coachMode_ != CoachMode::Mix && coachMode_ != CoachMode::Master)
        coachMode_ = CoachMode::Mix;
    advanceFromSetup();
}

// ═══════════════════════════════════════════════════════════════════════════
//  sendPhaseGuidance — Mensaje contextual con acción concreta por fase
//  Se llama al avanzar a una nueva fase (desde advanceFromSetup y /next).
//  V4: Comportamiento bifurcado por modo (Mix vs Master).
// ═══════════════════════════════════════════════════════════════════════════
void CoachEngine::sendPhaseGuidance(MentorPhase phase)
{
    // Escoger nombre de fase según modo
    const char* phaseLabel = isMasterMode()
        ? masterPhaseNames[static_cast<int>(phase)]
        : phaseNames[static_cast<int>(phase)];

    juce::String msg;
    msg += "\xF0\x9F\x97\xBA **" + juce::String(phaseLabel) + "**\n";
    msg += "\xF0\x9F\x93\x8B " + juce::String(phaseManager_.phaseDescription(phase)) + "\n\n";

    // ═══ MASTER MODE: guía específica de masterización ══════════════════
    if (isMasterMode())
    {
        switch (phase)
        {
            case MentorPhase::Organizacion:
                msg += "\xF0\x9F\x93\x8C **Configuraci\xC3\xB3n del Master**\n\n"
                       "Configura el destino de masterizaci\xC3\xB3n y aseg\xC3\xBArate "
                       "de que solo el master llegue a MixCoach.\n\n"
                       "Target: **" + juce::String(getDestinationLUFS(), 1) + " LUFS**\n"
                       "True Peak m\xC3\xA1ximo: **" + juce::String(getDestinationTruePeak(), 1) + " dBTP**";
                break;

            case MentorPhase::GainStaging:
                msg += "\xF0\x9F\x93\x8A **An\xC3\xA1lisis de Niveles del Master**\n\n"
                       "Voy a medir:\n"
                       "  \xE2\x80\xA2 LUFS Integrado vs target\n"
                       "  \xE2\x80\xA2 True Peak y crest factor\n"
                       "  \xE2\x80\xA2 Headroom disponible\n\n"
                       "Dame un momento para analizar el master...";
                break;

            case MentorPhase::Balance:
                msg += "\xF0\x9F\x93\x8A **Balance Espectral del Master**\n\n"
                       "Voy a medir:\n"
                       "  \xE2\x80\xA2 Balance espectral (sub, bajos, medios, agudos)\n"
                       "  \xE2\x80\xA2 Correlaci\xC3\xB3n est\xC3\xA9reo\n"
                       "  \xE2\x80\xA2 Loudness Range (LRA)\n\n"
                       "Dame un momento para analizar el master...";
                break;

            case MentorPhase::EQ:
                msg += "\xF0\x9F\x94\xA7 **EQ de Master**\n\n"
                       "Basado en el an\xC3\xA1lisis, voy a sugerir:\n"
                       "  \xE2\x80\xA2 EQ sutiles para balance espectral\n"
                       "  \xE2\x80\xA2 Ajustes de presencia y aire\n\n"
                       "\xF0\x9F\x92\xA1 Hablo solo del balance espectral global, no de pistas.";
                break;

            case MentorPhase::Compresion:
                msg += "\xF0\x9F\x94\xA7 **Compresi\xC3\xB3n y Din\xC3\xA1mica**\n\n"
                       "Voy a sugerir:\n"
                       "  \xE2\x80\xA2 Compresi\xC3\xB3n suave si es necesario\n"
                       "  \xE2\x80\xA2 Limitaci\xC3\xB3n para alcanzar el LUFS target\n"
                       "  \xE2\x80\xA2 Saturaci\xC3\xB3n arm\xC3\xB3nica\n\n"
                       "\xF0\x9F\x92\xA1 No voy a hablar de kicks, snares ni pistas individuales.";
                break;

            case MentorPhase::Espacio:
                msg += "\xF0\x9F\x94\xAE **Imagen Est\xC3\xA9reo**\n\n"
                       "Voy a sugerir:\n"
                       "  \xE2\x80\xA2 Ajustes de ancho est\xC3\xA9reo\n"
                       "  \xE2\x80\xA2 Correlaci\xC3\xB3n y profundidad\n"
                       "  \xE2\x80\xA2 Automatizaci\xC3\xB3n final";
                break;

            case MentorPhase::MasterCheck:
            {
                bool hasRef = hasReference();
                msg += "\xF0\x9F\x93\x80 **Master Check**\n\n";
                if (!hasRef)
                {
                    msg += "Carga una referencia a volumen real para comparar:\n"
                           "  1. Arrastra un WAV/MP3 masterizado al panel REFERENCE\n"
                           "  2. La referencia se usar\xC3\xA1 a su loudness original\n"
                           "  3. Comparar\xC3\xA9: LUFS, True Peak, espectro, LRA\n\n"
                           "\xF0\x9F\x92\xA1 En Master Mode NO normalizamos a -6dB.";
                }
                else
                {
                    msg += "Referencia cargada: **" + getReferenceName() + "**\n"
                           "Comparando a volumen real contra tu master.\n";
                }
                msg += "\n\xF0\x9F\x8F\x86 **Veredicto Final**\n"
                       "Vamos a verificar:\n"
                       "  1. \xE2\x9C\x85 LUFS dentro del target\n"
                       "  2. \xE2\x9C\x85 Sin clipping ni distorsi\xC3\xB3n\n"
                       "  3. \xE2\x9C\x85 Balance espectral s\xC3\xB3lido\n"
                       "  4. \xE2\x9C\x85 Compatibilidad mono\n"
                       "  5. \xE2\x9C\x85 Ready para " + juce::String(destinationNames[static_cast<int>(masterDestination_)]) + "\n\n"
                       "Usa **/analisis** para el veredicto completo.";
                break;
            }

            default:
                msg += "\xF0\x9F\x92\xA1 Trabajando en el master. "
                       "Preg\xC3\xBAntame c\xC3\xB3mo van los niveles o el balance.";
                break;
        }
    }
    // ═══ MIX MODE: guía específica de mezcla ═══
    else
    {
        switch (phase)
        {
            case MentorPhase::Organizacion:
                msg += "\xF0\x9F\x8E\xAF **Acci\xC3\xB3n:** Activa **Messengers** en cada pista, asigna **rol**, **bus y nombre**:\n"
                       "  1. **Rol**: Kick, Snare, Voz, Bajo, etc.\n"
                       "  2. **Bus**: Drums, Bass, Guitars, Keys o Vocals\n"
                       "  3. **Color**: elige un color representativo por familia\n"
                       "  4. **Nombre**: nombre descriptivo (ej: \"Kick 808\", \"Voz Principal\")\n\n"
                       "\xF0\x9F\x92\xA1 Organizar ahora = mezclar mejor despu\xC3\xA9s.";
                break;

            case MentorPhase::GainStaging:
                msg += "\xF0\x9F\x94\x84 **Gain Staging**\n\n"
                       "Ajusta niveles para headroom saludable:\n"
                       "  \xE2\x80\xA2 Cada pista: picos entre -18 dB y -12 dB\n"
                       "  \xE2\x80\xA2 Master: picos entre -12 dB y -6 dB\n"
                       "  \xE2\x80\xA2 Sin clipping en ninguna pista\n\n"
                       "\xF0\x9F\x92\xA1 Solo niveles — nada de EQ ni compresi\xC3\xB3n a\xC3\xBA" "n.";
                break;

            case MentorPhase::Balance:
                msg += "\xF0\x9F\x8E\x9B **Balance de Mezcla**\n\n"
                       "Usa faders y paneo para balancear niveles relativos:\n"
                       "  \xE2\x80\xA2 El kick y bajo son la base — que se sientan s\xC3\xB3lidos\n"
                       "  \xE2\x80\xA2 La voz debe estar encima, no enterrada\n"
                       "  \xE2\x80\xA2 Busca que nada domine ni desaparezca\n"
                       "  \xE2\x80\xA2 Escucha en mono para verificar balance\n\n"
                       "\xF0\x9F\x92\xA1 Solo faders y pan — sin EQ ni compresi\xC3\xB3n todav\xC3\xAD" "a.";
                break;

            case MentorPhase::EQ:
                msg += "\xF0\x9F\x8E\x9B **Balance Tonal (EQ)**\n\n"
                       "Moldea el sonido de cada pista:\n"
                       "  \xE2\x80\xA2 HPF en pistas que no necesitan graves\n"
                       "  \xE2\x80\xA2 Carving espectral para que cada instrumento tenga su espacio\n"
                       "  \xE2\x80\xA2 Elimina enmascaramiento entre pistas que compiten\n"
                       "  \xE2\x80\xA2 EQ sutiles de 2-3 dB m\xC3\xA1ximo\n\n"
                       "\xF0\x9F\x92\xA1 Un buen EQ = menos compresi\xC3\xB3n despu\xC3\xA9s.";
                break;

            case MentorPhase::Compresion:
                msg += "\xF0\x9F\x94\xA7 **Compresi\xC3\xB3n y Din\xC3\xA1mica**\n\n"
                       "Controla la din\xC3\xA1mica de cada pista:\n"
                       "  \xE2\x80\xA2 Compresores para nivelar picos y dar consistencia\n"
                       "  \xE2\x80\xA2 Saturaci\xC3\xB3n para calidez y arm\xC3\xB3nicos\n"
                       "  \xE2\x80\xA2 Busca crest factor entre 8-14 dB\n\n"
                       "\xF0\x9F\x92\xA1 Menos es m\xC3\xA1s. No comprimas solo porque s\xC3\xAD.";
                break;

            case MentorPhase::Espacio:
                msg += "\xF0\x9F\x94\xAE **Espacio y Profundidad**\n\n"
                       "Crea la escena sonora:\n"
                       "  1. Panoramas: separa instrumentos en el campo est\xC3\xA9reo\n"
                       "  2. Reverb/Delay: profundidad y ambiente\n"
                       "  3. Automatizaci\xC3\xB3n: volumen, FX, filtros\n"
                       "  4. Verificaci\xC3\xB3n mono: suena bien en mono?\n\n"
                       "\xF0\x9F\x92\xA1 El espacio debe servir a la mezcla, no al rev\xC3\xA9s.";
                break;

            case MentorPhase::MasterCheck:
            {
                bool hasRef = hasReference();
                msg += "\xF0\x9F\x8F\x86 **Master Check**\n\n";
                if (!hasRef)
                {
                    msg += "Carga una referencia para comparar:\n"
                           "  1. Arrastra un WAV/MP3 al panel REFERENCE\n"
                           "  2. La referencia se normalizar\xC3\xA1 a -6 dBFS\n"
                           "  3. Comparar\xC3\xA9 balance espectral y din\xC3\xA1mica\n\n"
                           "\xF0\x9F\x92\xA1 En Mix Mode evitamos perseguir loudness.";
                }
                else
                {
                    msg += "Referencia cargada: **" + getReferenceName() + "**\n"
                           "Comparando balance y espectro.\n";
                }
                msg += "\n\xF0\x9F\x93\x8B Verificaci\xC3\xB3n Final:\n"
                       "  \xE2\x80\xA2 El balance es similar a la referencia?\n"
                       "  \xE2\x80\xA2 Sale bien en mono?\n"
                       "  \xE2\x80\xA2 Headroom adecuado para masterizar?\n\n"
                       "Cuando est\xC3\xA9s listo, la mezcla est\xC3\xA1 completa.";
                break;
            }

            default:
                msg += "\xF0\x9F\x92\xA1 Puedes preguntarme cualquier cosa sobre la mezcla "
                       "o escribir **/help** para ver los comandos disponibles.";
                break;
        }
    }

    respondWithPremium(msg, MentorMessage::Type::Tip);
}

// ═══════════════════════════════════════════════════════════════════════════
//  REFERENCE HELPERS — Gestion de referencias de audio
//  Implementaciones minimas para resolver linker errors.
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::setReferenceAudio(const juce::String& filePath)
{
    if (filePath.isNotEmpty())
    {
        pendingReferencePath_ = filePath;
        referenceMetadata_.type = ReferenceMetadata::Type::File;
        referenceMetadata_.name = juce::File(filePath).getFileNameWithoutExtension();
        referenceMetadata_.path = filePath;
        LogHelper::writeToLog("[CoachEngine] Referencia de audio: " + filePath);
    }
}

void CoachEngine::setReferenceURL(const juce::String& name, const juce::String& url)
{
    referenceMetadata_.type = ReferenceMetadata::Type::URL;
    referenceMetadata_.name = name;
    referenceMetadata_.path = url;
    LogHelper::writeToLog("[CoachEngine] Referencia URL: " + name + " (" + url + ")");
}

void CoachEngine::clearReferences()
{
    referenceMetadata_ = ReferenceMetadata{};
    referenceFingerprint_ = ReferenceFingerprint{};
    referenceSections_.clear();
    activeSectionIndex_ = -1;
    lastReferenceComparison_ = ReferenceComparison{};
    LogHelper::writeToLog("[CoachEngine] Referencias limpiadas");
}

juce::String CoachEngine::getReferenceName() const
{
    if (referenceMetadata_.valid())
        return referenceMetadata_.name;
    return {};
}

// ═══════════════════════════════════════════════════════════════════════════
//  BusGroupSummary — Métricas agregadas por familia de instrumentos
//  Computa promedios de peak/RMS/crest/correlation por tipo de bus
// ═══════════════════════════════════════════════════════════════════════════

std::array<BusGroupSummary, kNumBuses + 1> CoachEngine::computeBusSummaries() const
{
    std::array<BusGroupSummary, kNumBuses + 1> summaries;
    auto& registry = sharedData_.getSlotRegistry();

    // Inicializar resumenes
    for (int b = 0; b <= kNumBuses; ++b)
    {
        summaries[b].busType = static_cast<BusType>(b - 1); // -1 = None, 0..6 = Drums..Melody
        summaries[b].peakMax = -100.0f;
        summaries[b].rmsSum = -100.0f;
        summaries[b].loudestTrackPeak = -100.0f;
    }

    // Acumular por bus
    registry.forEachActive([&](const SlotInfo& info) {
        int busIdx = static_cast<int>(info.bus) + 1; // None=-1→0, Drums=0→1, ..., Melody=6→7
        if (busIdx < 0 || busIdx > kNumBuses)
            busIdx = 0;

        auto& summary = summaries[busIdx];
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0)
            return;

        float peak = juce::jmax(telem.peakLeft, telem.peakRight);
        float rms = juce::jmax(telem.rmsLeft, telem.rmsRight);

        auto& state = trackStates_[info.slotIndex];

        summary.trackCount++;
        if (peak > summary.peakMax)
        {
            summary.peakMax = peak;
            summary.loudestTrackPeak = peak;
            juce::String name = juce::String(info.trackName).trim();
            if (name.isEmpty())
                name = "Pista " + juce::String(info.slotIndex + 1);
            summary.loudestTrackName = name;
        }
        if (rms > summary.rmsSum || summary.rmsSum < -90.0f)
            summary.rmsSum = rms;

        summary.avgCrest += state.lastCrestFactor;
        summary.avgCorrelation += telem.correlation;

        // Acumular bandEnergies
        for (int b = 0; b < 30 && b < kNumSpectralBands; ++b)
        {
            if (telem.bandEnergies[b] > summary.avgBandEnergies[b])
                summary.avgBandEnergies[b] = telem.bandEnergies[b];
        }
    });

    // Promediar
    for (int b = 0; b <= kNumBuses; ++b)
    {
        if (summaries[b].trackCount > 0)
        {
            summaries[b].avgCrest /= summaries[b].trackCount;
            summaries[b].avgCorrelation /= summaries[b].trackCount;
        }
    }

    return summaries;
}

// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceDrivenEngine — Delegacion a ReferenceDrivenEngine para gaps
// ═══════════════════════════════════════════════════════════════════════════

std::vector<DomainGap> CoachEngine::getReferenceGaps() const
{
    if (!referenceFingerprint_.valid)
        return {};

    // Delegar a ReferenceDrivenEngine
    return ReferenceDrivenEngine::computeGaps(
        referenceFingerprint_, audioAnalyzer_, setupGenre_);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Helper espectral: Energía promedio en un rango de bins
//  CoachEngine::spectrumBandEnergy — miembro, diferente de la función
//  static en CoachEngineReference.cpp que usa internamente.
// ═══════════════════════════════════════════════════════════════════════════

float CoachEngine::spectrumBandEnergy(const float* spectrum,
                                       int startBin, int endBin) const noexcept
{
    if (spectrum == nullptr || startBin < 0 || endBin <= startBin
        || startBin >= kNumSpectrumBins)
        return -100.0f;

    int count = endBin - startBin;
    if (count <= 0) return -100.0f;

    float sum = 0.0f;
    int n = juce::jmin(endBin, kNumSpectrumBins);
    for (int i = startBin; i < n; ++i)
        sum += spectrum[i];

    return sum / static_cast<float>(count);
}

} // namespace mixcoach

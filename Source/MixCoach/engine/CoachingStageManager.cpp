#include "CoachingStageManager.h"
#include "CoachEngine.h"
#include "PhaseManager.h"
#include "RefinementProfile.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  StageInfo — Metadatos estáticos de cada etapa
    // ═══════════════════════════════════════════════════════════════════════════

    const char* StageInfo::name(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging: return "Gain Staging";
            case CoachingStage::Balance:     return "Balance";
            case CoachingStage::EQ:          return "EQ";
            case CoachingStage::Compression: return "Compresión";
            case CoachingStage::Spatial:     return "Espacial";
            case CoachingStage::Automation:  return "Automatización";
            case CoachingStage::Refinement:  return "Refinamiento";
            default:                         return "";
        }
    }

    const char* StageInfo::icon(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging: return "🎚️";
            case CoachingStage::Balance:     return "⚖️";
            case CoachingStage::EQ:          return "🎛️";
            case CoachingStage::Compression: return "📈";
            case CoachingStage::Spatial:     return "🌊";
            case CoachingStage::Automation:  return "⏱️";
            case CoachingStage::Refinement:  return "✨";
            default:                         return "";
        }
    }

    const char* StageInfo::description(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging:
                return "Ajusta los niveles de ganancia de cada pista para tener headroom saludable "
                       "(-18 dB a -3 dB de pico) sin clipping.";
            case CoachingStage::Balance:
                return "Balancea faders y paneo: niveles relativos entre instrumentos, "
                       "sin procesar aún. Busca un balance claro antes de ecualizar.";
            case CoachingStage::EQ:
                return "Corrige el balance tonal: aplica EQ por pista y bus, "
                       "haz carving espectral, elimina enmascaramiento entre instrumentos.";
            case CoachingStage::Compression:
                return "Controla la dinámica: aplica compresores y saturadores, "
                       "busca un crest factor saludable (6-14 dB).";
            case CoachingStage::Spatial:
                return "Crea profundidad y espacio: reverb, delay, ancho estéreo y panoramas.";
            case CoachingStage::Automation:
                return "Ajusta la dinámica de secciones: LUFS, loudness, automatización de "
                       "volumen y efectos entre verso y coro para mantener el interés.";
            case CoachingStage::Refinement:
                return "Refinamiento artístico: profundidad, impacto, movimiento, "
                       "pegamento y emoción. La mezcla ya suena bien técnicamente.";
            default:
                return "";
        }
    }

    const char* StageInfo::checklist(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging:
                return "  ✅ Sin clipping en ninguna pista\n"
                       "  ✅ Headroom entre -18 dB y -3 dB\n"
                       "  ✅ Ninguna pista cerca del límite";
            case CoachingStage::Balance:
                return "  ✅ Menos del 30% de pares desbalanceados\n"
                       "  ✅ Niveles relativos coherentes entre instrumentos\n"
                       "  ✅ Paneo básico aplicado";
            case CoachingStage::EQ:
                return "  ✅ Balance tonal revisado\n"
                       "  ✅ Sin excesos graves/presencia\n"
                       "  ✅ Sin enmascaramiento crítico\n"
                       "  ✅ Pendiente espectral aceptable";
            case CoachingStage::Compression:
                return "  ✅ Crest factor promedio entre 6-14 dB\n"
                       "  ✅ Compresores ajustados por rol\n"
                       "  ✅ Sin sobrecompresión general";
            case CoachingStage::Spatial:
                return "  ✅ Correlación entre 0.3 y 0.8\n"
                       "  ✅ Efectos espaciales aplicados\n"
                       "  ✅ Ancho estéreo coherente";
            case CoachingStage::Automation:
                return "  ✅ LUFS consistente entre secciones\n"
                       "  ✅ Automatización de volumen aplicada\n"
                       "  ✅ Transiciones verso/coro suaves\n"
                       "  ✅ Rango de loudness controlado";
            case CoachingStage::Refinement:
                return "  ✅ MixScore >= 70\n"
                       "  ✅ Perfil de refinamiento activo\n"
                       "  ✅ Scores de profundidad, impacto, emoción evaluados";
            default:
                return "";
        }
    }

    const char* StageInfo::guidanceMessage(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging:
                return "**🎚️ Gain Staging — Ajuste de niveles**\n\n"
                       "Vamos a asegurarnos de que todas las pistas tengan un nivel saludable "
                       "antes de procesar.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Baja el fader de ganancia (trim) de cada pista\n"
                       "  2. Busca picos entre **-18 dB y -3 dB** en el submix\n"
                       "  3. Ninguna pista debe recortar (clipping)\n"
                       "  4. Las baterías y percusiones pueden tener picos más altos\n\n"
                       "**Por qué es importante:**\n"
                       "  El headroom correcto evita distorsión y te da margen para "
                       "EQ, compresores y efectos sin saturar el master.\n\n"
                       "  *Pregúntame \"¿Hay clipping?\" o \"Revisa niveles\" para un análisis.*";

            case CoachingStage::Balance:
                return "**⚖️ Balance — Niveles relativos**\n\n"
                       "Ahora que los niveles de ganancia están sanos, balanceemos "
                       "la mezcla.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Ajusta faders de volumen (no gain) para balancear instrumentos\n"
                       "  2. El bombo y el bajo deben tener presencia similar\n"
                       "  3. La voz principal debe destacar sin dominar\n"
                       "  4. Aplica paneo básico para separar instrumentos\n"
                       "  5. No proceses aún — solo niveles y paneo\n\n"
                       "**Por qué es importante:**\n"
                       "  Un buen balance antes de procesar evita compensar con EQ "
                       "o compresión lo que debería ser un fader.\n\n"
                       "  *Pregúntame \"¿Cómo está el balance?\" para un análisis.*";

            case CoachingStage::EQ:
                return "**🎛️ EQ — Balance Tonal**\n\n"
                       "Ahora vamos a esculpir el sonido de cada pista con EQ.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Ecualiza por pista: limpia frecuencias problemáticas\n"
                       "  2. Aplica filtros pasa altos donde sea necesario\n"
                       "  3. Haz carving espectral: cada instrumento en su rango\n"
                       "  4. Verifica que no haya enmascaramiento entre pistas\n"
                       "  5. Usa el Analyzer (panel Analysis) para ver el espectro\n\n"
                       "**Por qué es importante:**\n"
                       "  El EQ correcto evita que los instrumentos compitan por "
                       "el mismo espacio espectral. Cada elemento debe tener su lugar.\n\n"
                       "  *Pregúntame \"¿Cómo está el espectro?\" para un análisis.*";

            case CoachingStage::Compression:
                return "**📈 Compresión — Control Dinámico**\n\n"
                       "Hora de controlar la dinámica con compresores y saturadores.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Aplica compresión por pista donde sea necesaria\n"
                       "  2. Comprime buses (batería, bajo, voces) para pegar\n"
                       "  3. Busca crest factor saludable (6-14 dB)\n"
                       "  4. No sobrecomprimas: la dinámica natural es buena\n"
                       "  5. Saturación suave para armónicos y calidez\n\n"
                       "**Por qué es importante:**\n"
                       "  La compresión controlada da consistencia y pegada. "
                       "Demasiada compresión mata la vida de la mezcla.\n\n"
                       "  *Pregúntame \"¿Demasiada compresión?\" para un análisis.*";

            case CoachingStage::Spatial:
                return "**🌊 Espacial — Profundidad y Ambiente**\n\n"
                       "Ahora creemos espacio tridimensional en la mezcla.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Aplica reverb y delay a elementos seleccionados\n"
                       "  2. Crea profundidad: seco al frente, húmedo atrás\n"
                       "  3. Verifica correlación estéreo (0.3 a 0.8)\n"
                       "  4. Reverb de sala en buses para pegamento\n\n"
                       "**Por qué es importante:**\n"
                       "  El espacio bien aplicado da profesionalismo. "
                       "Demasiado arruina la claridad; muy poco suena seco.\n\n"
                       "  *Pregúntame \"¿Cómo está el espacio?\" para un análisis.*";

            case CoachingStage::Automation:
                return "**⏱️ Automatización — Loudness y Secciones**\n\n"
                       "Ahora vamos a darle movimiento y dinámica a la mezcla "
                       "a través de automatización y control de loudness.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Revisa el LUFS de cada sección (verso vs coro)\n"
                       "  2. Automatiza volumen para mantener la voz frontal\n"
                       "  3. Aplica automatización de efectos (reverb, delay)\n"
                       "  4. Verifica que las transiciones sean suaves\n"
                       "  5. Busca rango de loudness coherente\n\n"
                       "**Por qué es importante:**\n"
                       "  La automatización le da vida a la mezcla. "
                       "Una canción sin movimiento suena estática y aburrida, "
                       "incluso si técnicamente es correcta.\n\n"
                       "  *Pregúntame \"¿Cómo está el LUFS?\" para un análisis.*";

            case CoachingStage::Refinement:
                return "**✨ Refinamiento — Calidad Artística**\n\n"
                       "La mezcla ya suena bien técnicamente. Ahora hablemos de "
                       "que suene **inolvidable**.\n\n"
                       "**Qué hacer:**\n"
                       "  1. Escucha la mezcla completa y toma notas\n"
                       "  2. Compara con tu referencia musical\n"
                       "  3. Ajusta detalles: automatización fina, saturadores\n"
                       "  4. Busca profundidad, impacto, movimiento\n"
                       "  5. Pregúntate: ¿transmite la emoción correcta?\n\n"
                       "**Por qué es importante:**\n"
                       "  La técnica es el piso. El arte es el techo. "
                       "Aquí es donde una mezcla buena se vuelve profesional.\n\n"
                       "  *Dime \"¿cómo suena?\" para un análisis de refinamiento.*";

            default:
                return "";
        }
    }

    const char* StageInfo::approvalMessage(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging:
                return "**✅ Gain Staging completado**\n\n"
                       "Los niveles están saludables. ¿Listo para pasar a "
                       "**Balance** y ajustar faders y paneo?";

            case CoachingStage::Balance:
                return "**✅ Balance completado**\n\n"
                       "Los niveles relativos suenan bien. ¿Listo para pasar a "
                       "**EQ** y esculpir el balance tonal?";

            case CoachingStage::EQ:
                return "**✅ EQ completado**\n\n"
                       "El balance tonal está encaminado. ¿Listo para pasar a "
                       "**Compresión** y controlar la dinámica?";

            case CoachingStage::Compression:
                return "**✅ Compresión completada**\n\n"
                       "La dinámica está bajo control. ¿Listo para pasar a "
                       "**Espacial** y crear profundidad?";

            case CoachingStage::Spatial:
                return "**✅ Espacial completado**\n\n"
                       "El espacio y la profundidad están trabajados. ¿Listo para pasar a "
                       "**Automatización** y darle movimiento a la mezcla?";

            case CoachingStage::Automation:
                return "**✅ Automatización completada**\n\n"
                       "El loudness y las transiciones están trabajados. ¿Listo para pasar a "
                       "**Refinamiento** y pulir los detalles artísticos?";

            case CoachingStage::Refinement:
                return "**✅ Refinamiento completado**\n\n"
                       "¡Has completado todas las etapas! La mezcla está lista "
                       "para comparar con referencia y escuchar en diferentes sistemas.";

            default:
                return "";
        }
    }

    const char* StageInfo::welcomeMessage(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging:
                return "🎚️ **¡Bienvenido a Gain Staging!** Vamos a ajustar niveles.";

            case CoachingStage::Balance:
                return "⚖️ **¡Bienvenido a Balance!** Ajustemos faders y paneo.";

            case CoachingStage::EQ:
                return "🎛️ **¡Bienvenido a EQ!** Esa esculpamos el balance tonal.";

            case CoachingStage::Compression:
                return "📈 **¡Bienvenido a Compresión!** Controlemos la dinámica.";

            case CoachingStage::Spatial:
                return "🌊 **¡Bienvenido a Espacial!** Creamos profundidad y ambiente.";

            case CoachingStage::Automation:
                return "⏱️ **¡Bienvenido a Automatización!** Démosle movimiento a la mezcla.";

            case CoachingStage::Refinement:
                return "✨ **¡Bienvenido a Refinamiento!** Ahora hablemos de arte.";

            default:
                return "";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingStageManager Implementation
    // ═══════════════════════════════════════════════════════════════════════════

    CoachingStageManager::CoachingStageManager(PhaseManager& phaseManager)
        : phaseManager_(phaseManager)
    {
        for (auto& flag : stageCompletedFlags_)
            flag = false;
    }

    void CoachingStageManager::initialize(CoachingStage startStage, bool autoRequestApproval)
    {
        initialized_ = true;
        currentStage_ = startStage;
        autoRequestApproval_ = autoRequestApproval;
        awaitingApproval_ = false;

        for (auto& flag : stageCompletedFlags_)
            flag = false;

        LogHelper::writeToLog("[CoachingStageManager] Initialized: "
                              + juce::String(StageInfo::name(startStage)));
    }

    void CoachingStageManager::reset()
    {
        initialized_ = false;
        currentStage_ = CoachingStage::GainStaging;
        awaitingApproval_ = false;

        for (auto& flag : stageCompletedFlags_)
            flag = false;

        LogHelper::writeToLog("[CoachingStageManager] Reset");
    }

    bool CoachingStageManager::isStageCompleted(CoachingStage stage) const noexcept
    {
        auto idx = static_cast<int>(stage);
        if (idx < 0 || idx >= static_cast<int>(CoachingStage::COUNT))
            return false;
        return stageCompletedFlags_[idx];
    }

    bool CoachingStageManager::isComplete() const noexcept
    {
        for (auto flag : stageCompletedFlags_)
            if (!flag) return false;
        return initialized_;
    }

    float CoachingStageManager::getStageProgress() const noexcept
    {
        if (!initialized_) return 0.0f;

        if (stageCompletedFlags_[static_cast<int>(currentStage_)])
            return 1.0f;

        // Refinement progress: desde RefinementProfile en vez de PhaseManager::MasterCheck
        if (currentStage_ == CoachingStage::Refinement) {
            // No tenemos acceso al RefinementProfile aquí (es const, sin engine ref)
            // Retornamos 0 — CoachEngine puede sobreescribir este valor externamente
            // si necesita progreso real de refinamiento.
            return 0.0f;
        }

        return phaseManager_.getPhaseProgress(coachingStageToMentorPhase(currentStage_));
    }

    float CoachingStageManager::getOverallProgress() const noexcept
    {
        if (!initialized_) return 0.0f;

        int total = static_cast<int>(CoachingStage::COUNT);
        float progress = 0.0f;

        for (int i = 0; i < total; ++i) {
            if (stageCompletedFlags_[i])
                progress += 1.0f;
            else if (static_cast<CoachingStage>(i) == currentStage_)
                progress += getStageProgress();
        }

        return juce::jlimit(0.0f, 1.0f, progress / static_cast<float>(total));
    }

    void CoachingStageManager::update(const CoachEngine& engine, const AudioAnalyzer& analyzer)
    {
        juce::ignoreUnused(analyzer);

        if (!initialized_ || awaitingApproval_)
            return;

        // Si la etapa ya está marcada como completa, no hacer nada
        if (stageCompletedFlags_[static_cast<int>(currentStage_)])
            return;

        // Evaluar si la etapa actual está completa
        bool stageComplete = evaluateStageCompletion(engine);

        if (stageComplete) {
            stageCompletedFlags_[static_cast<int>(currentStage_)] = true;

            LogHelper::writeToLog("[CoachingStageManager] Stage complete: "
                                  + juce::String(StageInfo::name(currentStage_)));

            if (autoRequestApproval_) {
                awaitingApproval_ = true;
                sendApprovalRequest();
            }
        }
    }

    bool CoachingStageManager::evaluateStageCompletion(const CoachEngine& engine) const noexcept
    {
        juce::ignoreUnused(engine);

        // Delegar a PhaseManager para GainStaging → Spatial
        if (currentStage_ != CoachingStage::Refinement) {
            auto mp = coachingStageToMentorPhase(currentStage_);
            return phaseManager_.isPhaseComplete(mp);
        }

        // Refinement: usar RefinementProfile
        // Refinement está completo cuando el perfil de refinamiento es relevante
        // y el usuario ha trabajado en los 5 dominios
        auto refine = engine.getCachedRefinementProfile();
        if (!refine.valid || !refine.isRelevant)
            return false;

        // Refinement se considera completo cuando el score general es >= 75%
        // y al menos 3 de los 5 dominios están por encima de 60%
        int healthyDomains = 0;
        float scores[5] = { refine.depth.score, refine.impact.score,
                            refine.movement.score, refine.glue.score, refine.emotion.score };
        for (float s : scores) {
            if (s >= 0.60f) healthyDomains++;
        }

        return refine.overallRefinement >= 0.75f && healthyDomains >= 3;
    }

    void CoachingStageManager::sendMessage(const juce::String& text, MentorMessage::Type type)
    {
        if (sendMessageCb_)
            sendMessageCb_(text, type);
    }

    void CoachingStageManager::sendStageGuidance()
    {
        const char* msg = StageInfo::guidanceMessage(currentStage_);
        if (msg != nullptr && std::strlen(msg) > 0) {
            sendMessage(juce::String(msg), MentorMessage::Type::Tip);
        }
    }

    void CoachingStageManager::sendApprovalRequest()
    {
        const char* msg = StageInfo::approvalMessage(currentStage_);
        if (msg != nullptr && std::strlen(msg) > 0) {
            sendMessage(juce::String(msg), MentorMessage::Type::Question);
            LogHelper::writeToLog("[CoachingStageManager] Approval requested for: "
                                  + juce::String(StageInfo::name(currentStage_)));
        }
    }

    void CoachingStageManager::sendStageWelcome()
    {
        const char* msg = StageInfo::welcomeMessage(currentStage_);
        if (msg != nullptr && std::strlen(msg) > 0) {
            sendMessage(juce::String(msg), MentorMessage::Type::Achievement);
        }
    }

    bool CoachingStageManager::approveAdvance()
    {
        if (!awaitingApproval_) {
            LogHelper::writeToLog("[CoachingStageManager] approveAdvance() called but not awaiting approval");
            return false;
        }

        awaitingApproval_ = false;

        auto oldStage = currentStage_;
        auto nextIdx = static_cast<int>(currentStage_) + 1;

        if (nextIdx >= static_cast<int>(CoachingStage::COUNT)) {
            // Ya estamos en Refinement y completado
            LogHelper::writeToLog("[CoachingStageManager] All stages complete!");
            return false;
        }

        currentStage_ = static_cast<CoachingStage>(nextIdx);

        // ═══ Mantener PhaseManager sincronizado con CoachingStage ═══
        // Si PhaseManager está atrasado (ej: nunca se auto-avanzó), lo avanzamos
        // para que la pipeline de análisis use la MentorPhase correcta.
        {
            auto currentMentorPhase = phaseManager_.getCurrentPhase();
            auto expectedMentorPhase = coachingStageToMentorPhase(currentStage_);
            // Solo avanzar si PhaseManager está detrás
            if (static_cast<int>(currentMentorPhase) < static_cast<int>(expectedMentorPhase)) {
                while (phaseManager_.getCurrentPhase() < expectedMentorPhase)
                    phaseManager_.advanceToNextPhase();
                LogHelper::writeToLog("[CoachingStageManager] Synced PhaseManager: "
                                      + juce::String(phaseNames[static_cast<int>(currentMentorPhase)]) + " -> "
                                      + juce::String(phaseNames[static_cast<int>(expectedMentorPhase)]));
            }
        }

        LogHelper::writeToLog("[CoachingStageManager] Advanced: "
                              + juce::String(StageInfo::name(oldStage)) + " -> "
                              + juce::String(StageInfo::name(currentStage_)));

        // Enviar bienvenida y guía de la nueva etapa
        sendStageWelcome();
        sendStageGuidance();

        // Disparar callback
        if (stageChangedCb_)
            stageChangedCb_(oldStage, currentStage_);

        return true;
    }

    void CoachingStageManager::rejectAdvance()
    {
        awaitingApproval_ = false;
        // Reactivar la etapa para que pueda volver a evaluarse
        stageCompletedFlags_[static_cast<int>(currentStage_)] = false;

        // ═══ Cooldown anti-bucle: no volver a evaluar completitud por 120s ═══
        approvalRejectedAtUs_ = juce::Time::getMillisecondCounter() * 1000;

        sendMessage(
            "**Sin prisa.** Tómate el tiempo que necesites con esta etapa.\n\n"
            + juce::String(StageInfo::checklist(currentStage_))
            + "\n\nCuando estés listo, solo dime **\"listo\"** y avanzamos.\n\n"
            "También puedes preguntarme cualquier cosa sobre la mezcla "
            "o escribir **/next** para avanzar manualmente.",
            MentorMessage::Type::Tip);

        LogHelper::writeToLog("[CoachingStageManager] Advance rejected, staying at: "
                              + juce::String(StageInfo::name(currentStage_))
                              + " (cooldown 120s)");
    }

    bool CoachingStageManager::forceAdvance()
    {
        if (!initialized_) {
            initialize();
            return true;
        }

        auto oldStage = currentStage_;
        auto nextIdx = static_cast<int>(currentStage_) + 1;

        if (nextIdx >= static_cast<int>(CoachingStage::COUNT)) {
            sendMessage("🎉 ¡Todas las etapas están completas!", MentorMessage::Type::Achievement);
            return false;
        }

        // Marcar etapa actual como completa
        stageCompletedFlags_[static_cast<int>(currentStage_)] = true;
        awaitingApproval_ = false;

        currentStage_ = static_cast<CoachingStage>(nextIdx);

        // ═══ Mantener PhaseManager sincronizado con CoachingStage ═══
        {
            auto currentMentorPhase = phaseManager_.getCurrentPhase();
            auto expectedMentorPhase = coachingStageToMentorPhase(currentStage_);
            if (static_cast<int>(currentMentorPhase) < static_cast<int>(expectedMentorPhase)) {
                while (phaseManager_.getCurrentPhase() < expectedMentorPhase)
                    phaseManager_.advanceToNextPhase();
                LogHelper::writeToLog("[CoachingStageManager] Synced PhaseManager: "
                                      + juce::String(phaseNames[static_cast<int>(currentMentorPhase)]) + " -> "
                                      + juce::String(phaseNames[static_cast<int>(expectedMentorPhase)]));
            }
        }

        LogHelper::writeToLog("[CoachingStageManager] Force advanced: "
                              + juce::String(StageInfo::name(oldStage)) + " -> "
                              + juce::String(StageInfo::name(currentStage_)));

        sendStageWelcome();
        sendStageGuidance();

        if (stageChangedCb_)
            stageChangedCb_(oldStage, currentStage_);

        return true;
    }

} // namespace mixcoach

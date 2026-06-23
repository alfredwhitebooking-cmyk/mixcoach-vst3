#include "AiCoachAdapter.h"
#include "../../Common/types/LogHelper.h"
#include "../engine/MixScore.h"

namespace mixcoach {

// ===========================================================================
//  buildSystemPrompt - System prompt for the LLM
//  Defines the AI coach's identity, personality, and phase-specific instructions
// ===========================================================================
juce::String AiCoachAdapter::buildSystemPrompt() const
{
    juce::String prompt;
    LogHelper::writeToLog("[DIAG] buildSystemPrompt() ENTRY");

    if (coachEngine_.isMasterMode())
    {
        prompt += "=== SISTEMA: MIXCOACH MASTER MODE ===\n";
        prompt += "Tu ERES MixCoach en modo MASTERING, un ingeniero de mastering con 20+ anos incrustado en el Master de FL Studio.\n";
        prompt += "NO eres un chatbot ni un asistente externo. Eres parte del sistema.\n";
        prompt += "En este modo, trabajas SOLO con el audio final del master bus: balance espectral global, LUFS, True Peak, LRA, correlacion, estereo, profundidad.\n";
        prompt += "Nunca hablas de kicks, snares, voces ni instrumentos individuales.\n\n";
        prompt += "TUS SENTIDOS (en vivo, se actualizan cada ~8s):\n";
        prompt += "  - AudioAnalyzer en Master (LUFS, True Peak, FFT 16384, correlacion estereo, LRA)\n";
        prompt += "  - Referencia cargada: " + (coachEngine_.hasReference() ? coachEngine_.getReferenceName() : "ninguna") + "\n";
        prompt += "  - Modo Master: destino " + juce::String(destinationNames[static_cast<int>(coachEngine_.getMasterDestination())]) + "\n";
        prompt += "    Target: " + juce::String(coachEngine_.getDestinationLUFS(), 1) + " LUFS | True Peak max: " + juce::String(coachEngine_.getDestinationTruePeak(), 1) + " dBTP\n";
        prompt += "  - Fase actual: " + phaseLabel(phaseManager_.getCurrentPhase()) + "\n\n";

        prompt += "[MASTER GUIDE]\n";
        prompt += "Interpreta los datos asi:\n";
        prompt += "  LUFS Integrated > target +0.5 → mezcla demasiado caliente para el destino\n";
        prompt += "  LUFS Integrated < target -3.0 → hay espacio para subir\n";
        prompt += "  True Peak > target → riesgo de distortion al convertir a lossy\n";
        prompt += "  LRA > 12 LU → muy dinamico para el genero, podria sonar debil en streaming\n";
        prompt += "  Correlacion < 0.3 → posible problema de fase mono\n";
        prompt += "  StereoWidth > 0.7 → estereo extremo, revisar mono compatibilidad\n";
        prompt += "\n";

        prompt += "PERSONALIDAD (Mastering Engineer):\n";
        prompt += "- Hablas en terminos de formato final: \"Oye, para streaming esto necesita...\"\n";
        prompt += "- Tus recomendaciones incluyen el target exacto y la diferencia actual.\n";
        prompt += "- Siempre dices cuantos dB faltan: \"Te falta 1.2 LUFS para llegar a -14\"\n";
        prompt += "- Emojis con moderacion. Nada de estructurar respuestas.\n\n";

        prompt += "FASE ACTUAL: " + phaseLabel(phaseManager_.getCurrentPhase()) + "\n";
        prompt += "  Enfoque: " + juce::String(phaseManager_.getPhaseDescription(phaseManager_.getCurrentPhase())) + "\n";
        prompt += "  Prioriza: LUFS target > True Peak > LRA > correlacion > estereo > balance espectral\n\n";
    }
    else
    {
        prompt += "=== SISTEMA: MIXCOACH ===\n";
        prompt += "Tu ERES MixCoach, un ingeniero de mezcla con 20+ anos incrustado en el Master de FL Studio.\n";
        prompt += "NO eres un chatbot ni un asistente externo. Eres parte del sistema.\n";
        prompt += "Eres el copiloto del ingeniero. El usuario tiene los faders, tu tienes el criterio.\n\n";
        prompt += "TUS SENTIDOS (en vivo, se actualizan cada ~8s):\n";
        prompt += "  - " + juce::String(sharedData_.getSlotRegistry().activeCount()) + " Messengers activos (peak, RMS, correlacion, crest factor, 30-band spectrum)\n";
        prompt += "  - AudioAnalyzer en Master (LUFS, True Peak, FFT 16384, correlacion estereo, LRA, StereoWidth)\n";
        prompt += "  - Referencia cargada: " + (coachEngine_.hasReference() ? coachEngine_.getReferenceName() : "ninguna") + "\n";
        prompt += "  - Fase actual: " + phaseLabel(phaseManager_.getCurrentPhase()) + "\n\n";

        // ── Reference Match Guide ──
        if (coachEngine_.hasReference())
        {
            // ── REFERENCE-DRIVEN MODE: La referencia como norte absoluto ──
            if (coachEngine_.isReferenceDrivenMode())
            {
                prompt += "⭐ [REFERENCE-DRIVEN MODE ACTIVO] ⭐\n";
                prompt += "La referencia ES EL NORTE ABSOLUTO de todas las recomendaciones.\n";
                prompt += "Cada sugerencia debe medirse contra la referencia:\n";
                prompt += "  - Si el match actual es <50%: enfocate en los gaps mas grandes PRIMERO.\n";
                prompt += "  - Si el match es 50-75%: refina las regiones que aun tienen gaps >3dB.\n";
                prompt += "  - Si el match es >75%: ajustes finos, celebra el progreso.\n";
                prompt += "  - Siempre menciona el match actual: \"Estamos al X% de la referencia\"\n";
                prompt += "  - La tendencia importa: si el match esta mejorando, refuerza; si empeora, avisa.\n";
                prompt += "  - Prioriza los gaps en este orden: GANANCIA > TONAL > DINAMICA > ESPACIAL > LOUDNESS\n";
                prompt += "\n";

                // Añadir progreso actual al system prompt
                auto refProgress = coachEngine_.getReferenceProgress();
                if (refProgress.hasAudio)
                {
                    prompt += juce::String("[PROGRESO ACTUAL]\n");
                    prompt += juce::String("  Match: ") + juce::String(static_cast<int>(refProgress.currentMatch * 100.0f)) + juce::String("%\n");
                    prompt += juce::String("  Tendencia: ") + juce::String(refProgress.trendEmoji()) + juce::String(" ") + juce::String(refProgress.trendLabel()) + juce::String("\n");
                    prompt += juce::String("  Gaps: ") + juce::String(refProgress.criticalGaps) + juce::String(" criticos, ")
                              + juce::String(refProgress.warningGaps) + juce::String(" warnings\n");
                    prompt += "\n";
                }
            }

            prompt += "[REFERENCE MATCH GUIDE]\n";
            prompt += "Si hay datos de DifferenceProfile abajo, usalos asi:\n";
            prompt += "  - Match Score (0-100): que tan cerca esta la mezcla de la referencia. <50 = muy lejos, >80 = cerca.\n";
            prompt += "  - DeltaRegionEnergy: diferencia por region espectral. POSITIVO = la referencia tiene MAS energia.\n";
            prompt += "  - GapPriorities: que regiones requieren atencion URGENTE primero.\n";
            prompt += "Como usar la referencia:\n";
            prompt += "  1. Si una region muestra delta >4dB -> recomienda ajustar para acercarse a la referencia\n";
            prompt += "  2. Si la referencia tiene mas sub/bajo que el mix -> el usuario necesita mas peso en esa zona\n";
            prompt += "  3. Si hay LUFS gap >2 LUFS -> priorizar nivel general antes de balance tonal\n";
            prompt += "  4. No digas \"la referencia tiene X\". Di cosas como: \"Escucha... respecto a la ref, el bajo necesita un poco mas de cuerpo, unos 2dB alrededor de 120Hz\"\n";
            prompt += "\n";
        }

        // ── Track-Specific Coaching ──
        prompt += "[TRACK-SPECIFIC COACHING]\n";
        prompt += "Los datos de [TRACKS] y [WORKFLOW EVENTS] te dicen QUE pistas estan activas.\n";
        prompt += "Usa el rol de cada pista para dar consejos contextuales:\n";
        prompt += "  - Kick: habla de ataque (60-100Hz), sub (40-60Hz), click (3-5kHz)\n";
        prompt += "  - Snare: cuerpo (200-400Hz), crack (5-8kHz)\n";
        prompt += "  - 808/Bass: fundamental (40-100Hz), harmonics (100-300Hz)\n";
        prompt += "  - Voz: presencia (3-6kHz), cuerpo (200-500Hz), sibilancia (6-10kHz)\n";
        prompt += "  - HiHat/Platos: aire (8-12kHz), cuerpo (200-400Hz)\n";
        prompt += "  - Guitarras: mordiente (2-5kHz), cuerpo (200-800Hz)\n";
        prompt += "  - Pad/Keys: calidez (200-500Hz), brillo (5-10kHz)\n";
        prompt += "La confianza del rol (✅ ≥75%, ⚠ ≥40%, ❌ <40%) te dice si el rol inferido es fiable.\n";
        prompt += "Si la confianza es baja, no asumas el instrumento. Di algo como: \"esta pista suena a...\"\n";
        prompt += "\n";

        // ── Track Diagnosis Guide (Sprint 6) ──
        prompt += "[TRACK DIAGNOSIS GUIDE]\n";
        prompt += "[TRACK DIAGNOSIS] contiene el analisis PRE-CALCULADO por pista con targets del rol.\n";
        prompt += "Estos datos son mas precisos que tu interpretacion manual de los datos crudos en [TRACKS].\n";
        prompt += "Reglas:\n";
        prompt += "  - Si dice \"OffTarget\" CONFIAG Y PRIORITALO. El target es el del rol (kick != vocal).\n";
        prompt += "  - Si dice \"Sobre-comprimido (crest X vs Y)\" → ese Y es el target real del instrumento.\n";
        prompt += "  - Si dice \"demasiado bajo (-14 dBFS, target -6.0)\" → el usuario necesita subir ~8 dB.\n";
        prompt += "  - Si NO aparece [TRACK DIAGNOSIS] (sin rol asignado), usa los datos crudos de [TRACKS].\n";
        prompt += "  - Comienza tu respuesta por la pista con el issue mas critico (OffTarget rojo primero).\n";
        prompt += "  - 0 tokens: esta seccion ya se calculo en C++, no la recalculates.\n";
        prompt += "\n";

        // ── Phase-Specific Priority ──
        prompt += "FASE ACTUAL: " + phaseLabel(phaseManager_.getCurrentPhase()) + "\n";
        prompt += "  Enfoque: " + juce::String(phaseManager_.getPhaseDescription(phaseManager_.getCurrentPhase())) + "\n";
        prompt += "  Prioriza: clipping > balance > EQ > compresion > efectos\n";
        prompt += "  NO adelantes trabajo de fases futuras. Si el usuario insiste, sugierelo pero marca que es para despues.\n";
        prompt += "  Si el usuario pregunta de otra fase, responde naturalmente. No seas rigido.\n";
        prompt += "  Las restricciones exactas de la fase actual estan en [SESSION CONTEXT] > PhaseRestrictions. SIGUELAS.\n\n";

        // ── LUFS / Level Interpretation ──
        prompt += "[LEVEL INTERPRETATION]\n";
        prompt += "Cuando veas los peaks de las pistas en [TRACKS], interpretalos asi:\n";
        prompt += "  - Peak > -0.5dB -> CLIPPING. Prioridad #1.\n";
        prompt += "  - Peak -1 a -3dB -> muy caliente, apenas respira. Bajale 2-3dB de gain.\n";
        prompt += "  - Peak -6 a -10dB -> rango sano para mezclar, headroom suficiente.\n";
        prompt += "  - Peak -12 a -18dB -> nivel moderado. OK si es un pad o reverb, bajo si es kick.\n";
        prompt += "  - Peak < -20dB -> demasiado bajo. Probablemente necesita gain staging.\n";
        prompt += "  - Crest < 6dB -> comprimido. Crest > 20dB -> muy dinamico.\n";
        prompt += "  - Correlation < 0.2 -> posible problema de fase. Correlation > 0.9 -> muy mono.\n";
        prompt += "\n";

        // ── Mix mode Personalidad ──
        prompt += "[PERSONALIDAD]\n";
        prompt += "Hablas como un ingeniero en el estudio:\n";
        prompt += "  - \"Oye, escucha esto...\" en vez de \"Permiteme sugerirte...\"\n";
        prompt += "  - \"Mira, el kick esta peleando con el bajo...\" en vez de \"Hay una colision de frecuencias...\"\n";
        prompt += "  - \"Siii, asi suena mejor! Ese cambio funciona\" cuando el usuario mejora algo\n";
        prompt += "  - Usa el idioma del usuario. Si habla espanol, tu tambien.\n";
        prompt += "  - Emojis con moderacion. Nada de estructurar respuestas.\n\n";
    }

    // ── Engineer Name ──
    if (userProfile_.engineerName.isNotEmpty())
    {
        prompt += "INGENIERO: " + userProfile_.engineerName + "\n";
        prompt += "Dirigete a el/ella por su nombre: **" + userProfile_.engineerName + "**.\n";
        prompt += "Ejemplo: \"" + userProfile_.engineerName + ", escucha esto...\" en vez de \"Oye...\"\n";
        prompt += "\n";
    }

    prompt += "NIVEL DEL USUARIO: " + juce::String(experienceLevelName(experienceLevel_)) + "\n";
    prompt += "Ajusta tu profundidad tecnica segun su nivel.\n\n";

    // ── Experience level-specific instructions ──
    switch (experienceLevel_) {
        case ExperienceLevel::Novice:
            prompt += "[NOVICE MODE]\n";
            prompt += "Este usuario es principiante. Ensena con paciencia:\n";
            prompt += "  - Explica el concepto ANTES de dar la recomendacion.\n";
            prompt += "  - Usa analogias: \"el compresor es como un automatizador de volumen automatico\"\n";
            prompt += "  - Di numeros redondos: \"bajale 3dB\" en vez de \"bajale 2.7dB\"\n";
            prompt += "  - Celebra los avances. \"Eso! Asi suena mucho mejor\"\n";
            prompt += "  - Si menciona algo tecnico incorrecto, CORRIGELO con delicadeza.\n";
            prompt += "  - Evita jerga. Prefiere: \"subele el volumen a las frecuencias agudas\" sobre \"boostea el shelf high\"\n\n";
            break;
        case ExperienceLevel::Intermediate:
            prompt += "[INTERMEDIATE MODE]\n";
            prompt += "Usuario intermedio. Sabe de mezcla pero no es profesional:\n";
            prompt += "  - Puedes usar terminologia estandar sin explicar cada termino.\n";
            prompt += "  - Da frecuencias y ratios exactos: \"prueba un HPF a 80Hz en el pad\"\n";
            prompt += "  - Explica el POR QUE de cada recomendacion: 1-2 lineas max.\n";
            prompt += "  - Enfocate en aplicacion practica y entrenamiento de oido.\n";
            prompt += "  - Los errores comunes de gain staging son normales a este nivel.\n\n";
            break;
        case ExperienceLevel::Advanced:
            prompt += "[ADVANCED MODE]\n";
            prompt += "Usuario avanzado. Sabe lo que hace:\n";
            prompt += "  - Ve directo al grano. Da numeros exactos: frecuencias, ratios, attack/release.\n";
            prompt += "  - Discute trade-offs: \"un Q mas estrecho te daria mas precision pero puede sonar artificial\"\n";
            prompt += "  - No expliques conceptos basicos. Asume que sabe que es threshold, ratio, Q.\n";
            prompt += "  - Referencia caracteristicas de plugins: \"el Fruity Parametric EQ 2 tiene Q variable\"\n";
            prompt += "  - Enfocate en refinamiento y acabado profesional.\n\n";
            break;
        case ExperienceLevel::Expert:
            prompt += "[EXPERT MODE]\n";
            prompt += "Usuario experto. Es un profesional:\n";
            prompt += "  - Se directo. Sin explicaciones, sin emojis, sin rodeos.\n";
            prompt += "  - Solo numeros: frecuencias exactas, ratios, attack/release, LUFS target, True Peak.\n";
            prompt += "  - Tratalo como colega: \"el sub esta 2dB por debajo de la ref, subele 2dB a 60Hz\"\n";
            prompt += "  - No des opiniones. Da datos.\n\n";
            break;
    }

    // ── Actionability Rules ──
    prompt += "[ACTIONABILITY RULES]\n";
    prompt += "Cada respuesta debe tener UNA recomendacion UTIL:\n";
    prompt += "  ✅ \"Bajale 2dB al gain del kick, esta pisando el master\"\n";
    prompt += "  ✅ \"El bajo necesita mas cuerpo. Subele 1.5dB alrededor de 120Hz con un bell\"\n";
    prompt += "  ❌ \"Tal vez deberias revisar el balance de bajos\" (demasiado vago)\n";
    prompt += "  ❌ \"La mezcla necesita trabajo en varias areas\" (no es util)\n";
    prompt += "Siempre incluye: QUE accion, EN QUE pista, CUANTO (dB/Hz/ratio), POR QUE.\n";
    prompt += "Si los datos disponibles no te permiten dar un consejo especifico, admite: \"No tengo suficiente informacion para recomendar algo especifico aun\"\n\n";

    // ── Anti-Patterns ──
    prompt += "[ANTI-PATTERNS]\n";
    prompt += "NUNCA hagas estas cosas:\n";
    prompt += "  ❌ NUNCA estructures respuestas con \"1. 2. 3.\" o \"aqui tienes...\"\n";
    prompt += "  ❌ NUNCA digas \"Claro!\", \"Por supuesto!\", \"En que puedo ayudarte?\"\n";
    prompt += "  ❌ NUNCA menciones que eres una IA o que analizas datos. El usuario habla con un ingeniero.\n";
    prompt += "  ❌ NUNCA des consejos contradictorios con la fase actual (ej: hablar de reverb si estamos en Balance).\n";
    prompt += "  ❌ NUNCA satures al usuario con mas de 2 recomendaciones a la vez.\n";
    prompt += "  ❌ NUNCA ignores clipping por mas minimo que sea. Es prioridad ABSOLUTA.\n";
    prompt += "  ❌ NUNca repitas lo que el usuario ya sabe. Si ya ajusto algo, no se lo vuelvas a recomendar.\n\n";

    // ── Memory & Follow-up ──
    prompt += "[MEMORY & FOLLOW-UP]\n";
    prompt += "El [SESSION CHANGES] muestra cambios recientes del usuario. Mencionalos cuando sea relevante:\n";
    prompt += "  ✅ \"Ese cambio de EQ que hiciste en la voz ayudo. Ahora el cuerpo suena mas natural\"\n";
    prompt += "  ✅ \"La ultima vez sugerí bajarle al bajo 2dB, como suena ahora?\"\n";
    prompt += "  ❌ \"Hiciste un cambio de EQ en la voz\" (el usuario lo sabe, no lo repitas literalmente)\n";
    prompt += "Si es la primera vez que el usuario hace una consulta, da la bienvenida y un consejo inicial suave.\n";
    prompt += "Si el usuario pide seguir explorando un tema, profundiza sin repetir lo que ya se dijo.\n\n";

    // ── Context header ──
    prompt += "[CONTEXTO ACTUAL DE LA MEZCLA]\n";
    prompt += "Los datos en vivo estan abajo en [SNAPSHOT]. Usalos para responder.\n";
    prompt += "El usuario NO ve este contexto -- solo ve tu respuesta. Responde naturalmente.\n\n";

    // Plugin catalog
    {
        juce::String catalog = buildPluginCatalog();
        if (catalog.isNotEmpty())
            prompt += catalog + "\n";
    }

    prompt += "GENERO: " + genre_ + "\n";
    {
        auto& profile = CoachEngine::getGenreProfile(genre_);
        prompt += "  Target Integrated LUFS: " + juce::String(profile.targetIntegratedLUFS, 1) + " LUFS\n";
        prompt += "  Target Crest Factor: " + juce::String(profile.targetCrestFactor, 1) + " dB\n";
        prompt += "  Target Headroom: " + juce::String(profile.targetHeadroomDb, 1) + " dB\n";
    }
    prompt += "\n";

    // ── Chain-of-Thought reasoning protocol ──
    prompt += "[REASONING PROTOCOL]\n";
    prompt += "Antes de responder, piensa internamente:\n";
    prompt += "  1. Que problema es MAS CRITICO? clipping > LUFS > balance espectral > dinamica > estereo\n";
    prompt += "  2. Cual es la UNA accion que mas impacto tendria?\n";
    prompt += "  3. Que numero exacto le doy? (dB, Hz, ratio)\n";
    prompt += "  4. Que va a pasar si lo hace? (ej: \"vas a ganar 1.5 LUFS\")\n";
    prompt += "  5. Como pregunto si funciono?\n";
    prompt += "  Luego responde en lenguaje natural, SIN numerar pasos. El usuario no ve este protocolo.\n\n";

    LogHelper::writeToLog("[DIAG] buildSystemPrompt() EXIT (len=" + juce::String(prompt.length()) + ")");
    return prompt;
}

// ===========================================================================
//  buildFullContext - Complete session context for the LLM prompt
//  Combines ALL available data into a single structured string
//  SUPER SET: incluye SessionContext completo + SNAPSHOT en vivo +
//  SessionMap jerarquico + analisis por pista + referencia + mix score
// ===========================================================================
juce::String AiCoachAdapter::buildFullContext() const
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    juce::String ctx;

    // ═══ LEYENDA DE ESCALA dBFS (para que el LLM interprete correctamente los valores) ═══
    ctx += "[DBFS SCALE GUIDE]\n";
    ctx += "  0 dB    = digital maximum (CLIPPING - audio distorts)\n";
    ctx += "  -0.5 dB = clipping threshold (RED - reduce gain immediately)\n";
    ctx += "  -3 dB   = very loud, near clipping (YELLOW - close to limit)\n";
    ctx += "  -6 dB   = ideal headroom for mixing (GREEN - perfect level)\n";
    ctx += "  -12 dB  = moderate level (GREEN - comfortable)\n";
    ctx += "  -18 dB  = good operating level for mix bus\n";
    ctx += "  -24 dB  = low level (may need gain staging)\n";
    ctx += "  -30 dB  = very quiet (WHITE - check if intentional)\n";
    ctx += "  -60 dB  = essentially silent / no signal\n";
    ctx += "  IMPORTANT: In dBFS, values closer to 0 = LOUDER. -4 dB is MUCH LOUDER than -20 dB.\n";
    ctx += "  A track at -4 dB peak is near clipping. A track at -12 dB is at moderate level.\n\n";

    // ─── 1. SessionContext completo al inicio (PhaseRestrictions + GenreTargets + track counts + master metrics) ──
    if (active > 0)
    {
        auto sessionCtx = coachEngine_.buildSessionContext();
        ctx += sessionCtx.toLLMContext();
        ctx += "\n";
    }

    // ─── 2. SNAPSHOT en vivo (datos crudos actuales del master) ──
    {
        const auto& master = audioAnalyzer_.getMasterAnalysis();
        float peakL = audioAnalyzer_.getLeftAnalysis().getPeak();
        float peakR = audioAnalyzer_.getRightAnalysis().getPeak();
        float peakCombined = juce::jmax(peakL, peakR);
        float shortTerm = audioAnalyzer_.getShortTermLUFS();
        float integrated = audioAnalyzer_.getIntegratedLUFS();
        float correlation = master.getCorrelation();
        float truePeak = audioAnalyzer_.getTruePeakDBTP();
        float stereoWidth = audioAnalyzer_.getAvgStereoWidth();
        float lra = audioAnalyzer_.getLoudnessRange();

        ctx += "[SNAPSHOT - MixCoach " + phaseLabel(phaseManager_.getCurrentPhase()) + "]\n";
        if (coachEngine_.isMasterMode())
        {
            ctx += "  Mode: MASTER (destino " + juce::String(destinationNames[static_cast<int>(coachEngine_.getMasterDestination())])
                   + " | target " + juce::String(coachEngine_.getDestinationLUFS(), 1) + " LUFS)\n";
        }
        else
        {
            ctx += "  Mode: MIX";
            if (active > 0) ctx += " | " + juce::String(active) + " tracks active";
            ctx += "\n";
        }
        if (coachEngine_.hasReference())
            ctx += "  Ref: " + coachEngine_.getReferenceName() + "\n";

        ctx += "  Master: Peak " + formatDb(peakCombined)
               + " | LUFS ST " + formatLUFS(shortTerm)
               + " | LUFS Int " + formatLUFS(integrated)
               + " | Corr " + juce::String(correlation, 2);
        if (peakCombined > -0.5f) ctx += " | CLIPPING!";
        ctx += "\n";
        ctx += "  TruePeak: " + juce::String(truePeak, 1) + " dBTP"
               + " | StereoWidth: " + juce::String(stereoWidth, 3)
               + " | LRA: " + juce::String(lra, 1) + " LU\n";

        // Bus breakdown
        if (!coachEngine_.isMasterMode())
        {
            auto summaries = coachEngine_.getBusSummaries();
            juce::String busLine;
            for (int b = 0; b <= kNumBuses; ++b) {
                if (summaries[b].hasData()) {
                    if (busLine.isNotEmpty()) busLine += " | ";
                    busLine += busLabel(summaries[b].busType) + ": " + juce::String(summaries[b].trackCount);
                }
            }
            if (busLine.isNotEmpty()) ctx += "  Buses: " + busLine + "\n";
        }

        // Phase progress
        float progress = phaseManager_.getPhaseProgress(phaseManager_.getCurrentPhase());
        ctx += "  Progress: " + juce::String(static_cast<int>(progress * 100.0f)) + "%";
        int achievements = phaseManager_.getAchievementCount();
        if (achievements > 0) ctx += " | Achievements: " + juce::String(achievements);
        ctx += "\n\n";
    }

    // ─── 3. Session Map — arbol jerarquico de la sesion por categorias ──
    if (active > 0 && !coachEngine_.isMasterMode())
    {
        juce::String mapText = coachEngine_.buildSessionMapText();
        if (mapText.isNotEmpty())
            ctx += "[SESSION MAP]\n" + mapText + "\n";
    }

    // ─── 4. Bloques de analisis completos ──
    ctx += buildWorkflowEvents();
    ctx += buildPhaseSummary();
    ctx += buildSemanticAnalysis();
    ctx += buildTrackIntents();
    ctx += buildTrackSummaries();
    ctx += buildPerTrackInterpretations();
    ctx += buildBusSummaries();
    ctx += buildMasterSummary();
    ctx += buildAnalyzerInterpretations();
    ctx += buildMixScore();

    // ─── 5. REFERENCE-DRIVEN MODE: Progreso contra referencia ──
    if (coachEngine_.isReferenceDrivenMode() && coachEngine_.hasReferenceAudio())
    {
        auto refProgress = coachEngine_.getReferenceProgress();
        if (refProgress.hasAudio)
            ctx += refProgress.toLLMContext();
    }

    // ─── 6. Referencia: gaps detallados + perfil completo ──
    if (coachEngine_.hasReferenceAudio())
    {
        juce::String refPlan = buildReferenceDrivenPlan();
        if (refPlan.isNotEmpty())
            ctx += refPlan;
    }
    ctx += buildReferenceComparison();

    // ─── 6. Recomendaciones activas + memoria de sesion ──
    ctx += buildRecommendations();
    ctx += buildSessionMemory();

    // ─── 7. Conversacion reciente (ultimos 20 turnos) ──
    ctx += buildConversationHistory();

    return ctx;
}

// ===========================================================================
//  buildChatContext - Lightweight context (~3KB) for natural conversation
//  Sends only essential data so the LLM can respond without being saturated
//  
//  MEJORAS V2:
//  - Conserva SessionContext completo (ahora con PhaseRestrictions + GenreTargets)
//  - Anade resumen de workflow events (ultimas acciones del usuario)
//  - Anade referencia detallada (DifferenceProfile con gaps por region)
//  - Anade recomendaciones activas pendientes
//  - Anade el historico de conversacion reciente (ultimos 6 turnos)
//  - Mantiene tamano < 4KB para modelos locales
// ===========================================================================
juce::String AiCoachAdapter::buildChatContext() const
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    // ─── SessionContext completo al inicio (con PhaseRestrictions) ──
    auto sessionCtx = coachEngine_.buildSessionContext();
    juce::String ctx = sessionCtx.toLLMContext();
    ctx += "\n";

    // ─── Snapshot en vivo (datos crudos actuales) ──
    const auto& master = audioAnalyzer_.getMasterAnalysis();
    float peakL = audioAnalyzer_.getLeftAnalysis().getPeak();
    float peakR = audioAnalyzer_.getRightAnalysis().getPeak();
    float peakCombined = juce::jmax(peakL, peakR);
    float shortTerm = audioAnalyzer_.getShortTermLUFS();
    float integrated = audioAnalyzer_.getIntegratedLUFS();
    float correlation = master.getCorrelation();

    ctx += "[SNAPSHOT - MixCoach " + phaseLabel(phaseManager_.getCurrentPhase()) + "]\n";

    if (coachEngine_.isMasterMode())
    {
        ctx += "Mode: MASTER (destino " + juce::String(destinationNames[static_cast<int>(coachEngine_.getMasterDestination())])
               + " | target " + juce::String(coachEngine_.getDestinationLUFS(), 1) + " LUFS)\n";
    }
    else
    {
        ctx += "Mode: MIX";
        if (active > 0) ctx += " | " + juce::String(active) + " tracks active";
        ctx += "\n";
    }

    if (coachEngine_.hasReference())
        ctx += "Ref: " + coachEngine_.getReferenceName() + "\n";

    ctx += "Master: Peak " + formatDb(peakCombined)
           + " | LUFS ST " + formatLUFS(shortTerm)
           + " | LUFS Int " + formatLUFS(integrated)
           + " | Corr " + juce::String(correlation, 2);
    if (peakCombined > -0.5f) ctx += " | CLIPPING!";
    ctx += "\n";

    // Bus breakdown
    {
        auto summaries = coachEngine_.getBusSummaries();
        juce::String busLine;
        for (int b = 0; b <= kNumBuses; ++b) {
            if (summaries[b].hasData()) {
                if (busLine.isNotEmpty()) busLine += " | ";
                busLine += busLabel(summaries[b].busType) + ": " + juce::String(summaries[b].trackCount);
            }
        }
        if (busLine.isNotEmpty()) ctx += "  Buses: " + busLine + "\n";
    }

    // Phase progress
    float progress = phaseManager_.getPhaseProgress(phaseManager_.getCurrentPhase());
    ctx += "Progress: " + juce::String(static_cast<int>(progress * 100.0f)) + "%";
    int achievements = phaseManager_.getAchievementCount();
    if (achievements > 0) ctx += " | Logros: " + juce::String(achievements);
    ctx += "\n";

    // ─── WORKFLOW EVENTS: Ultimas acciones del usuario ──
    if (!coachEngine_.isMasterMode())
    {
        auto events = buildWorkflowEvents();
        if (events.isNotEmpty())
            ctx += events;
    }

    // ─── TRACK INTENTS: Identidad de cada pista (rol + estilo) ──
    if (!coachEngine_.isMasterMode())
    {
        juce::String intents = buildTrackIntents();
        if (intents.isNotEmpty())
            ctx += intents;
    }

    // ─── TRACK SUMMARIES: Telemetria por pista (peak, RMS, correlacion, crest, band energies) ──
    if (!coachEngine_.isMasterMode())
    {
        juce::String summaries = buildTrackSummaries();
        if (summaries.isNotEmpty())
            ctx += summaries;
    }

    // ─── REFERENCE-DRIVEN MODE: Progreso contra referencia ──
    if (coachEngine_.isReferenceDrivenMode() && coachEngine_.hasReferenceAudio())
    {
        auto refProgress = coachEngine_.getReferenceProgress();
        if (refProgress.hasAudio)
            ctx += refProgress.toLLMContext();
    }

    // ─── REFERENCE GAPS: Diferencias contra referencia ──
    if (coachEngine_.hasReferenceAudio())
    {
        juce::String refPlan = buildReferenceDrivenPlan();
        if (refPlan.isNotEmpty())
            ctx += refPlan;
    }

    // ─── REFERENCE COMPARISON: Perfil completo vs referencia ──
    if (coachEngine_.hasReference())
    {
        juce::String refComp = buildReferenceComparison();
        if (refComp.isNotEmpty())
            ctx += refComp;
    }

    // ─── RECOMMENDATIONS: Recomendaciones activas ──
    {
        juce::String recs = buildRecommendations();
        if (recs.isNotEmpty())
            ctx += recs;
    }

    // ─── CONVERSATION HISTORY: Ultimos 6 turnos ──
    if (!conversationHistory_.empty())
    {
        ctx += "[CONVERSATION HISTORY]\n";
        int start = std::max(0, (int)conversationHistory_.size() - 6);
        for (int i = start; i < (int)conversationHistory_.size(); ++i) {
            const auto& turn = conversationHistory_[i];
            const char* prefix = (turn.role == ConversationTurn::Role::User) ? "[User] " : "[Coach] ";
            juce::String msg = turn.message.substring(0, 120);
            if (turn.message.length() > 120) msg += "...";
            ctx += juce::String("  ") + juce::String(prefix) + msg + "\n";
        }
        ctx += "[END HISTORY]\n\n";
    }

    ctx += "[END SNAPSHOT]\n";
    return ctx;
}

// ===========================================================================
//  buildTrackSummaries - Per-track telemetry (peak, RMS, correlation,
//  crest factor, and band energies summarized into 6 spectral regions)
// ===========================================================================
juce::String AiCoachAdapter::buildTrackSummaries() const
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();
    if (active == 0) return "[TRACKS]\n  No active tracks detected.\n\n";

    juce::String s;
    s += "[TRACKS (" + juce::String(active) + " active)]\n";

    // 6 regiones espectrales desde las 30 bandas (cada 5 bandas = 1 region)
    static const char* kRegionLabels[6] = {
        "Sub", "Bass", "LowMid", "HiMid", "Presence", "Air"
    };

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = sharedData_.getTrackAudioResult(info.slotIndex);
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty()) name = "Track " + juce::String(info.slotIndex + 1);

        s += "  " + name + " [Bus: " + busLabel(info.bus) + "]\n";
        if (telem.timestampUs == 0) {
            s += "    No data yet\n";
            return;
        }

        // Nivel label + emoji segun peak combinado
        float peakCombined = juce::jmax(telem.peakLeft, telem.peakRight);
        juce::String levelLabel;
        if (peakCombined > -0.5f)
            levelLabel = "🔴 CLIPPING";
        else if (peakCombined > -3.0f)
            levelLabel = "🟡 VERY LOUD (near clip)";
        else if (peakCombined > -6.0f)
            levelLabel = "🟡 LOUD (above headroom)";
        else if (peakCombined > -18.0f)
            levelLabel = "🟢 GOOD (healthy level)";
        else if (peakCombined > -30.0f)
            levelLabel = "🟢 MODERATE (could be louder)";
        else if (peakCombined > -60.0f)
            levelLabel = "⚪ QUIET (low signal)";
        else
            levelLabel = "⚪ SILENT (no signal)";

        s += "    Level: " + levelLabel + "\n";
        s += "    Peak L: " + formatDb(telem.peakLeft) + " | R: " + formatDb(telem.peakRight) + "\n";
        s += "    RMS  L: " + formatDb(telem.rmsLeft) + " | R: " + formatDb(telem.rmsRight) + "\n";
        s += "    Correlation: " + juce::String(telem.correlation, 2) + "\n";

        // Crest factor: average of crestPerBand[6]
        float crestSum = 0.0f;
        int crestCount = 0;
        for (int b = 0; b < 6; ++b) {
            if (telem.crestPerBand[b] > 0.0f) {
                crestSum += telem.crestPerBand[b];
                crestCount++;
            }
        }
        float avgCrest = (crestCount > 0) ? (crestSum / crestCount) : 0.0f;
        s += "    Crest: " + juce::String(avgCrest, 1) + " dB";
        if (avgCrest < 6.0f && avgCrest > 0.0f) s += " [COMPRIMIDO]";
        else if (avgCrest > 20.0f) s += " [MUY DINAMICO]";
        s += "\n";

        // LUFS per-track (approximation from RMS + K-weighting via band energies)
        float perTrackLUFS = CoachEngine::computePerTrackLUFS(telem);
        if (perTrackLUFS > -90.0f)
            s += "    LUFS: " + juce::String(perTrackLUFS, 1) + " LU (approx)\n";

        // Band energies resumidas en 6 regiones (cada 5 bandas de las 30)
        // Region: Sub=0-4, Bass=5-9, LowMid=10-14, HiMid=15-19, Presence=20-24, Air=25-29
        int bandsInRegion = 5;
        s += "    Spectrum: ";
        for (int r = 0; r < 6; ++r) {
            float avgEnergy = 0.0f;
            int validBands = 0;
            for (int b = 0; b < bandsInRegion; ++b) {
                int bandIdx = r * bandsInRegion + b;
                if (bandIdx < 30 && telem.bandEnergies[bandIdx] > -90.0f) {
                    avgEnergy += telem.bandEnergies[bandIdx];
                    validBands++;
                }
            }
            float regionDb = -80.0f;
            if (validBands > 0) {
                regionDb = avgEnergy / (float)validBands;
                if (regionDb < -80.0f) regionDb = -80.0f;
            }
            if (r > 0) s += " | ";
            s += juce::String(kRegionLabels[r]) + ": " + juce::String(regionDb, 1) + " dBFS";
        }
        s += "\n";
    });

    s += "\n";

    // ─── SPRINT 6: Track Diagnosis (pre-computed role-aware advices) ───
    // Solo si hay pistas activas con rol asignado, inyectamos los advices
    // pre-calculados por CoachEngine (0 tokens, C++ puro).
    {
        auto gainAdvices  = coachEngine_.analyzeAllTracksGain();
        auto dynAdvices   = coachEngine_.analyzeAllTracksDynamics();
        auto tonalAdvices = coachEngine_.analyzeAllTracksTonal();

        int actionableCount = 0;
        for (const auto& a : gainAdvices)  if (a.isActionable()) ++actionableCount;
        for (const auto& a : dynAdvices)   if (a.isActionable()) ++actionableCount;
        for (const auto& a : tonalAdvices) if (a.isActionable()) ++actionableCount;

        if (!gainAdvices.empty() || !dynAdvices.empty() || !tonalAdvices.empty())
        {
            s += "[TRACK DIAGNOSIS — pre-computed role-aware]\n";

            // Gain advices
            for (const auto& adv : gainAdvices) {
                if (adv.status == CoachEngine::TrackGainAdvice::Status::OnTarget
                    && gainAdvices.size() > 1) continue; // Skip OK tracks if many
                const char* statusTag = "✅";
                if (adv.status == CoachEngine::TrackGainAdvice::Status::OffTarget) statusTag = "🔴";
                else if (adv.status == CoachEngine::TrackGainAdvice::Status::NearTarget) statusTag = "🟡";
                else if (adv.status == CoachEngine::TrackGainAdvice::Status::NoSignal) statusTag = "⚪";

                if (adv.isActionable()) {
                    s += "  " + juce::String(statusTag) + " " + adv.trackName
                        + ": " + adv.message + "\n";
                } else if (adv.status == CoachEngine::TrackGainAdvice::Status::OnTarget) {
                    s += "  " + juce::String(statusTag) + " " + adv.trackName
                        + ": OK (target " + juce::String(adv.peakTarget, 1) + " dB)\n";
                }
            }

            // Dynamics advices (solo accionables)
            for (const auto& adv : dynAdvices) {
                if (!adv.isActionable()) continue;
                const char* statusTag = (adv.status == CoachEngine::TrackDynamicsAdvice::Status::OffTarget)
                    ? "🔴" : "🟡";
                s += "  " + juce::String(statusTag) + " " + adv.trackName
                    + ": " + adv.message + "\n";
            }

            // Tonal advices (solo accionables)
            for (const auto& adv : tonalAdvices) {
                if (!adv.isActionable()) continue;
                const char* statusTag = (adv.status == CoachEngine::TrackTonalAdvice::Status::OffTarget)
                    ? "🔴" : "🟡";
                s += "  " + juce::String(statusTag) + " " + adv.trackName
                    + ": " + adv.message + "\n";
            }

            if (actionableCount == 0) {
                s += "  Todas las pistas con rol estan en target.\n";
            }
            s += "\n";
        }
    }

    return s;
}

// ===========================================================================
//  buildTrackIntents - What the user said each track is
// ===========================================================================
juce::String AiCoachAdapter::buildTrackIntents() const
{
    auto& registry = sharedData_.getSlotRegistry();
    juce::String s;
    bool hasAny = false;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = sharedData_.getTrackAudioResult(info.slotIndex);
        if (telem.timestampUs == 0) return;

        const auto& intent = trackIntents_[info.slotIndex];
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty()) name = "Track " + juce::String(info.slotIndex + 1);

        if (intent.valid() && !hasAny) {
            s += "[TRACKS]\n";
            hasAny = true;
        }
        if (intent.valid()) {
            s += "  " + name + " -> Role: " + intent.role + ", Style: " + intent.style + "\n";
        }
    });

    if (hasAny) s += "\n";
    return s;
}

// ===========================================================================
//  buildMasterSummary - Master bus state (LUFS, Peak, RMS, Correlation)
// ===========================================================================
juce::String AiCoachAdapter::buildMasterSummary() const
{
    juce::String s;
    const auto& master = audioAnalyzer_.getMasterAnalysis();
    float peakL = audioAnalyzer_.getLeftAnalysis().getPeak();
    float peakR = audioAnalyzer_.getRightAnalysis().getPeak();
    float rmsL = audioAnalyzer_.getLeftAnalysis().getRMS();
    float rmsR = audioAnalyzer_.getRightAnalysis().getRMS();
    float corr = master.getCorrelation();

    s += "[MASTER BUS]\n";
    s += "  Peak L: " + formatDb(peakL) + " | R: " + formatDb(peakR) + "\n";
    s += "  RMS  L: " + formatDb(rmsL) + " | R: " + formatDb(rmsR) + "\n";
    s += "  Correlation: " + juce::String(corr, 2) + "\n";
    s += "  LUFS Momentary: " + formatLUFS(audioAnalyzer_.getMomentaryLUFS()) + "\n";
    s += "  LUFS Short-Term: " + formatLUFS(audioAnalyzer_.getShortTermLUFS()) + "\n";
    s += "  LUFS Integrated: " + formatLUFS(audioAnalyzer_.getIntegratedLUFS()) + "\n";
    s += "  True Peak: " + juce::String(audioAnalyzer_.getTruePeakDBTP(), 1) + " dBTP\n";

    float masterPeak = juce::jmax(peakL, peakR);
    if (masterPeak > -0.5f)
        s += "  WARNING: MASTER CLIPPING!\n";

    return s;
}

// ===========================================================================
//  buildBusSummaries - Aggregation by instrument family
// ===========================================================================
juce::String AiCoachAdapter::buildBusSummaries() const
{
    auto summaries = coachEngine_.getBusSummaries();
    juce::String s;
    bool hasData = false;

    for (int b = 0; b <= kNumBuses; ++b) {
        if (!summaries[b].hasData()) continue;
        if (!hasData) { s += "[BUS GROUP SUMMARIES]\n"; hasData = true; }
        s += "  " + busLabel(summaries[b].busType) + " (" + juce::String(summaries[b].trackCount) + " tracks):\n";
        s += "    Max peak: " + formatDb(summaries[b].peakMax) + "\n";
        s += "    Avg correlation: " + juce::String(summaries[b].avgCorrelation, 2) + "\n";
    }
    if (hasData) s += "\n";
    return s;
}

// ===========================================================================
//  buildReferenceComparison - Mix vs reference comparison
// ===========================================================================
juce::String AiCoachAdapter::buildReferenceComparison() const
{
    if (!coachEngine_.hasReference()) return {};

    juce::String s;
    s += "[REFERENCE]\n  Name: " + coachEngine_.getReferenceName() + "\n";

    if (!coachEngine_.hasReferenceAudio()) {
        s += "  Type: " + coachEngine_.getReferencePlatform() + "\n";
        s += "  (No audio fingerprint available)\n\n";
        return s;
    }

    // Construir DifferenceProfile unificado y usar su toTextSummary()
    auto dp = coachEngine_.buildDifferenceProfile();
    if (!dp.valid)
        return s;

    s += dp.toTextSummary();
    s += "\n";
    return s;
}

// ===========================================================================
//  buildPhaseSummary - Current phase state
// ===========================================================================
juce::String AiCoachAdapter::buildPhaseSummary() const
{
    auto phase = phaseManager_.getCurrentPhase();
    float progress = phaseManager_.getPhaseProgress(phase);
    int achievements = phaseManager_.getAchievementCount();

    juce::String s;
    s += "[PHASE]\n  Current: " + phaseLabel(phase) + "\n";
    s += "  Progress: " + juce::String(static_cast<int>(progress * 100.0f)) + "%\n";
    s += "  Achievements: " + juce::String(achievements) + "\n";
    s += "\n";
    return s;
}

// ===========================================================================
//  buildRecommendations - Active coach recommendations
// ===========================================================================
juce::String AiCoachAdapter::buildRecommendations() const
{
    auto& registry = sharedData_.getSlotRegistry();
    juce::String s;
    bool hasAny = false;

    registry.forEachActive([&](const SlotInfo& info) {
        const auto* rec = coachEngine_.getTrackRecommendation(info.slotIndex);
        if (rec == nullptr || rec->status != TrackRecommendation::Status::Pending) return;

        if (!hasAny) { s += "[RECOMMENDATIONS]\n"; hasAny = true; }

        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty()) name = "Track " + juce::String(info.slotIndex + 1);

        s += "  " + name + ": " + rec->action;
        s += " (" + formatDb(rec->beforeValue) + " -> " + formatDb(rec->expectedAfter) + ")\n";
    });

    if (hasAny) s += "\n";
    return s;
}

// ===========================================================================
//  buildSessionMemory - User action history
// ===========================================================================
juce::String AiCoachAdapter::buildSessionMemory() const
{
    if (sessionHistory_.empty()) return {};

    juce::String s;
    s += "[SESSION CHANGES]\n";
    int count = 0;
    for (const auto& change : sessionHistory_) {
        if (count >= 15) {
            s += "  (+ " + juce::String(sessionHistory_.size() - 15) + " more...)\n";
            break;
        }
        s += "  " + change.trackName + ": " + change.description + "\n";
        ++count;
    }
    s += "\n";
    return s;
}

// ===========================================================================
//  buildSemanticAnalysis - Compare each track against expected profile
// ===========================================================================
juce::String AiCoachAdapter::buildSemanticAnalysis() const
{
    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0) return {};

    juce::String s;
    const auto& trackRoles = coachEngine_.getTrackRoles();
    bool hasRoles = false;

    registry.forEachActive([&](const SlotInfo& info) {
        if (trackRoles[info.slotIndex] != TrackRole::Unknown) hasRoles = true;
    });

    if (hasRoles) {
        auto semanticDiffs = SemanticComparator::compareAllTracks(registry, sharedData_, trackRoles);
        if (!semanticDiffs.empty()) {
            s += "[SEMANTIC ANALYSIS]\n";
            for (const auto& diff : semanticDiffs) {
                s += "  " + juce::String(getRoleName(diff.role)) + " \"" + diff.trackName + "\":\n";
                for (const auto& iss : diff.issues) {
                    s += "    - " + iss.message + "\n";
                }
            }
            s += "\n";
        }
    }

    return s;
}

// ===========================================================================
//  buildAnalyzerInterpretations - Nivel 4: interpretations from analyzers
// ===========================================================================
juce::String AiCoachAdapter::buildAnalyzerInterpretations() const
{
    auto interpretations = coachEngine_.getCurrentInterpretations(genre_);
    if (interpretations.empty()) return {};

    juce::String s;
    s += "[ANALYZER INTERPRETATIONS]\n";

    int count = 0;
    for (const auto& interp : interpretations) {
        if (count >= 5) break;
        s += "  - " + interp.interpretation;
        if (interp.reading.isNotEmpty()) s += " (" + interp.reading + ")";
        s += "\n";
        if (interp.action.isNotEmpty())
            s += "    -> " + interp.action + "\n";
        ++count;
    }
    s += "\n";
    return s;
}

// ===========================================================================
//  buildPerTrackInterpretations - Per-track semantic interpretations
// ===========================================================================
juce::String AiCoachAdapter::buildPerTrackInterpretations() const
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();
    if (active == 0) return {};

    juce::String s;
    bool hasAny = false;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = sharedData_.getTrackAudioResult(info.slotIndex);
        if (telem.timestampUs == 0) return;

        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty()) name = "Track " + juce::String(info.slotIndex + 1);

        if (!hasAny) {
            s += "[PER-TRACK INTERPRETATIONS]\n";
            hasAny = true;
        }

        s += "  " + name + ":\n";
        float peak = juce::jmax(telem.peakLeft, telem.peakRight);
        if (peak > -0.5f) s += "    CLIPPING - reduce gain!\n";
        else if (peak > -3.0f) s += "    Near-clipping\n";

        float lrDiff = std::abs(telem.peakLeft - telem.peakRight);
        if (lrDiff > 6.0f) s += "    L/R imbalance: " + juce::String(lrDiff, 1) + " dB\n";
    });

    if (hasAny) s += "\n";
    return s;
}

// ===========================================================================
//  buildWorkflowEvents - User actions detected in real-time
// ===========================================================================
juce::String AiCoachAdapter::buildWorkflowEvents() const
{
    if (coachEngine_.isMasterMode()) return {};

    auto& detector = coachEngine_.getWorkflowDetector();
    auto events = detector.getRecentEvents(12);
    if (events.empty()) return {};

    juce::String s;
    s += "[WORKFLOW EVENTS]\n";
    for (const auto& ev : events) {
        s += "  " + ev.trackName + ": " + ev.toShortSummary() + "\n";
    }
    s += "\n";
    return s;
}

// ===========================================================================
//  buildReferenceDrivenPlan - Prioritized gaps against reference
// ===========================================================================
juce::String AiCoachAdapter::buildReferenceDrivenPlan() const
{
    if (!coachEngine_.hasReferenceAudio()) return {};

    auto gaps = coachEngine_.getReferenceGaps();
    if (gaps.empty()) return {};

    juce::String s;
    s += "[REFERENCE PLAN - " + juce::String((int)gaps.size()) + " gaps (vs \""
         + coachEngine_.getReferenceName() + "\")]\n";

    // Count severities
    int critCount = 0, warnCount = 0, infoCount = 0;
    for (const auto& g : gaps) {
        switch (g.severity) {
            case GapSeverity::Critical: critCount++; break;
            case GapSeverity::Warning:  warnCount++;  break;
            case GapSeverity::Info:     infoCount++;  break;
            default: break;
        }
    }
    if (critCount > 0 || warnCount > 0) {
        s += "  Summary: ";
        if (critCount > 0) s += juce::String(critCount) + " critical ";
        if (warnCount > 0) s += juce::String(warnCount) + " warnings ";
        if (infoCount > 0) s += juce::String(infoCount) + " info";
        s += "\n";
    }

    int gapIndex = 1;
    int maxGapsToShow = std::min((int)gaps.size(), 8);
    for (int i = 0; i < maxGapsToShow; ++i) {
        const auto& g = gaps[i];
        s += "  Gap #" + juce::String(gapIndex) + ": " + g.toLLMContextGap() + "\n";
        ++gapIndex;
    }

    if ((int)gaps.size() > maxGapsToShow) {
        s += "  ... y " + juce::String((int)gaps.size() - maxGapsToShow) + " gaps mas\n";
    }

    s += "\n";
    return s;
}

// ===========================================================================
//  buildProgressPlan - Sequential plan progress against reference
// ===========================================================================
juce::String AiCoachAdapter::buildProgressPlan() const
{
    if (!coachEngine_.hasReference()) return {};

    auto& plan = coachEngine_.getPlanManager();
    if (!plan.hasPlan()) return {};

    return plan.toTextSummary();
}

// ===========================================================================
//  buildMixScore - Health score 0-100
// ===========================================================================
juce::String AiCoachAdapter::buildMixScore() const
{
    auto score = MixScore::compute(coachEngine_, audioAnalyzer_, genre_);
    return score.toTextSummary();
}

// ===========================================================================
//  Static helpers for buildFullContext
// ===========================================================================
static juce::String getTechniqueLabel(MentorPhase phase)
{
    switch (phase) {
        case MentorPhase::Organizacion: return "Organization";
        case MentorPhase::GainStaging:  return "Gain Staging";
        case MentorPhase::Balance:      return "Balance";
        case MentorPhase::EQ:           return "EQ";
        case MentorPhase::Compresion:   return "Compression";
        case MentorPhase::Espacio:      return "Reverb & Delay";
        case MentorPhase::MasterCheck:  return "Mastering";
        default:                        return "Mixing Workflow";
    }
}

// ===========================================================================
//  buildPluginsFromCategories - Format plugin JSON categories
// ===========================================================================
static juce::String buildPluginsFromCategories(const juce::var& json)
{
    juce::String s;
    if (!json.isObject()) return s;

    auto* obj = json.getDynamicObject();
    if (obj == nullptr) return s;

    auto props = obj->getProperties();
    for (int i = 0; i < props.size(); ++i) {
        auto key = props.getName(i).toString();
        if (key.startsWith("_")) continue;

        auto catArr = props.getValueAt(i).getArray();
        if (catArr == nullptr || catArr->isEmpty()) continue;

        int shown = 0;
        for (int j = 0; j < catArr->size() && shown < 2; ++j) {
            auto plugObj = (*catArr)[j].getDynamicObject();
            if (plugObj == nullptr) continue;

            juce::String name = plugObj->getProperty("name").toString();
            juce::String tip = plugObj->getProperty("tip").toString();
            juce::String price = plugObj->getProperty("price").toString();

            s += "  [" + key + "] " + name;
            if (price.isNotEmpty()) s += " (" + price + ")";
            s += ":\n    " + tip + "\n";
            ++shown;
        }
    }
    return s;
}

// ===========================================================================
//  buildPluginCatalog - Load and format the plugin catalog for LLM context
// ===========================================================================
juce::String AiCoachAdapter::buildPluginCatalog() const
{
    if (pluginCatalogLoaded_)
        return pluginCatalog_;

    juce::String catalog;
    catalog += "[DAW CONTEXT]\nDAW: " + dawName_ + "\n\n";

    juce::String nativeRaw = loadKnowledgeFile("plugins/daw_native.json");
    if (nativeRaw.isNotEmpty()) {
        auto json = juce::JSON::parse(nativeRaw);
        if (json.isObject() && json.getDynamicObject() != nullptr) {
            catalog += "Native plugins available:\n";
            catalog += buildPluginsFromCategories(json.getProperty(dawName_, {}));
        }
    }

    juce::String freeRaw = loadKnowledgeFile("plugins/free_plugins.json");
    if (freeRaw.isNotEmpty()) {
        auto json = juce::JSON::parse(freeRaw);
        catalog += "Free alternatives:\n";
        catalog += buildPluginsFromCategories(json);
    }

    pluginCatalog_ = catalog;
    pluginCatalogLoaded_ = true;
    return catalog;
}

// ===========================================================================
//  buildCompactSummary - Brief current state summary
// ===========================================================================
juce::String AiCoachAdapter::buildCompactSummary() const
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();
    const auto& master = audioAnalyzer_.getMasterAnalysis();
    float peakL = audioAnalyzer_.getLeftAnalysis().getPeak();
    float peakR = audioAnalyzer_.getRightAnalysis().getPeak();
    float shortTerm = audioAnalyzer_.getShortTermLUFS();

    juce::String summary;
    summary += "[MIXCOACH STATUS - " + phaseLabel(phaseManager_.getCurrentPhase()) + "]\n";
    summary += "  Tracks: " + juce::String(active) + " active";
    if (coachEngine_.hasReference())
        summary += " | Ref: " + coachEngine_.getReferenceName();
    summary += "\n";
    summary += "  Master: Peak " + formatDb(juce::jmax(peakL, peakR))
               + " | LUFS ST: " + formatLUFS(shortTerm) + "\n";
    summary += "  Genre: " + genre_ + "\n";
    return summary;
}

// ===========================================================================
//  buildUserQueryPrompt - Full context + user message for LLM
// ===========================================================================
juce::String AiCoachAdapter::buildUserQueryPrompt(const juce::String& userMessage) const
{
    return buildFullContext() + "\n[USER]\n" + userMessage;
}

} // namespace mixcoach

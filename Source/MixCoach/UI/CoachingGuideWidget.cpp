#include "CoachingGuideWidget.h"
#include "../engine/PluginSuggestionsProvider.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  StageInfo local helpers (mirrors CoachingStageManager::StageInfo
    //  but adds tier suggestion defaults)
    // ═══════════════════════════════════════════════════════════════════════════

    static const char* stageShortName(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging: return "Gain Staging";
            case CoachingStage::Balance:     return "Balance";
            case CoachingStage::EQ:          return "EQ";
            case CoachingStage::Compression: return "Compresi\u00F3n";
        case CoachingStage::Spatial:     return "Espacial";
        case CoachingStage::Automation:  return "Automatización";
        case CoachingStage::Refinement:  return "Refinamiento";
            default:                         return "";
        }
    }

    static const char* stageIcon(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging: return "\xF0\x9F\x8E\x9A"; // 🎚️
            case CoachingStage::Balance:     return "\xE2\x9A\x96";      // ⚖️
            case CoachingStage::EQ:          return "\xE2\x97\x89";      // ◉
            case CoachingStage::Compression: return "\xE2\x96\xA0";      // ■    case CoachingStage::Spatial:     return "\xF0\x9F\x8C\x8A";              // 🌊
        case CoachingStage::Automation:  return "\xE2\x8F\xB1";          // ⏱️
        case CoachingStage::Refinement:  return "\xE2\x9C\xA8";              // ✨
            default:                         return "";
        }
    }

    // ─── Default 3-tier suggestions per stage ─────────────────────────────
    struct TierDefaults {
        const char* ajusta;   // Native plugin tip
        const char* verifica; // Free plugin tip
        const char* mejora;   // Premium plugin tip
    };

    static const TierDefaults& getTierDefaults(CoachingStage stage) noexcept
    {
        static const TierDefaults defaults[] = {
            // GainStaging
            { "Usa Fruity Balance para ajustar niveles r\u00E1pido",
              "Descarga YouLean Loudness Meter para monitorear LUFS",
              "Hazte con Hornet VU Meter: medici\u00F3n profesional de nivel" },
            // Balance
            { "Fruity Stereo Shaper: paneo b\u00E1sico y ancho est\u00E9reo",
              "Prueba Flux Stereo Tool para verificaci\u00F3n de balance",
              "iZotope Relay: balance preciso con visualizaci\u00F3n espectral" },
            // EQ
            { "Parametric EQ 2 de FL Studio: ecualizaci\u00F3n quir\u00FArgica",
              "TDR Nova: EQ din\u00E1mico gratuito con spectrum visual",
              "FabFilter Pro-Q 3: el est\u00E1ndar de EQ profesional" },
            // Compression
            { "Fruity Compressor: compresi\u00F3n b\u00E1sica en cada pista",
              "Rough Rider 3: compresor gratuito con car\u00E1cter",
              "FabFilter Pro-C 2: compresi\u00F3n transparente de nivel mundial" },
            // Spatial
            { "Fruity Reverb 2: reverb nativa con buenos presets",
              "Valhalla Supermassive: reverb y delay gratuito \u00E9pico",
              "ValhallaVintageVerb: reverb de clase mundial por $50" },
            // Automation
            { "Fruity Love Philter: automatizaci\u00F3n de filtros nativa",
              "TDR Kotelnikov: compresor con sidechain gratuito",
              "Waves Vocal Rider: automatizaci\u00F3n vocal autom\u00E1tica" },
            // Refinement
            { "Maximus: multiband nativo para pegamento y punch final",
              "Ozone 11 EQ: limpieza final con asistente de referencia",
              "oeksound Soothe 2: control din\u00E1mico de frecuencias problem\u00E1ticas" }
        };
        auto idx = static_cast<int>(stage);
        if (idx < 0 || idx >= static_cast<int>(CoachingStage::COUNT)) return defaults[0];
        return defaults[idx];
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════

    CoachingGuideWidget::CoachingGuideWidget()
    {
        setOpaque(false);
        setSize(280, 400);
        setDefaultTierSuggestions(CoachingStage::GainStaging);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Data updates
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::updateFromStageManager(const CoachingStageManager& manager)
    {
        currentStage_ = manager.getCurrentStage();
        stageProgress_ = manager.getStageProgress();
        overallProgress_ = manager.getOverallProgress();
        awaitingApproval_ = manager.isAwaitingApproval();
        allComplete_ = manager.isComplete();

        // Update completed flags
        for (int i = 0; i < static_cast<int>(CoachingStage::COUNT); ++i) {
            auto stage = static_cast<CoachingStage>(i);
            stageCompletedFlags_[i] = manager.isStageCompleted(stage);
        }

        setDefaultTierSuggestions(currentStage_);
        resized();
        repaint();
    }

    void CoachingGuideWidget::setStageDirectly(CoachingStage stage,
                                                 float stageProgress,
                                                 float overallProgress,
                                                 const PluginSuggestionsProvider* provider)
    {
        currentStage_ = stage;
        stageProgress_ = stageProgress;
        overallProgress_ = overallProgress;
        awaitingApproval_ = false;
        allComplete_ = false;

        // Mark all stages before current as completed
        int currentIdx = static_cast<int>(stage);
        for (int i = 0; i < static_cast<int>(CoachingStage::COUNT); ++i) {
            stageCompletedFlags_[i] = (i < currentIdx);
        }

        // Usar provider si está disponible, sino defaults hardcodeados
        if (provider != nullptr)
            setStageSuggestionsFromProvider(*provider, stage);
        else
            setDefaultTierSuggestions(stage);

        resized();
        repaint();
    }

    void CoachingGuideWidget::setTierSuggestions(const juce::String& ajusta,
                                                  const juce::String& verifica,
                                                  const juce::String& mejora)
    {
        ajustaSuggestion_ = ajusta;
        verificaSuggestion_ = verifica;
        mejoraSuggestion_ = mejora;
        repaint();
    }

    void CoachingGuideWidget::setDefaultTierSuggestions(CoachingStage stage)
    {
        auto& d = getTierDefaults(stage);
        ajustaSuggestion_ = juce::String(d.ajusta);
        verificaSuggestion_ = juce::String(d.verifica);
        mejoraSuggestion_ = juce::String(d.mejora);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setStageSuggestionsFromProvider — Query real plugin database per stage
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::setStageSuggestionsFromProvider(
        const PluginSuggestionsProvider& provider,
        CoachingStage stage)
    {
        if (!provider.isReady()) {
            setDefaultTierSuggestions(stage);
            return;
        }

        // ═══ Map CoachingStage → ProblemTypes ═════════════════════════════
        // Cada etapa se asocia a 1-3 problemas típicos de mezcla.
        static const std::vector<ProblemType> stageProblems[] = {
            { ProblemType::Gain, ProblemType::Clipping },                          // GainStaging
            { ProblemType::Spatial, ProblemType::Gain },                           // Balance
            { ProblemType::TonalExcess, ProblemType::TonalDeficit, ProblemType::Masking }, // EQ
            { ProblemType::DynamicsOvercompressed, ProblemType::DynamicsTooDynamic },       // Compression
            { ProblemType::Spatial, ProblemType::Reverb, ProblemType::Phase },     // Spatial
            { ProblemType::Spatial, ProblemType::Reverb },                         // Automation
            { ProblemType::Saturation, ProblemType::Limiting }                     // Refinement
        };

        int idx = static_cast<int>(stage);
        if (idx < 0 || idx >= static_cast<int>(CoachingStage::COUNT)) { setDefaultTierSuggestions(stage); return; }

        auto& problems = stageProblems[idx];

        juce::String ajustaText;
        juce::String verificaText;
        juce::String mejoraText;

        // ═══ Query provider for each problem, collect best per tier ═══════
        for (auto problem : problems) {
            auto suggestions = provider.getSuggestionsForProblem(problem);
            for (auto& sug : suggestions) {
                if (!sug.isValid() || sug.plugin == nullptr) continue;

                auto action = PluginSuggestionsProvider::interpolateAction(
                    sug.config->actionText, 0.0f, 0.0f);
                juce::String entry = juce::String(sug.plugin->name);
                if (action.isNotEmpty())
                    entry += ": " + action;

                switch (sug.plugin->tier) {
                    case PluginTier::Native:
                        if (ajustaText.isEmpty()) ajustaText = entry;
                        break;
                    case PluginTier::Free:
                        if (verificaText.isEmpty()) verificaText = entry;
                        break;
                    case PluginTier::Premium:
                        if (mejoraText.isEmpty()) mejoraText = entry;
                        break;
                    default:
                        break;
                }
            }
        }

        // ═══ Fallback a defaults si algún tier quedó vacío ═══════════════
        auto& d = getTierDefaults(stage);
        if (ajustaText.isEmpty())   ajustaText   = juce::String(d.ajusta);
        if (verificaText.isEmpty()) verificaText = juce::String(d.verifica);
        if (mejoraText.isEmpty())   mejoraText   = juce::String(d.mejora);

        setTierSuggestions(ajustaText, verificaText, mejoraText);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — Calcula layout completo
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::resized()
    {
        auto bounds = getLocalBounds().reduced(6, 4);

        // ─── Section 1: Stage Timeline (6 items × kTimelineItemH) ──────────
        int timelineH = static_cast<int>(CoachingStage::COUNT) * kTimelineItemH + (static_cast<int>(CoachingStage::COUNT) - 1) * kTimelineGap;
        bounds.removeFromTop(timelineH);
        bounds.removeFromTop(kSectionGap);

        // ─── Section 2: Current Stage Detail ───────────────────────────────
        // Description line ~36px + Progress bar ~14px + Checklist ~30px
        int detailH = 36 + 14 + 30;
        bounds.removeFromTop(detailH);
        bounds.removeFromTop(kSectionGap);

        // ─── Section 3: 3-Tier Suggestion Buttons ──────────────────────────
        int tiersH = 3 * kTierButtonH + 2 * kTimelineGap;
        auto tiersArea = bounds.removeFromTop(tiersH);

        // Store tier button bounds
        ajustaBtnBounds_ = tiersArea.removeFromTop(kTierButtonH).toFloat().reduced(2, 1);
        tiersArea.removeFromTop(kTimelineGap);
        verificaBtnBounds_ = tiersArea.removeFromTop(kTierButtonH).toFloat().reduced(2, 1);
        tiersArea.removeFromTop(kTimelineGap);
        mejoraBtnBounds_ = tiersArea.removeFromTop(kTierButtonH).toFloat().reduced(2, 1);

        bounds.removeFromTop(kSectionGap);

        // ─── Section 4: Bottom Action Buttons ──────────────────────────────
        auto actionsArea = bounds;
        advanceBtnBounds_ = actionsArea.removeFromTop(kActionButtonH).toFloat().reduced(2, 1);
        actionsArea.removeFromTop(kTimelineGap);
        helpBtnBounds_ = actionsArea.removeFromTop(kActionButtonH).toFloat().reduced(2, 1);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja el widget completo
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        const float cr = 8.0f;

        // ─── Shadow ────────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.toFloat().expanded(1, 2), cr + 1);

        // ─── Background glass ──────────────────────────────────────────────────
        juce::ColourGradient bgGrad(juce::Colour(0xE8180838),
                                    (float)bounds.getX(), (float)bounds.getY(),
                                    juce::Colour(0xE8060820),
                                    (float)bounds.getX(), (float)bounds.getBottom(),
                                    false);
        bgGrad.addColour(0.5f, juce::Colour(0xE8101830));
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds.toFloat(), cr);

        // ─── Border ────────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.drawRoundedRectangle(bounds.toFloat(), cr, 0.8f);

        // ─── Left accent bar ──────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.30f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>((float)bounds.getX() + 1.5f, (float)bounds.getY() + 4,
                                   2.0f, (float)bounds.getHeight() - 8),
            1.0f);

        auto area = getLocalBounds().reduced(6, 4);

        // ═══ 1. STAGE TIMELINE ═══════════════════════════════════════════════
        drawStageTimeline(g, area);

        int timelineH = static_cast<int>(CoachingStage::COUNT) * kTimelineItemH + (static_cast<int>(CoachingStage::COUNT) - 1) * kTimelineGap;
        area.removeFromTop(timelineH + kSectionGap);

        // ═══ 2. CURRENT STAGE DETAIL ═══════════════════════════════════════
        drawStageDetail(g, area);

        int detailH = 36 + 14 + 30;
        area.removeFromTop(detailH + kSectionGap);

        // ═══ 3. 3-TIER SUGGESTIONS ═════════════════════════════════════════
        drawTierSuggestions(g, area);

        int tiersH = 3 * kTierButtonH + 2 * kTimelineGap;
        area.removeFromTop(tiersH + kSectionGap);

        // ═══ 4. BOTTOM ACTIONS ═════════════════════════════════════════════
        drawBottomActions(g, area);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStageTimeline — Muestra todas las 6 etapas verticalmente
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::drawStageTimeline(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto rect = area.toFloat();
        float x = rect.getX() + 4.0f;
        float y = rect.getY();

        // ─── Vertical line connecting all stages ─────────────────────────────
        float lineX = x + 8.0f;
        float lineY0 = y + 12.0f;
        float lineY1 = y + static_cast<float>(CoachingStage::COUNT) * kTimelineItemH + (static_cast<float>(CoachingStage::COUNT) - 1.0f) * kTimelineGap - 12.0f;
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine((int)lineX, lineY0, lineY1);

        for (int i = 0; i < static_cast<int>(CoachingStage::COUNT); ++i) {
            auto stage = static_cast<CoachingStage>(i);
            bool isCurrent = (stage == currentStage_);
            bool isCompleted = (i < static_cast<int>(CoachingStage::COUNT) && stageCompletedFlags_[i]);
            bool isFuture = (!isCurrent && !isCompleted);
            bool isHovered = (i == hoveredStage_);

            float itemY = y + i * (kTimelineItemH + kTimelineGap);

            // ─── Dot on timeline ─────────────────────────────────────────────
            float dotCx = lineX;
            float dotCy = itemY + kTimelineItemH / 2.0f;
            float dotR = isCurrent ? 5.0f : 4.0f;

            juce::Colour dotColour;
            if (isCompleted) dotColour = MixCoachTheme::success();
            else if (isCurrent) dotColour = MixCoachTheme::accentGlow();
            else dotColour = MixCoachTheme::textMuted().withAlpha(0.3f);

            // Outer glow for current stage
            if (isCurrent) {
                float pulse = 0.5f + 0.5f * std::sin(juce::Time::getMillisecondCounter() * 0.004f);
                g.setColour(dotColour.withAlpha(0.20f * pulse));
                g.fillEllipse(dotCx - 8.0f, dotCy - 8.0f, 16.0f, 16.0f);
                g.setColour(dotColour.withAlpha(0.12f * pulse));
                g.fillEllipse(dotCx - 12.0f, dotCy - 12.0f, 24.0f, 24.0f);
            }

            g.setColour(dotColour);
            g.fillEllipse(dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);

            // ─── Icon ────────────────────────────────────────────────────────
            auto iconArea = juce::Rectangle<float>(x + 18.0f, itemY + 4.0f, 22.0f, (float)kTimelineItemH);
            float iconAlpha = isFuture ? 0.30f : (isCurrent ? 1.0f : 0.65f);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(juce::Colours::white.withAlpha(iconAlpha));
            g.drawText(juce::String(juce::CharPointer_UTF8(stageIcon(stage))),
                       iconArea, juce::Justification::centredLeft);

            // ─── Stage name ──────────────────────────────────────────────────
            auto nameArea = iconArea.translated(24.0f, 0.0f)
                                .withWidth(rect.getWidth() - 60.0f);
            g.setFont(juce::Font(juce::FontOptions(isCurrent ? 9.5f : 8.5f))
                          .boldened());
            g.setColour(isCurrent ? MixCoachTheme::textBright()
                        : isFuture ? MixCoachTheme::textMuted().withAlpha(0.35f)
                        : MixCoachTheme::textPrimary().withAlpha(0.7f));
            g.drawText(juce::String(juce::CharPointer_UTF8(stageShortName(stage))),
                       nameArea, juce::Justification::centredLeft);

            // ─── Checkmark for completed stages ──────────────────────────────
            if (isCompleted) {
                auto checkArea = juce::Rectangle<float>(
                    nameArea.getRight() - 16.0f, itemY + 4.0f,
                    16.0f, (float)kTimelineItemH);
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.setColour(MixCoachTheme::success());
                g.drawText("\xE2\x9C\x93", checkArea, juce::Justification::centred);  // ✓
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStageDetail — Muestra detalles de la etapa actual
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::drawStageDetail(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto rect = area.toFloat();

        // ─── Stage name + completing message ─────────────────────────────────
        auto headerArea = rect.removeFromTop(36.0f);

        // Icon + name
        auto iconArea = headerArea.removeFromLeft(22.0f);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawText(juce::String(juce::CharPointer_UTF8(stageIcon(currentStage_))),
                   iconArea, juce::Justification::centredLeft);
        iconArea.removeFromRight(20.0f);

        auto nameArea = headerArea;
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText(juce::String(juce::CharPointer_UTF8(stageShortName(currentStage_))),
                   nameArea, juce::Justification::centredLeft);

        // ─── Progress bar ────────────────────────────────────────────────────
        auto progressArea = rect.removeFromTop(kProgressBarH + 4.0f).reduced(0, 2);
        drawProgressBar(g, progressArea);

        // ─── Info text ───────────────────────────────────────────────────────
        rect.removeFromTop(4.0f);
        juce::String infoText;

        if (allComplete_) {
            infoText = "\xE2\x9C\xA8 Todas las etapas completadas!";
        } else if (awaitingApproval_) {
            infoText = "\xF0\x9F\x91\x8D \xBFListo para avanzar?";
        } else {
            switch (currentStage_) {
                case CoachingStage::GainStaging:
                    infoText = "Ajusta niveles sin clipping. Headroom: -18 a -3 dB.";
                    break;
                case CoachingStage::Balance:
                    infoText = "Balancea faders y paneo. Sin procesar aun.";
                    break;
                case CoachingStage::EQ:
                    infoText = "Corrige balance tonal. Carving espectral.";
                    break;
                case CoachingStage::Compression:
                    infoText = "Controla dinamica. Crest factor: 6-14 dB.";
                    break;
                case CoachingStage::Spatial:
                    infoText = "Crea profundidad con reverb, delay y ancho.";
                    break;
                case CoachingStage::Automation:
                    infoText = "Automatizacion: LUFS, loudness, secciones.";
                    break;
                case CoachingStage::Refinement:
                    infoText = "Refinamiento artistico: profundidad, emocion.";
                    break;
            }
        }

        auto infoArea = rect.removeFromTop(22.0f).reduced(2, 0);
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText(infoText, infoArea, juce::Justification::centredLeft);

        // ═══ Live meters per stage (actualizado en tiempo real) ═══════════
        auto meterArea = rect.removeFromTop(30.0f).reduced(2, 0);
        if (!allComplete_ && !awaitingApproval_) {
            auto& d = liveMeterData_;
            juce::String meterLine1, meterLine2;
            juce::Colour valColour1 = MixCoachTheme::accentGlow();
            juce::Colour valColour2 = MixCoachTheme::accentGlow();

            switch (currentStage_) {
                case CoachingStage::GainStaging:
                    // Peak + RMS con colores por rango
                    valColour1 = (d.peakDb > -3.0f) ? MixCoachTheme::error()
                                : (d.peakDb > -6.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine1 = "Pico: " + juce::String(d.peakDb, 1) + " dB";
                    valColour2 = (d.rmsDb > -3.0f) ? MixCoachTheme::error()
                                : (d.rmsDb > -12.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine2 = "RMS: " + juce::String(d.rmsDb, 1) + " dB  |  Crest: " + juce::String(d.crestFactor, 1) + " dB";
                    break;

                case CoachingStage::Balance:
                    meterLine1 = "Pistas: " + juce::String(d.activeTrackCount) + " activas";
                    valColour2 = (d.stereoWidth < 0.3f) ? MixCoachTheme::warning()
                                : (d.stereoWidth > 0.9f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine2 = "Ancho: " + juce::String(d.stereoWidth * 100.0f, 0) + "%  |  Corr: " + juce::String(d.correlation, 2);
                    break;

                case CoachingStage::EQ:
                    meterLine1 = "Centroide: " + juce::String(d.spectralCentroidHz, 0) + " Hz";
                    valColour2 = (d.crestFactor < 4.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine2 = "Crest: " + juce::String(d.crestFactor, 1) + " dB  |  Pico: " + juce::String(d.peakDb, 1) + " dB";
                    break;

                case CoachingStage::Compression:
                    valColour1 = (d.crestFactor < 4.0f) ? MixCoachTheme::error()
                                : (d.crestFactor < 8.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine1 = "Crest: " + juce::String(d.crestFactor, 1) + " dB";
                    valColour2 = (d.rmsDb > -10.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine2 = "RMS: " + juce::String(d.rmsDb, 1) + " dB  |  Pico: " + juce::String(d.peakDb, 1) + " dB";
                    break;

                case CoachingStage::Spatial:
                    valColour1 = (d.correlation < -0.3f) ? MixCoachTheme::error()
                                : (d.correlation < 0.3f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine1 = "Corr: " + juce::String(d.correlation, 2);
                    valColour2 = (d.stereoWidth < 0.3f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine2 = "Ancho: " + juce::String(d.stereoWidth * 100.0f, 0) + "%  |  Pistas: " + juce::String(d.activeTrackCount);
                    break;

                case CoachingStage::Automation:
                    valColour1 = (d.lufsIntegrated < -18.0f) ? MixCoachTheme::warning()
                                : (d.lufsIntegrated < -8.0f) ? MixCoachTheme::success()
                                : MixCoachTheme::error();
                    meterLine1 = "LUFS: " + juce::String(d.lufsIntegrated, 1);
                    valColour2 = (d.loudnessRange < 6.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine2 = "LRA: " + juce::String(d.loudnessRange, 1) + " dB  |  Secciones: " + juce::String(d.activeTrackCount);
                    break;

                case CoachingStage::Refinement:
                    valColour1 = (d.crestFactor < 4.0f) ? MixCoachTheme::error()
                                : (d.crestFactor < 6.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::success();
                    meterLine1 = "Crest: " + juce::String(d.crestFactor, 1) + " dB";
                    valColour2 = (d.spectralCentroidHz < 800.0f) ? MixCoachTheme::warning()
                                : MixCoachTheme::accentGlow();
                    meterLine2 = "Centroide: " + juce::String(d.spectralCentroidHz, 0) + " Hz"
                                + "  |  Pistas: " + juce::String(d.activeTrackCount) + "/" + juce::String(d.totalTrackCount);
                    break;
            }

            // ─── Line 1 ────────────────────────────────────────────────────
            auto m1l = meterArea.removeFromTop(15.0f);
            auto m1label = m1l.removeFromLeft(60.0f);
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("\xE2\x96\x88", m1label.removeFromLeft(10), juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(valColour1);
            g.drawText(meterLine1, m1label, juce::Justification::centredLeft);

            // ─── Line 2 ────────────────────────────────────────────────────
            auto m2l = meterArea;
            auto m2label = m2l.removeFromLeft(60.0f);
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("\xE2\x96\x88", m2label.removeFromLeft(10), juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(valColour2);
            g.drawText(meterLine2, m2label, juce::Justification::centredLeft);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawProgressBar — Barra de progreso horizontal animada
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::drawProgressBar(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Background track
        g.setColour(MixCoachTheme::bgInput().withAlpha(0.4f));
        g.fillRoundedRectangle(area, 3.0f);

        // Fill
        float fillW = area.getWidth() * stageProgress_;
        if (fillW > 1.0f) {
            auto fillRect = area.withWidth(fillW);
            juce::Colour fillColour = (stageProgress_ >= 0.8f) ? MixCoachTheme::success()
                                       : (stageProgress_ >= 0.4f) ? MixCoachTheme::warning()
                                       : MixCoachTheme::accent();
            juce::ColourGradient fillGrad(fillColour.withAlpha(0.7f),
                                          fillRect.getX(), fillRect.getY(),
                                          fillColour.withAlpha(0.3f),
                                          fillRect.getX(), fillRect.getBottom(),
                                          false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillRect, 3.0f);
        }

        // Percentage text
        auto labelRect = area.translated(area.getWidth() + 4.0f, -2.0f)
                             .withWidth(32.0f);
        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
        g.drawText(juce::String(static_cast<int>(stageProgress_ * 100.0f)) + "%",
                   labelRect, juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTierSuggestions — Muestra 3 botones de sugerencia
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::drawTierSuggestions(juce::Graphics& g, juce::Rectangle<int> area)
    {
        juce::ignoreUnused(area);

        // ─── Tier 1: AJUSTA (Native) ───────────────────────────────────────
        {
            bool hovered = (hoveredTier_ == 0);
            auto pill = ajustaBtnBounds_;
            juce::Colour tierColour = MixCoachTheme::accent(); // Purple for Native

            if (hovered) {
                g.setColour(tierColour.withAlpha(0.18f));
                g.fillRoundedRectangle(pill.expanded(2, 2), 5.0f);
            }

            g.setColour(tierColour.withAlpha(0.12f));
            g.fillRoundedRectangle(pill, 5.0f);
            g.setColour(tierColour.withAlpha(0.35f));
            g.drawRoundedRectangle(pill, 5.0f, 0.6f);

            // Icon + label
            auto textArea = pill.reduced(8, 0);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(tierColour);
            g.drawText("\xE2\x96\xB8 AJUSTA", textArea.removeFromLeft(80),  // ▸
                       juce::Justification::centredLeft);

            // Suggestion text
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.7f));
            g.drawText(ajustaSuggestion_, textArea, juce::Justification::centredLeft);
        }

        // ─── Tier 2: VERIFICA (Free) ───────────────────────────────────────
        {
            bool hovered = (hoveredTier_ == 1);
            auto pill = verificaBtnBounds_;
            juce::Colour tierColour = MixCoachTheme::success(); // Green for Free

            if (hovered) {
                g.setColour(tierColour.withAlpha(0.18f));
                g.fillRoundedRectangle(pill.expanded(2, 2), 5.0f);
            }

            g.setColour(tierColour.withAlpha(0.12f));
            g.fillRoundedRectangle(pill, 5.0f);
            g.setColour(tierColour.withAlpha(0.35f));
            g.drawRoundedRectangle(pill, 5.0f, 0.6f);

            auto textArea = pill.reduced(8, 0);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(tierColour);
            g.drawText("\xF0\x9F\x9F\xA2 VERIFICA", textArea.removeFromLeft(90),
                       juce::Justification::centredLeft);

            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.7f));
            g.drawText(verificaSuggestion_, textArea, juce::Justification::centredLeft);
        }

        // ─── Tier 3: MEJORA (Premium) ───────────────────────────────────────
        {
            bool hovered = (hoveredTier_ == 2);
            auto pill = mejoraBtnBounds_;
            juce::Colour tierColour = MixCoachTheme::warning(); // Amber/Orange for Premium

            if (hovered) {
                g.setColour(tierColour.withAlpha(0.18f));
                g.fillRoundedRectangle(pill.expanded(2, 2), 5.0f);
            }

            g.setColour(tierColour.withAlpha(0.12f));
            g.fillRoundedRectangle(pill, 5.0f);
            g.setColour(tierColour.withAlpha(0.35f));
            g.drawRoundedRectangle(pill, 5.0f, 0.6f);

            auto textArea = pill.reduced(8, 0);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(tierColour);
            g.drawText("\xE2\xAD\x90 MEJORA", textArea.removeFromLeft(85),
                       juce::Justification::centredLeft);

            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.7f));
            g.drawText(mejoraSuggestion_, textArea, juce::Justification::centredLeft);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawBottomActions — Botones de acción en la parte inferior
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::drawBottomActions(juce::Graphics& g, juce::Rectangle<int> area)
    {
        juce::ignoreUnused(area);

        // ─── Advance button (or "Complete!" if all complete) ─────────────────
        {
            auto btn = advanceBtnBounds_;
            bool hovered = hoveringAdvance_;

            juce::Colour btnColour = allComplete_
                                         ? MixCoachTheme::success()
                                         : (awaitingApproval_ ? MixCoachTheme::accentGlow()
                                                              : MixCoachTheme::accent());

            if (hovered) {
                g.setColour(btnColour.withAlpha(0.20f));
                g.fillRoundedRectangle(btn.expanded(2, 2), 5.0f);
            }

            g.setColour(btnColour.withAlpha(0.10f));
            g.fillRoundedRectangle(btn, 5.0f);
            g.setColour(btnColour.withAlpha(0.30f));
            g.drawRoundedRectangle(btn, 5.0f, 0.6f);

            juce::String btnText;
            if (allComplete_) btnText = "\xE2\x9C\xA8 \xA1Todo completo!";
            else if (awaitingApproval_) btnText = "\xF0\x9F\x91\x8D Siguiente etapa";
            else btnText = "Ver progreso";

            g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
            g.setColour(btnColour);
            g.drawText(btnText, btn, juce::Justification::centred);
        }

        // ─── Help button ────────────────────────────────────────────────────
        {
            auto btn = helpBtnBounds_;
            bool hovered = hoveringHelp_;

            if (hovered) {
                g.setColour(MixCoachTheme::info().withAlpha(0.15f));
                g.fillRoundedRectangle(btn.expanded(2, 2), 5.0f);
            }

            g.setColour(MixCoachTheme::info().withAlpha(0.06f));
            g.fillRoundedRectangle(btn, 5.0f);
            g.setColour(MixCoachTheme::info().withAlpha(0.20f));
            g.drawRoundedRectangle(btn, 5.0f, 0.5f);

            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::info());
            g.drawText("\xF0\x9F\x92\xA1 Consejos para esta etapa", btn,
                       juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Mouse handling — Hover + click detection
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachingGuideWidget::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();

        int oldStage = hoveredStage_;
        int oldTier = hoveredTier_;
        bool oldAdvance = hoveringAdvance_;
        bool oldHelp = hoveringHelp_;

        hoveredStage_ = -1;
        hoveredTier_ = -1;
        hoveringAdvance_ = false;
        hoveringHelp_ = false;

        // Check tier buttons
        if (ajustaBtnBounds_.contains(pos)) hoveredTier_ = 0;
        else if (verificaBtnBounds_.contains(pos)) hoveredTier_ = 1;
        else if (mejoraBtnBounds_.contains(pos)) hoveredTier_ = 2;

        // Check action buttons
        else if (advanceBtnBounds_.contains(pos)) hoveringAdvance_ = true;
        else if (helpBtnBounds_.contains(pos)) hoveringHelp_ = true;

        if (hoveredTier_ >= 0 || hoveringAdvance_ || hoveringHelp_)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);

        if (oldStage != hoveredStage_ || oldTier != hoveredTier_
            || oldAdvance != hoveringAdvance_ || oldHelp != hoveringHelp_)
            repaint();
    }

    void CoachingGuideWidget::mouseExit(const juce::MouseEvent&)
    {
        if (hoveredStage_ >= 0 || hoveredTier_ >= 0
            || hoveringAdvance_ || hoveringHelp_) {
            hoveredStage_ = -1;
            hoveredTier_ = -1;
            hoveringAdvance_ = false;
            hoveringHelp_ = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void CoachingGuideWidget::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();

        // ─── Tier buttons ────────────────────────────────────────────────────
        if (ajustaBtnBounds_.contains(pos)) {
            if (onTierClicked) onTierClicked(0);
            return;
        }
        if (verificaBtnBounds_.contains(pos)) {
            if (onTierClicked) onTierClicked(1);
            return;
        }
        if (mejoraBtnBounds_.contains(pos)) {
            if (onTierClicked) onTierClicked(2);
            return;
        }

        // ─── Advance button ──────────────────────────────────────────────────
        if (advanceBtnBounds_.contains(pos)) {
            if (onAdvanceStage) onAdvanceStage();
            return;
        }

        // ─── Help button ─────────────────────────────────────────────────────
        if (helpBtnBounds_.contains(pos)) {
            if (onRequestHelp) onRequestHelp();
            return;
        }
    }

} // namespace mixcoach

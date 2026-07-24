#include "SessionPrepChecklist.h"
#include "../engine/CoachEngine.h"

namespace mixcoach {

    namespace {
        // Constantes de layout
        constexpr float kCardRadius   = 14.0f;
        constexpr float kRowHeight    = 38.0f;
        constexpr float kContinueH   = 44.0f;
        constexpr float kPad          = 18.0f;
        constexpr int   kRefreshHz    = 30;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor / Timer
    // ═══════════════════════════════════════════════════════════════════════════
    SessionPrepChecklist::SessionPrepChecklist()
    {
        setInterceptsMouseClicks(true, false);
        animStartMs_ = juce::Time::getMillisecondCounter();
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void SessionPrepChecklist::visibilityChanged()
    {
        if (isShowing()) {
            startTimerHz(kRefreshHz);
        } else {
            stopTimer();
        }
    }

    void SessionPrepChecklist::timerCallback()
    {
        refreshAndRepaint();
    }

    void SessionPrepChecklist::setVisible(bool show)
    {
        if (show && !isVisible()) {
            animStartMs_ = juce::Time::getMillisecondCounter();
            fadeIn_ = true;
            fadeAlpha_ = 0.0f;
            startTimerHz(kRefreshHz);
        } else if (!show) {
            stopTimer();
        }
        Component::setVisible(show);
    }

    // ─── Previous state for detecting item transitions ─────────────────────
    namespace {
        constexpr int kNumChecks = 4;
        bool gPrevChecks[kNumChecks] = {false};
        bool gFirstRefresh = true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  refresh — Calcula los 4 checks iterando el SlotRegistry (Message Thread)
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionPrepChecklist::refresh()
    {
        // Save previous state for transition detection
        bool prevTracksReady = tracksReady_;
        bool prevOrgReady    = organized_;
        bool prevMessengers  = messengersActive_;
        bool prevMsgrReady   = messengersRoutedAndColored_;
        bool prevChecks[kNumChecks] = {prevTracksReady, prevOrgReady, prevMessengers, prevMsgrReady};

        activeTrackCount_ = 0;
        unamed_           = 0;
        uncolored_        = 0;
        unrouted_         = 0;
        distinctBuses_    = 0;

        if (registry_ == nullptr) {
            tracksReady_ = false;
            organized_ = false;
            messengersActive_ = false;
            messengersRoutedAndColored_ = false;
            return;
        }

        // Buses distintos con pistas (para check agrupación)
        int busUsed[(int)BusType::Melody + 2] = { 0 };

        registry_->forEachActive([&](const SlotInfo& info) {
            ++activeTrackCount_;

            // ─── Check Nombres: vacío o default "Pista NN"
            juce::String name = juce::String(info.trackName).trim();
            if (name.isEmpty() || name.startsWith("Pista "))
                ++unamed_;

            // ─── Check Colores: default grey
            if (info.colour == juce::Colours::grey)
                ++uncolored_;

            // ─── Check Buses: BusType::None
            if (info.bus == BusType::None) {
                ++unrouted_;
            } else {
                int bi = static_cast<int>(info.bus);
                if (bi >= 0 && bi <= (int)BusType::Melody)
                    busUsed[bi] = 1;
            }
        });

        // Contar buses distintos con pistas
        for (int i = 0; i <= (int)BusType::Melody; ++i)
            distinctBuses_ += busUsed[i];

        // ─── Check 1: Tracks insertados ✅ (al menos 1 track activo)
        tracksReady_ = (activeTrackCount_ > 0);

        // ─── Check 2: Organización (nombres + colores + buses + grupos) ✅
        organized_ = (activeTrackCount_ > 0)
                     && (unamed_ == 0)
                     && (uncolored_ == 0)
                     && (unrouted_ == 0)
                     && (distinctBuses_ >= 2);

        // ─── Check 3: Messengers insertados en cada track
        messengersActive_ = (registry_->activeCount() > 0);

        // ─── Check 4: Messengers ruteados y coloreados
        //     Por ahora: mismo check que Item 3 (cuando hay datos de color/ruteo de Messenger, mejorar)
        messengersRoutedAndColored_ = messengersActive_;

        // ─── Detectar transiciones unchecked→checked y disparar onItemChecked ───
        bool nowTracksReady = tracksReady_;
        bool nowOrgReady    = organized_;
        bool nowMessengers  = messengersActive_;
        bool nowMsgrReady   = messengersRoutedAndColored_;
        bool nowChecks[kNumChecks] = {nowTracksReady, nowOrgReady, nowMessengers, nowMsgrReady};

        if (!gFirstRefresh) {
            for (int i = 0; i < kNumChecks; ++i) {
                if (!gPrevChecks[i] && nowChecks[i]) {
                    if (onItemChecked)
                        onItemChecked(i);
                }
            }
        }

        // Update previous state for next iteration
        for (int i = 0; i < kNumChecks; ++i)
            gPrevChecks[i] = nowChecks[i];
        gFirstRefresh = false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Checklist premium con 2 tiempos
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionPrepChecklist::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float cr = kCardRadius;

        // ─── Fade-in de entrada ──────────────────────────────────────────────
        if (fadeIn_) {
            juce::uint32 elapsed = juce::Time::getMillisecondCounter() - animStartMs_;
            float t = juce::jmin(1.0f, (float)elapsed / 250.0f);
            fadeAlpha_ = 1.0f - (1.0f - t) * (1.0f - t);  // ease-out quad
            if (t >= 1.0f) fadeIn_ = false;
        } else {
            fadeAlpha_ = 1.0f;
        }
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::translation(0.0f, (1.0f - fadeAlpha_) * 12.0f));
        g.setOpacity(fadeAlpha_);

        // ─── Sombra + glow sutil ──────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds.expanded(2.0f, 3.0f), cr + 2.0f);

        // ─── Fondo glass ─────────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds, cr);

        // ─── Borde morado tenue (más vivo si organized_) ──────────────────────
        g.setColour(organized_ ? MixCoachTheme::accent().withAlpha(0.35f)
                               : juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(bounds, cr, 1.0f);

        auto area = bounds.reduced(kPad, kPad - 4);

        // ═══ Header: "Preparar sesión" ═══════════════════════════════════════
        auto headerArea = area.removeFromTop(30);
        g.setFont(juce::Font(juce::FontOptions(22.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("\xF0\x9F\x97\xA1  Preparar sesi\xC3\xB3n",
                   headerArea, juce::Justification::centredLeft);

        area.removeFromTop(2);
        auto subArea = area.removeFromTop(16);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText("El orden es la base de toda buena mezcla.",
                   subArea, juce::Justification::centredLeft);

        area.removeFromTop(4);

        // ═══ 4 items — orden según el usuario ═══════════════════════════════
        bool ckTracks    = tracksReady_;              // Check 1: tracks insertados
        bool ckOrganized = organized_;                 // Check 2: agrupar+rutear+colorear
        bool ckMsgr      = messengersActive_;          // Check 3: Messenger en cada track
        bool ckMsgrReady = messengersRoutedAndColored_; // Check 4: Messenger ruteados/coloreados

        // ─── Item 1: Inserta los tracks en tu DAW ──────────────────────────
        auto r1 = area.removeFromTop((int)kRowHeight);  area.removeFromTop(2);
        drawCheckRow(g, r1, ckTracks, "Inserta los tracks en tu DAW",
                     activeTrackCount_ > 0
                        ? juce::String(activeTrackCount_) + " track(s) detectados"
                        : "Esperando tracks...");

        // ─── Item 2: Agrupa, rutea y colorea ───────────────────────────────
        auto r2 = area.removeFromTop((int)kRowHeight);  area.removeFromTop(2);
        {
            juce::String detail;
            if (activeTrackCount_ == 0)
                detail = "Sin tracks a\xC3\xAAn";
            else {
                juce::StringArray parts;
                if (unamed_ > 0)    parts.add(juce::String(unamed_) + " sin nombre");
                if (uncolored_ > 0) parts.add(juce::String(uncolored_) + " sin color");
                if (unrouted_ > 0)  parts.add(juce::String(unrouted_) + " sin bus");
                if (distinctBuses_ < 2) parts.add("Faltan grupos (min. 2 buses)");
                detail = parts.isEmpty() ? juce::String("\xE2\x9C\x93 Todo listo")
                                         : juce::String(activeTrackCount_) + " tracks | " + parts.joinIntoString(", ");
            }
            drawCheckRow(g, r2, ckOrganized, "Agrupa, rutea y colorea", detail);
        }

        // ─── Item 3: Inserta Messenger en cada track ────────────────────────
        auto r3 = area.removeFromTop((int)kRowHeight);  area.removeFromTop(2);
        drawCheckRow(g, r3, ckMsgr, "Inserta Messenger en cada track",
                     ckMsgr
                        ? juce::String("Detectados: ") + juce::String(registry_ ? registry_->activeCount() : 0)
                        : "A\xC3\x9An no insertas ninguno");

        // ─── Item 4: Rutea y colorea los Messengers ─────────────────────────
        auto r4 = area.removeFromTop((int)kRowHeight);  area.removeFromTop(2);
        drawCheckRow(g, r4, ckMsgrReady, "Rutea y colorea los Messengers",
                     ckMsgrReady ? "Messengers listos" : "Pendiente");

        // ═══ Botón Confirmar — SIEMPRE visible ═══════════════════════════════
        area.removeFromTop(8);
        auto btnArea = area.removeFromTop((int)kContinueH);
        // Botón normal cuando allReady, modo informativo si faltan checks
        drawContinueButton(g, btnArea);
    }

    void SessionPrepChecklist::resized()
    {
        //Sin children — todo en paint()
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers de dibujo
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionPrepChecklist::drawCheckRow(juce::Graphics& g, juce::Rectangle<float> area,
                                            bool done, const juce::String& label,
                                            const juce::String& count) const
    {
        // ─── Círculo de check ─────────────────────────────────────────────────
        float circleR = 9.0f;
        float cy = area.getCentreY();
        float cx = area.getX() + circleR + 2.0f;

        if (done) {
            g.setColour(MixCoachTheme::success().withAlpha(0.18f));
            g.fillEllipse(cx - circleR - 2.0f, cy - circleR - 2.0f,
                          (circleR + 2.0f) * 2.0f, (circleR + 2.0f) * 2.0f);
            g.setColour(MixCoachTheme::success());
            g.fillEllipse(cx - circleR, cy - circleR, circleR * 2.0f, circleR * 2.0f);
            // Check mark
            g.setColour(juce::Colours::white);
            juce::Path check;
            check.startNewSubPath(cx - 4.0f, cy);
            check.lineTo(cx - 1.0f, cy + 3.0f);
            check.lineTo(cx + 4.5f, cy - 3.5f);
            g.strokePath(check, juce::PathStrokeType(2.0f));
        } else {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillEllipse(cx - circleR, cy - circleR, circleR * 2.0f, circleR * 2.0f);
            g.setColour(MixCoachTheme::warning().withAlpha(0.5f));
            g.drawEllipse(cx - circleR, cy - circleR, circleR * 2.0f, circleR * 2.0f, 1.2f);
        }

        // ─── Label + count ────────────────────────────────────────────────────
        auto textArea = area.withLeft(cx + circleR + 14.0f);
        g.setFont(juce::Font(juce::FontOptions(15.0f)));
        g.setColour(done ? MixCoachTheme::textBright() : MixCoachTheme::textDim());
        g.drawText(label, textArea.removeFromTop(18), juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(done ? MixCoachTheme::success().withAlpha(0.8f) : MixCoachTheme::warning());
        g.drawText(count, textArea, juce::Justification::centredLeft);
    }

    void SessionPrepChecklist::drawContinueButton(juce::Graphics& g, juce::Rectangle<float> area)
    {
        continueBounds_ = area;
        const float cr = 10.0f;

        bool ready = allReady();

        // Glow — más intenso si allReady
        g.setColour(MixCoachTheme::accent().withAlpha(ready ? (continueHovered_ ? 0.25f : 0.15f)
                                                            : 0.06f));
        g.fillRoundedRectangle(area.expanded(4.0f, 4.0f), cr + 4.0f);

        // Fondo con gradiente — más brillante si allReady
        if (ready) {
            juce::ColourGradient bg(MixCoachTheme::accent(),
                                    area.getX(), area.getY(),
                                    MixCoachTheme::accentDim(),
                                    area.getX(), area.getBottom(),
                                    false);
            g.setGradientFill(bg);
        } else {
            g.setColour(MixCoachTheme::bgPanel().withAlpha(0.5f));
        }
        g.fillRoundedRectangle(area, cr);

        g.setColour(ready ? MixCoachTheme::accent().withAlpha(0.6f)
                          : juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(area, cr, 1.0f);

        // Texto "Confirmar"
        g.setColour(ready ? juce::Colours::white : MixCoachTheme::textMuted().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        auto txtArea = area.withTrimmedRight(28);
        g.drawText("Confirmar" + juce::String(ready ? "  \xE2\x9E\xA1" : ""),  // ➡ si ready
                   txtArea, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Mouse events
    // ═══════════════════════════════════════════════════════════════════════════
    void SessionPrepChecklist::mouseMove(const juce::MouseEvent& e)
    {
        bool nowHover = allReady() && continueBounds_.contains(e.position.toFloat());
        if (nowHover != continueHovered_) {
            continueHovered_ = nowHover;
            setMouseCursor(nowHover ? juce::MouseCursor::PointingHandCursor
                                    : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void SessionPrepChecklist::mouseExit(const juce::MouseEvent&)
    {
        if (continueHovered_) {
            continueHovered_ = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void SessionPrepChecklist::mouseDown(const juce::MouseEvent& e)
    {
        if (!continueBounds_.contains(e.position.toFloat()))
            return;

        if (allReady()) {
            if (onContinue) onContinue();
        } else {
            // Mostrar un guiño visual: el botón ya está visible pero dimmed.
            // Si el usuario hace clic igual, podemos mostrar un mensaje en el futuro.
            // Por ahora solo ignoramos el clic (el botón visual ya comunica "no listo").
        }
    }

} // namespace mixcoach

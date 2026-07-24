#include "PhaseProgressBar.h"
#include <cmath>

namespace mixcoach {

    /** Ease-out quad: suave, aceleración decreciente. */
    static float easeOutQuad(float t) noexcept
    {
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    PhaseProgressBar::PhaseProgressBar()
    {
        setOpaque(false);
        setBufferedToImage(true);
        initDots();
        startTimerHz(60);

        // Estado inicial: SmoothValues en 0 (no iniciado)
        for (auto& dot : dots_)
            dot.progress.reset(0.0f);

        // Marcar tiempo de creación para grace period de celebraciones
        creationTimeMs_ = (int64_t)juce::Time::getMillisecondCounter();
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void PhaseProgressBar::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(60);
        }
        // Nunca detener el timer — las animaciones de progreso y bursts
        // deben continuar aunque el componente no sea visible momentáneamente.
    }

    void PhaseProgressBar::initDots()
    {
        struct DotInfo {
            MentorPhase phase;
            const char* label;
            const char* icon;
        };

        static const DotInfo kPhaseMeta[] = {
            { MentorPhase::Organizacion, "SETUP",   "S" },
            { MentorPhase::GainStaging,  "GAIN",    "G" },
            { MentorPhase::Balance,      "BALANCE", "B" },
            { MentorPhase::EQ,           "EQ",      "E" },
            { MentorPhase::Compresion,   "COMP",    "C" },
            { MentorPhase::Espacio,      "SPACE",   "P" },
            { MentorPhase::MasterCheck,  "MASTER",  "M" },
        };

        dots_.clear();
        for (int i = 0; i < 7; ++i) {
            PhaseDot dot;
            dot.phase    = kPhaseMeta[i].phase;
            dot.label    = kPhaseMeta[i].label;
            dot.icon     = kPhaseMeta[i].icon;
            dot.completed = false;
            dot.isCurrent = false;
            dot.progress  = SmoothValue(0.0f, 40.0f, 200.0f);
            dot.displayRadius = 6.0f;
            dots_.push_back(std::move(dot));
        }

        // ═══ Inicializar DotBursts (1 por dot, todos inactivos) ══════════
        dotBursts_.resize(dots_.size());
        for (auto& b : dotBursts_)
            b.active = false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  triggerDotBurst — Dispara animación 1.5× → checkmark → ripple
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::triggerDotBurst(int dotIndex)
    {
        if (dotIndex < 0 || dotIndex >= (int)dotBursts_.size()) return;

        auto& burst = dotBursts_[dotIndex];
        burst.active = true;
        burst.startTimeMs = (int)juce::Time::getMillisecondCounter();
        burst.scale = 1.0f;
        burst.glowRadius = 0.0f;
        burst.glowAlpha = 0.0f;

        // También disparar wave de propagación desde este dot
        wavePulse_.active = true;
        wavePulse_.startTimeMs = (int)juce::Time::getMillisecondCounter();
        wavePulse_.originIndex = dotIndex;
        wavePulse_.waveRadius = 0.0f;
        wavePulse_.waveAlpha = 0.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateFromPhaseManager
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::updateFromPhaseManager(const PhaseManager& pm)
    {
        auto currentPhase = pm.getCurrentPhase();

        // ─── Detectar transiciones completadas para XP burst + dot burst ═══
        // Usar grace period en vez de firstUpdateDone: durante los primeros
        // kGracePeriodMs tras la construcción, se suprimen celebraciones
        // para evitar falsos positivos durante carga inicial de datos.
        // Pasado ese periodo, las celebraciones funcionan normalmente,
        // incluso si el usuario reabre el plugin con fases ya completadas.
        bool graceElapsed = ((int64_t)juce::Time::getMillisecondCounter() - creationTimeMs_) >= kGracePeriodMs;

        int dotIdx = 0;
        for (auto& dot : dots_) {
            bool wasComplete = graceElapsed ? dot.completed : true;
            dot.completed = pm.isPhaseComplete(dot.phase);
            dot.isCurrent = (dot.phase == currentPhase);

            if (dot.completed && !wasComplete) {
                // ¡Esta fase acaba de completarse! XP burst + dot burst + wave
                xpBurst_.active = true;
                xpBurst_.startTimeMs = (int)juce::Time::getMillisecondCounter();
                xpBurst_.xpAmount += 120;  // 100 por fase + 20 extra
                triggerDotBurst(dotIdx);
            }

            ++dotIdx;

            if (dot.completed) {
                dot.progress.setTargetValue(1.0f);
            } else if (dot.isCurrent) {
                dot.progress.setTargetValue(0.5f);
            } else {
                dot.progress.setTargetValue(0.0f);
            }
        }

        // ─── XP: 100 por fase completada + 50 por achievement ───────────
        int phasesDone = 0;
        for (const auto& dot : dots_)
            if (dot.completed) phasesDone++;

        int xpFromPhases = phasesDone * 100;
        int xpFromAchievements = pm.getAchievementCount() * 50;
        targetXp_ = xpFromPhases + xpFromAchievements;
        xpValue_.setTargetValue((float)targetXp_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  triggerXpBurst — Dispara animación XP burst manualmente
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::triggerXpBurst(int xpAmount)
    {
        xpBurst_.active = true;
        xpBurst_.startTimeMs = (int)juce::Time::getMillisecondCounter();
        xpBurst_.xpAmount += xpAmount;

        // También incrementar el XP target para que el contador suba
        targetXp_ += xpAmount;
        xpValue_.setTargetValue((float)targetXp_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Advance SmoothValue animations + burst + wave
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::timerCallback()
    {
        bool changed = false;
        const int nowMs = (int)juce::Time::getMillisecondCounter();

        for (auto& dot : dots_) {
            if (dot.progress.advance(60.0))
                changed = true;

            // Pulse animation for current phase dot
            if (dot.isCurrent) {
                float pulse = 0.12f * std::sin((float)nowMs * 0.004f);
                dot.displayRadius = 7.0f + pulse;
                changed = true;
            } else {
                dot.displayRadius = 6.0f;
            }
        }

        if (xpValue_.advance(60.0))
            changed = true;

        // ═══ Avanzar animación XP burst ═══════════════════════════════════
        if (xpBurst_.active) {
            int elapsedBurst = nowMs - xpBurst_.startTimeMs;
            // Incluir delay en la duración total
            if (elapsedBurst > xpBurst_.kDurationMs + xpBurst_.kDelayMs) {
                xpBurst_.active = false;
                xpBurst_.xpAmount = 0;
            }
            changed = true;
        }

        // ═══ Avanzar DotBurst animations ═════════════════════════════════
        for (auto& burst : dotBursts_) {
            if (!burst.active) continue;
            int elapsed = nowMs - burst.startTimeMs;
            float t = juce::jmin(1.0f, (float)elapsed / (float)DotBurst::kDurationMs);

            // Phase 1 (0-0.35): Scale up to 1.5x + glow increasing
            if (t < 0.35f) {
                float p = t / 0.35f;  // 0 → 1
                burst.scale = 1.0f + 0.5f * p;  // 1.0 → 1.5
                burst.glowRadius = 2.0f + 28.0f * p;  // 2 → 30px
                burst.glowAlpha = 0.6f * p;  // 0 → 0.6
            }
            // Phase 2 (0.35-0.65): Scale back to 1.0, glow peaks then fades
            else if (t < 0.65f) {
                float p = (t - 0.35f) / 0.30f;  // 0 → 1
                burst.scale = 1.5f - 0.5f * p;  // 1.5 → 1.0
                burst.glowRadius = 30.0f + 20.0f * p;  // 30 → 50px
                burst.glowAlpha = 0.6f * (1.0f - p * 0.5f);  // 0.6 → 0.3
            }
            // Phase 3 (0.65-1.0): Fade out glow ring
            else {
                float p = (t - 0.65f) / 0.35f;  // 0 → 1
                burst.scale = 1.0f;
                burst.glowRadius = 50.0f + 30.0f * p;  // 50 → 80px
                burst.glowAlpha = 0.3f * (1.0f - p);  // 0.3 → 0
            }

            if (t >= 1.0f) {
                burst.active = false;
                burst.scale = 1.0f;
                burst.glowRadius = 0.0f;
                burst.glowAlpha = 0.0f;
            }
            changed = true;
        }

        // ═══ Avanzar WavePulse (propagación) ═════════════════════════════
        if (wavePulse_.active) {
            int elapsed = nowMs - wavePulse_.startTimeMs;
            float t = juce::jmin(1.0f, (float)elapsed / (float)WavePulse::kDurationMs);
            wavePulse_.waveRadius = t;  // 0 → 1
            // Alpha: 0 → 0.5 → 0 (peak at 40%)
            if (t < 0.4f)
                wavePulse_.waveAlpha = 0.5f * (t / 0.4f);
            else
                wavePulse_.waveAlpha = 0.5f * (1.0f - (t - 0.4f) / 0.6f);
            wavePulse_.waveAlpha = juce::jmax(0.0f, wavePulse_.waveAlpha);

            if (t >= 1.0f) {
                wavePulse_.active = false;
            }
            changed = true;
        }

        if (changed)
            repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::resized()
    {
        auto area = getLocalBounds();

        // ─── XP label: right side, ~120px ──────────────────────────────────
        xpLabelBounds_ = area.removeFromRight(120);

        // ─── Dots area: remaining space ────────────────────────────────────
        auto dotsArea = area.reduced(12, 0);
        int nDots = (int)dots_.size();
        if (nDots < 2) return;

        int spacing = dotsArea.getWidth() / (nDots - 1);
        int centreY = dotsArea.getCentreY();

        dotLayouts_.clear();
        for (int i = 0; i < nDots; ++i) {
            int x = dotsArea.getX() + i * spacing;
            DotLayout layout;
            layout.bounds = juce::Rectangle<float>(
                (float)(x - 16), (float)(centreY - 14),
                32.0f, 28.0f
            );
            if (i < nDots - 1) {
                layout.lineTo = {
                    (float)(x + spacing),
                    (float)centreY
                };
            } else {
                layout.lineTo = { (float)x, (float)centreY };
            }
            dotLayouts_.push_back(layout);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // ─── Background (subtle dark strip) ──────────────────────────────
        g.setColour(MixCoachTheme::bgDark().darker(0.94f));
        g.fillRect(bounds);

        // Top highlight
        g.setColour(juce::Colours::white.withAlpha(0.02f));
        g.fillRect(bounds.withHeight(1.0f));

        // Bottom border
        g.setColour(MixCoachTheme::divider().withAlpha(0.12f));
        g.fillRect(bounds.removeFromBottom(1.0f));

        // ═══ Wave propagation (ripple) — dibujar ANTES de los dots ═══════
        drawWavePropagation(g);

        // ─── Connecting line ─────────────────────────────────────────────
        drawConnectingLine(g);

        // ─── Draw each dot ───────────────────────────────────────────────
        for (int i = 0; i < (int)dots_.size() && i < (int)dotLayouts_.size(); ++i)
            drawDot(g, i, dotLayouts_[i]);

        // ─── XP counter ──────────────────────────────────────────────────
        drawXpCounter(g, xpLabelBounds_);

        // ═══ XP burst animation (celebración — mejorada) ═══════════════
        // Estilo HTML: badge "⚡ +N XP" que hace pop-in con scale bounce,
        // flota hacia arriba, y se desvanece. Tiene glow y sombra.
        if (xpBurst_.active) {
            int burstElapsed = (int)juce::Time::getMillisecondCounter() - xpBurst_.startTimeMs;

            // Retardo inicial (300ms) para que el dot burst termine primero
            float t = 0.0f;
            if (burstElapsed > xpBurst_.kDelayMs) {
                float adjusted = (float)(burstElapsed - xpBurst_.kDelayMs);
                t = juce::jmin(1.0f, adjusted / (float)XpBurst::kDurationMs);
            }

            if (t > 0.0f) {
                // ─── Scale bounce-in (pop effect): 0 → 1.3 → 1.0 en primeros 35% ──
                float scale;
                if (t < 0.15f) {
                    // Pop-in rápido: 0 → 1.3 con ease-out
                    float p = t / 0.15f;
                    scale = 1.3f * easeOutQuad(p);
                } else if (t < 0.35f) {
                    // Overshoot settle: 1.3 → 1.0
                    float p = (t - 0.15f) / 0.20f;
                    scale = 1.3f - 0.3f * easeOutQuad(p);
                } else {
                    scale = 1.0f;
                }

                // ─── Float up: translateY -40px over full duration ──────────
                float floatOffset = t * 40.0f;

                // ─── Fade: hold full alpha until 40%, then fade to 0 ────────
                float alpha;
                if (t < 0.40f)
                    alpha = 1.0f;
                else
                    alpha = 1.0f - (t - 0.40f) / 0.60f;
                alpha = juce::jmax(0.0f, alpha);

                // ─── Position: centered above the XP counter pill ───────────
                float pillW = 86.0f;
                float pillH = 22.0f;
                float pillX = xpLabelBounds_.toFloat().getCentreX() - pillW * 0.5f;
                float pillY = xpLabelBounds_.toFloat().getY() - 8.0f - floatOffset - pillH;
                auto pillRect = juce::Rectangle<float>(pillX, pillY, pillW, pillH);

                // ─── Outer glow (behind the pill) ───────────────────────────
                g.setColour(MixCoachTheme::warning().withAlpha(0.08f * alpha));
                g.fillRoundedRectangle(pillRect.expanded(6.0f, 4.0f), 13.0f);

                // ─── Shadow ─────────────────────────────────────────────────
                g.setColour(juce::Colours::black.withAlpha(0.25f * alpha));
                g.fillRoundedRectangle(pillRect.translated(0, 2.0f), pillH * 0.5f);

                // ─── Save state for scale transform ────────────────────────
                g.saveState();
                float cx = pillRect.getCentreX();
                float cy = pillRect.getCentreY();
                g.addTransform(juce::AffineTransform::scale(scale, scale, cx, cy));

                // ─── Background pill with gradient ──────────────────────────
                {
                    juce::ColourGradient pillGrad(
                        MixCoachTheme::warning().withAlpha(0.20f * alpha),
                        pillRect.getX(), pillRect.getY(),
                        MixCoachTheme::warning().withAlpha(0.08f * alpha),
                        pillRect.getX(), pillRect.getBottom(), false);
                    g.setGradientFill(pillGrad);
                    g.fillRoundedRectangle(pillRect, pillH * 0.5f);
                }

                // ─── Border glow ────────────────────────────────────────────
                g.setColour(MixCoachTheme::warning().withAlpha(0.40f * alpha));
                g.drawRoundedRectangle(pillRect, pillH * 0.5f, 1.0f);

                // ─── Inner highlight ────────────────────────────────────────
                auto highlightRect = pillRect.withHeight(pillRect.getHeight() * 0.4f);
                juce::ColourGradient hlGrad(
                    juce::Colours::white.withAlpha(0.15f * alpha),
                    highlightRect.getX(), highlightRect.getY(),
                    juce::Colour(0x00000000),
                    highlightRect.getX(), highlightRect.getBottom(), false);
                g.setGradientFill(hlGrad);
                g.fillRoundedRectangle(highlightRect, pillH * 0.5f);

                // ─── Text: ⚡ +N XP ───────────────────────────────────────
                g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
                g.setColour(MixCoachTheme::warning().withAlpha(0.95f * alpha));
                g.drawText("+" + juce::String(xpBurst_.xpAmount) + " XP",
                           pillRect.toNearestInt(), juce::Justification::centred);

                g.restoreState();
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawConnectingLine
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::drawConnectingLine(juce::Graphics& g)
    {
        if (dotLayouts_.size() < 2) return;

        // Calculamos cuántos dots están completados
        int completedCount = 0;
        for (const auto& dot : dots_)
            if (dot.progress.getCurrent() > 0.75f) completedCount++;

        int nLayouts = (int)dotLayouts_.size();

        // ─── Base line (dim) ────────────────────────────────────────────
        juce::Path basePath;
        float firstY = dotLayouts_[0].bounds.getCentreY();
        basePath.startNewSubPath(dotLayouts_[0].bounds.getCentreX(), firstY);
        for (int i = 1; i < nLayouts; ++i)
            basePath.lineTo(dotLayouts_[i].bounds.getCentreX(), dotLayouts_[i].bounds.getCentreY());

        g.setColour(MixCoachTheme::divider().withAlpha(0.18f));
        g.strokePath(basePath, juce::PathStrokeType(2.0f));

        // ─── Fill line (completed portion) ──────────────────────────────
        if (completedCount > 0 && completedCount < nLayouts) {
            juce::Path fillPath;
            fillPath.startNewSubPath(dotLayouts_[0].bounds.getCentreX(), firstY);
            for (int i = 1; i <= completedCount && i < nLayouts; ++i)
                fillPath.lineTo(dotLayouts_[i].bounds.getCentreX(), dotLayouts_[i].bounds.getCentreY());

            juce::ColourGradient lineGrad(
                MixCoachTheme::accent().withAlpha(0.6f),
                dotLayouts_[0].bounds.getCentreX(), firstY,
                MixCoachTheme::success().withAlpha(0.6f),
                dotLayouts_[completedCount].bounds.getCentreX(),
                dotLayouts_[completedCount].bounds.getCentreY(),
                false);
            g.setGradientFill(lineGrad);
            g.strokePath(fillPath, juce::PathStrokeType(2.0f));
        } else if (completedCount >= nLayouts) {
            g.setColour(MixCoachTheme::success().withAlpha(0.6f));
            g.strokePath(basePath, juce::PathStrokeType(2.0f));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawWavePropagation — Ripple expansivo desde el dot completado
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::drawWavePropagation(juce::Graphics& g)
    {
        if (!wavePulse_.active || wavePulse_.originIndex < 0) return;
        if (dotLayouts_.size() < 2) return;

        float t = wavePulse_.waveRadius;  // 0 → 1
        float alpha = wavePulse_.waveAlpha;
        if (alpha <= 0.01f) return;

        // Calcular la distancia máxima en píxeles que ha recorrido la onda
        // desde el dot origen hasta el punto más lejano alcanzado
        int originIdx = wavePulse_.originIndex;
        float maxDistPx = 0.0f;
        for (size_t i = 0; i < dotLayouts_.size(); ++i) {
            float dist = std::abs((float)((int)i - originIdx));
            if (dist > maxDistPx)
                maxDistPx = dist;
        }
        maxDistPx *= dotLayouts_[1].bounds.getCentreX() - dotLayouts_[0].bounds.getCentreX();
        if (maxDistPx < 1.0f) return;

        float currentRadius = t * maxDistPx;  // Anillo actual en px
        float cx = dotLayouts_[originIdx].bounds.getCentreX();
        float cy = dotLayouts_[originIdx].bounds.getCentreY();

        // Dibujar el anillo de propagación como un círculo semitransparente
        // que se expande desde el dot origen
        juce::Colour waveCol = MixCoachTheme::accent().withAlpha(alpha * 0.25f);
        g.setColour(waveCol);
        g.drawEllipse(cx - currentRadius, cy - currentRadius,
                      currentRadius * 2.0f, currentRadius * 2.0f, 2.0f);

        // Segundo anillo interior
        float innerR = currentRadius * 0.7f;
        juce::Colour innerCol = MixCoachTheme::accent().withAlpha(alpha * 0.12f);
        g.setColour(innerCol);
        g.drawEllipse(cx - innerR, cy - innerR,
                      innerR * 2.0f, innerR * 2.0f, 1.0f);

        // Glow fill
        juce::Colour fillCol = MixCoachTheme::success().withAlpha(alpha * 0.06f);
        g.setColour(fillCol);
        g.fillEllipse(cx - currentRadius, cy - currentRadius,
                      currentRadius * 2.0f, currentRadius * 2.0f);

        // ─── Iluminar los dots vecinos cuando la onda los alcanza ────────
        for (size_t i = 0; i < dotLayouts_.size(); ++i) {
            if ((int)i == originIdx) continue;
            float dist = std::abs(dotLayouts_[i].bounds.getCentreX() - cx);
            // Cuando la onda pasa sobre un dot vecino, iluminarlo suavemente
            float proximity = 1.0f - std::abs(dist - currentRadius) / (maxDistPx * 0.15f);
            if (proximity > 0.0f && proximity < 1.0f) {
                float dotGlow = proximity * alpha * 0.4f;
                float dx = dotLayouts_[i].bounds.getCentreX();
                float dy = dotLayouts_[i].bounds.getCentreY();
                g.setColour(MixCoachTheme::accent().withAlpha(dotGlow));
                g.fillEllipse(dx - 10.0f, dy - 10.0f, 20.0f, 20.0f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawDot — Incluye burst glow ring animado en fase completada
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::drawDot(juce::Graphics& g, int index, const DotLayout& layout)
    {
        if (index < 0 || index >= (int)dots_.size()) return;

        const auto& dot = dots_[index];
        float cx = layout.bounds.getCentreX();
        float cy = layout.bounds.getCentreY();
        float progress = dot.progress.getCurrent(); // 0=gray, 0.5=accent, 1.0=green
        float radius = dot.displayRadius;
        bool isComplete = (progress > 0.75f);
        bool isActive = (progress > 0.25f && progress <= 0.75f);

        // ═══ Burst glow ring (animación de completado) ═══════════════════
        if (index < (int)dotBursts_.size() && dotBursts_[index].active) {
            const auto& burst = dotBursts_[index];
            // Anillo expansivo con fade-out
            g.setColour(MixCoachTheme::success().withAlpha(burst.glowAlpha * 0.7f));
            float ringR = radius + burst.glowRadius;
            g.drawEllipse(cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, 2.0f);
            // Segundo anillo más sutil
            g.setColour(MixCoachTheme::success().withAlpha(burst.glowAlpha * 0.3f));
            float ringR2 = ringR + 8.0f;
            g.drawEllipse(cx - ringR2, cy - ringR2, ringR2 * 2.0f, ringR2 * 2.0f, 1.5f);
            // Glow fill interior (expansivo)
            g.setColour(MixCoachTheme::success().withAlpha(burst.glowAlpha * 0.15f));
            g.fillEllipse(cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);
        }

        // ─── Glow ring for active phase ──────────────────────────────────
        if (isActive) {
            float pulse = 0.20f + 0.12f * std::sin(juce::Time::getMillisecondCounter() * 0.005f + (float)index * 0.3f);
            g.setColour(MixCoachTheme::accent().withAlpha(pulse));
            g.fillEllipse(cx - radius - 5.0f, cy - radius - 5.0f,
                          (radius + 5.0f) * 2.0f, (radius + 5.0f) * 2.0f);
        }

        // ─── Dot circle ──────────────────────────────────────────────────
        if (isComplete) {
            // Completed: green filled
            g.setColour(MixCoachTheme::success().withAlpha(0.85f));
            g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);
            // Checkmark
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(juce::Colours::white);
            g.drawText("V",
                       juce::Rectangle<float>(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f)
                           .toNearestInt(),
                       juce::Justification::centred);
        } else if (isActive) {
            // Active: accent filled (pulsing)
            float actualR = radius + 1.0f;
            g.setColour(MixCoachTheme::accent().withAlpha(0.90f));
            g.fillEllipse(cx - actualR, cy - actualR, actualR * 2.0f, actualR * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawEllipse(cx - actualR, cy - actualR, actualR * 2.0f, actualR * 2.0f, 1.5f);
        } else {
            // Future: hollow gray
            g.setColour(MixCoachTheme::divider().withAlpha(0.30f));
            g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);
            // Subtle fill
            g.setColour(MixCoachTheme::divider().withAlpha(0.04f));
            g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        }

        // ─── Phase icon above dot ─────────────────────────────────────────
        float iconY = cy - radius - 14.0f;
        auto iconArea = juce::Rectangle<float>(cx - 10.0f, iconY, 20.0f, 12.0f);
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        juce::Colour iconColour;
        if (isComplete)       iconColour = MixCoachTheme::success().withAlpha(0.85f);
        else if (isActive)    iconColour = MixCoachTheme::accentGlow();
        else                  iconColour = MixCoachTheme::textMuted().withAlpha(0.35f);
        g.setColour(iconColour);
        g.drawText(juce::String(dot.icon), iconArea.toNearestInt(), juce::Justification::centred);

        // ─── Phase label below dot ────────────────────────────────────────
        float labelY = cy + radius + 3.0f;
        auto labelArea = juce::Rectangle<float>(cx - 24.0f, labelY, 48.0f, 10.0f);
        g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
        juce::Colour textColour;
        if (isComplete)       textColour = MixCoachTheme::success().withAlpha(0.75f);
        else if (isActive)    textColour = MixCoachTheme::accent().withAlpha(0.85f);
        else                  textColour = MixCoachTheme::textMuted().withAlpha(0.30f);
        g.setColour(textColour);
        g.drawText(juce::String(dot.label), labelArea.toNearestInt(), juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawXpCounter
    // ═══════════════════════════════════════════════════════════════════════════
    void PhaseProgressBar::drawXpCounter(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        if (bounds.isEmpty()) return;

        auto area = bounds.toFloat();

        // ─── Counter background pill ───────────────────────────────────────
        float xpW = 72.0f;
        float xpH = 18.0f;
        float pillX = area.getRight() - xpW - 8.0f;
        float pillY = area.getCentreY() - xpH * 0.5f;
        auto pillRect = juce::Rectangle<float>(pillX, pillY, xpW, xpH);

        g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
        g.fillRoundedRectangle(pillRect, 9.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.drawRoundedRectangle(pillRect, 9.0f, 0.5f);

        // ─── Star icon ────────────────────────────────────────────────────
        auto starArea = juce::Rectangle<float>(pillX + 4.0f, pillY, 18.0f, xpH);
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(MixCoachTheme::warning().withAlpha(0.85f));
        g.drawText("\xE2\xAD\x90", starArea.toNearestInt(), juce::Justification::centred);

        // ─── XP number ─────────────────────────────────────────────────────
        int xp = (int)xpValue_.getCurrent();
        auto xpTextArea = juce::Rectangle<float>(pillX + 18.0f, pillY, xpW - 22.0f, xpH);
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::textBright().withAlpha(0.90f));
        g.drawText(juce::String(xp) + " XP", xpTextArea.toNearestInt(), juce::Justification::centred);
    }

} // namespace mixcoach

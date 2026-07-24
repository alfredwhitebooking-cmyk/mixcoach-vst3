#include "SessionScanCard.h"
#include <cmath>

namespace mixcoach {

    SessionScanCard::SessionScanCard()
    {
        setOpaque(false);
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void SessionScanCard::visibilityChanged()
    {
        if (isShowing()) {
            startTimerHz(60);
        } else {
            stopTimer();
        }
    }

    void SessionScanCard::startScan(int totalTracks, const std::vector<juce::String>& trackNames)
    {
        totalTracks_ = totalTracks;
        trackNames_ = trackNames;
        currentProgress_ = 0.0f;
        targetProgress_ = 1.0f; // Queremos que llegue al final
        currentTrackIndex_ = 0;
        currentStatus_ = "Identificando instrumentos...";
        
        startTimeMs_ = juce::Time::getMillisecondCounter();
        startTimerHz(60); // 60fps para fluidez
        setVisible(true);
    }

    void SessionScanCard::setProgress(float progress)
    {
        targetProgress_ = juce::jlimit(0.0f, 1.0f, progress);
    }

    void SessionScanCard::setStatusMessage(const juce::String& status)
    {
        currentStatus_ = status;
        repaint();
    }

    void SessionScanCard::timerCallback()
    {
        bool needsRepaint = false;
        
        // ─── Avanzar barra de progreso suavemente ──────────────────────────
        if (currentProgress_ < targetProgress_) {
            float delta = (targetProgress_ - currentProgress_) * 0.05f;
            if (delta < 0.001f) delta = 0.001f;
            currentProgress_ += delta;
            if (currentProgress_ >= targetProgress_) {
                currentProgress_ = targetProgress_;
            }
            needsRepaint = true;
        }

        // ─── Animación de nombres de pistas flickering ──────────────────────
        if (currentProgress_ < 1.0f && !trackNames_.empty()) {
            // Cambiar nombre mostrado cada 3-4 frames para efecto de velocidad
            static int frameCounter = 0;
            if (++frameCounter % 4 == 0) {
                currentTrackIndex_ = (currentTrackIndex_ + 1) % (int)trackNames_.size();
                flickeringTrackName_ = trackNames_[currentTrackIndex_];
                needsRepaint = true;
            }
        }

        // ─── Check completion ──────────────────────────────────────────────
        if (currentProgress_ >= 1.0f) {
            static bool completionTriggered = false;
            if (!completionTriggered) {
                completionTriggered = true;
                currentStatus_ = "¡Escaneo completado!";
                flickeringTrackName_ = "";
                if (onScanComplete) onScanComplete();
                stopTimer();
            }
        }

        if (needsRepaint) repaint();
    }

    void SessionScanCard::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float cr = 10.0f;

        // ─── Fondo Glass (según SceneVisualSpec) ──────────────────────────
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.85f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, cr, 1.0f);

        auto area = bounds.reduced(16.0f, 12.0f);

        // ─── Header: "ESCANEANDO SESIÓN" ───────────────────────────────────
        auto header = area.removeFromTop(20);
        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(MixCoachTheme::textDim());
        g.drawText("ANALIZANDO ESTRUCTURA...", header, juce::Justification::centredLeft);

        // Badge de cantidad de pistas
        auto badge = header.removeFromRight(80);
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(MixCoachTheme::accent());
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.drawText(juce::String(totalTracks_) + " PISTAS", badge, juce::Justification::centred);

        area.removeFromTop(8);

        // ─── Nivel actual / Nombre de pista flickery ───────────────────────
        auto trackArea = area.removeFromTop(24);
        if (currentProgress_ < 1.0f) {
            g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
            g.setColour(MixCoachTheme::accentCyan());
            g.drawText("➜ " + flickeringTrackName_, trackArea, juce::Justification::centredLeft);
        } else {
            g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
            g.setColour(MixCoachTheme::success());
            g.drawText("✓ Todas las pistas identificadas", trackArea, juce::Justification::centredLeft);
        }

        area.removeFromTop(6);

        // ─── Barra de progreso ─────────────────────────────────────────────
        auto barArea = area.removeFromTop(8);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(barArea, 4.0f);

        auto fillArea = barArea.withWidth(barArea.getWidth() * currentProgress_);
        juce::ColourGradient grad(MixCoachTheme::accent(), barArea.getX(), 0,
                                  MixCoachTheme::accentCyan(), barArea.getRight(), 0, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillArea, 4.0f);

        // Glow sutil en la punta de la barra
        if (currentProgress_ > 0 && currentProgress_ < 1.0f) {
            float tipX = fillArea.getRight();
            g.setColour(MixCoachTheme::accentCyan().withAlpha(0.4f));
            g.fillEllipse(tipX - 4, barArea.getCentreY() - 4, 8, 8);
        }

        area.removeFromTop(8);

        // ─── Texto de estado inferior ──────────────────────────────────────
        auto statusArea = area;
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText(currentStatus_, statusArea, juce::Justification::centredLeft);

        // Porcentaje a la derecha
        int pct = (int)(currentProgress_ * 100);
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::String(pct) + "%", statusArea, juce::Justification::centredRight);
    }

    void SessionScanCard::resized()
    {
    }

} // namespace mixcoach

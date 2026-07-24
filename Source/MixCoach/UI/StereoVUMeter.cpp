#include "StereoVUMeter.h"

namespace mixcoach {

    StereoVUMeter::StereoVUMeter()
    {
        titleLabel_.setText(juce::CharPointer_UTF8("[TREND] LEVEL METERS (RMS / PEAK)"),
                            juce::dontSendNotification);
        titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        titleLabel_.setJustificationType(juce::Justification::centredLeft);
        titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textAccent()); // Lavanda premium
        addAndMakeVisible(titleLabel_);

        leftLabel_.setText("L: --.- dB", juce::dontSendNotification);
        leftLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
        leftLabel_.setJustificationType(juce::Justification::centred);
        leftLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
        addAndMakeVisible(leftLabel_);

        rightLabel_.setText("R: --.- dB", juce::dontSendNotification);
        rightLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
        rightLabel_.setJustificationType(juce::Justification::centred);
        rightLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
        addAndMakeVisible(rightLabel_);
    }

    void StereoVUMeter::resized()
    {
        auto area = getLocalBounds().reduced(4);
        titleLabel_.setBounds(area.removeFromTop(18));

        // Dejar espacio inferior para las lecturas numéricas de precisión
        auto footerArea = area.removeFromBottom(16);
        auto halfWidth  = footerArea.getWidth() / 2;
        leftLabel_.setBounds(footerArea.removeFromLeft(halfWidth));
        rightLabel_.setBounds(footerArea);
    }

    void StereoVUMeter::setLevels(float leftRMS, float rightRMS, float leftPeak, float rightPeak)
    {
        leftRMS_.setTarget(leftRMS);
        rightRMS_.setTarget(rightRMS);
        leftPeak_.setTarget(leftPeak);
        rightPeak_.setTarget(rightPeak);

        // Ballistics avanzadas para retención de picos (Hold & Decay)
        if (leftPeak > leftPeakHold_) {
            leftPeakHold_  = leftPeak;
            leftHoldTimer_ = 45; // ~750ms a 60fps antes de caer
        }
        else if (leftHoldTimer_ > 0) {
            leftHoldTimer_--;
        }
        else {
            leftPeakHold_ -= 0.6f; // Caída lineal suave en dB por frame
            if (leftPeakHold_ < -60.0f) leftPeakHold_ = -60.0f;
        }

        if (rightPeak > rightPeakHold_) {
            rightPeakHold_  = rightPeak;
            rightHoldTimer_ = 45;
        }
        else if (rightHoldTimer_ > 0) {
            rightHoldTimer_--;
        }
        else {
            rightPeakHold_ -= 0.6f;
            if (rightPeakHold_ < -60.0f) rightPeakHold_ = -60.0f;
        }

        // Actualización de texto con formateo limpio de un solo decimal
        leftLabel_.setText("L: " + (leftPeak >= 0.0f ? juce::String("+") : "") + juce::String(leftPeak, 1) + " dB",
                           juce::dontSendNotification);
        rightLabel_.setText("R: " + (rightPeak >= 0.0f ? juce::String("+") : "") + juce::String(rightPeak, 1) + " dB",
                            juce::dontSendNotification);

        // Cambiar color de la etiqueta si hay clipping (Saturación)
        leftLabel_.setColour(juce::Label::textColourId,
                             leftPeak >= 0.0f ? MixCoachTheme::error() : MixCoachTheme::textDim());
        rightLabel_.setColour(juce::Label::textColourId,
                              rightPeak >= 0.0f ? MixCoachTheme::error() : MixCoachTheme::textDim());

        repaint();
    }

    void StereoVUMeter::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

        auto area = getLocalBounds().reduced(6);
        area.removeFromTop(22);    // Espacio para el título
        area.removeFromBottom(16); // Espacio para los labels inferiores

        auto content    = area.reduced(4, 0);
        float halfWidth = (float)content.getWidth() * 0.5f;

        // Renderizado simétrico de los canales Izquierdo y Derecho
        drawChannelMeter(g,
                         content.removeFromLeft((int)halfWidth).toFloat().reduced(4, 0),
                         leftRMS_.getCurrent(),
                         leftPeak_.getCurrent(),
                         leftPeakHold_,
                         true); // Is Left
        drawChannelMeter(g,
                         content.removeFromLeft((int)halfWidth).toFloat().reduced(4, 0),
                         rightRMS_.getCurrent(),
                         rightPeak_.getCurrent(),
                         rightPeakHold_,
                         false); // Is Right
    }

    void StereoVUMeter::drawChannelMeter(
        juce::Graphics& g, juce::Rectangle<float> bounds, float rms, float peak, float peakHold, bool isLeftChannel)
    {
        // Fondo del canal (Riel contenedor)
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(bounds, 2.0f);

        // Reducir márgenes internos para las marcas de escala y las barras activas
        auto meterBar = bounds.reduced(1.0f, 2.0f);

        // Escala Máster de Referencia (+6 a -60 dB)
        constexpr float kScaleVals[] = {
            6.0f, 0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -40.0f, -50.0f, -60.0f};
        constexpr int kNumScale = 11;

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));

        for (int i = 0; i < kNumScale; ++i) {
            float glNorm = juce::jlimit(0.0f, 1.0f, (kScaleVals[i] + 60.0f) / 66.0f);
            float glY    = meterBar.getBottom() - meterBar.getHeight() * glNorm;

            // Líneas de subdivisión transparentes
            g.setColour(MixCoachTheme::rowDivider().withAlpha(kScaleVals[i] == 0.0f ? 0.6f : 0.25f));
            g.drawHorizontalLine((int)glY, meterBar.getX(), meterBar.getRight());

            // Números laterales integrados discretamente dentro del canal
            g.setColour(kScaleVals[i] > 0.0f ? MixCoachTheme::error().withAlpha(0.5f)
                                             : MixCoachTheme::textMuted().withAlpha(0.4f));
            float textX = isLeftChannel ? meterBar.getX() + 3.0f : meterBar.getRight() - 17.0f;
            g.drawText(juce::String((int)kScaleVals[i]),
                       juce::Rectangle<float>(textX, glY - 4.5f, 14.0f, 9.0f),
                       isLeftChannel ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }

        // --- 1. Renderizado de la barra de nivel RMS ---
        float rmsNorm = juce::jlimit(0.0f, 1.0f, (rms + 60.0f) / 66.0f);
        if (rmsNorm > 0.005f) {
            auto rmsBounds = meterBar.withTop(meterBar.getBottom() - meterBar.getHeight() * rmsNorm);

            // El gradiente está anclado a las dimensiones fijas del canal completo
            juce::ColourGradient barGrad(MixCoachTheme::meterGreen(),
                                         meterBar.getCentreX(),
                                         meterBar.getBottom(),
                                         MixCoachTheme::meterRed(),
                                         meterBar.getCentreX(),
                                         meterBar.getY(),
                                         false);

            barGrad.addColour(0.55f, MixCoachTheme::meterLime());
            barGrad.addColour(0.75f, MixCoachTheme::meterYellow());
            barGrad.addColour(0.88f, MixCoachTheme::meterOrange());

            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(rmsBounds, 1.5f);
        }

        // --- 2. Renderizado de la barra de respuesta Peak (Superposición translúcida) ---
        float peakNorm = juce::jlimit(0.0f, 1.0f, (peak + 60.0f) / 66.0f);
        if (peakNorm > rmsNorm) {
            auto peakBounds = meterBar.withTop(meterBar.getBottom() - meterBar.getHeight() * peakNorm)
                                  .withBottom(meterBar.getBottom() - meterBar.getHeight() * rmsNorm);

            juce::ColourGradient peakGrad(MixCoachTheme::meterGreen(),
                                          meterBar.getCentreX(),
                                          meterBar.getBottom(),
                                          MixCoachTheme::meterRed(),
                                          meterBar.getCentreX(),
                                          meterBar.getY(),
                                          false);

            peakGrad.addColour(0.55f, MixCoachTheme::meterLime());
            peakGrad.addColour(0.75f, MixCoachTheme::meterYellow());
            peakGrad.addColour(0.88f, MixCoachTheme::meterOrange());

            g.setGradientFill(peakGrad);

            // Capa translúcida reactiva rápida estilo iZotope
            g.saveState();
            g.setOpacity(0.45f);
            g.fillRoundedRectangle(peakBounds, 1.5f);
            g.restoreState();
        }

        // --- 3. Triángulos de Pico de Hardware (Modern Peak Triangles ◀ / ▶) ---
        if (peak > -60.0f) {
            float pNorm = juce::jlimit(0.0f, 1.0f, (peak + 60.0f) / 66.0f);
            float peakY = meterBar.getBottom() - meterBar.getHeight() * pNorm;

            juce::Path tri;
            float triSize = 3.0f;

            if (isLeftChannel) {
                // Canal izquierdo: el triángulo se ubica a la izquierda y apunta a la barra
                float triLeft = bounds.getX() - 1.5f;
                tri.addTriangle(triLeft,
                                peakY,
                                triLeft - triSize,
                                peakY - triSize + 0.5f,
                                triLeft - triSize,
                                peakY + triSize - 0.5f);
            }
            else {
                // Canal derecho: el triángulo se ubica a la derecha y apunta a la barra
                float triRight = bounds.getRight() + 1.5f;
                tri.addTriangle(triRight,
                                peakY,
                                triRight + triSize,
                                peakY - triSize + 0.5f,
                                triRight + triSize,
                                peakY + triSize - 0.5f);
            }

            g.setColour(peak >= 0.0f ? MixCoachTheme::error() : juce::Colours::white.withAlpha(0.9f));
            g.fillPath(tri);
        }

        // --- 4. Línea de Retención de Pico (Peak Hold Line) ---
        if (peakHold > -60.0f) {
            float holdNorm = juce::jlimit(0.0f, 1.0f, (peakHold + 60.0f) / 66.0f);
            float holdY    = meterBar.getBottom() - meterBar.getHeight() * holdNorm;

            g.setColour(peakHold >= 0.0f ? MixCoachTheme::error() : juce::Colours::white.withAlpha(0.75f));
            g.fillRect(meterBar.getX(), holdY - 0.5f, meterBar.getWidth(), 1.5f);
        }
    }

} // namespace mixcoach

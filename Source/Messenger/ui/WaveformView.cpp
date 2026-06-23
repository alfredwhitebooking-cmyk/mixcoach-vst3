#include "WaveformView.h"
#include "../../MixCoach/UI/MixCoachTheme.h"

namespace mixcoach {

    WaveformView::WaveformView()
    {
        setOpaque(false);
        std::fill(waveData_.begin(), waveData_.end(), 0.0f);
    }

    void WaveformView::pushSample(float amplitude)
    {
        waveData_[writePos_] = juce::jlimit(-1.0f, 1.0f, amplitude);
        writePos_            = (writePos_ + 1) % kNumSamples;
        if (++samplesSinceUpdate_ > kDecimation) {
            repaint();
            samplesSinceUpdate_ = 0;
        }
    }

    void WaveformView::setWaveColour(juce::Colour col)
    {
        waveColour_ = col;
    }

    void WaveformView::setSignalPresent(bool hasSignal)
    {
        hasSignal_ = hasSignal;
    }

    void WaveformView::setRMS(float rms)
    {
        currentRMS_ = rms;
    }

    void WaveformView::paint(juce::Graphics& g)
    {
        auto bounds   = getLocalBounds().toFloat().reduced(1.0f);
        auto w        = bounds.getWidth();
        auto h        = bounds.getHeight();
        float centerY = bounds.getCentreY();

        // Fondo
        g.setColour(MixCoachTheme::bgInput());
        g.fillRoundedRectangle(bounds, 3.0f);

        // Dibujar waveform
        if (hasSignal_) {
            juce::Path wavePath;
            bool first = true;

            for (int i = 0; i < kNumSamples; ++i) {
                int idx         = (writePos_ + i) % kNumSamples;
                float x         = bounds.getX() + (float)i / (float)kNumSamples * w;
                float sampleVal = waveData_[idx];

                if (first) {
                    wavePath.startNewSubPath(x, centerY - sampleVal * (h * 0.4f));
                    first = false;
                }
                else {
                    wavePath.lineTo(x, centerY - sampleVal * (h * 0.4f));
                }
            }

            g.setColour(waveColour_.withAlpha(0.8f));
            g.strokePath(wavePath, juce::PathStrokeType(1.2f));

            // Relleno suave debajo de la forma de onda
            juce::Path fillPath = wavePath;
            fillPath.lineTo(bounds.getRight(), centerY);
            fillPath.lineTo(bounds.getX(), centerY);
            fillPath.closeSubPath();
            g.setColour(waveColour_.withAlpha(0.1f));
            g.fillPath(fillPath);

            // Brillo cuando hay señal
            if (juce::jmax(currentRMS_, 0.0f) > 0.01f) {
                g.setColour(waveColour_.withAlpha(0.05f));
                g.fillRoundedRectangle(bounds, 3.0f);
            }
        }
        else {
            // Sin señal — línea plana con opacidad baja
            g.setColour(waveColour_.withAlpha(0.2f));
            g.drawHorizontalLine((int)centerY, bounds.getX(), bounds.getRight());
        }

        // Borde
        g.setColour(MixCoachTheme::borderCard().withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    }

} // namespace mixcoach

#include "ReferenceOnboardingCard.h"

namespace mixcoach {

    ReferenceOnboardingCard::ReferenceOnboardingCard()
    {
        // ─── Header label ─────────────────────────────────────────────────────
        headerLabel_.setText("[REPORT] Cargar referencia", juce::dontSendNotification);
        headerLabel_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        addAndMakeVisible(headerLabel_);

        // ─── DropZoneComponent (reutilizado) ──────────────────────────────────
        dropZone_.onFileDropped = [this](const juce::String& path) {
            // Guardar la ruta completa para que NavigationShell pueda leerla
            filePath_ = path;
            url_.clear();

            // Extraer info básica del archivo
            juce::File f(path);
            fileName_ = f.getFileName();
            hasFile_  = true;
            isURL_    = false;

            // Intentar leer metadatos del archivo
            // ═══ BUG #3: Usar unique_ptr evita memory leak si hay excepcion ═══
            juce::AudioFormatManager fmtMgr;
            fmtMgr.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(fmtMgr.createReaderFor(f));
            if (reader != nullptr) {
                double dur       = reader->lengthInSamples / reader->sampleRate;
                int sr           = static_cast<int>(reader->sampleRate);
                int bits         = reader->bitsPerSample;
                int channels     = reader->numChannels;
                juce::String durStr;
                if (dur < 60.0)
                    durStr = juce::String(dur, 1) + "s";
                else
                    durStr = juce::String(static_cast<int>(dur / 60)) + "m "
                             + juce::String(static_cast<int>(dur) % 60) + "s";

                fileInfoText_ = juce::String(channels) + "ch "
                                + juce::String(sr / 1000) + "." + juce::String((sr % 1000) / 100) + " kHz "
                                + juce::String(bits) + "bit "
                                + durStr;
                // reader se destruye automaticamente aqui — no hay delete manual
            } else {
                fileInfoText_ = "Archivo de audio";
            }

            setFileInfo(fileName_, 0.0, 0, 0, false);
            if (onFileDropped) onFileDropped(path);
        };

        dropZone_.onURLAdded = [this](const juce::String& url) {
            filePath_.clear();
            url_      = url;
            fileName_ = url;
            hasFile_  = true;
            isURL_    = true;
            fileInfoText_ = "URL de referencia";
            setFileInfo(url, 0.0, 0, 0, true);
            if (onURLAdded) onURLAdded(url);
        };

        addAndMakeVisible(dropZone_);

        // ─── File Info Label ──────────────────────────────────────────────────
        fileInfoLabel_.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        fileInfoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
        fileInfoLabel_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(fileInfoLabel_);
        fileInfoLabel_.setVisible(false);

        // ─── Analyze Button ───────────────────────────────────────────────────
        analyzeButton_.setButtonText("Analizar referencia");
        analyzeButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.25f));
        analyzeButton_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent().withAlpha(0.40f));
        analyzeButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::accentGlow());
        analyzeButton_.setColour(juce::TextButton::textColourOnId, MixCoachTheme::accentGlow().brighter(0.3f));
        analyzeButton_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        analyzeButton_.onClick = [this]() {
            if (onAnalyzeClicked) onAnalyzeClicked();
        };
        addAndMakeVisible(analyzeButton_);
        analyzeButton_.setVisible(false);

        // ─── Clear Button ────────────────────────────────────────────────────
        clearButton_.setButtonText("✕");
        clearButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
        clearButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::textMuted());
        clearButton_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        clearButton_.onClick = [this]() { reset(); };
        addAndMakeVisible(clearButton_);
        clearButton_.setVisible(false);

        setSize(400, 160);
    }

    void ReferenceOnboardingCard::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float cr = 6.0f;

        // ─── Background (más prominente con header) ─────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Glass highlight sutil en la parte superior ─────────────────────
        auto glassH = bounds.withHeight(headerLabel_.getBottom() - bounds.getY());
        juce::ColourGradient glassGrad(MixCoachTheme::glassHighlight().withAlpha(0.03f),
                                       glassH.getX(), glassH.getY(),
                                       juce::Colours::transparentBlack,
                                       glassH.getX(), glassH.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassH, cr);

        // ─── Borde ──────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::border().withAlpha(0.30f));
        g.drawRoundedRectangle(bounds, cr, 0.8f);

        if (hasFile_) {
            // ─── Status badge (File/URL indicator) ────────────────────────────
            g.saveState();
            float badgeCx = bounds.getRight() - 22.0f;
            float badgeCy = bounds.getY() + 18.0f;
            juce::Colour badgeCol = isURL_ ? MixCoachTheme::accentCyan() : MixCoachTheme::success();
            g.setColour(badgeCol.withAlpha(0.15f));
            g.fillEllipse(badgeCx - 8.0f, badgeCy - 8.0f, 16.0f, 16.0f);
            g.setColour(badgeCol);
            g.drawEllipse(badgeCx - 8.0f, badgeCy - 8.0f, 16.0f, 16.0f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.drawText(isURL_ ? "URL" : "WAV",
                       juce::Rectangle<float>(badgeCx - 8.0f, badgeCy - 8.0f, 16.0f, 16.0f),
                       juce::Justification::centred);
            g.restoreState();
        }
    }

    void ReferenceOnboardingCard::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        if (!hasFile_) {
            // ─── Estado vacío: Header + DropZone dual ─────────────────────────
            auto headerArea = area.removeFromTop(20);
            headerLabel_.setBounds(headerArea.reduced(2, 1));
            headerLabel_.setVisible(true);

            area.removeFromTop(2);

            dropZone_.setBounds(area);
            dropZone_.setShowDropHint(true);

            fileInfoLabel_.setVisible(false);
            analyzeButton_.setVisible(false);
            clearButton_.setVisible(false);
        } else {
            // ─── Estado poblado: Header + DropZone compacto + info + botón ───
            headerLabel_.setVisible(false);

            auto topArea = area.removeFromTop(60);
            dropZone_.setBounds(topArea);
            dropZone_.setShowDropHint(false);

            area.removeFromTop(4);

            // File info row
            auto infoRow = area.removeFromTop(18);
            fileInfoLabel_.setBounds(infoRow.reduced(6, 0));
            fileInfoLabel_.setVisible(true);

            // Button row — centered
            auto btnRow = area.removeFromTop(24);
            float btnW = 160.0f;
            float clearW = 30.0f;
            float totalBtnW = btnW + 6.0f + clearW;
            float btnOffset = (btnRow.getWidth() - totalBtnW) * 0.5f;
            if (btnOffset < 0.0f) btnOffset = 0.0f;
            analyzeButton_.setBounds(btnRow.withX(btnRow.getX() + btnOffset).withWidth(btnW).reduced(0, 2));
            clearButton_.setBounds(btnRow.withX(btnRow.getX() + btnOffset + btnW + 6.0f).withWidth(clearW).reduced(0, 2));
            analyzeButton_.setVisible(true);
            clearButton_.setVisible(true);
        }
    }

    void ReferenceOnboardingCard::setFileInfo(const juce::String& fileName,
                                              double /*durationSecs*/,
                                              int /*sampleRate*/,
                                              int /*bitDepth*/,
                                              bool /*isURL*/)
    {
        fileName_ = fileName;
        hasFile_  = true;
        juce::String display = fileName_ + "  |  " + fileInfoText_;
        fileInfoLabel_.setText(display, juce::dontSendNotification);
        resized();
        repaint();
    }

    void ReferenceOnboardingCard::reset()
    {
        hasFile_ = false;
        isURL_   = false;
        fileName_.clear();
        filePath_.clear();
        url_.clear();
        fileInfoText_.clear();
        fileInfoLabel_.setText({}, juce::dontSendNotification);
        dropZone_.setShowDropHint(true);
        resized();
        repaint();
    }

} // namespace mixcoach

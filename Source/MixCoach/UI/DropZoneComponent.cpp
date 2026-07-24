#include "DropZoneComponent.h"
#include "ReferencePanelIcons.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

    DropZoneComponent::DropZoneComponent()
    {
        setSize(360, 120); // default size evita 0x0 si se agrega sin bounds
        // ─── URL Input ─────────────────────────────────────────────────────────
        urlInput_.setMultiLine(false);
        urlInput_.setFont(juce::Font(juce::FontOptions(11.0f)));
        urlInput_.setTextToShowWhenEmpty("Pega enlace YouTube, Spotify...", MixCoachTheme::textMuted());
        urlInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
        urlInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
        urlInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
        urlInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
        urlInput_.setIndents(8, 4);
        urlInput_.addListener(this);
        addAndMakeVisible(urlInput_);

        // ─── Add URL button ────────────────────────────────────────────────────
        addUrlButton_.setButtonText("+");
        addUrlButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.3f));
        addUrlButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::accentGlow());
        addUrlButton_.onClick = [this]() {
            auto url = urlInput_.getText().trim();
            if (url.isNotEmpty() && onURLAdded) {
                onURLAdded(url);
                urlInput_.clear();
            }
        };
        addAndMakeVisible(addUrlButton_);

        // ─── Browse button ──────────────────────────────────────────────────
        browseButton_.setButtonText("EXPLORAR");
        browseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
        browseButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::accentGlow());
        browseButton_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent().withAlpha(0.15f));
        browseButton_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        browseButton_.onClick = [this]() { browseForFiles(); };
        addAndMakeVisible(browseButton_);

        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    DropZoneComponent::~DropZoneComponent() = default;

    void DropZoneComponent::resized()
    {
        auto area = getLocalBounds().reduced(4, 2);

        int gap        = 4;
        int halfW      = (area.getWidth() - gap) / 2;
        auto audioHalf = area.removeFromLeft(halfW);
        area.removeFromLeft(gap);
        auto linkHalf = area;

        audioSectionBounds_ = audioHalf;
        linkSectionBounds_  = linkHalf;

        if (showHint_) {
            // --- ESTADO VACÍO (Más espacio, botones centrados) ---
            // Audio card: browse button at the bottom centre
            {
                auto audioBody = audioHalf.reduced(4, 4);
                browseButton_.setBounds(audioBody.removeFromBottom(20).withSizeKeepingCentre(100, 20));
            }

            // Link card: input text and + button at the bottom
            {
                auto linkBody = linkHalf.reduced(4, 4);
                auto urlRow   = linkBody.removeFromBottom(20);
                addUrlButton_.setBounds(urlRow.removeFromRight(26).reduced(1, 1));
                urlInput_.setBounds(urlRow.reduced(0, 1));
            }
        }
        else {
            // --- ESTADO POBLADO (Compacto, sin superposiciones) ---
            // Audio card: browse button fills the right part of left column, leaving icon space
            {
                auto audioBody = audioHalf.reduced(2, 2);
                browseButton_.setBounds(audioBody.removeFromRight(80).withHeight(20));
            }

            // Link card: input text fills the column
            {
                auto linkBody = linkHalf.reduced(2, 2);
                auto urlRow   = linkBody;
                addUrlButton_.setBounds(urlRow.removeFromRight(22).withHeight(20).reduced(1, 1));
                urlInput_.setBounds(urlRow.withTrimmedLeft(45).reduced(0, 1)); // deja 45px para "LINK" e icono
            }
        }
    }

    void DropZoneComponent::paint(juce::Graphics& g)
    {
        auto bounds        = getLocalBounds().toFloat().reduced(1.0f);
        const float radius = 6.0f;

        // Fondo y borde general (glass panel consistency)
        if (isDragging_) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
            g.fillRoundedRectangle(bounds, radius);
            g.setColour(MixCoachTheme::accent().withAlpha(0.7f));
            juce::Path dashRect;
            dashRect.addRoundedRectangle(bounds, radius);
            g.strokePath(dashRect, juce::PathStrokeType(1.5f));
        }
        else {
            // Shadow sutil
            g.setColour(juce::Colours::black.withAlpha(0.10f));
            g.fillRoundedRectangle(bounds.expanded(1.5f, 2.0f), radius + 1.0f);
            // Fondo panel
            g.setColour(MixCoachTheme::bgPanel().withAlpha(0.85f));
            g.fillRoundedRectangle(bounds, radius);
            // Glass highlight
            auto glassH = bounds.withHeight(bounds.getHeight() * 0.35f);
            juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f),
                                           glassH.getX(),
                                           glassH.getY(),
                                           juce::Colour(0x00000000),
                                           glassH.getX(),
                                           glassH.getBottom(),
                                           false);
            g.setGradientFill(glassGrad);
            g.fillRoundedRectangle(glassH, radius);
            // Borde estándar
            g.setColour(MixCoachTheme::border());
            g.drawRoundedRectangle(bounds, radius, 0.8f);
        }

        if (showHint_) {
            // --- ESTADO VACÍO (Dos tarjetas grandes y descriptivas) ---
            if (!audioSectionBounds_.isEmpty()) {
                auto r = audioSectionBounds_.toFloat().reduced(3, 2);
                // Hover glow para tarjeta de audio
                if (audioSectionBounds_.contains(getMouseXYRelative())) {
                    g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
                    g.fillRoundedRectangle(r.expanded(2, 2), 6.0f);
                }
                g.setColour(MixCoachTheme::accent().withAlpha(0.04f));
                g.fillRoundedRectangle(r, 6.0f);
                g.setColour(MixCoachTheme::accent().withAlpha(0.25f));
                g.drawRoundedRectangle(r, 6.0f, 0.8f);

                drawFileIcon(g, r.getX() + 22.0f, r.getY() + r.getHeight() * 0.30f, 22.0f, MixCoachTheme::accentGlow());

                auto textR = r.withTrimmedLeft(40.0f).toNearestInt();
                g.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());
                g.setColour(MixCoachTheme::accentGlow());
                g.drawText("Archivo de Audio", textR.withHeight(16).translated(0, 6), juce::Justification::centredLeft);

                g.setFont(juce::Font(juce::FontOptions(8.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText("Arrastra o explora...",
                           textR.withHeight(12).translated(0, 22),
                           juce::Justification::centredLeft);

                // ═══ File format badges (WAV / FLAC / MP3) ═══════════════════
                const char* audioFormats[] = {"WAV", "FLAC", "MP3"};
                juce::Colour colWav = MixCoachTheme::success().withAlpha(0.7f);
                juce::Colour colFlac = MixCoachTheme::accentCyan().withAlpha(0.7f);
                juce::Colour colMp3 = MixCoachTheme::warning().withAlpha(0.7f);
                juce::Colour formatCols[] = { colWav, colFlac, colMp3 };
                int badgeY = textR.getY() + 38;
                int badgeX = textR.getX();
                for (int i = 0; i < 3; ++i) {
                    auto badgeFont = juce::Font(juce::FontOptions(6.5f)).boldened();
                    float bW = juce::GlyphArrangement::getStringWidthInt(badgeFont, audioFormats[i]) + 10.0f;
                    auto badgeR = juce::Rectangle<int>(badgeX, badgeY, (int)bW, 16).toFloat();
                    g.setColour(formatCols[i].withAlpha(0.12f));
                    g.fillRoundedRectangle(badgeR, 3.0f);
                    g.setColour(formatCols[i].withAlpha(0.4f));
                    g.drawRoundedRectangle(badgeR, 3.0f, 0.4f);
                    g.setFont(badgeFont);
                    g.setColour(formatCols[i]);
                    g.drawText(audioFormats[i], badgeR.toNearestInt(), juce::Justification::centred);
                    badgeX += (int)bW + 4;
                }
            }

            if (!linkSectionBounds_.isEmpty()) {
                auto r = linkSectionBounds_.toFloat().reduced(3, 2);
                // Hover glow para tarjeta de enlace
                if (linkSectionBounds_.contains(getMouseXYRelative())) {
                    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.08f));
                    g.fillRoundedRectangle(r.expanded(2, 2), 6.0f);
                }
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.04f));
                g.fillRoundedRectangle(r, 6.0f);
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.25f));
                g.drawRoundedRectangle(r, 6.0f, 0.8f);

                drawLinkIcon(g,
                             r.getX() + 22.0f,
                             r.getY() + r.getHeight() * 0.30f,
                             22.0f,
                             MixCoachTheme::accentCyan().brighter(0.2f));

                auto textR = r.withTrimmedLeft(40.0f).toNearestInt();
                g.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText("Enlace Web", textR.withHeight(16).translated(0, 6), juce::Justification::centredLeft);

                g.setFont(juce::Font(juce::FontOptions(8.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(
                    "YouTube, Spotify...", textR.withHeight(12).translated(0, 22), juce::Justification::centredLeft);

                // ═══ Streaming platform badges (YouTube / Spotify) ═══════════
                const char* platforms[] = {"YouTube", "Spotify"};
                juce::Colour colYt = juce::Colour(0xFFFF0000).withAlpha(0.6f);
                juce::Colour colSp = juce::Colour(0xFF1DB954).withAlpha(0.6f);
                juce::Colour platCols[] = { colYt, colSp };
                int badgeY = textR.getY() + 38;
                int badgeX = textR.getX();
                for (int i = 0; i < 2; ++i) {
                    auto badgeFont = juce::Font(juce::FontOptions(6.5f)).boldened();
                    float bW = juce::GlyphArrangement::getStringWidthInt(badgeFont, platforms[i]) + 12.0f;
                    auto badgeR = juce::Rectangle<int>(badgeX, badgeY, (int)bW, 16).toFloat();
                    g.setColour(platCols[i].withAlpha(0.12f));
                    g.fillRoundedRectangle(badgeR, 3.0f);
                    g.setColour(platCols[i].withAlpha(0.35f));
                    g.drawRoundedRectangle(badgeR, 3.0f, 0.4f);
                    g.setFont(badgeFont);
                    g.setColour(platCols[i]);
                    g.drawText(platforms[i], badgeR.toNearestInt(), juce::Justification::centred);
                    badgeX += (int)bW + 4;
                }
            }

            // ═══ Línea divisoria sutil entre las dos tarjetas ════════════════
            if (!audioSectionBounds_.isEmpty()) {
                int midX = audioSectionBounds_.getRight();
                g.setColour(MixCoachTheme::border().withAlpha(0.10f));
                g.drawVerticalLine(midX, bounds.getY() + 8.0f, bounds.getBottom() - 8.0f);
            }
        }
        else {
            // --- ESTADO CON ELEMENTOS (Barra de herramientas compacta) ---
            if (!audioSectionBounds_.isEmpty()) {
                auto r = audioSectionBounds_.toFloat().reduced(2, 1);
                g.setColour(MixCoachTheme::accent().withAlpha(0.02f));
                g.fillRoundedRectangle(r, 4.0f);

                drawFileIcon(g, r.getX() + 14.0f, r.getCentreY(), 11.0f, MixCoachTheme::accentGlow().withAlpha(0.6f));

                auto textR = r.withTrimmedLeft(26.0f).toNearestInt();
                g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
                g.setColour(MixCoachTheme::textMuted());
                g.drawText("AUDIO", textR, juce::Justification::centredLeft);
            }

            if (!linkSectionBounds_.isEmpty()) {
                auto r = linkSectionBounds_.toFloat().reduced(2, 1);
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.02f));
                g.fillRoundedRectangle(r, 4.0f);

                drawLinkIcon(g, r.getX() + 14.0f, r.getCentreY(), 11.0f, MixCoachTheme::accentCyan().withAlpha(0.6f));

                auto textR = r.withTrimmedLeft(26.0f).toNearestInt();
                g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
                g.setColour(MixCoachTheme::textMuted());
                g.drawText("LINK", textR, juce::Justification::centredLeft);
            }

            // Línea divisoria central sutil
            if (!audioSectionBounds_.isEmpty()) {
                int midX = audioSectionBounds_.getRight() + 2;
                g.setColour(MixCoachTheme::border().withAlpha(0.12f));
                g.drawVerticalLine(midX, bounds.getY() + 4.0f, bounds.getBottom() - 4.0f);
            }
        }
    }

    void DropZoneComponent::setShowDropHint(bool showHint)
    {
        showHint_ = showHint;
        repaint();
    }

    void DropZoneComponent::browseForFiles()
    {
        // Usar unique_ptr para evitar memory leak
        // Antes: new + delete manual. Si el usuario cancelaba el diálogo,
        // el callback nunca se ejecutaba y el FileChooser fugaba.
        // Ahora: unique_ptr se limpia automáticamente al destruir el componente
        // o al llamar browseForFiles() de nuevo.
        fileChooser_ = std::make_unique<juce::FileChooser>(
            "Seleccionar archivos de referencia", juce::File(), "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");

        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [this](const juce::FileChooser& fc) {
                                      auto results = fc.getResults();
                                      for (auto& f : results) {
                                          if (onFileDropped) onFileDropped(f.getFullPathName());
                                      }
                                      // unique_ptr se limpia automáticamente al salir del scope
                                      // o al llamar browseForFiles() de nuevo.
                                  });
    }

    void DropZoneComponent::mouseEnter(const juce::MouseEvent&)
    {
        isHovering_ = true;
        repaint();
    }

    void DropZoneComponent::mouseExit(const juce::MouseEvent&)
    {
        isHovering_ = false;
        repaint();
    }

    void DropZoneComponent::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // ═══ Click en tarjeta izquierda (Audio) → browse files ═══════════
        if (audioSectionBounds_.contains(pos)) {
            browseForFiles();
            return;
        }

        // ═══ Click en tarjeta derecha (Link) → focus URL input ═══════════
        if (linkSectionBounds_.contains(pos)) {
            urlInput_.grabKeyboardFocus();
            return;
        }
    }

    void DropZoneComponent::fileDragEnter(const juce::StringArray&, int, int)
    {
        isDragging_ = true;
        repaint();
    }

    void DropZoneComponent::fileDragExit(const juce::StringArray&)
    {
        isDragging_ = false;
        repaint();
    }

    void DropZoneComponent::filesDropped(const juce::StringArray& files, int, int)
    {
        isDragging_ = false;
        for (const auto& f : files) {
            auto file = juce::File(f);
            auto ext  = file.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aiff" || ext == ".aif" || ext == ".ogg") {
                if (onFileDropped) onFileDropped(file.getFullPathName());
            }
        }
        repaint();
    }

    void DropZoneComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
    {
        if (&editor == &urlInput_) {
            auto url = urlInput_.getText().trim();
            if (url.isNotEmpty() && onURLAdded) {
                onURLAdded(url);
                urlInput_.clear();
            }
        }
    }

} // namespace mixcoach

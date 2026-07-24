#include "WelcomeComponent.h"
#include "MixCoachTheme.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constants — Diseño exacto según spec
    // ═══════════════════════════════════════════════════════════════════════════

    namespace WelcomeLayout {
        // Avatar
        constexpr int kAvatarSize = 300;

        // Title
        constexpr float kTitleFontSize = 56.0f;

        // Subtitle
        constexpr float kSubtitleFontSize = 28.0f;
        constexpr int kTitleSubtitleGap = 50;

        // Input
        constexpr int kInputWidth = 740;
        constexpr int kInputHeight = 74;
        constexpr float kInputFontSize = 22.0f;
        constexpr float kInputRadius = 14.0f;
        constexpr int kInputBorderWidth = 2;

        // Button
        constexpr int kButtonWidth = 430;
        constexpr int kButtonHeight = 74;
        constexpr float kButtonFontSize = 24.0f;
        constexpr float kButtonRadius = 14.0f;
        constexpr int kInputButtonGap = 32;

        // Label above input
        constexpr float kLabelFontSize = 13.0f;
        constexpr int kSubtitleLabelGap = 16;
        constexpr int kLabelInputGap = 8;

        // Spacing
        constexpr int kAvatarTitleGap = 24;
        constexpr int kInputPaddingLeft = 24;
    }

    namespace WelcomeColours {
        inline juce::Colour bgTop()           { return juce::Colour(0xFF06060B); }
        inline juce::Colour bgCenter()        { return juce::Colour(0xFF090812); }
        inline juce::Colour bgBottom()        { return juce::Colour(0xFF130E21); }
        inline juce::Colour glowColour()      { return juce::Colour(0xFF8B5CF6); }
        inline juce::Colour titleWhite()      { return juce::Colours::white; }
        inline juce::Colour titlePurple()     { return juce::Colour(0xFFA855F7); }
        inline juce::Colour subtitleGray()    { return juce::Colour(0xFFB8B8C5); }
        inline juce::Colour subtitlePurple()  { return juce::Colour(0xFFA855F7); }
        inline juce::Colour inputBg()         { return juce::Colour(0xFF1A1826); }
        inline juce::Colour inputBorder()     { return juce::Colour(0xFF7C3AED); }
        inline juce::Colour inputPlaceholder(){ return juce::Colour(0xFF8C8C98); }
        inline juce::Colour inputText()       { return juce::Colours::white; }
        inline juce::Colour inputFocusGlow()  { return juce::Colour(0xFF8B5CF6); }
        inline juce::Colour btnGradTop()      { return juce::Colour(0xFFA855F7); }
        inline juce::Colour btnGradBot()      { return juce::Colour(0xFF7C3AED); }
        inline juce::Colour btnGlow()         { return juce::Colour(0xFF8B5CF6); }
        inline juce::Colour btnText()         { return juce::Colours::white; }
        inline juce::Colour borderError()     { return juce::Colour(0xFFFF3B30); }
        inline juce::Colour errorText()       { return juce::Colour(0xFFFF3B30); }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Easing helpers
    // ═══════════════════════════════════════════════════════════════════════════

    float WelcomeComponent::easeOutCubic(float t)
    {
        return 1.0f - std::pow(1.0f - t, 3.0f);
    }

    float WelcomeComponent::easeOutBack(float t)
    {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Layout helper
    // ═══════════════════════════════════════════════════════════════════════════

    float WelcomeComponent::getLayoutScale() const noexcept
    {
        const float scaleX = static_cast<float>(getWidth()) / 1600.0f;
        const float scaleY = static_cast<float>(getHeight()) / 900.0f;
        return juce::jlimit(0.72f, 1.0f, juce::jmin(scaleX, scaleY));
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::visibilityChanged()
    {
        // NOTA: El timer arranca en el constructor (startTimerHz(60)).
        // Por diseño, NO lo detenemos al ocultar: el WelcomeComponent
        // debe mantener su animación incluso si la ventana se minimiza,
        // para reanudar correctamente al restaurar.
        // El timerCallback ya maneja auto-stop cuando la animación termina.
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(60);
        }
    }

    int WelcomeComponent::getContentCenterY() const noexcept
    {
        const float scale = getLayoutScale();
        const int blockH = getContentBlockHeight();
        const int availableH = getHeight();
        const int topY = juce::jmax(0, (availableH - blockH) / 2);
        const int avatarSize = juce::roundToInt(WelcomeLayout::kAvatarSize * scale);

        return topY + avatarSize / 2;
    }

    int WelcomeComponent::getContentBlockHeight() const noexcept
    {
        const float scale = getLayoutScale();
        const float titleLine = WelcomeLayout::kTitleFontSize * 1.2f;
        const float subtitleLine = WelcomeLayout::kSubtitleFontSize * 1.2f;

        return juce::roundToInt(
            (WelcomeLayout::kAvatarSize
             + WelcomeLayout::kAvatarTitleGap
             + titleLine
             + WelcomeLayout::kTitleSubtitleGap
             + subtitleLine
             + WelcomeLayout::kSubtitleLabelGap
             + WelcomeLayout::kLabelFontSize
             + WelcomeLayout::kLabelInputGap
             + WelcomeLayout::kInputHeight
             + WelcomeLayout::kInputButtonGap
             + WelcomeLayout::kButtonHeight) * scale);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════

    WelcomeComponent::WelcomeComponent()
    {
        setSize(1600, 900);
        // ─── Avatar component ──────────────────────────────────────────────
        avatar_.setOpaque(false);
        avatar_.setExpression(AvatarExpression::Neutral);
        addChildComponent(avatar_);

        // ─── Text input ────────────────────────────────────────────────────
        nameEditor_.setMultiLine(false);
        nameEditor_.setReturnKeyStartsNewLine(false);
        nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        nameEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        nameEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        nameEditor_.setColour(juce::CaretComponent::caretColourId, juce::Colours::white);
        nameEditor_.setFont(interFont(WelcomeLayout::kInputFontSize));
        nameEditor_.setTextToShowWhenEmpty("Escribe tu nombre...", WelcomeColours::inputPlaceholder());
        nameEditor_.setJustification(juce::Justification::centredLeft);
        nameEditor_.setBorder(juce::BorderSize<int>(0, WelcomeLayout::kInputPaddingLeft, 0, 12));
        nameEditor_.addListener(this);
        addChildComponent(nameEditor_);

        // ─── COMENZAR button ───────────────────────────────────────────────
        startButton_.setButtonText("COMENZAR");
        startButton_.setLookAndFeel(&startButtonLnf_);
        startButton_.setColour(juce::TextButton::buttonColourId, WelcomeColours::btnGradTop());
        startButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        startButton_.onClick = [this] { onStartClicked(); };
        addChildComponent(startButton_);

        // ─── Timer a 60fps para animaciones ───────────────────────────────
        startTimerHz(60);
    }

    WelcomeComponent::~WelcomeComponent()
    {
        stopTimer();
        startButton_.setLookAndFeel(nullptr);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  startWelcomeAnimation — Arranca el staggered sequence
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::startWelcomeAnimation()
    {
        animPhase_ = AnimPhase::RobotReveal;
        animStartMs_ = juce::Time::getMillisecondCounter();
        robotAnimStartMs_ = animStartMs_;

        // Reset alphas
        robotAlpha_ = 0.0f;
        robotScale_ = 0.95f;
        titleAlpha_ = 0.0f;
        subtitleAlpha_ = 0.0f;
        inputAlpha_ = 0.0f;
        buttonAlpha_ = 0.0f;

        // Mostrar componentes (con alpha 0 inicialmente)
        avatar_.setVisible(true);
        nameEditor_.setVisible(true);
        startButton_.setVisible(true);
        avatar_.setAlpha(0.0f);
        nameEditor_.setAlpha(0.0f);
        startButton_.setAlpha(0.0f);

        // ═══ GARANTIZAR que el timer esté corriendo ═══════════════════
        // El timer arranca en el constructor (startTimerHz(60)), pero puede ser
        // detenido por visibilityChanged() durante la inicialización si el
        // componente no está en un parent visible aún. Este fallback asegura
        // que el timer SIEMPRE esté activo para animar los alpha values.
        if (!isTimerRunning()) {
            startTimerHz(60);
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — 60fps: animaciones escalonadas + robot continuas
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::timerCallback()
    {
        if (animPhase_ == AnimPhase::Inactive) return;

        int64_t now = juce::Time::getMillisecondCounter();
        int64_t elapsed = now - animStartMs_;
        int64_t robotElapsed = now - robotAnimStartMs_;

        // ═══ Staggered phases (timing en ms) ═══════════════════════════════
        constexpr int kRobotEnd    = 160;
        constexpr int kTitleEnd    = 260;
        constexpr int kSubtitleEnd = 360;
        constexpr int kInputEnd    = 460;
        constexpr int kButtonEnd   = 560;

        // Robot: 0-160ms
        if (elapsed < kRobotEnd) {
            animPhase_ = AnimPhase::RobotReveal;
            float t = juce::jmin(1.0f, elapsed / 140.0f);
            robotAlpha_ = easeOutCubic(t);
            robotScale_ = 0.95f + 0.05f * easeOutBack(t);
            avatar_.setAlpha(robotAlpha_);
        }
        // Title: 160-260ms
        else if (elapsed < kTitleEnd) {
            animPhase_ = AnimPhase::TitleFadeIn;
            float t = juce::jmin(1.0f, (elapsed - kRobotEnd) / 90.0f);
            titleAlpha_ = easeOutCubic(t);
        }
        // Subtitle: 260-360ms
        else if (elapsed < kSubtitleEnd) {
            animPhase_ = AnimPhase::SubtitleFadeIn;
            float t = juce::jmin(1.0f, (elapsed - kTitleEnd) / 90.0f);
            subtitleAlpha_ = easeOutCubic(t);
        }
        // Input: 360-460ms
        else if (elapsed < kInputEnd) {
            animPhase_ = AnimPhase::InputFadeIn;
            float t = juce::jmin(1.0f, (elapsed - kSubtitleEnd) / 90.0f);
            inputAlpha_ = easeOutCubic(t);
            nameEditor_.setAlpha(inputAlpha_);
        }
        // Button: 460-560ms
        else if (elapsed < kButtonEnd) {
            animPhase_ = AnimPhase::ButtonFadeIn;
            float t = juce::jmin(1.0f, (elapsed - kInputEnd) / 90.0f);
            buttonAlpha_ = easeOutCubic(t);
            startButton_.setAlpha(buttonAlpha_);
        }
        // Complete
        else {
            if (animPhase_ != AnimPhase::Complete) {
                animPhase_ = AnimPhase::Complete;
                // Final values
                robotAlpha_ = 1.0f;
                robotScale_ = 1.0f;
                titleAlpha_ = 1.0f;
                subtitleAlpha_ = 1.0f;
                inputAlpha_ = 1.0f;
                buttonAlpha_ = 1.0f;
                avatar_.setAlpha(1.0f);
                nameEditor_.setAlpha(1.0f);
                startButton_.setAlpha(1.0f);
                // Dar foco al input
                nameEditor_.grabKeyboardFocus();
            }
        }

        // ═══ Continuous robot animations (halo pulse) ═══════════════════
        // MixBotComponent maneja floating/breathing/blink internamente en su timer a 60fps.
        // WelcomeComponent solo controla el halo pulsante exterior.
        if (elapsed >= 0) {
            float cycleSec = robotElapsed / 1000.0f;
            // Halo pulsing: seno, 0.0 ↔ 0.55, periodo ~3s
            robotHaloAlpha_ = 0.275f + 0.275f * std::sin(cycleSec * juce::MathConstants<float>::pi * 0.67f);
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::resized()
    {
        const float scale = getLayoutScale();
        const int cx = getWidth() / 2;
        const int centerY = getContentCenterY();
        const int avatarSize = juce::roundToInt(WelcomeLayout::kAvatarSize * scale);
        const int avatarTitleGap = juce::roundToInt(WelcomeLayout::kAvatarTitleGap * scale);
        const int titleLine = juce::roundToInt(WelcomeLayout::kTitleFontSize * 1.2f * scale);
        const int titleSubtitleGap = juce::roundToInt(WelcomeLayout::kTitleSubtitleGap * scale);
        const int subtitleLine = juce::roundToInt(WelcomeLayout::kSubtitleFontSize * 1.2f * scale);
        const int inputW = juce::roundToInt(WelcomeLayout::kInputWidth * scale);
        const int inputH = juce::roundToInt(WelcomeLayout::kInputHeight * scale);
        const int buttonW = juce::roundToInt(WelcomeLayout::kButtonWidth * scale);
        const int buttonH = juce::roundToInt(WelcomeLayout::kButtonHeight * scale);
        const int inputButtonGap = juce::roundToInt(WelcomeLayout::kInputButtonGap * scale);

        nameEditor_.setFont(interFont(WelcomeLayout::kInputFontSize * scale));
        nameEditor_.setBorder(juce::BorderSize<int>(0,
                                                    juce::roundToInt(WelcomeLayout::kInputPaddingLeft * scale),
                                                    0,
                                                    juce::roundToInt(12.0f * scale)));

        // Robot avatar
        int avatarY = centerY - avatarSize / 2;
        avatar_.setBounds(cx - avatarSize / 2,
                          avatarY,
                          avatarSize,
                          avatarSize);

        // Label "?COMO TE LLAMAS?" above input
        int labelFontH = juce::roundToInt(WelcomeLayout::kLabelFontSize * scale);
        int labelY = avatarY + avatarSize
                     + avatarTitleGap
                     + titleLine
                     + titleSubtitleGap
                     + subtitleLine
                     + juce::roundToInt(WelcomeLayout::kSubtitleLabelGap * scale);

        int inputY = labelY + labelFontH
                     + juce::roundToInt(WelcomeLayout::kLabelInputGap * scale);

        nameEditor_.setBounds(cx - inputW / 2,
                              inputY,
                              inputW,
                              inputH);

        // Button
        int buttonY = inputY + inputH + inputButtonGap;
        startButton_.setBounds(cx - buttonW / 2,
                               buttonY,
                               buttonW,
                               buttonH);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Fondo + todos los elementos visuales
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::paint(juce::Graphics& g)
    {
        const float scale = getLayoutScale();

        // ═══ FALLBACK: si el timer no está corriendo, dibujar todo visible ═══
        // Protege contra casos donde el timer fue detenido antes de animar
        // (ej: visibilityChanged durante inicialización con isShowing()=false).
        bool timerRunning = isTimerRunning();
        if (!timerRunning && animPhase_ != AnimPhase::Complete) {
            // Forzar alpha final en todos los elementos
            robotAlpha_ = 1.0f;
            robotScale_ = 1.0f;
            titleAlpha_ = 1.0f;
            subtitleAlpha_ = 1.0f;
            inputAlpha_ = 1.0f;
            buttonAlpha_ = 1.0f;
            robotHaloAlpha_ = 0.275f;
            avatar_.setAlpha(1.0f);
            nameEditor_.setAlpha(1.0f);
            startButton_.setAlpha(1.0f);
            animPhase_ = AnimPhase::Complete;
        }

        // ─── 1. Background gradient + radial glow ────────────────────────
        drawBackground(g);
        drawRadialGlow(g);

        // ─── 2. Avatar circle border + glow ──────────────────────────────
        auto avatarBounds = avatar_.getBounds();
        drawAvatarArea(g, avatarBounds);

        // ─── 3. Title — "Bienvenido a MixCoach" ────────────────────────
        int titleY = avatarBounds.getBottom() + juce::roundToInt(WelcomeLayout::kAvatarTitleGap * scale);
        drawTitle(g, titleY);

        // ─── 4. Subtitle — "Tu mentor de mezcla impulsado por IA." ────
        int subtitleY = titleY
                        + juce::roundToInt(WelcomeLayout::kTitleFontSize * 1.2f * scale)
                        + juce::roundToInt(WelcomeLayout::kTitleSubtitleGap * scale);
        drawSubtitle(g, subtitleY);

        // ─── 5. Label "?COMO TE LLAMAS?" sobre el input ──────────────────
        {
            float alpha = inputAlpha_;
            if (alpha > 0.0f) {
                int cx = getWidth() / 2;
                float fontSize = WelcomeLayout::kLabelFontSize * getLayoutScale();
                g.setFont(interFont(fontSize).boldened()); // BUG #21: usar interFont evita crash FL Studio
                g.setColour(WelcomeColours::titlePurple().withAlpha(alpha * 0.9f));

                // Tracking: calculamos centro del input como punto de referencia
                auto inputBounds = nameEditor_.getBounds();
                juce::String labelText = "¿COMO TE LLAMAS?";
                float textW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), labelText);
                float labelX = cx - textW * 0.5f;
                // Label Y: justo encima del input, con labelInputGap de separacion
                // Baseline = labelTop + fontSize*0.85f, where labelTop = inputBounds.Y - gap*scale - fontSize
                float gapPx = WelcomeLayout::kLabelInputGap * getLayoutScale();
                float labelY = (float)inputBounds.getY() - gapPx - fontSize * 0.15f;
                g.drawSingleLineText(labelText,
                                     juce::roundToInt(labelX),
                                     juce::roundToInt(labelY),
                                     juce::Justification::left);
            }
        }

        // ─── 6. Input border (custom painted over TextEditor) ────────────
        drawInputBorder(g, nameEditor_.getBounds());
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Background: gradiente vertical #06060B → #090812 → #130E21
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::drawBackground(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float h = bounds.getHeight();
        float midY = h * 0.5f;

        // Top → Center
        {
            juce::ColourGradient grad(
                WelcomeColours::bgTop(), 0.0f, 0.0f,
                WelcomeColours::bgCenter(), 0.0f, midY, false
            );
            g.setGradientFill(grad);
            g.fillRect(0.0f, 0.0f, bounds.getWidth(), midY);
        }

        // Center → Bottom
        {
            juce::ColourGradient grad(
                WelcomeColours::bgCenter(), 0.0f, midY,
                WelcomeColours::bgBottom(), 0.0f, h, false
            );
            g.setGradientFill(grad);
            g.fillRect(0.0f, midY, bounds.getWidth(), h - midY);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Radial glow: #8B5CF6 al 10-15% detrás del contenido
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::drawRadialGlow(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY() - 60.0f; // Ligeramente arriba para iluminar avatar
        float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.40f;

        juce::ColourGradient glow(
            WelcomeColours::glowColour().withAlpha(0.12f), cx, cy,
            juce::Colours::transparentBlack, cx + radius, cy + radius, true
        );
        glow.addColour(0.5f, WelcomeColours::glowColour().withAlpha(0.06f));
        glow.addColour(0.8f, WelcomeColours::glowColour().withAlpha(0.02f));
        g.setGradientFill(glow);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }



    // ═══════════════════════════════════════════════════════════════════════════
    //  Avatar area: círculo transparente + glow + RobotAvatarComponent dentro
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::drawAvatarArea(juce::Graphics& g, juce::Rectangle<int> avatarBounds)
    {
        float alpha = robotAlpha_;
        if (alpha <= 0.0f) return;

        const float layoutScale = getLayoutScale();
        auto bounds = avatarBounds.toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float r = bounds.getWidth() * 0.5f;

        // ─── Halo pulsante exterior ────────────────────────────────────
        float glowR = r * 1.6f;
        float haloA = robotHaloAlpha_ * alpha;
        juce::ColourGradient haloGlow(
            WelcomeColours::glowColour().withAlpha(0.55f * haloA), cx, cy,
            juce::Colours::transparentBlack, cx + glowR, cy + glowR, true
        );
        g.setGradientFill(haloGlow);
        g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);

        // ─── Círculo fijo (sin float — solo el avatar se mueve) ────────
        // El círculo permanece estático; solo el child avatar flota
        g.setColour(juce::Colour(0x0AFFFFFF).withAlpha(0.04f * alpha));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        // Border
        g.setColour(juce::Colour(0x14FFFFFF).withAlpha(0.08f * alpha));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

        // ─── MixBotComponent se renderiza en vivo (floating + breathing + blink
        //     son manejados internamente por su timer a 60fps).
        //     Solo aplicamos un pequeño offset vertical para centrar mejor.
        const float avatarOffsetY = -8.0f * layoutScale;
        avatar_.setTransform(
            juce::AffineTransform::translation(0.0f, avatarOffsetY)
        );
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Title: "Bienvenido a MixCoach" — "MixCoach" en púrpura
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::drawTitle(juce::Graphics& g, int titleY)
    {
        float alpha = titleAlpha_;
        if (alpha <= 0.0f) return;

        int cx = getWidth() / 2;
        float fontSize = WelcomeLayout::kTitleFontSize * getLayoutScale();
        g.setFont(interFont(fontSize).boldened()); // BUG #21: usar interFont evita crash FL Studio

        juce::String fullText = "Bienvenido a MixCoach";
        juce::String before = "Bienvenido a ";
        juce::String highlight = "MixCoach";

        auto font = g.getCurrentFont();
        float totalW = juce::GlyphArrangement::getStringWidth(font, fullText);
        float beforeW = juce::GlyphArrangement::getStringWidth(font, before);
        float startX = cx - totalW * 0.5f;
        float baselineY = titleY + fontSize * 0.85f;

        // "Bienvenido a " in white
        g.setColour(WelcomeColours::titleWhite().withAlpha(alpha));
        g.drawSingleLineText(before,
                             juce::roundToInt(startX),
                             juce::roundToInt(baselineY),
                             juce::Justification::left);

        // "MixCoach" in purple
        g.setColour(WelcomeColours::titlePurple().withAlpha(alpha));
        g.drawSingleLineText(highlight,
                             juce::roundToInt(startX + beforeW),
                             juce::roundToInt(baselineY),
                             juce::Justification::left);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Subtitle: "Tu mentor de mezcla impulsado por IA." — "mentor" en púrpura
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::drawSubtitle(juce::Graphics& g, int subtitleY)
    {
        float alpha = subtitleAlpha_;
        if (alpha <= 0.0f) return;

        int cx = getWidth() / 2;
        float fontSize = WelcomeLayout::kSubtitleFontSize * getLayoutScale();
        g.setFont(interFont(fontSize)); // BUG #21: usar interFont evita crash FL Studio

        juce::String before = "Tu ";
        juce::String highlight = "mentor";
        juce::String after = " de mezcla impulsado por IA.";

        auto font = g.getCurrentFont();
        float beforeW = juce::GlyphArrangement::getStringWidth(font, before);
        float highlightW = juce::GlyphArrangement::getStringWidth(font, highlight);
        float afterW = juce::GlyphArrangement::getStringWidth(font, after);
        float totalW = beforeW + highlightW + afterW;

        float startX = cx - totalW * 0.5f;
        float baselineY = subtitleY + fontSize * 0.82f;

        // "Tu " in gray
        g.setColour(WelcomeColours::subtitleGray().withAlpha(alpha));
        g.drawSingleLineText(before,
                             juce::roundToInt(startX),
                             juce::roundToInt(baselineY),
                             juce::Justification::left);

        // "mentor" in purple
        g.setColour(WelcomeColours::subtitlePurple().withAlpha(alpha));
        g.drawSingleLineText(highlight,
                             juce::roundToInt(startX + beforeW),
                             juce::roundToInt(baselineY),
                             juce::Justification::left);

        // rest in gray
        g.setColour(WelcomeColours::subtitleGray().withAlpha(alpha));
        g.drawSingleLineText(after,
                             juce::roundToInt(startX + beforeW + highlightW),
                             juce::roundToInt(baselineY),
                             juce::Justification::left);
    }



    // ═══════════════════════════════════════════════════════════════════════════
    //  Input border: rounded rect painted over the TextEditor
    //  JUCE TextEditor no soporta border-radius nativo
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::drawInputBorder(juce::Graphics& g, juce::Rectangle<int> inputBounds)
    {
        float alpha = inputAlpha_;
        if (alpha <= 0.0f) return;

        auto bounds = inputBounds.toFloat();
        const float radius = juce::jmin(WelcomeLayout::kInputRadius, bounds.getHeight() * 0.22f);
        const float borderWidth = juce::jmax(1.0f, bounds.getHeight() * 0.027f);

        // ─── Fondo del input ─────────────────────────────────────────
        g.setColour(WelcomeColours::inputBg().withAlpha(alpha));
        g.fillRoundedRectangle(bounds, radius);

        // ─── Borde ──────────────────────────────────────────────────
        juce::Colour borderColour;
        if (hasValidationError_) {
            borderColour = WelcomeColours::borderError().withAlpha(alpha);
        } else if (nameEditor_.hasKeyboardFocus(true)) {
            borderColour = WelcomeColours::inputBorder().withAlpha(0.9f * alpha);
        } else {
            borderColour = WelcomeColours::inputBorder().withAlpha(0.4f * alpha);
        }

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, radius, borderWidth);

        // ─── Focus glow ──────────────────────────────────────────
        if (!hasValidationError_ && nameEditor_.hasKeyboardFocus(true)) {
            g.setColour(WelcomeColours::inputFocusGlow().withAlpha(0.10f * alpha));
            g.fillRoundedRectangle(bounds.expanded(bounds.getHeight() * 0.05f, bounds.getHeight() * 0.05f),
                                   radius + 2.0f);
        }
    }



    // ═══════════════════════════════════════════════════════════════════════════
    //  StartButtonLookAndFeel — Button con gradiente, glow, flecha →
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::StartButtonLookAndFeel::drawButtonBackground(
        juce::Graphics& g,
        juce::Button& button,
        const juce::Colour& /*backgroundColour*/,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown)
    {
        float alpha = button.getAlpha();
        auto bounds = button.getLocalBounds().toFloat();
        const float radius = juce::jmin(WelcomeLayout::kButtonRadius, bounds.getHeight() * 0.22f);
        const float shadowOffset = juce::jmax(2.0f, bounds.getHeight() * 0.04f);

        // ─── Shadow ─────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.25f * alpha));
        g.fillRoundedRectangle(bounds.translated(0.0f, shadowOffset), radius);

        // ─── Gradient background ────────────────────────────────────────
        juce::Colour topCol = WelcomeColours::btnGradTop();
        juce::Colour botCol = WelcomeColours::btnGradBot();

        if (shouldDrawButtonAsDown) {
            topCol = topCol.darker(0.15f);
            botCol = botCol.darker(0.15f);
        } else if (shouldDrawButtonAsHighlighted) {
            topCol = topCol.brighter(0.08f);
        }

        juce::ColourGradient btnGrad(topCol.withAlpha(alpha),
                                     bounds.getCentreX(), bounds.getY(),
                                     botCol.withAlpha(alpha),
                                     bounds.getCentreX(), bounds.getBottom(),
                                     false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(bounds, radius);

        // ─── Hover glow exterior ────────────────────────────────────────
        if (shouldDrawButtonAsHighlighted) {
            g.setColour(WelcomeColours::btnGlow().withAlpha(0.15f * alpha));
            g.fillRoundedRectangle(bounds.expanded(bounds.getHeight() * 0.07f, bounds.getHeight() * 0.05f),
                                   radius + 2.0f);
        }

        // ─── Glass highlight (top half) ────────────────────────────────
        auto glassArea = bounds.withHeight(bounds.getHeight() * 0.5f);
        juce::ColourGradient glassGrad(
            juce::Colours::white.withAlpha(0.08f * alpha),
            glassArea.getCentreX(), glassArea.getY(),
            juce::Colours::transparentBlack,
            glassArea.getCentreX(), glassArea.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassArea, radius);
    }

    void WelcomeComponent::StartButtonLookAndFeel::drawButtonText(
        juce::Graphics& g,
        juce::TextButton& button,
        bool /*shouldDrawButtonAsHighlighted*/,
        bool /*shouldDrawButtonAsDown*/)
    {
        // Usar el texto por defecto de JUCE con el color btnText
        // En lugar del tracking + flecha personalizados que causaban glitches
        auto& lnf = juce::LookAndFeel_V4::getDefaultLookAndFeel();
        lnf.drawButtonText(g, button, false, false);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TextEditor listeners
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::textEditorReturnKeyPressed(juce::TextEditor&)
    {
        onStartClicked();
    }

    void WelcomeComponent::textEditorEscapeKeyPressed(juce::TextEditor&)
    {
        nameEditor_.clear();
        hasValidationError_ = false;
        nameEditor_.setTextToShowWhenEmpty("Escribe tu nombre...",
                                           WelcomeColours::inputPlaceholder());
        nameEditor_.grabKeyboardFocus();
        repaint();
    }

    void WelcomeComponent::textEditorTextChanged(juce::TextEditor& editor)
    {
        if (hasValidationError_) {
            hasValidationError_ = false;
            nameEditor_.setTextToShowWhenEmpty("Escribe tu nombre...",
                                               WelcomeColours::inputPlaceholder());
            repaint();
        }

        // ═══ P1: Robot reacciona al escribir ═══════════════════════════════
        // El robot mira hacia el input de texto y muestra expresi\xC3\xB3n de escucha
        avatar_.setGaze(0.0f, 0.35f);  // Mirar ligeramente hacia abajo (input)

        if (editor.getText().isNotEmpty()) {
            avatar_.setExpressionWithDecay(AvatarExpression::Happy, 3000);
        } else {
            avatar_.setExpressionWithDecay(AvatarExpression::Neutral, 1000);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  onStartClicked — Validar y disparar callback
    // ═══════════════════════════════════════════════════════════════════════════

    void WelcomeComponent::onStartClicked()
    {
        LogHelper::writeToLog("[DIAG] WelcomeComponent::onStartClicked() called");
        auto userName = nameEditor_.getText().trim();

        if (userName.isEmpty()) {
            hasValidationError_ = true;
            nameEditor_.setTextToShowWhenEmpty("Por favor, escribe tu nombre",
                                               WelcomeColours::borderError().withAlpha(0.8f));
            nameEditor_.grabKeyboardFocus();
            repaint();
            return;
        }

        // ═══ P1: Robot celebra y asiente al confirmar nombre ═══════════════
        avatar_.setExpression(AvatarExpression::Happy);
        avatar_.triggerNod(500);

        // Disable controls
        startButton_.setEnabled(false);
        nameEditor_.setEnabled(false);

        // Fade out animation (rapid) — esperar a que el nod termine (~500ms)
        if (onStart) {
            juce::Component::SafePointer<WelcomeComponent> safeRef(this);
            int64_t delayMs = 450;
            juce::Timer::callAfterDelay(delayMs, [safeRef, userName]() {
                if (safeRef != nullptr && safeRef->onStart) {
                    safeRef->onStart(userName);
                }
            });
        }
    }

} // namespace mixcoach

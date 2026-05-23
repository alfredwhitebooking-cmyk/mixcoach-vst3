#include "CoachChatComponent.h"
#include "../../Common/Constants.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MessengerListComponent Implementation
//  Lista agrupada por buses con cabeceras de sección coloreadas
// ═══════════════════════════════════════════════════════════════════════════

MessengerListComponent::MessengerListComponent()
{
    titleLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\xA1 Pistas Detectadas")), juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accent());
    addAndMakeVisible(titleLabel_);

    emptyLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8C Conecta plugins Messenger en tus pistas para verlas aqui.\n\nCada pista mostrara su nivel, bus y una sugerencia de IA."), juce::dontSendNotification);
    emptyLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(emptyLabel_);
}

void MessengerListComponent::resized()
{
    auto area = getLocalBounds().reduced(4);
    titleLabel_.setBounds(area.removeFromTop(22));
    emptyLabel_.setBounds(area);
}

void MessengerListComponent::updateMessengers(SlotRegistry& registry)
{
    // Sincronizar desde memoria compartida (IPC) si está disponible
    // Esto detecta Messengers en OTROS procesos (VST3 en el DAW)
    registry.syncFromShared();

    // Optimización: si el contador de cambios no ha cambiado, saltar
    auto currentChangeCount = registry.getChangeCount();
    if (currentChangeCount == displayVersion_ && activeMessengerCount_ > 0) {
        // Solo actualizar telemetría, no la estructura
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx >= 0 && idx < SlotRegistry::kMaxSlots && messengers_[idx].hasSignal) {
                auto& telem = registry.getTelemetry(idx);
                auto latest = telem.latest();
                messengers_[idx].peakLeft  = latest.peakLeft;
                messengers_[idx].peakRight = latest.peakRight;
                messengers_[idx].rmsAvg    = (latest.rmsLeft + latest.rmsRight) * 0.5f;
            }
        });
        // Repaint periódico para animaciones (status dot, level bars)
        repaint();
        return;
    }

    activeMessengerCount_ = 0;
    displayVersion_ = currentChangeCount;

    // Limpiar grupos
    for (auto& bg : busGroups_) bg.count = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx >= 0 && idx < SlotRegistry::kMaxSlots) {
            auto& entry = messengers_[idx];
            entry.info = info;

            auto& telem = registry.getTelemetry(idx);
            auto latest = telem.latest();
            entry.peakLeft  = latest.peakLeft;
            entry.peakRight = latest.peakRight;
            entry.rmsAvg    = (latest.rmsLeft + latest.rmsRight) * 0.5f;
            entry.hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);

            // Generar sugerencia AI según el nivel de la pista
            if (entry.hasSignal) {
                if (entry.peakLeft > -3.0f) {
                    entry.aiSuggestion = "\xE2\x9A\xA0 Reduce ganancia, muy cerca de clipping!";
                } else if (entry.peakLeft > -8.0f) {
                    entry.aiSuggestion = "Buen nivel, un poco caliente";
                } else if (entry.peakLeft > -18.0f) {
                    entry.aiSuggestion = "Nivel optimo, excelente!";
                } else if (entry.peakLeft > -30.0f) {
                    entry.aiSuggestion = "Podria subir un poco mas";
                } else {
                    entry.aiSuggestion = "Muy bajo, revisa el gain staging";
                }
            } else {
                entry.aiSuggestion.clear();
            }

            // Asignar al grupo de bus correspondiente
            int busIdx = (info.bus != BusType::None)
                ? static_cast<int>(info.bus)
                : kNumBuses; // "Sin Bus" al final
            auto& group = busGroups_[busIdx];
            if (group.count < SlotRegistry::kMaxSlots)
                group.slotIndices[group.count++] = idx;

            activeMessengerCount_++;
        }
    });

    repaint();
}

void MessengerListComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Fondo glass
    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    auto area = bounds.reduced(4);
    auto titleArea = area.removeFromTop(22);

    // Track count badge
    auto badgeArea = titleArea.removeFromRight(80);
    g.setColour(MixCoachTheme::accent().withAlpha(0.2f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 8.0f);
    g.setColour(MixCoachTheme::accent());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    g.drawText(juce::String(activeMessengerCount_) + " activo" +
               (activeMessengerCount_ != 1 ? "s" : ""),
               badgeArea, juce::Justification::centred);

    if (activeMessengerCount_ == 0) {
        emptyLabel_.setVisible(true);
        return;
    }
    emptyLabel_.setVisible(false);

    // ─── Dibujar grupos de buses ────────────────────────────────────────────
    const int kRowHeight = 54;
    const int kHeaderHeight = 20;

    for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
        auto& group = busGroups_[busIdx];
        if (group.count == 0) continue;

        // Cabecera del bus
        if (area.getHeight() < kHeaderHeight + kRowHeight) break;
        drawBusHeader(g, area, busIdx, group.count);

        // Filas de este bus
        int rowsInGroup = juce::jmin(group.count, kMaxRowsPerBus);
        for (int r = 0; r < rowsInGroup; ++r) {
            if (area.getHeight() < kRowHeight) break;
            int slotIdx = group.slotIndices[r];
            auto rowArea = area.removeFromTop(kRowHeight).reduced(2, 2);
            drawMessengerRow(g, rowArea, messengers_[slotIdx], r);
        }

        // "+ N más" si hay más de los que mostramos
        if (group.count > kMaxRowsPerBus) {
            if (area.getHeight() < 14) break;
            auto moreArea = area.removeFromTop(14).reduced(4, 0);
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
            g.drawText("+ " + juce::String(group.count - kMaxRowsPerBus) + " mas...",
                       moreArea, juce::Justification::centredRight);
        }
    }
}

// ─── Cabecera de sección de bus ─────────────────────────────────────────────
void MessengerListComponent::drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                                            int busIdx, int count)
{
    auto headerArea = bounds.removeFromTop(20).reduced(2, 0);

    juce::Colour busColour;
    juce::String busName;
    const char* busIcon;

    if (busIdx == kNumBuses) {
        // "Sin Bus" — usamos color neutro
        busColour = MixCoachTheme::textMuted();
        busName = "SIN ASIGNAR";
        busIcon = "\xE2\x97\x8B";
    } else {
        busColour = getBusColour(busIdx);
        busName = juce::String(busNames[busIdx]).toUpperCase();
        static const char* icons[] = {
            "\xF0\x9F\xA5\x81",  // Bateria
            "\xF0\x9F\x8E\xB8",  // Bajo
            "\xF0\x9F\x8E\xA8",  // Guitarras
            "\xF0\x9F\x8E\xB9",  // Teclados
            "\xF0\x9F\x8E\xA4",  // Voces
            "\xF0\x9F\x94\x80"   // FX
        };
        busIcon = icons[busIdx];
    }

    // Barra de color izquierda
    auto accentBar = headerArea.removeFromLeft(3);
    g.setColour(busColour);
    g.fillRoundedRectangle(accentBar.toFloat(), 1.5f);

    // Icono
    auto iconArea = headerArea.removeFromLeft(18);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(busColour);
    g.drawText(juce::String(busIcon), iconArea.reduced(1, 1), juce::Justification::centred);

    // Nombre del bus
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(busColour);
    auto nameArea = headerArea.removeFromLeft(80);
    g.drawText(busName, nameArea, juce::Justification::centredLeft);

    // Count badge
    auto badgeArea = headerArea.removeFromLeft(30).reduced(0, 2);
    g.setColour(busColour.withAlpha(0.15f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 6.0f);
    g.setColour(busColour);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    g.drawText(juce::String(count), badgeArea, juce::Justification::centred);

    // Separador
    auto lineY = headerArea.getBottom();
    g.setColour(busColour.withAlpha(0.15f));
    g.drawHorizontalLine(lineY, 4.0f, (float)getWidth() - 4);
}

// ─── Fila de Messenger — Diseño profesional tipo IK Multimedia ──────────────
void MessengerListComponent::drawMessengerRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                                const MessengerEntry& entry, int index)
{
    juce::ignoreUnused(index);

    // ─── Glass card background ─────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Highlight superior
    auto topGlow = bounds.withHeight(2);
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.fillRect(topGlow);

    // ─── Barra de color lateral (track colour) ──────────────────────────────
    auto colourBar = bounds.removeFromLeft(4);
    g.setColour(entry.info.colour);
    g.fillRoundedRectangle(colourBar.toFloat(), 2.0f);

    bounds.removeFromLeft(4);

    // ─── Status dot ────────────────────────────────────────────────────────
    auto statusDot = bounds.removeFromLeft(10).reduced(0, 12);
    if (entry.hasSignal) {
        float pulse = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.006f);
        g.setColour(MixCoachTheme::success().withAlpha(pulse));
    } else {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    }
    g.fillEllipse(statusDot.toFloat());

    bounds.removeFromLeft(2);

    // ─── Track name (con truncado inteligente) ──────────────────────────────
    auto routeArea = bounds.removeFromRight(86).reduced(2, 8);
    auto statsArea = bounds.removeFromRight(92).reduced(2, 5);
    auto barArea = bounds.removeFromRight(58).reduced(2, 12);

    auto nameArea = bounds.reduced(0, 4);
    auto nameTop = nameArea.removeFromTop(22);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    g.setColour(MixCoachTheme::textBright());
    auto name = juce::String(entry.info.trackName).trim();
    if (name.isEmpty())
        name = "Pista " + juce::String(entry.info.slotIndex + 1);
    g.drawFittedText(name, nameTop, juce::Justification::centredLeft, 1);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.setColour(MixCoachTheme::textMuted());
    g.drawText("Slot " + juce::String(entry.info.slotIndex + 1) +
               (entry.hasSignal ? "  |  recibiendo audio" : "  |  conectado"),
               nameArea, juce::Justification::centredLeft);

    // ─── Mini level bar con gradiente ───────────────────────────────────────
    float norm = juce::jlimit(0.0f, 1.0f, (entry.peakLeft + 60.0f) / 66.0f);

    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

    juce::Colour barColour;
    juce::Colour barGlow;
    if (entry.peakLeft > -6.0f) {
        barColour = MixCoachTheme::error();
        barGlow = MixCoachTheme::error().withAlpha(0.2f);
    } else if (entry.peakLeft > -12.0f) {
        barColour = MixCoachTheme::warning();
        barGlow = MixCoachTheme::warning().withAlpha(0.15f);
    } else if (entry.peakLeft > -18.0f) {
        barColour = MixCoachTheme::success();
        barGlow = MixCoachTheme::success().withAlpha(0.1f);
    } else {
        barColour = MixCoachTheme::meterBlue();
        barGlow = MixCoachTheme::meterBlue().withAlpha(0.1f);
    }

    auto fillBar = barArea.withRight(barArea.getX() + (int)(barArea.getWidth() * norm));
    if (fillBar.getWidth() > 1) {
        g.setColour(barColour);
        g.fillRoundedRectangle(fillBar.toFloat(), 2.0f);
        // Glow en la punta
        if (norm > 0.1f) {
            auto glowBar = fillBar.withLeft(fillBar.getRight() - juce::jmax(3, fillBar.getWidth() / 4));
            juce::ColourGradient barGlowGrad(
                juce::Colours::white.withAlpha(0.3f),
                (float)glowBar.getX(), 0.0f,
                juce::Colour(0x00000000),
                (float)glowBar.getRight(), 0.0f,
                false);
            g.setGradientFill(barGlowGrad);
            g.fillRect(glowBar);
        }
    }

    // ─── RMS value ─────────────────────────────────────────────────────────
    auto rmsArea = statsArea.removeFromTop(statsArea.getHeight() / 2);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.setColour(MixCoachTheme::textDim());
    g.drawText("RMS " + juce::String(entry.rmsAvg, 1) + " dB", rmsArea, juce::Justification::centredLeft);

    // ─── Peak value ────────────────────────────────────────────────────────
    g.setColour(barColour);
    g.drawText("PK  " + juce::String(entry.peakLeft, 1) + " dB", statsArea, juce::Justification::centredLeft);

    // ─── Routing status ────────────────────────────────────────────────────
    if (entry.info.bus != BusType::None) {
        int busIdx = static_cast<int>(entry.info.bus);
        g.setColour(getBusColour(busIdx).withAlpha(0.2f));
        g.fillRoundedRectangle(routeArea.toFloat(), 4.0f);
        g.setColour(getBusColour(busIdx));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
        g.drawFittedText("Ruta\n" + juce::String(busNames[busIdx]), routeArea, juce::Justification::centred, 2);
    } else {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawFittedText("Ruta\nSin bus", routeArea, juce::Justification::centred, 2);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MixCoachPanel Implementation
// ═══════════════════════════════════════════════════════════════════════════

MixCoachPanel::MixCoachPanel()
{
    // ─── Header ─────────────────────────────────────────────────────────────
    coachHeader_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\x9B MixCoach — Mentor Inteligente")), juce::dontSendNotification);
    coachHeader_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTitle)).boldened());
    coachHeader_.setJustificationType(juce::Justification::centredLeft);
    coachHeader_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(coachHeader_);

    // ─── Panel de Referencias ──────────────────────────────────────────────
    addAndMakeVisible(refPanel_);

    // ─── Messenger List (panel derecho) ────────────────────────────────────
    addAndMakeVisible(messengerList_);

    // ─── Divider bar ──────────────────────────────────────────────────────
    dividerBar_.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    addAndMakeVisible(dividerBar_);

    // ─── Chat History ──────────────────────────────────────────────────────
    chatHistory_.setMultiLine(true);
    chatHistory_.setReadOnly(true);
    chatHistory_.setScrollbarsShown(true);
    chatHistory_.setCaretVisible(false);
    chatHistory_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    chatHistory_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
    chatHistory_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatHistory_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border().withAlpha(0.3f));
    addAndMakeVisible(chatHistory_);

    // ─── Chat Input ────────────────────────────────────────────────────────
    chatInput_.setMultiLine(false);
    chatInput_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    chatInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
    chatInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
    chatInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
    chatInput_.setTextToShowWhenEmpty("Preguntale algo al mentor...", MixCoachTheme::textMuted());
    chatInput_.setIndents(8, 6);
    chatInput_.addListener(this);
    addAndMakeVisible(chatInput_);

    // ─── Status ─────────────────────────────────────────────────────────────
    statusLabel_.setText(juce::String(juce::CharPointer_UTF8("\xE2\x97\x89 Conectado  |  Fase: Gain Staging  |  IA activa")), juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    statusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(statusLabel_);
}

void MixCoachPanel::resized()
{
    auto area = getLocalBounds().reduced(6);

    // ─── Header ─────────────────────────────────────────────────────────────
    auto headerArea = area.removeFromTop(28);
    coachHeader_.setBounds(headerArea);

    // ─── Status bar ─────────────────────────────────────────────────────────
    auto statusArea = area.removeFromBottom(16);
    statusLabel_.setBounds(statusArea);

    // ─── Split vertical: Left (chat + refs) | Right (messenger list) ────────
    int splitX = (int)(area.getWidth() * 0.58f); // 58% chat, 42% messengers

    auto leftArea = area.removeFromLeft(splitX);
    auto rightArea = area.reduced(4, 0);

    // ─── Divider ───────────────────────────────────────────────────────────
    dividerBar_.setBounds(leftArea.getRight(), leftArea.getY(), 4, leftArea.getHeight());

    // ─── Right side: Messenger List ────────────────────────────────────────
    messengerList_.setBounds(rightArea);

    // ─── Left side: Referencias ─────────────────────────────────────────────
    bool hasRefs = refPanel_.getNumReferences() > 0;
    int refPanelHeight = hasRefs ? 130 : 108;
    auto refArea = leftArea.removeFromTop(refPanelHeight);
    refPanel_.setBounds(refArea);

    // ─── Left side: Chat Input ──────────────────────────────────────────────
    chatInput_.setBounds(leftArea.removeFromBottom(32));

    // ─── Left side: Chat History (ocupa el resto) ───────────────────────────
    chatHistory_.setBounds(leftArea);
}

void MixCoachPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Fondo principal
    g.fillAll(MixCoachTheme::bgDark());

    // Glass overlay
    juce::ColourGradient bgGrad(
        MixCoachTheme::glassHighlight(),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, (float)area.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(area);

    // Header bottom line
    g.setColour(MixCoachTheme::accent().withAlpha(0.3f));
    g.drawHorizontalLine(32, 6.0f, (float)(area.getWidth() - 6));

    // Split vertical line (divider visual)
    int splitX = (int)(area.getWidth() * 0.58f) + 6;
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.drawVerticalLine(splitX, 36.0f, (float)(area.getHeight() - 20));
}

void MixCoachPanel::updateMessengers(SlotRegistry& registry)
{
    messengerList_.updateMessengers(registry);
}

void MixCoachPanel::addMessage(const MentorMessage& msg)
{
    appendFormattedMessage(msg);
}

void MixCoachPanel::clearMessages()
{
    chatHistory_.clear();
}

void MixCoachPanel::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &chatInput_) {
        auto text = chatInput_.getText().trim();
        if (text.isNotEmpty() && onMessageSent) {
            onMessageSent(text);
            chatInput_.clear();
        }
    }
}

void MixCoachPanel::appendFormattedMessage(const MentorMessage& msg)
{
    juce::String prefix;
    juce::String typeTag;
    switch (msg.type) {
        case MentorMessage::Type::Tip:
            prefix = juce::CharPointer_UTF8("\xF0\x9F\x92\xA1 ");
            typeTag = "[TIP] ";
            break;
        case MentorMessage::Type::Warning:
            prefix = juce::CharPointer_UTF8("\xE2\x9A\xA0\xEF\xB8\x8F ");
            typeTag = "[!] ";
            break;
        case MentorMessage::Type::Achievement:
            prefix = juce::CharPointer_UTF8("\xF0\x9F\x8F\x86 ");
            typeTag = "[LOGRO] ";
            break;
        case MentorMessage::Type::Question:
            prefix = juce::CharPointer_UTF8("\xE2\x9D\x93 ");
            typeTag = "[?] ";
            break;
        default:
            prefix = juce::CharPointer_UTF8("\xF0\x9F\xA4\x96 ");
            typeTag = "[INFO] ";
            break;
    }

    if (!msg.context.empty()) {
        typeTag += "[" + juce::String(msg.context) + "] ";
    }

    chatHistory_.setCaretPosition(chatHistory_.getTotalNumChars());
    auto fullText = prefix + juce::String(msg.text) + "\n\n";
    chatHistory_.insertTextAtCaret(fullText);
    chatHistory_.moveCaretToEnd();
}

} // namespace mixcoach

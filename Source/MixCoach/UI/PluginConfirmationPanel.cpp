#include "PluginConfirmationPanel.h"

namespace mixcoach {

    PluginConfirmationPanel::PluginConfirmationPanel()
    {
        setOpaque(false);
        setVisible(false);

        // ─── Title ─────────────────────────────────────────────────────────
        titleLabel_.setText("[COACH] Insertos por Pista", juce::dontSendNotification);
        titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
        titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
        titleLabel_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel_);

        // ─── Close button (✕) ──────────────────────────────────────────────
        closeButton_.setText("\xC3\x97", juce::dontSendNotification);  // ×
        closeButton_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
        closeButton_.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        closeButton_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(closeButton_);

        // ─── Confirm button ────────────────────────────────────────────────
        confirmButton_.setButtonText("[DONE] Confirmar insertos");
        confirmButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.8f));
        // JUCE 8: TextButton no tiene textColourId. Usamos buttonColourId para background y
        // el LookAndFeel se encarga del color del texto autom\xC3\xA1ticamente.
        confirmButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.85f));
        confirmButton_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent());
        confirmButton_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent());
        confirmButton_.onClick = [this]() {
            if (onConfirm)
                onConfirm(getConfirmedInserts());
            setVisible(false);
        };
        addAndMakeVisible(confirmButton_);
    }

    void PluginConfirmationPanel::populate(
        const std::vector<std::tuple<int, juce::String, juce::String>>& slotData,
        const std::vector<juce::String>& pluginNames)
    {
        // Limpiar filas anteriores
        rows_.clear();

        for (const auto& [slotIndex, trackName, roleEmoji] : slotData) {
            auto row = std::make_unique<TrackRow>();
            row->slotIndex = slotIndex;

            // Track label: emoji + nombre
            juce::String labelText = roleEmoji + " " + trackName;
            if (roleEmoji.isEmpty() || roleEmoji == trackName)
                labelText = trackName;

            row->trackLabel.setText(labelText, juce::dontSendNotification);
            row->trackLabel.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
            row->trackLabel.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
            row->trackLabel.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(row->trackLabel);

            // Plugin ComboBox
            row->pluginCombo.setTextWhenNoChoicesAvailable("Ninguno");
            row->pluginCombo.setTextWhenNothingSelected("Seleccionar plugin...");
            row->pluginCombo.addItem("— Ninguno —", 1);
            row->pluginCombo.setSelectedId(1);

            // Poblar con plugins detectados (ordenados alfabéticamente)
            juce::StringArray sortedPlugins;
            for (const auto& name : pluginNames) {
                if (name.isNotEmpty())
                    sortedPlugins.add(name);
            }
            sortedPlugins.sort(true);

            int id = 2;
            for (const auto& name : sortedPlugins) {
                row->pluginCombo.addItem(name, id++);
            }

            row->pluginCombo.setColour(juce::ComboBox::backgroundColourId, MixCoachTheme::bgInput());
            row->pluginCombo.setColour(juce::ComboBox::textColourId, MixCoachTheme::textPrimary());
            row->pluginCombo.setColour(juce::ComboBox::arrowColourId, MixCoachTheme::accent());
            row->pluginCombo.setColour(juce::ComboBox::outlineColourId, MixCoachTheme::border());
            addAndMakeVisible(row->pluginCombo);

            rows_.push_back(std::move(row));
        }

        resized();
        repaint();
    }

    std::unordered_map<int, juce::String> PluginConfirmationPanel::getConfirmedInserts() const
    {
        std::unordered_map<int, juce::String> result;
        for (const auto& row : rows_) {
            int selectedId = row->pluginCombo.getSelectedId();
            if (selectedId > 1) {
                // Id 1 = "Ninguno", Id > 1 = plugin name
                juce::String pluginName = row->pluginCombo.getText();
                if (pluginName.isNotEmpty() && pluginName != "\xE2\x80\x94 Ninguno \xE2\x80\x94")
                    result[row->slotIndex] = pluginName;
            }
        }
        return result;
    }

    void PluginConfirmationPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(8);
        if (bounds.isEmpty()) return;

        // Header row: title + close button
        auto headerArea = bounds.removeFromTop(24);
        titleLabel_.setBounds(headerArea.removeFromLeft(headerArea.getWidth() - 30));
        closeButton_.setBounds(headerArea.removeFromRight(24));

        // Track rows + confirm button area
        int rowHeight = 28;
        int comboWidth = juce::jmax(180, bounds.getWidth() / 3);
        int labelWidth = bounds.getWidth() - comboWidth - 8;
        int y = bounds.getY();
        int spacing = 2;

        for (auto& row : rows_) {
            if (row == nullptr) continue;

            auto rowArea = juce::Rectangle<int>(bounds.getX(), y, bounds.getWidth(), rowHeight);
            row->bounds = rowArea;

            auto labelArea = rowArea.removeFromLeft(labelWidth);
            auto comboArea = rowArea.removeFromRight(comboWidth);

            row->trackLabel.setBounds(labelArea.reduced(2, 0));
            row->pluginCombo.setBounds(comboArea.reduced(2, 2));

            y += rowHeight + spacing;
        }

        // Confirm button at bottom
        int buttonY = bounds.getBottom() - 36;
        auto buttonArea = juce::Rectangle<int>(bounds.getCentreX() - 100, buttonY, 200, 28);
        confirmButton_.setBounds(buttonArea);
    }

    void PluginConfirmationPanel::paint(juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        const float radius = 8.0f;

        // ─── Sombra exterior ─────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(b.expanded(2.0f, 2.0f), radius + 1.0f);

        // ─── Fondo glass oscuro ──────────────────────────────────────────
        juce::ColourGradient bgGrad(
            MixCoachTheme::bgPanel().withAlpha(0.97f), b.getX(), b.getY(),
            MixCoachTheme::bgDark().withAlpha(0.95f), b.getX(), b.getBottom(), false);
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(b, radius);

        // ─── Borde ───────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::glassEdge().withAlpha(0.4f));
        g.drawRoundedRectangle(b, radius, 1.0f);

        // ─── Separador entre header y contenido ──────────────────────────
        auto headerBottom = (float)titleLabel_.getBottom() + 4.0f;
        g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
        g.drawHorizontalLine((int)headerBottom, (int)(b.getX() + 8), (int)(b.getRight() - 8));
    }

    void PluginConfirmationPanel::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();
        // Close button hit test
        if (closeButton_.getBoundsInParent().contains(pos)) {
            if (onDismiss) onDismiss();
            setVisible(false);
        }
    }

    void PluginConfirmationPanel::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();
        bool overClose = closeButton_.getBoundsInParent().contains(pos);
        if (overClose != closeHovered_) {
            closeHovered_ = overClose;
            closeButton_.setColour(juce::Label::textColourId,
                overClose ? MixCoachTheme::warning() : MixCoachTheme::textMuted());
            repaint();
        }
    }

} // namespace mixcoach

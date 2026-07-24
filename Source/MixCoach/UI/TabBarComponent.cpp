#include "TabBarComponent.h"
#include "../engine/PanelRevealManager.h"
#include <cmath>

namespace mixcoach {

    const TabBarComponent::TabInfo& TabBarComponent::getTabInfo(Tab t) noexcept
    {
        static const TabInfo kTabs[Count] = {
            { "Coach"   },
            { "Session" },
            { "Tools"   },
        };
        return kTabs[static_cast<int>(t)];
    }

    TabBarComponent::TabBarComponent()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        startTimerHz(60);
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void TabBarComponent::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(60);
        }
        // Nunca detener el timer — las transiciones de tab (highlight,
        // hover) deben seguir animándose aunque el componente no sea visible.
    }

    void TabBarComponent::resized() {}

    juce::Rectangle<int> TabBarComponent::getTabBounds(int index) const noexcept
    {
        if (index < 0 || index >= Count) return {};
        auto bounds = getLocalBounds();
        int tabWidth = bounds.getWidth() / Count;
        return { index * tabWidth, 0, tabWidth, bounds.getHeight() };
    }

    int TabBarComponent::tabAtPosition(juce::Point<int> pos) const noexcept
    {
        auto bounds = getLocalBounds();
        if (!bounds.contains(pos)) return -1;
        int tabWidth = bounds.getWidth() / Count;
        int index = pos.x / tabWidth;
        return (index >= 0 && index < Count) ? index : -1;
    }

    void TabBarComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();

        g.setColour(MixCoachTheme::bgDark().darker(0.92f));
        g.fillRect(bounds);

        g.setColour(MixCoachTheme::divider().withAlpha(0.30f));
        g.fillRect(0, bounds.getBottom() - 1, bounds.getWidth(), 1);

        auto glassTop = bounds.withHeight(4);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.025f),
                                       glassTop.getCentreX(), glassTop.getY(),
                                       juce::Colour(0x00000000),
                                       glassTop.getCentreX(), glassTop.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRect(glassTop);

        int tabWidth = bounds.getWidth() / Count;

        for (int i = 0; i < Count; ++i) {
            Tab tab = static_cast<Tab>(i);
            const auto& info = getTabInfo(tab);
            bool isActive = (i == static_cast<int>(activeTab_));
            float anim    = hoverAnim_[i];

            auto tabBounds = juce::Rectangle<int>(i * tabWidth, 0, tabWidth, bounds.getHeight());
            float cx = tabBounds.getCentreX();
            float cy = tabBounds.getCentreY();

            auto bgBounds = tabBounds.reduced(6, 4).toFloat();

            if (isActive) {
                juce::ColourGradient activeGrad(
                    MixCoachTheme::accent().withAlpha(0.15f),
                    bgBounds.getX(), bgBounds.getY(),
                    MixCoachTheme::accent().withAlpha(0.0f),
                    bgBounds.getRight(), bgBounds.getCentreY(), false);
                g.setGradientFill(activeGrad);
                g.fillRoundedRectangle(bgBounds, 8.0f);
            } else if (anim > 0.005f) {
                g.setColour(juce::Colours::white.withAlpha(0.035f * anim));
                g.fillRoundedRectangle(bgBounds, 8.0f);
            }

            if (isActive) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
                g.fillRoundedRectangle((float)(i * tabWidth) + 16.0f, (float)(bounds.getBottom() - 6),
                                       (float)(tabWidth - 32), 6.0f, 3.0f);
                g.setColour(MixCoachTheme::accentGlow());
                juce::Path indicator;
                indicator.addRoundedRectangle((float)(i * tabWidth) + 20.0f, (float)(bounds.getBottom() - 4),
                                              (float)(tabWidth - 40), 3.0f, 1.5f);
                g.fillPath(indicator);
            }

            float iconY = cy - 10.0f;
            auto iconArea = juce::Rectangle<float>(cx - 9.0f, iconY, 18.0f, 16.0f);
            juce::Colour iconColour;
            if (locked_[i])
                iconColour = MixCoachTheme::textMuted().withAlpha(0.15f);
            else if (isActive)
                iconColour = MixCoachTheme::accentGlow();
            else if (anim > 0.3f)
                iconColour = MixCoachTheme::textBright().withAlpha(0.7f + 0.2f * anim);
            else
                iconColour = MixCoachTheme::textMuted().withAlpha(0.45f);

            switch (tab) {
                case Coach:   drawCoachIcon(g, iconArea, iconColour); break;
                case Session: drawSessionIcon(g, iconArea, iconColour); break;
                case Tools:   drawToolsIcon(g, iconArea, iconColour); break;
                default: break;
            }

            float labelY = cy + 7.0f;
            auto labelArea = juce::Rectangle<float>((float)(i * tabWidth), labelY, (float)tabWidth, 11.0f);
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            if (locked_[i])
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.20f));
            else if (isActive)
                g.setColour(MixCoachTheme::accent().withAlpha(0.95f));
            else if (anim > 0.3f)
                g.setColour(MixCoachTheme::textPrimary().withAlpha(0.5f + 0.4f * anim));
            else
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.45f));
            g.drawText(juce::String(info.label), labelArea.toNearestInt(), juce::Justification::centredTop);

            // ─── Lock indicator for locked tabs ─────────────────────────
            if (locked_[i]) {
                // Small lock icon at top-right of the tab
                auto lockArea = juce::Rectangle<float>((float)(i * tabWidth + tabWidth - 20), (float)(cy - 12.0f), 10.0f, 10.0f);
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.30f));
                g.drawText(juce::CharPointer_UTF8("[EMPTY]"), lockArea, juce::Justification::centred);
            }

            // ─── NEW badge (pulsating) for recently unlocked tabs ────────
            if (newBadgeAnim_[i] > 0.01f) {
                float badgeAlpha = newBadgeAnim_[i];
                float t = juce::Time::getMillisecondCounter() * 0.001f;
                // Pulse: rápido al inicio (2Hz), más lento al final
                float pulseHz = 2.0f - newBadgeAnim_[i] * 0.8f;
                float pulse = 0.5f + 0.5f * std::sin(t * juce::MathConstants<float>::twoPi * pulseHz);
                float effectiveAlpha = badgeAlpha * (0.6f + 0.4f * pulse);

                // Badge area: top-right corner of the tab icon area
                auto badgeArea = juce::Rectangle<float>(
                    (float)(i * tabWidth + tabWidth - 50),
                    (float)(cy - 14.0f),
                    30.0f, 14.0f);

                // Glow behind the badge
                float glowSize = 8.0f + 6.0f * pulse;
                g.setColour(MixCoachTheme::accentCyan().withAlpha(effectiveAlpha * 0.15f));
                g.fillEllipse(badgeArea.getCentreX() - glowSize, badgeArea.getCentreY() - glowSize,
                              glowSize * 2.0f, glowSize * 2.0f);

                // Badge background
                g.setColour(MixCoachTheme::accentCyan().withAlpha(effectiveAlpha * 0.85f));
                g.fillRoundedRectangle(badgeArea, 7.0f);

                // Badge border glow
                g.setColour(MixCoachTheme::accentCyanBright().withAlpha(effectiveAlpha * 0.40f));
                g.drawRoundedRectangle(badgeArea, 7.0f, 1.0f);

                // "NEW" text
                g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
                g.setColour(juce::Colours::white.withAlpha(effectiveAlpha));
                g.drawText("NEW", badgeArea.toNearestInt(), juce::Justification::centred);
            }
        }

        // ─── Panels Menu button (☰) ────────────────────────────────────
        {
            int btnSize = 24;
            int btnX = bounds.getRight() - 44;
            int btnY = (bounds.getHeight() - btnSize) / 2;
            menuBtnBounds_ = {btnX, btnY, btnSize, btnSize};

            auto btnF = menuBtnBounds_.toFloat();

            // Hover glow
            if (menuBtnHovered_) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
                g.fillRoundedRectangle(btnF, 5.0f);
            }

            // Draw hamburger icon (3 lines)
            float cx = btnF.getCentreX();
            float cy = btnF.getCentreY();
            float lineW = 14.0f;
            float lineH = 2.0f;
            float gap = 3.5f;
            float startY = cy - gap;

            juce::Colour btnCol = menuBtnHovered_ ? MixCoachTheme::textBright() : MixCoachTheme::textMuted().withAlpha(0.55f);
            g.setColour(btnCol);

            for (int i = 0; i < 3; ++i) {
                float ly = startY + i * gap;
                auto line = juce::Rectangle<float>(cx - lineW * 0.5f, ly, lineW, lineH);
                g.fillRoundedRectangle(line, 1.0f);
            }

            // ─── NEW badge (pulsating) on menu button when panels were just revealed ──
            if (menuNewBadgeAnim_ > 0.01f) {
                float badgeAlpha = menuNewBadgeAnim_;
                float t = juce::Time::getMillisecondCounter() * 0.001f;
                // Pulse: rápido al inicio (2Hz), más lento al final
                float pulseHz = 2.0f - menuNewBadgeAnim_ * 0.8f;
                float pulse = 0.5f + 0.5f * std::sin(t * juce::MathConstants<float>::twoPi * pulseHz);
                float effectiveAlpha = badgeAlpha * (0.6f + 0.4f * pulse);

                // Badge area: to the LEFT of the hamburger icon (replaces version label)
                float badgeW = 28.0f;
                float badgeH = 13.0f;
                float badgeX = btnF.getX() - badgeW - 2.0f;
                float badgeY = btnF.getCentreY() - badgeH * 0.5f;
                auto badgeArea = juce::Rectangle<float>(badgeX, badgeY, badgeW, badgeH);

                // Glow behind the badge
                float glowSize = 10.0f + 8.0f * pulse;
                g.setColour(MixCoachTheme::accentCyan().withAlpha(effectiveAlpha * 0.12f));
                g.fillEllipse(badgeArea.getCentreX() - glowSize, badgeArea.getCentreY() - glowSize,
                              glowSize * 2.0f, glowSize * 2.0f);

                // Badge background
                g.setColour(MixCoachTheme::accentCyan().withAlpha(effectiveAlpha * 0.85f));
                g.fillRoundedRectangle(badgeArea, 6.5f);

                // Badge border glow
                g.setColour(MixCoachTheme::accentCyanBright().withAlpha(effectiveAlpha * 0.35f));
                g.drawRoundedRectangle(badgeArea, 6.5f, 1.0f);

                // "NEW" text
                g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
                g.setColour(juce::Colours::white.withAlpha(effectiveAlpha));
                g.drawText("NEW", badgeArea.toNearestInt(), juce::Justification::centred);
            }

            // ─── Badge count: cuántos paneles están desbloqueados ──────────
            if (revealManager_ != nullptr) {
                // Paneles contables (excluyendo Coach que siempre está activo)
                static const PanelId kCountablePanels[] = {
                    PanelId::Reference,
                    PanelId::Messengers,
                    PanelId::MixMap,
                    PanelId::Tools,
                    PanelId::Session,
                    PanelId::Report
                };
                int unlockedCount = 0;
                for (auto pid : kCountablePanels) {
                    if (revealManager_->isPanelRevealed(pid))
                        ++unlockedCount;
                }

                if (unlockedCount > 0) {
                    // Badge bg (cyan pill) at top-right of menu button
                    juce::String badgeText = juce::String(unlockedCount);
                    float badgeW = 16.0f;
                    float badgeH = 11.0f;
                    float badgeX = btnF.getRight() - badgeW + 2.0f;
                    float badgeY = btnF.getY() - 2.0f;
                    auto badgeArea = juce::Rectangle<float>(badgeX, badgeY, badgeW, badgeH);

                    // Badge background
                    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.85f));
                    g.fillRoundedRectangle(badgeArea, 5.5f);

                    // Badge text
                    g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
                    g.setColour(juce::Colours::white);
                    g.drawText(badgeText, badgeArea.toNearestInt(), juce::Justification::centred);
                }
            }
        }

        // ─── Version label (hidden while NEW badge is active) ───────────
        if (menuNewBadgeAnim_ <= 0.01f) {
            auto versionArea = juce::Rectangle<int>(menuBtnBounds_.getX() - 40, 0, 36, bounds.getHeight());
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.25f));
            g.drawText("v2.0", versionArea, juce::Justification::centred);
        }
    }

    void TabBarComponent::mouseDown(const juce::MouseEvent& e)
    {
        // ─── Check panels menu button ───────────────────────────────────
        if (menuBtnBounds_.contains(e.getPosition())) {
            clearMenuNewBadge(); // Dismiss NEW badge when user opens the menu
            showPanelsMenu();
            return;
        }

        // ─── Normal tab selection ───────────────────────────────────────
        int idx = tabAtPosition(e.getPosition());
        if (idx >= 0 && idx < Count) {
            Tab tab = static_cast<Tab>(idx);
            // Clear NEW badge when user clicks the tab
            clearNewBadge(tab);
            if (tab != activeTab_ && !locked_[idx]) {
                activeTab_ = tab;
                repaint();
                if (onTabSelected) onTabSelected(tab);
            }
        }
    }

    void TabBarComponent::mouseMove(const juce::MouseEvent& e)
    {
        int newHover = tabAtPosition(e.getPosition());
        bool newMenuHover = menuBtnBounds_.contains(e.getPosition());

        if (newHover != hoveredTab_ || newMenuHover != menuBtnHovered_) {
            hoveredTab_ = newHover;
            menuBtnHovered_ = newMenuHover;
            repaint();
        }

        if (newMenuHover)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor((newHover >= 0 && !locked_[newHover]) ? juce::MouseCursor::PointingHandCursor
                                         : juce::MouseCursor::NormalCursor);
    }

    void TabBarComponent::mouseExit(const juce::MouseEvent&)
    {
        if (hoveredTab_ >= 0 || menuBtnHovered_) {
            hoveredTab_ = -1;
            menuBtnHovered_ = false;
            repaint();
        }
    }

    void TabBarComponent::timerCallback()
    {
        bool changed = false;
        for (int i = 0; i < Count; ++i) {
            // ─── Hover animation ────────────────────────────────────────────
            if (locked_[i]) {
                if (hoverAnim_[i] != 0.0f) {
                    hoverAnim_[i] = 0.0f;
                    changed = true;
                }
            } else {
                float target = (i == hoveredTab_ || i == static_cast<int>(activeTab_)) ? 1.0f : 0.0f;
                float diff   = target - hoverAnim_[i];
                if (std::abs(diff) > 0.003f) {
                    hoverAnim_[i] += diff * 0.22f;
                    changed = true;
                } else if (hoverAnim_[i] != target) {
                    hoverAnim_[i] = target;
                    changed = true;
                }
            }

            // ─── NEW badge decay ────────────────────────────────────────────
            if (newBadgeAnim_[i] > 0.0f) {
                newBadgeAnim_[i] = juce::jmax(0.0f, newBadgeAnim_[i] - kNewBadgeDecayPerFrame);
                changed = true;
            }
        }

        // ─── Menu NEW badge decay ───────────────────────────────────────────
        if (menuNewBadgeAnim_ > 0.0f) {
            menuNewBadgeAnim_ = juce::jmax(0.0f, menuNewBadgeAnim_ - kNewBadgeDecayPerFrame);
            changed = true;
        }

        if (changed) repaint();
    }

    void TabBarComponent::drawCoachIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        g.setColour(colour);
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float w  = 16.0f;
        float h  = 11.0f;
        float x  = cx - w * 0.5f;
        float y  = cy - h * 0.5f;

        juce::Path bubble;
        bubble.addRoundedRectangle(x, y, w, h, 3.0f);

        juce::Path tail;
        tail.addTriangle(x + 2.0f, y + h,
                         x + 7.0f, y + h,
                         x + 7.0f, y + h + 4.0f);
        bubble.addPath(tail);
        g.fillPath(bubble);

        g.setColour(MixCoachTheme::bgDark());
        float dotR = 1.2f;
        float dotY = cy + 0.5f;
        float dotSpacing = 3.5f;
        float dotStartX = cx - dotSpacing;
        for (int i = 0; i < 3; ++i) {
            float dx = dotStartX + i * dotSpacing;
            g.fillEllipse(dx - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    void TabBarComponent::drawSessionIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        g.setColour(colour);
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float w  = 14.0f;
        float h  = 12.0f;
        float x  = cx - w * 0.5f;
        float y  = cy - h * 0.5f;

        float barH = 2.5f;
        float gap  = 2.0f;

        for (int i = 0; i < 3; ++i) {
            float by = y + i * (barH + gap);
            float bw = w - i * 2.5f;
            float bx = x + (w - bw) * 0.5f;
            juce::Path bar;
            bar.addRoundedRectangle(bx, by, bw, barH, 1.0f);
            g.fillPath(bar);
        }
        g.fillRect(x - 2.0f, y, 1.5f, h);
    }

    void TabBarComponent::drawToolsIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        g.setColour(colour);
        float cx    = bounds.getCentreX();
        float cy    = bounds.getCentreY();
        float barW  = 3.5f;
        float gap   = 3.0f;
        float total = 3.0f * barW + 2.0f * gap;
        float start = cx - total * 0.5f;
        float base  = cy + 6.0f;

        float heights[3] = { 5.0f, 11.0f, 8.0f };

        for (int i = 0; i < 3; ++i) {
            float bx   = start + i * (barW + gap);
            float barH = heights[i];
            juce::Path bar;
            bar.addRoundedRectangle(bx, base - barH, barW, barH, 0.8f);
            g.fillPath(bar);
        }
    }

// ═══════════════════════════════════════════════════════════════════════════
//  Constants for panels menu (defined once to avoid duplication)
// ═══════════════════════════════════════════════════════════════════════════
namespace {
    struct PanelMenuItem {
        const char* icon;
        const char* name;
        PanelId panelId;
        bool isTab;
        TabBarComponent::Tab tab;
    };

    static constexpr PanelMenuItem kPanelMenuItems[] = {
        { "\xF0\x9F\x92\xAC", "Coach",      PanelId::Coach,      false, TabBarComponent::Coach },
        { "\xF0\x9F\x93\x81", "Reference",   PanelId::Reference,  false, TabBarComponent::Coach },
        { "\xF0\x9F\x8E\x9A", "Messengers",  PanelId::Messengers, false, TabBarComponent::Coach },
        { "\xF0\x9F\x97\xBA", "Mix Map",     PanelId::MixMap,     false, TabBarComponent::Coach },
        { "[CHART]", "Analyzers",   PanelId::Tools,      true,  TabBarComponent::Tools },
        { "[TREND]", "Session",     PanelId::Session,    true,  TabBarComponent::Session },
        { "[NOTES]", "Report",      PanelId::Report,     false, TabBarComponent::Coach },
    };

    static constexpr int kNumPanelMenuItems =
        sizeof(kPanelMenuItems) / sizeof(kPanelMenuItems[0]);
}

// ═══════════════════════════════════════════════════════════════════════════
//  showPanelsMenu — Muestra menú desplegable con todos los paneles
// ═══════════════════════════════════════════════════════════════════════════
void TabBarComponent::showPanelsMenu()
{
    if (revealManager_ == nullptr) return;

    juce::PopupMenu menu;

    for (int i = 0; i < kNumPanelMenuItems; ++i) {
        const auto& item = kPanelMenuItems[i];
        bool revealed = revealManager_->isPanelRevealed(item.panelId);

        juce::String label = juce::String(item.icon) + " " + juce::String(item.name);

        if (item.panelId == PanelId::Coach || revealed) {
            bool ticked = (item.isTab && getActiveTab() == item.tab);
            menu.addItem(i + 1, label, true, ticked);
        } else {
            juce::String lockedLabel = label + "  \xF0\x9F\x94\x92";
            menu.addColouredItem(i + 1,
                                 lockedLabel,
                                 MixCoachTheme::textMuted().withAlpha(0.30f),
                                 false,
                                 false);
        }
    }

    // ─── Mostrar menú en coordenadas de pantalla ────────────────────────────
    auto screenArea = localAreaToGlobal(menuBtnBounds_)
                          .translated(0, getHeight())
                          .toFloat()
                          .toNearestInt();

    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withMinimumWidth(200)
                           .withMaximumNumColumns(1)
                           .withTargetScreenArea(screenArea),
                       [this](int result) {
                           clearMenuNewBadge(); // Dismiss NEW badge when user selects a panel
                           if (result <= 0) return;

                           int idx = result - 1;
                           if (idx < 0 || idx >= kNumPanelMenuItems) return;

                           const auto& selected = kPanelMenuItems[idx];

                           // Navegar al tab si aplica
                           if (selected.isTab && onTabSelected) {
                               onTabSelected(selected.tab);
                           }

                           // Notificar al NavigationShell
                           if (onPanelMenuSelected) {
                               onPanelMenuSelected(selected.panelId);
                           }
                       });
}

} // namespace mixcoach

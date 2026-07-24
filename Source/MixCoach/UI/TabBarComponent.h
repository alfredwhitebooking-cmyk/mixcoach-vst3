#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "MixCoachTheme.h"

namespace mixcoach {
    class PanelRevealManager;
    enum class PanelId : uint8_t;
} // namespace mixcoach

namespace mixcoach {

    class TabBarComponent : public juce::Component,
                             private juce::Timer
    {
    public:
        enum Tab
        {
            Coach = 0,
            Session,
            Tools,
            Count
        };

        TabBarComponent();
        ~TabBarComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        std::function<void(Tab tab)> onTabSelected;

        void setActiveTab(Tab tab) noexcept
        {
            if (tab != activeTab_) {
                hoverAnim_[static_cast<int>(activeTab_)] = 0.0f;
                activeTab_ = tab;
                hoverAnim_[static_cast<int>(tab)] = 1.0f;
                repaint();
            }
        }

        [[nodiscard]] Tab getActiveTab() const noexcept { return activeTab_; }

        static constexpr int kHeight = 36;

        // ═══ Panels Menu (Chat-Commanded UI) ════════════════════════════════
        /** Conecta el PanelRevealManager para consultar estado de revelación. */
        void setRevealManager(PanelRevealManager* manager) noexcept { revealManager_ = manager; }

        /** Callback cuando el usuario selecciona un panel del menú. */
        std::function<void(PanelId panelId)> onPanelMenuSelected;

        /** Retorna bounds del botón menú (para que NavigationShell posicione tooltips). */
        [[nodiscard]] juce::Rectangle<int> getMenuButtonBounds() const noexcept { return menuBtnBounds_; }

    private:
        // ─── Menu button ────────────────────────────────────────────────────
        juce::Rectangle<int> menuBtnBounds_;
        bool menuBtnHovered_ = false;
        PanelRevealManager* revealManager_ = nullptr;

        /** Construye y muestra el PopupMenu de paneles. */
        void showPanelsMenu();

    private:
        Tab activeTab_ = Coach;
        int hoveredTab_ = -1;

        struct TabInfo
        {
            const char* label;
        };

        static const TabInfo& getTabInfo(Tab t) noexcept;
        juce::Rectangle<int> getTabBounds(int index) const noexcept;
        int tabAtPosition(juce::Point<int> pos) const noexcept;

        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        static void drawCoachIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
        static void drawSessionIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
        static void drawToolsIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);

        float hoverAnim_[Count] = { 0.0f, 0.0f, 0.0f };
        bool locked_[Count] = { false, true, true }; // Session & Tools locked by default

        // ═══ NEW badge animation state ═══════════════════════════════════════
        float newBadgeAnim_[Count] = { 0.0f, 0.0f, 0.0f }; // 1.0 = full visibility, decays to 0.0
        static constexpr float kNewBadgeDurationSec = 3.0f; // Badge pulses for ~3 seconds
        static constexpr float kNewBadgeDecayPerFrame = 1.0f / (kNewBadgeDurationSec * 60.0f);

        // ═══ Menu button NEW badge animation ═════════════════════════════════
        float menuNewBadgeAnim_ = 0.0f; // 1.0 = full visibility, decays to 0.0

    public:
        /** Dispara la animación 'NEW' en el botón ☰ (pulsa + glow + fadeout en 5s). */
        void triggerMenuNewBadge() noexcept
        {
            menuNewBadgeAnim_ = 1.0f;
            repaint();
        }

        /** Limpia el badge NEW del botón ☰ inmediatamente. */
        void clearMenuNewBadge() noexcept
        {
            menuNewBadgeAnim_ = 0.0f;
            repaint();
        }

        /** Dispara la animación 'NEW' en un tab (pulsa + glow + fadeout en 5s). */
        void triggerNewBadge(Tab tab) noexcept
        {
            if (tab >= 0 && tab < Count) {
                newBadgeAnim_[static_cast<int>(tab)] = 1.0f;
                repaint();
            }
        }

        /** Limpia el badge NEW de un tab (cuando el usuario hace clic en él). */
        void clearNewBadge(Tab tab) noexcept
        {
            if (tab >= 0 && tab < Count) {
                newBadgeAnim_[static_cast<int>(tab)] = 0.0f;
                repaint();
            }
        }

    private:

    public:
        /** Lock/unlock a tab. Locked tabs are dimmed and not clickable. */
        void setTabLocked(Tab tab, bool locked) noexcept
        {
            if (locked_[static_cast<int>(tab)] != locked) {
                locked_[static_cast<int>(tab)] = locked;
                repaint();
            }
        }

        /** Check if a tab is locked. */
        [[nodiscard]] bool isTabLocked(Tab tab) const noexcept
        {
            return locked_[static_cast<int>(tab)];
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabBarComponent)
    };

} // namespace mixcoach

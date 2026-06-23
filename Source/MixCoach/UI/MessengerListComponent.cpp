#include "MessengerListComponent.h"
#include "MessengerListAdvice.h"
#include "MessengerListRoles.h"
#include "../engine/CoachEngine.h"
#include "../engine/TrackFeedCore.h"
#include "../engine/TrackState.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

namespace mixcoach {

    // ─── Constants ────────────────────────────────────────────────────────────
    static constexpr float kMeterMinDb    = -60.0f;
    static constexpr float kMeterMaxDb    = 0.0f;
    static constexpr float kGreenZoneEnd  = -18.0f;
    static constexpr float kYellowZoneEnd = -6.0f;
    static constexpr float kRedZoneEnd    = 0.0f;

    // ─── Static persistent data ───────────────────────────────────────────────
    std::array<MessengerEntry, SlotRegistry::kMaxSlots> MessengerListComponent::s_persistentData_{};
    std::array<BusGroup, kNumBuses + 1> MessengerListComponent::s_persistentGroups_{};
    int MessengerListComponent::s_persistentCount_{0};
    bool MessengerListComponent::s_persistentReady_{false};

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor / Destructor
    // ═══════════════════════════════════════════════════════════════════════════

    MessengerListComponent::MessengerListComponent()
    {
        emptyLabel_.setText(juce::CharPointer_UTF8("No tracks detected. Insert Messenger on your mixer tracks."),
                            juce::dontSendNotification);
        emptyLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
        emptyLabel_.setJustificationType(juce::Justification::centred);
        emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
        addAndMakeVisible(emptyLabel_);

        startTimerHz(120);

        if (s_persistentReady_) restoreFromPersistent();
    }

    MessengerListComponent::~MessengerListComponent()
    {
        stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Render loop independent a 120fps
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerListComponent::timerCallback()
    {
        if (activeMessengerCount_ <= 0) return;

        if (!isPaused_) smoothMeters();

        repaint();
        if (auto* parent = getParentComponent()) parent->repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Mouse / Hit Test
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerListComponent::mouseDown(const juce::MouseEvent& e)
    {
        if (activeMessengerCount_ == 0) return;

        // ═══ SPRINT 5: Health pill click — toggle filter ═══════════════
        auto& pill = lastHealthPillBounds_;
        if (pill.valid) {
            if (pill.critical.contains(e.getPosition())) {
                setHealthFilter(healthFilter_ == HealthFilter::Critical ? HealthFilter::None : HealthFilter::Critical);
                return;
            }
            if (pill.warning.contains(e.getPosition())) {
                setHealthFilter(healthFilter_ == HealthFilter::Warning ? HealthFilter::None : HealthFilter::Warning);
                return;
            }
            if (pill.clean.contains(e.getPosition())) {
                setHealthFilter(healthFilter_ == HealthFilter::Clean ? HealthFilter::None : HealthFilter::Clean);
                return;
            }
            if (pill.silent.contains(e.getPosition())) {
                setHealthFilter(healthFilter_ == HealthFilter::Silent ? HealthFilter::None : HealthFilter::Silent);
                return;
            }
        }

        // ═══ SPRINT 5: Banner collapse/expand click ════════════════════
        if (bannerCollapseBtn_.contains(e.getPosition())) {
            bannerCollapsed_ = !bannerCollapsed_;
            repaint();
            return;
        }

        // === SPRINT 7: Banner event click ===
        if (!bannerEventBounds_.empty() && !bannerCollapsed_) {
            for (int ei = 0; ei < (int)bannerEventBounds_.size(); ++ei) {
                if (bannerEventBounds_[ei].contains(e.getPosition())) {
                    clickedEventIdx_ = (clickedEventIdx_ == ei) ? -1 : ei;
                    if (clickedEventIdx_ >= 0) clickedMousePos_ = e.getPosition();
                    repaint();
                    return;
                }
            }
            // Clicked outside events = dismiss popup
            if (clickedEventIdx_ >= 0) {
                clickedEventIdx_ = -1;
                repaint();
                // Continue to hitTestSlot below
            }
        }

        int slot = hitTestSlot(e.getPosition());
        if (slot < 0) return;

        // Check role pill / pending-confirmation badge click
        auto roleRect = getRolePillBounds(slot);
        if (roleRect.contains(e.getPosition()) && coachEngine_ != nullptr) {
            // ═══ Pending badge click — quick confirm without menu ═══
            // Sprint 1: now covers ANY pending inferred role, not just signal-order.
            bool hasRole =
                (messengers_[slot].trackRole != TrackRole::Unknown && messengers_[slot].trackRole != TrackRole::Master);
            bool isPending = messengers_[slot].signalOrderPending || messengers_[slot].roleInferredPending;
            if (hasRole && isPending) {
                // Check if click is on the badge (right 14px of the pill area)
                auto badgeRect =
                    juce::Rectangle<int>(roleRect.getRight() - 14, roleRect.getY(), 14, roleRect.getHeight());
                if (badgeRect.contains(e.getPosition())) {
                    // Quick confirm via the generalized API (covers all 3 inference origins)
                    coachEngine_->confirmRole(slot);
                    messengers_[slot].signalOrderPending  = false;
                    messengers_[slot].roleInferredPending = false;
                    repaint();
                    if (onRolesConfirmed) onRolesConfirmed();
                    return;
                }
            }

            // Normal role menu
            auto menu       = buildRoleMenu();
            int currentSlot = slot;

            menu.showMenuAsync(juce::PopupMenu::Options(), [this, currentSlot](int result) {
                TrackRole newRole = (result > 0) ? roleFromMenuId(result) : TrackRole::Unknown;

                if (currentSlot >= 0 && currentSlot < SlotRegistry::kMaxSlots)
                    messengers_[currentSlot].trackRole = newRole;

                if (coachEngine_ != nullptr) coachEngine_->setTrackRoleWithLearning(currentSlot, newRole);

                repaint();

                if (onSlotSelected) onSlotSelected(currentSlot);
            });
            return;
        }

        // Normal slot selection
        if (slot != selectedSlot_) {
            selectedSlot_ = slot;
            hoverGlow_.setTargetValue(0.0f);
            repaint();
            if (onSlotSelected) onSlotSelected(slot);
        }
    }

    void MessengerListComponent::mouseMove(const juce::MouseEvent& e)
    {
        int slot = hitTestSlot(e.getPosition());
        if (slot != hoveredSlot_) {
            hoveredSlot_ = slot;
            hoverGlow_.setTargetValue(slot >= 0 ? 1.0f : 0.0f);
            repaint();
        }

        // ═══ SPRINT 5: Health dot tooltip detection ═════════════════════
        if (slot >= 0 && slot < SlotRegistry::kMaxSlots) {
            auto& entry = messengers_[slot];
            if (entry.hasSignal && entry.trackHealth != TrackHealth::Unknown) {
                // Check if mouse is over the health dot area
                auto area      = getLocalBounds().reduced(2, 4);
                int circleSize = 10;
                int circleX    = area.getX() + 6;
                int nameX      = circleX + circleSize + 8;
                int hDotSize   = 8;
                int hDotX      = nameX;
                // Find the Y position of this slot
                int y = findSlotY(slot);
                if (y >= 0) {
                    auto hDotBounds = juce::Rectangle<int>(
                        hDotX - 2, y + (kCardHeight - hDotSize) / 2 - 2, hDotSize + 4, hDotSize + 4);
                    if (hDotBounds.contains(e.getPosition())) {
                        if (tooltipSlot_ != slot) {
                            tooltipSlot_ = slot;
                            repaint();
                        }
                        return;
                    }
                }
            }
        }

        // No health dot hovered
        if (tooltipSlot_ >= 0) {
            tooltipSlot_ = -1;
            repaint();
        }
    }

    void MessengerListComponent::mouseExit(const juce::MouseEvent& e)
    {
        juce::ignoreUnused(e);
        hoveredSlot_ = -1;
        hoverGlow_.setTargetValue(0.0f);
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Selection / Collapse / Grouping
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerListComponent::setSelectedSlot(int slotIndex)
    {
        if (selectedSlot_ == slotIndex) return;
        selectedSlot_ = slotIndex;
        repaint();
    }

    void MessengerListComponent::collapseAll()
    {
        for (auto& collapsed : collapsedGroups_) collapsed = true;
        repaint();
    }

    void MessengerListComponent::expandAll()
    {
        for (auto& collapsed : collapsedGroups_) collapsed = false;
        repaint();
    }

    void MessengerListComponent::resized()
    {
        auto area = getLocalBounds().reduced(2, 4);
        emptyLabel_.setBounds(area);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Visibility / Grouping
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerListComponent::visibilityChanged()
    {
        isPaused_ = !isShowing();
        if (!isPaused_ && s_persistentReady_) repaint();
    }

    void MessengerListComponent::setGroupingMode(GroupingMode mode)
    {
        if (groupingMode_ == mode) return;
        groupingMode_ = mode;
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateMessengers — Full initialization (one-shot)
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerListComponent::updateMessengers(SlotRegistry& registry, SharedData& sharedData)
    {
        bool anyData = false;
        syncTelemetryFromRegistry(registry, anyData, sharedData);

        if (s_persistentReady_) return;

        if (!anyData) {
            repaint();
            return;
        }

        for (auto& group : busGroups_) group.count = 0;
        activeMessengerCount_ = 0;

        for (auto& entry : messengers_) entry = MessengerEntry{};

        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            auto& entry = messengers_[idx];
            entry.info  = info;

            TrackAudioResult audioResult = sharedData.getTrackAudioResult(idx);

            float newPeakCombined = kMeterMinDb;
            float newRmsCombined  = kMeterMinDb;
            float newPeakL = kMeterMinDb, newPeakR = kMeterMinDb;
            bool hasSignal = false;

            if (audioResult.timestampUs > 0) {
                newPeakCombined = audioResult.getPeakCombined();
                newRmsCombined  = audioResult.getRmsCombined();
                newPeakL        = audioResult.peakLeft;
                newPeakR        = audioResult.peakRight;
                hasSignal       = (audioResult.getPeakCombined() > -60.0f);
            }

            entry.peakLeft       = newPeakL;
            entry.peakRight      = newPeakR;
            entry.barLevel       = newPeakCombined;
            entry.peakHold       = newPeakCombined;
            entry.peakHoldTimeMs = juce::Time::getMillisecondCounter();
            entry.peakHoldAlpha  = 1.0f;
            entry.rmsAvg         = newRmsCombined;
            entry.rmsSmooth      = newRmsCombined;
            entry.hasSignal      = hasSignal;
            {
                auto suggestion        = analyzeTrackSuggestion(newPeakCombined, newRmsCombined, hasSignal, info);
                entry.aiSuggestion     = suggestion.text;
                entry.suggestionStatus = suggestion.status;
            }

            ++activeMessengerCount_;

            int busIdx = static_cast<int>(info.bus);
            if (busIdx < 0) busIdx = kNumBuses;
            auto& group = busGroups_[busIdx];
            if (group.count < SlotRegistry::kMaxSlots) group.slotIndices[group.count++] = idx;
        });

        if (activeMessengerCount_ > 0) {
            uint32_t now = juce::Time::getMillisecondCounter();
            for (auto& entry : messengers_) {
                if (entry.info.active && entry.fadeAlpha >= 1.0f) {
                    entry.fadeAlpha   = 0.0f;
                    entry.fadeStartMs = now;
                }
            }

            s_persistentData_   = messengers_;
            s_persistentGroups_ = busGroups_;
            s_persistentCount_  = activeMessengerCount_;
            s_persistentReady_  = true;
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Role helpers — buildRoleMenu delegates to Roles.cpp free function
    // ═══════════════════════════════════════════════════════════════════════════

    juce::PopupMenu MessengerListComponent::buildRoleMenu()
    {
        return buildRoleMenuStatic();
    }

    juce::Rectangle<int> MessengerListComponent::getRolePillBounds(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return {};

        auto area = getLocalBounds().reduced(2, 4);
        int y     = area.getY();

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            y += kHeaderHeight;

            if (!collapsedGroups_[busIdx]) {
                for (int r = 0; r < group.count; ++r) {
                    int idx = group.slotIndices[r];
                    if (idx == slotIndex) {
                        int cardX = area.getX();
                        int cardY = y;
                        int cardW = area.getWidth();
                        int cardH = kCardHeight;

                        int routeX    = cardX + cardW - 34;
                        bool hasBadge = (messengers_[slotIndex].signalOrderPending
                                         && messengers_[slotIndex].trackRole != TrackRole::Unknown);
                        int offset    = hasBadge ? 48 : 32;
                        int roleX     = routeX - offset;
                        int pillW     = hasBadge ? 46 : 32;
                        return {roleX, cardY + cardH - 14, pillW, 12};
                    }
                    y += kCardHeight;
                }
            }
            y += 4;
        }
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SPRINT 5: updateTopEvents — Obtiene top eventos globales del TrackFeedCore
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerListComponent::updateTopEvents(TrackFeedCore& feed)
    {
        topEvents_ = feed.getGlobalEvents(5);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SPRINT 5: isFilteredOut — Verifica si un slot debe filtrarse
    // ═══════════════════════════════════════════════════════════════════════════
    bool MessengerListComponent::isFilteredOut(int slotIndex) const
    {
        if (healthFilter_ == HealthFilter::None || slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return false;

        if (!messengers_[slotIndex].info.active) return true;

        auto health = messengers_[slotIndex].trackHealth;

        switch (healthFilter_) {
            case HealthFilter::Critical:
                return !(health == TrackHealth::ClippingRisk || health == TrackHealth::Overcompressed
                         || health == TrackHealth::StereoCollapse);
            case HealthFilter::Warning:
                return !(health == TrackHealth::NeedsEQ || health == TrackHealth::NeedsCompression
                         || health == TrackHealth::MaskingIssue || health == TrackHealth::PhaseIssue);
            case HealthFilter::Clean:
                return health != TrackHealth::Clean;
            case HealthFilter::Silent:
                return !(health == TrackHealth::LowSignal || health == TrackHealth::Silent
                         || !messengers_[slotIndex].hasSignal);
            default:
                return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SPRINT 5: findSlotY — Encuentra la posición Y de un slot en la lista
    // ═══════════════════════════════════════════════════════════════════════════
    int MessengerListComponent::findSlotY(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return -1;

        auto area = getLocalBounds().reduced(2, 4);

        // Account for health summary bar + banner
        area.removeFromTop(kTitleHeight); // health summary
        area.removeFromTop(2);
        area.removeFromTop(kTitleHeight); // TrackFeed banner
        area.removeFromTop(2);

        int y = area.getY();

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            y += kHeaderHeight;

            if (!collapsedGroups_[busIdx]) {
                for (int r = 0; r < group.count; ++r) {
                    int idx = group.slotIndices[r];
                    if (idx == slotIndex) return y;
                    y += kCardHeight;
                }
            }
            y += 4;
        }
        return -1;
    }

} // namespace mixcoach

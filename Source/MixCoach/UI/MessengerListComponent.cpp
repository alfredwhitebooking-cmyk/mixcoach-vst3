#include "MessengerListComponent.h"
#include "../../Common/types/Constants.h"
#include <cmath>

namespace mixcoach {

// --- Constantes de la barra de nivel tipo DAW ---------------------------------
static constexpr float kMeterMinDb      = -60.0f;
static constexpr float kMeterMaxDb      =   0.0f;
static constexpr float kGreenZoneEnd    = -18.0f;
static constexpr float kYellowZoneEnd   =  -6.0f;
static constexpr float kRedZoneEnd      =   0.0f;
static constexpr float kPeakHoldDecayDbPerSec = 30.0f;
static constexpr float kMinBarLevel     = -60.0f;

// --- Colores fijos para track cards ------------------------------------------
static const juce::Colour kCardBg       = juce::Colour(0xFF1C1D2A);
static const juce::Colour kCardBorder   = juce::Colour(0xFF2E2F3E);
static const juce::Colour kBarBg        = juce::Colour(0xFF0E0F18);
static const juce::Colour kBarBorder    = juce::Colour(0xFF25262E);
static const juce::Colour kMeterGreen   = juce::Colour(0xFF22C55E);
static const juce::Colour kMeterLime    = juce::Colour(0xFF84CC16);
static const juce::Colour kMeterYellow  = juce::Colour(0xFFEAB308);
static const juce::Colour kMeterOrange  = juce::Colour(0xFFF97316);
static const juce::Colour kMeterRed     = juce::Colour(0xFFEF4444);
static const juce::Colour kPkTriangle   = juce::Colour(0xFFFFFFFF);
static const juce::Colour kTextBright   = juce::Colour(0xFFF1F1F6);
static const juce::Colour kTextDim      = juce::Colour(0xFF8B8FA3);
static const juce::Colour kTextMuted    = juce::Colour(0xFF5C5F73);

// --- Decaimiento fijo para RMS (slower, smoother) -----------------------------
static constexpr float kRmsReleaseCoeff = 0.06f;  // RMS release (slower, smoother)

// --- Datos estaticos PERSISTENTES (sobreviven recreacion del editor) ----------
std::array<MessengerEntry, SlotRegistry::kMaxSlots> MessengerListComponent::s_persistentData_{};
std::array<BusGroup, kNumBuses + 1>                MessengerListComponent::s_persistentGroups_{};
int  MessengerListComponent::s_persistentCount_{0};
bool MessengerListComponent::s_persistentReady_{false};

// --- Generador de sugerencias IA placeholder basadas en tipo de bus ----------
static juce::String generateAISuggestion(const SlotInfo& info, float rmsAvg)
{
    juce::ignoreUnused(rmsAvg);
    
    switch (info.bus)
    {
        case BusType::Drums:
            return "Sube 1.0 dB en 60 Hz";
        case BusType::Bass:
            return "Recorta 2.0 dB en 250 Hz";
        case BusType::Guitars:
            return "Recorta 3.0 dB en 3.5 kHz";
        case BusType::Keys:
            return "Sube 1.5 dB en 120 Hz";
        case BusType::Vocals:
            return "Sube 0.5 dB en 4 kHz";
        case BusType::FX:
            return "Recorta 1.0 dB en 8 kHz";
        default:
            return {};
    }
}

// ===== MessengerListComponent Implementation =================================

MessengerListComponent::MessengerListComponent()
{
    emptyLabel_.setText(juce::CharPointer_UTF8("No tracks detected. Insert Messenger on your mixer tracks."), juce::dontSendNotification);
    emptyLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(emptyLabel_);

    // ═══ Timer interno a 120fps — Render Loop INDEPENDIENTE ═════════════
    // Este es el RENDER LOOP del meter de audio. NO depende del editor
    // timer, de resize events, ni de la cadena de componentes.
    //
    //   • timerCallback() → repaint() + parent->repaint() a 120fps
    //   • smoothMeters()  → fixed dB/frame decay (independiente del target)
    //   • syncTelemetryFromRegistry() → attack instantáneo cuando hay datos
    //
    // Las barras NUNCA se congelan porque el decaimiento es CONSTANTE por
    // frame, no depende de que rawPeak cambie entre frames.
    startTimerHz(120);

    if (s_persistentReady_)
        restoreFromPersistent();
}

MessengerListComponent::~MessengerListComponent()
{
    stopTimer();
}

// ═══ timerCallback — Render Loop INDEPENDIENTE a 120fps ═══════════════════
//  Este timer es el RENDER LOOP del meter de audio. Es completamente
//  independiente del editor timer, resize events, y la cadena de componentes.
//
//  1. smoothMeters()  → fixed dB/frame decay + peak hold (NUNCA se congela)
//  2. repaint()        → dibuja la barra con el nivel actualizado
//  3. parent->repaint()→ asegura que el Viewport también se repinte
//
//  El editor timer (60fps) SOLO se encarga de syncTelemetryFromRegistry()
//  para el attack instantáneo. El decay y el repaint son 100% autonomos.
void MessengerListComponent::timerCallback()
{
    if (activeMessengerCount_ <= 0)
        return;

    // ═══ smoothMeters() solo cuando NO está pausado ═══════════════════
    // Si isPaused_ es true, skip smoothMeters() pero SIGUE repintando.
    // Esto es CRÍTICO porque isPaused_ puede quedar true si visibilityChanged()
    // se disparó antes de que el componente estuviera completamente en el
    // árbol de visibilidad. Al repintar siempre, las cards aparecen aunque
    // el fade-in animation esté congelado.
    if (!isPaused_)
        smoothMeters();

    // ═══ Repaint SIEMPRE a 120fps (independiente de isPaused_) ═══════════
    // Así los messengers nunca desaparecen aunque isPaused_ esté trabado.
    repaint();
    if (auto* parent = getParentComponent())
        parent->repaint();
}

// ===== mouseDown - Hit detection sobre track cards para seleccion =============
void MessengerListComponent::mouseDown(const juce::MouseEvent& e)
{
    if (activeMessengerCount_ == 0)
        return;

    auto area = getLocalBounds().reduced(2, 4);
    int y = area.getY();

    for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx)
    {
        auto& group = busGroups_[busIdx];
        if (group.count == 0) continue;

        y += kHeaderHeight;

        if (!collapsedGroups_[busIdx])
        {
            for (int r = 0; r < group.count; ++r)
            {
                int slotIdx = group.slotIndices[r];
                auto cardBounds = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kCardHeight);

                if (cardBounds.contains(e.getPosition()))
                {
                    int newSlot = messengers_[slotIdx].info.slotIndex;
                    if (newSlot != selectedSlot_)
                    {
                        selectedSlot_ = newSlot;
                        repaint();
                        if (onSlotSelected)
                            onSlotSelected(newSlot);
                    }
                    return;
                }

                y += kCardHeight;
            }
        }

        y += 4;
    }
}

// ===== setSelectedSlot ========================================================
void MessengerListComponent::setSelectedSlot(int slotIndex)
{
    if (selectedSlot_ == slotIndex)
        return;
    selectedSlot_ = slotIndex;
    repaint();
}

// ===== collapseAll / expandAll ================================================
void MessengerListComponent::collapseAll()
{
    for (auto& collapsed : collapsedGroups_)
        collapsed = true;
    repaint();
}

void MessengerListComponent::expandAll()
{
    for (auto& collapsed : collapsedGroups_)
        collapsed = false;
    repaint();
}

void MessengerListComponent::resized()
{
    auto area = getLocalBounds().reduced(2, 4);
    emptyLabel_.setBounds(area);
}

// --- Restaurar desde datos estaticos ------------------------------------------
void MessengerListComponent::restoreFromPersistent()
{
    messengers_         = s_persistentData_;
    busGroups_          = s_persistentGroups_;
    activeMessengerCount_ = s_persistentCount_;

    for (int i = 0; i < SlotRegistry::kMaxSlots; ++i)
    {
        auto& entry = messengers_[i];
        auto& cache = telemetryCache_[i];

        if (entry.peakLeft > -90.0f || entry.peakRight > -90.0f)
        {
            float pk = juce::jmax(entry.peakLeft, entry.peakRight, kMinBarLevel);
            entry.barLevel = pk;

            cache.peakLeft  = entry.peakLeft;
            cache.peakRight = entry.peakRight;
            cache.rmsAvg    = entry.rmsAvg;
            cache.hasSignal = entry.hasSignal;
            cache.active    = true;

            entry.fadeAlpha = 1.0f;
        }
    }

    repaint();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  syncTelemetryFromRegistry — Attack instantáneo (SIN release)
//  
//  CRÍTICO: El release/decaimiento NO se hace aquí. Lo maneja smoothMeters()
//  con decaimiento FIJO por frame (kDecayDbPerFrame), independiente del target.
//  Así las barras SIEMPRE se mueven aunque los datos de telemetría no cambien
//  entre frames (lo que pasa cuando el background thread actualiza cada 500ms).
//
//    Attack:  directo (sin suavizado)
//      if (rawPeak > barLevel) barLevel = rawPeak
//
//    Release: NINGUNO — smoothMeters() aplica decaimiento fijo hacia -infinito
//      barLevel -= kDecayDbPerFrame  (0.5 dB/frame ≈ 30 dB/sec)
//
//  Cuando el background thread trae un nuevo peak más alto, el attack
//  instantáneo sube la barra inmediatamente. El decaimiento constante
//  entre frames crea movimiento fluido y continuo, como un VU meter real.
// ═══════════════════════════════════════════════════════════════════════════════
void MessengerListComponent::syncTelemetryFromRegistry(SlotRegistry& registry, bool& anyDataOut)
{
    anyDataOut = false;
    registry.forEachActive([&](const SlotInfo& info)
    {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
        anyDataOut = true;

        auto& telem = registry.getTelemetry(idx);
        auto latest = telem.latest();

        auto& entry = messengers_[idx];

        // ═══ FIX: Sincronizar metadatos (nombre, color, bus) en tiempo real ═══
        // syncTelemetryFromRegistry() se llama a 60fps pero NUNCA actualizaba
        // entry.info, solo los niveles de audio. updateMessengers() sí copiaba
        // info pero solo se llamaba UNA VEZ al inicio (cuando bgHasNewResults_).
        // Esto provocaba que cambios de nombre/color/ruta desde el Messenger
        // nunca se reflejaran en la lista de MixCoach hasta reiniciar.
        entry.info = info;

        entry.peakLeft  = latest.peakLeft;
        entry.peakRight = latest.peakRight;

        float rmsAvg = (latest.rmsLeft + latest.rmsRight) * 0.5f;
        entry.rmsAvg = rmsAvg;
        entry.hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);

        // ═══ Attack instantáneo (SIN release — lo maneja smoothMeters) ══════
        // Si el nuevo peak es más alto que el nivel actual, subimos
        // instantáneamente. El decaimiento hacia -infinito se aplica en
        // smoothMeters() con dB/frame fijo, independiente del target.
        float rawPeak = juce::jmax(latest.peakLeft, latest.peakRight, kMinBarLevel);
        
        if (rawPeak > entry.barLevel)
            entry.barLevel = rawPeak;  // Attack: directo, instantáneo
        // NO hay release aquí — smoothMeters() se encarga del decaimiento
        // fijo, creando movimiento continuo aunque rawPeak no cambie.
        
        // RMS smoothing (exponencial suave para promedio)
        if (rmsAvg > entry.rmsSmooth)
            entry.rmsSmooth = rmsAvg;
        else
            entry.rmsSmooth += (rmsAvg - entry.rmsSmooth) * kRmsReleaseCoeff;

        // ═══ Peak hold ══════════════════════════════════════════════════════
        uint32_t now = juce::Time::getMillisecondCounter();
        if (rawPeak > entry.peakHold)
        {
            entry.peakHold = rawPeak;
            entry.peakHoldTimeMs = now;
            entry.peakHoldAlpha = 1.0f;
        }
        else if ((now - entry.peakHoldTimeMs) > 500)
        {
            float elapsed = (now - entry.peakHoldTimeMs - 500) * 0.001f;
            float decay = kPeakHoldDecayDbPerSec * elapsed;
            entry.peakHold = juce::jmax(rawPeak, entry.peakHold - decay);
            entry.peakHoldAlpha = juce::jmax(0.25f, 1.0f - elapsed / 2.0f);
        }

        // Update cache
        telemetryCache_[idx].peakLeft  = latest.peakLeft;
        telemetryCache_[idx].peakRight = latest.peakRight;
        telemetryCache_[idx].rmsAvg    = rmsAvg;
        telemetryCache_[idx].hasSignal = entry.hasSignal;
        telemetryCache_[idx].active    = true;
    });
}

void MessengerListComponent::refreshTelemetryFromRegistry(SlotRegistry& registry)
{
    bool anyData = false;
    syncTelemetryFromRegistry(registry, anyData);
    // ═══ Reconstruir grupos de bus después de sync metadatos ═══════════════
    // syncTelemetryFromRegistry() actualiza entry.info (incluyendo bus) a
    // 60fps, pero NO repuebla busGroups_. Sin esto, si el usuario cambia el
    // bus de un track en el Messenger, el track se queda dibujado en su grupo
    // VIEJO aunque entry.info.bus ya esté actualizado.
    //
    // rebuildBusGroups() itera messengers_ y reasigna cada slot al grupo que
    // le corresponde según su bus actual. Al llamarse a 60fps, el cambio de
    // bus se refleja instantáneamente en la UI.
    rebuildBusGroups();
    juce::ignoreUnused(anyData);
}

/** Reconstruye busGroups_ desde messengers_ actuales.
    Itera todos los slots y los asigna al grupo correspondiente según su bus.
    Se llama después de cada syncTelemetryFromRegistry() para reflejar cambios
    de bus en tiempo real (sin esperar a updateMessengers() ). */
void MessengerListComponent::rebuildBusGroups()
{
    for (auto& group : busGroups_)
        group.count = 0;

    for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx)
    {
        auto& entry = messengers_[idx];
        if (!entry.info.active)
            continue;

        int busIdx = static_cast<int>(entry.info.bus);
        if (busIdx < 0 || busIdx > kNumBuses)
            busIdx = kNumBuses;

        auto& group = busGroups_[busIdx];
        if (group.count < SlotRegistry::kMaxSlots)
            group.slotIndices[group.count++] = idx;
    }
}

// ===== updateMessengers =======================================================
void MessengerListComponent::updateMessengers(SlotRegistry& registry)
{
    bool anyData = false;
    syncTelemetryFromRegistry(registry, anyData);

    if (s_persistentReady_)
        return;

    if (!anyData)
    {
        repaint();
        return;
    }

    for (auto& group : busGroups_) group.count = 0;
    activeMessengerCount_ = 0;

    for (auto& entry : messengers_)
        entry = MessengerEntry{};

    registry.forEachActive([&](const SlotInfo& info)
    {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
        auto& entry = messengers_[idx];
        entry.info = info;

        auto& telem = registry.getTelemetry(idx);
        auto latest = telem.latest();
        entry.peakLeft  = latest.peakLeft;
        entry.peakRight = latest.peakRight;
        float newPeak = juce::jmax(latest.peakLeft, latest.peakRight, kMinBarLevel);
        entry.barLevel  = newPeak;
        entry.peakHold  = newPeak;
        entry.peakHoldTimeMs = juce::Time::getMillisecondCounter();
        entry.peakHoldAlpha = 1.0f;
        entry.rmsAvg    = (latest.rmsLeft + latest.rmsRight) * 0.5f;
        entry.rmsSmooth = entry.rmsAvg;
        entry.hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
        entry.aiSuggestion = generateAISuggestion(info, entry.rmsAvg);

        ++activeMessengerCount_;

        int busIdx = static_cast<int>(info.bus);
        if (busIdx < 0) busIdx = kNumBuses;
        auto& group = busGroups_[busIdx];
        if (group.count < SlotRegistry::kMaxSlots)
            group.slotIndices[group.count++] = idx;
    });

    if (activeMessengerCount_ > 0)
    {
        uint32_t now = juce::Time::getMillisecondCounter();
        for (auto& entry : messengers_)
        {
            if (entry.info.active && entry.fadeAlpha >= 1.0f)
            {
                entry.fadeAlpha = 0.0f;
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

// ===== smoothMeters ===========================================================
//  ═══ DECAIMIENTO FIJO hacia -infinito (render loop independiente) ════════════
//
//  CRÍTICO: El decaimiento es CONSTANTE por frame (kDecayDbPerFrame dB).
//  NO depende del target (cachePeak), ni de que rawPeak haya cambiado.
//  Cada frame, la barra decae 0.5 dB hacia -infinito, SIEMPRE.
//
//  ¿Qué pasa cuando el background thread actualiza cada 500ms?
//    • syncTelemetryFromRegistry() ataca instantáneamente
//    • Luego, smoothMeters() decae 0.5 dB/frame durante 500ms
//    • La barra se mueve ~30 dB en 500ms = movimiento constante y fluido
//    • Cuando llegan datos nuevos, el attack instantáneo la sube de nuevo
//
//  Esto replica la sensación de un VU meter analógico donde la aguja
//  SIEMPRE está en movimiento, aunque la señal no cambie.
// ═══════════════════════════════════════════════════════════════════════════════
void MessengerListComponent::smoothMeters()
{
    for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx)
    {
        if (!telemetryCache_[idx].active)
            continue;

        auto& cache = telemetryCache_[idx];
        auto& entry = messengers_[idx];

        // Sync raw values from cache (fallback cuando tryEnter() falla)
        entry.peakLeft  = cache.peakLeft;
        entry.peakRight = cache.peakRight;
        entry.rmsAvg    = cache.rmsAvg;
        entry.hasSignal = cache.hasSignal;

        // ═══ Fixed decay toward -infinity (dB per frame) ════════════════════
        // Decay es CONSTANTE por frame, independiente de cachePeak.
        // Así la barra SIEMPRE se mueve aunque los datos de telemetría
        // no hayan cambiado entre frames.
        entry.barLevel -= kDecayDbPerFrame;
        entry.barLevel = juce::jmax(entry.barLevel, kMinBarLevel);
        
        // RMS smooth decay (exponencial suave)
        if (entry.rmsSmooth > entry.rmsAvg + 0.5f)
            entry.rmsSmooth += (entry.rmsAvg - entry.rmsSmooth) * kRmsReleaseCoeff;
        else if (entry.hasSignal)
            entry.rmsSmooth = entry.rmsAvg;
        else
            entry.rmsSmooth = juce::jmax(-80.0f, entry.rmsSmooth - kDecayDbPerFrame);

        // ═══ Peak hold decay (time-based, como antes) ═══════════════════════
        uint32_t now = juce::Time::getMillisecondCounter();
        float elapsed = (now - entry.peakHoldTimeMs) * 0.001f;
        if (elapsed > 0.5f)
        {
            float decayTime = elapsed - 0.5f;
            float decay = kPeakHoldDecayDbPerSec * decayTime;
            entry.peakHold = juce::jmax(entry.barLevel, entry.peakHold - decay);
            entry.peakHoldAlpha = juce::jmax(0.25f, 1.0f - decayTime / 2.0f);
        }
    }

    // ═══ Fade-in animation ═════════════════════════════════════════════════════
    uint32_t now = juce::Time::getMillisecondCounter();
    for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx)
    {
        auto& entry = messengers_[idx];
        if (entry.fadeAlpha >= 1.0f)
            continue;
        float elapsed = (float)(now - entry.fadeStartMs);
        entry.fadeAlpha = juce::jmin(1.0f, elapsed / MessengerEntry::kFadeDurationMs);
    }

    // NOTA: El repaint() lo maneja el Timer interno (timerCallback) a 120fps
    // y el editor timer a 60fps vía tabbedComponent_->smoothMeters().
    // No hacer repaint() aquí para evitar duplicados.
}

// --- Visibilidad --------------------------------------------------------------
void MessengerListComponent::visibilityChanged()
{
    isPaused_ = !isShowing();
    if (!isPaused_ && s_persistentReady_)
        repaint();
}

// --- Set grouping mode -------------------------------------------------------
void MessengerListComponent::setGroupingMode(GroupingMode mode)
{
    if (groupingMode_ == mode)
        return;
    groupingMode_ = mode;
    repaint();
}

// --- Altura total para Viewport -----------------------------------------------
int MessengerListComponent::getPreferredHeight() const
{
    if (activeMessengerCount_ == 0)
        return getHeight();

    int totalHeight = 4;

    if (groupingMode_ == GroupingMode::Bus)
    {
        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx)
        {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;
            totalHeight += kHeaderHeight + 2;
            if (!collapsedGroups_[busIdx])
                totalHeight += (kCardHeight + kCardGap) * group.count;
            totalHeight += 6;
        }
    }
    else
    {
        totalHeight += (kCardHeight + kCardGap) * activeMessengerCount_;
        totalHeight += 6;
    }

    return totalHeight + 8;
}

// ===== PAINT ==================================================================
void MessengerListComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colours::transparentBlack);

    auto area = bounds.reduced(2, 4);

    if (activeMessengerCount_ == 0) {
        emptyLabel_.setVisible(true);
        return;
    }
    emptyLabel_.setVisible(false);

    if (groupingMode_ == GroupingMode::Bus)
    {
        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx)
        {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            if (area.getHeight() < kHeaderHeight + kCardHeight + kCardGap) break;
            drawBusHeader(g, area, busIdx, group.count);

            if (!collapsedGroups_[busIdx])
            {
                int rowsInGroup = juce::jmin(group.count, kMaxRowsPerBus);
                for (int r = 0; r < rowsInGroup; ++r)
                {
                    if (area.getHeight() < kCardHeight + kCardGap) break;
                    int slotIdx = group.slotIndices[r];
                    auto cardArea = area.removeFromTop(kCardHeight).reduced(0, kCardGap / 2);
                    drawTrackCard(g, cardArea, messengers_[slotIdx], r);
                }
            }
            area.removeFromTop(4);
        }
    }
    else
    {
        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx)
        {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            for (int r = 0; r < group.count; ++r)
            {
                if (area.getHeight() < kCardHeight + kCardGap) break;
                int slotIdx = group.slotIndices[r];
                auto cardArea = area.removeFromTop(kCardHeight).reduced(0, kCardGap / 2);
                drawTrackCard(g, cardArea, messengers_[slotIdx], r);
            }
        }
    }
}

// --- Cabecera de bus ----------------------------------------------------------
void MessengerListComponent::drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                                            int busIdx, int count)
{
    auto headerArea = bounds.removeFromTop(kHeaderHeight).reduced(0, 1);

    juce::Colour busColour;
    juce::String busName;

    if (busIdx == kNumBuses) {
        busColour = MixCoachTheme::textMuted();
        busName = "UNASSIGNED";
    } else {
        busColour = getBusColour(busIdx);
        busName = juce::String(busNames[busIdx]).toUpperCase();
    }

    auto accentBar = headerArea.removeFromLeft(2);
    g.setColour(busColour.withAlpha(0.6f));
    g.fillRect(accentBar.reduced(0, 2).toFloat());
    headerArea.removeFromLeft(4);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(busColour);
    auto nameArea = headerArea.removeFromLeft(80);
    g.drawText(busName, nameArea, juce::Justification::centredLeft);

    auto badgeArea = headerArea.removeFromLeft(26).reduced(0, 3);
    g.setColour(busColour.withAlpha(0.12f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 5.0f);
    g.setColour(busColour);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    g.drawText(juce::String(count), badgeArea, juce::Justification::centred);
}

// ===== TRACK CARD =============================================================
void MessengerListComponent::drawTrackCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                                             const MessengerEntry& entry, int index)
{
    juce::ignoreUnused(index);

    float fade = entry.fadeAlpha;
    if (fade <= 0.001f) return;

    bool isSelected = (entry.info.slotIndex == selectedSlot_);

    auto cardBounds = bounds.toFloat();

    // --- Card shadow -----------------------------------------------------------
    auto shadowBounds = cardBounds.expanded(1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.2f * fade));
    g.fillRoundedRectangle(shadowBounds, (float)kCardCorner);

    // --- Card background -------------------------------------------------------
    g.setColour(kCardBg.withAlpha(fade));
    g.fillRoundedRectangle(cardBounds, (float)kCardCorner);

    if (isSelected) {
        g.setColour(juce::Colour(0xFF7C3AED).withAlpha(0.08f));
        g.fillRoundedRectangle(cardBounds, (float)kCardCorner);
    }

    // Track colour bar (left, 3px)
    auto colourBar = bounds.removeFromLeft(3).toFloat();
    g.setColour(entry.info.colour.withAlpha(fade));
    g.fillRoundedRectangle(colourBar.reduced(0, 4), 1.5f);
    bounds.removeFromLeft(2);

    // --- Card border -----------------------------------------------------------
    if (isSelected) {
        g.setColour(MixCoachTheme::accent().withAlpha(fade * 0.7f));
        g.drawRoundedRectangle(cardBounds, (float)kCardCorner, 1.5f);
    } else {
        g.setColour(kCardBorder.withAlpha(fade * 0.6f));
        g.drawRoundedRectangle(cardBounds, (float)kCardCorner, 1.0f);
    }

    // Glass highlight
    auto glassBar = cardBounds.withHeight(cardBounds.getHeight() * 0.45f);
    juce::ColourGradient glassGrad(
        juce::Colours::white.withAlpha(isSelected ? 0.06f : 0.04f * fade),
        juce::Point<float>(0.0f, glassBar.getY()),
        juce::Colour(0x00000000),
        juce::Point<float>(0.0f, glassBar.getBottom()),
        false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(glassBar, (float)kCardCorner);

    if (isSelected) {
        auto selectedAccent = colourBar.reduced(0, 4);
        g.setColour(MixCoachTheme::accent());
        g.fillRoundedRectangle(selectedAccent, 1.5f);
    }

    // --- Layout (right to left) -----------------------------------------------
    auto routeArea      = bounds.removeFromRight(44).reduced(2, 6);
    auto suggestionArea = bounds.removeFromRight(110).reduced(2, 6);
    auto statsArea      = bounds.removeFromRight(44).reduced(0, 6);
    auto barArea        = bounds.removeFromRight(70).reduced(0, 10);

    // --- Track name + status dot ----------------------------------------------
    auto nameArea = bounds.reduced(0, 5);

    auto dotArea = nameArea.removeFromLeft(8).reduced(0, 9);
    if (entry.hasSignal)
        g.setColour(kMeterGreen.withAlpha(0.85f * fade));
    else
        g.setColour(kTextMuted.withAlpha(0.2f * fade));
    g.fillEllipse(dotArea.toFloat());
    nameArea.removeFromLeft(2);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(kTextBright.withAlpha(fade));
    auto name = juce::String(entry.info.trackName).trim();
    if (name.isEmpty())
        name = "Track " + juce::String(entry.info.slotIndex + 1);
    g.drawFittedText(name, nameArea, juce::Justification::centredLeft, 1);

    // ===== DAW PROFESSIONAL VU METER ==========================================
    //  Usa barLevel (Messenger VUMeter-style: attack instant, release 0.15f)
    //  Sub-pixel float rendering | Dual Peak + RMS | Glass shine | Peak hold ◀

    float levelDb = juce::jmax(entry.barLevel, kMinBarLevel);
    float rmsDb   = juce::jmax(entry.rmsSmooth, kMinBarLevel);
    float levelNorm = juce::jlimit(0.01f, 1.0f,
        (levelDb - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb));
    float rmsNorm = juce::jlimit(0.01f, 1.0f,
        (rmsDb - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb));

    auto barFloat   = barArea.toFloat();
    float barWidthF = barFloat.getWidth();
    float barH      = barFloat.getHeight();

    // --- Bar background -------------------------------------------------------
    g.setColour(kBarBg.withAlpha(fade));
    g.fillRoundedRectangle(barFloat, 3.0f);

    // Inner shadow
    juce::ColourGradient innerShade;
    innerShade.isRadial = false;
    innerShade.point1 = juce::Point<float>(barFloat.getX(), barFloat.getY());
    innerShade.point2 = juce::Point<float>(barFloat.getX(), barFloat.getBottom());
    innerShade.addColour(0.0f, juce::Colours::black.withAlpha(0.30f));
    innerShade.addColour(0.2f, juce::Colours::black.withAlpha(0.0f));
    innerShade.addColour(0.8f, juce::Colours::black.withAlpha(0.0f));
    innerShade.addColour(1.0f, juce::Colours::black.withAlpha(0.18f));
    g.setGradientFill(innerShade);
    g.fillRoundedRectangle(barFloat, 3.0f);

    g.setColour(kBarBorder.withAlpha(fade * 0.6f));
    g.drawRoundedRectangle(barFloat, 3.0f, 1.0f);

    // --- RMS overlay bar ------------------------------------------------------
    if (rmsNorm > 0.02f)
    {
        float rmsW = juce::jmax(2.0f, barWidthF * rmsNorm);
        float rmsH = barH * 0.60f;
        float rmsY = barFloat.getY() + (barH - rmsH) * 0.5f;
        auto rmsRect = juce::Rectangle<float>(barFloat.getX(), rmsY, rmsW, rmsH);

        juce::ColourGradient rmsGrad;
        rmsGrad.isRadial = false;
        rmsGrad.point1 = rmsRect.getTopLeft();
        rmsGrad.point2 = rmsRect.getTopRight();
        rmsGrad.addColour(0.00f, kMeterGreen.withAlpha(0.35f));
        rmsGrad.addColour(0.50f, kMeterLime.withAlpha(0.35f));
        rmsGrad.addColour(0.70f, kMeterYellow.withAlpha(0.35f));
        rmsGrad.addColour(0.85f, kMeterOrange.withAlpha(0.35f));
        rmsGrad.addColour(0.95f, kMeterRed.withAlpha(0.35f));
        rmsGrad.addColour(1.00f, kMeterRed.withAlpha(0.35f));
        g.setGradientFill(rmsGrad);
        g.fillRoundedRectangle(rmsRect, 2.0f);
    }

    // --- PEAK bar fill (sub-pixel float width) --------------------------------
    float fillW = juce::jmax(2.0f, barWidthF * levelNorm);
    auto fillRect = juce::Rectangle<float>(barFloat.getX(), barFloat.getY(),
                                            fillW, barH);

    juce::ColourGradient meterGrad;
    meterGrad.isRadial = false;
    meterGrad.point1 = fillRect.getTopLeft();
    meterGrad.point2 = fillRect.getTopRight();
    meterGrad.addColour(0.00f, kMeterGreen);
    meterGrad.addColour(0.50f, kMeterLime);
    meterGrad.addColour(0.70f, kMeterYellow);
    meterGrad.addColour(0.85f, kMeterOrange);
    meterGrad.addColour(0.95f, kMeterRed);
    meterGrad.addColour(1.00f, kMeterRed);
    g.setGradientFill(meterGrad);
    g.fillRoundedRectangle(fillRect, 3.0f);

    // --- Glass shine ----------------------------------------------------------
    if (fillW > 10.0f)
    {
        float shineAlpha = levelNorm > 0.95f ? 0.20f : 0.14f;
        auto shineLine = juce::Rectangle<float>(fillRect.getX() + 2.0f,
                                                  fillRect.getY() + 1.0f,
                                                  juce::jmin(fillW - 4.0f, barWidthF * 0.7f),
                                                  2.0f);
        juce::ColourGradient shineGrad;
        shineGrad.isRadial = false;
        shineGrad.point1 = juce::Point<float>(shineLine.getX(), shineLine.getY());
        shineGrad.point2 = juce::Point<float>(shineLine.getRight(), shineLine.getY());
        shineGrad.addColour(0.0f, juce::Colours::white.withAlpha(shineAlpha * fade));
        shineGrad.addColour(0.6f, juce::Colours::white.withAlpha(shineAlpha * 0.5f * fade));
        shineGrad.addColour(1.0f, juce::Colours::white.withAlpha(0.0f));
        g.setGradientFill(shineGrad);
        g.fillRoundedRectangle(shineLine, 1.0f);
    }

    // --- Clip glow ------------------------------------------------------------
    float clipNorm = (kRedZoneEnd - 0.5f - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb);
    if (levelNorm > clipNorm)
    {
        float glowIntensity = (levelNorm - clipNorm) / (1.0f - clipNorm);
        glowIntensity = juce::jmin(1.0f, glowIntensity);
        g.setColour(kMeterRed.withAlpha(glowIntensity * 0.12f * fade));
        g.fillRoundedRectangle(barFloat.expanded(1.0f), 4.0f);
    }

    // --- Zone markers ---------------------------------------------------------
    auto drawZoneMarker = [&](float dbThreshold, const juce::Colour& col)
    {
        float norm = (dbThreshold - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb);
        float x = barFloat.getX() + barWidthF * norm;
        g.setColour(col.withAlpha(0.35f * fade));
        g.drawVerticalLine((int) x, barFloat.getY() + 2.0f, barFloat.getBottom() - 2.0f);
    };
    drawZoneMarker(kGreenZoneEnd,  kMeterYellow);
    drawZoneMarker(kYellowZoneEnd, kMeterRed);

    // --- PEAK HOLD triangle with alpha fade ----------------------------------
    float peakHoldNorm = juce::jlimit(0.0f, 1.0f,
        (entry.peakHold - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb));
    if (peakHoldNorm > 0.02f)
    {
        float peakX = barFloat.getX() + barWidthF * peakHoldNorm;
        peakX = juce::jlimit(barFloat.getX() + 4.0f, barFloat.getRight() - 4.0f, peakX);

        float triAlpha = entry.peakHoldAlpha * fade;

        if (entry.peakHoldAlpha > 0.8f)
        {
            g.setColour(juce::Colours::white.withAlpha(triAlpha * 0.15f));
            g.fillEllipse(peakX - 3.0f, barFloat.getY() + 1.0f,
                          7.0f, barFloat.getHeight() - 2.0f);
        }

        juce::Path tri;
        float ty = barFloat.getY() + 2.0f;
        float by = barFloat.getBottom() - 2.0f;
        tri.addTriangle(peakX + 3.5f, ty,
                        peakX + 3.5f, by,
                        peakX - 3.5f, (ty + by) * 0.5f);
        g.setColour(kPkTriangle.withAlpha(triAlpha));
        g.fillPath(tri);
    }

    // --- Stats: PK dB ---------------------------------------------------------
    juce::Colour pkCol = (entry.peakLeft > kYellowZoneEnd) ? kMeterRed
                        : (entry.peakLeft > kGreenZoneEnd) ? kMeterYellow
                        : kTextDim;
    g.setColour(pkCol.withAlpha(fade));
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    g.drawText(juce::String(entry.peakLeft, 1) + " dB",
               statsArea, juce::Justification::centredLeft);

    // --- AI Suggestion (cyan #00B4D8, 11px) ----------------------------------
    if (entry.aiSuggestion.isNotEmpty())
    {
        g.setColour(MixCoachTheme::accentCyan().withAlpha(fade * 0.85f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawFittedText(entry.aiSuggestion, suggestionArea,
                         juce::Justification::centredLeft, 1);
    }
    else
    {
        g.setColour(kTextMuted.withAlpha(fade * 0.3f));
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.drawFittedText("\xe2\x80\x94", suggestionArea,
                         juce::Justification::centredLeft, 1);
    }

    // --- Status dot ------------------------------------------------------------
    if (entry.hasSignal) {
        auto statusDot = routeArea.removeFromLeft(6).reduced(0, 8);
        g.setColour(kMeterGreen.withAlpha(fade * 0.6f));
        g.fillEllipse(statusDot.toFloat());
        routeArea.removeFromLeft(2);
    }

    // --- Bus pill --------------------------------------------------------------
    if (entry.info.bus != BusType::None) {
        int busIdx = static_cast<int>(entry.info.bus);
        auto busCol = getBusColour(busIdx);
        juce::String abbr;
        switch (entry.info.bus) {
            case BusType::Drums:   abbr = "DRM"; break;
            case BusType::Bass:    abbr = "BAS"; break;
            case BusType::Guitars: abbr = "GTR"; break;
            case BusType::Keys:    abbr = "KEY"; break;
            case BusType::Vocals:  abbr = "VOX"; break;
            case BusType::FX:      abbr = "FX";  break;
            default:               abbr = juce::String(busNames[busIdx]).substring(0, 3).toUpperCase(); break;
        }
        g.setColour(busCol.withAlpha(0.15f * fade));
        g.fillRoundedRectangle(routeArea.toFloat(), 4.0f);
        g.setColour(busCol.withAlpha(fade * 0.9f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
        g.drawFittedText(abbr, routeArea, juce::Justification::centred, 1);
    } else {
        g.setColour(kTextMuted.withAlpha(0.4f * fade));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawFittedText("-", routeArea, juce::Justification::centred, 1);
    }
}

} // namespace mixcoach

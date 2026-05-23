#include "PluginEditor.h"
#include "../Common/LogHelper.h"

namespace mixcoach {

MixCoachAudioProcessorEditor::MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData* sharedData)
    : AudioProcessorEditor(&processor)
    , processorRef_(processor)
    , sharedData_(sharedData)
{
    setSize(960, 640);
    setResizable(true, true);
    setResizeLimits(800, 500, 1920, 1440);

    // ─── Placeholder mientras sharedData no está disponible ─────────────────
    // Mostramos un mensaje amigable mientras se inicializa SharedData en
    // segundo plano desde el timerCallback(). Esto evita que el message
    // thread de FL Studio se congele por CreateFileMapping.
    if (sharedData_ == nullptr) {
        placeholderLabel_.setText(
            juce::CharPointer_UTF8("\xF0\x9F\x94\x84 Inicializando MixCoach...\n\n"
                                   "Conectando con el sistema compartido.\n"
                                   "Esto toma solo un instante."),
            juce::dontSendNotification);
        placeholderLabel_.setFont(juce::Font(juce::FontOptions(18.0f)));
        placeholderLabel_.setJustificationType(juce::Justification::centred);
        placeholderLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
        addAndMakeVisible(placeholderLabel_);
    } else {
        // SharedData ya disponible → construir UI completa inmediatamente
        buildFullUI();
    }

    // Version label (siempre visible)
    versionLabel_.setText("v1.0.0", juce::dontSendNotification);
    versionLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    versionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(versionLabel_);

    // Timer de 30fps para inicialización lazy + actualización de UI
    // Si sharedData no está listo, el primer tick lo inicializa
    announcedSlots_.fill(false);
    initSharedData();
    startTimerHz(30);
}

MixCoachAudioProcessorEditor::~MixCoachAudioProcessorEditor()
{
    stopTimer();

    // Limpiar callbacks del SlotRegistry para evitar dangling pointers
    if (sharedData_) {
        auto& registry = sharedData_->getSlotRegistry();
        registry.onSlotChanged    = nullptr;
        registry.onSlotRegistered = nullptr;
        registry.onSlotReleased   = nullptr;
    }
}    // ─── Inicialización gradual de SharedData ───────────────────────────────────
// Llamado desde timerCallback(). NO desde el constructor o createEditor().
// Esto asegura que CreateFileMapping NO se ejecute en el message thread
// del host DAW durante la creación inicial de la UI.
void MixCoachAudioProcessorEditor::initSharedData()
{
    if (sharedData_ != nullptr && fullUIBuilt_ && tabbedComponent_ != nullptr)
        return;

    // Backoff: si falló antes, esperar 1 segundo antes de reintentar
    if (sharedData_ == nullptr) {
        auto now = juce::Time::getMillisecondCounter();
        if (lastInitAttemptMs_ > 0 && (now - lastInitAttemptMs_) < 1000)
            return;

        lastInitAttemptMs_ = now;
    }

    // Llamar a ensureSharedData del processor — esto puede hacer
    // CreateFileMapping, pero como es desde un timer callback (no desde
    // createEditor), el message thread no está bloqueado en medio de
    // una operación crítica de creación de ventana.
    if (sharedData_ == nullptr) {
        processorRef_.ensureSharedData();
        sharedData_ = processorRef_.getSharedData();
    }

    if (sharedData_ != nullptr && (!fullUIBuilt_ || tabbedComponent_ == nullptr)) {
        // ¡SharedData disponible! Construir la UI completa.
        buildFullUI();
        if (tabbedComponent_) {
            tabbedComponent_->setBounds(getLocalBounds()
                .withTrimmedTop(36)); // dejar espacio para header
            resized();
            repaint();
        }
    }
}

// ─── Construir UI completa (solo cuando sharedData está disponible) ─────────
void MixCoachAudioProcessorEditor::buildFullUI()
{
    if (fullUIBuilt_ || sharedData_ == nullptr)
        return;

    placeholderLabel_.setVisible(false);

    tabbedComponent_ = std::make_unique<MainTabbedComponent>(
        processorRef_, *sharedData_);
    addAndMakeVisible(tabbedComponent_.get());
    fullUIBuilt_ = true;
    LogHelper::writeToLog("[MixCoachEditor] UI completa construida con tabs");

    // ─── Configurar callbacks en SlotRegistry ────────────────────────────
    auto& registry = sharedData_->getSlotRegistry();

    registry.onSlotChanged = [this](int) {
        juce::Component::SafePointer<MixCoachAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis]() {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (!self->sharedData_ || !self->tabbedComponent_) return;
            self->tabbedComponent_->getCoachPanel().updateMessengers(
                self->sharedData_->getSlotRegistry());
            self->repaint();
        });
    };

    registry.onSlotRegistered = [this](int slotIndex) {
        juce::Component::SafePointer<MixCoachAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis, slotIndex]() {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (!self->sharedData_ || !self->tabbedComponent_) return;
            // Verificar bounds ANTES de acceder a getSlotInfo
            if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
                return;
            auto& reg = self->sharedData_->getSlotRegistry();
            auto info = reg.getSlotInfo(slotIndex);
            self->announcedSlots_[slotIndex] = true;
            auto* coach = self->processorRef_.getCoachEngine();
            if (coach) {
                coach->announceNewTrack(
                    slotIndex,
                    juce::String(info.trackName),
                    info.colour);
            }
            self->tabbedComponent_->getCoachPanel().updateMessengers(reg);
            self->repaint();
        });
    };

    registry.onSlotReleased = [this](int slotIndex) {
        juce::Component::SafePointer<MixCoachAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis, slotIndex]() {
            if (safeThis == nullptr) return;
            auto* self = safeThis.getComponent();
            if (!self->sharedData_ || !self->tabbedComponent_) return;
            if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) {
                self->announcedSlots_[slotIndex] = false;
            }
            self->tabbedComponent_->getCoachPanel().updateMessengers(
                self->sharedData_->getSlotRegistry());
            self->repaint();
        });
    };
}

// ═══════════════════════════════════════════════════════════════════════════
//  Timer callback (30fps)
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::timerCallback()
{
    // Paso 1: Si sharedData aún no está disponible, intentar inicializarlo
    if (sharedData_ == nullptr || !tabbedComponent_) {
        initSharedData();
        // Si sigue sin estar disponible, esperar al próximo tick
        if (sharedData_ == nullptr || !tabbedComponent_)
            return;
    }

    auto& registry = sharedData_->getSlotRegistry();

    // 2. Sincronizar desde memoria compartida (IPC)
    registry.syncFromShared();

    // 3. Detectar nuevos Messengers
    detectNewMessengers();

    // 4. Actualizar panel de Messengers en el chat
    tabbedComponent_->getCoachPanel().updateMessengers(registry);

    // 5. Actualizar panel de analizadores
    auto& analyzers = tabbedComponent_->getAnalyzersPanel();
    analyzers.updateAnalyzers(registry);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Layout y pintado
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header delgado
    auto headerBounds = area.removeFromTop(36);
    versionLabel_.setBounds(headerBounds.removeFromRight(60));

    if (tabbedComponent_) {
        tabbedComponent_->setBounds(area);
    } else {
        placeholderLabel_.setBounds(area);
    }
}

void MixCoachAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Fondo general
    g.fillAll(MixCoachTheme::bgDarker());

    // ─── Header con degradado profesional ─────────────────────────────────
    // Siempre se dibuja, incluso cuando tabbedComponent_ está presente,
    // porque el header (y=0..36) no es cubierto por el tabbed component.
    auto headerBounds = bounds.removeFromTop(36);

    juce::ColourGradient headerGrad(
        MixCoachTheme::accent().withAlpha(0.08f),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::accent2().withAlpha(0.03f),
        juce::Point<float>((float)headerBounds.getWidth(), 0.0f),
        false);
    g.setGradientFill(headerGrad);
    g.fillRect(headerBounds);

    // Línea inferior del header
    g.setColour(MixCoachTheme::accent().withAlpha(0.3f));
    g.drawHorizontalLine(headerBounds.getBottom(), 0.0f, (float)bounds.getWidth());

    // Logo / Título
    auto logoArea = headerBounds.reduced(8, 0);
    g.setFont(juce::Font(juce::FontOptions(18.0f)));
    g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x8E\x9B"), logoArea.removeFromLeft(24),
               juce::Justification::centred);
    g.setColour(MixCoachTheme::textBright());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    g.drawText("MixCoach", logoArea.removeFromLeft(110), juce::Justification::centredLeft);
    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    g.drawText("Sistema Inteligente de Mentoria para Mezcla",
               logoArea, juce::Justification::centredLeft);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Detección de nuevos Messengers
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::detectNewMessengers()
{
    if (!sharedData_ || !tabbedComponent_) return;
    auto& registry = sharedData_->getSlotRegistry();
    auto* coach    = processorRef_.getCoachEngine();

    if (!coach) return;

    int currentActiveCount = registry.activeCount();

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx >= 0 && idx < SlotRegistry::kMaxSlots && !announcedSlots_[idx]) {
            announcedSlots_[idx] = true;
            coach->announceNewTrack(idx,
                                    juce::String(info.trackName),
                                    info.colour);
        }
    });

    if (currentActiveCount < lastActiveSlotCount_) {
        for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
            auto info = registry.getSlotInfo(i);
            if (!info.active && announcedSlots_[i]) {
                announcedSlots_[i] = false;
            }
        }
    }

    lastActiveSlotCount_ = currentActiveCount;
}

} // namespace mixcoach

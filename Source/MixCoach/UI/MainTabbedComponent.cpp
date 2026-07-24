#include "MainTabbedComponent.h"

namespace mixcoach {

    MainTabbedComponent::MainTabbedComponent(MixCoachAudioProcessor& processor, SharedData& /*sharedData*/) :
        juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop),
        processorRef_(processor),
        coachPanel_(nullptr),
        analyzersPanel_(std::make_unique<AnalyzersPanelComponent>(processor.getAudioAnalyzer()))

    {
        coachPanel_ = std::make_unique<MixCoachPanel>();

        // ═══ Sync Tab 1 selection -> Tab 2 (ya no necesita slot, siempre master) ═══
        coachPanel_->onTrackSelected = [this](int /*slotIndex*/) {
            // V2: Tab 2 siempre muestra master, no necesita selección de slot
        };

        // ═══ Conectar referencias → CoachEngine (el cerebro) ═══════════════
        auto cacheRefs = [this]() {
            auto& refPanel = coachPanel_->getRefPanel();
            processorRef_.cacheReferencePaths(refPanel.getFilePaths(), refPanel.getURLs());
        };

        coachPanel_->onReferenceFileAdded = [this, cacheRefs](const juce::String& path) {
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng != nullptr) eng->setReferenceAudio(path);
            cacheRefs();
        };
        coachPanel_->onReferenceURLAdded = [this, cacheRefs](const juce::String& name, const juce::String& url) {
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng != nullptr) eng->setReferenceURL(name, url);
            cacheRefs();
        };
        // Cuando se vacían las referencias, limpiar el cerebro
        coachPanel_->onReferenceCleared = [this, cacheRefs]() {
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng != nullptr) eng->clearReferences();
            cacheRefs();
        };

        // ═══ Play/Pause reference audio ════════════════════════════════
        // --- Seek callback ---
        coachPanel_->onSeekReference = [this](double seconds) { processorRef_.getRefPlayer().setPosition(seconds); };

        coachPanel_->onPlayReference = [this](int refIndex) {
            auto& refs = coachPanel_->getRefPanel();
            if (refIndex >= 0 && refIndex < refs.getNumReferences()) {
                const auto& ref = coachPanel_->getRefPanel().getReference(refIndex);
                if (ref.type == MixReference::Type::File) {
                    processorRef_.getRefPlayer().playFileAtPath(ref.path, refIndex);
                    // Update UI state immediately (timer polling handles finish detection)
                    coachPanel_->setRefPanelPlaybackState(refIndex, true);
                }
            }
        };

        // ═══ Reference selection callback (swap active match reference) ═══
        coachPanel_->onReferenceSelected = [this](int refIndex) {
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng == nullptr) return;
            auto& refs = coachPanel_->getRefPanel();
            if (refIndex >= 0 && refIndex < refs.getNumReferences()) {
                const auto& ref = refs.getReference(refIndex);
                if (ref.type == MixReference::Type::File) eng->setReferenceAudio(ref.path);
            }
            else {
                // Deseleccionó — solo limpiar match panel, engine conserva su ref
                refs.updateMatchData(DifferenceProfile{});
            }
        };

        // ═══ Ollama retry callback (Phi-3 local) ────────────────────────
        coachPanel_->onRetryOllama = [this]() { processorRef_.retryOllamaConnection(); };

        // ═══ Section selection callback ════════════════════════════════
        coachPanel_->onSectionSelected = [this](int sectionIndex) {
            // -1 = global fingerprint, 0..7 = section fingerprint
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->setActiveSection(sectionIndex);
        };

        // ─── Tab 1: Mix Coach ──────────────────────────────────────────────────
        // NOTA: refPanel_.onSectionSeekTo ya está cableado internamente en
        // CoachChatComponent.cpp a través de onSeekReference, que se conecta
        // al método de arriba (coachPanel_->onSeekReference).
        addTab("Mix Coach",
               MixCoachTheme::bgPanel(),
               coachPanel_.get(),
               false,
               0);

        // ─── Tab 2: Professional Metering ──────────────────────────────────────
        addTab(juce::CharPointer_UTF8("[CHART]  Metering"),
               MixCoachTheme::bgPanel(),
               analyzersPanel_.get(),
               false,
               1);


        setTabBarDepth(0);

        setCurrentTabIndex(0);

        // ═══ Restaurar referencias guardadas (desde setStateInformation) ═══
        if (processorRef_.hasPendingReferences()) {
            auto filePaths = processorRef_.takePendingFilePaths();
            auto urls      = processorRef_.takePendingURLs();
            if (!filePaths.empty() || !urls.empty()) {
                coachPanel_->getRefPanel().restoreFromPaths(filePaths, urls);
            }
        }
    }

    void MainTabbedComponent::resized()
    {
        // Tab bar hidden (depth=0); header tabs live in PluginEditor.
        // ═══ Solo redimensionar el panel ACTIVO ═══════════════════════════════
        // Antes se redimensionaban AMBOS paneles en cada resize, incluso cuando
        // uno estaba oculto (tab inactivo). Esto causaba layout innecesario y
        // contribuía a los freezes al cambiar de tab, porque el panel oculto
        // ejecutaba su resized() completo (AnalyzersPanel o MixCoachPanel)
        // sin necesidad.
        auto area     = getLocalBounds();
        int activeTab = getCurrentTabIndex();
        if (activeTab == 0 && coachPanel_ != nullptr) coachPanel_->setBounds(area);
        else if (activeTab == 1 && analyzersPanel_ != nullptr)
            analyzersPanel_->setBounds(area);
    }

    void MainTabbedComponent::updateAllPanels(SlotRegistry& registry, SharedData& sharedData, double sampleRate)
    {
        try {
            // Update MixCoach panel messenger list (siempre, para Tab 1)
            coachPanel_->updateMessengers(registry, sharedData);

            // ═══ Actualizar analyzers SOLO si Tab 2 (Metering) está activo ═══
            // Tab 3 (System) tiene su propio update vía updateProfessionalAnalyzers.
            if (getCurrentTabIndex() == 1) {
                analyzersPanel_->updateAnalyzers(sampleRate);
                analyzersPanel_->updateAudioDNA(registry, sharedData);
            }
        }

        catch (const std::exception& e) {
            juce::Logger::outputDebugString("[MainTabbedComponent::updateAllPanels] Exception: "
                                            + juce::String(e.what()));
        }
    }

    void MainTabbedComponent::smoothAnalyzersPanel(double sampleRateHz)
    {
        if (analyzersPanel_) analyzersPanel_->smoothVisuals(sampleRateHz);
    }

} // namespace mixcoach

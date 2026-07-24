// ═══════════════════════════════════════════════════════════════════════════════
//  PluginEditorBackground.cpp — BACKGROUND ANALYSIS + Editor lifecycle handlers
// ═══════════════════════════════════════════════════════════════════════════════
//
//  INCREMENTO 1: El background loop (telemetry, FFT, envelope tracking) se ha
//  movido a MixCoachBgService (en PluginProcessor). Pero los handlers de
//  ChangeBroadcaster (changeListenerCallback, handleChangeBroadcast, initSharedData)
//  aún necesitan estar aquí para el lifecycle del editor.
// ═══════════════════════════════════════════════════════════════════════════════

#include "PluginEditor.h"
#include "MixCoachBgService.h"
#include "../../Common/types/LogHelper.h"

extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  changeListenerCallback — Despacha a handleChangeBroadcast con SafePointer
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        if (source != &processorRef_.sharedDataChangeBroadcaster_) return;

        if (editorBeingDestroyed_) return;

        __try {
            handleChangeBroadcast();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            earlyCrashLog("CHANGE", "SEH capturado en changeListenerCallback");
        }
    }

    void MixCoachAudioProcessorEditor::handleChangeBroadcast()
    {
        if (editorBeingDestroyed_) return;

        LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: notificaci├│n recibida");

        sharedData_ = processorRef_.getSharedData();

        if (sharedData_ != nullptr && sharedData_->isAvailable()) {
            LogHelper::writeToLog(
                "[MixCoachEditor] ⚡ ChangeBroadcaster: sharedData AHORA disponible, construyendo UI...");

            if (!fullUIBuilt_ || tabbedComponent_ == nullptr) {
                buildFullUI();
                if (tabbedComponent_) {
                    tabbedComponent_->setBounds(getLocalBounds().withTrimmedTop(28));
                    resized();
                    repaint();
                }
            }
            else {
                if (tabbedComponent_) {
                    double sr      = processorRef_.getSampleRate();
                    auto& registry = sharedData_->getSlotRegistry();
                    tabbedComponent_->updateAllPanels(registry, *sharedData_, sr);
                    repaint();
                }
            }
        }
        else {
            LogHelper::writeToLog("[MixCoachEditor] ⚡ ChangeBroadcaster: sharedData sigue NO disponible");
            if (lastInitAttemptMs_ > 0) {
                lastInitAttemptMs_ = 0;
            }
        }
    }

    // ─── Inicialización LIGERA de SharedData (SIN bloquear message thread) ─────
    void MixCoachAudioProcessorEditor::initSharedData()
    {
        if (editorBeingDestroyed_) return;

        if (sharedData_ == nullptr) {
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: obteniendo SharedData singleton...");
            sharedData_ = &SharedData::getInstance();
            LogHelper::writeToLog(juce::String("[MixCoachEditor] initSharedData: sharedData_=")
                                  + (sharedData_ == nullptr ? "NULL" : "OK"));
        }

        if (sharedData_ == nullptr) return;

        if (sharedData_->isAvailable()) {
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: Llamando initBrainModules...");
            bool brainOk = processorRef_.initBrainModules();

            if (brainOk) {
                LogHelper::writeToLog("[MixCoachEditor] initSharedData: Brain Modules listos");

                // INCREMENTO 1: Start bg service if not already running
                if (!processorRef_.getBgService().isRunning()) {
                    processorRef_.getBgService().start(*sharedData_);
                    LogHelper::writeToLog("[MixCoachEditor] initSharedData: Background service started");
                }

                if (!fullUIBuilt_ || tabbedComponent_ == nullptr)
                    processorRef_.sharedDataChangeBroadcaster_.sendChangeMessage();
            }
            else {
                LogHelper::writeToLog("[MixCoachEditor] initSharedData: Brain Modules NO disponibles");
            }
        }

        if (sharedData_ != nullptr && sharedData_->isAvailable() && (!fullUIBuilt_ || tabbedComponent_ == nullptr)) {
            LogHelper::writeToLog("[MixCoachEditor] initSharedData: sharedData OK, construyendo UI...");
            buildFullUI();
            if (tabbedComponent_) {
                tabbedComponent_->setBounds(getLocalBounds().withTrimmedTop(36));
                resized();
                repaint();
            }
        }
    }

} // namespace mixcoach

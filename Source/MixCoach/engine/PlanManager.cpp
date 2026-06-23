#include "PlanManager.h"
#include "ReferenceDrivenEngine.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  PlanStep helpers
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String PlanStep::toTextSummary() const
    {
        juce::String s;
        s += "  " + statusEmoji() + " Paso " + juce::String(stepNumber) + "/" + juce::String(totalSteps) + " — ";

        switch (domain) {
            case Domain::Gain:
                s += "GANANCIA";
                break;
            case Domain::Tonal:
                s += "TONAL";
                break;
            case Domain::Dynamics:
                s += "DINAMICA";
                break;
            case Domain::Spatial:
                s += "ESPACIAL";
                break;
            case Domain::Loudness:
                s += "LOUDNESS";
                break;
        }

        s += ": " + description + "\n";
        s += "       Target: " + juce::String(targetValue, 1) + " | Current: " + juce::String(currentValue, 1)
             + " | Progreso: " + juce::String(static_cast<int>(getProgress() * 100.0f)) + "%\n";
        s += "       " + suggestion + "\n";
        return s;
    }

    juce::String PlanStep::statusEmoji() const noexcept
    {
        switch (status) {
            case PlanStatus::Pending:
                return "\xe2\x97\x8b"; // ○
            case PlanStatus::InProgress:
                return "\xf0\x9f\x94\x84"; // 🔄
            case PlanStatus::Completed:
                return "\xe2\x9c\x85"; // ✅
            case PlanStatus::Skipped:
                return "\xe2\x8f\x8f"; // ⏏
        }
        return "?";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PlanManager implementation
    // ═══════════════════════════════════════════════════════════════════════════

    void PlanManager::updateFromGaps(const std::vector<DomainGap>& gaps)
    {
        if (gaps.empty()) return;

        // Si no hay plan previo, crear uno nuevo desde los gaps
        if (steps_.empty()) {
            steps_.clear();
            steps_.reserve(gaps.size());

            int stepNum = 0;
            for (const auto& gap : gaps) {
                // Saltar gaps de tipo Praise (no son pasos accionables)
                if (gap.severity == GapSeverity::Praise) continue;

                PlanStep step;
                step.stepNumber       = ++stepNum;
                step.totalSteps       = 0; // Se actualiza después
                step.domain           = gap.domain;
                step.originalSeverity = gap.severity;
                step.metric           = gap.metric;
                step.targetValue      = gap.targetValue;
                step.initialValue     = gap.actualValue;
                step.currentValue     = gap.actualValue;
                step.description      = gap.description;
                step.suggestion       = gap.suggestion;
                step.frequencyHint    = gap.frequencyHint;
                step.status           = PlanStatus::Pending;
                step.completedAtUs    = 0;

                steps_.push_back(step);
            }

            // Actualizar totalSteps
            int total = static_cast<int>(steps_.size());
            for (auto& step : steps_) step.totalSteps = total;

            LogHelper::writeToLog("[PlanManager] Nuevo plan creado: " + juce::String(total) + " pasos");
        }
        else {
            // Plan existente — reevaluar progreso
            reevaluate(gaps);
        }
    }

    void PlanManager::reevaluate(const std::vector<DomainGap>& gaps)
    {
        if (steps_.empty() || gaps.empty()) return;

        int updatedCount   = 0;
        int completedCount = 0;

        for (auto& step : steps_) {
            if (step.status == PlanStatus::Completed || step.status == PlanStatus::Skipped) {
                if (step.status == PlanStatus::Completed) completedCount++;
                continue;
            }

            // Buscar gap correspondiente
            for (const auto& gap : gaps) {
                if (gap.domain == step.domain && gap.metric == step.metric) {
                    step.currentValue = gap.actualValue;

                    // Verificar si el gap se resolvió
                    auto gapSeverity = gap.severity;
                    if (gapSeverity == GapSeverity::Praise
                        || (gapSeverity == GapSeverity::Info && step.originalSeverity >= GapSeverity::Warning)) {
                        // El gap mejoró significativamente
                        step.status        = PlanStatus::Completed;
                        step.completedAtUs = juce::Time::getMillisecondCounter() * 1000;
                        completedCount++;
                        updatedCount++;
                        LogHelper::writeToLog("[PlanManager] Paso completado: " + step.metric + " ("
                                              + juce::String(step.initialValue, 1) + " → "
                                              + juce::String(step.currentValue, 1) + ")");
                    }
                    else if (gapSeverity == GapSeverity::Info || std::abs(gap.normalisedGap) < step.getProgress()) {
                        // Hay progreso pero no está completo
                        if (step.status == PlanStatus::Pending) step.status = PlanStatus::InProgress;
                        updatedCount++;
                    }

                    break;
                }
            }
        }

        if (updatedCount > 0)
            LogHelper::writeToLog("[PlanManager] Reevaluacion: " + juce::String(updatedCount) + " pasos actualizados, "
                                  + juce::String(completedCount) + " completados");
    }

    void PlanManager::markCompleted(int stepIndex)
    {
        if (stepIndex >= 0 && stepIndex < static_cast<int>(steps_.size())) {
            steps_[stepIndex].status        = PlanStatus::Completed;
            steps_[stepIndex].completedAtUs = juce::Time::getMillisecondCounter() * 1000;
            LogHelper::writeToLog("[PlanManager] Paso marcado manual como completado: " + steps_[stepIndex].metric);
        }
    }

    void PlanManager::markSkipped(int stepIndex)
    {
        if (stepIndex >= 0 && stepIndex < static_cast<int>(steps_.size())) {
            steps_[stepIndex].status        = PlanStatus::Skipped;
            steps_[stepIndex].completedAtUs = juce::Time::getMillisecondCounter() * 1000;
            LogHelper::writeToLog("[PlanManager] Paso saltado: " + steps_[stepIndex].metric);
        }
    }

    void PlanManager::reset()
    {
        steps_.clear();
        LogHelper::writeToLog("[PlanManager] Plan reseteado");
    }

    int PlanManager::getCompletedSteps() const noexcept
    {
        int count = 0;
        for (const auto& step : steps_)
            if (step.status == PlanStatus::Completed) count++;
        return count;
    }

    float PlanManager::getProgress() const noexcept
    {
        if (steps_.empty()) return 0.0f;
        return static_cast<float>(getCompletedSteps()) / static_cast<float>(steps_.size());
    }

    const PlanStep* PlanManager::getCurrentStep() const noexcept
    {
        for (const auto& step : steps_)
            if (step.status == PlanStatus::Pending || step.status == PlanStatus::InProgress) return &step;
        return nullptr;
    }

    int PlanManager::findStep(const DomainGap& gap) const noexcept
    {
        for (int i = 0; i < static_cast<int>(steps_.size()); ++i) {
            if (steps_[i].domain == gap.domain && steps_[i].metric == gap.metric) return i;
        }
        return -1;
    }

    juce::String PlanManager::toTextSummary() const
    {
        if (steps_.empty()) return {};

        juce::String s;
        s += "[REFERENCE PROGRESS PLAN]\n";
        s += "  Overall progress: " + juce::String(static_cast<int>(getProgress() * 100.0f)) + "% ("
             + juce::String(getCompletedSteps()) + "/" + juce::String(getTotalSteps()) + " steps)\n\n";

        for (const auto& step : steps_) s += step.toTextSummary() + "\n";

        // Siguiente paso
        const auto* current = getCurrentStep();
        if (current != nullptr) {
            s += "  → NEXT STEP: " + current->metric + " — " + current->description + "\n";
            s += "    Try: " + current->suggestion + "\n";
        }
        else if (getTotalSteps() > 0) {
            s += "  🎉 All steps completed! Your mix is aligned with the reference.\n";
        }

        s += "\n";
        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  JSON Persistence
    // ═══════════════════════════════════════════════════════════════════════════

    void PlanManager::saveToJson(juce::DynamicObject& obj) const
    {
        juce::Array<juce::var> stepsArr;

        for (const auto& step : steps_) {
            auto stepObj = juce::DynamicObject::Ptr(new juce::DynamicObject());
            stepObj->setProperty("domain", static_cast<int>(step.domain));
            stepObj->setProperty("originalSeverity", static_cast<int>(step.originalSeverity));
            stepObj->setProperty("stepNumber", step.stepNumber);
            stepObj->setProperty("totalSteps", step.totalSteps);
            stepObj->setProperty("metric", step.metric);
            stepObj->setProperty("targetValue", static_cast<double>(step.targetValue));
            stepObj->setProperty("initialValue", static_cast<double>(step.initialValue));
            stepObj->setProperty("currentValue", static_cast<double>(step.currentValue));
            stepObj->setProperty("description", step.description);
            stepObj->setProperty("suggestion", step.suggestion);
            stepObj->setProperty("frequencyHint", step.frequencyHint);
            stepObj->setProperty("status", static_cast<int>(step.status));
            stepObj->setProperty("completedAtUs", static_cast<int64_t>(step.completedAtUs));
            stepsArr.add(juce::var(stepObj));
        }

        obj.setProperty("planSteps", stepsArr);
    }

    void PlanManager::loadFromJson(const juce::DynamicObject& obj)
    {
        steps_.clear();

        auto stepsArr = obj.getProperty("planSteps").getArray();
        if (stepsArr == nullptr) return;

        for (int i = 0; i < stepsArr->size(); ++i) {
            auto stepObj = (*stepsArr)[i].getDynamicObject();
            if (stepObj == nullptr) continue;

            PlanStep step;
            step.domain = static_cast<Domain>(static_cast<int>(stepObj->getProperty("domain")));
            step.originalSeverity =
                static_cast<GapSeverity>(static_cast<int>(stepObj->getProperty("originalSeverity")));
            step.stepNumber    = stepObj->getProperty("stepNumber");
            step.totalSteps    = stepObj->getProperty("totalSteps");
            step.metric        = stepObj->getProperty("metric").toString();
            step.targetValue   = static_cast<float>(static_cast<double>(stepObj->getProperty("targetValue")));
            step.initialValue  = static_cast<float>(static_cast<double>(stepObj->getProperty("initialValue")));
            step.currentValue  = static_cast<float>(static_cast<double>(stepObj->getProperty("currentValue")));
            step.description   = stepObj->getProperty("description").toString();
            step.suggestion    = stepObj->getProperty("suggestion").toString();
            step.frequencyHint = stepObj->getProperty("frequencyHint").toString();
            step.status        = static_cast<PlanStatus>(static_cast<int>(stepObj->getProperty("status")));
            step.completedAtUs = stepObj->getProperty("completedAtUs");

            steps_.push_back(step);
        }

        if (!steps_.empty())
            LogHelper::writeToLog("[PlanManager] Plan restaurado: " + juce::String((int)steps_.size()) + " pasos");
    }

} // namespace mixcoach

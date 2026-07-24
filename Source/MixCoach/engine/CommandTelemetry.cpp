#include "CommandTelemetry.h"
#include "LlmCommandInterpreter.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String actionTypeToString(int actionType)
    {
        // LlmCommandInterpreter::Action enum values (uint8_t):
        // 0=RevealPanel, 1=SetCoachState, 2=SwitchTab, 3=HighlightTrack,
        // 4=Celebrate, 5=SetMode, 6=ReturnToCoach, 7=AdvancePhase,
        // 8=ShowReport, 9=ShowSuggestions, 10=SpectrumHighlight,
        // 11=MixmapHighlight, 12=AvatarEmotion, 13=ShowIssueCard
        switch (actionType) {
            case 0:  return "reveal_panel";
            case 1:  return "set_coach_state";
            case 2:  return "switch_tab";
            case 3:  return "highlight_track";
            case 4:  return "celebrate";
            case 5:  return "set_mode";
            case 6:  return "return_to_coach";
            case 7:  return "advance_phase";
            case 8:  return "show_report";
            case 9:  return "show_suggestions";
            case 10: return "spectrum_highlight";
            case 11: return "mixmap_highlight";
            case 12: return "avatar_emotion";
            case 13: return "show_issue_card";
            default: return "unknown(" + juce::String(actionType) + ")";
        }
    }

    

    // ═══════════════════════════════════════════════════════════════════════════
    //  CommandTelemetryCollector Implementation
    // ═══════════════════════════════════════════════════════════════════════════

    void CommandTelemetryCollector::recordCommand(const CommandTelemetryEntry& entry)
    {
        const juce::ScopedLock lock(mutex_);

        // Actualizar buffer circular
        int idx = entryIndex_;
        entries_[idx] = entry;
        entryIndex_ = (idx + 1) % kMaxEntries;
        if (entryCount_ < kMaxEntries)
            ++entryCount_;

        // Actualizar contadores atómicos
        totalCommands_.fetch_add(1, std::memory_order_relaxed);
        if (entry.success)
            successfulCommands_.fetch_add(1, std::memory_order_relaxed);
        else
            failedCommands_.fetch_add(1, std::memory_order_relaxed);

        // Actualizar stats por tipo de comando
        int action = entry.actionType;
        if (action >= 0 && action < 16) {
            actionCounts_[action].fetch_add(1, std::memory_order_relaxed);
            if (!entry.success)
                actionFails_[action].fetch_add(1, std::memory_order_relaxed);
        }

        // Loggear comando
        juce::String logMsg = "[CmdTelemetry] " + entry.actionName
                              + " " + (entry.success ? "OK" : "FAIL")
                              + " | " + entry.paramSummary;
        if (!entry.success && entry.failReason.isNotEmpty())
            logMsg += " | reason: " + entry.failReason;
        LogHelper::writeToLog(logMsg);
    }

    void CommandTelemetryCollector::recordUserPreference(const UserPreferenceSignal& signal)
    {
        const juce::ScopedLock lock(mutex_);

        int idx = prefIndex_;
        preferences_[idx] = signal;
        prefIndex_ = (idx + 1) % kMaxPreferences;
        if (prefCount_ < kMaxPreferences)
            ++prefCount_;

        LogHelper::writeToLog("[CmdTelemetry] UserPreference: action="
                              + signal.actionName + " outcome="
                              + juce::String(static_cast<int>(signal.outcome)));
    }

    TelemetrySummary CommandTelemetryCollector::getSummary() const
    {
        const juce::ScopedLock lock(mutex_);

        TelemetrySummary summary;
        summary.totalCommands       = totalCommands_.load(std::memory_order_relaxed);
        summary.successfulCommands  = successfulCommands_.load(std::memory_order_relaxed);
        summary.failedCommands      = failedCommands_.load(std::memory_order_relaxed);
        summary.successRate         = getSuccessRate();

        // Últimos 5 comandos
        int count = juce::jmin(entryCount_, kMaxEntries);
        int start = (entryIndex_ - juce::jmin(5, count) + kMaxEntries) % kMaxEntries;
        for (int i = 0; i < juce::jmin(5, count); ++i) {
            int idx = (start + i) % kMaxEntries;
            summary.recentEntries.push_back(entries_[idx]);
        }

        // Top comandos
        summary.topCommands = getCommandFrequencies(5);

        // Preferencias
        summary.userFavorites = getUserFavorites();

        return summary;
    }

    std::vector<CommandFrequency> CommandTelemetryCollector::getCommandFrequencies(int topN) const
    {
        const juce::ScopedLock lock(mutex_);

        // Recoger todos los tipos de comando con al menos 1 ejecución
        std::vector<CommandFrequency> freqs;
        for (int i = 0; i < 14; ++i) {  // 14 action types
            int count = actionCounts_[i].load(std::memory_order_relaxed);
            if (count > 0) {
                CommandFrequency freq;
                freq.actionType = i;
                freq.name       = actionTypeToString(i);
                freq.count      = count;
                freq.failCount  = actionFails_[i].load(std::memory_order_relaxed);
                freq.failRate   = static_cast<float>(freq.failCount) / static_cast<float>(count);
                freqs.push_back(freq);
            }
        }

        // Ordenar por count descendente
        std::sort(freqs.begin(), freqs.end());

        // Top N
        if (topN > 0 && static_cast<int>(freqs.size()) > topN)
            freqs.resize(topN);

        return freqs;
    }

    float CommandTelemetryCollector::getFailureRate(int actionType) const
    {
        if (actionType < 0 || actionType >= 14)
            return 0.0f;
        int count = actionCounts_[actionType].load(std::memory_order_relaxed);
        if (count == 0) return 0.0f;
        int fails = actionFails_[actionType].load(std::memory_order_relaxed);
        return static_cast<float>(fails) / static_cast<float>(count);
    }

    std::vector<CommandTelemetryEntry> CommandTelemetryCollector::getRecentCommands(int count) const
    {
        const juce::ScopedLock lock(mutex_);

        int actual = juce::jmin(count, entryCount_);
        std::vector<CommandTelemetryEntry> result;
        result.reserve(actual);

        int start = (entryIndex_ - actual + kMaxEntries) % kMaxEntries;
        for (int i = 0; i < actual; ++i) {
            int idx = (start + i) % kMaxEntries;
            result.push_back(entries_[idx]);
        }

        return result;
    }

    std::vector<CommandFrequency> CommandTelemetryCollector::getUserFavorites() const
    {
        // Las preferencias del usuario se infieren de los comandos con
        // alta tasa de éxito Y alta frecuencia de uso.
        // Esto indica que el LLM está usando comandos que al usuario
        // le resultan útiles (no los ignora).
        auto freqs = getCommandFrequencies();
        
        // Filtrar solo comandos con >= 70% de éxito y al menos 3 ejecuciones
        std::vector<CommandFrequency> favorites;
        int minCount = juce::jmin(3, totalCommands_.load(std::memory_order_relaxed));
        for (const auto& f : freqs) {
            if (f.count >= minCount && f.failRate <= 0.30f)
                favorites.push_back(f);
        }

        return favorites;
    }

    float CommandTelemetryCollector::getSuccessRate() const noexcept
    {
        int total = totalCommands_.load(std::memory_order_relaxed);
        if (total == 0) return 1.0f;
        int success = successfulCommands_.load(std::memory_order_relaxed);
        return static_cast<float>(success) / static_cast<float>(total);
    }

    juce::String CommandTelemetryCollector::toLLMContext() const
    {
        const juce::ScopedLock lock(mutex_);

        int total    = totalCommands_.load(std::memory_order_relaxed);
        int success  = successfulCommands_.load(std::memory_order_relaxed);
        int failed   = failedCommands_.load(std::memory_order_relaxed);

        if (total == 0)
            return "[CMD TELEMETRY] No commands recorded yet in this session.\n";

        juce::String s;
        s += "[CMD TELEMETRY - UI Command Usage]\n";
        s += "  Total: " + juce::String(total) + " commands | "
             + juce::String(success) + " OK | "
             + juce::String(failed) + " FAIL | "
             + juce::String(static_cast<int>(getSuccessRate() * 100.0f)) + "% success\n\n";

        // Top comandos
        auto freqs = getCommandFrequencies(5);
        if (!freqs.empty()) {
            s += "  Most used commands:\n";
            for (const auto& f : freqs) {
                s += "    " + juce::String(f.count) + "x " + f.name;
                if (f.failCount > 0)
                    s += " (" + juce::String(static_cast<int>(f.failRate * 100.0f)) + "% fail)";
                s += "\n";
            }
        }

        // Últimos comandos
        auto recent = getRecentCommands(3);
        if (!recent.empty()) {
            s += "\n  Last commands:\n";
            for (const auto& e : recent) {
                s += juce::String("    ") + (e.success ? "OK" : "FAIL") + " " + e.actionName;
                if (e.paramSummary.isNotEmpty())
                    s += " (" + e.paramSummary + ")";
                s += "\n";
            }
        }

        // Preferencias del usuario
        auto favs = getUserFavorites();
        if (!favs.empty()) {
            s += "\n  User prefers: ";
            for (size_t i = 0; i < favs.size(); ++i) {
                if (i > 0) s += ", ";
                s += favs[i].name + " (" + juce::String(favs[i].count) + "x)";
            }
            s += "\n";
        }

        s += "[END CMD TELEMETRY]\n";
        return s;
    }

    juce::String CommandTelemetryCollector::toShortText() const
    {
        int total   = totalCommands_.load(std::memory_order_relaxed);
        int success = successfulCommands_.load(std::memory_order_relaxed);

        return "[CmdTelemetry] " + juce::String(total) + " total, "
               + juce::String(success) + " OK, "
               + juce::String(static_cast<int>(getSuccessRate() * 100.0f)) + "% success";
    }

    void CommandTelemetryCollector::reset()
    {
        const juce::ScopedLock lock(mutex_);

        totalCommands_.store(0, std::memory_order_relaxed);
        successfulCommands_.store(0, std::memory_order_relaxed);
        failedCommands_.store(0, std::memory_order_relaxed);

        for (auto& c : actionCounts_)
            c.store(0, std::memory_order_relaxed);
        for (auto& f : actionFails_)
            f.store(0, std::memory_order_relaxed);

        entryCount_ = 0;
        entryIndex_ = 0;
        prefCount_  = 0;
        prefIndex_  = 0;

        LogHelper::writeToLog("[CmdTelemetry] Reset");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Serialization — JSON (via juce::DynamicObject)
    // ═══════════════════════════════════════════════════════════════════════════

    #define ID(s) juce::Identifier(juce::String(s))

    void CommandTelemetryCollector::toJson(juce::DynamicObject& obj) const
    {
        const juce::ScopedLock lock(mutex_);

        obj.setProperty(ID("version"), 1);
        obj.setProperty(ID("totalCommands"),      totalCommands_.load(std::memory_order_relaxed));
        obj.setProperty(ID("successfulCommands"), successfulCommands_.load(std::memory_order_relaxed));
        obj.setProperty(ID("failedCommands"),     failedCommands_.load(std::memory_order_relaxed));

        // Guardar actionCounts como array JSON
        juce::Array<juce::var> counts, fails;
        for (int i = 0; i < 14; ++i) {
            counts.add(actionCounts_[i].load(std::memory_order_relaxed));
            fails.add(actionFails_[i].load(std::memory_order_relaxed));
        }
        obj.setProperty(ID("actionCounts"), counts);
        obj.setProperty(ID("actionFails"),  fails);
    }

    void CommandTelemetryCollector::fromJson(const juce::DynamicObject& obj, CommandTelemetryCollector& collector){
        

        if (obj.hasProperty(ID("totalCommands")))
            collector.totalCommands_.store(
                static_cast<int>(obj.getProperty(ID("totalCommands"))),
                std::memory_order_relaxed);
        if (obj.hasProperty(ID("successfulCommands")))
            collector.successfulCommands_.store(
                static_cast<int>(obj.getProperty(ID("successfulCommands"))),
                std::memory_order_relaxed);
        if (obj.hasProperty(ID("failedCommands")))
            collector.failedCommands_.store(
                static_cast<int>(obj.getProperty(ID("failedCommands"))),
                std::memory_order_relaxed);

        // Restaurar actionCounts
        auto countsVar = obj.getProperty(ID("actionCounts"));
        if (countsVar.isArray()) {
            auto* arr = countsVar.getArray();
            for (int i = 0; i < juce::jmin(14, arr->size()); ++i)
                collector.actionCounts_[i].store(static_cast<int>((*arr)[i]), std::memory_order_relaxed);
        }

        auto failsVar = obj.getProperty(ID("actionFails"));
        if (failsVar.isArray()) {
            auto* arr = failsVar.getArray();
            for (int i = 0; i < juce::jmin(14, arr->size()); ++i)
                collector.actionFails_[i].store(static_cast<int>((*arr)[i]), std::memory_order_relaxed);
        }

        
    }

#undef ID

} // namespace mixcoach

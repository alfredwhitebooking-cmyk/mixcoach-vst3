#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <cstdint>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionEvent — Un evento individual en el timeline de la sesión
    //
    //  Cada evento registra:
    //    • Tipo de evento (qué ocurrió)
    //    • Timestamp (μs desde inicio del plugin)
    //    • Descripción legible para mostrar en el reporte
    //    • Valor numérico opcional (score, fase, etc.)
    // ═══════════════════════════════════════════════════════════════════════════
    struct SessionEvent
    {
        enum class Type : uint8_t
        {
            // ─── Setup ──────────────────────────────────────────────────────
            SetupStarted,          // Usuario inició el setup
            SetupNameEntered,      // Usuario ingresó su nombre
            SetupModeSelected,     // Modo seleccionado (Mix/Master)
            SetupGenreSelected,    // Género seleccionado
            ReferenceLoaded,       // Archivo de referencia cargado
            ReferenceAnalyzed,     // Referencia analizada OK
            ReferenceSkipped,      // Usuario saltó la referencia

            // ─── Organización ───────────────────────────────────────────────
            SessionPrepped,        // Checklist de sesión completado
            SessionScanned,        // Escaneo de sesión completado
            MixMapGenerated,       // Mix Map generado
            MixMapConfirmed,       // Mix Map confirmado por el usuario
            SetupSkipped,          // Usuario saltó todo el setup

            // ─── Coaching ───────────────────────────────────────────────────
            CoachingStarted,       // Coaching activo comenzó
            GainStagingPhase,      // Fase Gain Staging iniciada
            BalancePhase,          // Fase Balance iniciada
            EQPhase,               // Fase EQ iniciada
            CompressionPhase,      // Fase Compresión iniciada
            SpacePhase,            // Fase Espacio iniciada
            RefinementPhase,       // Fase Refinement iniciada
            MasterCheckPhase,      // Fase Master Check iniciada
            PhaseComplete,         // Fase completada
            CoachingComplete,       // Todo el coaching completado

            // ─── Correcciones ───────────────────────────────────────────────
            CorrectionIssued,      // Coach recomendó un cambio
            CorrectionApplied,     // Usuario aplicó el cambio
            CorrectionVerified,    // Cambio verificado OK
            CorrectionFailed,      // Cambio no detectado
            PluginApplied,         // Usuario aplicó un plugin específico

            // ─── Reporte ────────────────────────────────────────────────────
            ReportGenerated,       // Reporte generado
            ReportExported,        // Reporte exportado a HTML

            Count                  // Total de tipos de evento
        };

        Type type = Type::SetupStarted;
        int64_t timestampUs = 0; // μs desde epoch (Time::getMillisecondCounter() * 1000)
        juce::String description; // Texto legible: "Género: Afrobeat"
        int numericValue = 0;     // Opcional: score, phase index, etc.

        /** Retorna el icono emoji para este tipo de evento. */
        [[nodiscard]] static const char* iconFor(Type type) noexcept
        {
            switch (type) {
                case Type::SetupStarted:          return "\xF0\x9F\x9A\x80"; // 🚀
                case Type::SetupNameEntered:      return "\xF0\x9F\x91\xA4"; // 👤
                case Type::SetupModeSelected:     return "[GEAR]"; // ⚙️
                case Type::SetupGenreSelected:    return "[MUSIC]"; // 🎵
                case Type::ReferenceLoaded:       return "[REPORT]"; // 📂
                case Type::ReferenceAnalyzed:     return "[DONE]";     // ✅
                case Type::ReferenceSkipped:      return "\xE2\x8F\xAF";     // ⏯
                case Type::SessionPrepped:        return "[NOTES]"; // 📋
                case Type::SessionScanned:        return "[SEARCH]"; // 🔍
                case Type::MixMapGenerated:       return "\xF0\x9F\x97\xBA"; // 🗺
                case Type::MixMapConfirmed:       return "[DONE]";     // ✅
                case Type::SetupSkipped:          return "[BOLT]";     // ⚡
                case Type::CoachingStarted:       return "\xF0\x9F\x8E\xAE"; // 🎮
                case Type::GainStagingPhase:      return "\xF0\x9F\x94\x8A"; // 🔊
                case Type::BalancePhase:          return "\xE2\x9A\x96"; // ⚖️
                case Type::EQPhase:               return "[TREND]"; // 📈
                case Type::CompressionPhase:      return "[TREND]"; // 📉
                case Type::SpacePhase:            return "\xF0\x9F\x8C\x8A"; // 🌊
                case Type::RefinementPhase:       return "\xF0\x9F\x92\x8E"; // 💎
                case Type::MasterCheckPhase:      return "[CHART]"; // 📊
                case Type::PhaseComplete:         return "[DONE]";     // ✅
                case Type::CoachingComplete:      return "[TROPHY]"; // 🏆
                case Type::CorrectionIssued:      return "[CHANGE]"; // 🔧
                case Type::CorrectionApplied:     return "[OK]";     // ✓
                case Type::CorrectionVerified:    return "[DONE]";     // ✅
                case Type::CorrectionFailed:      return "\xE2\x9D\x8C";     // ❌
                case Type::PluginApplied:         return "[PLUGIN]"; // 🔌
                case Type::ReportGenerated:       return "[EXPORT]"; // 📄
                case Type::ReportExported:        return "\xF0\x9F\x93\xA5"; // 📥
                default:                          return "\xE2\x80\xA2";     // •
            }
        }

        /** Retorna nombre corto del tipo (debugging). */
        [[nodiscard]] static const char* typeName(Type type) noexcept
        {
            switch (type) {
                case Type::SetupStarted:          return "SetupStarted";
                case Type::SetupNameEntered:      return "NameEntered";
                case Type::SetupModeSelected:     return "ModeSelected";
                case Type::SetupGenreSelected:    return "GenreSelected";
                case Type::ReferenceLoaded:       return "RefLoaded";
                case Type::ReferenceAnalyzed:     return "RefAnalyzed";
                case Type::ReferenceSkipped:      return "RefSkipped";
                case Type::SessionPrepped:        return "SessionPrepped";
                case Type::SessionScanned:        return "SessionScanned";
                case Type::MixMapGenerated:       return "MixMapGen";
                case Type::MixMapConfirmed:       return "MixMapConfirmed";
                case Type::SetupSkipped:          return "SetupSkipped";
                case Type::CoachingStarted:       return "CoachingStarted";
                case Type::GainStagingPhase:      return "GainStaging";
                case Type::BalancePhase:          return "Balance";
                case Type::EQPhase:               return "EQ";
                case Type::CompressionPhase:      return "Compression";
                case Type::SpacePhase:            return "Space";
                case Type::RefinementPhase:       return "Refinement";
                case Type::MasterCheckPhase:      return "MasterCheck";
                case Type::PhaseComplete:         return "PhaseComplete";
                case Type::CoachingComplete:      return "CoachingComplete";
                case Type::CorrectionIssued:      return "CorrectionIssued";
                case Type::CorrectionApplied:     return "CorrectionApplied";
                case Type::CorrectionVerified:    return "CorrectionVerified";
                case Type::CorrectionFailed:      return "CorrectionFailed";
                case Type::PluginApplied:         return "PluginApplied";
                case Type::ReportGenerated:       return "ReportGenerated";
                case Type::ReportExported:        return "ReportExported";
                default:                          return "Unknown";
            }
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionEventLog — Buffer circular de eventos de sesión
    //
    //  Registra hasta kMaxEvents eventos con timestamp para mostrar un timeline
    //  en el reporte final y tener analytics de progresión. Cada evento guarda
    //  tipo, timestamp (μs), descripción legible, y valor numérico opcional.
    //
    //  Los eventos se registran desde:
    //    • NavigationShell::setCoachRoomState — cambios de estado UI
    //    • CoachEngine — setup steps, fases, correcciones
    //    • ExperienceManager — transiciones de etapa
    //
    //  El log es serializable (toJson/fromJson) para persistencia entre sesiones.
    // ═══════════════════════════════════════════════════════════════════════════
    class SessionEventLog
    {
    public:
        /** Número máximo de eventos en el buffer circular.
            Si se excede, los eventos más antiguos se descartan. */
        static constexpr int kMaxEvents = 256;

        SessionEventLog() = default;

        // ─── API de registro ────────────────────────────────────────────────
        /** Registra un nuevo evento con timestamp automático.
            @param type         Tipo de evento
            @param description  Texto legible ("Género: Afrobeat")
            @param numericValue Valor numérico opcional (score, phase index) */
        void logEvent(SessionEvent::Type type,
                      const juce::String& description = {},
                      int numericValue = 0)
        {
            SessionEvent ev;
            ev.type = type;
            ev.timestampUs = juce::Time::getMillisecondCounter() * 1000;
            ev.description = description;
            ev.numericValue = numericValue;

            events_.push_back(std::move(ev));

            // Mantener tamaño máximo (descartar el más antiguo)
            if (events_.size() > kMaxEvents)
                events_.erase(events_.begin());
        }

        /** Retorna todos los eventos registrados. */
        [[nodiscard]] const std::vector<SessionEvent>& getEvents() const noexcept
        {
            return events_;
        }

        /** Retorna eventos de un tipo específico. */
        [[nodiscard]] std::vector<SessionEvent> getEventsByType(SessionEvent::Type type) const
        {
            std::vector<SessionEvent> result;
            for (const auto& e : events_)
                if (e.type == type)
                    result.push_back(e);
            return result;
        }

        /** Retorna eventos desde un timestamp (para timeline incremental). */
        [[nodiscard]] std::vector<SessionEvent> getEventsSince(int64_t sinceUs) const
        {
            std::vector<SessionEvent> result;
            for (const auto& e : events_)
                if (e.timestampUs > sinceUs)
                    result.push_back(e);
            return result;
        }

        /** Retorna el timestamp del primer evento (inicio de sesión). */
        [[nodiscard]] int64_t getSessionStartUs() const noexcept
        {
            if (events_.empty()) return 0;
            return events_.front().timestampUs;
        }

        /** Retorna la duración de la sesión en μs desde el primer evento. */
        [[nodiscard]] int64_t getSessionDurationUs() const noexcept
        {
            if (events_.size() < 2) return 0;
            return events_.back().timestampUs - events_.front().timestampUs;
        }

        /** Retorna el número total de eventos registrados. */
        [[nodiscard]] int getEventCount() const noexcept
        {
            return static_cast<int>(events_.size());
        }

        /** Limpia todos los eventos. */
        void clear() noexcept { events_.clear(); }

        // ─── Serialización JSON ─────────────────────────────────────────────
        void toJson(juce::DynamicObject& obj) const
        {
            juce::Array<juce::var> arr;
            for (const auto& e : events_) {
                auto* evObj = new juce::DynamicObject();
                evObj->setProperty("type", static_cast<int>(e.type));
                evObj->setProperty("ts", static_cast<juce::int64>(e.timestampUs));
                evObj->setProperty("desc", e.description);
                evObj->setProperty("val", e.numericValue);
                arr.add(juce::var(evObj));
            }
            obj.setProperty("events", arr);
        }

        static SessionEventLog fromJson(const juce::DynamicObject& obj)
        {
            SessionEventLog log;
            auto* arr = obj.getProperty("events").getArray();
            if (arr == nullptr) return log;

            for (const auto& v : *arr) {
                auto* evObj = v.getDynamicObject();
                if (evObj == nullptr) continue;

                SessionEvent ev;
                ev.type = static_cast<SessionEvent::Type>(
                    static_cast<int>(evObj->getProperty("type")));
                ev.timestampUs = static_cast<int64_t>(
                    static_cast<juce::int64>(evObj->getProperty("ts")));
                ev.description = evObj->getProperty("desc").toString();
                ev.numericValue = evObj->getProperty("val");

                if (log.events_.size() < kMaxEvents)
                    log.events_.push_back(std::move(ev));
            }
            return log;
        }

    private:
        std::vector<SessionEvent> events_;
    };

} // namespace mixcoach

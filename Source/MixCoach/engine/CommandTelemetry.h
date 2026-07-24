#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>

namespace mixcoach {

    // Forward declarations
    class LlmCommandInterpreter;

    // ═══════════════════════════════════════════════════════════════════════════
    //  CommandTelemetryEntry — Una ejecución de comando registrada
    //
    //  Se crea cada vez que el LlmCommandInterpreter procesa o ejecuta un
    //  comando UI. Almacena timestamp, acción, éxito/fallo, y contexto.
    //
    //  Se usa para:
    //    • Estadísticas: qué comandos se usan más, cuáles fallan más
    //    • Preferencias de usuario: qué acciones tolera/ignora
    //    • Diagnóstico: por qué falló un comando
    //    • Feedback al LLM: el sistema informa al LLM sobre su uso de comandos
    // ═══════════════════════════════════════════════════════════════════════════
    struct CommandTelemetryEntry
    {
        int64_t timestampUs      = 0;   // Cuándo se ejecutó
        int actionType           = -1;  // LlmCommandInterpreter::Action como int
        juce::String actionName;        // "reveal_panel", "switch_tab", etc.
        bool success             = false; // true = se ejecutó correctamente
        juce::String paramSummary;      // Resumen breve de parámetros (ej: "panel=reference")
        juce::String failReason;        // Por qué falló (vacío si success=true)
        double parseTimeMs       = 0.0; // Tiempo de parseo (solo en entry de batch)
        int commandIndex         = -1;  // Posición en el batch de comandos
        int batchSize            = 0;   // Total de comandos en este batch
        juce::String coachMessage;      // Fragmento del mensaje del coach que acompañó el comando
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CommandFrequency — Conteo de uso por tipo de comando
    // ═══════════════════════════════════════════════════════════════════════════
    struct CommandFrequency
    {
        int actionType      = -1;
        juce::String name;  // "reveal_panel"
        int count           = 0;  // Veces ejecutado
        int failCount       = 0;  // Veces que falló
        float failRate      = 0.0f; // failCount / count

        // Orden descendente por count
        bool operator<(const CommandFrequency& other) const noexcept
        {
            return count > other.count;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  UserPreferenceSignal — Señal de preferencia del usuario
    //
    //  Detecta patrones de comportamiento del usuario frente a comandos:
    //    • Accepted: El usuario aceptó la recomendación que acompañaba al comando
    //    • Ignored: El usuario ignoró el comando (no interactuó con el panel/track)
    //    • Rejected: El usuario hizo lo opuesto o canceló
    // ═══════════════════════════════════════════════════════════════════════════
    struct UserPreferenceSignal
    {
        int64_t timestampUs = 0;
        int actionType      = -1;
        juce::String actionName;

        enum class Outcome : uint8_t
        {
            Unknown,   // No se ha detectado feedback aún
            Accepted,  // El usuario interactuó positivamente
            Ignored,   // El usuario ignoró (no hubo interacción)
            Rejected   // El usuario hizo lo contrario o descartó
        };
        Outcome outcome = Outcome::Unknown;

        juce::String context; // Descripción del contexto
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  TelemetrySummary — Resumen agregado para el LLM
    // ═══════════════════════════════════════════════════════════════════════════
    struct TelemetrySummary
    {
        int totalCommands       = 0;
        int successfulCommands  = 0;
        int failedCommands      = 0;
        float successRate       = 0.0f;

        // Últimos 5 comandos ejecutados (para contexto inmediato)
        std::vector<CommandTelemetryEntry> recentEntries;

        // Comandos más frecuentes (top 5)
        std::vector<CommandFrequency> topCommands;

        // Preferencias detectadas del usuario (top 3 acciones favoritas)
        std::vector<CommandFrequency> userFavorites;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CommandTelemetryCollector — Colector de telemetría de comandos UI
    //
    //  Singleton ligero que acumula datos de ejecución de comandos del LLM.
    //  Thread-safe para escritura desde cualquier thread.
    //
    //  Proporciona:
    //    • Buffer circular de últimos N comandos
    //    • Estadísticas agregadas por tipo de comando
    //    • Generación de resumen para inyectar en contexto del LLM
    //    • Serialización para persistencia entre sesiones
    // ═══════════════════════════════════════════════════════════════════════════
    class CommandTelemetryCollector
    {
    public:
        CommandTelemetryCollector() = default;

        // ─── Recording ──────────────────────────────────────────────────────

        /** Registra un comando ejecutado. Thread-safe. */
        void recordCommand(const CommandTelemetryEntry& entry);

        /** Registra una señal de preferencia del usuario. Thread-safe. */
        void recordUserPreference(const UserPreferenceSignal& signal);

        // ─── Query ──────────────────────────────────────────────────────────

        /** Retorna un resumen agregado de toda la telemetría. */
        [[nodiscard]] TelemetrySummary getSummary() const;

        /** Retorna las frecuencias de comandos, ordenadas por uso descendente. */
        [[nodiscard]] std::vector<CommandFrequency> getCommandFrequencies(int topN = 10) const;

        /** Retorna la tasa de fallo por tipo de comando. */
        [[nodiscard]] float getFailureRate(int actionType) const;

        /** Retorna los últimos N comandos ejecutados. */
        [[nodiscard]] std::vector<CommandTelemetryEntry> getRecentCommands(int count = 10) const;

        /** Retorna las preferencias detectadas del usuario. */
        [[nodiscard]] std::vector<CommandFrequency> getUserFavorites() const;

        /** Retorna el total de comandos registrados. */
        [[nodiscard]] int getTotalCommands() const noexcept { return totalCommands_.load(); }

        /** Retorna el total de comandos exitosos. */
        [[nodiscard]] int getSuccessfulCommands() const noexcept { return successfulCommands_.load(); }

        /** Retorna el total de comandos fallidos. */
        [[nodiscard]] int getFailedCommands() const noexcept { return failedCommands_.load(); }

        /** Retorna la tasa de éxito global (0.0-1.0). */
        [[nodiscard]] float getSuccessRate() const noexcept;

        // ─── LLM Context ────────────────────────────────────────────────────

        /** Genera un bloque de texto formateado para inyectar en el prompt del LLM.
         *  Incluye: totales, tasa de éxito, top comandos, preferencias detectadas,
         *  y últimos comandos ejecutados. */
        [[nodiscard]] juce::String toLLMContext() const;

        /** Versión corta para debugging. */
        [[nodiscard]] juce::String toShortText() const;

        // ─── Lifecycle ──────────────────────────────────────────────────────

        /** Resetea toda la telemetría. */
        void reset();

        /** Serializa la telemetría a un DynamicObject. */
        void toJson(juce::DynamicObject& obj) const;

        /** Deserializa telemetría desde un DynamicObject. */
        static void fromJson(const juce::DynamicObject& obj, CommandTelemetryCollector& out);

    private:
        // ─── Constantes ───────────────────────────────────────────────────
        static constexpr int kMaxEntries     = 500;
        static constexpr int kMaxPreferences = 100;

        // ─── Almacenamiento ──────────────────────────────────────────────
        // Buffer circular de entradas recientes
        std::array<CommandTelemetryEntry, kMaxEntries> entries_;
        int entryCount_ = 0;  // Total entradas registradas (para saber si el buffer dio la vuelta)
        int entryIndex_ = 0;  // Índice actual en el buffer circular

        // Preferencias de usuario
        std::array<UserPreferenceSignal, kMaxPreferences> preferences_;
        int prefCount_  = 0;
        int prefIndex_  = 0;

        // ─── Estadísticas agregadas (thread-safe) ────────────────────────
        mutable juce::CriticalSection mutex_;
        std::atomic<int> totalCommands_{0};
        std::atomic<int> successfulCommands_{0};
        std::atomic<int> failedCommands_{0};

        // Frecuencia por tipo de comando (mapeado de Action → count)
        mutable std::array<std::atomic<int>, 16> actionCounts_{};  // 14 actions + padding
        mutable std::array<std::atomic<int>, 16> actionFails_{};
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    /** Convierte un Action enum a string legible. */
    juce::String actionTypeToString(int actionType);  // Pass static_cast<int>(LlmCommandInterpreter::Action)

    /** Convierte un UiCommand a un resumen de parámetros. */

} // namespace mixcoach

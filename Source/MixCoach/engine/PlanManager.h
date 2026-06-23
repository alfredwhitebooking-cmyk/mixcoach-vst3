#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace mixcoach {

// Forward declarations from ReferenceDrivenEngine — avoids circular include
// because ReferenceDrivenEngine.h includes CoachEngine.h
// Domain, GapSeverity, and DomainGap are defined inside this namespace
// in ReferenceDrivenEngine.h
enum class Domain : uint8_t;
enum class GapSeverity : uint8_t;
struct DomainGap;

// ═══════════════════════════════════════════════════════════════════════════
//  PlanStatus — Estado de cada paso del plan
// ═══════════════════════════════════════════════════════════════════════════
enum class PlanStatus : uint8_t {
    Pending,     // No se ha empezado a trabajar en este paso
    InProgress,  // El usuario ha empezado a ajustar (gap se redujo)
    Completed,   // Gap resuelto (dentro de tolerancia)
    Skipped      // Usuario saltó este paso
};

// ═══════════════════════════════════════════════════════════════════════════
//  PlanStep — Un paso concreto del plan secuencial
//
//  Cada DomainGap del ReferenceDrivenEngine se convierte en un PlanStep.
//  Los pasos se numeran 1..N en el orden recomendado.
//  El PlanManager trackea el progreso y persiste el estado.
// ═══════════════════════════════════════════════════════════════════════════
struct PlanStep {
    Domain       domain;         // Gain, Tonal, Dynamics, Spatial, Loudness
    GapSeverity  originalSeverity; // Severidad cuando se creó el paso
    int          stepNumber;     // 1-based: "Paso 1/8"
    int          totalSteps;     // Total de pasos en el plan

    juce::String metric;         // "LUFS Integrated", "Sub (0-86Hz)", etc.
    float        targetValue;    // Valor objetivo (de la referencia)
    float        initialValue;   // Valor cuando se creó el paso
    float        currentValue;   // Valor actual (actualizado en reevaluate)

    juce::String description;    // Descripción textual del gap
    juce::String suggestion;     // Sugerencia de acción
    juce::String frequencyHint;  // Frecuencia sugerida (para EQ)

    PlanStatus   status;         // Estado actual
    int64_t      completedAtUs;  // Timestamp si completado o saltado

    // Progreso del paso (0.0 = sin mejora, 1.0 = completado)
    [[nodiscard]] float getProgress() const noexcept
    {
        float gap = std::abs(initialValue - targetValue);
        if (gap < 0.1f) return 1.0f;
        float currentGap = std::abs(currentValue - targetValue);
        return juce::jlimit(0.0f, 1.0f, 1.0f - (currentGap / gap));
    }

    [[nodiscard]] juce::String toTextSummary() const;
    [[nodiscard]] juce::String statusEmoji() const noexcept;
};

// ═══════════════════════════════════════════════════════════════════════════
//  PlanManager — Gestiona el plan secuencial contra la referencia
//
//  Toma los DomainGap[] del ReferenceDrivenEngine y los organiza como
//  pasos secuenciales. Cuando los gaps cambian (porque el usuario ajustó),
//  reevalúa el progreso y marca pasos como completados.
//
//  Persistencia:
//    planManager.saveToJson(obj) → se almacena dentro de session_memory.json
//    planManager.loadFromJson(obj) → se restaura al cargar la sesión
// ═══════════════════════════════════════════════════════════════════════════
class PlanManager {
public:
    PlanManager() = default;

    // ─── Gestión del plan ─────────────────────────────────────────────────
    /** Toma los gaps actuales y actualiza el plan.
        Si no hay plan previo, crea uno nuevo.
        Si ya hay plan, reevalúa el progreso de cada paso.
        @param gaps Gaps priorizados del ReferenceDrivenEngine */
    void updateFromGaps(const std::vector<DomainGap>& gaps);

    /** Reevalúa el progreso sin crear nuevos pasos (para llamadas periódicas). */
    void reevaluate(const std::vector<DomainGap>& gaps);

    /** Marca un paso manualmente como completado. */
    void markCompleted(int stepIndex);

    /** Marca un paso manualmente como saltado. */
    void markSkipped(int stepIndex);

    /** Resetea todo el plan (cuando se cambia de referencia). */
    void reset();

    // ─── Consultas ────────────────────────────────────────────────────────
    [[nodiscard]] const std::vector<PlanStep>& getSteps() const noexcept { return steps_; }
    [[nodiscard]] int getTotalSteps() const noexcept { return static_cast<int>(steps_.size()); }
    [[nodiscard]] int getCompletedSteps() const noexcept;
    [[nodiscard]] float getProgress() const noexcept; // 0.0 a 1.0
    [[nodiscard]] bool hasPlan() const noexcept { return !steps_.empty(); }

    /** Retorna el siguiente paso no completado. */
    [[nodiscard]] const PlanStep* getCurrentStep() const noexcept;

    /** Texto formateado para el contexto del LLM. */
    [[nodiscard]] juce::String toTextSummary() const;

    // ─── Persistencia JSON ────────────────────────────────────────────────
    /** Guarda el plan a un objeto DynamicObject (para session_memory.json). */
    void saveToJson(juce::DynamicObject& obj) const;
    /** Carga el plan desde un objeto DynamicObject. */
    void loadFromJson(const juce::DynamicObject& obj);

private:
    std::vector<PlanStep> steps_;

    /** Busca un paso existente con el mismo domain + metric. */
    [[nodiscard]] int findStep(const DomainGap& gap) const noexcept;
};

} // namespace mixcoach

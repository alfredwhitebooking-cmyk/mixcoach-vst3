#include "MixPriorityEngine.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  getRoleWeight — Busca el peso de importancia para un TrackRole
    // ═══════════════════════════════════════════════════════════════════════════

    uint8_t MixPriorityEngine::getRoleWeight(TrackRole role) noexcept
    {
        for (const auto& entry : kRoleWeights) {
            if (entry.first == role)
                return entry.second;
        }
        return 1; // Unknown default
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getRoleWeightLabel — Retorna label textual para el peso
    // ═══════════════════════════════════════════════════════════════════════════

    const char* MixPriorityEngine::getRoleWeightLabel(uint8_t weight) noexcept
    {
        if (weight >= 9) return "CRITICAL";
        if (weight >= 7) return "HIGH";
        if (weight >= 5) return "MEDIUM";
        if (weight >= 3) return "LOW";
        return "MINIMAL";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getGenreModifier — Ajuste por género para un dominio específico
    // ═══════════════════════════════════════════════════════════════════════════

    float MixPriorityEngine::getGenreModifier(const juce::String& genre,
                                                const juce::String& domain) noexcept
    {
        // Normalizar género a lowercase
        juce::String g = genre.trim().toLowerCase();

        // ─── Reggaeton / Latin: bass y dynamics pesan más ────────────────
        if (g == "reggaeton" || g == "latin" || g == "dembow" || g == "reggaeton/latin") {
            if (domain == "gain")     return 1.10f; // El bombo 808 necesita nivel exacto
            if (domain == "dynamics") return 1.15f; // La dinámica del 808 es crítica
            if (domain == "tonal")    return 1.05f; // Balance sub-bass importa
            return 1.00f;
        }

        // ─── Trap / Hip-Hop: 808 y sub-graves son críticos ──────────────
        if (g == "trap" || g == "hip hop" || g == "hiphop" || g == "rap") {
            if (domain == "gain")     return 1.15f; // 808 clipping = desastre
            if (domain == "dynamics") return 1.20f; // Compresión del 808 es arte
            if (domain == "tonal")    return 1.10f; // Sub-bass balance
            return 1.00f;
        }

        // ─── Rock / Metal: guitarras y batería pesan más ─────────────────
        if (g == "rock" || g == "metal" || g == "punk" || g == "alternative") {
            if (domain == "gain")     return 1.05f;
            if (domain == "dynamics") return 1.10f; // Compresión de batería crítica
            if (domain == "tonal")    return 1.15f; // Tono de guitarra es clave
            return 1.00f;
        }

        // ─── Pop: todo importa igual, la voz un poco más ──────────────────
        if (g == "pop" || g == "pop/rock") {
            if (domain == "gain")     return 1.05f;
            if (domain == "tonal")    return 1.10f; // Voz cristalina
            return 1.00f;
        }

        // ─── EDM / Electronic: mezcla precisa de sub y presencia ──────────
        if (g == "edm" || g == "electronic" || g == "house" || g == "techno") {
            if (domain == "gain")     return 1.10f; // Headroom es sagrado
            if (domain == "dynamics") return 1.10f; // Sidechain dynamics
            if (domain == "tonal")    return 1.05f;
            if (domain == "spatial")  return 1.10f; // Stereo field es clave
            return 1.00f;
        }

        // ─── Jazz / Classical: dinámica natural y balance tonal ──────────
        if (g == "jazz" || g == "classical" || g == "acoustic") {
            if (domain == "dynamics") return 1.15f; // Rango dinámico natural
            if (domain == "tonal")    return 1.20f; // Balance acústico
            if (domain == "spatial")  return 1.10f; // Imagen estéreo natural
            return 0.90f; // Gain menos crítico (menos compresión)
        }

        // ─── Afrobeat / World: percusión y bajo pesan más ────────────────
        if (g == "afrobeat" || g == "afrobeats" || g == "world") {
            if (domain == "gain")     return 1.10f;
            if (domain == "dynamics") return 1.15f; // Percusión dinámica
            return 1.00f;
        }

        // Default: sin modificación
        return 1.00f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeScore — Computa el PriorityScore completo para un issue
    // ═══════════════════════════════════════════════════════════════════════════

    PriorityScore MixPriorityEngine::computeScore(float rawSeverity,
                                                    TrackRole role,
                                                    const juce::String& domain,
                                                    const juce::String& issueType,
                                                    const juce::String& trackName,
                                                    const juce::String& genre) noexcept
    {
        PriorityScore score;
        score.slotIndex     = -1; // Quien llama debe setear esto
        score.rawSeverity   = juce::jlimit(0.0f, 1.0f, rawSeverity);
        score.roleWeight    = getRoleWeight(role);
        score.domain        = domain;
        score.issueType     = issueType;
        score.trackName     = trackName;
        score.roleName      = getRoleName(role);

        // Domain weight
        score.domainWeight = DomainWeight::forDomain(domain);

        // Genre modifier
        score.genreModifier = getGenreModifier(genre, domain);

        // Fórmula final
        float normalizedRoleWeight = static_cast<float>(score.roleWeight) / 10.0f; // 1-10 → 0.1-1.0
        score.finalScore = score.rawSeverity * normalizedRoleWeight * score.domainWeight * score.genreModifier;

        return score;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  sortByPriority — Sort por finalScore descendente
    // ═══════════════════════════════════════════════════════════════════════════

    void MixPriorityEngine::sortByPriority(std::vector<PriorityScore>& scores) noexcept
    {
        std::sort(scores.begin(), scores.end(), [](const PriorityScore& a, const PriorityScore& b) {
            return a.finalScore > b.finalScore;
        });
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getTopPriority — Retorna los top N scores
    // ═══════════════════════════════════════════════════════════════════════════

    std::vector<PriorityScore> MixPriorityEngine::getTopPriority(
        const std::vector<PriorityScore>& scores,
        int maxCount) noexcept
    {
        if (scores.empty() || maxCount <= 0)
            return {};

        std::vector<PriorityScore> sorted = scores;
        sortByPriority(sorted);

        int count = juce::jmin(maxCount, static_cast<int>(sorted.size()));
        std::vector<PriorityScore> result;
        result.reserve(count);
        for (int i = 0; i < count; ++i)
            result.push_back(sorted[i]);

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  scoresToLLMContext — Convierte scores a texto para el LLM
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String MixPriorityEngine::scoresToLLMContext(
        const std::vector<PriorityScore>& scores) noexcept
    {
        if (scores.empty())
            return "No priority issues detected.";

        juce::String result;
        result += "[PRIORITY ISSUES — ordenados por score multidimensional]\n";
        result += "Formato: Track [Rol] Dominio/Tipo → Score (severity × role × domain × genre)\n";
        result += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

        for (size_t i = 0; i < scores.size(); ++i) {
            const auto& s = scores[i];
            juce::String severityLabel;
            if (s.finalScore >= 0.7f)      severityLabel = "🔴";
            else if (s.finalScore >= 0.4f) severityLabel = "🟡";
            else if (s.finalScore >= 0.2f) severityLabel = "🟢";
            else                           severityLabel = "⚪";

            result += severityLabel + " #" + juce::String(static_cast<int>(i + 1)) + " "
                      + s.trackName + " [" + s.roleName + "] "
                      + s.domain + "/" + s.issueType + "\n";
            result += "       Score: " + juce::String(s.finalScore, 3)
                      + " (severity=" + juce::String(s.rawSeverity, 2)
                      + " × role=" + juce::String(s.roleWeight)
                      + "/10=" + juce::String(static_cast<float>(s.roleWeight) / 10.0f, 2)
                      + " × domain=" + juce::String(s.domainWeight, 2)
                      + " × genre=" + juce::String(s.genreModifier, 2) + ")\n";
        }

        result += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        return result;
    }

} // namespace mixcoach

#include "CorrectionLearner.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SpectralCorrection::similarityTo — Similaridad con un perfil actual
// ═══════════════════════════════════════════════════════════════════════════
float SpectralCorrection::similarityTo(const TrackSpectralProfile& p) const noexcept
{
    return CorrectionLearner::spectralSimilarity(
        p, bandLevelDb, crestDb, correlation, transientRatio, avgStereoWidth);
}

// ═══════════════════════════════════════════════════════════════════════════
//  recordCorrection — Registra una corrección del usuario
// ═══════════════════════════════════════════════════════════════════════════
void CorrectionLearner::recordCorrection(TrackRole oldRole,
                                          TrackRole newRole,
                                          bool inferredFromName,
                                          const std::vector<juce::String>& keywords,
                                          const TrackSpectralProfile& spectral)
{
    // ─── Si no hay cambio real, ignorar ──────────────────────────────────
    if (oldRole == newRole)
        return;

    // ─── 1. Aprendizaje por keywords ─────────────────────────────────────
    if (inferredFromName && !keywords.empty()) {
        for (const auto& kw : keywords) {
            // Incrementar contador: keyword → newRole
            auto& roleMap = keywordCorrections_[kw];
            roleMap[newRole]++;

            // Opcional: decrementar count para oldRole (penalizar el error)
            // Solo si oldRole != Unknown (no penalizamos inferir Unknown)
            if (oldRole != TrackRole::Unknown) {
                auto it = roleMap.find(oldRole);
                if (it != roleMap.end() && it->second > 0)
                    it->second--;
                // Si llegó a 0, lo dejamos (no eliminamos para no sesgar)
            }
        }

        LogHelper::writeToLog(
            "[CorrectionLearner] Keyword correction: "
            + juce::String(getRoleName(oldRole)) + " -> "
            + juce::String(getRoleName(newRole))
            + " (keywords: " + juce::String((int)keywords.size()) + ")");
    }

    // ─── 2. Aprendizaje espectral ────────────────────────────────────────
    if (!inferredFromName && spectral.hasData()) {
        // Buscar si ya existe una entrada similar
        bool found = false;
        for (auto& sc : spectralCorrections_) {
            float sim = spectralSimilarity(spectral, sc.bandLevelDb,
                                           sc.crestDb, sc.correlation,
                                           sc.transientRatio, sc.avgStereoWidth);
            if (sim > 0.85f) {
                // Ya existe: incrementar contador y actualizar rol si cambió
                sc.correctionCount++;
                if (sc.correctedRole == oldRole)
                    sc.correctedRole = newRole;
                found = true;
                break;
            }
        }

        if (!found && (int)spectralCorrections_.size() < kMaxSpectralCorrections) {
            // Crear nueva entrada
            SpectralCorrection sc;
            for (int b = 0; b < 6; ++b)
                sc.bandLevelDb[b] = spectral.bandLevelDb[b];
            sc.crestDb         = spectral.crestDb;
            sc.correlation     = spectral.correlation;
            sc.transientRatio  = spectral.transientRatio;
            sc.avgStereoWidth  = spectral.avgStereoWidth;
            sc.correctedRole   = newRole;
            sc.correctionCount = 1;
            spectralCorrections_.push_back(sc);
        }

        LogHelper::writeToLog(
            "[CorrectionLearner] Spectral correction: "
            + juce::String(getRoleName(oldRole)) + " -> "
            + juce::String(getRoleName(newRole)));
    }

    // ─── 3. También registrar si no tenemos keywords ni espectro (rol manual directo) ──
    // En este caso no podemos aprender patrones, pero podemos registrar la corrección
    if (keywords.empty() && !spectral.hasData()) {
        LogHelper::writeToLog(
            "[CorrectionLearner] Manual correction (no data to learn from): "
            + juce::String(getRoleName(oldRole)) + " -> "
            + juce::String(getRoleName(newRole)));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  getKeywordBoost — Retorna boost de confianza para keyword+rol
// ═══════════════════════════════════════════════════════════════════════════
float CorrectionLearner::getKeywordBoost(const juce::String& keyword,
                                          TrackRole candidateRole) const noexcept
{
    auto kwIt = keywordCorrections_.find(keyword);
    if (kwIt == keywordCorrections_.end())
        return 0.0f;

    const auto& roleMap = kwIt->second;
    auto roleIt = roleMap.find(candidateRole);
    if (roleIt == roleMap.end() || roleIt->second < kMinCorrectionsForBoost)
        return 0.0f;

    // Boost proporcional al número de correcciones, con límite
    float boost = (float)roleIt->second * kBoostPerCorrection;
    return std::min(boost, kMaxBoost);
}

// ═══════════════════════════════════════════════════════════════════════════
//  getSpectralCorrection — Busca si hay una corrección espectral similar
// ═══════════════════════════════════════════════════════════════════════════
TrackRole CorrectionLearner::getSpectralCorrection(
    const TrackSpectralProfile& profile, float& confidence) const noexcept
{
    confidence = 0.0f;

    if (!profile.hasData() || spectralCorrections_.empty())
        return TrackRole::Unknown;

    TrackRole bestRole = TrackRole::Unknown;
    float bestSim = 0.0f;

    for (const auto& sc : spectralCorrections_) {
        float sim = spectralSimilarity(profile, sc.bandLevelDb,
                                       sc.crestDb, sc.correlation,
                                       sc.transientRatio, sc.avgStereoWidth);

        // Ponderar por número de correcciones (más correcciones = más peso)
        float weight = 1.0f + (float)(sc.correctionCount - 1) * 0.15f;
        float weightedSim = sim * std::min(weight, 1.5f);

        if (weightedSim > bestSim) {
            bestSim = weightedSim;
            bestRole = sc.correctedRole;
        }
    }

    // Solo retornar si la similaridad es significativa
    if (bestSim > 0.75f) {
        confidence = std::min(bestSim, 0.95f);
        return bestRole;
    }

    return TrackRole::Unknown;
}

// ═══════════════════════════════════════════════════════════════════════════
//  getTotalCorrections
// ═══════════════════════════════════════════════════════════════════════════
int CorrectionLearner::getTotalCorrections() const noexcept
{
    int total = 0;
    for (const auto& [kw, roleMap] : keywordCorrections_) {
        for (const auto& [role, count] : roleMap)
            total += count;
    }
    total += (int)spectralCorrections_.size();
    return total;
}

// ═══════════════════════════════════════════════════════════════════════════
//  dumpState — Debug
// ═══════════════════════════════════════════════════════════════════════════
juce::String CorrectionLearner::dumpState() const
{
    juce::String msg;
    msg += "=== CorrectionLearner State ===\n";

    int totalKwCorrections = 0;
    for (const auto& [kw, roleMap] : keywordCorrections_) {
        msg += "  Keyword '" + kw + "':\n";
        for (const auto& [role, count] : roleMap) {
            msg += "    -> " + juce::String(getRoleName(role))
                   + " x" + juce::String(count) + "\n";
            totalKwCorrections += count;
        }
    }
    msg += "  Total keyword corrections: " + juce::String(totalKwCorrections) + "\n";

    msg += "  Spectral corrections: " + juce::String((int)spectralCorrections_.size()) + "\n";
    for (size_t i = 0; i < spectralCorrections_.size() && i < 5; ++i) {
        auto& sc = spectralCorrections_[i];
        msg += "    [" + juce::String((int)i) + "] -> "
               + juce::String(getRoleName(sc.correctedRole))
               + " x" + juce::String(sc.correctionCount) + "\n";
    }

    return msg;
}

// ═══════════════════════════════════════════════════════════════════════════
//  persistence: toJson / fromJson
// ═══════════════════════════════════════════════════════════════════════════
void CorrectionLearner::toJson(juce::DynamicObject& obj) const
{
    // ─── Keyword corrections ─────────────────────────────────────────────
    juce::Array<juce::var> kwArray;
    for (const auto& [kw, roleMap] : keywordCorrections_) {
        for (const auto& [role, count] : roleMap) {
            auto* entry = new juce::DynamicObject();
            entry->setProperty("keyword", kw);
            entry->setProperty("role", (int)role);
            entry->setProperty("count", count);
            kwArray.add(juce::var(entry));
        }
    }
    obj.setProperty("keywordCorrections", kwArray);

    // ─── Spectral corrections ────────────────────────────────────────────
    juce::Array<juce::var> spArray;
    for (const auto& sc : spectralCorrections_) {
        if (sc.correctionCount <= 0) continue;
        auto* entry = new juce::DynamicObject();
        auto* bands = new juce::DynamicObject();
        for (int b = 0; b < 6; ++b)
            bands->setProperty("b" + juce::String(b), sc.bandLevelDb[b]);
        entry->setProperty("bands", juce::var(bands));
        entry->setProperty("crestDb", sc.crestDb);
        entry->setProperty("correlation", sc.correlation);
        entry->setProperty("transientRatio", sc.transientRatio);
        entry->setProperty("avgStereoWidth", sc.avgStereoWidth);
        entry->setProperty("role", (int)sc.correctedRole);
        entry->setProperty("count", sc.correctionCount);
        spArray.add(juce::var(entry));
    }
    obj.setProperty("spectralCorrections", spArray);
}

void CorrectionLearner::fromJson(const juce::DynamicObject& obj)
{
    keywordCorrections_.clear();
    spectralCorrections_.clear();

    // ─── Keyword corrections ─────────────────────────────────────────────
    auto kwVar = obj.getProperty("keywordCorrections");
    if (auto* kwArray = kwVar.getArray()) {
        for (const auto& item : *kwArray) {
            auto* entry = item.getDynamicObject();
            if (!entry) continue;
            juce::String kw = entry->getProperty("keyword").toString();
            TrackRole role = (TrackRole)(int)entry->getProperty("role");
            int count = juce::jmax(0, (int)entry->getProperty("count"));
            if (kw.isNotEmpty() && count > 0)
                keywordCorrections_[kw][role] = count;
        }
    }

    // ─── Spectral corrections ────────────────────────────────────────────
    auto spVar = obj.getProperty("spectralCorrections");
    if (auto* spArray = spVar.getArray()) {
        for (const auto& item : *spArray) {
            auto* entry = item.getDynamicObject();
            if (!entry) continue;

            SpectralCorrection sc;
            auto* bandsObj = entry->getProperty("bands").getDynamicObject();
            if (bandsObj) {
                for (int b = 0; b < 6; ++b) {
                    auto prop = bandsObj->getProperty("b" + juce::String(b));
                    sc.bandLevelDb[b] = (float)(double)prop;
                }
            }
            sc.crestDb         = (float)(double)entry->getProperty("crestDb");
            sc.correlation     = (float)(double)entry->getProperty("correlation");
            sc.transientRatio  = (float)(double)entry->getProperty("transientRatio");
            sc.avgStereoWidth  = (float)(double)entry->getProperty("avgStereoWidth");
            sc.correctedRole   = (TrackRole)(int)entry->getProperty("role");
            sc.correctionCount = juce::jmax(1, (int)entry->getProperty("count"));

            if (sc.correctedRole != TrackRole::Unknown && sc.correctionCount > 0)
                spectralCorrections_.push_back(sc);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  spectralSimilarity — Compara dos perfiles espectrales
//  Retorna 0.0-1.0 donde 1.0 = idénticos
// ═══════════════════════════════════════════════════════════════════════════
float CorrectionLearner::spectralSimilarity(
    const TrackSpectralProfile& a,
    const float bBandLevel[6],
    float bCrestDb,
    float bCorrelation,
    float bTransientRatio,
    float bAvgStereoWidth) noexcept
{
    // ─── Similaridad de bandas espectrales (6 bandas dBFS) ───────────────
    float bandSim = 0.0f;
    int validBands = 0;
    for (int b = 0; b < 6; ++b) {
        float aVal = a.bandLevelDb[b];
        float bVal = bBandLevel[b];
        if (aVal > -90.0f && bVal > -90.0f) {
            float diff = std::abs(aVal - bVal);
            // Si diff < 3dB → alta similaridad, si diff > 15dB → baja
            float bandScore = 1.0f - std::min(diff / 15.0f, 1.0f);
            bandSim += bandScore * bandScore; // Quadratic to penalize big differences
            validBands++;
        }
    }
    float bandScore = (validBands > 0) ? std::sqrt(bandSim / (float)validBands) : 0.0f;

    // ─── Crest similarity ────────────────────────────────────────────────
    float crestDiff = std::abs(a.crestDb - bCrestDb);
    float crestScore = 1.0f - std::min(crestDiff / 12.0f, 1.0f);

    // ─── Correlation similarity ──────────────────────────────────────────
    float corrDiff = std::abs(a.correlation - bCorrelation);
    float corrScore = 1.0f - std::min(corrDiff / 0.5f, 1.0f);

    // ─── Transient ratio similarity ──────────────────────────────────────
    float transDiff = std::abs(a.transientRatio - bTransientRatio);
    float transScore = 1.0f - std::min(transDiff / 3.0f, 1.0f);

    // ─── Stereo width similarity ─────────────────────────────────────────
    float widthDiff = std::abs(a.avgStereoWidth - bAvgStereoWidth);
    float widthScore = 1.0f - std::min(widthDiff / 0.5f, 1.0f);

    // ─── Ponderación: bandas tienen más peso ─────────────────────────────
    constexpr float kBandWeight   = 0.50f;
    constexpr float kCrestWeight  = 0.15f;
    constexpr float kCorrWeight   = 0.15f;
    constexpr float kTransWeight  = 0.10f;
    constexpr float kWidthWeight  = 0.10f;

    return bandScore * kBandWeight
         + crestScore * kCrestWeight
         + corrScore  * kCorrWeight
         + transScore * kTransWeight
         + widthScore * kWidthWeight;
}

} // namespace mixcoach

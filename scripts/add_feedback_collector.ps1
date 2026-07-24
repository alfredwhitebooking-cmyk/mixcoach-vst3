param(
    [string]$FilePath = "Source/MixCoach/engine/CoachEngine.h"
)

$content = Get-Content $FilePath -Raw

# 1. Add #include "FeedbackCollector.h" after #include "CorrectionLearner.h"
$content = $content.Replace(
    '#include "CorrectionLearner.h"' + [char]13 + [char]10 + '#include "SceneManager.h"',
    '#include "CorrectionLearner.h"' + [char]13 + [char]10 + '#include "FeedbackCollector.h"' + [char]13 + [char]10 + '#include "SceneManager.h"'
)

# 2. Add FeedbackCollector member + getters after getCorrectionLearner() const
$oldSection = '        [[nodiscard]] const CorrectionLearner& getCorrectionLearner() const noexcept { return correctionLearner_; }' + [char]13 + [char]10 + [char]13 + [char]10 + '        // ═══ Tracking de inferencia'
$newSection = '        [[nodiscard]] const CorrectionLearner& getCorrectionLearner() const noexcept { return correctionLearner_; }' + [char]13 + [char]10 + [char]13 + [char]10 + '        // ═══ FeedbackCollector — Recolecta y analiza feedback de correcciones ═══' + [char]13 + [char]10 + '        FeedbackCollector feedbackCollector_;' + [char]13 + [char]10 + [char]13 + [char]10 + '        /** Retorna el FeedbackCollector para análisis y reportes. */' + [char]13 + [char]10 + '        [[nodiscard]] FeedbackCollector& getFeedbackCollector() noexcept { return feedbackCollector_; }' + [char]13 + [char]10 + [char]13 + [char]10 + '        [[nodiscard]] const FeedbackCollector& getFeedbackCollector() const noexcept { return feedbackCollector_; }' + [char]13 + [char]10 + [char]13 + [char]10 + '        // ═══ Tracking de inferencia'

$content = $content.Replace($oldSection, $newSection)

Set-Content $FilePath -Value $content -NoNewline -Encoding UTF8

Write-Host "Done. Checking for FeedbackCollector references..."
Select-String -Path $FilePath -Pattern "FeedbackCollector" -SimpleMatch | ForEach-Object { Write-Host $_.LineNumber ": " $_.Line }

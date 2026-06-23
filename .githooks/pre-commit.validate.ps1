# ═══════════════════════════════════════════════════════════════════════════
#  pre-commit.validate.ps1 — MixCoach AI Rules Validation
#
#  Lógica de validación (PowerShell). Invocada por el wrapper .githooks/pre-commit
#  (POSIX sh), que detecta pwsh o powershell.exe para ejecutar este script.
#
#  Compatible con Windows PowerShell 5.1 (powershell.exe) y PowerShell 7+ (pwsh).
#
#  Validates BEFORE each commit:
#    1. No hardcoded bus colours (must use MixCoachTheme)
#    2. If SharedSlotEntry modified, kCurrentStructVersion must increment
#    3. New .cpp/.h files must be registered in CMakeLists.txt
#    4. AI documentation is synchronized
#    5. No V2 legacy modules reintroduced
#
#  Skip validation:
#    git commit --no-verify
#    or set env SKIP_MIXCOACH_AI_VALIDATION=1
# ═══════════════════════════════════════════════════════════════════════════

$ErrorActionPreference = "Continue"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ExitCode = 0

# ─── Check if validation should be skipped ───────────────────────────────
if ($env:SKIP_MIXCOACH_AI_VALIDATION -eq "1") {
    Write-Host "[pre-commit] SKIP_MIXCOACH_AI_VALIDATION=1, skipping AI rules validation" -ForegroundColor Yellow
    exit 0
}

# ─── Get staged files ────────────────────────────────────────────────────
$StagedFiles = @()
try {
    $StagedFiles = git diff --cached --name-only --diff-filter=ACMR | Where-Object { $_ -match '\.(cpp|h|yaml|md|ps1|py)$' }
} catch {
    Write-Host "[pre-commit] Warning: Could not get staged files" -ForegroundColor Yellow
    exit 0
}

if ($StagedFiles.Count -eq 0) {
    exit 0
}

Write-Host "`n━━━ MixCoach AI Rules — Pre-commit Validation ━━━" -ForegroundColor Cyan
Write-Host "  Files to validate: $($StagedFiles.Count)" -ForegroundColor Gray
Write-Host ""

# ─── RULE 1: No hardcoded bus colours ────────────────────────────────────
Write-Host "▸ Rule 1: No hardcoded bus colours" -ForegroundColor Yellow

$colourPattern = 'Colour\(0xFF[0-9A-Fa-f]{6}\)'
$mixCoachThemeFiles = @()

foreach ($file in $StagedFiles) {
    if ($file -notmatch '\.(cpp|h)$') { continue }
    if ($file -match 'MixCoachTheme\.h') { continue }  # Excepción: propia definición
    if ($file -match 'Constants\.h') { continue }       # Excepción: kBusColourARGB

    $fullPath = Join-Path $ProjectRoot $file
    if (-not (Test-Path $fullPath)) { continue }

    $content = Get-Content $fullPath -Raw

    # Buscar patterns sospechosos: Colour(0xFF...) que NO estén comentados
    $matches = [regex]::Matches($content, $colourPattern)
    $foundInvalid = @()
    foreach ($m in $matches) {
        $lineStart = $content.LastIndexOf("`n", $m.Index) + 1
        if ($lineStart -lt 0) { $lineStart = 0 }
        $lineContent = $content.Substring($lineStart, ($content.IndexOf("`n", $m.Index) - $lineStart)).Trim()

        # Saltar si está comentado o es parte de getBusColour/kBusColourARGB/MixCoachTheme
        if ($lineContent -match '^\s*//' -or
            $lineContent -match 'getBusColour' -or
            $lineContent -match 'kBusColourARGB' -or
            $lineContent -match 'MixCoachTheme::') {
            continue
        }
        $foundInvalid += @{ File = $file; Line = $lineContent }
    }

    if ($foundInvalid.Count -gt 0) {
        Write-Host "  [FAIL] Hardcoded colour found in $file" -ForegroundColor Red
        foreach ($f in $foundInvalid) {
            Write-Host "    $($f.Line)" -ForegroundColor Gray
        }
        $ExitCode = 1
    }
}

if ($ExitCode -eq 0) {
    Write-Host "  [OK] No hardcoded colours found" -ForegroundColor Green
}

# ─── RULE 2: IPC versioning ──────────────────────────────────────────────
Write-Host "`n▸ Rule 2: IPC versioning (SharedSlotEntry → kCurrentStructVersion)" -ForegroundColor Yellow

$touchedSharedSlotEntry = $StagedFiles | Where-Object { $_ -match 'SharedMemory\.(h|cpp)$' }
$touchedVersion = $StagedFiles | Where-Object { $_ -match 'SharedMemory\.h$' }

if ($touchedSharedSlotEntry.Count -gt 0) {
    if ($touchedVersion.Count -eq 0) {
        Write-Host "  [FAIL] SharedMemory touched but SharedMemory.h (version) not staged!" -ForegroundColor Red
        Write-Host "    Files: $($touchedSharedSlotEntry -join ', ')" -ForegroundColor Gray
        Write-Host "    → You MUST stage SharedMemory.h as well (it contains kCurrentStructVersion)" -ForegroundColor Yellow
        $ExitCode = 1
    } else {
        # Check if version was actually incremented
        $shmPath = Join-Path $ProjectRoot "Source/Common/memory/SharedMemory.h"
        if (Test-Path $shmPath) {
            $shmContent = Get-Content $shmPath -Raw
            if ($shmContent -match 'kCurrentStructVersion\s*=\s*(\d+)') {
                $version = [int]$Matches[1]
                Write-Host "  [OK] kCurrentStructVersion = $version" -ForegroundColor Green
            }
        }
    }
} else {
    Write-Host "  [OK] No IPC files modified" -ForegroundColor Green
}

# ─── RULE 3: New files must be in CMakeLists.txt ─────────────────────────
Write-Host "`n▸ Rule 3: New .cpp/.h files registered in CMakeLists.txt" -ForegroundColor Yellow

# Solo .cpp necesitan estar en CMakeLists.txt; los .h se incluyen via #include.
$newSourceFiles = $StagedFiles | Where-Object {
    $_ -match '^Source/.*\.cpp$'
}

$cmakePath = Join-Path $ProjectRoot "CMakeLists.txt"
$cmakeContent = Get-Content $cmakePath -Raw

$unregisteredFiles = @()
foreach ($file in $newSourceFiles) {
    # Normalizar: CMake usa /, git usa \ en Windows
    $normalized = $file.Replace('\', '/')
    if ($cmakeContent -notmatch [regex]::Escape($normalized)) {
        $unregisteredFiles += $file
    }
}

if ($unregisteredFiles.Count -gt 0) {
    Write-Host "  [FAIL] Files not registered in CMakeLists.txt:" -ForegroundColor Red
    foreach ($f in $unregisteredFiles) {
        Write-Host "    ✗ $f" -ForegroundColor Red
    }
    Write-Host "  → Add to target_sources() in CMakeLists.txt" -ForegroundColor Yellow
    $ExitCode = 1
} else {
    Write-Host "  [OK] All new source files registered in CMakeLists.txt" -ForegroundColor Green
}

# ─── RULE 4: AI doc synchronization ──────────────────────────────────────
Write-Host "`n▸ Rule 4: AI doc synchronization (quick check)" -ForegroundColor Yellow

$touchedAIYaml = $StagedFiles | Where-Object { $_ -match 'AI_COMPONENT_INDEX\.yaml$' }
$touchedSource = $StagedFiles | Where-Object { $_ -match '^Source/' }

if ($touchedSource.Count -gt 0 -and $touchedAIYaml.Count -eq 0) {
    Write-Host "  [WARN] Source files modified but AI_COMPONENT_INDEX.yaml not updated" -ForegroundColor Yellow
    Write-Host "    → Consider updating the YAML if components changed" -ForegroundColor Gray
}

# ─── RULE 5: No V2 legacy modules reintroduced ──────────────────────────
Write-Host "`n▸ Rule 5: No V2 legacy modules reintroduced" -ForegroundColor Yellow

$legacyPatterns = @(
    'TelemetryProvider',
    'TelemetryBuffer',
    'TelemetryCollector',
    'MeterComponent\.(h|cpp)',
    'TelemetryData\.h',
    'AudioRingBuffer'
)

foreach ($file in $StagedFiles) {
    foreach ($pattern in $legacyPatterns) {
        if ($file -match $pattern) {
            Write-Host "  [FAIL] V2 legacy module reintroduced: $file" -ForegroundColor Red
            Write-Host "    → These modules were intentionally removed in V3. Do not reintroduce." -ForegroundColor Yellow
            $ExitCode = 1
        }
    }
}

if ($ExitCode -eq 0) {
    Write-Host "  [OK] No V2 legacy modules detected" -ForegroundColor Green
}

# ─── Summary ─────────────────────────────────────────────────────────────
Write-Host ""
if ($ExitCode -eq 0) {
    Write-Host "━━━ All AI rules pass ✅ ━━━" -ForegroundColor Green
} else {
    Write-Host "━━━ AI rules FAILED ❌ ━━━" -ForegroundColor Red
    Write-Host "  To bypass: git commit --no-verify" -ForegroundColor Yellow
    Write-Host "  Or set: `$env:SKIP_MIXCOACH_AI_VALIDATION=1" -ForegroundColor Yellow
}
Write-Host ""

exit $ExitCode

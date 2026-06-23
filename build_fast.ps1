param(
    [switch]$All,
    [switch]$NoDeploy,
    [switch]$SkipPatternValidator,
    [switch]$Help
)

$Cyan   = "Cyan"
$Green  = "Green"
$Yellow = "Yellow"
$Red    = "Red"
$Gray   = "Gray"

if ($Help) {
    Write-Host "MixCoach - Build Express v1.0" -ForegroundColor $Cyan
    Write-Host "Usage:"
    Write-Host "  .\build_fast.ps1              Build Release MixCoach + deploy"
    Write-Host "  .\build_fast.ps1 -All          Build BOTH MixCoach + Messenger + deploy"
    Write-Host "  .\build_fast.ps1 -NoDeploy     Build only, no deploy"
    Write-Host "  .\build_fast.ps1 -Help         Show this help"
    exit 0
}

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$StartTime = Get-Date

Write-Host "`n=== MixCoach - Build Express ===" -ForegroundColor $Cyan

# Step 0: Pre-check CMakeLists.txt files
$checkScript = Join-Path $ProjectRoot "scripts/check_cmakelists_files.ps1"
if (Test-Path $checkScript) {
    & $checkScript
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Missing files in CMakeLists.txt - fix before building" -ForegroundColor $Red
        exit 1
    }
    Write-Host "[OK] CMakeLists.txt: all files exist" -ForegroundColor $Green
}

# Step 0.5: Pattern Validator (pre-build lint)
if (-not $SkipPatternValidator) {
    $pvScript = Join-Path $ProjectRoot "scripts/pattern_validator.py"
    if (Test-Path $pvScript) {
        Write-Host "[..] Running pattern validator..." -ForegroundColor $Yellow
        $pvOutput = python $pvScript 2>&1
        $pvExit = $LASTEXITCODE
        if ($pvExit -ne 0) {
            Write-Host "[!!] Pattern validator found issues (continuing with build)" -ForegroundColor $Yellow
        } else {
            Write-Host "[OK] Pattern validator: all checks passed" -ForegroundColor $Green
        }
    }
} else {
    Write-Host "[..] Pattern validator: skipped (-SkipPatternValidator)" -ForegroundColor $Gray
}

# Step 1: Build MixCoach_VST3
$buildLog = Join-Path $ProjectRoot "build_fast_output.txt"
Write-Host "[..] Building MixCoach_VST3 (Release)..." -ForegroundColor $Yellow

$buildResult = cmake --build $BuildDir --config Release --target MixCoach_VST3 2>&1
$buildExitCode = $LASTEXITCODE
$buildResult | Out-File -FilePath $buildLog -Encoding utf8

if ($buildExitCode -ne 0) {
    Write-Host "[FAIL] Build error (exit code: $buildExitCode)" -ForegroundColor $Red
    Write-Host "Log: $buildLog" -ForegroundColor $Yellow
    Write-Host "Last lines:" -ForegroundColor $Yellow
    $buildResult | Select-Object -Last 10
    
    # AI Build Loop: analyze errors with ERROR_PATTERNS.json
    $blScript = Join-Path $ProjectRoot "ai_build_loop.py"
    if (Test-Path $blScript) {
        Write-Host "`n--- AI Build Loop: Analyzing errors ---" -ForegroundColor $Cyan
        $blOutput = python $blScript --analyze-only $buildLog 2>&1
        $blOutput | ForEach-Object { if ($_.Trim() -ne "") { Write-Host "  $_" } }
    }
    
    exit 1
}

Write-Host "[OK] Build successful" -ForegroundColor $Green

# Step 2: Verify MixCoach VST3 exists
$vst3Path = Join-Path $BuildDir "MixCoach_artefacts/Release/VST3/MixCoach.vst3"
if (-not (Test-Path $vst3Path)) {
    Write-Host "[FAIL] VST3 not found: $vst3Path" -ForegroundColor $Red
    exit 1
}
Write-Host "[OK] MixCoach.vst3 generated" -ForegroundColor $Green

# ─── Step 2.5: Build Messenger_VST3 (solo con -All) ─────────────────────
if ($All) {
    Write-Host "[..] Building Messenger_VST3 (Release)..." -ForegroundColor $Yellow

    $msgBuildResult = cmake --build $BuildDir --config Release --target Messenger_VST3 2>&1
    $msgBuildExitCode = $LASTEXITCODE
    $msgBuildResult | Out-File -FilePath $buildLog -Encoding utf8 -Append

    if ($msgBuildExitCode -ne 0) {
        Write-Host "[FAIL] Messenger build error (exit code: $msgBuildExitCode)" -ForegroundColor $Red
        Write-Host "Log: $buildLog" -ForegroundColor $Yellow
        Write-Host "Last lines:" -ForegroundColor $Yellow
        $msgBuildResult | Select-Object -Last 10
        exit 1
    }
    Write-Host "[OK] Messenger build successful" -ForegroundColor $Green

    # Verify Messenger VST3
    $msgVst3Path = Join-Path $BuildDir "Messenger_artefacts/Release/VST3/Messenger.vst3"
    if (-not (Test-Path $msgVst3Path)) {
        Write-Host "[FAIL] Messenger VST3 not found: $msgVst3Path" -ForegroundColor $Red
        exit 1
    }
    Write-Host "[OK] Messenger.vst3 generated" -ForegroundColor $Green
}

# Step 3: Deploy
if (-not $NoDeploy) {
    $deployScript = Join-Path $ProjectRoot "DeployVST3.ps1"
    if (Test-Path $deployScript) {
        & $deployScript -Config Release
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[OK] Deploy complete" -ForegroundColor $Green
        } else {
            Write-Host "[!!] Deploy failed (FL Studio open? Run DeployVST3.ps1 manually)" -ForegroundColor $Yellow
        }
    }
}

$Duration = (Get-Date) - $StartTime
Write-Host ("`n=== BUILD EXPRESS in " + $Duration.TotalSeconds.ToString("F1") + "s ===") -ForegroundColor $Green

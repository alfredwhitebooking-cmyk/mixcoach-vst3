<#
.SYNOPSIS
    MixCoach Agent Mode v2.0 — Simplified pipeline: context → build → deploy.

.DESCRIPTION
    Lightweight pipeline with only the active scripts:
    Context Selection -> Context Optimization -> Build Loop -> Deploy

.PARAMETER Query
    Natural language task description.

.PARAMETER Config
    Build configuration: Release (default) or Debug.

.PARAMETER Analyze
    Run project intelligence analysis only.

.PARAMETER Build
    Run context selection + build.

.PARAMETER Full
    Shortcut for -Build (included for backwards compatibility).

.PARAMETER Deploy
    Deploy VST3 after successful build.

.PARAMETER Step
    Run a specific step: context, build, learn, code.

.PARAMETER UseSmart
    Use context_selector.py for intelligent file selection (passed to local_coder.py).

.PARAMETER Json
    Output results in JSON format.

.EXAMPLE
    .\agent_mode.ps1 "mejorar analizador de espectro" -Build
    .\agent_mode.ps1 -Analyze
    .\agent_mode.ps1 "buscar shared memory" -Step context
    .\agent_mode.ps1 "optimizar FFT" -Step code       # Run local_coder.py
    .\agent_mode.ps1 "arreglar UI" -Step code -UseSmart  # Use context_selector for file selection
#>

param(
    [Parameter(Position=0)]
    [string]$Query,

    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",

    [switch]$Analyze,
    [switch]$Build,
    [switch]$Full,
    [switch]$Deploy,
    [switch]$UseSmart,
    [switch]$Json,

    [ValidateSet("context", "build", "learn", "code")]
    [string]$Step = ""
)

$ErrorActionPreference = "Continue"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$PythonCmd = "python"

# --- Helper Functions ---------------------------------------------------

function Write-Step {
    param([string]$Title, [string]$Status, [string]$StepNum = "")
    $num = if ($StepNum) { " [$StepNum]" } else { "" }
    $icon = switch ($Status) {
        "running" { "[RUN]" }
        "done"    { "[OK]" }
        "skip"    { "[--]" }
        "error"   { "[!!]" }
        default   { "[->]" }
    }
    Write-Host ""
    Write-Host "$icon$num $Title" -ForegroundColor Cyan
}

function Get-Timestamp {
    return Get-Date -Format "HH:mm:ss"
}

function Write-JsonOrText {
    param($Data, [string]$Title = "")
    if ($Json) {
        $jsonStr = $Data | ConvertTo-Json -Depth 3
        Write-Host $jsonStr
    } else {
        if ($Title) { Write-Host "`n$Title" -ForegroundColor White }
        Write-Host $Data
    }
}

# --- Banner -------------------------------------------------------------

Clear-Host
Write-Host "======================================================" -ForegroundColor Magenta
Write-Host "  MixCoach Agent Mode v2.0" -ForegroundColor Cyan
Write-Host "  Simplified pipeline: context -> build -> deploy" -ForegroundColor DarkGray
Write-Host "======================================================" -ForegroundColor Magenta
Write-Host ""

if ((-not $Query) -and (-not $Analyze) -and (-not $Step)) {
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\agent_mode.ps1 'query' [-Build] [-Deploy]" -ForegroundColor Gray
    Write-Host "  .\agent_mode.ps1 -Analyze" -ForegroundColor Gray
    Write-Host "  .\agent_mode.ps1 'query' -Step context|build|learn|code" -ForegroundColor Gray
    Write-Host "  .\agent_mode.ps1 'query' -Step code -UseSmart" -ForegroundColor Gray
    exit 0
}

# --- Mode: Analyze ------------------------------------------------------

if ($Analyze) {
    Write-Step "Running Project Intelligence" "running"
    Write-Host ("> [{0}] Analyzing..." -f (Get-Timestamp)) -ForegroundColor DarkGray
    $result = & $PythonCmd "$ProjectRoot\project_intelligence.py" --all 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host $result
    } else {
        Write-Host ("> [{0}] Analysis failed" -f (Get-Timestamp)) -ForegroundColor Red
        Write-Host $result
    }

    Write-Step "Refreshing session state" "running"
    & $PythonCmd "$ProjectRoot\project_intelligence.py" --refresh-session 2>&1

    Write-Host ""
    Write-Host "[OK] Analysis complete." -ForegroundColor Green
    exit 0
}

# --- Step Mode ----------------------------------------------------------

if ($Step) {
    switch ($Step) {
        "context" {
            Write-Step "Context Selection" "running"
            Write-Host ("> Query: $Query") -ForegroundColor White
            & $PythonCmd "$ProjectRoot\context_selector.py" $Query 2>&1
        }
        "build" {
            Write-Step "Build Loop" "running"
            $buildArgs = @(
                "$ProjectRoot\ai_build_loop.py",
                "--config", $Config
            )
            if ($Deploy) { $buildArgs += "--deploy" }
            Write-Host ("> Building ($Config)...") -ForegroundColor White
            & $PythonCmd $buildArgs 2>&1
        }
        "learn" {
            Write-Step "Error Pattern Learning" "running"
            & $PythonCmd "$ProjectRoot\ai_build_loop.py" --learn 2>&1
            & $PythonCmd "$ProjectRoot\ai_build_loop.py" --stats 2>&1
        }
        "code" {
            Write-Step "Local Coder (Qwen/DeepSeek)" "running"
            $smartFlag = if ($UseSmart) { "--smart" } else { "" }
            $modelName = & $PythonCmd -c "
import sys
sys.path.insert(0, '$ProjectRoot')
kw = ['fft','dsp','audio','processblock','thread','realtime','latency','performance','buffer','analyzer','phase','ipc','telemetry']
task = '$Query'.lower()
print('deepseek-coder-v2:latest' if any(w in task for w in kw) else 'qwen2.5-coder:7b')
" 2>&1 | Select-Object -Last 1
            Write-Host ("> Using model: $modelName") -ForegroundColor Yellow
            Write-Host ("> Query: $Query") -ForegroundColor White
            & $PythonCmd "$ProjectRoot\scripts\local_coder.py" $smartFlag $Query 2>&1
        }
    }
    exit 0
}

# --- Build mode (replaces old -Full and -Build) ---------------------------

if ($Build -or $Full) {
    $startTime = Get-Date
    $selectedFiles = @()

    # Step 1: Context Selection
    Write-Step "1/3: Context Selection" "running" "1"
    $contextOutput = & $PythonCmd "$ProjectRoot\context_selector.py" $Query 2>$null
    foreach ($line in $contextOutput) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^Source') {
            $selectedFiles += $trimmed
        }
    }
    Write-Host ("> [{0}] Selected {1} files" -f (Get-Timestamp), $selectedFiles.Count) -ForegroundColor Green
    foreach ($f in $selectedFiles) {
        Write-Host "    $f" -ForegroundColor Gray
    }

    # Step 2: Context Optimization
    Write-Step "2/3: Context Optimization" "running" "2"
    if ($selectedFiles.Count -gt 0) {
        & $PythonCmd "$ProjectRoot\token_optimizer.py" --summarize-batch $selectedFiles 2>&1
    }

    # Step 3: Build
    Write-Step "3/3: Build Loop" "running" "3"
    $buildArgs = @(
        "$ProjectRoot\ai_build_loop.py",
        "--config", $Config
    )
    if ($Deploy) { $buildArgs += "--deploy" }
    & $PythonCmd $buildArgs 2>&1

    $elapsed = (Get-Date) - $startTime
    $minutes = [math]::Round($elapsed.TotalMinutes, 1)
    Write-Host ""
    Write-Host "[OK] Pipeline complete in ${minutes}min" -ForegroundColor Green

    # Update session
    & $PythonCmd "$ProjectRoot\project_intelligence.py" --refresh-session 2>&1 | Out-Null

    exit 0
}

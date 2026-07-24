# MixCoach - Deploy rapido VST3
# Compila y copia el VST3 a Common Files o FL Studio.
# Uso:
#   .\deploy.ps1              Compila Release + Common Files
#   .\deploy.ps1 -Debug       Compila Debug
#   .\deploy.ps1 -FLStudio    Compila + FL Studio user folder
#   .\deploy.ps1 -Help        Muestra ayuda

param(
    [switch]$Debug,
    [switch]$FLStudio,
    [switch]$NoBuild,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$config = if ($Debug) { "Debug" } else { "Release" }

if ($Help) {
    Write-Host "MixCoach Deploy"
    Write-Host ""
    Write-Host "  .\deploy.ps1              Compila Release + Common Files"
    Write-Host "  .\deploy.ps1 -Debug       Compila Debug + Common Files"
    Write-Host "  .\deploy.ps1 -FLStudio    Compila + FL Studio user folder"
    Write-Host "  .\deploy.ps1 -NoBuild     Solo copia (sin compilar)"
    exit 0
}

# Check cmake availability
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: cmake not found in PATH" -ForegroundColor Red
    exit 1
}

$buildDir  = Join-Path $scriptRoot "build"
$vst3Src   = Join-Path $buildDir "MixCoach_artefacts\$config\VST3\MixCoach.vst3"

# Destination
if ($FLStudio) {
    $destBase = "$env:USERPROFILE\Documents\Image-Line\FL Studio\Presets\Plugin presets\Effects\VST3"
} else {
    $destBase = "$env:ProgramData\VST3"
}
$destPath = Join-Path $destBase "MixCoach.vst3"

# --- 1. Build ---
if (-not $NoBuild) {
    Write-Host "Building MixCoach_VST3 ($config)..."

    $result = cmake --build $buildDir --config $config --target MixCoach_VST3 2>&1
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        Write-Host "BUILD FAILED" -ForegroundColor Red
        $result | Select-Object -Last 10 | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        exit 1
    }
    Write-Host "Build OK" -ForegroundColor Green
} else {
    Write-Host "Skipping build (-NoBuild)"
}

# --- 2. Validate ---
Write-Host "Validating VST3..."

if (-not (Test-Path $vst3Src)) {
    Write-Host "ERROR: VST3 not found at $vst3Src" -ForegroundColor Red
    exit 1
}

$innerBin = Join-Path $vst3Src "Contents\x86_64-win\MixCoach.vst3"
if (-not (Test-Path $innerBin)) {
    Write-Host "ERROR: Inner binary missing - bundle incomplete" -ForegroundColor Red
    exit 1
}

$size = (Get-Item $innerBin).Length
$minSize = 512 * 1024
if ($size -lt $minSize) {
    Write-Host "ERROR: Binary too small ($size bytes, min $minSize)" -ForegroundColor Red
    exit 1
}

Write-Host "VST3 validated: $($size.ToString('N0')) bytes" -ForegroundColor Green

# --- 3. Deploy ---
$destDirParent = Split-Path $destPath -Parent
if (-not (Test-Path $destDirParent)) {
    New-Item -Path $destDirParent -ItemType Directory -Force | Out-Null
}

if (Test-Path $destPath) {
    Remove-Item -Recurse -Force $destPath
    Write-Host "Removed old deploy" -ForegroundColor Yellow
}

Write-Host "Copying to $destPath ..."
Copy-Item -LiteralPath $vst3Src -Destination $destPath -Recurse -Force

# --- 4. Verify ---
$deployedBin = Join-Path $destPath "Contents\x86_64-win\MixCoach.vst3"
if (Test-Path $deployedBin) {
    $deployedSize = (Get-Item $deployedBin).Length
    Write-Host "Deploy OK - $($deployedSize.ToString('N0')) bytes" -ForegroundColor Green
} else {
    Write-Host "DEPLOY FAILED" -ForegroundColor Red
    exit 1
}

$targetText = if ($FLStudio) { "FL Studio" } else { "Common Files" }
Write-Host "`nDone! MixCoach deployed to $targetText" -ForegroundColor Green
Write-Host "Open FL Studio and load MixCoach on the Master channel." -ForegroundColor Cyan

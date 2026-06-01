# Deploy VST3 Manual
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$VST3_System = "C:\Program Files\Common Files\VST3"

Write-Host "=== Deploying MixCoach & Messenger VST3 ===" -ForegroundColor Cyan

$MixCoachVST3 = Join-Path $BuildDir "MixCoach_artefacts\Release\VST3\MixCoach.vst3"
$MessengerVST3 = Join-Path $BuildDir "Messenger_artefacts\Release\VST3\Messenger.vst3"

if (-not (Test-Path $MixCoachVST3)) {
    Write-Error "MixCoach.vst3 not found at $MixCoachVST3"
    exit 1
}
if (-not (Test-Path $MessengerVST3)) {
    Write-Error "Messenger.vst3 not found at $MessengerVST3"
    exit 1
}

# ensure target dir exists
if (-not (Test-Path $VST3_System)) {
    New-Item -Path $VST3_System -ItemType Directory -Force | Out-Null
}

# copy MixCoach
$targetMix = Join-Path $VST3_System "MixCoach.vst3"
if (Test-Path $targetMix) { Remove-Item $targetMix -Recurse -Force }
Copy-Item $MixCoachVST3 $targetMix -Recurse -Force
Write-Host "MixCoach deployed to $targetMix" -ForegroundColor Green

# copy Messenger
$targetMsg = Join-Path $VST3_System "Messenger.vst3"
if (Test-Path $targetMsg) { Remove-Item $targetMsg -Recurse -Force }
Copy-Item $MessengerVST3 $targetMsg -Recurse -Force
Write-Host "Messenger deployed to $targetMsg" -ForegroundColor Green

Write-Host "Deployment completed successfully." -ForegroundColor Cyan

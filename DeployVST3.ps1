param(
    [ValidateSet("Deploy", "ListBackups", "Restore")]
    [string]$Action = "Deploy",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [string]$Backup = "",

    [switch]$NoBackup,

    [switch]$Force
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$VST3System = "C:\Program Files\Common Files\VST3"
$DeployBackupRoot = Join-Path $ProjectRoot "workspace_backups\vst3_deploy"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

function Write-Step($Message) { Write-Host "`n== $Message ==" -ForegroundColor Cyan }
function Write-OK($Message) { Write-Host "  [OK] $Message" -ForegroundColor Green }
function Write-Warn($Message) { Write-Host "  [WARN] $Message" -ForegroundColor Yellow }

function Assert-VST3System {
    if (-not (Test-Path $VST3System)) {
        New-Item -Path $VST3System -ItemType Directory -Force | Out-Null
    }
}

function Get-BackupDirectories {
    if (-not (Test-Path $DeployBackupRoot)) { return @() }
    return @(Get-ChildItem -LiteralPath $DeployBackupRoot -Directory | Sort-Object Name -Descending)
}

function Resolve-BackupDirectory {
    param([string]$BackupValue)

    if ([string]::IsNullOrWhiteSpace($BackupValue) -or $BackupValue -eq "latest") {
        $latest = Get-BackupDirectories | Select-Object -First 1
        if (-not $latest) { throw "No VST3 deploy backups found in $DeployBackupRoot" }
        return $latest.FullName
    }

    if (Test-Path -LiteralPath $BackupValue) {
        return (Resolve-Path -LiteralPath $BackupValue).Path
    }

    $candidate = Join-Path $DeployBackupRoot $BackupValue
    if (Test-Path -LiteralPath $candidate) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    throw "Backup not found: $BackupValue"
}

function Copy-VST3Bundle {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Source,
        [Parameter(Mandatory=$true)][string]$Destination,
        [switch]$SkipBackup
    )

    if (-not (Test-Path $Source)) {
        throw "$Name source not found: $Source"
    }

    Assert-VST3System

    if ((Test-Path $Destination) -and -not $SkipBackup) {
        $backupDir = Join-Path $DeployBackupRoot $Timestamp
        New-Item -Path $backupDir -ItemType Directory -Force | Out-Null
        $backupPath = Join-Path $backupDir "$Name.vst3"
        Copy-Item -LiteralPath $Destination -Destination $backupPath -Recurse -Force
        Write-OK "Backup previous $Name.vst3 -> $backupPath"
    }

    if (Test-Path $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }

    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    Write-OK "$Name deployed -> $Destination"
}

function Invoke-Deploy {
    Write-Step "Deploying MixCoach VST3s to Common Files"
    Write-Host "  Config: $Config" -ForegroundColor Gray
    Write-Host "  Target: $VST3System" -ForegroundColor Gray

    $mixCoachVST3 = Join-Path $BuildDir "MixCoach_artefacts\$Config\VST3\MixCoach.vst3"
    $messengerVST3 = Join-Path $BuildDir "Messenger_artefacts\$Config\VST3\Messenger.vst3"

    try {
        Copy-VST3Bundle -Name "MixCoach" -Source $mixCoachVST3 -Destination (Join-Path $VST3System "MixCoach.vst3") -SkipBackup:$NoBackup
        Copy-VST3Bundle -Name "Messenger" -Source $messengerVST3 -Destination (Join-Path $VST3System "Messenger.vst3") -SkipBackup:$NoBackup
    }
    catch {
        Write-Warn "Deploy failed: $($_.Exception.Message)"
        Write-Host "  Close FL Studio or any DAW that may be locking the VST3 files, then run:" -ForegroundColor Yellow
        Write-Host "  .\DeployVST3.ps1 -Config $Config" -ForegroundColor Yellow
        exit 1
    }

    Write-Step "Deployment completed"
    Write-OK "MixCoach and Messenger are in C:\Program Files\Common Files\VST3"
}

function Show-Backups {
    Write-Step "Available VST3 deploy backups"
    $dirs = Get-BackupDirectories
    if ($dirs.Count -eq 0) {
        Write-Warn "No VST3 deploy backups found."
        return
    }

    foreach ($dir in $dirs) {
        $mix = Join-Path $dir.FullName "MixCoach.vst3"
        $msg = Join-Path $dir.FullName "Messenger.vst3"
        $hasMix = Test-Path $mix
        $hasMsg = Test-Path $msg
        $status = "MixCoach=$hasMix Messenger=$hasMsg"
        Write-Host "$($dir.Name)  $status" -ForegroundColor Gray
        Write-Host "  $($dir.FullName)" -ForegroundColor DarkGray
    }
}

function Restore-Backup {
    if (-not $Force) {
        throw "Restore requires -Force to avoid accidental overwrite."
    }

    $backupDir = Resolve-BackupDirectory $Backup
    Write-Step "Restoring VST3 deploy backup"
    Write-Host "  Source: $backupDir" -ForegroundColor Gray
    Write-Host "  Target: $VST3System" -ForegroundColor Gray

    try {
        Copy-VST3Bundle -Name "MixCoach" -Source (Join-Path $backupDir "MixCoach.vst3") -Destination (Join-Path $VST3System "MixCoach.vst3") -SkipBackup:$NoBackup
        Copy-VST3Bundle -Name "Messenger" -Source (Join-Path $backupDir "Messenger.vst3") -Destination (Join-Path $VST3System "Messenger.vst3") -SkipBackup:$NoBackup
    }
    catch {
        Write-Warn "Restore failed: $($_.Exception.Message)"
        Write-Host "  Close FL Studio or any DAW that may be locking the VST3 files, then retry." -ForegroundColor Yellow
        exit 1
    }

    Write-OK "VST3 deploy restored from $backupDir"
}

switch ($Action) {
    "Deploy" { Invoke-Deploy }
    "ListBackups" { Show-Backups }
    "Restore" { Restore-Backup }
}

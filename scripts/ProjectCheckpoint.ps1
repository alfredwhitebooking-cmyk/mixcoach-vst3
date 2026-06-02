param(
    [ValidateSet("Save", "List", "Restore")]
    [string]$Action = "Save",

    [string]$Name = "",

    [string]$Checkpoint = "",

    [switch]$Force
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BackupRoot = Join-Path $ProjectRoot "workspace_backups\project_checkpoints"

function Write-Step($Message) { Write-Host "`n== $Message ==" -ForegroundColor Cyan }
function Write-OK($Message) { Write-Host "  [OK] $Message" -ForegroundColor Green }
function Write-Warn($Message) { Write-Host "  [WARN] $Message" -ForegroundColor Yellow }

function Get-SafeName {
    param([string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) { return "manual" }
    return ($Value -replace '[^a-zA-Z0-9._-]', '_').Trim('_')
}

function Get-GitValue {
    param([string[]]$Args)
    try {
        $value = & git -C $ProjectRoot @Args 2>$null
        if ($LASTEXITCODE -eq 0) { return ($value -join "`n") }
    }
    catch {}
    return ""
}

function New-ProjectCheckpoint {
    param([string]$CheckpointName)

    New-Item -Path $BackupRoot -ItemType Directory -Force | Out-Null

    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $safeName = Get-SafeName $CheckpointName
    $checkpointDir = Join-Path $BackupRoot "$timestamp`_$safeName"
    $stagingDir = Join-Path $checkpointDir "project"
    $zipPath = Join-Path $checkpointDir "MixCoach_$timestamp`_$safeName.zip"
    $manifestPath = Join-Path $checkpointDir "MANIFEST.txt"

    New-Item -Path $stagingDir -ItemType Directory -Force | Out-Null

    $excludedDirs = @(
        ".git",
        "build",
        ".vs",
        ".vscode",
        "workspace_backups"
    )

    $files = Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File -Force | Where-Object {
        $relative = $_.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/')
        $parts = $relative -split '[\\/]'
        (-not ($parts | Where-Object { $excludedDirs -contains $_ })) -and
        ($_.Name -ne "nul") -and
        (Test-Path -LiteralPath $_.FullName)
    }

    foreach ($file in $files) {
        $relative = $file.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/')
        $dest = Join-Path $stagingDir $relative
        $destDir = Split-Path -Parent $dest
        if (-not (Test-Path $destDir)) {
            New-Item -Path $destDir -ItemType Directory -Force | Out-Null
        }
        Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
    }

    $gitBranch = Get-GitValue @("branch", "--show-current")
    $gitCommit = Get-GitValue @("rev-parse", "--short", "HEAD")
    $gitStatus = Get-GitValue @("status", "--short")

    @(
        "MixCoach Project Checkpoint",
        "Created: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
        "Name: $safeName",
        "Root: $ProjectRoot",
        "Git branch: $gitBranch",
        "Git commit: $gitCommit",
        "",
        "Git status at checkpoint:",
        $(if ($gitStatus) { $gitStatus } else { "clean or git unavailable" }),
        "",
        "Restore command:",
        ".\scripts\ProjectCheckpoint.ps1 -Action Restore -Checkpoint `"$checkpointDir`" -Force"
    ) | Out-File -LiteralPath $manifestPath -Encoding UTF8

    Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath -Force
    Remove-Item -LiteralPath $stagingDir -Recurse -Force

    Write-OK "Checkpoint saved"
    Write-Host "  $zipPath" -ForegroundColor Gray
    Write-Host "  $manifestPath" -ForegroundColor Gray
}

function Show-Checkpoints {
    if (-not (Test-Path $BackupRoot)) {
        Write-Warn "No checkpoints found yet."
        return
    }

    Get-ChildItem -LiteralPath $BackupRoot -Directory | Sort-Object Name -Descending | ForEach-Object {
        $zip = Get-ChildItem -LiteralPath $_.FullName -Filter "*.zip" -File | Select-Object -First 1
        $size = if ($zip) { "{0:N1} MB" -f ($zip.Length / 1MB) } else { "no zip" }
        Write-Host "$($_.Name)  $size" -ForegroundColor Gray
        Write-Host "  $($_.FullName)" -ForegroundColor DarkGray
    }
}

function Restore-ProjectCheckpoint {
    param([string]$CheckpointPath)

    if (-not $Force) {
        throw "Restore requires -Force. This protects the current workspace from accidental overwrite."
    }

    if ([string]::IsNullOrWhiteSpace($CheckpointPath)) {
        throw "Pass -Checkpoint with either a checkpoint folder or zip path."
    }

    $resolved = Resolve-Path -LiteralPath $CheckpointPath -ErrorAction Stop
    $item = Get-Item -LiteralPath $resolved.Path

    if ($item.PSIsContainer) {
        $zip = Get-ChildItem -LiteralPath $item.FullName -Filter "*.zip" -File | Select-Object -First 1
    } else {
        $zip = $item
    }

    if (-not $zip -or -not (Test-Path $zip.FullName)) {
        throw "Checkpoint zip not found: $CheckpointPath"
    }

    Write-Warn "Creating emergency checkpoint before restore..."
    New-ProjectCheckpoint "pre_restore_$(Get-Date -Format 'yyyyMMdd_HHmmss')"

    $restoreTemp = Join-Path $env:TEMP ("MixCoach_restore_" + [guid]::NewGuid().ToString("N"))
    New-Item -Path $restoreTemp -ItemType Directory -Force | Out-Null

    Expand-Archive -LiteralPath $zip.FullName -DestinationPath $restoreTemp -Force

    $files = Get-ChildItem -LiteralPath $restoreTemp -Recurse -File -Force
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($restoreTemp.Length).TrimStart('\', '/')
        $dest = Join-Path $ProjectRoot $relative
        $destDir = Split-Path -Parent $dest
        if (-not (Test-Path $destDir)) {
            New-Item -Path $destDir -ItemType Directory -Force | Out-Null
        }
        Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
    }

    Remove-Item -LiteralPath $restoreTemp -Recurse -Force
    Write-OK "Checkpoint restored over current workspace"
    Write-Warn "Files created after the checkpoint are not deleted automatically."
}

switch ($Action) {
    "Save" {
        Write-Step "Saving project checkpoint"
        New-ProjectCheckpoint $Name
    }
    "List" {
        Write-Step "Available project checkpoints"
        Show-Checkpoints
    }
    "Restore" {
        Write-Step "Restoring project checkpoint"
        Restore-ProjectCheckpoint $Checkpoint
    }
}

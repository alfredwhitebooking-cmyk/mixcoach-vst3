param(
    [ValidateSet("Deploy", "ListBackups", "Restore")]
    [string]$Action = "Deploy",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [string]$Backup = "",

    [switch]$NoBackup,

    [switch]$Force,

    [switch]$KillFL
)

$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════
#  Constants
# ═══════════════════════════════════════════════════════════════════════════
$ProjectRoot      = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir         = Join-Path $ProjectRoot "build"
$VST3System       = "C:\Program Files\Common Files\VST3"
$DeployBackupRoot = Join-Path $ProjectRoot "workspace_backups\vst3_deploy"
$DeployLog        = Join-Path $ProjectRoot "workspace_memory\deploy_log.txt"
$Timestamp        = Get-Date -Format "yyyyMMdd_HHmmss"

# Minimum expected size for a valid VST3 binary (512 KB)
$MinBinarySize    = 512 * 1024

# ═══════════════════════════════════════════════════════════════════════════
#  I/O Helpers
# ═══════════════════════════════════════════════════════════════════════════
function Write-Step($Message) { Write-Host "`n== $Message ==" -ForegroundColor Cyan }
function Write-OK($Message)  { Write-Host "  [OK] $Message" -ForegroundColor Green }
function Write-Warn($Message){ Write-Host "  [WARN] $Message" -ForegroundColor Yellow }
function Write-Err($Message) { Write-Host "  [FAIL] $Message" -ForegroundColor Red }
function Write-Info($Message){ Write-Host "  .. $Message" -ForegroundColor Gray }

function Append-Log {
    param([string]$Message)
    $now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $dir = Split-Path $DeployLog -Parent
    if (-not (Test-Path $dir)) {
        New-Item -Path $dir -ItemType Directory -Force | Out-Null
    }
    Add-Content -Path $DeployLog -Value ("[$now] " + $Message) -Encoding utf8
}

# ═══════════════════════════════════════════════════════════════════════════
#  Validation Functions
# ═══════════════════════════════════════════════════════════════════════════

# Validates that a VST3 bundle folder contains the inner binary
# at Contents/x86_64-win/<Name>.vst3 with reasonable file size.
function Test-VST3BundleIntegrity {
    param(
        [Parameter(Mandatory=$true)][string]$BundlePath,
        [string]$ExpectedName = ""
    )

    if (-not (Test-Path $BundlePath)) {
        return $false, ("Bundle path does not exist: " + $BundlePath)
    }

    # Derive name from folder if not provided
    if ([string]::IsNullOrWhiteSpace($ExpectedName)) {
        $ExpectedName = Split-Path $BundlePath -Leaf
        if ($ExpectedName.EndsWith(".vst3")) {
            $ExpectedName = $ExpectedName.Substring(0, $ExpectedName.Length - 5)
        }
    }

    $innerBinary = Join-Path $BundlePath ("Contents\x86_64-win\" + $ExpectedName + ".vst3")

    if (-not (Test-Path $innerBinary)) {
        return $false, ("Inner binary MISSING: " + $innerBinary + " - bundle is empty/dead")
    }

    $fileInfo = Get-Item $innerBinary
    if ($fileInfo.Length -lt $MinBinarySize) {
        return $false, ("Inner binary too small: " + $fileInfo.Length.ToString() `
            + " bytes (min: " + $MinBinarySize.ToString() + ") - likely corrupt")
    }

    $timeStr = $fileInfo.LastWriteTime.ToString("yyyy-MM-dd HH:mm")
    return $true, ("OK (" + $fileInfo.Length.ToString() + " bytes, " + $timeStr + ")")
}

# Detects if FL Studio is running. With -KillFL, auto-terminates.
# Returns $true if locked (and not killed), $false if no lock or killed.
function Test-ProcessLock {
    $locked = $false
    $procs = @()

    foreach ($name in @("FL64", "FL", "FLEngine")) {
        $p = Get-Process -Name $name -ErrorAction SilentlyContinue
        if ($p) {
            $locked = $true
            $procs += $p
        }
    }

    if (-not $locked) { return $false }

    Write-Warn ("FL Studio appears to be running (" + $procs.Count.ToString() + " process(es) detected)")

    if ($KillFL) {
        Write-Info "  -KillFL is set - terminating FL Studio..."
        $procs | Stop-Process -Force

        # Poll until process is gone (up to 10 seconds)
        $timeout = 10
        $elapsed = 0
        while ($elapsed -lt $timeout) {
            $remaining = Get-Process -Name "FL64", "FL", "FLEngine" -ErrorAction SilentlyContinue
            if (-not $remaining) { break }
            Start-Sleep -Milliseconds 500
            $elapsed += 0.5
        }
        if ($elapsed -ge $timeout) {
            throw "Could not terminate FL Studio within " + $timeout.ToString() + "s. Close it manually."
        }

        Write-OK "FL Studio terminated"
        Append-Log "FL Studio auto-killed (-KillFL)"
        return $false
    }

    # Not killing - warn and let the deploy fail with a clear message
    Write-Warn "  Use -KillFL to auto-close FL Studio, or close it manually and retry."
    return $true
}

# Validates ALL source bundles BEFORE any copy starts.
# Throws if any bundle is missing or corrupt.
function Assert-SourceBundlesReady {
    param(
        [string]$MixCoachSource,
        [string]$MessengerSource
    )

    Write-Info "Validating build output bundles before deploy..."
    $anyFailed = $false

    $sources = @(
        @{ Name="MixCoach";  Path=$MixCoachSource },
        @{ Name="Messenger"; Path=$MessengerSource }
    )

    foreach ($s in $sources) {
        if (-not (Test-Path $s.Path)) {
            Write-Err ($s.Name + ": bundle not found at " + $s.Path)
            $anyFailed = $true
            continue
        }
        $ok, $msg = Test-VST3BundleIntegrity -BundlePath $s.Path -ExpectedName $s.Name
        if (-not $ok) {
            Write-Err ($s.Name + ": " + $msg)
            $anyFailed = $true
        } else {
            Write-OK ($s.Name + ": " + $msg)
        }
    }

    if ($anyFailed) {
        throw "Source bundles have integrity issues. Build first: .\build_fast.ps1 -All"
    }
}

# ═══════════════════════════════════════════════════════════════════════════
#  Core Deploy Logic
# ═══════════════════════════════════════════════════════════════════════════

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
        if (-not $latest) { throw "No VST3 deploy backups found in " + $DeployBackupRoot }
        return $latest.FullName
    }

    if (Test-Path -LiteralPath $BackupValue) {
        return (Resolve-Path -LiteralPath $BackupValue).Path
    }

    $candidate = Join-Path $DeployBackupRoot $BackupValue
    if (Test-Path -LiteralPath $candidate) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    throw "Backup not found: " + $BackupValue
}

# Deploys a VST3 bundle with transactional safety:
#   1. Backup current destination (if exists, unless -SkipBackup)
#   2. Copy source to temp directory
#   3. Validate temp copy integrity
#   4. Atomic swap: remove destination -> move temp to destination
#   5. Post-deploy validate destination
#   6. Rollback from backup if post-validation fails
function Copy-VST3Bundle {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Source,
        [Parameter(Mandatory=$true)][string]$Destination,
        [switch]$SkipBackup
    )

    # Pre-validate source integrity
    $sourceOk, $sourceMsg = Test-VST3BundleIntegrity -BundlePath $Source -ExpectedName $Name
    if (-not $sourceOk) {
        throw ($Name + " source bundle corrupt: " + $sourceMsg)
    }

    Assert-VST3System

    # Track backup path for potential rollback
    $backupPath = ""

    # Step 1: Backup current destination
    if ((Test-Path $Destination) -and (-not $SkipBackup)) {
        $backupDir = Join-Path $DeployBackupRoot $Timestamp
        New-Item -Path $backupDir -ItemType Directory -Force | Out-Null
        $backupPath = Join-Path $backupDir ($Name + ".vst3")
        Copy-Item -LiteralPath $Destination -Destination $backupPath -Recurse -Force
        Write-OK ("Backup " + $Name + ".vst3 -> " + $backupPath)
    }

    # Step 2: Copy to temp staging
    $tmpRoot = [System.IO.Path]::GetTempPath()
    $tmpDir = Join-Path $tmpRoot ("mixcoach_deploy_" + $Timestamp)
    New-Item -Path $tmpDir -ItemType Directory -Force | Out-Null
    $tmpDest = Join-Path $tmpDir ($Name + ".vst3")

    Write-Info ("Copying " + $Name + " to staging...")
    Copy-Item -LiteralPath $Source -Destination $tmpDest -Recurse -Force

    # Step 3: Validate temp copy
    $tmpOk, $tmpMsg = Test-VST3BundleIntegrity -BundlePath $tmpDest -ExpectedName $Name
    if (-not $tmpOk) {
        Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
        throw ($Name + " staging copy failed validation: " + $tmpMsg)
    }

    # Step 4: Atomic swap
    if (Test-Path $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    Move-Item -LiteralPath $tmpDest -Destination $Destination -Force

    # Cleanup temp
    if (Test-Path $tmpDir) {
        Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    # Step 5: Post-deploy validation
    $finalOk, $finalMsg = Test-VST3BundleIntegrity -BundlePath $Destination -ExpectedName $Name
    if (-not $finalOk) {
        Write-Err ($Name + " post-deploy validation FAILED: " + $finalMsg)
        if (($backupPath -ne "") -and (Test-Path $backupPath)) {
            Write-Warn "  Rolling back from backup..."
            Copy-Item -LiteralPath $backupPath -Destination $Destination -Recurse -Force
            Write-OK ($Name + " rolled back from backup")
            throw ($Name + " deploy failed - rolled back to previous version")
        } else {
            Write-Warn "  No backup was available for rollback (-NoBackup was active)"
            throw ($Name + " deploy failed - no rollback available")
        }
    }

    Write-OK ($Name + " deployed -> " + $Destination + " (" + $finalMsg + ")")
}

# ═══════════════════════════════════════════════════════════════════════════
#  Actions
# ═══════════════════════════════════════════════════════════════════════════

function Invoke-Deploy {
    Write-Step "Deploying MixCoach VST3s to Common Files"
    Write-Info ("Config: " + $Config)
    Write-Info ("Target: " + $VST3System)

    $mixCoachSource  = Join-Path $BuildDir ("MixCoach_artefacts\" + $Config + "\VST3\MixCoach.vst3")
    $messengerSource = Join-Path $BuildDir ("Messenger_artefacts\" + $Config + "\VST3\Messenger.vst3")

    # Pre-flight checks
    $hasLock = Test-ProcessLock
    if ($hasLock) {
        throw "FL Studio is running. Use -KillFL to auto-close, or close it manually."
    }

    Assert-SourceBundlesReady -MixCoachSource $mixCoachSource -MessengerSource $messengerSource

    # Deploy
    try {
        Copy-VST3Bundle -Name "MixCoach"  -Source $mixCoachSource  -Destination (Join-Path $VST3System "MixCoach.vst3")  -SkipBackup:$NoBackup
        Copy-VST3Bundle -Name "Messenger" -Source $messengerSource -Destination (Join-Path $VST3System "Messenger.vst3") -SkipBackup:$NoBackup
    }
    catch {
        Write-Err ("Deploy failed: " + $_.Exception.Message)
        Write-Host ""
        Append-Log ("DEPLOY FAILED: " + $_.Exception.Message)
        Write-Host "  Close FL Studio or any DAW that may be locking the VST3 files:" -ForegroundColor Yellow
        Write-Host "    taskkill /F /IM FL64.exe 2>nul" -ForegroundColor Gray
        Write-Host "    taskkill /F /IM FL.exe 2>nul" -ForegroundColor Gray
        Write-Host "  Then retry:" -ForegroundColor Yellow
        Write-Host ("    .\DeployVST3.ps1 -Config " + $Config) -ForegroundColor Gray
        exit 1
    }

    Write-Step "Deployment completed"
    Write-OK ("MixCoach and Messenger are deployed to " + $VST3System)
    Append-Log ("DEPLOY OK: Config=" + $Config + " | MixCoach OK | Messenger OK")
    Write-Info ("Log: " + $DeployLog)
}

function Show-Backups {
    Write-Step "Available VST3 deploy backups"
    $dirs = Get-BackupDirectories
    if ($dirs.Count -eq 0) {
        Write-Warn "No VST3 deploy backups found."
        return
    }

    foreach ($dir in $dirs) {
        $mixPath = Join-Path $dir.FullName "MixCoach.vst3"
        $msgPath = Join-Path $dir.FullName "Messenger.vst3"

        $mixOk = $false
        $mixMsg = "not found"
        if (Test-Path $mixPath) {
            $mixOk, $mixMsg = Test-VST3BundleIntegrity -BundlePath $mixPath -ExpectedName "MixCoach"
        }

        $msgOk = $false
        $msgMsg = "not found"
        if (Test-Path $msgPath) {
            $msgOk, $msgMsg = Test-VST3BundleIntegrity -BundlePath $msgPath -ExpectedName "Messenger"
        }

        $mixStatus = if ($mixOk) { "OK" } else { "BROKEN" }
        $msgStatus = if ($msgOk) { "OK" } else { "BROKEN" }

        Write-Host ($dir.Name + "  MixCoach=" + $mixStatus + "  Messenger=" + $msgStatus) -ForegroundColor Gray
        if (-not $mixOk) { Write-Host ("    " + $mixMsg) -ForegroundColor DarkYellow }
        if (-not $msgOk) { Write-Host ("    " + $msgMsg) -ForegroundColor DarkYellow }
    }

    Write-Host ""
    Write-Info "Restore with: .\DeployVST3.ps1 -Action Restore -Backup <dir> -Force"
}

function Restore-Backup {
    if (-not $Force) {
        throw "Restore requires -Force to avoid accidental overwrite."
    }

    $backupDir = Resolve-BackupDirectory $Backup
    Write-Step "Restoring VST3 deploy backup"
    Write-Info ("Source: " + $backupDir)
    Write-Info ("Target: " + $VST3System)

    $hasLock = Test-ProcessLock
    if ($hasLock) {
        throw "FL Studio is running. Use -KillFL to auto-close, or close it manually."
    }

    $mixBackup  = Join-Path $backupDir "MixCoach.vst3"
    $msgBackup  = Join-Path $backupDir "Messenger.vst3"

    # Validate backup integrity first
    $backupOk = $true
    if (Test-Path $mixBackup) {
        $ok, $msg = Test-VST3BundleIntegrity -BundlePath $mixBackup -ExpectedName "MixCoach"
        if (-not $ok) { Write-Warn ("MixCoach backup: " + $msg); $backupOk = $false }
    }
    if (Test-Path $msgBackup) {
        $ok, $msg = Test-VST3BundleIntegrity -BundlePath $msgBackup -ExpectedName "Messenger"
        if (-not $ok) { Write-Warn ("Messenger backup: " + $msg); $backupOk = $false }
    }
    if (-not $backupOk) {
        Write-Warn "Some backups have integrity issues - proceeding anyway (-Force)"
    }

    try {
        Copy-VST3Bundle -Name "MixCoach"  -Source $mixBackup -Destination (Join-Path $VST3System "MixCoach.vst3")  -SkipBackup:$NoBackup
        Copy-VST3Bundle -Name "Messenger" -Source $msgBackup -Destination (Join-Path $VST3System "Messenger.vst3") -SkipBackup:$NoBackup
    }
    catch {
        Write-Err ("Restore failed: " + $_.Exception.Message)
        Write-Host "  Close FL Studio or any DAW that may be locking the VST3 files." -ForegroundColor Yellow
        exit 1
    }

    Write-OK ("VST3 deploy restored from " + $backupDir)
    Append-Log ("RESTORE OK: from " + $backupDir)
}

function Show-Help {
    Write-Host "MixCoach VST3 Deploy Tool" -ForegroundColor Cyan
    Write-Host "Usage:"
    Write-Host "  .\DeployVST3.ps1                                    Deploy Release both plugins"
    Write-Host "  .\DeployVST3.ps1 -Config Debug                      Deploy Debug build"
    Write-Host "  .\DeployVST3.ps1 -KillFL                            Auto-close FL Studio before deploy"
    Write-Host "  .\DeployVST3.ps1 -NoBackup                          Skip backup of current system VST3s"
    Write-Host ""
    Write-Host "  .\DeployVST3.ps1 -Action ListBackups                List available deploy backups"
    Write-Host "  .\DeployVST3.ps1 -Action Restore -Backup latest -Force   Restore latest backup"
    Write-Host "  .\DeployVST3.ps1 -Action Restore -Backup 20260623_145046 -Force  Restore specific backup"
}

# ═══════════════════════════════════════════════════════════════════════════
#  Entry point
# ═══════════════════════════════════════════════════════════════════════════

if ($Action -eq "Deploy") {
    if (-not (Test-Path $BuildDir)) {
        Write-Err ("Build directory not found: " + $BuildDir)
        Write-Host "  Run build first: .\build_fast.ps1" -ForegroundColor Yellow
        exit 1
    }
}

switch ($Action) {
    "Deploy"       { Invoke-Deploy }
    "ListBackups"  { Show-Backups }
    "Restore"      { Restore-Backup }
}

$ErrorActionPreference = "SilentlyContinue"

Write-Host "============================================" -ForegroundColor Magenta
Write-Host "  MIXCOACH SYSTEM CLEANUP" -ForegroundColor Magenta
Write-Host "============================================" -ForegroundColor Magenta

# PASO 1: MATAR PROCESOS
Write-Host "[1/4] Matando procesos innecesarios..." -ForegroundColor Cyan

$processesToKill = @(
    "UAMixerEngine", "UAMixerHelper",
    "ArmouryCrate", "ArmouryCrateControlInterface",
    "ArmouryCrateService",
    "AsusAppService", "AsusCertService", "ASUSOptimization",
    "ASUSSoftwareManager", "ASUSSwitch", "ASUSSystemAnalysis",
    "ASUSSystemDiagnosis", "LightingService", "asus_framework",
    "VirtualPet",
    "CrossDeviceService",
    "SoundIDReference",
    "CymaticsHub",
    "LMStudio",
    "mscopilot",
    "WavesLocalServer", "WavesPluginServer",
    "NIHardwareAccessibilityHelper",
    "DigidesignMMERefresh",
    "OneDrive",
    "PhoneExperienceHost",
    "StartMenuExperienceHost",
    "TextInputHost",
    "Widgets", "WidgetService",
    "SearchHost"
)

$killed = 0
foreach ($proc in $processesToKill) {
    $p = Get-Process -Name $proc -ErrorAction SilentlyContinue
    if ($p) {
        Stop-Process -Name $proc -Force
        Write-Host "  OK: $proc" -ForegroundColor Green
        $killed++
    }
}
Write-Host "  => $killed procesos terminados" -ForegroundColor Yellow

# PASO 2: DESHABILITAR STARTUPS (Registry)
Write-Host "[2/4] Deshabilitando programas de inicio..." -ForegroundColor Cyan

$startupEntries = @(
    "UA Connect",
    "SoundID Reference.exe",
    "fm.cymatics.hub",
    "electron.app.LM Studio",
    "NIHardwareAccessibilityHelper.exe",
    "WavesLocalServer",
    "WavesPluginServer",
    "Virtual Pet"
)

$path = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$disabledStartup = 0

foreach ($entry in $startupEntries) {
    $val = Get-ItemProperty -Path $path -Name $entry -ErrorAction SilentlyContinue
    if ($val -ne $null) {
        Remove-ItemProperty -Path $path -Name $entry
        Write-Host "  OK: Startup '$entry' eliminado" -ForegroundColor Green
        $disabledStartup++
    }
}

# Edge auto-launch
$edgeTask = Get-ScheduledTask -TaskPath "\Microsoft\Edge\" -TaskName "EdgeUpdateTaskMachineUA" -ErrorAction SilentlyContinue
if ($edgeTask) {
    Disable-ScheduledTask -TaskPath "\Microsoft\Edge\" -TaskName "EdgeUpdateTaskMachineUA"
    Write-Host "  OK: Edge auto-launch deshabilitado" -ForegroundColor Green
    $disabledStartup++
}

# OneDrive
$onedrive = Get-Process -Name "OneDrive" -ErrorAction SilentlyContinue
if ($onedrive) {
    Stop-Process -Name "OneDrive" -Force
}
# Deshabilitar OneDrive startup via registry
$odVal = Get-ItemProperty -Path $path -Name "OneDrive" -ErrorAction SilentlyContinue
if ($odVal -ne $null) {
    Remove-ItemProperty -Path $path -Name "OneDrive"
    Write-Host "  OK: OneDrive startup eliminado" -ForegroundColor Green
    $disabledStartup++
}

Write-Host "  => $disabledStartup startups deshabilitados" -ForegroundColor Yellow

# PASO 3: DESHABILITAR SERVICIOS
Write-Host "[3/4] Deshabilitando servicios..." -ForegroundColor Cyan

$servicesToDisable = @(
    @{Name="ArmouryCrateControlInterface"; Action="Disabled"},
    @{Name="ArmouryCrateService"; Action="Disabled"},
    @{Name="AsusAppService"; Action="Disabled"},
    @{Name="AsusCertService"; Action="Disabled"},
    @{Name="asus"; Action="Disabled"},
    @{Name="ASUSOptimization"; Action="Disabled"},
    @{Name="ASUSSoftwareManager"; Action="Disabled"},
    @{Name="ASUSSwitch"; Action="Disabled"},
    @{Name="ASUSSystemAnalysis"; Action="Disabled"},
    @{Name="ASUSSystemDiagnosis"; Action="Disabled"},
    @{Name="LightingService"; Action="Disabled"},
    @{Name="UAHelperService"; Action="Manual"},
    @{Name="edgeupdate"; Action="Manual"},
    @{Name="edgeupdatem"; Action="Manual"},
    @{Name="NvContainerLocalSystem"; Action="Manual"}
)

$disabledSvcs = 0
foreach ($svc in $servicesToDisable) {
    $s = Get-Service -Name $svc.Name -ErrorAction SilentlyContinue
    if ($s) {
        Set-Service -Name $svc.Name -StartupType $svc.Action
        if ($s.Status -eq 'Running') {
            Stop-Service -Name $svc.Name -Force
        }
        Write-Host "  OK: $($svc.Name) => $($svc.Action)" -ForegroundColor Green
        $disabledSvcs++
    }
}
Write-Host "  => $disabledSvcs servicios reconfigurados" -ForegroundColor Yellow

# PASO 4: MATAR EDGE WEBVIEW2
Write-Host "[4/4] Limpiando Edge WebView2..." -ForegroundColor Cyan
$wv = Get-Process -Name "msedgewebview2" -ErrorAction SilentlyContinue
if ($wv) {
    $wvCount = ($wv | Measure-Object).Count
    Stop-Process -Name "msedgewebview2" -Force
    Write-Host "  OK: $wvCount procesos msedgewebview2 terminados" -ForegroundColor Green
} else {
    Write-Host "  - No hay procesos msedgewebview2" -ForegroundColor Gray
}

# RESUMEN
Write-Host "`n============================================" -ForegroundColor Green
Write-Host "  LIMPIEZA COMPLETADA" -ForegroundColor Green
Write-Host "  Procesos terminados:      $killed" -ForegroundColor Green
Write-Host "  Startups deshabilitados:  $disabledStartup" -ForegroundColor Green
Write-Host "  Servicios reconfigurados: $disabledSvcs" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host "  NOTA: Para efectos completos, reinicia el PC." -ForegroundColor Yellow

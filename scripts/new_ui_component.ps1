#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Scaffolds a new JUCE UI component following MixCoach project conventions.

.DESCRIPTION
    Creates .h + .cpp template files for a UI component with proper includes,
    namespace, MixCoachTheme integration, and all boilerplate. Optionally
    registers the files in CMakeLists.txt and/or creates a test scaffold.

    Run with -DryRun first to preview without writing anything.

.PARAMETER ComponentName
    PascalCase name of the component (e.g. "GainReducer", "SpectralMatch").

.PARAMETER OutputDir
    Directory for the new files, relative to project root.
    Default: "Source/MixCoach/UI"

.PARAMETER Target
    Which plugin target to add sources to. Valid: "MixCoach" or "Messenger".
    Default: "MixCoach"

.PARAMETER WithSmoothValue
    Include a SmoothValue<float> member and the required header.

.PARAMETER WithSharedData
    Forward-declare SharedData and pass it in the constructor (for components
    that need access to shared audio analysis data).

.PARAMETER WithTelemetry
    Include TelemetryData.h and TelemetryCollector.h for DSP pipeline access.

.PARAMETER ParentClass
    Base class for the component. Default: "juce::Component"
    Other common: "juce::TabbedComponent", "juce::GroupComponent"

.PARAMETER UpdateCMake
    Register the new .h/.cpp files in CMakeLists.txt under ANALYZERS_UI_SOURCES
    (or add to Messenger's target_sources if -Target is Messenger).

.PARAMETER CreateTest
    Scaffold a unit test file in tests/Test<ComponentName>.cpp.

.PARAMETER Force
    Overwrite existing files without prompt.

.PARAMETER DryRun
    Preview the files that would be created and the CMake changes, without
    writing anything.

.EXAMPLE
    .\scripts\new_ui_component.ps1 -ComponentName SpectralMatch -DryRun

.EXAMPLE
    .\scripts\new_ui_component.ps1 -ComponentName GainReducer -WithSmoothValue -UpdateCMake

.EXAMPLE
    .\scripts\new_ui_component.ps1 -ComponentName PhaseRotator -UpdateCMake -CreateTest

.EXAMPLE
    .\scripts\new_ui_component.ps1 -ComponentName TelemetryInspector -Target Messenger -WithTelemetry -UpdateCMake
#>

param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidatePattern('^[A-Z][a-zA-Z0-9]+$')]
    [string]$ComponentName,

    [Parameter(Mandatory = $false)]
    [string]$OutputDir = "Source/MixCoach/UI",

    [Parameter(Mandatory = $false)]
    [ValidateSet("MixCoach", "Messenger")]
    [string]$Target = "MixCoach",

    [Parameter(Mandatory = $false)]
    [switch]$WithSmoothValue,

    [Parameter(Mandatory = $false)]
    [switch]$WithSharedData,

    [Parameter(Mandatory = $false)]
    [switch]$WithTelemetry,

    [Parameter(Mandatory = $false)]
    [string]$ParentClass = "juce::Component",

    [Parameter(Mandatory = $false)]
    [switch]$UpdateCMake,

    [Parameter(Mandatory = $false)]
    [switch]$CreateTest,

    [Parameter(Mandatory = $false)]
    [switch]$Force,

    [Parameter(Mandatory = $false)]
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

# Helper: Color output
function Write-Step   { Write-Host "  $($args -join ' ')" -ForegroundColor Yellow }
function Write-OK     { Write-Host "  $($args -join ' ')" -ForegroundColor Green }
function Write-Info   { Write-Host "    $($args -join ' ')" -ForegroundColor Gray }
function Write-Create { Write-Host "  $($args -join ' ')" -ForegroundColor Cyan }
function Write-Error  { Write-Host "  $($args -join ' ')" -ForegroundColor Red }

# Validate component name
if ($ComponentName.Length -lt 2) {
    Write-Error "Component name must be at least 2 characters."
    exit 1
}
if (-not ($ComponentName -cmatch '^[A-Z][a-zA-Z0-9]*$')) {
    Write-Error "Component name must be PascalCase (e.g. SpectralMatch), got: $ComponentName"
    exit 1
}

# Resolve paths
$HeaderPath = Join-Path $ProjectRoot "$OutputDir/$ComponentName.h"
$ImplPath   = Join-Path $ProjectRoot "$OutputDir/$ComponentName.cpp"
$CMakePath  = Join-Path $ProjectRoot "CMakeLists.txt"
$TestDir    = Join-Path $ProjectRoot "tests"
$TestPath   = Join-Path $TestDir "Test$ComponentName.cpp"

# Validate OutputDir exists
$FullOutputDir = Join-Path $ProjectRoot $OutputDir
if (-not (Test-Path $FullOutputDir)) {
    Write-Error "Output directory '$OutputDir' does not exist. Create it first or pick an existing one."
    exit 1
}

# Validate CMakeLists.txt exists (if -UpdateCMake)
if ($UpdateCMake -and -not (Test-Path $CMakePath)) {
    Write-Error "CMakeLists.txt not found at '$CMakePath'. Cannot update."
    exit 1
}

# ---------------------------------------------------------------------------
# BUILD HEADER CONTENT
# ---------------------------------------------------------------------------
$includes = @(
    '#include <juce_gui_basics/juce_gui_basics.h>',
    '#include "MixCoachTheme.h"'
)
if ($WithSmoothValue) { $includes += '#include "SmoothValue.h"' }
if ($WithTelemetry)   { $includes += '#include <TelemetryData.h>' }
if ($WithSharedData)  { $includes += 'class SharedData;' }

$members = @()
if ($WithSmoothValue) { $members += "    SmoothValue<float> smoothValue_;" }
if ($WithSharedData)  { $members += "    SharedData& sharedData_;" }

$memberSection = ""
if ($members.Count -gt 0) {
    $memberSection = "`n`nprivate:`n" + ($members -join "`n")
}

if ($WithSharedData) {
    $constructorDecl = "    explicit $ComponentName(SharedData& sharedData);"
} else {
    $constructorDecl = "    $ComponentName();"
}

$headerContent = @"
#pragma once
$($includes -join "`n")

namespace mixcoach {

/**
 * $ComponentName
 *
 * TODO: Describe the purpose of this component.
 * Follows MixCoach UI conventions: glass panel background, theme colors,
 * proper resized() layout.
 */
class $ComponentName : public $ParentClass {
public:
$constructorDecl
    ~$ComponentName() override = default;

    void paint(juce::Graphics\& g) override;
    void resized() override;

    /**
     * Called externally to push fresh data for display.
     */
    void update();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR($ComponentName)$memberSection
};

} // namespace mixcoach
"@

# ---------------------------------------------------------------------------
# BUILD IMPLEMENTATION CONTENT
# ---------------------------------------------------------------------------
if ($WithSharedData) {
    $implContent = @"
#include "$ComponentName.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

$ComponentName::$ComponentName(SharedData& sharedData)
    : sharedData_(sharedData)
{
    setOpaque(true);
}

void $ComponentName::paint(juce::Graphics\& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds);
    g.setColour(MixCoachTheme::textDim());
    g.setFont(MixCoachTheme::fontSizeBody);
    g.drawText("$ComponentName", bounds, juce::Justification::centred);
}

void $ComponentName::resized()
{
    auto area = getLocalBounds();
    // TODO: layout child components
}

void $ComponentName::update()
{
    repaint();
}

} // namespace mixcoach
"@
} else {
    $implContent = @"
#include "$ComponentName.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

$ComponentName::$ComponentName()
{
    setOpaque(true);
}

void $ComponentName::paint(juce::Graphics\& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds);
    g.setColour(MixCoachTheme::textDim());
    g.setFont(MixCoachTheme::fontSizeBody);
    g.drawText("$ComponentName", bounds, juce::Justification::centred);
}

void $ComponentName::resized()
{
    auto area = getLocalBounds();
    // TODO: layout child components
}

void $ComponentName::update()
{
    repaint();
}

} // namespace mixcoach
"@
}

# ---------------------------------------------------------------------------
# BUILD TEST CONTENT (if -CreateTest)
# Use single-quoted here-string to avoid any $variable expansion issues.
# ---------------------------------------------------------------------------
$rawTestContent = @'
// ═══════════════════════════════════════════════════════════════════════════
//  TestPLACEHOLDER.cpp — Tests unitarios para PLACEHOLDER
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <algorithm>

// ─── Macros de test (inline, sin dependencia externa) ─────────────────────
static int g_TestPassed = 0;
static int g_TestFailed = 0;

#define TEST(name, condition)                                         \
    do {                                                              \
        if (!(condition)) {                                           \
            std::printf("  FAIL %s (%s:%d)\n", name, __FILE__, __LINE__); \
            ++g_TestFailed;                                           \
        } else {                                                      \
            std::printf("  PASS %s\n", name);                         \
            ++g_TestPassed;                                           \
        }                                                             \
    } while(0)

#define TEST_NEAR(name, a, b, tol)                                    \
    do {                                                              \
        auto diff = std::fabs((double)(a) - (double)(b));             \
        if (diff > (double)(tol)) {                                   \
            std::printf("  FAIL %s: |%g - %g| = %g > %g\n",           \
                name, (double)(a), (double)(b), diff, (double)(tol)); \
            ++g_TestFailed;                                           \
        } else {                                                      \
            std::printf("  PASS %s\n", name);                         \
            ++g_TestPassed;                                           \
        }                                                             \
    } while(0)

// TODO: link JUCE modules if needed, or write standalone tests
// #include <juce_core/juce_core.h>
// #include <juce_graphics/juce_graphics.h>

// ─── Test: Construction ────────────────────────────────────────────────────
static void test_construction()
{
    std::printf("\n── Construction ──\n");
    // TODO: instantiate PLACEHOLDER and verify default state
    // PLACEHOLDER component;
    // TEST("default state ok", true);
}

// ─── Test: Basic properties ────────────────────────────────────────────────
static void test_basic_properties()
{
    std::printf("\n── Basic Properties ──\n");
    // TODO: test setters/getters
}

// ─── Main ──────────────────────────────────────────────────────────────────
int main()
{
    std::printf("\n════════════════════════════════════════════════════\n");
    std::printf("  TestPLACEHOLDER\n");
    std::printf("════════════════════════════════════════════════════\n");

    test_construction();
    test_basic_properties();

    std::printf("\n────────────────────────────────────────────────────\n");
    std::printf("  Results: %d passed, %d failed\n", g_TestPassed, g_TestFailed);
    std::printf("────────────────────────────────────────────────────\n");

    return g_TestFailed > 0 ? 1 : 0;
}
'@

# Replace PLACEHOLDER with actual component name
$testContent = $rawTestContent -replace 'PLACEHOLDER', $ComponentName

# ---------------------------------------------------------------------------
# CHECK FOR EXISTING FILES
# ---------------------------------------------------------------------------
$filesToCreate = @(
    @{ Path = $HeaderPath; Label = "$OutputDir/$ComponentName.h" }
    @{ Path = $ImplPath;   Label = "$OutputDir/$ComponentName.cpp" }
)
if ($CreateTest) {
    $filesToCreate += @{ Path = $TestPath; Label = "tests/Test$ComponentName.cpp" }
}

$existingFiles = $filesToCreate | Where-Object { Test-Path $_.Path }
if ($existingFiles.Count -gt 0 -and -not $Force -and -not $DryRun) {
    Write-Error "Existing files found. Use -Force to overwrite:"
    $existingFiles | ForEach-Object { Write-Error "  $($_.Label)" }
    exit 1
}

# ---------------------------------------------------------------------------
# SHOW SUMMARY
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Scaffold UI Component: $ComponentName" -ForegroundColor Cyan
Write-Host "  Target: $Target   |   Parent: $ParentClass" -ForegroundColor Cyan
if ($WithSmoothValue) { Write-Host "  + SmoothValue smoothing member" -ForegroundColor Cyan }
if ($WithSharedData)  { Write-Host "  + SharedData dependency" -ForegroundColor Cyan }
if ($WithTelemetry)   { Write-Host "  + Telemetry pipeline" -ForegroundColor Cyan }
if ($CreateTest)      { Write-Host "  + Test scaffold" -ForegroundColor Cyan }
if ($UpdateCMake)     { Write-Host "  + CMakeLists.txt update" -ForegroundColor Cyan }
Write-Host "════════════════════════════════════════════════════" -ForegroundColor Cyan

Write-Host ""
Write-Step "Files to create:"
foreach ($file in $filesToCreate) {
    if (Test-Path $file.Path) {
        Write-Error "$($file.Label) - already exists (will be overwritten)"
    } else {
        Write-Create $file.Label
    }
}

if ($DryRun) {
    Write-Host ""
    Write-Info "--- Header preview ---"
    $headerContent -split "`n" | ForEach-Object { Write-Info $_ }
    Write-Info "--- Implementation preview ---"
    $implContent -split "`n" | ForEach-Object { Write-Info $_ }
    if ($CreateTest) {
        Write-Info "--- Test preview ---"
        $testContent -split "`n" | ForEach-Object { Write-Info $_ }
    }
    if ($UpdateCMake) {
        Write-Info "--- CMake: will add to ANALYZERS_UI_SOURCES or Messenger target_sources ---"
    }
    Write-Host ""
    Write-OK "Dry run complete - no files written."
    return
}

# ---------------------------------------------------------------------------
# WRITE FILES
# ---------------------------------------------------------------------------
Write-Host ""

try {
    Set-Content -Path $HeaderPath -Value $headerContent -Encoding UTF8 -NoNewline
    Write-OK "$OutputDir/$ComponentName.h"
} catch {
    Write-Error "Failed to write header: $_"
    exit 1
}

try {
    Set-Content -Path $ImplPath -Value $implContent -Encoding UTF8 -NoNewline
    Write-OK "$OutputDir/$ComponentName.cpp"
} catch {
    Write-Error "Failed to write implementation: $_"
    exit 1
}

if ($CreateTest) {
    try {
        if (-not (Test-Path $TestDir)) {
            New-Item -ItemType Directory -Path $TestDir -Force | Out-Null
        }
        Set-Content -Path $TestPath -Value $testContent -Encoding UTF8 -NoNewline
        Write-OK "tests/Test$ComponentName.cpp"
    } catch {
        Write-Error "Failed to write test: $_"
        exit 1
    }
}

# ---------------------------------------------------------------------------
# UPDATE CMakeLists.txt (if -UpdateCMake)
# ---------------------------------------------------------------------------
if ($UpdateCMake) {
    $relHeader = "$OutputDir/$ComponentName.h" -replace '\\', '/'
    $relImpl   = "$OutputDir/$ComponentName.cpp" -replace '\\', '/'

    $cmakeContent = Get-Content $CMakePath -Raw

    if ($Target -eq "Messenger") {
        $pattern = '(target_sources\(Messenger PRIVATE(?:\s+(?:\r?\n))?)((?:    [^\r\n]*(?:\r?\n))*)(\s+\))'
        $replacement = '${1}${2}    ' + $relHeader + "`r`n    " + $relImpl + "`r`n" + '${3}'
        if ($cmakeContent -match $pattern) {
            $newContent = $cmakeContent -replace $pattern, $replacement
            Set-Content -Path $CMakePath -Value $newContent -Encoding UTF8 -NoNewline
            Write-OK "Updated Messenger target_sources in CMakeLists.txt"
        } else {
            Write-Error "Could not find Messenger target_sources block in CMakeLists.txt"
            exit 1
        }
    } else {
        $pattern = '(set\(ANALYZERS_UI_SOURCES(?:\s+(?:\r?\n))?)((?:    [^\r\n]*(?:\r?\n))*)(\s+\))'
        $replacement = '${1}${2}    ' + $relHeader + "`r`n    " + $relImpl + "`r`n" + '${3}'
        if ($cmakeContent -match $pattern) {
            $newContent = $cmakeContent -replace $pattern, $replacement
            Set-Content -Path $CMakePath -Value $newContent -Encoding UTF8 -NoNewline
            Write-OK "Updated ANALYZERS_UI_SOURCES in CMakeLists.txt"
        } else {
            Write-Error "Could not find ANALYZERS_UI_SOURCES block in CMakeLists.txt"
            exit 1
        }
    }
}

# ---------------------------------------------------------------------------
# SUMMARY
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-OK "Component $ComponentName created!"
Write-Host ""
Write-Info "Next steps:"
Write-Info "  1. Implement paint() and resized() in $OutputDir/$ComponentName.cpp"
if ($WithSmoothValue) {
    Write-Info "  2. Configure SmoothValue time constant in constructor"
}
if ($UpdateCMake) {
    Write-Info "  3. Reconfigure CMake: cd MixCoach && cmake -B build"
    Write-Info "  4. Rebuild:   cmake --build build --config Release --target MixCoach"
} else {
    Write-Info "  3. Register in CMakeLists.txt (run with -UpdateCMake next time)"
}
if ($CreateTest) {
    Write-Info "  4. Implement tests in tests/Test$ComponentName.cpp"
    Write-Info "  5. Add Test$ComponentName to CMakeLists.txt and validate.ps1"
}
Write-Host "════════════════════════════════════════════════════" -ForegroundColor Cyan

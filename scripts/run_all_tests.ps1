param(
    [string]$TestsDir = "tests/Release"
)

$projectRoot = Split-Path -Parent $PSScriptRoot
$testsPath = Join-Path $projectRoot "build/$TestsDir"

Write-Host "=== MixCoach Test Suite ===" -ForegroundColor Cyan
Write-Host "Tests directory: $testsPath" -ForegroundColor Gray
Write-Host ""

$tests = Get-ChildItem -Path $testsPath -Filter "Test*.exe" | Sort-Object Name
$pass = 0
$fail = 0
$failList = @()

foreach ($t in $tests) {
    $name = $t.Name
    Write-Host "  Running: $name" -NoNewline
    
    $proc = Start-Process -FilePath $t.FullName -NoNewWindow -Wait -PassThru
    
    if ($proc.ExitCode -eq 0) {
        Write-Host "  [PASS]" -ForegroundColor Green
        $pass++
    }
    else {
        Write-Host "  [FAIL] (exit code: $($proc.ExitCode))" -ForegroundColor Red
        $fail++
        $failList += "$name (exit=$($proc.ExitCode))"
    }
}

Write-Host ""
Write-Host "=== RESULTS ===" -ForegroundColor Cyan
Write-Host "  Total:  $($tests.Count)" -ForegroundColor White
Write-Host "  Passed: $pass" -ForegroundColor Green
Write-Host "  Failed: $fail" -ForegroundColor Red

if ($fail -gt 0) {
    Write-Host ""
    Write-Host "FAILED TESTS:" -ForegroundColor Red
    foreach ($f in $failList) {
        Write-Host "  • $f" -ForegroundColor Red
    }
}

if ($fail -eq 0) {
    Write-Host ""
    Write-Host "✅ ALL TESTS PASSED" -ForegroundColor Green
}

exit $fail

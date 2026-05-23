@echo off
REM ──────────────────────────────────────────────────────────────────────────
REM  MixCoach — Build Wrapper (cmd.exe)
REM  Redirige a build.ps1 para compatibilidad con PowerShell
REM ──────────────────────────────────────────────────────────────────────────
REM  Uso: build.bat [-Clean] [-Debug] [-NoDeploy] [-Help]
REM ──────────────────────────────────────────────────────────────────────────

powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build fallo. Revisa el log en build_output.txt
    pause
)

@echo off
REM ──────────────────────────────────────────────────────────────────────────
REM  MixCoach — Deploy VST3 Post-Build Script
REM  Uso: deploy_vst3_postbuild.bat <cmake_exe> <src> <dst>
REM  Ejemplo:
REM    deploy_vst3_postbuild.bat "C:\cmake\bin\cmake.exe" ^
REM        "C:\Build\MixCoach.vst3" ^
REM        "C:\Program Files\Common Files\VST3\MixCoach.vst3"
REM
REM  Este script INTENTA copiar el VST3 al directorio del sistema.
REM  Si el archivo esta bloqueado (por el DAW), falla silenciosamente
REM  con codigo 0 para NO detener la compilacion.
REM ──────────────────────────────────────────────────────────────────────────

set CMAKE_EXE=%~1
set SRC=%~2
set DST=%~3

REM Eliminar version anterior si existe (puede fallar si esta bloqueado)
if exist "%DST%" (
    rmdir /s /q "%DST%" 2>nul
)

REM Copiar el VST3 (puede fallar si el DAW lo tiene bloqueado)
"%CMAKE_EXE%" -E copy_directory "%SRC%" "%DST%" >nul 2>&1

if %errorlevel% neq 0 (
    echo [AVISO] No se pudo copiar %~nx3 - puede estar bloqueado por el DAW
    echo [AVISO] Cierra el DAW y ejecuta: .\DeployVST3.ps1
)

REM Siempre salir con 0 para no detener la compilacion
exit /b 0

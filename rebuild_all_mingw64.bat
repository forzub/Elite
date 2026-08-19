@echo off
setlocal EnableExtensions

rem Thin Windows wrapper. The real build logic lives in rebuild_all_mingw64.sh
rem so Git Bash/MSYS2 and double-click/CMD use the same canonical procedure.

set "ROOT=%~dp0"
set "MSYS2_SHELL=C:\msys64\msys2_shell.cmd"

if not exist "%MSYS2_SHELL%" (
    echo [REBUILD][FAIL] MSYS2 shell was not found at:
    echo   %MSYS2_SHELL%
    echo.
    echo Run rebuild_all_mingw64.sh from the existing MINGW64 shell,
    echo or update MSYS2_SHELL in this wrapper if MSYS2 is installed elsewhere.
    exit /b 2
)

pushd "%ROOT%" >nul
call "%MSYS2_SHELL%" -defterm -here -no-start -mingw64 -c "bash ./rebuild_all_mingw64.sh %*"
set "RC=%ERRORLEVEL%"
popd >nul

exit /b %RC%

@echo off
setlocal
cd /d "%~dp0"

rem Windows entry point for the build tooling. Prefers uv (which can fetch its
rem own Python), falls back to the py launcher, then plain python. The real
rem work lives in git_wrapper_builder.pyz, built by build_pyz.py if needed.

where uv >nul 2>nul
if not errorlevel 1 goto use_uv
where py >nul 2>nul
if not errorlevel 1 goto use_py
where python >nul 2>nul
if not errorlevel 1 goto use_python

echo [!] Neither uv nor Python was found on PATH.
echo     Install uv: https://docs.astral.sh/uv/   (or Python 3.12+)
pause
exit /b 1

:use_uv
cls
echo Running build script using uv...
echo.
uv run python build_pyz.py --run %*
goto check

:use_py
cls
echo Running build script using py...
echo.
py build_pyz.py --run %*
goto check

:use_python
cls
echo Running build script using python...
echo.
python build_pyz.py --run %*
goto check

:check
if errorlevel 1 (
    echo.
    echo [!] Build failed or was interrupted. Press any key to close...
    pause >nul
    exit /b 1
)
endlocal

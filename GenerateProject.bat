@echo off
cd /d "%~dp0"

set "PY=python"
where %PY% >nul 2>nul
if errorlevel 1 (
    set "PY=py"
    where %PY% >nul 2>nul
    if errorlevel 1 (
        echo Python is not installed or not in PATH.
        pause
        exit /b 1
    )
)

%PY% -c "import imgui_bundle" >nul 2>nul
if errorlevel 1 (
    echo Installing imgui-bundle...
    %PY% -m pip install --user --upgrade --only-binary=:all: imgui-bundle
    if errorlevel 1 (
        echo Failed to install imgui-bundle.
        pause
        exit /b 1
    )
) else (
    start "" /b cmd /c "%PY% -m pip install --user --upgrade --quiet --only-binary=:all: --disable-pip-version-check imgui-bundle >nul 2>nul"
)

%PY% GenerateProject.py %*

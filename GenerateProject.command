#!/bin/bash
cd "$(dirname "$0")"

PY="python3"
if ! command -v "$PY" >/dev/null 2>&1; then
    PY="python"
    if ! command -v "$PY" >/dev/null 2>&1; then
        echo "Python is not installed or not in PATH."
        read -n 1 -s -r -p "Press any key to exit..."
        exit 1
    fi
fi

if ! "$PY" -c "import imgui_bundle" >/dev/null 2>&1; then
    echo "Installing imgui-bundle..."
    if ! "$PY" -m pip install --user --upgrade --only-binary=:all: imgui-bundle; then
        echo "Failed to install imgui-bundle."
        read -n 1 -s -r -p "Press any key to exit..."
        exit 1
    fi
else
    ( nohup "$PY" -m pip install --user --upgrade --quiet --only-binary=:all: --disable-pip-version-check imgui-bundle >/dev/null 2>&1 & )
fi

"$PY" GenerateProject.py "$@"

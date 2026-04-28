@echo off
setlocal
set "BUILDER=%~1\Bin\Windows\AssetsBuilder.exe"
if exist "%BUILDER%" (
    echo Host AssetsBuilder.exe present, skipping native rebuild.
    exit /b 0
)
cmake --preset windows ^
 && cmake --build --preset windows -j 4 --target AssetsBuilder

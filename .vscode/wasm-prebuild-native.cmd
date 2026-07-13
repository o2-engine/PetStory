@echo off
setlocal
set "BUILDER=%~1\Bin\Windows\AssetsBuilder.exe"
if not exist "%BUILDER%" (
    cmake --preset windows || exit /b 1
    cmake --build --preset windows -j 4 --target AssetsBuilder || exit /b 1
)
"%BUILDER%" -platform WebAssembly -source "%~1\Assets\" -target "%~1\BuiltAssets\WebAssembly\Data\" -target-tree "%~1\BuiltAssets\WebAssembly\Data.json" -compressor-config "%~1\o2\CompressToolsConfig.json"

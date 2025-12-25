@echo off
del /Q .\Bin\*.spv
setlocal enabledelayedexpansion

for %%f in (*.vert *.frag *.geom *.tesc *.tese *.comp) do (
    set "name=%%~nf"
    set "ext=%%~xf"

    glslc "%%f" -O -o ".\Bin\!name!!ext!.spv" --target-env=vulkan1.1
)
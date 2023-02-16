@echo off
setlocal enabledelayedexpansion

rem glslangValidator src/shaders/blit.vert -V --vn blit_vert -o src/shaders/blit.vert.h
rem glslangValidator src/shaders/blit.frag -V --vn blit_frag -o src/shaders/blit.frag.h
rem glslangValidator src/shaders/triangle.vert -V --vn triangle_vert -o src/shaders/triangle.vert.h
rem glslangValidator src/shaders/triangle.frag -V --vn triangle_frag -o src/shaders/triangle.frag.h
rem glslangValidator src/shaders/skybox.vert -V --vn skybox_vert -o src/shaders/skybox.vert.h
rem glslangValidator src/shaders/skybox.frag -V --vn skybox_frag -o src/shaders/skybox.frag.h
rem glslangValidator src/shaders/particle.vert -V --vn particle_vert -o src/shaders/particle.vert.h
rem glslangValidator src/shaders/particle.frag -V --vn particle_frag -o src/shaders/particle.frag.h
rem glslangValidator src/shaders/portal.vert -V --vn portal_vert -o src/shaders/portal.vert.h
rem glslangValidator src/shaders/portal.frag -V --vn portal_frag -o src/shaders/portal.frag.h
rem glslangValidator src/shaders/voxel.vert -V --vn voxel_vert -o src/shaders/voxel.vert.h

set COMPILE_FLAGS=-fno-unwind-tables -fno-rtti -fno-asynchronous-unwind-tables -fno-exceptions -Wall -nostdlib -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe -Iinclude
set WASM_COMPILE_FLAGS=--target=wasm32 -msimd128
set LINK_FLAGS=-Xlinker -stack:0x100000,0x100000
set WASM_LINK_FLAGS=-Wl,--no-entry,--export-dynamic,--allow-undefined

if not exist build mkdir build

rem clang src/assets/asset_generator.c -Ofast -o src/assets/asset_generator.exe %COMPILE_FLAGS% %LINK_FLAGS% -Xlinker -subsystem:console

clang src/dwrite.cpp src/triangle.c -Ofast -o build/triangle_speed.exe %COMPILE_FLAGS% %LINK_FLAGS% -Xlinker -subsystem:windows
clang src/dwrite.cpp src/triangle.c -Oz -o build/triangle_size.exe %COMPILE_FLAGS% %LINK_FLAGS% -Xlinker -subsystem:windows
clang src/dwrite.cpp src/triangle.c -g -o build/triangle_debug.exe %COMPILE_FLAGS% %LINK_FLAGS% -Xlinker -subsystem:windows

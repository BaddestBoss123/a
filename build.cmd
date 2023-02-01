@echo off
setlocal enabledelayedexpansion

glslangValidator src/shaders/triangle.vert -V --vn triangle_vert -o src/shaders/triangle.vert.h
glslangValidator src/shaders/triangle.frag -V --vn triangle_frag -o src/shaders/triangle.frag.h
glslangValidator src/shaders/skybox.vert -V --vn skybox_vert -o src/shaders/skybox.vert.h
glslangValidator src/shaders/skybox.frag -V --vn skybox_frag -o src/shaders/skybox.frag.h
glslangValidator src/shaders/voxel.vert -V --vn voxel_vert -o src/shaders/voxel.vert.h

set COMPILE_FLAGS=-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -Wall -nostdlib -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe -I%VULKAN_SDK%/Include
set WASM_COMPILE_FLAGS=--target=wasm32 -msimd128
set LINK_FLAGS=-Xlinker -stack:0x100000,0x100000 -Xlinker -subsystem:windows
set WASM_LINK_FLAGS=-Wl,--no-entry,--export-dynamic,--allow-undefined

if not exist build mkdir build

clang src/triangle.c -Ofast -o build/triangle_speed.exe %COMPILE_FLAGS% %LINK_FLAGS%
clang src/triangle.c -Oz -o build/triangle_size.exe %COMPILE_FLAGS% %LINK_FLAGS%
clang src/triangle.c -g -o build/triangle_debug.exe %COMPILE_FLAGS% %LINK_FLAGS%

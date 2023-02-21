@echo off
setlocal enabledelayedexpansion

rem pushd src
rem pushd shaders

rem glslangValidator --target-env vulkan1.0 blit.vert -V -o blit.vert.spv
rem glslangValidator --target-env vulkan1.0 blit.frag -V -o blit.frag.spv
rem glslangValidator --target-env vulkan1.0 triangle.vert -V -o triangle.vert.spv
rem glslangValidator --target-env vulkan1.0 triangle.frag -V -o triangle.frag.spv
rem glslangValidator --target-env vulkan1.0 skybox.vert -V -o skybox.vert.spv
rem glslangValidator --target-env vulkan1.0 skybox.frag -V -o skybox.frag.spv
rem glslangValidator --target-env vulkan1.0 particle.vert -V -o particle.vert.spv
rem glslangValidator --target-env vulkan1.0 particle.frag -V -o particle.frag.spv
rem glslangValidator --target-env vulkan1.0 portal.vert -V -o portal.vert.spv
rem glslangValidator --target-env vulkan1.0 portal.frag -V -o portal.frag.spv
rem glslangValidator --target-env vulkan1.0 voxel.vert -V -o voxel.vert.spv

rem spirv-opt blit.vert.spv -Os -o blit.vert.spv
rem spirv-opt blit.frag.spv -Os -o blit.frag.spv
rem spirv-opt triangle.vert.spv -Os -o triangle.vert.spv
rem spirv-opt triangle.frag.spv -Os -o triangle.frag.spv
rem spirv-opt skybox.vert.spv -Os -o skybox.vert.spv
rem spirv-opt skybox.frag.spv -Os -o skybox.frag.spv
rem spirv-opt particle.vert.spv -Os -o particle.vert.spv
rem spirv-opt particle.frag.spv -Os -o particle.frag.spv
rem spirv-opt portal.vert.spv -Os -o portal.vert.spv
rem spirv-opt portal.frag.spv -Os -o portal.frag.spv
rem spirv-opt voxel.vert.spv -Os -o voxel.frag.spv

rem spirv-dis blit.vert.spv -o blit.vert.spvasm
rem spirv-dis blit.frag.spv -o blit.frag.spvasm
rem spirv-dis triangle.vert.spv -o triangle.vert.spvasm
rem spirv-dis triangle.frag.spv -o triangle.frag.spvasm
rem spirv-dis skybox.vert.spv -o skybox.vert.spvasm
rem spirv-dis skybox.frag.spv -o skybox.frag.spvasm
rem spirv-dis particle.vert.spv -o particle.vert.spvasm
rem spirv-dis particle.frag.spv -o particle.frag.spvasm
rem spirv-dis portal.vert.spv -o portal.vert.spvasm
rem spirv-dis portal.frag.spv -o portal.frag.spvasm
rem spirv-dis voxel.vert.spv -o voxel.frag.spvasm

rem popd
rem popd

rem spirv-as src/shaders.spvasm --target-env vulkan1.0 -o shaders.spv
rem spirv-val shaders.spv

set COMPILE_FLAGS=-fno-unwind-tables -fno-rtti -fno-asynchronous-unwind-tables -fno-exceptions -nostdlib -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe -Iinclude -Wall
set LINK_FLAGS=-Xlinker -stack:0x100000,0x100000 -Xlinker -subsystem:windows
set WASM_COMPILE_FLAGS=--target=wasm32 -msimd128
set WASM_LINK_FLAGS=-Wl,--no-entry,--export-dynamic,--allow-undefined

if not exist build mkdir build

rem clang src/asset_generator.c -Ofast -o build/asset_generator.exe %COMPILE_FLAGS% %LINK_FLAGS%
rem pushd build
rem asset_generator.exe
rem popd

rem clang src/dwrite.cpp src/triangle.c -Ofast -o build/triangle_speed.exe %COMPILE_FLAGS% %LINK_FLAGS%
rem clang src/dwrite.cpp src/triangle.c -Oz -o build/triangle_size.exe %COMPILE_FLAGS% %LINK_FLAGS%
clang src/dwrite.cpp src/triangle.c -g -o build/triangle_debug.exe %COMPILE_FLAGS% %LINK_FLAGS%

@echo off
setlocal enabledelayedexpansion

set INCLUDE_PATHS=-IInclude

set COMPILE_FLAGS=-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -Wall -nostdlib -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe
set WASM_COMPILE_FLAGS=--target=wasm32 -msimd128
set LINK_FLAGS=-Xlinker -stack:0x100000,0x100000 -Xlinker -subsystem:windows
set WASM_LINK_FLAGS=-Wl,--no-entry,--export-dynamic,--allow-undefined

if not exist build mkdir build

clang src/triangle.c %INCLUDE_PATHS% -Ofast -o build/triangle_speed.exe %COMPILE_FLAGS% %LINK_FLAGS%
clang src/triangle.c %INCLUDE_PATHS% -Oz -o build/triangle_size.exe %COMPILE_FLAGS% %LINK_FLAGS%
clang src/triangle.c %INCLUDE_PATHS% -g -o build/triangle_debug.exe %COMPILE_FLAGS% %LINK_FLAGS%

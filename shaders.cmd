@echo off
setlocal enabledelayedexpansion

rem todo: loop over the files

pushd bin
glslangValidator ../shaders/triangle.vert -V --vn triangle_vert -o ../src/shaders/triangle.vert.h
glslangValidator ../shaders/triangle.frag -V --vn triangle_frag -o ../src/shaders/triangle.frag.h
popd
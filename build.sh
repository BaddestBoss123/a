#!/bin/bash

clang server.c -Ofast -o server -fno-unwind-tables -fno-rtti -fno-asynchronous-unwind-tables -fno-exceptions -mcmodel=small -mavx2 -mfma -fenable-matrix -mno-stack-arg-probe -Wall
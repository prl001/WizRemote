#!/bin/sh

env PATH=/Users/eric/Projects/DP-S1/GPL_DA/toolchain/arm-elf/bin/:$PATH gcc -O2 -I/Users/eric/Projects/DP-S1/GPL_DA/build_arm/uClibc-0.9.26/include -L/Users/eric/Projects/DP-S1/GPL_DA/build_arm/uClibc-0.9.26/lib -Wl,-elf2flt -o wizremote ezxml.c wizremote.c

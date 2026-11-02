#!/bin/bash
# Host-side unit test of the DualSense -> DS4 translation (runs on a PC, not on the PS4).
# Needs: clang. Uses tiny stand-ins for the PS4 headers.
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d)
mkdir -p "$T/orbis/_types"
cp tests/pad_compat.h "$T/orbis/_types/pad.h"
cat > "$T/orbis/libkernel.h" <<'H'
#pragma once
#include <stdint.h>
static inline uint64_t sceKernelGetProcessTime(void){ return 12345; }
H
clang -std=gnu11 -Wall -I"$T" -Iinclude tests/ds5_test.c src/translator.c -o "$T/ds5_test"
"$T/ds5_test"

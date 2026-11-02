#!/bin/bash
# Build script run INSIDE the openorbisofficial/toolchain container (see .github/workflows/build.yml)
set -x
TC=/lib/OpenOrbisSDK
SDK=/workspace/sdk/GoldHEN_Plugins_SDK
cd /workspace

# --- 1. This image has no llvm-ar: provide one on top of GNU ar
if ! command -v llvm-ar >/dev/null 2>&1; then
  printf '#!/bin/sh\nif [ "$1" = "--format=bsd" ]; then shift; fi\nexec ar "$@"\n' > /usr/local/bin/llvm-ar
  chmod +x /usr/local/bin/llvm-ar
fi

# --- 2. Diagnostics: what does this toolchain provide?
ls "$TC/bin/linux"
ls "$TC/include/orbis" | tr '\n' ' '; echo
ls "$TC/include/orbis/_types" | tr '\n' ' '; echo
for h in Pad.h Usbd.h UserService.h libkernel.h; do
  [ -f "$TC/include/orbis/$h" ] && echo "have orbis/$h" || echo "MISSING orbis/$h"
done

# --- 3. Header compatibility: toolchains name the pad fields differently
if ! grep -q "quat" "$TC/include/orbis/_types/pad.h"; then
  sed -i 's/\bquat\b/orientation/g; s/\bacell\b/acceleration/g; s/->vel\./->angularVelocity./g' src/*.c
fi

# --- 4. GoldHEN SDK static library
make -C "$SDK" OO_PS4_TOOLCHAIN="$TC" || exit 1

# --- 5. Plugin: -k keeps going so ALL compile errors are listed in one run
make -k dirs bin/ds5_controller.elf OO_PS4_TOOLCHAIN="$TC" GOLDHEN_SDK="$SDK" 2>&1 | tee /workspace/build.log
[ -f bin/ds5_controller.elf ] || { echo "=== ELF NOT BUILT ==="; grep -c "error:" /workspace/build.log; exit 1; }

# --- 6. ELF -> PRX (tool name depends on the toolchain version)
if [ -x "$TC/bin/linux/create-fself" ]; then
  "$TC/bin/linux/create-fself" -in=bin/ds5_controller.elf -out=bin/ds5_controller.oelf --lib=bin/ds5_controller.prx --paid 0x3800000000000011
else
  "$TC/bin/linux/create-lib" -h 2>&1 | head -30
  "$TC/bin/linux/create-lib" -in=bin/ds5_controller.elf -out=bin/ds5_controller.oelf --lib=bin/ds5_controller.prx --paid 0x3800000000000011 || \
  "$TC/bin/linux/create-lib" -in=bin/ds5_controller.elf -out=bin/ds5_controller.prx --paid 0x3800000000000011
fi
ls -la bin

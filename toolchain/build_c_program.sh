#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <program.c>"
  exit 1
fi

PREFIX=${PREFIX:-$HOME/loongarch32-toolchain/install}
CC=${CC:-$PREFIX/bin/loongarch32-unknown-elf-gcc}
OBJCOPY=${OBJCOPY:-$PREFIX/bin/loongarch32-unknown-elf-objcopy}
OBJDUMP=${OBJDUMP:-$PREFIX/bin/loongarch32-unknown-elf-objdump}

SRC_C="$1"
BASENAME=$(basename "$SRC_C" .c)

OUTDIR=build_runtime
mkdir -p "${OUTDIR}"

# Allow overriding C flags from env while keeping the baseline flags as default.
CFLAGS=${CFLAGS:-"-ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 -g"}
INCLUDES="-Iruntime"

echo "[1/6] compile start.S"
"${CC}" ${CFLAGS} ${INCLUDES} -c runtime/start.S -o "${OUTDIR}/start.o"

echo "[2/6] compile trap.c"
"${CC}" ${CFLAGS} ${INCLUDES} -c runtime/trap.c -o "${OUTDIR}/trap.o"

echo "[3/6] compile ${SRC_C}"
"${CC}" ${CFLAGS} ${INCLUDES} -c "${SRC_C}" -o "${OUTDIR}/${BASENAME}.o"

echo "[4/6] link ELF"
"${CC}" \
  ${CFLAGS} \
  -T runtime/linker.ld \
  "${OUTDIR}/start.o" \
  "${OUTDIR}/trap.o" \
  "${OUTDIR}/${BASENAME}.o" \
  -o "${OUTDIR}/${BASENAME}.elf"

echo "[5/6] dump disassembly"
"${OBJDUMP}" -d "${OUTDIR}/${BASENAME}.elf" > "${OUTDIR}/${BASENAME}.dump"

echo "[6/6] objcopy to binary"
"${OBJCOPY}" -O binary "${OUTDIR}/${BASENAME}.elf" "${OUTDIR}/${BASENAME}.bin"

echo "[done] generated:"
ls -lh \
  "${OUTDIR}/${BASENAME}.elf" \
  "${OUTDIR}/${BASENAME}.bin" \
  "${OUTDIR}/${BASENAME}.dump"

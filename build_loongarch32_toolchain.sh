#!/usr/bin/env bash
set -euo pipefail

# =========================
# Config
# =========================
ROOT_DIR="${HOME}/loongarch32-toolchain"
SRC_DIR="${ROOT_DIR}/src"
BUILD_DIR="${ROOT_DIR}/build"
INSTALL_DIR="${ROOT_DIR}/install"

BINUTILS_REPO="https://github.com/cloudspurs/binutils-gdb.git"
BINUTILS_BRANCH="la32"

GCC_REPO="https://github.com/cloudspurs/gcc.git"
GCC_BRANCH="la32"

TARGET="loongarch32-unknown-elf"
JOBS="${JOBS:-$(nproc)}"

# =========================
# Helpers
# =========================
log() {
  echo
  echo "============================================================"
  echo "$1"
  echo "============================================================"
}

ensure_dir() {
  mkdir -p "$1"
}

# =========================
# 1. Install dependencies
# =========================
log "[1/8] Installing build dependencies"

echo 123 | sudo -S apt update
echo 123 | sudo -S apt install -y \
  git build-essential gperf bison flex texinfo help2man \
  libncurses-dev libtool-bin automake autoconf gawk \
  libexpat-dev libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev \
  python3 unzip bzip2 xz-utils wget curl

# =========================
# 2. Prepare directories
# =========================
log "[2/8] Preparing directories"

ensure_dir "${SRC_DIR}"
ensure_dir "${BUILD_DIR}"
ensure_dir "${INSTALL_DIR}"

# =========================
# 3. Fetch binutils
# =========================
log "[3/8] Cloning binutils-gdb (${BINUTILS_BRANCH})"

cd "${SRC_DIR}"
if [ ! -d "${SRC_DIR}/binutils-gdb" ]; then
  git clone --depth=1 --branch "${BINUTILS_BRANCH}" "${BINUTILS_REPO}" binutils-gdb
else
  echo "[INFO] binutils-gdb already exists, skipping clone"
fi

# =========================
# 4. Build binutils
# =========================
log "[4/8] Building binutils for ${TARGET}"

rm -rf "${BUILD_DIR}/binutils"
ensure_dir "${BUILD_DIR}/binutils"
cd "${BUILD_DIR}/binutils"

"${SRC_DIR}/binutils-gdb/configure" \
  --target="${TARGET}" \
  --prefix="${INSTALL_DIR}" \
  --disable-nls \
  --disable-werror

make -j"${JOBS}"
make install

# =========================
# 5. Fetch GCC
# =========================
log "[5/8] Cloning GCC (${GCC_BRANCH})"

cd "${SRC_DIR}"
if [ ! -d "${SRC_DIR}/gcc" ]; then
  git clone --depth=1 --branch "${GCC_BRANCH}" "${GCC_REPO}" gcc
else
  echo "[INFO] gcc already exists, skipping clone"
fi

# =========================
# 6. Patch GCC for bare-metal OPTION_GLIBC issue
# =========================
log "[6/8] Patching GCC (OPTION_GLIBC fallback for bare-metal)"

LINUX_H="${SRC_DIR}/gcc/gcc/config/loongarch/linux.h"

if ! grep -q 'OPTION_GLIBC 0' "${LINUX_H}"; then
  cp "${LINUX_H}" "${LINUX_H}.bak"

  python3 - <<'PY'
from pathlib import Path

path = Path.home() / "loongarch32-toolchain/src/gcc/gcc/config/loongarch/linux.h"
text = path.read_text()

patch = """#ifndef OPTION_GLIBC
#define OPTION_GLIBC 0
#endif

"""

if "HAVE_IFUNC_FOR_LIBATOMIC_16B" in text and "OPTION_GLIBC 0" not in text:
    marker = "#define HAVE_IFUNC_FOR_LIBATOMIC_16B"
    text = text.replace(marker, patch + marker, 1)

path.write_text(text)
PY

  echo "[INFO] Patched ${LINUX_H}"
else
  echo "[INFO] GCC patch already applied"
fi

# =========================
# 7. Build stage1 GCC
# =========================
log "[7/8] Building GCC (stage1 C compiler only)"

rm -rf "${BUILD_DIR}/gcc-stage1"
ensure_dir "${BUILD_DIR}/gcc-stage1"
cd "${BUILD_DIR}/gcc-stage1"

export PATH="${INSTALL_DIR}/bin:${PATH}"

"${SRC_DIR}/gcc/configure" \
  --target="${TARGET}" \
  --prefix="${INSTALL_DIR}" \
  --disable-nls \
  --enable-languages=c \
  --without-headers \
  --disable-shared \
  --disable-threads \
  --disable-libssp \
  --disable-libquadmath \
  --disable-libgomp \
  --disable-libatomic \
  --disable-libstdcxx \
  --disable-libsanitizer

make -j"${JOBS}" all-gcc
make install-gcc

# =========================
# 8. Verify toolchain
# =========================
log "[8/8] Verifying installed toolchain"

"${INSTALL_DIR}/bin/${TARGET}-as" --version | head -n 1
"${INSTALL_DIR}/bin/${TARGET}-ld" --version | head -n 1
"${INSTALL_DIR}/bin/${TARGET}-objdump" --version | head -n 1
"${INSTALL_DIR}/bin/${TARGET}-gcc" --version | head -n 1

echo
echo "[SUCCESS] LoongArch32 bare-metal toolchain installed."
echo
echo "Install path:"
echo "  ${INSTALL_DIR}"
echo
echo "Add this to your shell config (~/.bashrc or ~/.zshrc):"
echo "  export PATH=\"${INSTALL_DIR}/bin:\$PATH\""
echo
echo "Then reload shell:"
echo "  source ~/.bashrc"
echo
echo "Test command:"
echo "  ${TARGET}-gcc --version"
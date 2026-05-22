#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "${SCRIPT_DIR}/.." && pwd)

cd "$REPO_ROOT"

MANIFEST=${MANIFEST:-tests/program/c_test_copy_manifest.txt} \
SIM=${SIM:-build/mycpu_sim} \
BUILD_SCRIPT=${BUILD_SCRIPT:-toolchain/build_c_program.sh} \
exec toolchain/run_c_tests.sh

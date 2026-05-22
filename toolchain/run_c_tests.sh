#!/usr/bin/env bash
set -euo pipefail

MANIFEST=${MANIFEST:-tests/program/c_test_manifest.txt}
SIM=${SIM:-build/mycpu_sim}
BUILD_SCRIPT=${BUILD_SCRIPT:-toolchain/build_c_program.sh}

format_ms() {
  awk -v ns="$1" 'BEGIN { printf "%.3f", ns / 1000000 }'
}

format_us() {
  awk -v us="$1" 'BEGIN { printf "%.3f", us / 1000 }'
}

extract_exit_code() {
  printf '%s\n' "$1" | sed -n 's/.*Program halted with exit code \([0-9][0-9]*\).*/\1/p' | tail -n 1
}

extract_host_time_us() {
  printf '%s\n' "$1" |
    sed -n 's/.*host time spent = \([0-9][0-9,]*\) us.*/\1/p' |
    tail -n 1 |
    tr -d ','
}

extract_guest_instructions() {
  printf '%s\n' "$1" |
    sed -n 's/.*total guest instructions = \([0-9][0-9,]*\).*/\1/p' |
    tail -n 1 |
    tr -d ','
}

extract_steps() {
  local steps
  steps=$(printf '%s\n' "$1" | sed -n 's/.*halted after \([0-9][0-9]*\) step(s), exit_code=.*/\1/p' | tail -n 1)
  if [ -n "${steps:-}" ]; then
    printf '%s' "$steps"
    return
  fi

  steps=$(printf '%s\n' "$1" | sed -n 's/.*reached max_steps without halt, steps=\([0-9][0-9]*\).*/\1/p' | tail -n 1)
  printf '%s' "${steps:-unknown}"
}

if [ ! -f "$MANIFEST" ]; then
  echo "[ERROR] manifest not found: $MANIFEST"
  exit 1
fi

if [ ! -x "$BUILD_SCRIPT" ]; then
  echo "[ERROR] build script not executable: $BUILD_SCRIPT"
  exit 1
fi

if [ ! -x "$SIM" ]; then
  echo "[ERROR] simulator not found or not executable: $SIM"
  exit 1
fi

passed=0
failed=0
total=0
total_scored_us=0
total_start_ns=$(date +%s%N)

echo "=== C Program Test Runner ==="
echo "Running C Tests [sim *${SIM}*]"
echo

while read -r name src expected; do
  if [ -z "${name:-}" ]; then
    continue
  fi
  if [[ "$name" =~ ^# ]]; then
    continue
  fi

  total=$((total + 1))

  basename=$(basename "$src" .c)
  file_label=$(basename "$src")
  bin_path="build_runtime/${basename}.bin"
  case_start_ns=$(date +%s%N)

  if ! "$BUILD_SCRIPT" "$src" >/tmp/${basename}_build.log 2>&1; then
    case_end_ns=$(date +%s%N)
    case_ms=$(format_ms $((case_end_ns - case_start_ns)))
    echo "[${name}] ${file_label}: Failed."
    echo "time: ${case_ms} ms [build failed]"
    echo "reason: build failed, see /tmp/${basename}_build.log"
    echo
    failed=$((failed + 1))
    continue
  fi

  run_start_ns=$(date +%s%N)
  set +e
  output=$("$SIM" "$bin_path" 2>&1)
  rc=$?
  set -e
  run_end_ns=$(date +%s%N)

  run_ns=$((run_end_ns - run_start_ns))
  actual=$(extract_exit_code "$output")
  host_time_us=$(extract_host_time_us "$output")
  guest_instructions=$(extract_guest_instructions "$output")
  steps=$(extract_steps "$output")

  if [ -n "${guest_instructions:-}" ]; then
    steps="$guest_instructions"
  fi

  if [ -n "${host_time_us:-}" ]; then
    total_scored_us=$((total_scored_us + host_time_us))
    run_ms=$(format_us "$host_time_us")
  else
    run_ms=$(format_ms "$run_ns")
  fi

  if [ -z "${actual:-}" ]; then
    echo "[${name}] ${file_label}: Failed."
    echo "time: ${run_ms} ms [${steps} steps]"
    echo "reason: no exit code found (sim rc=${rc})"
    echo "$output"
    echo
    failed=$((failed + 1))
    continue
  fi

  if [ "$actual" = "$expected" ]; then
    echo "[${name}] ${file_label}: Passed."
    echo "time: ${run_ms} ms [${steps} steps]"
    echo
    passed=$((passed + 1))
  else
    echo "[${name}] ${file_label}: Failed."
    echo "time: ${run_ms} ms [${steps} steps]"
    echo "reason: expected=${expected} got=${actual} (sim rc=${rc})"
    echo "$output"
    echo
    failed=$((failed + 1))
  fi

done < "$MANIFEST"

total_end_ns=$(date +%s%N)
total_wall_ns=$((total_end_ns - total_start_ns))

if [ "$failed" -eq 0 ]; then
  echo "C Tests PASS"
else
  echo "C Tests FAIL"
fi
echo "${passed} / ${total} passed"
echo "Summary: ${passed} passed, ${failed} failed, ${total} total"
echo "Scored time: $(format_us "$total_scored_us") ms"
echo "Total time: $(format_ms "$total_wall_ns") ms"

if [ "$failed" -eq 0 ]; then
  exit 0
else
  exit 1
fi

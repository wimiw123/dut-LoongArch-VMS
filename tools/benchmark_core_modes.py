#!/usr/bin/env python3

from __future__ import annotations

import csv
import pathlib
import re
import statistics
import subprocess
from dataclasses import dataclass, asdict


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
MANIFEST_PATH = REPO_ROOT / "tests/program/c_test_copy_manifest.txt"
BUILD_SCRIPT = REPO_ROOT / "toolchain/build_c_program.sh"
BASELINE_SIM = REPO_ROOT / "toolchain/mycpu_sim_baseline.sh"
ADVANCED_SIM = REPO_ROOT / "toolchain/mycpu_sim_advanced.sh"
OUTPUT_DIR = REPO_ROOT / "docs/microarch_benchmark"
CSV_PATH = OUTPUT_DIR / "core_mode_compare.csv"
REPORT_PATH = OUTPUT_DIR / "core_mode_compare_report.md"


def parse_manifest(path: pathlib.Path) -> list[tuple[str, pathlib.Path, int]]:
    cases: list[tuple[str, pathlib.Path, int]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        name, source, expected = stripped.split()
        cases.append((name, (path.parent.parent.parent / source).resolve(), int(expected)))
    return cases


def run_checked(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=True,
    )


def run_capture(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def extract(pattern: str, text: str, cast=float, default=None):
    match = re.search(pattern, text, re.M)
    if match:
        return cast(match.group(1))
    return default


@dataclass
class SimMetrics:
    host_time_us: int
    guest_instructions: int
    guest_cycles: int
    guest_ipc: float
    guest_cpi: float
    serialized_cycles: int
    overlap_gain: float
    branch_instructions: int
    dynamic_branch_predictions: int
    branch_prediction_hits: int
    branch_prediction_misses: int
    branch_prediction_accuracy_pct: float
    pipeline_flushes: int
    speculative_squashes: int
    register_renames: int
    out_of_order_completions: int
    load_store_forwardings: int
    average_rob_occupancy: float
    peak_rob_occupancy: int
    average_rs_occupancy: float
    peak_rs_occupancy: int
    average_inflight_instructions: float
    peak_inflight_instructions: int
    rob_full_stalls: int
    rs_full_stalls: int
    decode_stalls: int
    issue_stalls: int
    load_store_order_stalls: int
    exit_code: int
    core_mode: str


def parse_sim_metrics(text: str) -> SimMetrics:
    exit_code = extract(r"Program halted with exit code (\d+)", text, int, default=-1)
    return SimMetrics(
        host_time_us=extract(r"host time spent = (\d+) us", text, int, 0),
        guest_instructions=extract(r"total guest instructions = (\d+)", text, int, 0),
        guest_cycles=extract(r"total guest cycles = (\d+)", text, int, 0),
        guest_ipc=extract(r"guest IPC = ([0-9.]+)", text, float, 0.0),
        guest_cpi=extract(r"guest CPI = ([0-9.]+)", text, float, 0.0),
        serialized_cycles=extract(r"serialized no-overlap cycles = (\d+)", text, int, 0),
        overlap_gain=extract(r"pipeline overlap gain = ([0-9.]+)", text, float, 0.0),
        branch_instructions=extract(r"branch instructions = (\d+)", text, int, 0),
        dynamic_branch_predictions=extract(r"dynamic branch predictions = (\d+)", text, int, 0),
        branch_prediction_hits=extract(r"branch prediction hits = (\d+)", text, int, 0),
        branch_prediction_misses=extract(r"branch prediction misses = (\d+)", text, int, 0),
        branch_prediction_accuracy_pct=extract(r"branch prediction accuracy = ([0-9.]+) %", text, float, 0.0),
        pipeline_flushes=extract(r"pipeline flushes = (\d+)", text, int, 0),
        speculative_squashes=extract(r"speculative squashes = (\d+)", text, int, 0),
        register_renames=extract(r"register renames = (\d+)", text, int, 0),
        out_of_order_completions=extract(r"out-of-order completions = (\d+)", text, int, 0),
        load_store_forwardings=extract(r"load/store forwardings = (\d+)", text, int, 0),
        average_rob_occupancy=extract(r"average ROB occupancy = ([0-9.]+)", text, float, 0.0),
        peak_rob_occupancy=extract(r"peak ROB occupancy = (\d+)", text, int, 0),
        average_rs_occupancy=extract(r"average RS occupancy = ([0-9.]+)", text, float, 0.0),
        peak_rs_occupancy=extract(r"peak RS occupancy = (\d+)", text, int, 0),
        average_inflight_instructions=extract(r"average inflight instructions = ([0-9.]+)", text, float, 0.0),
        peak_inflight_instructions=extract(r"peak inflight instructions = (\d+)", text, int, 0),
        rob_full_stalls=extract(r"ROB full stalls = (\d+)", text, int, 0),
        rs_full_stalls=extract(r"RS full stalls = (\d+)", text, int, 0),
        decode_stalls=extract(r"decode stalls = (\d+)", text, int, 0),
        issue_stalls=extract(r"issue stalls = (\d+)", text, int, 0),
        load_store_order_stalls=extract(r"load/store order stalls = (\d+)", text, int, 0),
        exit_code=exit_code,
        core_mode=extract(r"core mode = (\w+)", text, str, "unknown"),
    )


@dataclass
class CaseRow:
    name: str
    source: str
    expected_exit: int
    baseline_exit: int
    advanced_exit: int
    baseline_us: int
    advanced_us: int
    slowdown_x: float
    guest_instructions: int
    baseline_cycles: int
    advanced_cycles: int
    advanced_ipc: float
    advanced_cpi: float
    advanced_overlap_gain: float
    advanced_branch_accuracy_pct: float
    advanced_branch_predictions: int
    advanced_flushes: int
    advanced_speculative_squashes: int
    advanced_rob_avg: float
    advanced_rob_peak: int
    advanced_inflight_avg: float
    advanced_inflight_peak: int
    advanced_ooo_completions: int
    advanced_renames: int
    advanced_issue_stalls: int
    advanced_load_store_order_stalls: int


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    cases = parse_manifest(MANIFEST_PATH)
    rows: list[CaseRow] = []

    for name, source, expected in cases:
        run_checked([str(BUILD_SCRIPT), str(source)])
        bin_path = REPO_ROOT / "build_runtime" / f"{source.stem}.bin"

        baseline_run = run_capture([str(BASELINE_SIM), str(bin_path)])
        advanced_run = run_capture([str(ADVANCED_SIM), str(bin_path)])

        baseline = parse_sim_metrics(baseline_run.stdout + baseline_run.stderr)
        advanced = parse_sim_metrics(advanced_run.stdout + advanced_run.stderr)

        if baseline.exit_code != expected:
            raise SystemExit(f"Baseline mode failed for {name}: expected {expected}, got {baseline.exit_code}")
        if advanced.exit_code != expected:
            raise SystemExit(f"Advanced mode failed for {name}: expected {expected}, got {advanced.exit_code}")

        slowdown = float("inf") if baseline.host_time_us == 0 else advanced.host_time_us / baseline.host_time_us
        rows.append(
            CaseRow(
                name=name,
                source=source.name,
                expected_exit=expected,
                baseline_exit=baseline.exit_code,
                advanced_exit=advanced.exit_code,
                baseline_us=baseline.host_time_us,
                advanced_us=advanced.host_time_us,
                slowdown_x=slowdown,
                guest_instructions=advanced.guest_instructions,
                baseline_cycles=baseline.guest_cycles,
                advanced_cycles=advanced.guest_cycles,
                advanced_ipc=advanced.guest_ipc,
                advanced_cpi=advanced.guest_cpi,
                advanced_overlap_gain=advanced.overlap_gain,
                advanced_branch_accuracy_pct=advanced.branch_prediction_accuracy_pct,
                advanced_branch_predictions=advanced.dynamic_branch_predictions,
                advanced_flushes=advanced.pipeline_flushes,
                advanced_speculative_squashes=advanced.speculative_squashes,
                advanced_rob_avg=advanced.average_rob_occupancy,
                advanced_rob_peak=advanced.peak_rob_occupancy,
                advanced_inflight_avg=advanced.average_inflight_instructions,
                advanced_inflight_peak=advanced.peak_inflight_instructions,
                advanced_ooo_completions=advanced.out_of_order_completions,
                advanced_renames=advanced.register_renames,
                advanced_issue_stalls=advanced.issue_stalls,
                advanced_load_store_order_stalls=advanced.load_store_order_stalls,
            )
        )

    with CSV_PATH.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(asdict(rows[0]).keys()))
        writer.writeheader()
        for row in rows:
            writer.writerow(asdict(row))

    baseline_total_us = sum(row.baseline_us for row in rows)
    advanced_total_us = sum(row.advanced_us for row in rows)
    baseline_total_cycles = sum(row.baseline_cycles for row in rows)
    advanced_total_cycles = sum(row.advanced_cycles for row in rows)
    total_guest_instructions = sum(row.guest_instructions for row in rows)
    total_predictions = sum(row.advanced_branch_predictions for row in rows)
    total_flushes = sum(row.advanced_flushes for row in rows)
    total_squashes = sum(row.advanced_speculative_squashes for row in rows)
    total_ooo = sum(row.advanced_ooo_completions for row in rows)
    total_renames = sum(row.advanced_renames for row in rows)
    weighted_rob_avg = (
        sum(row.advanced_rob_avg * row.advanced_cycles for row in rows) / advanced_total_cycles
        if advanced_total_cycles
        else 0.0
    )
    weighted_inflight_avg = (
        sum(row.advanced_inflight_avg * row.advanced_cycles for row in rows) / advanced_total_cycles
        if advanced_total_cycles
        else 0.0
    )
    weighted_ipc = total_guest_instructions / advanced_total_cycles if advanced_total_cycles else 0.0
    weighted_cpi = advanced_total_cycles / total_guest_instructions if total_guest_instructions else 0.0
    weighted_branch_accuracy = (
        sum(row.advanced_branch_accuracy_pct * row.advanced_branch_predictions for row in rows) / total_predictions
        if total_predictions
        else 0.0
    )
    suite_slowdown = advanced_total_us / baseline_total_us if baseline_total_us else 0.0
    median_slowdown = statistics.median(row.slowdown_x for row in rows)

    top_slowest = sorted(rows, key=lambda row: row.slowdown_x, reverse=True)[:8]

    summary_table = "\n".join(
        [
            "| 指标 | baseline | advanced |",
            "|---|---:|---:|",
            f"| 35 程序总宿主时间(us) | {baseline_total_us} | {advanced_total_us} |",
            f"| 总 guest 指令数 | {total_guest_instructions} | {total_guest_instructions} |",
            f"| 总 guest cycles | {baseline_total_cycles} | {advanced_total_cycles} |",
            f"| suite IPC | {1.0:.4f} | {weighted_ipc:.4f} |",
            f"| suite CPI | {1.0:.4f} | {weighted_cpi:.4f} |",
            f"| suite slowdown | 1.0000x | {suite_slowdown:.4f}x |",
        ]
    )

    slow_rows = "\n".join(
        f"| {row.name} | {row.baseline_us} | {row.advanced_us} | {row.slowdown_x:.4f} | "
        f"{row.advanced_ipc:.4f} | {row.advanced_branch_accuracy_pct:.2f} | {row.advanced_rob_avg:.2f} | "
        f"{row.advanced_ooo_completions} | {row.advanced_overlap_gain:.4f} |"
        for row in rows
    )

    report = f"""# LoongArch 核心模式对比报告

## 1. 测试口径

- 使用现有 35 个程序清单：`tests/program/c_test_copy_manifest.txt`
- 每个程序仍通过现有 `toolchain/build_c_program.sh` 交叉编译
- `baseline` 对应改造前的顺序执行模型
- `advanced` 对应五级流水线 + 动态分支预测 + Tomasulo + ROB 乱序执行模型
- 对比指标以模拟器输出的 `host time spent` 为主，额外补充微结构统计

## 2. 总结论

- 35 个程序在两种模式下都保持 `35 / 35` 正确通过
- 从宿主机运行时间看，`advanced` 总体是 `baseline` 的 `{suite_slowdown:.4f}x`
- 中位数慢化倍数是 `{median_slowdown:.4f}x`
- 但 `advanced` 同时给出了流水线和乱序执行的直接证据：
  - 加权 branch prediction accuracy = `{weighted_branch_accuracy:.2f}%`
  - 平均 ROB 占用 = `{weighted_rob_avg:.2f}`
  - 平均在飞指令数 = `{weighted_inflight_avg:.2f}`
  - 总乱序完成次数 = `{total_ooo}`
  - 总寄存器重命名次数 = `{total_renames}`
  - 总冲刷水线次数 = `{total_flushes}`
  - 总 speculative squashes = `{total_squashes}`

## 3. 套件汇总表

{summary_table}

## 4. 逐程序表

| 程序 | baseline_us | advanced_us | slowdown_x | advanced_IPC | bp_acc_% | avg_ROB | ooo_done | overlap_gain |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
{slow_rows}

## 5. 最慢的 8 个程序

| 程序 | baseline_us | advanced_us | slowdown_x |
|---|---:|---:|---:|
{"".join(f"| {row.name} | {row.baseline_us} | {row.advanced_us} | {row.slowdown_x:.4f} |\n" for row in top_slowest)}

## 6. 结论解释

- `advanced` 更慢的主要原因不是 guest 程序变长，而是宿主机需要额外模拟取指/译码/保留站/ROB/预测器/刷流水线这些状态机。
- 但从 `branch prediction accuracy`、`avg ROB occupancy`、`out-of-order completions`、`register renames` 和 `overlap gain` 可以直接看到，这颗核心已经从“单步顺序解释器”变成了“有投机、有窗口、有重命名、有顺序提交”的更先进 CPU 模型。
- 因此，本次改造达成的是“微结构能力升级”，而不是“宿主机解释器提速”。
"""

    REPORT_PATH.write_text(report, encoding="utf-8")
    print(f"Wrote CSV to {CSV_PATH}")
    print(f"Wrote report to {REPORT_PATH}")


if __name__ == "__main__":
    main()

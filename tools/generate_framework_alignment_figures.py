#!/usr/bin/env python3

from __future__ import annotations

import csv
import json
import math
import pathlib
import re
from dataclasses import dataclass, asdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


RESULTS_PATH = pathlib.Path("latest_aligned_compare_results.md")
OUTPUT_DIR = pathlib.Path("docs/framework_alignment_paper_assets")

ROW_RE = re.compile(
    r"^(\S+)\s+"
    r"(HIT GOOD TRAP|HIT BAD TRAP)\s+(\d+)\s+([0-9.]+)\s+(\d+)\s+"
    r"(HIT GOOD TRAP|HIT BAD TRAP)\s+(\d+)\s+([0-9.]+)\s+(\d+)\s+"
    r"(-?\d+)\s+(.+?)\s+(-?\d+)\s+(.+)$",
    re.M,
)


@dataclass
class ResultRow:
    name: str
    loong_status: str
    loong_us: int
    loong_ms: float
    loong_inst: int
    pa_status: str
    pa_us: int
    pa_ms: float
    pa_inst: int
    delta_us: int
    time_higher: str
    delta_inst: int
    inst_higher: str


def load_rows(path: pathlib.Path) -> list[ResultRow]:
    text = path.read_text(encoding="utf-8")
    rows: list[ResultRow] = []
    for match in ROW_RE.finditer(text):
        (
            name,
            loong_status,
            loong_us,
            loong_ms,
            loong_inst,
            pa_status,
            pa_us,
            pa_ms,
            pa_inst,
            delta_us,
            time_higher,
            delta_inst,
            inst_higher,
        ) = match.groups()
        rows.append(
            ResultRow(
                name=name,
                loong_status=loong_status,
                loong_us=int(loong_us),
                loong_ms=float(loong_ms),
                loong_inst=int(loong_inst),
                pa_status=pa_status,
                pa_us=int(pa_us),
                pa_ms=float(pa_ms),
                pa_inst=int(pa_inst),
                delta_us=int(delta_us),
                time_higher=time_higher,
                delta_inst=int(delta_inst),
                inst_higher=inst_higher,
            )
        )
    if len(rows) != 35:
        raise SystemExit(f"Expected 35 result rows, got {len(rows)}")
    return rows


def official_scored_times_ms(path: pathlib.Path) -> tuple[float, float]:
    text = path.read_text(encoding="utf-8")
    scores = re.findall(r"Scored time:\s*([0-9.]+)\s*ms", text)
    if len(scores) < 2:
        raise SystemExit("Could not parse official scored times from results file")
    return float(scores[0]), float(scores[1])


def corr(xs: list[float], ys: list[float]) -> float:
    n = len(xs)
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    num = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    den = math.sqrt(
        sum((x - mean_x) ** 2 for x in xs) * sum((y - mean_y) ** 2 for y in ys)
    )
    return 0.0 if den == 0.0 else num / den


def write_csv(rows: list[ResultRow], output_dir: pathlib.Path) -> None:
    output_path = output_dir / "aligned_compare_data.csv"
    with output_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(asdict(rows[0]).keys()))
        writer.writeheader()
        for row in rows:
            writer.writerow(asdict(row))


def write_summary(rows: list[ResultRow], output_dir: pathlib.Path, official_times: tuple[float, float]) -> None:
    loong_total_us = sum(row.loong_us for row in rows)
    pa_total_us = sum(row.pa_us for row in rows)
    loong_total_inst = sum(row.loong_inst for row in rows)
    pa_total_inst = sum(row.pa_inst for row in rows)
    rows_wo_mersenne = [row for row in rows if row.name != "mersenne"]
    summary = {
        "official_compare_loong_ms": official_times[0],
        "official_compare_pa_ms": official_times[1],
        "detailed_loong_total_us": loong_total_us,
        "detailed_pa_total_us": pa_total_us,
        "detailed_loong_total_inst": loong_total_inst,
        "detailed_pa_total_inst": pa_total_inst,
        "loong_faster_cases": sum(row.delta_us < 0 for row in rows),
        "pa_faster_cases": sum(row.delta_us > 0 for row in rows),
        "median_pa_over_loong_time_ratio": sorted(
            row.pa_us / row.loong_us for row in rows if row.loong_us > 0
        )[len(rows) // 2],
        "loong_ns_per_inst": loong_total_us * 1000.0 / loong_total_inst,
        "pa_ns_per_inst": pa_total_us * 1000.0 / pa_total_inst,
        "throughput_ratio_loong_over_pa": (loong_total_inst / loong_total_us)
        / (pa_total_inst / pa_total_us),
        "loong_time_instruction_corr": corr(
            [float(row.loong_inst) for row in rows],
            [float(row.loong_us) for row in rows],
        ),
        "pa_time_instruction_corr": corr(
            [float(row.pa_inst) for row in rows],
            [float(row.pa_us) for row in rows],
        ),
        "excluding_mersenne": {
            "loong_total_us": sum(row.loong_us for row in rows_wo_mersenne),
            "pa_total_us": sum(row.pa_us for row in rows_wo_mersenne),
            "loong_total_inst": sum(row.loong_inst for row in rows_wo_mersenne),
            "pa_total_inst": sum(row.pa_inst for row in rows_wo_mersenne),
        },
        "largest_time_gap_programs": [
            asdict(row) for row in sorted(rows, key=lambda item: abs(item.delta_us), reverse=True)[:10]
        ],
        "largest_instruction_gap_programs": [
            asdict(row) for row in sorted(rows, key=lambda item: abs(item.delta_inst), reverse=True)[:10]
        ],
    }
    (output_dir / "analysis_summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )


def configure_matplotlib() -> None:
    plt.style.use("seaborn-v0_8-whitegrid")
    plt.rcParams.update(
        {
            "figure.facecolor": "#fbf7ef",
            "axes.facecolor": "#fffdf8",
            "savefig.facecolor": "#fbf7ef",
            "axes.edgecolor": "#dccfb5",
            "grid.color": "#eadfc9",
            "font.size": 10,
            "axes.titlesize": 13,
            "axes.labelsize": 10,
            "legend.frameon": False,
        }
    )


def fig_suite_totals(rows: list[ResultRow], output_dir: pathlib.Path) -> None:
    loong_total_us = sum(row.loong_us for row in rows)
    pa_total_us = sum(row.pa_us for row in rows)
    loong_total_inst = sum(row.loong_inst for row in rows)
    pa_total_inst = sum(row.pa_inst for row in rows)

    labels = ["LoongArch", "PA RISC-V"]
    time_ms = [loong_total_us / 1000.0, pa_total_us / 1000.0]
    insts = [loong_total_inst, pa_total_inst]
    colors = ["#0f766e", "#b45309"]

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.8), constrained_layout=True)

    axes[0].bar(labels, time_ms, color=colors, width=0.58)
    axes[0].set_title("Total Host Time")
    axes[0].set_ylabel("ms")
    for idx, value in enumerate(time_ms):
        axes[0].text(idx, value + max(time_ms) * 0.03, f"{value:.3f} ms", ha="center", va="bottom")

    axes[1].bar(labels, insts, color=colors, width=0.58)
    axes[1].set_title("Total Guest Instructions")
    axes[1].set_ylabel("instructions")
    for idx, value in enumerate(insts):
        axes[1].text(idx, value + max(insts) * 0.03, f"{value:,}", ha="center", va="bottom")

    fig.suptitle("Aligned Compare Totals (Detailed 35-case Dataset)")
    fig.text(
        0.5,
        0.01,
        "Lower time is better. Higher instructions do not necessarily mean slower when host-side per-instruction overhead differs.",
        ha="center",
        fontsize=9,
        color="#6b5c45",
    )
    fig.savefig(output_dir / "fig01_suite_totals.svg")
    plt.close(fig)


def fig_delta_time(rows: list[ResultRow], output_dir: pathlib.Path) -> None:
    rows_sorted = sorted(rows, key=lambda row: row.delta_us)
    names = [row.name for row in rows_sorted]
    values = [row.delta_us for row in rows_sorted]
    colors = ["#0f766e" if value < 0 else "#c2410c" for value in values]

    fig, ax = plt.subplots(figsize=(11.5, 9), constrained_layout=True)
    ax.barh(names, values, color=colors, edgecolor="#fffaf0")
    ax.axvline(0, color="#7c6f55", linewidth=1.2)
    ax.set_title("Per-program Time Delta")
    ax.set_xlabel("delta_us = loong_us - pa_us")
    ax.set_ylabel("program")

    for row in rows_sorted:
        if abs(row.delta_us) >= 1200 or row.name == "mersenne":
            x = row.delta_us
            offset = 70 if x >= 0 else -70
            ax.text(
                x + offset,
                row.name,
                f"{x:+d}",
                va="center",
                ha="left" if x >= 0 else "right",
                fontsize=8,
                color="#4b4030",
            )

    fig.text(
        0.5,
        0.01,
        "Negative bars mean PA is slower. Positive bars mean LoongArch is slower.",
        ha="center",
        fontsize=9,
        color="#6b5c45",
    )
    fig.savefig(output_dir / "fig02_delta_time.svg")
    plt.close(fig)


def fig_time_vs_inst(rows: list[ResultRow], output_dir: pathlib.Path) -> None:
    fig, ax = plt.subplots(figsize=(10.8, 6.2), constrained_layout=True)

    ax.scatter(
        [row.loong_inst for row in rows],
        [row.loong_us for row in rows],
        s=70,
        alpha=0.9,
        color="#0f766e",
        label="LoongArch",
    )
    ax.scatter(
        [row.pa_inst for row in rows],
        [row.pa_us for row in rows],
        s=70,
        alpha=0.9,
        color="#b45309",
        label="PA RISC-V",
    )

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_title("Host Time vs Guest Instructions")
    ax.set_xlabel("guest instructions (log scale)")
    ax.set_ylabel("host time us (log scale)")
    ax.legend(loc="upper left")

    for row in rows:
        if row.name in {"mersenne", "matrix-mul", "crc32"}:
            ax.annotate(
                f"{row.name} (Loong)",
                (row.loong_inst, row.loong_us),
                xytext=(8, 8),
                textcoords="offset points",
                fontsize=8,
                color="#0f766e",
            )
            ax.annotate(
                f"{row.name} (PA)",
                (row.pa_inst, row.pa_us),
                xytext=(8, -12),
                textcoords="offset points",
                fontsize=8,
                color="#b45309",
            )

    fig.text(
        0.5,
        0.01,
        "Both frameworks scale roughly linearly with guest instructions, but the vertical gap shows different host-side per-instruction overhead.",
        ha="center",
        fontsize=9,
        color="#6b5c45",
    )
    fig.savefig(output_dir / "fig03_time_vs_inst.svg")
    plt.close(fig)


def fig_delta_inst(rows: list[ResultRow], output_dir: pathlib.Path) -> None:
    rows_sorted = sorted(rows, key=lambda row: row.delta_inst)
    names = [row.name for row in rows_sorted]
    values = [row.delta_inst for row in rows_sorted]
    colors = ["#b45309" if value < 0 else "#0f766e" for value in values]

    fig, ax = plt.subplots(figsize=(11.5, 9), constrained_layout=True)
    ax.barh(names, values, color=colors, edgecolor="#fffaf0")
    ax.axvline(0, color="#7c6f55", linewidth=1.2)
    ax.set_title("Per-program Instruction Delta")
    ax.set_xlabel("delta_inst = loong_inst - pa_inst")
    ax.set_ylabel("program")

    for row in rows_sorted:
        if abs(row.delta_inst) >= 1000 or row.name == "mersenne":
            x = row.delta_inst
            offset = 7000 if x >= 0 else -7000
            ax.text(
                x + offset,
                row.name,
                f"{x:+d}",
                va="center",
                ha="left" if x >= 0 else "right",
                fontsize=8,
                color="#4b4030",
            )

    fig.text(
        0.5,
        0.01,
        "Positive bars mean LoongArch executes more guest instructions. Negative bars mean PA executes more guest instructions.",
        ha="center",
        fontsize=9,
        color="#6b5c45",
    )
    fig.savefig(output_dir / "fig04_delta_inst.svg")
    plt.close(fig)


def main() -> None:
    if not RESULTS_PATH.is_file():
        raise SystemExit(f"Missing results file: {RESULTS_PATH}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    rows = load_rows(RESULTS_PATH)
    official_times = official_scored_times_ms(RESULTS_PATH)

    configure_matplotlib()
    write_csv(rows, OUTPUT_DIR)
    write_summary(rows, OUTPUT_DIR, official_times)
    fig_suite_totals(rows, OUTPUT_DIR)
    fig_delta_time(rows, OUTPUT_DIR)
    fig_time_vs_inst(rows, OUTPUT_DIR)
    fig_delta_inst(rows, OUTPUT_DIR)


if __name__ == "__main__":
    main()

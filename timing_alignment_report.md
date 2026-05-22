# `test_copy` Timing Alignment Report

## 修改前的差异

### 1. 计时窗口不同
- LoongArch 的 [toolchain/run_c_tests.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/run_c_tests.sh) 原先用外层脚本包围整个模拟器进程，统计的是一次进程运行的墙钟时间。
- PA 的 [cpu_test_runner.py](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/scripts/cpu_test_runner.py) 原先优先解析 NEMU 输出的 `host time spent`，这部分只覆盖 NEMU `cpu_exec()` 内的执行段时间。

### 2. 测试集合不同
- LoongArch 默认清单 [c_test_manifest.txt](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/tests/program/c_test_manifest.txt) 既包含 `test_copy` 的 35 个程序，也包含 7 个额外样例。
- PA `cpu-tests` 默认是按 `tests/*.c` 全量发现，并没有显式锁定 compare 用例顺序。

### 3. 编译优化级别不同
- LoongArch 的 [build_c_program.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/build_c_program.sh) 默认已经是 `-O0 -g`。
- PA 的 AM 默认在 [abstract-machine/Makefile](/home/wimiw/ics2025/ics2025/abstract-machine/Makefile) 使用 `-O2`。

## 统一后的标准
- 只比较 `test_copy` 对应的 35 个 C 程序。
- 两边都使用 `-O0` 构建 compare 工作负载。
- `Scored time` 统一定义为“虚拟机主执行循环内的宿主机执行时间”。
- `Total time` 仍保留外层脚本/runner 的总墙钟时间，只作为辅助观察，不参与 compare 主分数。

## 本次修改内容

### LoongArch 项目
- 在 [include/SimulatorRunner.h](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/include/SimulatorRunner.h) 的 `RunResult` 新增 `host_time_us` 字段。
- 在 [src/SimulatorRunner.cpp](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/src/SimulatorRunner.cpp) 使用 `std::chrono::steady_clock` 只包围执行循环本身，不把程序加载计入 `host_time_us`。
- 在 [src/simulator_main.cpp](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/src/simulator_main.cpp) 新增并固定打印：
  - `host time spent = <us> us`
  - `total guest instructions = <steps>`
  - `simulation frequency = <inst/s> inst/s`
- 在 [toolchain/run_c_tests.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/run_c_tests.sh) 改为优先解析模拟器内部的 `host time spent` 和 `total guest instructions`，并将 `Scored time` 改成内部执行段总和。
- 新增 compare 专用清单 [c_test_copy_manifest.txt](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/tests/program/c_test_copy_manifest.txt)。
- 新增 compare 入口 [run_c_tests_compare.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/run_c_tests_compare.sh)，默认只跑 `test_copy` 的 35 项。

### PA `cpu-tests` 项目
- 在 [cpu_test_runner.py](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/scripts/cpu_test_runner.py) 新增：
  - `--manifest`
  - `--time-standard {auto,exec-only,wall}`
  - `--opt-level {keep,o0,o2}`
- compare 模式下：
  - 通过 manifest 固定 35 个用例和顺序；
  - 通过生成临时 Makefile 追加 `CFLAGS += -O0 -g` 覆盖默认 `-O2`；
  - 只接受 `host time spent` 作为主时间来源；
  - 若缺少 `host time spent`，直接判定为 `timing error`，不再退回 Python 墙钟时间。
- 在 [Makefile](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/Makefile) 新增：
  - `compare-build`
  - `compare-run`
- 新增 compare 清单 [compare_manifest.txt](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/compare_manifest.txt)，与 LoongArch compare 清单一一对应、顺序一致。

## 运行命令

### LoongArch compare
```bash
cd /home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main
toolchain/run_c_tests_compare.sh
```

### PA compare
```bash
cd /home/wimiw/ics2025/ics2025
make -C am-kernels/tests/cpu-tests compare-run ARCH=riscv32-nemu
```

### LoongArch 保留旧全量入口
```bash
cd /home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main
MANIFEST=tests/program/c_test_manifest.txt toolchain/run_c_tests.sh
```

## 仍然存在的非完全等价项
- 两边 ISA 不同，因此即使同为 `-O0`，编译器生成的具体指令流不可能完全一致。
- compare 结果仍包含各自 runtime 的启动、trap 和退出路径，并没有进一步裁成“只测 `main()` 函数体”。
- PA 侧 NEMU 的统计来自其现有 `cpu_exec()`，本次没有修改 NEMU 核心，只是把 compare 框架显式锁定到这套统计口径。

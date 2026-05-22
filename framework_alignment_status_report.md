# LoongArch 与 PA `test_copy` 测试框架最终对齐状态报告

## 1. 当前结论

截至本次修改，**两边测试框架在“可控的框架层面”已经基本完成对齐**。

已经完成对齐的部分：

- 测试集合一致：两边都只跑 `test_copy` 对应的 35 个程序。
- 测试顺序一致：manifest 顺序一一对应。
- compare 构建标准一致：两边 compare 都按 `-O0 -g` 构建。
- 主时间口径一致：都以虚拟机内部执行段 `host time spent` 作为 compare 主时间。
- 汇总字段一致：都能稳定给出 `status`、`host_time_us`、`host_time_ms`、`instructions`。
- compare 入口清晰分离：都保留旧入口，同时新增/保留 compare 专用入口。

但如果把“完全对齐”理解成“机器级工作负载完全相同”，那么答案仍然是**否**。原因不是测试框架还没改完，而是两边本来就存在不可消除的结构差异：

- ISA 不同：LoongArch32 vs RISC-V32
- 编译器后端不同
- runtime 不同
- trap/启动/退出路径不同
- guest 指令流天然不同

所以本次工作的准确结论是：

> 从测试框架设计和统计口径上，已经对齐到能严肃横向比较的程度。  
> 从机器级执行内容上，它们仍然不是“完全同一份工作负载”。

## 2. 两边测试框架各自测什么

## 2.1 LoongArch 测什么

LoongArch compare 链路由这些部分组成：

- `toolchain/build_c_program.sh`
- `toolchain/run_c_tests.sh`
- `toolchain/run_c_tests_compare.sh`
- `build/mycpu_sim`

它实际测的是：

1. 用 LoongArch 交叉编译器把
   - `runtime/start.S`
   - `runtime/trap.c`
   - 目标 `tests_copy/*.c`
   编译成 `build_runtime/*.bin`
2. 用自研 LoongArch 模拟器 `build/mycpu_sim` 执行该二进制
3. 用 guest 程序的 trap / halt 结果判定通过与否
4. 统计模拟器内部执行段的：
   - `host time spent`
   - `total guest instructions`

也就是说，LoongArch 当前 compare 测的是：

> “LoongArch 最小 runtime + 目标 C 程序”在自研 LoongArch 虚拟机中的完整执行成本

## 2.2 PA 测什么

PA compare 链路由这些部分组成：

- `am-kernels/tests/cpu-tests/Makefile`
- `am-kernels/tests/cpu-tests/scripts/cpu_test_runner.py`
- `nemu/build/riscv32-nemu-interpreter`

它实际测的是：

1. 通过 AM 构建系统把
   - `abstract-machine/am`
   - `abstract-machine/klib`
   - `am-kernels/tests/cpu-tests/tests/*.c`
   编译为 `build/<name>-riscv32-nemu.bin`
2. 用 NEMU 执行这些二进制
3. 用 NEMU 输出里的 `HIT GOOD TRAP` / `HIT BAD TRAP` 判定通过与否
4. 统计 NEMU 内部执行段的：
   - `host time spent`
   - `total guest instructions`

也就是说，PA 当前 compare 测的是：

> “RISC-V + AM runtime + 目标 C 程序”在 NEMU 中的完整执行成本

## 3. 修改前是什么样

## 3.1 LoongArch 修改前

LoongArch 修改前的主要问题有三个：

### 1. 主时间口径不对

原来的 `toolchain/run_c_tests.sh` 用外层脚本的墙钟时间包住整个模拟器进程。

这意味着下面这些都会被算进 `Scored time`：

- 进程启动
- 镜像加载
- shell 调度
- 其它外层固定开销

短程序很容易被固定启动噪声主导。

### 2. 默认集合不纯

默认 manifest `tests/program/c_test_manifest.txt` 总共 42 项，其中混入了 7 个不属于 `test_copy` 的程序。

### 3. 输出字段不完整

LoongArch 原先没有固定打印 NEMU 风格的三行：

- `host time spent = ... us`
- `total guest instructions = ...`
- `simulation frequency = ...`

导致脚本只能从外层粗略统计时间。

## 3.2 PA 修改前

PA 修改前比 LoongArch 更接近 compare 目标，但还不够完整。

它原本的优点：

- 主时间已经优先使用 NEMU 输出的 `host time spent`
- 已经天然支持 `HIT GOOD TRAP` / `HIT BAD TRAP`
- 已经能取出 `total guest instructions`

它原本的不足：

### 1. 没有 compare 专用 manifest

默认是按 `tests/*.c` 自动发现，而不是显式锁定 compare 35 项和固定顺序。

### 2. 没有 compare 专用时间模式

虽然优先用 `host time spent`，但缺失时会退回别的时间来源，不够严格。

### 3. 没有 compare 专用优化级别

AM 默认在 `abstract-machine/Makefile` 中使用 `-O2`，没有显式 compare 模式来锁成 `-O0`。

### 4. 工具链路径不够稳

原来的 RISC-V 配置默认写死 `riscv64-linux-gnu-`，在当前机器上会碰到：

- `rv32 + ilp32` glibc 头文件不完整
- 不适合这条 bare-metal compare 构建链路

## 4. 本次最终做了哪些修改

## 4.1 LoongArch 侧最终修改

### `include/SimulatorRunner.h`

- 在 `RunResult` 中新增 `host_time_us`

作用：

- 让模拟器底层直接把内部执行时间返回给上层 runner

### `src/SimulatorRunner.cpp`

- 引入 `std::chrono::steady_clock`
- 只对主执行循环计时
- 成功 halt、步数跑满、异常返回都会写入 `host_time_us`
- 加载失败不写执行统计
- 调试打印改为只有 `trace` 打开时才输出

作用：

- LoongArch 的 compare 主时间从“外层墙钟”改成“执行循环内部时间”

### `src/simulator_main.cpp`

- 固定输出：
  - `host time spent = <us> us`
  - `total guest instructions = <steps>`
  - `simulation frequency = <inst/s> inst/s`
- 保留：
  - `Program halted with exit code ...`
  - `[SIM] halted after ...`
- 新增 `--trace` 参数

作用：

- 输出风格与 NEMU 统计字段对齐，便于脚本统一解析

### `toolchain/run_c_tests.sh`

- 新增：
  - `extract_host_time_us()`
  - `extract_guest_instructions()`
- 优先从模拟器输出里取 `host time spent`
- `Scored time` 改为所有 case 的内部执行时间总和
- `Total time` 继续保留外层墙钟时间
- 步数展示优先用 `total guest instructions`

作用：

- LoongArch compare 的主分数定义和 PA 对齐

### `tests/program/c_test_copy_manifest.txt`

- 新增 compare 专用 35 项 manifest

作用：

- 锁定 compare 用例集合和顺序

### `toolchain/run_c_tests_compare.sh`

- 新增 compare 专用入口

作用：

- 让 compare 35 项与原 42 项旧入口彻底分开

### `toolchain/build_c_program.sh`

- 将默认 `CFLAGS` 改为可从环境变量覆盖
- 默认值仍然保持：
  - `-ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 -g`

作用：

- 保持 LoongArch compare 默认 `-O0 -g`
- 同时允许后续实验时显式覆盖

## 4.2 PA 侧最终修改

### `am-kernels/tests/cpu-tests/scripts/cpu_test_runner.py`

新增 compare 能力：

- `--manifest`
- `--time-standard {auto,exec-only,wall}`
- `--opt-level {keep,o0,o2}`

新增最终版工具链/构建逻辑：

- 自动优先选择 `riscv64-unknown-elf-`
- 若系统没有 `unknown-elf`，再退回 `riscv64-linux-gnu-`
- compare 模式下向 `make` 命令行显式传入：
  - `CROSS_COMPILE=...`
  - `AM_OPT_CFLAGS=-O0`
  - `AM_EXTRA_CFLAGS=-g -isystem /usr/lib/picolibc/riscv64-unknown-elf/include`

作用：

- compare 构建不再依赖 `linux-gnu` 那套不完整的 `rv32 ilp32` glibc 头文件
- `-O0/-g` 不再只作用于顶层 test 文件，而是可以传递到递归子构建

### `abstract-machine/Makefile`

新增：

- `AM_OPT_CFLAGS ?= -O2`
- `AM_EXTRA_CFLAGS ?=`

并将默认 CFLAGS 组织改为：

- 先使用 `AM_OPT_CFLAGS`
- 再追加 `AM_EXTRA_CFLAGS`

作用：

- compare runner 可以稳定地把 `-O0 -g` 传到 `abstract-machine/am` 和 `abstract-machine/klib`
- 默认旧行为仍然保持 `-O2`

### `am-kernels/tests/cpu-tests/Makefile`

保留并正式使用：

- `compare-build`
- `compare-run`

作用：

- compare 模式成为 PA 框架内的正式入口，而不是临时脚本

### `am-kernels/tests/cpu-tests/compare_manifest.txt`

- 新增 compare 专用 35 项 manifest

作用：

- 锁定 PA compare 的测试集合和顺序，与 LoongArch 保持一致

## 5. 现在两边到底对齐到什么程度

## 5.1 已经对齐的维度

从测试框架角度，下面这些维度现在已经对齐：

### 1. 测试集合

- 两边都是同 35 项
- 顺序一致

### 2. compare 编译目标

- LoongArch compare：默认 `-O0 -g`
- PA compare：显式锁成 `-O0 -g`

### 3. compare 主时间语义

- 两边都以模拟器/虚拟机内部的 `host time spent` 为主分数
- 都不再把外层总墙钟时间当 compare 主时间

### 4. 结果字段

- 两边都能稳定输出：
  - `status`
  - `host_time_us`
  - `host_time_ms`
  - `instructions`

### 5. compare 入口

- LoongArch：`toolchain/run_c_tests_compare.sh`
- PA：`make -C am-kernels/tests/cpu-tests compare-run ARCH=riscv32-nemu`

## 5.2 仍然不可避免的差异

下面这些差异仍然存在，而且本质上无法靠“改测试框架”完全消除：

### 1. ISA 不同

- LoongArch32 vs RISC-V32

### 2. runtime 不同

- LoongArch：`runtime/start.S` + `runtime/trap.c`
- PA：AM / NEMU 的 startup + trap 路径

### 3. 编译器后端不同

- LoongArch 与 RISC-V 使用的是不同后端生成机器码

### 4. guest 指令流不同

即使都是 `-O0`，也不可能生成完全相同的指令流。

### 5. compare 仍包含 runtime 路径

当前统计不是只测 `main()` 函数体，而是包含：

- 启动
- trap
- 退出

因此现在能做到的是：

> “测试框架基本对齐”

而不是：

> “机器级工作负载完全等价”

## 6. 静态校验结果

本次最终对齐后的关键静态校验如下：

### PA compare runner 当前会传入的核心构建参数

```text
['CROSS_COMPILE=riscv64-unknown-elf-', 'AM_OPT_CFLAGS=-O0', 'AM_EXTRA_CFLAGS=-g -isystem /usr/lib/picolibc/riscv64-unknown-elf/include']
```

### PA compare 产物已包含调试段，说明 `-g` 生效

`readelf -S am-kernels/tests/cpu-tests/build/add-riscv32-nemu.elf` 可见：

```text
.debug_info
.debug_abbrev
.debug_line
.debug_str
...
```

## 7. 最新正式运行命令

### LoongArch

```bash
cd /home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main
toolchain/run_c_tests_compare.sh
```

### PA

```bash
cd /home/wimiw/ics2025/ics2025
make -C am-kernels/tests/cpu-tests compare-run ARCH=riscv32-nemu
```

## 8. 最新结果结论

最新正式 compare 运行结果：

- LoongArch compare：`35 / 35 passed`
- PA compare：`35 / 35 passed`

最新结果与逐项差值已经整理到：

- `latest_aligned_compare_results.md`

这份结果文档包含：

- 两边最新结果总览
- 逐程序并排对比
- 每行的时间差和指令数差

## 9. 最终判断

如果问题是：

> “两边测试框架现在是否已经完整对齐到可以严肃比较？”

答案是：

**可以。**

如果问题是：

> “两边现在是否已经完全变成机器级同一份工作负载？”

答案是：

**不可能，也没有必要。**

因为 ISA、runtime、trap 路径和编译器后端本来就不同。

所以本次工作的最终定位应当是：

> 已完成“框架层面的最完整对齐”，  
> 未追求也不可能达到“机器级工作负载完全相同”。

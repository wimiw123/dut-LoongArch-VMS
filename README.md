# dut-LoongArch-VMS

`dut-LoongArch-VMS` 是一个面向计算机系统结构实验的 LoongArch32 虚拟机 / CPU 模拟器项目。它不仅实现了能够运行 LoongArch32 程序的基础解释执行器，还在此基础上扩展出带五级前端、动态分支预测、Tomasulo 风格保留站、寄存器重命名和 ROB 顺序提交的高级微结构核心。

本项目的目标不是只让几条指令跑起来，而是实现具有以下功能的CPU虚拟机：指令译码与执行、异常和中断、分页地址翻译、总线与 MMIO 设备、最小运行时、程序加载器、单元测试、程序级回归、C 程序批量测试，以及与 PA / NEMU 框架对齐后的实验结果分析。

## 1. 项目总体能力

### 1.1 虚拟机平台组成

整个虚拟机平台由以下模块组成：

| 模块 | 位置 | 作用 |
| --- | --- | --- |
| CPU | `src/CPU.cpp`, `include/CPU.h` | LoongArch32 指令执行、异常处理、MMU、微结构统计 |
| Memory | `src/Memory.cpp`, `include/Memory.h` | 默认 16 MiB 主存，支持 32 位读写 |
| Bus | `src/Bus.cpp`, `include/Bus.h` | 把 CPU 的物理地址访问路由到内存或 MMIO 设备 |
| Uart | `src/Uart.cpp`, `include/Uart.h` | 简化 UART 控制台输出设备 |
| Timer | `src/Timer.cpp`, `include/Timer.h` | 定时器 MMIO 与中断挂起能力 |
| TestDevice | `src/TestDevice.cpp`, `include/TestDevice.h` | goodtrap / badtrap 测试退出设备 |
| ProgramLoader | `src/ProgramLoader.cpp`, `include/ProgramLoader.h` | 自动加载 `.hex` 与 `.bin` 程序镜像 |
| SimulatorRunner | `src/SimulatorRunner.cpp`, `include/SimulatorRunner.h` | 一站式搭建平台、运行程序、收集结果 |
| Runtime | `runtime/` | C 程序启动、链接脚本、trap 协议 |
| CLI | `src/simulator_main.cpp` | 命令行模拟器入口 |

默认平台配置集中在 `include/PlatformConfig.h`：

| 配置项 | 当前值 | 含义 |
| --- | ---: | --- |
| 程序入口 | `0x1000` | CPU reset 后开始执行的位置 |
| 数据段基址 | `0x2000` | 运行时和测试程序的数据区基准 |
| 栈顶 | `0xF000` | C 程序初始栈顶 |
| 主存大小 | `16 MiB` | 默认模拟内存容量 |
| TestDevice | `0x1FFFF000` | 测试退出 MMIO 地址 |
| 最大执行步数 | `1,000,000` | 防止测试程序死循环的预算 |

### 1.2 两种 CPU 核心模式

项目支持两种核心模式，可以通过环境变量 `LOONGARCH_CORE_MODE` 或命令行参数 `--core-mode` 切换：

```bash
./build/mycpu_sim programs/test_pass.hex --core-mode=baseline
./build/mycpu_sim programs/test_pass.hex --core-mode=advanced

LOONGARCH_CORE_MODE=baseline ./build/mycpu_sim programs/test_pass.hex
LOONGARCH_CORE_MODE=advanced ./build/mycpu_sim programs/test_pass.hex
```

| 模式 | 定位 | 特点 |
| --- | --- | --- |
| `baseline` | 顺序解释执行核心 | 每条 guest 指令按取指、译码、执行、写回的顺序完成，适合做 ISA 正确性基准 |
| `advanced` | 高级微结构核心 | 默认模式；模拟五级前端、动态分支预测、Tomasulo 风格动态调度、寄存器重命名、ROB 顺序提交和投机恢复 |

`baseline` 的价值在于简单、直接、便于定位指令语义问题；`advanced` 的价值在于展示更接近现代处理器的微结构行为，包括乱序完成、精确提交、预测失败恢复、ROB / RS 占用、stall 与 flush 统计。

## 2. LoongArch32 指令与体系结构功能

### 2.1 已实现指令概况

根据 `CPU_INSTRUCTION_SUPPORT.md`，当前实现了 59 条 LoongArch 指令，覆盖课程实验常用的整数、访存、控制流和基础特权子集。

| 类别 | 已支持内容 |
| --- | --- |
| 系统指令 | `NOP`, `SYSCALL`, `ERTN`, `BREAK` |
| CSR 指令 | `CSRRD`, `CSRWR`, `CSRXCHG` |
| 常量 / PC 相对 | `LU12I.W`, `PCADDU12I`, `PCADDI` |
| 位操作与扩展 | `BSTRPICK.W`, `EXT.W.H`, `EXT.W.B` |
| 移位 | `SLLI.W`, `SRLI.W`, `SRAI.W`, `SLL.W`, `SRL.W`, `SRA.W` |
| 整数运算 | `ADD.W`, `SUB.W`, `AND`, `OR`, `XOR`, `NOR`, `SLT`, `SLTU` |
| 乘除与取模 | `MUL.W`, `MULH.W`, `MULH.WU`, `DIV.W`, `DIV.WU`, `MOD.W`, `MOD.WU` |
| 立即数运算 | `ADDI.W`, `ANDI`, `ORI`, `XORI`, `SLTI`, `SLTUI` |
| Load | `LD.B`, `LD.BU`, `LD.H`, `LD.HU`, `LD.W` |
| Store | `ST.B`, `ST.H`, `ST.W` |
| 分支跳转 | `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`, `B`, `BL`, `JIRL`, `BNEZ`, `BEQZ` |

这些指令已经可以支撑加减乘除、比较、循环、函数调用、数组访问、排序、字符串处理、递归、数论和矩阵乘法等 C 程序样例。

### 2.2 CPU 状态

CPU 内部维护了较完整的体系结构状态：

- 32 个 32 位通用寄存器；
- 程序计数器 `PC`；
- 异常返回寄存器 `EPC`；
- 异常状态寄存器 `ESTAT`；
- 控制寄存器 `CRMD` 和 `ECFG`；
- 页目录基址寄存器 `PGDL`；
- CSR 数组；
- cycle counter；
- 浮点寄存器字段和 LL/SC 字段的预留状态。

其中 `r0 = 0` 的架构约束会在每次执行后显式维护。浮点指令、LL/SC 原子指令、TLB 管理指令和 cache 管理指令目前尚未实现，但相关状态字段已经为后续扩展留下接口。

### 2.3 异常、中断与 CSR

项目实现了基础异常和中断路径：

- 地址错误异常；
- 非法指令异常；
- `SYSCALL`；
- `BREAK`；
- 外部中断挂起与分发；
- `ERTN` 异常返回；
- `CRMD`, `ECFG`, `ESTAT`, `EPC` 等基础控制状态。

异常入口采用简化向量方式：

```text
exception_entry = 0x1C000000 + ex_code * 0x100
```

异常发生时，CPU 保存 `EPC`、更新 `ESTAT`、关闭中断使能并跳转到对应入口；`ERTN` 负责从异常返回并恢复执行。

### 2.4 MMU 与分页地址翻译

当前 MMU 是教学型简化两级页表模型，通过 `PGDL` 指向页目录基址。翻译流程如下：

1. 如果 `CRMD` 中的分页位未开启，则虚拟地址和物理地址直通；
2. 如果分页开启，则从虚拟地址中提取页目录索引、页表索引和页内偏移；
3. 读取 PDE 并检查有效位；
4. 读取 PTE 并检查有效位；
5. 拼接物理页框号和页内偏移得到物理地址；
6. 未映射或无效时触发地址异常。

`tests/unit/test_mmu.cpp` 已覆盖成功翻译和缺页异常两条关键路径。

## 3. 高级核心微结构

`advanced` 模式是本项目最重要的扩展之一。它不是把顺序核心简单拆成几个阶段，而是实现了一个带投机执行和乱序后端的教学型 OoO 模型。

### 3.1 流水线与执行阶段

高级核心的整体阶段可以概括为：

1. `IF`：按照预测 PC 取指；
2. `ID`：译码并形成统一指令信息；
3. `Rename / Dispatch`：分配 ROB 和保留站，完成寄存器重命名；
4. `Issue`：选择 ready 的保留站项发射到功能部件；
5. `Execute / Writeback`：功能部件执行，结果写回 ROB 并唤醒依赖项；
6. `Commit`：ROB 按程序顺序提交，更新架构状态。

当前高级核心是单发射乱序模型，功能部件包括：

- ALU；
- MUL / DIV；
- MEM；
- BRANCH；
- SYSTEM。

### 3.2 Tomasulo 风格动态调度

核心数据结构包括：

| 结构 | 作用 |
| --- | --- |
| Reservation Station | 保存等待发射的指令、源操作数或生产者 tag |
| RAT | 记录架构寄存器到最新生产者 ROB tag 的映射 |
| ROB | 保存投机结果，保证最终顺序提交 |
| Functional Units | 模拟不同指令类别的执行延迟与完成 |

源操作数如果已经可用，会直接记录值；如果仍由更老指令产生，则记录生产者 ROB tag。生产者写回时通过 tag 唤醒依赖者。这样可以体现 Tomasulo 中“重命名、等待、唤醒、乱序执行、顺序退休”的核心思想。

### 3.3 分支预测与投机恢复

高级核心使用 128 项动态分支预测表，每项包含：

- 2-bit 饱和计数器；
- 目标地址有效位；
- 最近目标地址。

预测策略包括：

- `B` / `BL` 默认预测 taken；
- `JIRL` 使用历史目标；
- 条件分支使用 2-bit 计数器；
- `ERTN` 按顺序路径处理。

当真实分支结果与预测不一致时，核心会清除更年轻的 ROB 项、保留站项、前端阶段和功能部件执行状态，随后根据保留下来的 ROB 重建 RAT，并把取指 PC 重定向到真实路径。

### 3.4 Load / Store 顺序与前递

访存处理采用“ROB + 保守顺序检查”的教学模型：

- store 在执行阶段只把地址和值写入 ROB，不直接写内存；
- store 到达 ROB 队首提交时才真正写内存；
- load 会检查更老 store，避免破坏内存顺序；
- 对相同地址的已完成 store 支持 load/store forwarding。

这让高级核心既能支持投机回滚和精确异常，又能展示乱序执行中最容易出错的内存相关性问题。

### 3.5 微结构统计

模拟器每次运行会输出以下统计，便于分析高级核心行为：

- `core mode`；
- `total guest instructions`；
- `total guest cycles`；
- `guest IPC` / `guest CPI`；
- `serialized no-overlap cycles`；
- `pipeline overlap gain`；
- branch 指令数、预测次数、命中数、未命中数、准确率；
- pipeline flushes；
- speculative squashes；
- register renames；
- out-of-order completions；
- load/store forwardings；
- ROB / RS / inflight 平均和峰值占用；
- ROB full、RS full、decode、issue、load-store order stalls。

这些指标不是附加打印，而是验证高级核心确实存在投机、窗口、重命名、乱序完成和顺序提交行为的关键证据。

## 4. MMIO 设备与运行时协议

### 4.1 地址映射

| 设备 | 地址范围 / 基址 | 说明 |
| --- | ---: | --- |
| Main Memory | `[0, 16 MiB)` | 普通物理内存 |
| Timer | `0x1FE00100` | 定时器寄存器区域 |
| UART | `0x1FE001E0` | 控制台字符输出 |
| TestDevice | `0x1FFFF000` | 测试退出状态设备 |

### 4.2 TestDevice goodtrap / badtrap 协议

`TestDevice` 是测试框架和虚拟机之间的核心接口：

```text
write32(0x1FFFF000, 0)      => goodtrap，测试通过
write32(0x1FFFF000, nonzero) => badtrap(code)，测试失败并返回 code
```

C 运行时中的 `trap.c` 和 `trap.h` 将测试结果写入该 MMIO 地址，模拟器外层据此判断程序是否成功停机、退出码是多少、是否超过最大步数。

### 4.3 程序加载

`ProgramLoader` 支持两类输入：

- `.hex`：用于手写或固定回归测试；
- `.bin`：由 LoongArch32 交叉编译器生成的裸机程序镜像。

这让项目既可以跑极小的手写指令程序，也可以跑真实 C 程序交叉编译后的二进制。

## 5. 测试框架

测试框架分成三层：模块单元测试、程序级回归测试、C 程序批量测试。

### 5.1 单元测试层

单元测试位于 `tests/unit/`，使用一个轻量自定义测试头 `tests/unit/test_framework.h`，核心宏包括：

- `EXPECT_TRUE(...)`；
- `EXPECT_EQ(...)`；
- `EXPECT_THROW(...)`；
- `TEST_PASS()`。

每个测试源文件都会被 CMake 构建成独立可执行文件，当前覆盖：

| 测试目标 | 覆盖内容 |
| --- | --- |
| `test_cpu_basic` | CPU 基础取指、执行和寄存器行为 |
| `test_cpu_state` | reset、寄存器、CSR、cycle 等状态 |
| `test_memory` | 内存读写和越界行为 |
| `test_bus` | 总线地址路由 |
| `test_uart` | UART MMIO 输出 |
| `test_timer` | Timer 寄存器和中断挂起 |
| `test_testdevice` | goodtrap / badtrap 退出设备 |
| `test_program_loader` | `.hex` 加载 |
| `test_program_loader_bin` | `.bin` 加载 |
| `test_mmu` | 分页翻译和缺页异常 |
| `test_runtime_protocol` | runtime 与 TestDevice 协议 |
| `test_trap_protocol` | trap 协议 |
| `test_runtime_loop` | 运行时循环程序 |
| `test_runtime_halt` | 正常停机路径 |
| `test_simulator_runner` | 一站式运行器 |
| `test_branch_programs` | 分支程序回归 |
| `test_load_store_programs` | load/store 程序回归 |
| `test_array_programs` | 数组程序回归 |

### 5.2 `.hex` 程序级回归

`program_test_runner` 读取 `tests/program/program_test_manifest.txt`，逐个运行 `programs/` 下的 `.hex` 程序并检查退出码。

当前清单覆盖：

- `test_pass.hex` / `test_fail.hex`；
- `trap_pass.hex` / `trap_fail.hex`；
- `check_fail.hex`；
- `branch_pass.hex` / `branch_fail.hex`；
- `load_store_pass.hex` / `load_store_fail.hex`；
- `array_pass.hex` / `array_fail.hex`。

判定条件包括：

1. 程序镜像成功加载；
2. 在最大步数内通过 TestDevice 停机；
3. 实际退出码与 manifest 中的期望值一致。

### 5.3 C 程序批量测试

C 程序测试由 `toolchain/run_c_tests.sh` 驱动：

1. 读取 manifest；
2. 调用 `toolchain/build_c_program.sh`；
3. 编译 `runtime/start.S`、`runtime/trap.c` 和目标 C 文件；
4. 链接成裸机 ELF；
5. 反汇编生成 `.dump`；
6. `objcopy` 生成 `.bin`；
7. 用 `build/mycpu_sim` 运行；
8. 从模拟器输出中提取退出码、host time、guest instruction count；
9. 输出逐项 PASS / FAIL 和汇总耗时。

默认 C 测试清单是 `tests/program/c_test_manifest.txt`，包含 7 个基础课程测试和 35 个扩展 CPU-tests 样例，共 42 项：

- `test_main`, `test_fail_main`, `test_check_fail`, `test_add`, `test_if_else`, `test_fact`, `test_fib`；
- `add-longlong`, `add`, `bit`, `bubble-sort`, `crc32`, `div`, `dummy`, `fact`, `fib`, `goldbach`, `hello-str`, `if-else`, `leap-year`, `load-store`, `matrix-mul`, `max`, `mersenne`, `min3`, `mov-c`, `movsx`, `mul-longlong`, `pascal`, `prime`, `quick-sort`, `recursion`, `select-sort`, `shift`, `shuixianhua`, `string`, `sub-longlong`, `sum`, `switch`, `to-lower-case`, `unalign`, `wanshu`。

对齐 PA / NEMU 的 compare 清单是 `tests/program/c_test_copy_manifest.txt`，对应 35 个 CPU-tests 样例。

## 6. 构建与运行

### 6.1 构建 C++ 模拟器和单元测试

```bash
cmake -S . -B build
cmake --build build -j
```

构建完成后主要产物包括：

- `build/mycpu_sim`：命令行模拟器；
- `build/program_test_runner`：`.hex` 程序级回归入口；
- `build/test_*`：各个单元测试。

### 6.2 运行单个程序

```bash
./build/mycpu_sim programs/test_pass.hex
./build/mycpu_sim programs/branch_pass.hex --trace
./build/mycpu_sim build_runtime/add.bin --core-mode=baseline
./build/mycpu_sim build_runtime/add.bin --core-mode=advanced
```

成功时会看到类似：

```text
[RESULT] PASS (goodtrap)
Simulation finished successfully.
```

失败时会看到：

```text
[RESULT] FAIL (badtrap, code=...)
Simulation finished with failure.
```

### 6.3 运行程序级回归

```bash
./build/program_test_runner
```

### 6.4 运行 C 程序测试

C 程序测试需要 LoongArch32 裸机交叉编译器。默认路径为：

```bash
$HOME/loongarch32-toolchain/install
```

也可以通过环境变量覆盖：

```bash
PREFIX=/path/to/loongarch32-toolchain/install ./toolchain/run_c_tests.sh
```

运行默认 42 项测试：

```bash
./toolchain/run_c_tests.sh
```

运行 35 项 PA 对齐 compare 测试：

```bash
./toolchain/run_c_tests_compare.sh
```

分别强制 baseline 和 advanced：

```bash
SIM=toolchain/mycpu_sim_baseline.sh ./toolchain/run_c_tests_compare.sh
SIM=toolchain/mycpu_sim_advanced.sh ./toolchain/run_c_tests_compare.sh
```

## 7. 新增测试的方法

### 7.1 新增 `.hex` 回归程序

1. 把 `.hex` 文件放入 `programs/`；
2. 在 `tests/program/program_test_manifest.txt` 增加一行：

```text
name  ../programs/your_case.hex  expected_exit_code
```

3. 重新运行：

```bash
./build/program_test_runner
```

### 7.2 新增 C 程序测试

1. 把 C 文件放入 `programs/` 或 `tests_copy/`；
2. 在 `tests/program/c_test_manifest.txt` 增加一行：

```text
case_name  path/to/case.c  expected_exit_code
```

3. 运行：

```bash
./toolchain/run_c_tests.sh
```

如果程序没有停机，可以结合模拟器输出的 step 数和 `build_runtime/*.dump` 反汇编文件定位缺失指令或错误跳转路径。

## 8. 实验结果

### 8.1 42 项 C 程序最终测试结果

最终日志见 `c_tests_final.log`。当前结果：

```text
C Tests PASS
42 / 42 passed
Summary: 42 passed, 0 failed, 42 total
Scored time: 811.892 ms
Total time: 13021.673 ms
```

其中典型样例的 guest step 数如下：

| 程序 | steps |
| --- | ---: |
| `test_main` | 34 |
| `test_add` | 57 |
| `test_fact` | 139 |
| `test_fib` | 1480 |
| `bubble-sort` | 13212 |
| `crc32` | 36873 |
| `matrix-mul` | 65166 |
| `mersenne` | 434629 |
| `quick-sort` | 8811 |
| `shuixianhua` | 29370 |

完整逐项时间与步数见 `c_tests_time_steps.csv`。

### 8.2 与 PA / NEMU 的对齐 compare 结果

详细结果见 `latest_aligned_compare_results.md`。官方 compare 入口的最新汇总如下：

| 框架 | 通过数 | Scored time | Total time |
| --- | ---: | ---: | ---: |
| LoongArch VMS | 35 / 35 | 9.811 ms | 10818.285 ms |
| PA / NEMU | 35 / 35 | 29.616 ms | 23989.984 ms |

逐项提取后的汇总为：

```text
LOONG_TOTAL_US=9294
LOONG_TOTAL_MS=9.294
PA_TOTAL_US=29616
PA_TOTAL_MS=29.616
TOTAL_DELTA_US=-20322
LOONG_TOTAL_INST=706136
PA_TOTAL_INST=283882
```

解释：

- 两边在对齐清单上均为 35 / 35 通过；
- 总体上 PA / NEMU 的 scored time 更高，比 LoongArch VMS 多约 `20322 us`；
- 多数程序中 LoongArch VMS 更快；
- `mersenne` 是最大例外，LoongArch VMS 明显更慢，原因是 64 位取模辅助路径导致 guest 指令数暴涨；
- 因为 LoongArch32 和 RISC-V32 的 ISA、runtime、编译器后端不同，compare 结论应理解为“框架口径对齐后的性能对比”，不是机器级相同指令流对比。

### 8.3 baseline 与 advanced 核心模式对比

详细报告见 `docs/microarch_benchmark/core_mode_compare_report.md`。35 个程序在两种模式下均正确通过：

| 指标 | baseline | advanced |
| --- | ---: | ---: |
| 通过数 | 35 / 35 | 35 / 35 |
| Scored time | 35.176 ms | 363.112 ms |
| Total time | 21198.350 ms | 13948.983 ms |
| 总 guest 指令数 | 706136 | 706136 |
| 总 guest cycles | 706136 | 941745 |
| suite IPC | 1.0000 | 0.7498 |
| suite CPI | 1.0000 | 1.3337 |
| 宿主机慢化倍数 | 1.0000x | 10.3238x |

高级核心在宿主机上更慢，这是预期现象：它需要显式维护 IF / ID、预测器、RAT、RS、ROB、功能部件、投机恢复和提交队列等大量状态。但它提供了明确的微结构证据：

| advanced 微结构指标 | 数值 |
| --- | ---: |
| 加权 branch prediction accuracy | 78.52% |
| 平均 ROB 占用 | 18.74 |
| 平均在飞指令数 | 19.79 |
| 总乱序完成次数 | 233874 |
| 总寄存器重命名次数 | 558449 |
| 总流水线 flush 次数 | 14694 |
| 总 speculative squash 次数 | 135291 |

因此，advanced 模式达成的是“微结构能力升级”，不是“宿主机解释器提速”。如果目标是研究流水线、预测、重命名、乱序执行和精确提交，它比 baseline 更有教学价值；如果目标是最高宿主机吞吐，baseline 更轻。

## 9. 仓库目录说明

```text
.
├── include/                         # 公共头文件
├── src/                             # CPU、总线、内存、外设、运行器和 CLI 实现
├── runtime/                         # C 程序裸机运行时、trap 协议、链接脚本
├── programs/                        # 手写 .hex 回归程序和基础 C 测试
├── tests/
│   ├── unit/                        # 单元测试
│   └── program/                     # manifest 和 program_test_runner
├── tests_copy/                      # 35 个 PA 对齐 CPU-tests C 样例
├── my_programs_c/                   # 扩展 C 程序样例集合
├── toolchain/                       # C 程序构建、批量测试、compare 脚本
├── tools/                           # 实验数据处理与图表生成脚本
├── docs/                            # 微结构、对齐、性能实验报告和图表数据
├── CPU_INSTRUCTION_SUPPORT.md       # 指令支持清单
├── latest_aligned_compare_results.md# 最新 PA 对齐 compare 结果
├── performance_analysis_report.md   # 性能分析报告
└── CMakeLists.txt                   # CMake 构建入口
```

## 10. 当前限制与后续方向

当前项目已经覆盖课程实验中的主要整数程序和微结构演示需求，但仍有明确边界：

- 尚未实现浮点执行指令；
- 尚未实现 LL/SC 原子指令；
- 尚未实现完整 TLB 管理指令；
- 尚未实现 cache 管理指令；
- 特权体系是教学型基础子集，不是完整 LoongArch 系统模拟器；
- advanced 模式强调微结构可观察性，宿主机运行速度会低于 baseline；
- compare 实验已经对齐测试口径，但 LoongArch32 与 RISC-V32 不是相同 guest 指令流。

后续可继续扩展：

1. 补齐更多 LoongArch ISA 指令；
2. 增加 decode cache / basic block cache 提升解释器吞吐；
3. 加入 profiler，统计 opcode 热点、访存热点和 helper 热点；
4. 优化 64 位除法 / 取模 helper，改善 `mersenne` 等异常样例；
5. 扩展 TLB、cache、异常级别和更完整的特权状态；
6. 在 advanced 模式中继续研究多发射、LSQ、cache miss、分支预测器替换策略等微结构主题。

## 11. 项目结论

本项目已经形成了一套完整的 LoongArch32 虚拟机系统：它能执行手写 `.hex` 程序，也能运行交叉编译生成的裸机 C 程序；它有轻量单元测试、程序级回归、C 程序批量测试和 PA 对齐 compare 流程；它同时保留了易于验证的顺序核心和具备微结构研究价值的高级核心。

从实验结果看，当前 42 项 C 程序全部通过，35 项 PA 对齐 compare 也全部通过。baseline 模式适合作为正确性基准和快速执行入口；advanced 模式则展示了动态分支预测、Tomasulo 风格调度、寄存器重命名、ROB 顺序提交、投机恢复和微结构统计。本项目既可以作为 LoongArch CPU 功能模拟器，也可以作为体系结构实验平台继续扩展。

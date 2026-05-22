# LoongArch 与 PA `test_copy` 性能差异分析报告

## 1. 结论摘要

基于当前已经对齐后的 compare 框架，LoongArch 在这组 `test_copy` 基准上整体确实比 PA 的 RISC-V NEMU 更快，但这个结论需要精确理解：

- 在官方 compare 汇总结果中，LoongArch 为 `9.811 ms`，PA 为 `29.616 ms`。
- 在逐项重提取的详细结果中，LoongArch 总计 `9294 us`，PA 总计 `29616 us`，PA 比 LoongArch 多 `20322 us`。
- 但是，LoongArch 并不是“每一项都更快”。`mersenne` 是最明显的反例，LoongArch 在这一项上反而更慢。
- 从数据上看，**LoongArch 整体更快的主因不是测试框架，而是虚拟机执行热路径本身的宿主机开销更低**。
- 从数据上看，**程序本身的特性和编译器生成代码的差异会决定个别程序是否反转结论**，`mersenne` 就是代表。

因此，本次比较最准确的结论是：

> 在已经完成框架对齐之后，LoongArch 更快的主要原因是“每条 guest 指令在宿主机上的执行成本更低”；  
> ISA、程序特征和编译器代码生成差异是重要次因，并在个别程序上成为主因；  
> 测试框架差异已经不是当前结果的主要解释变量。

## 2. 数据基础与测量口径

本报告使用的基础数据来自：

- 对齐状态报告：[framework_alignment_status_report.md](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/framework_alignment_status_report.md)
- 最新正式结果：[latest_aligned_compare_results.md](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/latest_aligned_compare_results.md)

当前两边 compare 已经对齐到以下标准：

- 两边都只跑 35 个 `test_copy` 程序。
- 两边都按 `-O0 -g` 构建 compare 工作负载。
- 两边都用虚拟机内部执行段 `host time spent` 作为主时间。
- 两边都输出 `status`、`host_time_us`、`host_time_ms`、`instructions`。

也就是说，当前比较的不是“整个脚本从启动到退出的总墙钟时间”，而是：

> “二进制已经加载完成后，虚拟机执行这些 guest 指令，在宿主机上实际花了多少时间”

这一步非常关键，因为它把之前最容易误导结论的固定启动噪声基本剔掉了。

## 3. 这两套测试框架现在测的到底是什么

### 3.1 LoongArch 在测什么

LoongArch compare 当前测的是：

- LoongArch 最小 runtime
- 目标 `tests_copy/*.c`
- 在自研 LoongArch 功能级虚拟机中的执行成本

主要入口与实现位置：

- 构建脚本：[toolchain/build_c_program.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/build_c_program.sh)
- compare 入口：[toolchain/run_c_tests_compare.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/run_c_tests_compare.sh)
- 主 runner：[toolchain/run_c_tests.sh](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/toolchain/run_c_tests.sh)
- 模拟器执行循环：[src/SimulatorRunner.cpp](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/src/SimulatorRunner.cpp)

### 3.2 PA 在测什么

PA compare 当前测的是：

- RISC-V + AM runtime + klib
- 目标 `tests/*.c`
- 在 NEMU 中的执行成本

主要入口与实现位置：

- compare 入口：[am-kernels/tests/cpu-tests/Makefile](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/Makefile)
- runner：[cpu_test_runner.py](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/scripts/cpu_test_runner.py)
- NEMU 执行循环：[cpu-exec.c](/home/wimiw/ics2025/ics2025/nemu/src/cpu/cpu-exec.c)

所以两边当前比较的是：

> “各自 ISA、各自 runtime、各自虚拟机实现”下，同一组 C 程序的执行成本

这已经足够做“框架级、公平口径”的横向比较，但仍然不是“机器级完全同一工作负载”。

## 4. 从数据里可以直接看出的事实

### 4.1 总体上 LoongArch 更快

从官方 compare 汇总结果看：

- LoongArch：`9.811 ms`
- PA：`29.616 ms`

从逐项详细提取结果看：

- LoongArch：`9294 us`
- PA：`29616 us`

总体上，PA 比 LoongArch 慢约 `3.19x`。

### 4.2 绝大多数程序里，PA 都更慢

详细结果中，大多数程序的 `delta_us = loong_us - pa_us` 都是负值，也就是：

- `bubble-sort`：LoongArch `168 us`，PA `1615 us`
- `crc32`：LoongArch `455 us`，PA `4104 us`
- `matrix-mul`：LoongArch `731 us`，PA `6568 us`
- `pascal`：LoongArch `151 us`，PA `1115 us`

这些例子都说明一个现象：

> PA 的慢，不是只在某一两个程序里出现，而是一个很稳定、很普遍的趋势。

### 4.3 但 `mersenne` 是强烈反例

`mersenne` 的结果是：

- LoongArch：`5910 us`，`434629` 条 guest 指令
- PA：`1498 us`，`14679` 条 guest 指令

这一项中，LoongArch 不仅更慢，而且执行指令数远高于 PA。

这说明：

> “LoongArch 更快”不是无条件成立的。  
> 一旦程序特征触发了某条特别差的代码生成路径，LoongArch 也会明显变慢。

### 4.4 最关键的证据：很多程序两边指令数几乎一样，但 PA 还是慢很多

把那些“指令数几乎相同”的程序单独拿出来看，一共有 22 个，例如：

- `bubble-sort`：`13212` vs `13223`
- `crc32`：`36873` vs `36884`
- `matrix-mul`：`65166` vs `65177`
- `pascal`：`12289` vs `12300`
- `select-sort`：`10280` vs `10291`

在这 22 个程序上：

- LoongArch 总时间：`2316 us`
- PA 总时间：`19812 us`
- 两边总指令数几乎一样：`189616` vs `189753`

换算成“每条 guest 指令对应的宿主机时间”：

- LoongArch 约 `12.21 ns / guest inst`
- PA 约 `104.41 ns / guest inst`

也就是说，在这些“工作量几乎一样”的程序上，PA 仍然慢了大约 `8.55x`。

这是整份分析里最重要的证据，因为它基本锁定了主因：

> 当两边执行的 guest 指令数量都差不多时，PA 依然显著更慢。  
> 因此，主因不是“程序做了更多事情”，而是“PA 每执行一条 guest 指令，宿主机付出的成本更高”。

## 5. 为什么 LoongArch 更快：分层分析

可以把影响因素分成四类：

- 虚拟机功能结构 / 执行热路径
- ISA 与编译器代码生成
- 程序本身的特征
- 测试框架本身

下面按影响大小来分析。

## 6. 主因：虚拟机功能结构与执行热路径不同

这是当前数据里最主要的解释变量。

### 6.1 LoongArch 的热路径更“轻”

LoongArch 的主执行循环在 [src/SimulatorRunner.cpp](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/src/SimulatorRunner.cpp) 中很直接：

- 一次 `cpu.step()`
- 更新步数
- 检查 `testDevice.halted()`
- 命中 halt 就退出

这条循环本身没有把很多“调试器/全系统设备/事件轮询”逻辑固定放到每条 guest 指令之后。

同时，LoongArch 内部还有明确的热路径快路径：

- [src/CPU.cpp](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/src/CPU.cpp) 对对齐的 `read_u32()` / `write_u32()` 走单次翻译 + 单次总线访问
- [src/Bus.cpp](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/src/Bus.cpp) 只在少量设备范围间做简单分发

这些都意味着：

> LoongArch 当前是一个更偏“轻量、定制化、直通式”的功能级解释器。

### 6.2 NEMU 的热路径更“重”

NEMU 的执行循环在 [cpu-exec.c](/home/wimiw/ics2025/ics2025/nemu/src/cpu/cpu-exec.c) 中，每执行一条指令后都会做：

- `exec_once()`
- `g_nr_guest_inst++`
- `trace_and_difftest()`
- `check_watchpoints()`
- `device_update()`

也就是说，PA 当前比较用到的并不是一个“只负责把指令跑完”的极简解释器，而是一个带有更完整系统框架的通用模拟器。

这点还能从 NEMU 配置里看出来，[auto.conf](/home/wimiw/ics2025/ics2025/nemu/include/config/auto.conf) 中当前启用了：

- `CONFIG_DEVICE=y`
- `CONFIG_MODE_SYSTEM=y`
- `CONFIG_HAS_TIMER=y`
- `CONFIG_HAS_VGA=y`
- `CONFIG_HAS_KEYBOARD=y`
- `CONFIG_HAS_AUDIO=y`
- `CONFIG_HAS_DISK=y`

而 [device.c](/home/wimiw/ics2025/ics2025/nemu/src/device/device.c) 的 `device_update()` 内部又会：

- 调 `get_time()`
- 按需更新 VGA
- 轮询 SDL 事件

[watchpoint.c](/home/wimiw/ics2025/ics2025/nemu/src/monitor/sdb/watchpoint.c) 里的 `check_watchpoints()` 虽然大多数时候不会真正触发，但它仍然是每条指令后的一次额外函数调用和分支路径。

所以，对于 NEMU 来说：

> 每执行一条 guest 指令，不只是“解释这条指令”本身；  
> 还要顺带维护一整套更通用、更重的系统级机制。

### 6.3 这类差异正好能解释“同指令数但 PA 更慢”

为什么我认为“虚拟机功能结构”是主因，而不只是一个猜测？

因为数据和代码两边是互相对应的：

- 数据上，很多程序两边 guest 指令数几乎一样，但 PA 时间高很多
- 代码上，NEMU 每条指令后确实做了比 LoongArch 更多的宿主机工作

这两条证据是可以闭环的。

所以当前最合理的判断是：

> LoongArch 更快的第一主因，是它的每条 guest 指令在宿主机上的固定成本更低。  
> 这本质上是“虚拟机实现结构”的优势，而不是“测试脚本算快了”。

## 7. 次因：ISA、runtime 和编译器代码生成差异

这部分是第二层原因。

两边虽然已经在框架层面对齐，但仍然天然不同：

- LoongArch32 vs RISC-V32
- 不同后端的交叉编译器
- 不同 runtime
- 不同 trap、启动、退出路径

这些差异会直接反映到：

- 生成的 guest 指令序列不同
- 某些算术、访存、分支模式的展开方式不同
- 某些库辅助函数的实现成本不同

但为什么我把它排在“虚拟机结构”之后？

因为如果 ISA 差异是多数程序的第一主因，那么很多程序的 guest 指令数应该明显不同；可现实是：

- 在 22 个程序里，两边 guest 指令数几乎相同
- 但时间差仍然非常大

这说明：

> 对多数程序而言，ISA 与代码生成不是第一主因；  
> 它们是真实存在的影响因素，但更多是在个别程序上放大差异。

## 8. 程序特征与代码生成差异会主导异常点

这部分在 `mersenne` 上体现得最明显。

### 8.1 `mersenne` 源码本身是相同的

我核对过：

- [tests_copy/mersenne.c](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/tests_copy/mersenne.c)
- [mersenne.c](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/tests/mersenne.c)

两边源文件内容一致。

### 8.2 但编译后的执行路径差异非常大

从反汇编上看：

- RISC-V 的 `mersenne` 在 [mersenne-riscv32-nemu.elf](/home/wimiw/ics2025/ics2025/am-kernels/tests/cpu-tests/build/mersenne-riscv32-nemu.elf) 中会调用 `__moddi3`
- LoongArch 的 `mersenne` 在 [build_runtime/mersenne.dump](/home/wimiw/dult-my-cpu-2026-main/dult-my-cpu-2026-main/build_runtime/mersenne.dump) 中也会调用 `__moddi3`

也就是说，二者都遇到了“32 位 ISA 上处理 64 位取模”的辅助路径。

但是最终结果却是：

- LoongArch：`434629` 条指令
- PA：`14679` 条指令

这表明这里的差异已经不是“测试框架”或“虚拟机热路径”能解释的了，而是：

- 代码生成策略不同
- 辅助函数实现不同
- 相关循环或条件路径展开方式不同

换句话说：

> 在 `mersenne` 这种高度依赖 64 位算术辅助路径的程序里，程序特征 + 编译器代码生成差异，已经压过了虚拟机本身的解释开销优势。

### 8.3 这也说明一个通用公式

当前这类 compare 的主时间，可以近似理解为：

> `总时间 ≈ guest 指令数 × 每条 guest 指令的宿主机成本 + 小量固定开销`

在多数程序中：

- LoongArch 的“每条指令宿主机成本”更低
- 所以它更快

在 `mersenne` 这类异常点中：

- LoongArch 的 guest 指令数暴涨
- 于是即使单条指令更便宜，总时间仍然可能反超

## 9. 测试框架是不是影响因素

答案是：

- 修改前，是影响因素
- 修改后，不再是当前结论的主因

修改前，LoongArch 用的是外层脚本墙钟时间，PA 用的是 NEMU 内部执行时间，这确实会扭曲结论。

但现在 compare 已经对齐到：

- 相同测试集合
- 相同顺序
- 相同 `-O0 -g`
- 相同主时间口径

并且“同指令数程序仍有约 `8.55x` 时间差”这个事实，已经足够说明：

> 现在看到的主差异，不是脚本层计时方法造成的，而是虚拟机实现本身造成的。

所以对“测试框架”这个因素的最终判断是：

- 之前：重要干扰项
- 现在：已经被大幅消除
- 当前：只剩少量运行噪声，不是主因

## 10. 影响因素的优先级排序

综合当前数据和代码实现，我给出的因果排序是：

### 第一主因：虚拟机功能结构 / 执行热路径

- LoongArch 是更轻量、更定制的功能级解释器
- NEMU 是更通用、更完整的系统级模拟器
- 这决定了“每条 guest 指令的宿主机成本”存在系统性差距

### 第二主因：程序特征 + 编译器代码生成

- 这类因素在异常点上非常强
- `mersenne` 就是最典型的反例

### 第三主因：ISA 与 runtime 差异

- 它们会影响具体指令流和辅助函数实现
- 但从整体趋势看，不是多数程序上的第一解释变量

### 第四主因：测试框架差异

- 当前 compare 已经把它压到较小范围
- 现在它主要表现为微小噪声，不足以解释整体结论

## 11. 如果目标是“让 LoongArch 框架跑得更快”，最值得优先做什么

这里必须先区分两个目标：

- 目标 A：让当前软件虚拟机本身跑得更快
- 目标 B：让“被模拟的 CPU 微结构”更先进

这两个目标不是一回事。

## 12. 对当前项目最有价值的优化方向

如果当前项目的目标是“提高宿主机上的仿真吞吐”，也就是让 benchmark 更快跑完，那么最值得优先做的是下面这些。

### 12.1 第一优先级：Basic Block Cache / Translation Cache

当前 `cpu.step()` 基本还是“一条 guest 指令解释一次”。

最值得做的优化，是把经常重复执行的基本块缓存起来：

- 先把一个 basic block 预解码
- 缓存这个 block 里每条指令的已解析信息
- 以后直接按缓存结果执行，而不是重新完整解码

如果进一步走得更远，还可以做：

- 轻量级微操作缓存
- 简单 JIT

这是最可能带来数量级收益的方向，因为它直接减少了每条指令的宿主机分发成本。

### 12.2 第二优先级：Decode Cache / Predecode

即使暂时不做 block translation，也非常建议至少做：

- PC 到解码结果的缓存
- 常见操作码的预解码表

这样可以减少：

- 位段提取
- 大量 `if/else` 或 opcode 判定
- 反复构造相同的访存/立即数/寄存器信息

对于解释器来说，这是高性价比优化。

### 12.3 第三优先级：改进 dispatch 机制

当前解释器还是典型的“按条进入、按条判断”的结构。

可以考虑：

- dispatch table
- direct-threaded interpreter
- superinstruction

这类优化的目标是减少：

- host 分支跳转开销
- opcode 分派成本
- 每条 guest 指令的固定框架成本

如果实现得好，收益通常会非常稳定。

### 12.4 第四优先级：进一步压缩访存与地址翻译开销

当前 LoongArch 已经有对齐访存快路径，这是好基础。

下一步可以继续做：

- 最近页的地址翻译缓存
- 更激进的 RAM fast path
- 把 MMIO 路径和普通内存路径分得更开
- 对热点代码段/数据段做更直接的访问通道

因为在这些小 benchmark 里，访存和分支都是高频操作。

### 12.5 第五优先级：针对 64 位辅助运算做专项优化

`mersenne` 已经告诉我们，当前 LoongArch 框架在某些 64 位算术辅助路径上成本很高。

建议优先分析：

- `__moddi3`
- `__divdi3`
- `long long` 相关辅助代码

可以做的工作包括：

- 先定位热点 helper
- 检查 LoongArch 编译器在 `-O0` 下的具体展开方式
- 看是否能通过更合适的 runtime/helper 实现降低指令数
- 看是否能在解释器层对部分高频 helper 模式做专门快路径

如果这个方向处理好，`mersenne` 这类反例会明显改善。

### 12.6 第六优先级：先加 profiler，再做大改

非常建议在 LoongArch 框架里先加以下统计：

- 各 opcode 执行次数
- 各类访存次数
- 各类 branch 次数
- 解释器内部各阶段耗时
- runtime helper 的调用次数和累计时间

这样做的好处是：

- 不再凭感觉优化
- 能知道 `mersenne` 到底卡在什么地方
- 能知道最该优先优化的是 decode、dispatch、memory 还是 helper

## 13. 关于“流水线、分支预测、乱序执行”的判断

这部分要特别小心，因为它很容易混淆“模拟的 CPU 更先进”和“模拟器跑得更快”。

### 13.1 如果你的目标是让软件虚拟机跑得更快

那么结论是：

> **不要把流水线、分支预测、乱序执行当成当前的第一优化方向。**

原因很简单：

- 你现在的 LoongArch 框架本质上是功能级解释器
- 如果再去显式模拟流水线阶段、预测器状态、ROB、保留站、旁路和提交逻辑
- 宿主机要维护的状态会更多
- 解释器每步做的工作会更多

这通常会让模拟器更慢，而不是更快。

也就是说：

- 对真实 CPU 而言，流水线、分支预测、乱序执行会提升硬件性能
- 对软件解释器而言，显式模拟这些结构通常会降低宿主机吞吐

### 13.2 如果你的目标是做更真实的 CPU 微结构研究

那么这些方向当然有价值：

- 五级或多级流水线
- 静态或动态分支预测
- Scoreboard
- Tomasulo
- ROB + 乱序执行

但这时你的目标就变成了：

- 更真实地研究 CPI、冒险、旁路、flush、预测命中率
- 而不是“把 benchmark 更快跑完”

换句话说：

> 这些技术更适合“提高被模拟 CPU 的微结构真实性”，  
> 不适合“提高当前软件虚拟机的运行吞吐”。

## 14. 对 LoongArch 的推荐路线图

如果目标是“继续提升当前框架运行效率”，我建议的顺序是：

### 路线 1：先做吞吐优化

优先级从高到低：

1. block cache / translation cache
2. decode cache / predecode
3. dispatch 优化
4. memory / address translation fast path
5. `long long` helper 专项优化
6. profiler 驱动的热点消除

这条路线最适合当前这个项目，因为它直接对应“benchmark 跑得更快”。

### 路线 2：再决定是否做微结构模拟

如果后续目标转向“更像真正 CPU”，再考虑：

1. 五级流水线
2. 分支预测
3. cache 模型
4. 乱序执行

但这条路线应该被视为“功能扩展 / 研究扩展”，而不是当前性能优化路线。

## 15. 最终结论

基于当前已经对齐后的测试框架，我的最终判断是：

- LoongArch 整体比 PA 快，这个结论成立。
- 当前结论的主因不是测试框架，而是 LoongArch 虚拟机的执行热路径更轻、每条 guest 指令的宿主机成本更低。
- ISA 差异、runtime 差异和编译器代码生成差异是真实存在的，但对多数程序来说，它们不是第一解释变量。
- 程序特征在异常点上非常关键，`mersenne` 说明一旦 guest 指令数暴涨，LoongArch 也会明显变慢。
- 如果目标是继续提升 LoongArch 的运行效率，最该优先投入的是 block cache、decode cache、dispatch 优化、访存快路径和 64 位辅助运算优化。
- 如果目标是做更真实的 CPU 研究，流水线、分支预测、乱序执行值得做；但如果目标是让当前软件虚拟机更快，它们不是优先项，甚至可能让模拟器更慢。

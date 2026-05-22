# LoongArch 高级微结构实现说明

## 1. 报告目的

这份报告说明当前虚拟机里“`advanced` 核心”是如何实现以下能力的：

- 五级流水线前端
- 动态分支预测
- Tomasulo 风格保留站与寄存器重命名
- ROB 顺序提交与乱序执行

对应的核心代码主要在：

- `include/CPU.h:19-139`
- `src/CPU.cpp:35-2423`
- `src/SimulatorRunner.cpp:23-184`
- `src/simulator_main.cpp:9-127`

## 2. 总体设计

CPU 现在有两个运行模式：

- `baseline`：顺序执行解释器，入口在 `CPU::stepBaseline()`，代码位于 `src/CPU.cpp:1093-1489`
- `advanced`：带投机和乱序的核心，入口在 `CPU::stepAdvanced()`，代码位于 `src/CPU.cpp:1491-2423`

模式切换通过环境变量 `LOONGARCH_CORE_MODE` 完成，解析逻辑在 `src/CPU.cpp:837-854`，构造函数在 `src/CPU.cpp:858-861`；命令行参数 `--core-mode` 在 `src/simulator_main.cpp:64-84` 中转成环境变量。

从接口上看，`CPU` 新增了：

- `CoreMode`，位于 `include/CPU.h:19-23`
- `CoreMetrics`，位于 `include/CPU.h:25-57`
- `AdvancedState` 指针和 `stepAdvanced()`，位于 `include/CPU.h:123-139`

## 3. 关键数据结构

### 3.1 指令元信息 `InstructionInfo`

`InstructionInfo` 是整个 OoO 核心的“统一指令描述符”，定义在 `src/CPU.cpp:108-145`。它在译码阶段一次性记录：

- 指令种类 `op`
- 应该去哪个功能部件 `unit`
- 源寄存器、目的寄存器、立即数
- 是否是 branch / load / store
- load/store 大小和符号扩展语义
- 执行延迟 `latency`
- 顺序模型下的理论串行代价 `serial_cost`

这样后续 Rename、Issue、Execute、Commit 都不再重复译码。

### 3.2 ROB 项 `ROBEntry`

ROB 项定义在 `src/CPU.cpp:168-200`。每项保存：

- 全局顺序号 `seq`
- 指令元信息 `inst`
- 预测得到的 `predicted_next_pc`
- 执行完成后的结果、异常、访存信息、CSR 更新信息
- 各阶段时间戳 `fetch_cycle/rename_cycle/issue_cycle/writeback_cycle/commit_cycle`

ROB 用 `std::deque<ROBEntry>` 实现，位于 `AdvancedState` 的 `rob` 字段，定义在 `src/CPU.cpp:724`。

### 3.3 保留站 `ReservationStation`

保留站项定义在 `src/CPU.cpp:202-223`。这里沿用了 Tomasulo 的核心思想：每个源操作数都可以是：

- 已就绪的值 `value`
- 未就绪时等待某个生产者标签 `tag`

具体体现在：

- `src1_ready/src1_value/src1_tag`
- `src2_ready/src2_value/src2_tag`
- `extra_ready/extra_value/extra_tag`

这里的 `tag` 不是物理寄存器号，而是生产者 ROB 的 `seq`，所以本质上是“ROB tag 驱动的依赖唤醒”。

### 3.4 功能部件状态 `FunctionalUnitState`

功能部件定义在 `src/CPU.cpp:225-235`，当前实现为 5 类单发射功能部件：

- `alu`
- `muldiv`
- `mem`
- `branch`
- `system`

它们都放在 `AdvancedState` 中，见 `src/CPU.cpp:729-733`。每类只有 1 个实例，所以当前实现是 OoO，但不是 superscalar。

### 3.5 预测器与重命名表

`AdvancedState` 中还包含：

- 128 项分支预测表 `predictor`，位于 `src/CPU.cpp:735`
- 32 项寄存器别名表 `rat`，位于 `src/CPU.cpp:736`
- 前端 PC `fetch_pc`，位于 `src/CPU.cpp:737`
- 两级前端缓冲 `if_stage` / `id_stage`，位于 `src/CPU.cpp:726-727`

`rat[reg] = seq` 表示“这个架构寄存器的最新值将由 ROB 中 seq 对应的指令产生”。

## 4. 五级流水线前端是怎么落地的

严格来说，当前实现是“经典五级前端 + OoO 后端提交”的混合模型，不是教科书里纯粹的 `IF/ID/EX/MEM/WB`。在代码中更准确的阶段划分是：

1. `IF`：取指并给出预测下一 PC
2. `ID`：把取回的指令推进到译码槽
3. `Rename/Dispatch`：分配 ROB/RS，做寄存器重命名
4. `Issue`：从 RS 选择 ready 指令发射到 FU
5. `Execute/Writeback`：功能部件完成后写回 ROB，并唤醒依赖者

随后还有一个 OoO 机器必需的 `Commit` 阶段，所以整体实际上是 5 级前端加顺序退休。

### 4.1 IF

取指逻辑在 `src/CPU.cpp:2307-2328`：

- 用 `state.fetch_pc` 访问内存
- 成功则 `decode_instruction(raw, state.fetch_pc)`
- 失败则构造一条 `FetchFault` 伪指令
- 调用 `predict_next_pc()` 得到预测方向和目标
- 把结果塞进 `if_stage`

### 4.2 ID

ID 很简单，在 `src/CPU.cpp:2330-2334`，只是把 `if_stage` 推到 `id_stage`。这保留了一个显式的前端级间寄存器，方便统计和刷流水。

### 4.3 Rename / Dispatch

Rename/Dispatch 在 `src/CPU.cpp:2234-2305`：

- 为 `id_stage` 分配一项 ROB
- 为其分配一项 RS
- 根据 `rat` 判断每个源操作数是直接从架构寄存器文件读，还是挂到某个未完成生产者的 tag 上
- 如果目的寄存器存在，就更新 `rat[dest] = seq`

这一段就是 Tomasulo 风格重命名的核心：

- 不是让后继指令直接依赖架构寄存器
- 而是依赖“将来会在 ROB 中写回结果的生产者”

相关代码：

- ROB 分配：`src/CPU.cpp:2239-2249`
- 源操作数绑定：`src/CPU.cpp:2256-2289`
- 更新 RAT：`src/CPU.cpp:2300-2304`

### 4.4 Issue

Issue 在 `src/CPU.cpp:2336-2415`：

- 线性扫描 RS
- 找到操作数全部 ready 且 `earliest_issue_cycle <= m_cycle_count` 的项
- 按 `unit` 找空闲 FU
- 发射后从 RS 删除

当前是单发射，所以每个周期最多发射 1 条。发射动作本身在 `try_issue_to_fu()`，位于 `src/CPU.cpp:1920-1929`。

### 4.5 Execute / Writeback

每个周期都会调用 `complete_fu()` 检查各 FU 是否完成，见 `src/CPU.cpp:2215-2219`。真正的写回逻辑在 `src/CPU.cpp:1979-2117`：

- 执行指令语义，得到 `ExecuteResult`
- 把结果写回 ROB，而不是直接写架构寄存器
- 若有目的值，则调用 `wake_dependents()` 用 seq tag 唤醒等待中的 RS 项

依赖唤醒代码在 `src/CPU.cpp:1562-1581`。

## 5. 动态分支预测

### 5.1 预测器结构

分支预测器的每项结构定义在 `src/CPU.cpp:237-242`：

- `counter`：2-bit 饱和计数器
- `target_valid`
- `target`

索引函数是 `predictor_index(pc) = (pc >> 2) & 0x7F`，位于 `src/CPU.cpp:270-273`，也就是一个 128 项直接映射表。

### 5.2 预测策略

预测逻辑在 `predict_next_pc()`，位于 `src/CPU.cpp:749-792`：

- `B/BL`：固定预测 taken
- `Ertn`：不做动态预测，默认顺序流
- `Jirl`：如果表项里有合法 target，就预测 taken 到该 target
- 条件分支：当 `counter >= 2` 且 `target_valid` 时预测 taken，否则预测 not-taken

当前被标记为使用动态预测的指令在 `decode_instruction()` 中设置，代码位于 `src/CPU.cpp:667-705`。

### 5.3 更新与回滚

预测器在功能部件完成时更新，代码位于 `src/CPU.cpp:2074-2114`：

- 对条件分支：taken 时计数器加一，not-taken 时减一
- 对 `Jirl`：直接把计数器拉到强 taken，并写入 target
- 对所有动态预测分支：用 `predicted_next_pc` 和 `actual next_pc` 做比较

一旦失配，就调用 `squash_younger_than()`，代码位于 `src/CPU.cpp:1495-1546`，把：

- 更年轻的 ROB 项
- 更年轻的 RS 项
- `if_stage/id_stage`
- 正在执行的更年轻 FU

全部刷掉，并按保留下来的 ROB 重建 `rat`。

这部分就是投机执行后的恢复机制。

## 6. Tomasulo 风格保留站与寄存器重命名

### 6.1 风格而不是逐字复刻

当前实现是“保留站 + tag 唤醒 + RAT 重命名 + ROB 结果缓冲”的 Tomasulo 风格，而不是完全照搬论文版多 CDB、多广播端口设计。它保留了最关键的三点：

- 目的寄存器先重命名，再发射
- 操作数通过 tag 跟踪生产者
- 生产者完成后广播唤醒消费者

### 6.2 重命名

重命名发生在 `src/CPU.cpp:2300-2304`。只要指令有目的寄存器且目标不是 `r0`，就把 `rat[dest]` 改成当前指令的 `seq`。

这样之后读取同一个架构寄存器的指令，会先去看 `rat`，而不是直接读 `m_regs`，代码在 `src/CPU.cpp:2267-2288`。

### 6.3 依赖跟踪

如果生产者已经在 ROB 里 ready 并且有结果，就直接旁路拿值；否则就在 RS 里记下 `tag = producer_seq`，等待写回时唤醒。

这部分代码在：

- 绑定源操作数：`src/CPU.cpp:2256-2289`
- 写回唤醒：`src/CPU.cpp:2067-2069`

### 6.4 乱序完成

指令只要进入功能部件，完成顺序就不要求与程序顺序一致。代码通过 `completed_before_older` 记录“这条指令是否早于某条更老的未完成指令结束”，位于 `src/CPU.cpp:2054-2065`，并在提交时累计 `out_of_order_completions`，位于 `src/CPU.cpp:2150-2153`。

## 7. ROB 顺序提交与精确状态

### 7.1 为什么需要 ROB

乱序完成之后，结果不能直接写架构状态，否则异常和分支失预测后状态会乱。ROB 的作用就是：

- 先缓存 speculative 结果
- 按程序顺序退休
- 只在退休点更新架构寄存器、内存和 CSR

### 7.2 提交逻辑

提交逻辑在 `src/CPU.cpp:2145-2213`：

- 只看 `rob.front()`
- 只有队首 ready 才允许提交
- 每次 `CPU::step()` 最终只保证至少提交 1 条再返回

提交时做三件事：

- 写目的寄存器
- 写 store 到内存
- 应用 CSR 更新

对应代码：

- 写寄存器：`src/CPU.cpp:2177-2180`
- 写 store：`src/CPU.cpp:2182-2196`
- 写 CSR：`src/CPU.cpp:2172-2175`

### 7.3 精确异常

异常不会在执行阶段立即破坏架构状态，而是先记在 ROB 项里，字段在 `ROBEntry` 中定义于 `src/CPU.cpp:175-191`。当它走到 ROB 队首时，在提交阶段调用 `raise_exception()` 并 `clear_pipeline()`，代码位于 `src/CPU.cpp:2164-2169`。

这保证了异常对外表现为“按程序顺序在最老 faulting 指令处发生”，也就是精确异常。

## 8. 访存乱序中的顺序性处理

当前实现没有单独 LSQ，而是用“ROB + 保守顺序检查”处理内存相关性。

### 8.1 Load 不能越过不确定的更老 Store

`rob_has_older_pending_store()` 位于 `src/CPU.cpp:1931-1952`。策略是：

- 如果前面有更老 store 还没算出地址/数据，就阻塞这个 load
- 如果前面有更老 store 且地址与当前 load 相同，也阻塞

这比真正的内存别名预测更保守，但更容易保证正确性。

### 8.2 Store-to-Load Forwarding

`try_forward_store_value()` 位于 `src/CPU.cpp:1954-1977`。load 完成时会从 ROB 中倒着找最近的更老 ready store：

- 如果地址命中，就直接转发 store 的值
- 并把 `load_store_forwardings` 计数加一，见 `src/CPU.cpp:2016-2020`

### 8.3 Store 延迟到提交

store 在执行阶段只把地址和数据写到 ROB，不直接写内存；真正的内存写发生在提交阶段，代码是 `src/CPU.cpp:2182-2196`。这样才能和分支回滚、异常处理兼容。

## 9. 指标采集与对外输出

为了让“虽然更慢，但 CPU 更先进”这件事可以被量化，我把统计链路一起接出来了。

### 9.1 内部统计

`CoreMetrics` 定义在 `include/CPU.h:25-57`，覆盖：

- 取指/译码/发射/执行/提交计数
- 分支预测命中与失误
- pipeline flush / speculative squash
- register rename / out-of-order completion
- ROB/RS/inflight 占用
- ROB/RS/decode/issue/load-store 相关 stall

### 9.2 运行结果汇总

`captureMicroArchSummary()` 在 `src/SimulatorRunner.cpp:23-76` 中把 CPU 内部计数折算成：

- `guest_cycles`
- `guest IPC / CPI`
- `branch prediction accuracy`
- `average ROB occupancy`
- `average inflight instructions`
- `pipeline overlap gain`

### 9.3 命令行输出

`src/simulator_main.cpp:9-45` 会把这些指标打印出来，所以现有基准框架不需要改动模拟器主接口，就能额外抓到高级核心的数据。

## 10. 当前实现的边界与简化

为了保证教学项目可控，这个版本有意做了几处简化：

- 单发射，不是 superscalar
- 每类功能部件只有 1 个实例
- 没有独立 LSQ，访存相关性处理偏保守
- `Jirl` 更像简化版 BTB，而不是完整的间接跳转预测器
- 分支在 FU 完成时解析，不做更激进的早解析
- `CPU::step()` 的语义仍然是“推进到至少提交 1 条指令为止”，所以它既不是纯周期级，也不是单条解释级，而是“微结构内部循环 + 对外按提交计步”的混合接口

换句话说，这是一颗“教学型 OoO 核心”，重点是把关键概念都落进代码，而不是追求工业级宽发射实现。

## 11. 一句话总结

当前 `advanced` 核心的本质是：

“用 `if_stage/id_stage + predictor + RAT + RS + FU + ROB` 组织出一条可投机、可乱序执行、可顺序提交、可精确回滚的教学型 LoongArch OoO 微结构。”

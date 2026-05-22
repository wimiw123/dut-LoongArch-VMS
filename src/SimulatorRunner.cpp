#include "SimulatorRunner.h"
#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "PlatformConfig.h"
#include "ProgramLoader.h"
#include "TestDevice.h"
#include "Timer.h"
#include "Uart.h"

#include <exception>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <iostream>

namespace loongarch
{

namespace
{

void captureMicroArchSummary(const CPU &cpu, RunResult &result)
{
    const CoreMetrics &metrics = cpu.getCoreMetrics();
    result.guest_cycles = cpu.getCycleCount();
    result.core_mode = cpu.getCoreModeName();
    result.serialized_cycles = metrics.serialized_cycles;
    result.branch_instructions = metrics.branch_instructions;
    result.dynamic_branch_predictions = metrics.dynamic_branch_predictions;
    result.branch_prediction_hits = metrics.branch_prediction_hits;
    result.branch_prediction_misses = metrics.branch_prediction_misses;
    result.pipeline_flushes = metrics.pipeline_flushes;
    result.speculative_squashes = metrics.speculative_instructions_squashed;
    result.register_renames = metrics.register_renames;
    result.out_of_order_completions = metrics.out_of_order_completions;
    result.load_store_forwardings = metrics.load_store_forwardings;
    result.max_rob_occupancy = metrics.max_rob_occupancy;
    result.max_rs_occupancy = metrics.max_rs_occupancy;
    result.max_inflight_instructions = metrics.max_inflight_instructions;
    result.rob_full_stalls = metrics.rob_full_stalls;
    result.rs_full_stalls = metrics.rs_full_stalls;
    result.decode_stalls = metrics.decode_stalls;
    result.issue_stalls = metrics.issue_stalls;
    result.load_store_order_stalls = metrics.load_store_order_stalls;

    if (result.guest_cycles > 0u)
    {
        result.guest_ipc = static_cast<double>(result.steps) / static_cast<double>(result.guest_cycles);
        result.guest_cpi = static_cast<double>(result.guest_cycles) / static_cast<double>(result.steps == 0 ? 1 : result.steps);
        result.average_rob_occupancy =
            static_cast<double>(metrics.rob_occupancy_samples) / static_cast<double>(result.guest_cycles);
        result.average_rs_occupancy =
            static_cast<double>(metrics.rs_occupancy_samples) / static_cast<double>(result.guest_cycles);
        result.average_inflight_instructions =
            static_cast<double>(metrics.inflight_occupancy_samples) / static_cast<double>(result.guest_cycles);
    }

    if (result.dynamic_branch_predictions > 0u)
    {
        result.branch_prediction_accuracy =
            static_cast<double>(result.branch_prediction_hits) /
            static_cast<double>(result.dynamic_branch_predictions);
    }

    if (result.core_mode == "baseline")
    {
        result.serialized_cycles = result.guest_cycles;
        result.overlap_gain = 1.0;
    }
    else if (result.guest_cycles > 0u)
    {
        result.overlap_gain =
            static_cast<double>(result.serialized_cycles) / static_cast<double>(result.guest_cycles);
    }
}

} // namespace

// 一站式运行环境搭建与执行
RunResult runHexProgram(const std::string &program_path, std::uint32_t entry,
                        std::uint64_t max_steps, bool trace)
{
    // 【阶段 1：全村总动员式装配硬件】
    // 在这狭小的运行框中，生生“无中生有”捏造出了主板上的全员：
    Memory mem(loongarch::PlatformConfig::MEMORY_SIZE); // 造张内存条
    Uart uart;                                          // 造块串口板
    Timer timer;                                        // 造个定时器
    TestDevice testDevice;                              // 造个测试专用伪装板
    Bus bus(mem, uart, timer, testDevice);              // 将上述这堆破铜烂铁全部焊接挂在主干总线上
    CPU cpu(bus);                                       // 把龙芯 CPU 大脑安插在总线上
    ProgramLoader loader(mem);                          // 叫来外判装卸工

    RunResult result{}; // 拿好待填写的体检报告册子

    // 【阶段 2：环境清理】
    cpu.reset(entry);   // 把 CPU 大洗脑回到原点出厂态
    testDevice.reset(); // 清理测试探针状态

    // 【阶段 3：用装载工给此计算机注入第一股来自测试程序的代码灵魂！】
    const std::size_t loaded = loader.loadFileAuto(program_path, entry);
    result.loaded = (loaded > 0);

    // trace (追踪模式)下，就在屏幕大厅广众打印报告告诉你加载情况
    if (trace)
    {
        std::cout << "[SIM] program=" << program_path << "\n";
        std::cout << "[SIM] entry=0x" << std::hex << entry << std::dec
                  << " max_steps=" << max_steps << "\n";
        std::cout << "[SIM] loaded=" << loaded << " instruction(s)\n";
    }

    if (!result.loaded)
    {
        return result;
    }

    const auto exec_start = std::chrono::steady_clock::now();
    auto finalize_host_time = [&result, exec_start]() {
        result.host_time_us = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - exec_start)
                .count());
    };

    // 【阶段 4：惊心动魄的主执行大循环开始了！】
    try
    {
        // 我们只容忍它最多干 max_steps 这么多次粗活
        for (std::uint64_t step = 0; step < max_steps; ++step)
        {
            // 给 CPU 屁股上踹一脚！令其向前摸出哪怕一步！就这一步囊括了多少 Fetch/Decode/Run 的血与泪。
            cpu.step();
            result.steps = step + 1; // 给它记上一笔工分！

            // 调试追踪狂人专属的满屏炫光跑马灯打印 （打印出这一步走后的核心财产现状）
            if (trace && step < 100)
            {
                std::cout << "[SIM][step " << std::setw(3) << step + 1 << "] "
                          << "pc=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.getPC()
                          << " r4=0x" << std::setw(8) << cpu.getReg(4)
                          << " r12=0x" << std::setw(8) << cpu.getReg(12)
                          << " r13=0x" << std::setw(8) << cpu.getReg(13)
                          << " r14=0x" << std::setw(8) << cpu.getReg(14)
                          << std::dec << std::setfill(' ') << " cycles=" << cpu.getCycleCount()
                          << "\n";
            }

            // 【阶段 5：验证与停机刹车】
            // 每次干完活都要问下旁边的监工测控板：测试完毕申请下班停机了吗？！
            if (testDevice.halted())
            {
                result.halted = true; // 如愿正常停机
                result.exit_code = testDevice.exitCode(); // 看看给不给工钱
                if (trace)
                {
                    std::cout << "[SIM] halted after " << result.steps
                              << " step(s), exit_code=" << result.exit_code << "\n";
                }
                captureMicroArchSummary(cpu, result);
                // 收摊，打卡下班。
                finalize_host_time();
                return result;
            }
        }

        // 走到了这里说明什么？说明特么跑满定额步数还在那没命死转不出来！死循环！直接拖出去斩了！
        if (trace)
        {
            std::cout << "[SIM] reached max_steps without halt, steps=" << result.steps << "\n";
        }

        captureMicroArchSummary(cpu, result);
        finalize_host_time();
        return result;
    }
    catch (const std::exception &)
    {
        // 这一步说明发生重大内核车祸抛出了C++级致命系统报错
        captureMicroArchSummary(cpu, result);
        finalize_host_time();
        return result;
    }
}

} // namespace loongarch

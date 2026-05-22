#include "SimulatorRunner.h"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printStatistics(const loongarch::RunResult &result)
{
    const std::uint64_t frequency =
        (result.host_time_us == 0u) ? 0u : (result.steps * 1000000u) / result.host_time_us;

    std::cout << "host time spent = " << result.host_time_us << " us\n";
    std::cout << "total guest instructions = " << result.steps << "\n";
    std::cout << "simulation frequency = " << frequency << " inst/s\n";
    std::cout << "core mode = " << result.core_mode << "\n";
    std::cout << "total guest cycles = " << result.guest_cycles << "\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "guest IPC = " << result.guest_ipc << "\n";
    std::cout << "guest CPI = " << result.guest_cpi << "\n";
    std::cout << "serialized no-overlap cycles = " << result.serialized_cycles << "\n";
    std::cout << "pipeline overlap gain = " << result.overlap_gain << "\n";
    std::cout << "branch instructions = " << result.branch_instructions << "\n";
    std::cout << "dynamic branch predictions = " << result.dynamic_branch_predictions << "\n";
    std::cout << "branch prediction hits = " << result.branch_prediction_hits << "\n";
    std::cout << "branch prediction misses = " << result.branch_prediction_misses << "\n";
    std::cout << "branch prediction accuracy = " << (result.branch_prediction_accuracy * 100.0) << " %\n";
    std::cout << "pipeline flushes = " << result.pipeline_flushes << "\n";
    std::cout << "speculative squashes = " << result.speculative_squashes << "\n";
    std::cout << "register renames = " << result.register_renames << "\n";
    std::cout << "out-of-order completions = " << result.out_of_order_completions << "\n";
    std::cout << "load/store forwardings = " << result.load_store_forwardings << "\n";
    std::cout << "average ROB occupancy = " << result.average_rob_occupancy << "\n";
    std::cout << "peak ROB occupancy = " << result.max_rob_occupancy << "\n";
    std::cout << "average RS occupancy = " << result.average_rs_occupancy << "\n";
    std::cout << "peak RS occupancy = " << result.max_rs_occupancy << "\n";
    std::cout << "average inflight instructions = " << result.average_inflight_instructions << "\n";
    std::cout << "peak inflight instructions = " << result.max_inflight_instructions << "\n";
    std::cout << "ROB full stalls = " << result.rob_full_stalls << "\n";
    std::cout << "RS full stalls = " << result.rs_full_stalls << "\n";
    std::cout << "decode stalls = " << result.decode_stalls << "\n";
    std::cout << "issue stalls = " << result.issue_stalls << "\n";
    std::cout << "load/store order stalls = " << result.load_store_order_stalls << "\n";
    std::cout.unsetf(std::ios::floatfield);
}

} // namespace

// 【全村唯一的希望：最根源大本营入口的主函数（main）】
int main(int argc, char *argv[])
{
    // 默认测试文件。如果没有给参数喂它跑什么，它内部自嗨跑这个默认程序。
    std::string program_path = "../programs/test_exit.hex";
    bool trace = false;
    std::string core_mode;
    
    // argc如果 >= 2 意味着用户在黑框终端后面亲自带参数指点它去跑谁家里的代码用例
    if (argc >= 2)
    {
        program_path = argv[1];
    }

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--trace")
        {
            trace = true;
        }
        else if (arg.rfind("--core-mode=", 0) == 0)
        {
            core_mode = arg.substr(std::string("--core-mode=").size());
        }
        else if (arg == "--core-mode" && i + 1 < argc)
        {
            core_mode = argv[++i];
        }
    }

    if (!core_mode.empty())
    {
        setenv("LOONGARCH_CORE_MODE", core_mode.c_str(), 1);
    }

    // “全军出击！”
    // 直接调兵遣将呼唤刚刚包好的一次性快速打包发射器跑起来！跑的步长就是标准规定的超长马拉松
    const auto result = loongarch::runHexProgram(program_path, loongarch::PlatformConfig::ENTRY,
                                                 loongarch::PlatformConfig::MAX_STEPS, trace);

    // 分析刚刚送回来的作战体检报告单结果：

    // 1.连娘胎都出不来的死法（读取文件阶段拉手刹挂了）
    if (!result.loaded)
    {
        std::cerr << "Failed to load program: " << program_path << "\n";
        return 1; // 1 一般表示向操作系统报丧失败退堂
    }

    printStatistics(result);

    // 2.跑到累死跑成神仙死循环发狂不听指令停止的疯子死法
    if (!result.halted)
    {
        std::cerr << "Program did not halt within step budget.\n";
        std::cerr << "[SIM] reached max_steps without halt, steps=" << result.steps << "\n";
        return 1;
    }

    std::cout << "Program halted with exit code " << result.exit_code << "\n";
    std::cout << "[SIM] halted after " << result.steps
              << " step(s), exit_code=" << result.exit_code << "\n";

    // 3.圆满寿终正寝通过了所有考验的涅槃：如果汇编测试通过了，会向测试机设备留下 0 作为退出证明
    if (result.exit_code == 0)
    {
        std::cout << "[RESULT] PASS (goodtrap)\n";
        std::cout << "Simulation finished successfully.\n";
        return 0; // 0：通关撒花
    }
    else
    {
        // 4.测试计算没通过出错的耻辱死亡碑文（留下了非 0 数）
        std::cout << "[RESULT] FAIL (badtrap, code=" << result.exit_code << ")\n";
        std::cout << "Simulation finished with failure.\n";
        return 1;
    }
}

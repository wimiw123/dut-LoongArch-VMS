#pragma once // 确保只被包含一次

#include "PlatformConfig.h" // 运行器需要知道默认平台配置（例如入口点等）
#include <cstdint>
#include <string>

namespace loongarch
{

/**
 * @brief 记录一次模拟运行结果的结构体。
 * 
 * 就像是一份体检报告，运行结束后它会告诉你程序是怎么停下来的。
 */
struct RunResult
{
    bool loaded{false};          // 程序是否成功加载到内存中？（没加载成功就是 false）
    bool halted{false};          // 程序是否通过正常的途径（比如发 halt 信号）自己停下来的？
    std::uint32_t exit_code{0};  // 程序退出时的状态码（通常 0 表示大吉大利一切正常，非0表示出错了）
    std::uint64_t steps{0};      // 从开机到停下，CPU 总共艰难跋涉了多少步（执行了多少条指令）？
    std::uint64_t host_time_us{0}; // 主执行循环在宿主机上花掉的时间（微秒）
    std::uint64_t guest_cycles{0};
    double guest_ipc{0.0};
    double guest_cpi{0.0};
    std::string core_mode{"advanced"};

    std::uint64_t serialized_cycles{0};
    double overlap_gain{0.0};

    std::uint64_t branch_instructions{0};
    std::uint64_t dynamic_branch_predictions{0};
    std::uint64_t branch_prediction_hits{0};
    std::uint64_t branch_prediction_misses{0};
    double branch_prediction_accuracy{0.0};
    std::uint64_t pipeline_flushes{0};
    std::uint64_t speculative_squashes{0};

    std::uint64_t register_renames{0};
    std::uint64_t out_of_order_completions{0};
    std::uint64_t load_store_forwardings{0};

    double average_rob_occupancy{0.0};
    std::uint64_t max_rob_occupancy{0};
    double average_rs_occupancy{0.0};
    std::uint64_t max_rs_occupancy{0};
    double average_inflight_instructions{0.0};
    std::uint64_t max_inflight_instructions{0};

    std::uint64_t rob_full_stalls{0};
    std::uint64_t rs_full_stalls{0};
    std::uint64_t decode_stalls{0};
    std::uint64_t issue_stalls{0};
    std::uint64_t load_store_order_stalls{0};
};

/**
 * @brief 运行外部测试程序的快捷一键封装函数。
 * 
 * 它可以自动完成：创建内存 -> 挂载总线 -> 重置CPU -> 装载HEX程序 -> 开始死循环执行。
 * 一直跑到程序自己喊停，或者达到了最大强转步数（防止死循环）。
 * 
 * @param program_path  编译好的 hex 测试程序的路径。
 * @param entry         程序的入口地址，默认按照 PlatformConfig 里配好的 0x1000 去启动。
 * @param max_steps     最高允许跑的步数，如果执行指令超过这个数会被强行打断叫停。
 * @param trace         要不要开启跟踪调试打印（true的话每跑一步可能都要在屏幕刷一串信息）。
 * @return              返回前面定义的 RunResult 体检报告，告诉你跑完后的结果。
 */
RunResult runHexProgram(const std::string &program_path,
                        std::uint32_t entry = loongarch::PlatformConfig::ENTRY,
                        std::uint64_t max_steps = loongarch::PlatformConfig::MAX_STEPS,
                        bool trace = false);

} // namespace loongarch

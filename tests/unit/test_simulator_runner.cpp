#include "SimulatorRunner.h"
#include "test_framework.h"

#include <filesystem>
#include <string>

/*
 * 这个文件测试的是 SimulatorRunner 运行器模块。
 * 重点验证的功能包括：
 * 1) runHexProgram 能加载并运行可正常退出的程序；
 * 2) runHexProgram 在步数预算不足时，会返回 loaded=true 但 halted=false；
 * 3) trace 打开时终端会输出丰富的 [SIM] 调试信息。
 *
 * 当终端输出中出现：
 * - [SIM] 开头的逐步运行日志；
 * - [CHECK] 显示成功退出程序 exit_code=0；
 * - [CHECK] 显示预算不足时 halted=false、steps 等于 max_steps；
 * - 最后一行出现 [PASS] tests/unit/test_simulator_runner.cpp；
 * 就表示 SimulatorRunner 的加载、循环执行与返回结果封装功能正常。
 */
using namespace loongarch;

namespace
{
std::string test_exit_path()
{
    if (std::filesystem::exists("programs/test_exit.hex"))
    {
        return "programs/test_exit.hex";
    }
    return "../programs/test_exit.hex";
}
} // namespace

int main()
{
    const std::string program_path = test_exit_path();

    TEST_INFO("[SIM_RUNNER] 运行 test_exit.hex，验证正常退出路径");
    const RunResult pass_result =
        runHexProgram(program_path, PlatformConfig::ENTRY, 8, true);

    TEST_INFO("[SIM_RUNNER] 正常退出结果: loaded=" << pass_result.loaded
              << " halted=" << pass_result.halted
              << " exit=" << pass_result.exit_code
              << " steps=" << pass_result.steps);
    EXPECT_TRUE(pass_result.loaded);
    EXPECT_TRUE(pass_result.halted);
    EXPECT_EQ(pass_result.exit_code, 0u);
    EXPECT_EQ(pass_result.steps, 3u);

    TEST_INFO("[SIM_RUNNER] 再次运行 test_exit.hex，但只给 2 步预算，验证未退出路径");
    const RunResult budget_result =
        runHexProgram(program_path, PlatformConfig::ENTRY, 2, true);

    TEST_INFO("[SIM_RUNNER] 预算不足结果: loaded=" << budget_result.loaded
              << " halted=" << budget_result.halted
              << " exit=" << budget_result.exit_code
              << " steps=" << budget_result.steps);
    EXPECT_TRUE(budget_result.loaded);
    EXPECT_EQ(budget_result.halted, false);
    EXPECT_EQ(budget_result.exit_code, 0u);
    EXPECT_EQ(budget_result.steps, 2u);

    TEST_PASS();
}

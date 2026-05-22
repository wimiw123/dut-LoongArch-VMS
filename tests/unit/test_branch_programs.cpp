#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "ProgramLoader.h"
#include "TestDevice.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

/*
 * 运行判定说明（分支程序模块）：
 * 1) [CHECK] 显示 branch_pass.hex 返回 0；
 * 2) [CHECK] 显示 branch_fail.hex 返回 3；
 * 3) 最后一行 [PASS]。
 * 原因：说明分支控制流结果与预期一致，分支相关实现可区分正确/错误路径。
 */
using namespace loongarch;

static std::uint32_t run_and_get_exit_code(const char* path)
{
    Memory mem(16 * 1024 * 1024);
    Uart uart;
    Timer timer;
    TestDevice testDevice;
    Bus bus(mem, uart, timer, testDevice);
    CPU cpu(bus);
    ProgramLoader loader(mem);

    constexpr std::uint32_t ENTRY = 0x1000;
    constexpr std::uint64_t MAX_STEPS = 16;

    cpu.reset(ENTRY);
    testDevice.reset();

    const std::uint32_t loaded = loader.loadHexFile(path, ENTRY);
    EXPECT_TRUE(loaded > 0);

    for (std::uint64_t i = 0; i < MAX_STEPS; ++i) {
        cpu.step();
        if (testDevice.halted()) {
            return testDevice.exitCode();
        }
    }

    std::cerr << "[FAIL] program did not halt: " << path << "\n";
    return 0xFFFF'FFFFu;
}

int main() {
    EXPECT_EQ(run_and_get_exit_code("../programs/branch_pass.hex"), 0u);
    EXPECT_EQ(run_and_get_exit_code("../programs/branch_fail.hex"), 3u);
    TEST_PASS();
}
#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "ProgramLoader.h"
#include "TestDevice.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

/*
 * 运行判定说明（Trap 协议模块）：
 * 1) [CHECK] 显示 trap_pass.hex 返回 0；
 * 2) [CHECK] 显示 trap_fail.hex 返回 2；
 * 3) [CHECK] 显示 check_fail.hex 返回 1；
 * 4) 最后一行 [PASS]。
 * 原因：证明陷阱/失败分类路径能通过 TestDevice 返回正确错误码。
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
    constexpr std::uint64_t MAX_STEPS = 8;

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
    EXPECT_EQ(run_and_get_exit_code("../programs/trap_pass.hex"), 0u);
    EXPECT_EQ(run_and_get_exit_code("../programs/trap_fail.hex"), 2u);
    EXPECT_EQ(run_and_get_exit_code("../programs/check_fail.hex"), 1u);

    TEST_PASS();
}
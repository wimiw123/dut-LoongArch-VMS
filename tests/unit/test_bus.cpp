#include "Bus.h"
#include "Memory.h"
#include "TestDevice.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

#include <iomanip>
#include <iostream>
#include <sstream>

/*
 * 这个文件测试的是 Bus 总线模块。
 * 重点验证的功能包括：
 * 1) Bus 能否把普通地址正确路由到 Memory；
 * 2) Bus 能否把 Timer MMIO 地址正确路由到 Timer；
 * 3) Bus 能否把 UART MMIO 地址正确路由到 Uart，并输出字符；
 * 4) Bus 能否把 TestDevice 地址正确路由到退出设备；
 * 5) Bus 对未映射地址是否会抛出异常。
 *
 * 当终端输出中出现：
 * - 多条 [INFO] 路由调试信息；
 * - 多条 [CHECK] 且所有断言通过；
 * - 最后一行出现 [PASS] tests/unit/test_bus.cpp；
 * 就表示 Bus 模块的地址分发与异常保护功能正常运行。
 */
using namespace loongarch;

int main()
{
    TEST_INFO("[BUS] 初始化 Memory/UART/Timer/TestDevice/Bus");
    Memory memory(16 * 1024 * 1024);
    Uart uart;
    Timer timer;
    TestDevice testDevice;
    Bus bus(memory, uart, timer, testDevice);

    TEST_INFO("[BUS] 测试普通内存地址路由：向 0x20 写入 0x11223344");
    bus.write32(0x20u, 0x11223344u);
    const std::uint32_t memory_value = bus.read32(0x20u);
    TEST_INFO("[BUS] 从 0x20 读回值=0x" << std::hex << memory_value << std::dec);
    EXPECT_EQ(memory_value, 0x11223344u);

    TEST_INFO("[BUS] 测试 Timer 路由：设置阈值=3、使能计时器");
    bus.write32(Timer::PhysicalBase + 0x04u, 3u);
    bus.write32(Timer::PhysicalBase + 0x08u, 1u);
    EXPECT_EQ(bus.read32(Timer::PhysicalBase + 0x04u), 3u);
    EXPECT_EQ(bus.read32(Timer::PhysicalBase + 0x08u), 1u);

    TEST_INFO("[BUS] 手动 tick 三次，观察 pending 状态");
    timer.tick();
    TEST_INFO("[BUS] tick #1 pending=" << timer.pending());
    timer.tick();
    TEST_INFO("[BUS] tick #2 pending=" << timer.pending());
    timer.tick();
    TEST_INFO("[BUS] tick #3 pending=" << timer.pending());
    EXPECT_TRUE(timer.pending());

    TEST_INFO("[BUS] 通过 Bus 写 Timer 清中断寄存器");
    bus.write32(Timer::PhysicalBase + 0x0Cu, 0x1u);
    EXPECT_EQ(timer.pending(), false);

    TEST_INFO("[BUS] 测试 UART 路由，捕获 UART 输出字符 'Z'");
    std::ostringstream captured;
    std::streambuf *old_buf = std::cout.rdbuf(captured.rdbuf());
    bus.write32(Uart::PhysicalBase, static_cast<std::uint32_t>('Z'));
    std::cout.rdbuf(old_buf);
    TEST_INFO("[BUS] UART 捕获输出=\"" << captured.str() << "\"");
    EXPECT_EQ(captured.str(), std::string("Z"));

    TEST_INFO("[BUS] 测试 TestDevice 路由：写入退出码 9");
    bus.write32(TestDevice::BASE_ADDR, 9u);
    EXPECT_EQ(testDevice.halted(), true);
    EXPECT_EQ(testDevice.exitCode(), 9u);
    EXPECT_EQ(bus.read32(TestDevice::BASE_ADDR), 9u);
    EXPECT_EQ(bus.read32(TestDevice::BASE_ADDR + 4u), 1u);

    TEST_INFO("[BUS] 测试未映射地址异常：0xF0000000");
    EXPECT_THROW((void)bus.read32(0xF0000000u));
    EXPECT_THROW(bus.write32(0xF0000000u, 0x55u));

    TEST_PASS();
}

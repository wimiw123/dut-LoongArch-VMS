#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "TestDevice.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

/*
 * 运行判定说明（Runtime 单步循环模块）：
 * 1) [CHECK] 显示执行一条 ADDI.W 后 r1=42；
 * 2) [CHECK] 显示 PC 前进 4 字节、cycle=1；
 * 3) [CHECK] 显示 testDevice.halted()==false；
 * 4) 最后一行 [PASS]。
 * 原因：说明最小执行循环可稳定推进，不会误触发退出设备。
 */
using namespace loongarch;

int main() {
    Memory mem(16 * 1024 * 1024);
    Uart uart;
    Timer timer;
    TestDevice testDevice;
    Bus bus(mem, uart, timer, testDevice);
    CPU cpu(bus);

    constexpr std::uint32_t ENTRY = 0x1000;

    cpu.reset(ENTRY);
    testDevice.reset();

    // 已知可运行指令：ADDI.W r1, r0, 42
    mem.write32(ENTRY + 0, 0x0280A801);

    cpu.step();

    EXPECT_EQ(cpu.getReg(1), 42u);
    EXPECT_EQ(cpu.getPC(), ENTRY + 4);
    EXPECT_TRUE(cpu.getCycleCount() >= 1u);
    EXPECT_EQ(testDevice.halted(), false);

    TEST_PASS();
}

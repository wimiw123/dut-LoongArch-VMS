#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

/*
 * 运行判定说明（CPU 状态寄存器接口）：
 * 1) [CHECK] 应显示 reset 后 PC=0x1000、r0/r1=0、cycle=0；
 * 2) [CHECK] 显示 r1 可写为 123；
 * 3) [CHECK] 显示写 r0 后仍为 0；
 * 4) 最后一行 [PASS]。
 * 原因：验证了 CPU 状态初始化、通用寄存器写入、以及零寄存器不可变语义。
 */
using namespace loongarch;

int main() {
    Memory mem(16 * 1024 * 1024);
    Uart uart;
    Timer timer;
    TestDevice testDevice;
    Bus bus(mem, uart, timer, testDevice);
    CPU cpu(bus);

    cpu.reset(0x1000);

    EXPECT_EQ(cpu.getPC(), 0x1000u);
    EXPECT_EQ(cpu.getReg(0), 0u);
    EXPECT_EQ(cpu.getReg(1), 0u);
    EXPECT_EQ(cpu.getCycleCount(), 0u);

    cpu.setReg(1, 123);
    EXPECT_EQ(cpu.getReg(1), 123u);

    cpu.setReg(0, 999);
    EXPECT_EQ(cpu.getReg(0), 0u);  // r0 必须保持为 0

    TEST_PASS();
}
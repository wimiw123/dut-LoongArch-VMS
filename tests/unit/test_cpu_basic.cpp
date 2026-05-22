#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

/*
 * 运行判定说明（CPU 基础执行模块）：
 * 1) 先看到 [INFO] 中的初始化/写指令/step 前后状态日志；
 * 2) 再看到 [CHECK] 断言 r1=42 且 PC=0x1004；
 * 3) 最后一行 [PASS]。
 * 原因：说明 CPU 能正确取指并执行 ADDI.W，且顺序流控制（PC+4）正确。
 */
int main() {
    TEST_INFO("[CPU_BASIC] 初始化总线/内存/CPU");
    loongarch::Memory mem(16 * 1024 * 1024);
    loongarch::Uart uart;
    loongarch::Timer timer;
    loongarch::TestDevice testDevice;
    loongarch::Bus bus(mem, uart, timer, testDevice);
    loongarch::CPU cpu(bus);

    mem.write32(0x1000, 0x0280A801);
    TEST_INFO("[CPU_BASIC] 写入指令 ADDI.W r1, r0, 42 到 0x1000");

    cpu.setPC(0x1000);
    TEST_INFO("[CPU_BASIC] step 前: PC=0x" << std::hex << cpu.getPC() << std::dec);
    cpu.step();
    TEST_INFO("[CPU_BASIC] step 后: PC=0x" << std::hex << cpu.getPC() << std::dec
              << " r1=" << cpu.registers()[1]);

    EXPECT_EQ(cpu.registers()[1], 42u);
    EXPECT_EQ(cpu.getPC(), 0x1004u);

    TEST_PASS();
}

/*验证程序是否能写TestDevice，并退出*/
#include "Bus.h"
#include "CPU.h"
#include "Memory.h"
#include "TestDevice.h"
#include "Timer.h"
#include "Uart.h"
#include "test_framework.h"

/*
 * 运行判定说明（Runtime 退出路径模块）：
 * 1) [CHECK] 显示在 MAX_STEPS 内 halted=true；
 * 2) [CHECK] 显示 exitCode=0；
 * 3) [CHECK] 显示关键寄存器值 r1=0x1FFFF000、r2=0；
 * 4) 最后一行 [PASS]。
 * 原因：说明程序成功执行到写 TestDevice 的退出路径，且写入数据正确。
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
    constexpr std::uint64_t MAX_STEPS = 8;

    cpu.reset(ENTRY);
    testDevice.reset();

    mem.write32(ENTRY + 0x0, 0x143FFFE1u); // lu12i.w：将高 20 位立即数写入 r1
    mem.write32(ENTRY + 0x4, 0x02800002u); // addi.w：r2 = r0 + 0
    mem.write32(ENTRY + 0x8, 0x29800022u); // st.w：将 r2 写入 [r1+0]（触发 TestDevice 退出）

    bool halted = false;
    for (std::uint64_t i = 0; i < MAX_STEPS; ++i) {
        cpu.step();
        if (testDevice.halted()) {
            halted = true;
            break;
        }
    }

    EXPECT_TRUE(halted);
    EXPECT_EQ(testDevice.exitCode(), 0u);
    EXPECT_EQ(cpu.getReg(1), 0x1FFFF000u);
    EXPECT_EQ(cpu.getReg(2), 0u);

    TEST_PASS();
}
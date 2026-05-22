#include "TestDevice.h"
#include "test_framework.h"

/*
 * 运行判定说明（TestDevice 退出协议模块）：
 * 1) [CHECK] 显示初始 halted=false、exitCode=0；
 * 2) 向 BASE_ADDR 写入 7 后，[CHECK] 显示 halted=true、exitCode=7；
 * 3) 读回寄存器值与状态寄存器值正确；
 * 4) 最后一行 [PASS]。
 * 原因：证明模拟器“写设备地址触发退出码”的协议链路工作正常。
 */
using namespace loongarch;

int main() {
    TestDevice dev;

    EXPECT_EQ(dev.halted(), false);
    EXPECT_EQ(dev.exitCode(), 0u);

    dev.write32(TestDevice::BASE_ADDR, 7);

    EXPECT_EQ(dev.halted(), true);
    EXPECT_EQ(dev.exitCode(), 7u);
    EXPECT_EQ(dev.read32(TestDevice::BASE_ADDR), 7u);
    EXPECT_EQ(dev.read32(TestDevice::BASE_ADDR + 4), 1u);

    TEST_PASS();
}
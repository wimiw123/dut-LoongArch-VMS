#include "Timer.h"
#include "test_framework.h"

/*
 * 这个文件测试的是 Timer 定时器模块。
 * 重点验证的功能包括：
 * 1) TCFG/TCTL/TVAL/TCLR 等寄存器读写语义；
 * 2) Timer 在未使能时 tick 不计数；
 * 3) Timer 在达到阈值后会置位 pending 中断；
 * 4) 清中断后 pending 会被清零；
 * 5) 非法寄存器访问会抛出异常。
 *
 * 当终端输出中出现：
 * - [INFO] 中包含 tick 过程和 pending 状态变化；
 * - [CHECK] 显示寄存器值、中断状态、异常检查全部通过；
 * - 最后一行出现 [PASS] tests/unit/test_timer.cpp；
 * 就表示 Timer 模块的 MMIO 与中断行为正常。
 */
using namespace loongarch;

int main()
{
    Timer timer;

    TEST_INFO("[TIMER] 初始状态检查");
    EXPECT_EQ(timer.read32(0x00u), 0u);
    EXPECT_EQ(timer.read32(0x04u), 0u);
    EXPECT_EQ(timer.read32(0x08u), 0u);
    EXPECT_EQ(timer.pending(), false);

    TEST_INFO("[TIMER] 未使能时 tick 三次，TVAL 应保持 0");
    timer.tick();
    timer.tick();
    timer.tick();
    EXPECT_EQ(timer.read32(0x00u), 0u);
    EXPECT_EQ(timer.pending(), false);

    TEST_INFO("[TIMER] 写入阈值=4，并打开使能位");
    timer.write32(0x04u, 4u);
    timer.write32(0x08u, 1u);
    EXPECT_EQ(timer.read32(0x04u), 4u);
    EXPECT_EQ(timer.read32(0x08u), 1u);

    for (int i = 1; i <= 4; ++i)
    {
        timer.tick();
        TEST_INFO("[TIMER] tick=" << i << " TVAL=" << timer.read32(0x00u)
                  << " pending=" << timer.pending());
    }

    EXPECT_EQ(timer.read32(0x00u), 4u);
    EXPECT_TRUE(timer.pending());

    TEST_INFO("[TIMER] 通过写 TCLR 清除 pending");
    timer.write32(0x0Cu, 1u);
    EXPECT_EQ(timer.pending(), false);
    EXPECT_EQ(timer.read32(0x0Cu), 0u);

    TEST_INFO("[TIMER] 向只读 TVAL 写值应被忽略");
    timer.write32(0x00u, 999u);
    EXPECT_EQ(timer.read32(0x00u), 4u);

    TEST_INFO("[TIMER] 检查非法偏移访问异常");
    EXPECT_THROW((void)timer.read32(0x10u));
    EXPECT_THROW(timer.write32(0x10u, 0x1u));

    TEST_PASS();
}

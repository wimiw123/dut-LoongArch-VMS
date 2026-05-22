#include "Memory.h"
#include "test_framework.h"

/*
 * 运行判定说明（Memory 模块）：
 * 1) 终端出现多条 [CHECK]，并覆盖以下断言：
 *    - 对齐地址读写后值一致；
 *    - 越界访问会触发异常；
 *    - 非对齐访问会触发异常。
 * 2) 最后一行出现 [PASS] tests/unit/test_memory.cpp。
 * 原因：上述输出分别验证了内存模块最核心的正确性（功能正确 + 保护机制正确）。
 */
int main() {
    loongarch::Memory mem(1024);

    mem.write32(0, 0x12345678);
    EXPECT_EQ(mem.read32(0), 0x12345678u);

    mem.write32(4, 0xAABBCCDD);
    EXPECT_EQ(mem.read32(4), 0xAABBCCDDu);

    EXPECT_THROW((void)mem.read32(1024));
    EXPECT_THROW(mem.write32(1024, 1));

    EXPECT_THROW((void)mem.read32(2));
    EXPECT_THROW(mem.write32(2, 0x55));

    TEST_PASS();
}
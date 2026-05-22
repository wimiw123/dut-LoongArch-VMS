#include "Memory.h"
#include "ProgramLoader.h"
#include "test_framework.h"

/*
 * 运行判定说明（HEX ProgramLoader 模块）：
 * 1) [CHECK] 显示 loaded==3；
 * 2) [CHECK] 显示入口后 3 条机器码与期望值一致；
 * 3) 最后一行 [PASS]。
 * 原因：证明 HEX 解析与按地址写入内存逻辑正确。
 */
using namespace loongarch;

int main() {
    Memory mem(16 * 1024 * 1024);
    ProgramLoader loader(mem);

    constexpr std::uint32_t ENTRY = 0x1000;

    std::uint32_t loaded = loader.loadHexFile("../programs/test_exit.hex", ENTRY);

    EXPECT_EQ(loaded, 3u);
    EXPECT_EQ(mem.read32(ENTRY + 0x0), 0x143FFFE1u);
    EXPECT_EQ(mem.read32(ENTRY + 0x4), 0x02800002u);
    EXPECT_EQ(mem.read32(ENTRY + 0x8), 0x29800022u);

    TEST_PASS();
}
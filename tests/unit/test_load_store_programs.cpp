#include "SimulatorRunner.h"
#include "test_framework.h"

/*
 * 运行判定说明（Load/Store 程序模块）：
 * 1) [SIM] 输出中可见逐步 PC/寄存器/cycle/halted 变化；
 * 2) [INFO] 汇总应显示 pass 样例 exit=0、fail 样例 exit=4；
 * 3) [CHECK] 断言 loaded/halted/exit_code 全部通过；
 * 4) 最后一行 [PASS]。
 * 原因：说明访存指令路径可正确执行并得到可判定的退出码。
 */
using namespace loongarch;

int main() {
    TEST_INFO("[LOAD_STORE] 开始执行 load/store 程序组测试");
    {
        const RunResult result =
            runHexProgram("../programs/load_store_pass.hex", 0x1000, 32, true);

        TEST_INFO("[LOAD_STORE] pass 程序结果: loaded=" << result.loaded
                  << " halted=" << result.halted << " exit=" << result.exit_code
                  << " steps=" << result.steps);

        EXPECT_TRUE(result.loaded);
        EXPECT_TRUE(result.halted);
        EXPECT_EQ(result.exit_code, 0u);
    }

    {
        const RunResult result =
            runHexProgram("../programs/load_store_fail.hex", 0x1000, 32, true);

        TEST_INFO("[LOAD_STORE] fail 程序结果: loaded=" << result.loaded
                  << " halted=" << result.halted << " exit=" << result.exit_code
                  << " steps=" << result.steps);

        EXPECT_TRUE(result.loaded);
        EXPECT_TRUE(result.halted);
        EXPECT_EQ(result.exit_code, 4u);
    }

    TEST_PASS();
}

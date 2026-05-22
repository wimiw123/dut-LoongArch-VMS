#include "SimulatorRunner.h"
#include "test_framework.h"

/*
 * 运行判定说明（数组程序模块）：
 * 1) [SIM] 输出中可见数组程序执行过程（多 step 状态变化）；
 * 2) [INFO] 汇总应显示 pass 样例 exit=0、fail 样例 exit=5；
 * 3) [CHECK] 断言 loaded/halted/exit_code 全部通过；
 * 4) 最后一行 [PASS]。
 * 原因：说明数组相关访存与控制流组合逻辑可被正确验证。
 */
using namespace loongarch;

int main() {
    TEST_INFO("[ARRAY] 开始数组相关程序测试");
    {
        const RunResult result =
            runHexProgram("../programs/array_pass.hex", 0x1000, 64, true);

        TEST_INFO("[ARRAY] pass 程序结果: loaded=" << result.loaded
                  << " halted=" << result.halted << " exit=" << result.exit_code
                  << " steps=" << result.steps);

        EXPECT_TRUE(result.loaded);
        EXPECT_TRUE(result.halted);
        EXPECT_EQ(result.exit_code, 0u);
    }

    {
        const RunResult result =
            runHexProgram("../programs/array_fail.hex", 0x1000, 64, true);

        TEST_INFO("[ARRAY] fail 程序结果: loaded=" << result.loaded
                  << " halted=" << result.halted << " exit=" << result.exit_code
                  << " steps=" << result.steps);

        EXPECT_TRUE(result.loaded);
        EXPECT_TRUE(result.halted);
        EXPECT_EQ(result.exit_code, 5u);
    }

    TEST_PASS();
}

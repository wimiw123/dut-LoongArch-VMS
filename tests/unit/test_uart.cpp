#include "Uart.h"
#include "test_framework.h"

#include <iostream>
#include <sstream>
#include <string>

/*
 * 这个文件测试的是 Uart 串口模块。
 * 重点验证的功能包括：
 * 1) 写 offset 0 时，最低 8 位字符会输出到标准输出；
 * 2) 对齐的其他 offset 目前也会按当前实现输出字符；
 * 3) read32 当前总是返回 0；
 * 4) 连续写时字符顺序保持正确。
 *
 * 当终端输出中出现：
 * - [INFO] 打印捕获到的 UART 输出字符串；
 * - [CHECK] 验证读取值与输出内容全部通过；
 * - 最后一行出现 [PASS] tests/unit/test_uart.cpp；
 * 就表示 Uart 模块的当前输出语义正常运行。
 */
using namespace loongarch;

int main()
{
    Uart uart;
    std::ostringstream captured;

    TEST_INFO("[UART] 捕获标准输出，连续写入字符 'H'、'i'、'!' ");
    std::streambuf *old_buf = std::cout.rdbuf(captured.rdbuf());
    uart.write32(0x0u, static_cast<std::uint32_t>('H'));
    uart.write32(0x0u, static_cast<std::uint32_t>('i'));
    uart.write32(0x4u, static_cast<std::uint32_t>('!'));
    std::cout.rdbuf(old_buf);

    TEST_INFO("[UART] 捕获到的输出=\"" << captured.str() << "\"");
    EXPECT_EQ(captured.str(), std::string("Hi!"));

    TEST_INFO("[UART] read32 在当前实现中总返回 0");
    EXPECT_EQ(uart.read32(0x0u), 0u);
    EXPECT_EQ(uart.read32(0x100u), 0u);

    TEST_PASS();
}

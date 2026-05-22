#pragma once // 确保文件只被包含一次，防止重复定义错误

#include "Memory.h" // 引入自己写的模拟内存类，因为加载程序就是把代码塞进内存里
#include <cstdint>  // 引入标准位宽整数类型
#include <string>   // 引入 C++ 的 std::string 字符串类，用于处理文件路径

namespace loongarch
{

/**
 * @brief 程序加载器类
 * 
 * 它的作用就像是用光盘装系统，负责打开外部编译好的程序文件（.bin 或者 .hex），
 * 并把里面的指令数据原封不动地搬运到我们的模拟内存（Memory）特定地址中。
 */
class ProgramLoader
{
  public:
    /**
     * @brief 构造函数：创建一个给这块特定内存搬运程序的装载工。
     * @param memory 目标物理内存的引用。
     */
    explicit ProgramLoader(Memory &memory) noexcept;

    /**
     * @brief 加载十六进制文本文件（HEX 文件）。
     * HEX 文件里是人类可读的 ASCII 字符，每行包含内存地址和它的机器码。
     * @param path 文件的路径
     * @param load_addr 加载的起始物理地址
     * @return 返回程序的入口点（程序的首条指令应该从哪里开始执行）
     */
    std::uint32_t loadHexFile(const std::string &path, std::uint32_t load_addr);

    /**
     * @brief 加载纯二进制文件（BIN 文件）。
     * BIN 文件全是机器直接认的 01 数据流水，没有任何格式头，直接一古脑倒进内存。
     * @param path 文件的路径
     * @param load_addr 加载的起始物理地址
     * @return 返回总共成功加载了多少个字节
     */
    std::size_t loadBinFile(const std::string &path, std::uint32_t load_addr);

    /**
     * @brief 自动推断文件类型并加载。
     * 看扩展名是 .hex 还是 .bin 自动选择调用上面两个方法。
     * @param path 文件的路径
     * @param load_addr 加载的首地址
     * @return 返回加载的字节数或类似结果标记
     */
    std::size_t loadFileAuto(const std::string &path, std::uint32_t load_addr);

  private:
    Memory &m_memory; // 记录它所负责搬砖的目标内存的引用
};

} // namespace loongarch
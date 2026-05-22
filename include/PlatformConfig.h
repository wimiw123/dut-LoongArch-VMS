#pragma once // 确保文件全局只包含一次

#include <cstddef>
#include <cstdint>

namespace loongarch
{

/**
 * @brief 整个模拟硬件平台的全局地址配置表和基础设定（Platform Config）。
 *
 * 这里集中规定了我们的龙芯“小电脑”里，各个重要组件在内存里到底坐落在什么门牌号上。
 * 这就像一张城市的地图指南，大家都根据这里的数字去寻找想去的地方。
 */
struct PlatformConfig
{
    // ENTRY: 入口地址（可执行程序代码最初该被放在内存的哪里？CPU上电后从哪儿开始跑？）。默认是十六进制的 0x1000。
    static constexpr std::uint32_t ENTRY = 0x1000;
    
    // DATA_BASE: 数据段基址（全局变量之类的数据一般存放在从此开始的内存段）。
    static constexpr std::uint32_t DATA_BASE = 0x2000;
    
    // STACK_TOP: 栈顶指针。模拟器给程序提供的最高初始栈空间地址（一般程序的栈都是从高地址向低地址向下生长的）。
    static constexpr std::uint32_t STACK_TOP = 0xF000;
    
    // TEST_MMIO: 一块模拟出来专门用作“硬件测试”功能的外设设备的监控映射门牌号。
    // 程序只要向这个内存地址写点东西，其实等同于控制一个并不存在的测试设备输出状态。
    static constexpr std::uint32_t TEST_MMIO = 0x1FFFF000;
    
    // MEMORY_SIZE: 我们给这个小系统设定的默认主板“物理内存”条究竟有多大容量？
    // 这里设为了 16 * 1024 * 1024 字节，算一算其实就是整整 16 MB。
    static constexpr std::size_t MEMORY_SIZE = 16 * 1024 * 1024;
    
    // MAX_STEPS: 测试或者执行模拟时的一个默认保险丝/天花板阈值步数。
    // 某些 -O0 编译出来的 C 程序（例如带 64 位软件取模的样例）指令数会明显更多，
    // 因此这里给出更宽松的预算，避免把本来能正常 goodtrap 的程序误判成死循环。
    static constexpr std::uint64_t MAX_STEPS = 1000000;
};

} // namespace loongarch

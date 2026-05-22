/**
 * @file Bus.h
 * @brief 简单的系统总线（System Bus），用于连接内存（Memory）和串口设备（UART）。
 *
 * 这是一个类定义文件。总线（Bus）实现了Device接口，
 * 它的主要作用是根据物理地址，将32位（4字节）的数据读写请求
 * 路由（分配）到主内存（Memory）或者串口（UART）中。
 * 就像现实中的十字路口交警，决定数据该去哪里。
 */

#pragma once // 确保这个头文件在编译时只被包含（引入）一次，防止重复定义错误

// 引入其他相关设备的头文件，因为总线需要知道这些设备的外貌和功能
#include "Device.h"      // 基础设备接口类，是所有设备的父类
#include "Memory.h"      // 内存类
#include "Timer.h"       // 定时器类
#include "Uart.h"        // 串口类，主要用于控制台输入输出

#include <TestDevice.h>  // 测试用的虚拟设备类
#include <cstdint>       // 引入定宽无符号整数类型（比如 uint32_t，固定为32位（4字节），非常规范）

namespace loongarch // 将代码放在 loongarch（龙芯） 命名空间下，防止与别的库发生名字冲突
{

/**
 * @brief 最简化的系统总线实现。
 *
 * 地址映射表（Address map，说明地址是怎么分布的）：
 * - 当要求访问的地址在范围 [0, memory.size()) 之间时，直接将请求发给主内存（Memory）。
 * - 当要求访问的地址在范围 [Uart::PhysicalBase, Uart::PhysicalBase + Uart::RangeSizeBytes)
 *   之间时，这代表我们需要与物理串口（UART）打交道。
 *
 * 如果访问任何不属于上面两种地址的其他地址，程序会抛出异常（std::runtime_error），报内存访问错误。
 */
class Bus final : public Device // Bus 类公开地继承自 Device 基础设备类。final 表示这个类是最终版本，不能再被谁继承了
{
  public:
    // 构造函数：初始化总线。它需要其他四个核心模块的引用（绑定）
    // noexcept 关键字代表：这是一个安全的函数，保证绝对不会在初始化时抛出异常而崩溃
    Bus(Memory &memory, Uart &uart, Timer &timer, TestDevice &testDevice) noexcept;
    
    // 析构函数：在总线对象被销毁时触发
    // override 表示重写了父类 Device 中定义的虚析构函数。default 代表让编译器帮我们自动生成清理代码就好。
    ~Bus() override = default;

    // 以下是 C++ 的一种安全机制：直接删除拷贝构造函数和等号赋值！
    // 为什么呢？因为系统里只有一个真实的 Bus，相当于单例对象，复制出一模一样的 Bus 是极其荒唐且危险的。
    Bus(const Bus &) = delete;
    Bus &operator=(const Bus &) = delete;
    
    // 但是，总线可以“移动”它的控制权（即现代 C++ 中的移动语义）
    Bus(Bus &&) = default;
    Bus &operator=(Bus &&) = default;

    // 重写父类 Device 的核心方法读（read）：从指定内存或串口地址 addr 读取 32 位（4字节）的数据
    // [[nodiscard]] 这个特性会警告粗心的程序员：调用了这个方法它会返回值，不要直接扔掉这个值！
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;
    
    // 重写父类 Device 的核心方法写（write）：将 32 位的值 value 写入指定的内存或串口地址 addr
    void write32(std::uint32_t addr, std::uint32_t value) override;

  private:
    // 以下私有成员变量是总线需要连接管理的四大设备。
    // 我们使用了引用（Type &x）修饰符，说明总线不需要自己去创建它们，
    // 而是在外面建好之后直接给总线一个门牌号就行。这样可以节省内存空间，同时也方便其他部分与它们通信。
    Memory     &m_memory;       // 主内存引用（m_开头的通常代表 member instance, 私有成员变量的命名规范）
    Uart       &m_uart;         // 串口引用
    Timer      &m_timer;        // 定时器引用
    TestDevice &m_test_device;  // 测试设备引用
};

} // namespace loongarch 结束

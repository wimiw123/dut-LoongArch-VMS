/**
 * @file Uart.h
 * @brief 极简版本的内存映射串口外设（UART）。
 *
 * UART（通用异步收发传输器）是计算机向终端（屏幕）输出文字的基础硬件之一。
 * 这个文件把它抽象封装成了一个符合 Device 接口的 32位 设备。
 * 只要 CPU 向它的“发送寄存器”写数据，我们就会截取最低的 8个 bit 用作 ASCII 字符，
 * 并通过 C++ 的 std::cout 打印到你的电脑屏幕上。
 */

#pragma once

#include "Device.h" // 继承自基础设备接口 Device

#include <cstdint>

namespace loongarch
{

/**
 * @brief 为纯控制台输出量身定制的最微型的 UART 串口实现。
 *
 * 设计逻辑：
 * 这个模拟出来的串口其实也是寄居在系统的总线上。它有一个自己专属的固定物理大门牌号
 * （例如 0x1FE001E0）。但这个类自身不用管天下事，它只需要管理当别人敲它的门时，
 * 给出的相对地址（偏移量 offsets）该怎么回应。
 *
 * 当前版本中，我们模拟地非常简单：只在偏移 0 的位置设了一个“只用来发射字符”的寄存器：
 * - write32(0, value): 提取出传入数据最低的 8 个二进制位拼成一个英文字母（ASCII），并打印出来。
 * - read32(0): 我们现在还没实现键盘输入呢，所以如果它来读，永远只回馈 0 返回。
 */
class Uart final : public Device
{
  public:
    Uart() = default;
    ~Uart() override = default;

    // 防止被克隆，保证全球唯一的串口对象
    Uart(const Uart &) = delete;
    Uart &operator=(const Uart &) = delete;
    Uart(Uart &&) = default;
    Uart &operator=(Uart &&) = default;

    /// \brief 这个 UART 串口在整个计算机全局内存映射地图上的固定物理门牌基址。
    /// （这里使用了一个现实机器中相对真实的比较高的高位 MMIO 地址）。
    static constexpr std::uint32_t PhysicalBase = 0x1FE0'01E0u;
    
    /// \brief 这台极简版 UART 模块总共需要占用几个字节的地盘？答：只需要 4个字节。
    static constexpr std::uint32_t RangeSizeBytes = 4u;

    /// @copydoc Device::read32
    /// \brief 从 UART 寄存器空间读取 32 位的值（按照目前缩水功能，总是返回 0）。
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;

    /// @copydoc Device::write32
    /// \brief 向 UART 寄存器空间写入 32 位值（即向偏移地址为 0 的发射寄存器写数据打印）。
    void write32(std::uint32_t addr, std::uint32_t value) override;
};

} // namespace loongarch

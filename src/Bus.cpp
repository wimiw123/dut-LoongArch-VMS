/**
 * @file Bus.cpp
 * @brief 简单的系统总线（System Bus）功能实现文件。
 */

#include "Bus.h"
#include "Timer.h"

#include <stdexcept>
#include <string>

namespace loongarch
{

// 构造函数：总线初始化时把这几个外设小弟（内存、串口、定时器、测试板）牵过来保管好。
Bus::Bus(Memory &memory, Uart &uart, Timer &timer, TestDevice &testDevice) noexcept
    : m_memory{memory}, m_uart{uart}, m_timer{timer}, m_test_device{testDevice}
{
}

// 总线核心方法：读数据路由（交警指路）
std::uint32_t Bus::read32(std::uint32_t addr)
{
    // 1号路线：定时器设备范围 (如果地址正好在定时器家门口)
    if (addr >= Timer::PhysicalBase && addr < (Timer::PhysicalBase + Timer::RangeSizeBytes))
    {
        // 算出相对偏移量交给门卫
        const std::uint32_t offset = addr - Timer::PhysicalBase;
        return m_timer.read32(offset); // 定时器，请把你要反馈的数据给我！
    }

    // 2号路线：串口设备 UART
    if (addr >= Uart::PhysicalBase && addr < (Uart::PhysicalBase + Uart::RangeSizeBytes))
    {
        const std::uint32_t offset = addr - Uart::PhysicalBase;
        return m_uart.read32(offset);
    }

    // 3号路线：主流大头内存 Memory。这个简单，只要你请求的地皮比它自己最大的地盘小，就可以去里面找。
    if (addr < m_memory.size())
    {
        return m_memory.read32(addr);
    }

    // 4号路线：我们的测试用外设黑盒子
    if (addr >= TestDevice::BASE_ADDR && addr < TestDevice::BASE_ADDR + TestDevice::SIZE)
    {
        return m_test_device.read32(addr);
    }

    // 除了以上 4 处，哪都不认识。如果访问到了不存在的死胡同荒地，直接报错掀桌！
    throw std::runtime_error("Bus: unmapped read32 at address 0x" +
                             std::to_string(static_cast<unsigned long long>(addr)));
}

// 总线核心方法：写数据路由
void Bus::write32(std::uint32_t addr, std::uint32_t value)
{
    // 写给定时器
    if (addr >= Timer::PhysicalBase && addr < (Timer::PhysicalBase + Timer::RangeSizeBytes))
    {
        const std::uint32_t offset = addr - Timer::PhysicalBase;
        m_timer.write32(offset, value);
        return;
    }

    // 写给串口（相当于打印出屏幕文字）
    if (addr >= Uart::PhysicalBase && addr < (Uart::PhysicalBase + Uart::RangeSizeBytes))
    {
        const std::uint32_t offset = addr - Uart::PhysicalBase;
        m_uart.write32(offset, value);
        return;
    }

    // 普普通通地写进总内存大池子里
    if (addr < m_memory.size())
    {
        m_memory.write32(addr, value);
        return;
    }

    // 写给测试设备
    if (addr >= TestDevice::BASE_ADDR && addr < TestDevice::BASE_ADDR + TestDevice::SIZE)
    {
        m_test_device.write32(addr, value);
        return;
    }

    // 写到了一个谁家也不是的地方
    throw std::runtime_error("Bus: unmapped write32 at address 0x" +
                             std::to_string(static_cast<unsigned long long>(addr)));
}

} // namespace loongarch

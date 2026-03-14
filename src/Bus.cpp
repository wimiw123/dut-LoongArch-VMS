/**
 * @file Bus.cpp
 * @brief Simple system bus for memory and UART devices.
 */

#include "Bus.h"
#include "Timer.h"

#include <stdexcept>
#include <string>

namespace loongarch
{

Bus::Bus(Memory& memory, Uart& uart, Timer& timer) noexcept
    : m_memory{memory}
    , m_uart{uart}
    , m_timer{timer}
{
}

std::uint32_t Bus::read32(std::uint32_t addr)
{
    // Route Timer MMIO range.
    if (addr >= Timer::PhysicalBase &&
        addr < (Timer::PhysicalBase + Timer::RangeSizeBytes)) {
        const std::uint32_t offset = addr - Timer::PhysicalBase;
        return m_timer.read32(offset);
    }

    // Route UART MMIO range.
    if (addr >= Uart::PhysicalBase &&
        addr < (Uart::PhysicalBase + Uart::RangeSizeBytes)) {
        const std::uint32_t offset = addr - Uart::PhysicalBase;
        return m_uart.read32(offset);
    }

    // Route to main memory if within range.
    if (addr < m_memory.size()) {
        return m_memory.read32(addr);
    }

    throw std::runtime_error(
        "Bus: unmapped read32 at address 0x" +
        std::to_string(static_cast<unsigned long long>(addr)));
}

void Bus::write32(std::uint32_t addr, std::uint32_t value)
{
    // Route Timer MMIO range.
    if (addr >= Timer::PhysicalBase &&
        addr < (Timer::PhysicalBase + Timer::RangeSizeBytes)) {
        const std::uint32_t offset = addr - Timer::PhysicalBase;
        m_timer.write32(offset, value);
        return;
    }

    // Route UART MMIO range.
    if (addr >= Uart::PhysicalBase &&
        addr < (Uart::PhysicalBase + Uart::RangeSizeBytes)) {
        const std::uint32_t offset = addr - Uart::PhysicalBase;
        m_uart.write32(offset, value);
        return;
    }

    // Route to main memory if within range.
    if (addr < m_memory.size()) {
        m_memory.write32(addr, value);
        return;
    }

    throw std::runtime_error(
        "Bus: unmapped write32 at address 0x" +
        std::to_string(static_cast<unsigned long long>(addr)));
}

} // namespace loongarch


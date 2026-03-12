/**
 * @file Memory.cpp
 * @brief Implementation of the simple physical memory device.
 */

#include "Memory.h"

#include <stdexcept>
#include <string>

namespace loongarch
{

Memory::Memory(std::size_t sizeBytes)
    : m_data()
{
    if (sizeBytes == 0U) {
        throw std::runtime_error("Memory: sizeBytes must be greater than zero");
    }

    m_data.resize(sizeBytes, static_cast<std::uint8_t>(0U));
}

std::size_t Memory::size() const noexcept
{
    return m_data.size();
}

std::uint32_t Memory::read32(std::uint32_t addr)
{
    constexpr std::size_t accessSize = sizeof(std::uint32_t);
    checkAlignedAndInRange(addr, accessSize);

    const auto base = static_cast<std::size_t>(addr);

    std::uint32_t value = 0;
    value |= static_cast<std::uint32_t>(m_data[base]);
    value |= static_cast<std::uint32_t>(m_data[base + 1U]) << 8U;
    value |= static_cast<std::uint32_t>(m_data[base + 2U]) << 16U;
    value |= static_cast<std::uint32_t>(m_data[base + 3U]) << 24U;

    return value;
}

void Memory::write32(std::uint32_t addr, std::uint32_t value)
{
    constexpr std::size_t accessSize = sizeof(std::uint32_t);
    checkAlignedAndInRange(addr, accessSize);

    const auto base = static_cast<std::size_t>(addr);

    m_data[base] = static_cast<std::uint8_t>(value & 0xFFU);
    m_data[base + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    m_data[base + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    m_data[base + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void Memory::checkAlignedAndInRange(std::uint32_t addr,
                                    std::size_t accessSize) const
{
    if ((addr & 0x3U) != 0U) {
        throw std::runtime_error(
            "Memory: unaligned 32-bit access at address 0x" +
            std::to_string(static_cast<unsigned long long>(addr)));
    }

    const auto base = static_cast<std::size_t>(addr);
    if (accessSize == 0U || base > m_data.size() - accessSize) {
        throw std::runtime_error(
            "Memory: out-of-range 32-bit access at address 0x" +
            std::to_string(static_cast<unsigned long long>(addr)));
    }
}

} // namespace loongarch


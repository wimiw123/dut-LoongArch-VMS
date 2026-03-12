/**
 * @file Uart.cpp
 * @brief Simple memory-mapped UART device implementation.
 */

#include "Uart.h"

#include <iostream>

namespace loongarch
{

std::uint32_t Uart::read32(std::uint32_t /*addr*/)
{
    // For now, no readable UART state is modelled; return 0.
    return 0U;
}

void Uart::write32(std::uint32_t addr, std::uint32_t value)
{
    // We currently only model a single TX register at offset 0.
    // Writes to other offsets are ignored but could be extended later.
    if ((addr & 0x3U) == 0U) {
        const char ch = static_cast<char>(value & 0xFFU);
        std::cout << ch;
        std::cout.flush();
    }
}

} // namespace loongarch


/**
 * @file Uart.h
 * @brief Simple memory-mapped UART device.
 *
 * This UART exposes a 32-bit register interface via the Device
 * abstraction. Writes to the transmit register will print the lowest
 * byte as an ASCII character to std::cout.
 */

#pragma once

#include "Device.h"

#include <cstdint>

namespace loongarch
{

/**
 * @brief Minimal UART implementation for console output.
 *
 * The UART is assumed to be mapped at a fixed physical base address
 * (e.g. 0x1FE001E0) by the system bus. This class itself only sees
 * addresses relative to its own base (offsets) and focuses on
 * implementing the register behaviour.
 *
 * For now we model a single transmit register at offset 0:
 * - write32(0, value): print low 8 bits as an ASCII character.
 * - read32(0): always returns 0.
 */
class Uart final : public Device
{
public:
    Uart() = default;
    ~Uart() override = default;

    Uart(const Uart&) = delete;
    Uart& operator=(const Uart&) = delete;
    Uart(Uart&&) = default;
    Uart& operator=(Uart&&) = default;

    /// Fixed physical base address for this UART in the system map.
    /// This uses a more realistic high MMIO address.
    static constexpr std::uint32_t PhysicalBase = 0x1FE0'01E0u;
    /// Size of the UART address range in bytes.
    static constexpr std::uint32_t RangeSizeBytes = 4u;

    /// Read 32-bit value from UART register space (currently always 0).
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;

    /// Write 32-bit value to UART register space (TX at offset 0).
    void write32(std::uint32_t addr, std::uint32_t value) override;
};

} // namespace loongarch


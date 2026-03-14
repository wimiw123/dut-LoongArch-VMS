/**
 * @file Bus.h
 * @brief Simple system bus for memory and UART devices.
 *
 * The Bus implements the Device interface and routes 32-bit accesses
 * either to main memory or to the UART MMIO region based on the
 * physical address.
 */

#pragma once

#include "Device.h"
#include "Memory.h"
#include "Uart.h"
#include "Timer.h"

#include <cstdint>

namespace loongarch
{

/**
 * @brief Minimal system bus implementation.
 *
 * Address map:
 * - [0, memory.size()) is routed to main memory.
 * - [Uart::PhysicalBase, Uart::PhysicalBase + Uart::RangeSizeBytes)
 *   is routed to the UART device.
 *
 * Any other address currently results in std::runtime_error.
 */
class Bus final : public Device
{
public:
    Bus(Memory& memory, Uart& uart, Timer& timer) noexcept;
    ~Bus() override = default;

    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;
    Bus(Bus&&) = default;
    Bus& operator=(Bus&&) = default;

    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;
    void write32(std::uint32_t addr, std::uint32_t value) override;

private:
    Memory& m_memory;
    Uart&   m_uart;
    Timer&  m_timer;
};

} // namespace loongarch


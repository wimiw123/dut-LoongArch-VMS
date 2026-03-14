/**
 * @file Timer.h
 * @brief Memory-mapped timer device with interrupt generation.
 *
 * The Timer counts ticks driven by the main simulation loop.
 * When the tick count reaches a configurable threshold, it raises
 * a pending interrupt that the CPU can poll and acknowledge.
 *
 * MMIO register map (16 bytes, base = Timer::PhysicalBase):
 *   Offset 0x00 – TVAL (read-only)  : current tick count (low 32 bits)
 *   Offset 0x04 – TCFG (read/write) : interrupt threshold (fire every N ticks)
 *   Offset 0x08 – TCTL (read/write) : bit 0 = timer enable
 *   Offset 0x0C – TCLR (write-only) : write any value to clear pending IRQ
 */

#pragma once

#include "Device.h"

#include <cstdint>

namespace loongarch
{

class Timer final : public Device
{
public:
    Timer() = default;
    ~Timer() override = default;

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

    /// Fixed physical base address in the system MMIO map.
    static constexpr std::uint32_t PhysicalBase   = 0x1FE0'0100u;
    /// Size of the Timer address range in bytes (4 registers × 4 bytes).
    static constexpr std::uint32_t RangeSizeBytes = 16u;

    // ── MMIO interface ──────────────────────────────────────────────
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;
    void write32(std::uint32_t addr, std::uint32_t value) override;

    // ── Simulation interface ────────────────────────────────────────

    /// Advance the internal counter by one tick.
    /// If the timer is enabled and the counter reaches the threshold,
    /// the pending-interrupt flag is set.
    void tick();

    /// @return true if an interrupt is waiting to be serviced.
    [[nodiscard]] bool pending() const noexcept;

    /// Acknowledge / clear the pending interrupt.
    void clearPending() noexcept;

private:
    std::uint64_t m_ticks{0};
    std::uint32_t m_threshold{0};   // 0 = no automatic interrupt
    bool          m_enabled{false};
    bool          m_interruptPending{false};
};

} // namespace loongarch

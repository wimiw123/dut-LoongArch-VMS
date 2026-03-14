/**
 * @file Timer.cpp
 * @brief Memory-mapped timer device implementation.
 */

#include "Timer.h"

#include <stdexcept>
#include <string>

namespace loongarch
{

// ── MMIO register offsets ───────────────────────────────────────────
namespace
{
constexpr std::uint32_t REG_TVAL = 0x00u;   // read-only  : tick count
constexpr std::uint32_t REG_TCFG = 0x04u;   // read/write : threshold
constexpr std::uint32_t REG_TCTL = 0x08u;   // read/write : enable (bit 0)
constexpr std::uint32_t REG_TCLR = 0x0Cu;   // write-only : clear pending
} // namespace

std::uint32_t Timer::read32(std::uint32_t addr)
{
    switch (addr) {
    case REG_TVAL:
        return static_cast<std::uint32_t>(m_ticks & 0xFFFF'FFFFu);
    case REG_TCFG:
        return m_threshold;
    case REG_TCTL:
        return m_enabled ? 1u : 0u;
    case REG_TCLR:
        // Write-only register; reading returns 0.
        return 0u;
    default:
        throw std::runtime_error(
            "Timer: invalid read32 at offset 0x" +
            std::to_string(static_cast<unsigned long long>(addr)));
    }
}

void Timer::write32(std::uint32_t addr, std::uint32_t value)
{
    switch (addr) {
    case REG_TVAL:
        // TVAL is read-only; writes are silently ignored.
        break;
    case REG_TCFG:
        m_threshold = value;
        break;
    case REG_TCTL:
        m_enabled = (value & 1u) != 0u;
        break;
    case REG_TCLR:
        m_interruptPending = false;
        break;
    default:
        throw std::runtime_error(
            "Timer: invalid write32 at offset 0x" +
            std::to_string(static_cast<unsigned long long>(addr)));
    }
}

void Timer::tick()
{
    if (!m_enabled) {
        return;
    }

    ++m_ticks;

    // Fire an interrupt when the tick count is an exact multiple of
    // the configured threshold (and the threshold is non-zero).
    if (m_threshold != 0u && (m_ticks % m_threshold) == 0u) {
        m_interruptPending = true;
    }
}

bool Timer::pending() const noexcept
{
    return m_interruptPending;
}

void Timer::clearPending() noexcept
{
    m_interruptPending = false;
}

} // namespace loongarch

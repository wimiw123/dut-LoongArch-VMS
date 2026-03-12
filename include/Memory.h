/**
 * @file Memory.h
 * @brief Simple physical memory device implementation.
 *
 * This header declares a memory-mapped RAM device backed by a
 * contiguous byte buffer. It is intended to be used as the main
 * physical memory for the LoongArch simulator.
 */

#pragma once

#include "Device.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace loongarch
{

/**
 * @brief Simple byte-addressable physical memory device.
 *
 * The memory is backed by a @c std::vector<uint8_t> and supports
 * 32-bit little-endian aligned accesses.
 *
 * - Default size is 16 MiB, configurable at construction.
 * - 32-bit accesses must be 4-byte aligned; misaligned or out-of-range
 *   accesses will throw @c std::runtime_error.
 */
class Memory final : public Device
{
public:
    /// Default memory size in bytes (16 MiB).
    static constexpr std::size_t DefaultSizeBytes =
        static_cast<std::size_t>(16u) * 1024u * 1024u;

    /**
     * @brief Construct a memory device.
     *
     * @param sizeBytes Total size of the memory in bytes. Defaults to
     *        16 MiB. The size may be any positive value; callers are
     *        responsible for using addresses within this range.
     *
     * @throws std::runtime_error if sizeBytes is zero.
     */
    explicit Memory(std::size_t sizeBytes = DefaultSizeBytes);

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;
    Memory(Memory&&) = default;
    Memory& operator=(Memory&&) = default;

    /// @return Size of the memory in bytes.
    [[nodiscard]] std::size_t size() const noexcept;

    /// @copydoc Device::read32
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;

    /// @copydoc Device::write32
    void write32(std::uint32_t addr, std::uint32_t value) override;

private:
    std::vector<std::uint8_t> m_data;

    /// Validate that a 32-bit access is aligned and within range.
    void checkAlignedAndInRange(std::uint32_t addr,
                                std::size_t accessSize) const;
};

} // namespace loongarch


/**
 * @file Device.h
 * @brief Abstract base class for LoongArch simulator devices.
 *
 * This header defines the common 32-bit memory-mapped access interface
 * for all devices attached to the simulated bus.
 */

#pragma once

#include <cstdint>

namespace loongarch
{

/**
 * @brief Abstract base class for all memory-mapped devices.
 *
 * A device exposes a 32-bit read/write interface using guest physical
 * addresses (offsets) in the device's own address space.
 *
 * Implementations must be safe for the access patterns of the simulator
 * (alignment, bounds, etc.) and are expected to signal invalid accesses
 * via exceptions.
 */
class Device
{
public:
    Device() = default;
    virtual ~Device() = default;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;

    /**
     * @brief Read a 32-bit little-endian value from the device.
     *
     * @param addr 32-bit byte address within the device's address space.
     * @return The 32-bit value read.
     *
     * @throws std::runtime_error Implementations may throw on invalid
     *         accesses (misalignment, out-of-range, etc.).
     */
    [[nodiscard]] virtual std::uint32_t read32(std::uint32_t addr) = 0;

    /**
     * @brief Write a 32-bit little-endian value to the device.
     *
     * @param addr 32-bit byte address within the device's address space.
     * @param value The 32-bit value to write.
     *
     * @throws std::runtime_error Implementations may throw on invalid
     *         accesses (misalignment, out-of-range, etc.).
     */
    virtual void write32(std::uint32_t addr, std::uint32_t value) = 0;
};

} // namespace loongarch


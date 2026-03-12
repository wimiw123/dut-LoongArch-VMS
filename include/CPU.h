/**
 * @file CPU.h
 * @brief Basic LoongArch CPU core skeleton.
 *
 * This header declares a minimal CPU model with 32 general-purpose
 * registers and a program counter, plus a single-step execution
 * function using a classic Fetch/Decode/Execute pipeline skeleton.
 */

#pragma once

#include "Device.h"

#include <cstdint>
#include <cstddef>

namespace loongarch
{

/**
 * @brief Minimal LoongArch CPU core.
 *
 * This CPU model owns a reference to a @c Device instance (typically a
 * system bus) and maintains architectural state:
 * - 32 general-purpose registers (GPRs), each 32 bits wide.
 * - A 32-bit program counter (PC).
 *
 * By convention, @c regs[0] is hard-wired to zero; the implementation
 * ensures this invariant after each step.
 */
class CPU
{
public:
    /**
     * @brief Construct a CPU core bound to a bus device.
     *
     * @param bus The system bus handling instruction and data accesses.
     *        The caller must ensure the lifetime of @p bus exceeds that
     *        of the CPU instance.
     */
    explicit CPU(Device& bus) noexcept;

    CPU(const CPU&) = delete;
    CPU& operator=(const CPU&) = delete;
    CPU(CPU&&) = default;
    CPU& operator=(CPU&&) = default;

    /// @return Reference to the underlying bus device.
    [[nodiscard]] Device& bus() noexcept;

    /// @return Const reference to the underlying bus device.
    [[nodiscard]] const Device& bus() const noexcept;

    /// @return Current program counter value.
    [[nodiscard]] std::uint32_t getPC() const noexcept;

    /// Set the program counter to a new value.
    void setPC(std::uint32_t newPc) noexcept;

    /// @return Pointer to the internal GPR array (size 32).
    [[nodiscard]] const std::uint32_t* registers() const noexcept;

    /**
     * @brief Execute a single instruction.
     *
     * This performs a classic three-stage pipeline in a single
     * function:
     * - Fetch:  read 32-bit instruction from @c pc and increment @c pc by 4.
     * - Decode: derive opcode/operands from the raw instruction word.
     * - Execute: perform the operation, possibly reading/writing memory
     *   and registers.
     *
     * The current implementation only contains a high-level skeleton;
     * the real instruction semantics should be filled in later.
     *
     * @throws std::runtime_error if the fetch causes an invalid memory
     *         access (propagated from @c Device::read32).
     */
    void step();

private:
    Device&      m_bus;
    std::uint32_t m_regs[32]{};
    std::uint32_t m_pc{0};

    // 简化版控制状态寄存器（CSR）
    // EPC   : 发生异常时的指令地址
    // ESTAT : 异常原因码等
    // CRMD  : 当前模式/状态（这里先只作为占位）
    std::uint32_t m_epc{0};
    std::uint32_t m_estat{0};
    std::uint32_t m_crmd{0};

    /// Ensure architectural invariants (e.g., regs[0] == 0).
    void enforceInvariants() noexcept;

    /// 触发架构级异常：保存 EPC/ESTAT 并跳转到异常入口。
    void raise_exception(std::uint32_t ex_code) noexcept;
};

} // namespace loongarch

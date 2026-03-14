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

// 标志位枚举
enum class AccessType { FETCH, LOAD, STORE };

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

    // Test accessors for CSRs
    void setCRMD(std::uint32_t value) noexcept { m_crmd = value; }
    [[nodiscard]] std::uint32_t getCRMD() const noexcept { return m_crmd; }

    void setPGDL(std::uint32_t value) noexcept { m_pgdl = value; }
    [[nodiscard]] std::uint32_t getPGDL() const noexcept { return m_pgdl; }

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

    /**
     * @brief Signal an external interrupt to the CPU.
     *
     * Sets the internal pending-interrupt flag so that the next
     * call to step() will service it (if interrupts are enabled
     * in CRMD).
     *
     * @param code  Exception/interrupt code to pass to raise_exception.
     */
    void signalInterrupt(std::uint32_t code) noexcept;

private:
    Device&      m_bus;
    std::uint32_t m_regs[32]{};
    std::uint32_t m_pc{0};

    // 简化版控制状态寄存器（CSR）
    // EPC   : 发生异常时的指令地址
    // ESTAT : 异常原因码等
    // CRMD  : 当前模式/状态，bit 0 = IE（全局中断使能）
    // ECFG  : 中断使能配置（预留）
    std::uint32_t m_epc{0};
    std::uint32_t m_estat{0};
    std::uint32_t m_crmd{1};   // 默认使能中断 (IE=1)
    std::uint32_t m_ecfg{0xFFFF'FFFFu}; // 默认全部中断源使能

    // 外部中断信号
    bool          m_interrupt_pending{false};
    std::uint32_t m_interrupt_code{0};

    // 内部寄存器：一级页表物理基地址
    std::uint32_t m_pgdl{0};

    /// Ensure architectural invariants (e.g., regs[0] == 0).
    void enforceInvariants() noexcept;

    /// 触发架构级异常：保存 EPC/ESTAT 并跳转到异常入口。
    void raise_exception(std::uint32_t ex_code) noexcept;

    // 返回转换后的物理地址。如果发生缺页或权限错误，抛出特定的异常或调用 raise_exception。
    std::uint32_t translate_address(std::uint32_t vaddr, AccessType type);
};

} // namespace loongarch

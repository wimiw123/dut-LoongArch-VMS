/**
 * @file CPU.cpp
 * @brief Basic LoongArch CPU core skeleton implementation.
 */

#include "CPU.h"
#include "decoder.h"

namespace loongarch
{

namespace
{

// 异常码（示例值，可根据实际 ISA 调整）
constexpr std::uint32_t EXC_ILLEGAL_INSTR = 1u;
constexpr std::uint32_t EXC_ADDR_ERROR    = 2u;
constexpr std::uint32_t EXC_SYSCALL       = 3u;

// 辅助：安全读取 GPR
inline std::uint32_t get_reg(const std::uint32_t (&regs)[32],
                             std::uint32_t        idx) noexcept
{
    return (idx < 32u) ? regs[idx] : 0u;
}

// 辅助：安全写 GPR（允许写 x0，这里不做硬件约束，约束由 enforceInvariants 统一处理）
inline void set_reg(std::uint32_t (&regs)[32],
                    std::uint32_t idx,
                    std::uint32_t value) noexcept
{
    if (idx < 32u) {
        regs[idx] = value;
    }
}

} // namespace

CPU::CPU(Device& bus) noexcept
    : m_bus{bus}
{
    // All registers are value-initialized to 0 by the in-class initializer.
    m_pc = 0U;
    enforceInvariants();
}

Device& CPU::bus() noexcept
{
    return m_bus;
}

const Device& CPU::bus() const noexcept
{
    return m_bus;
}

std::uint32_t CPU::getPC() const noexcept
{
    return m_pc;
}

void CPU::setPC(std::uint32_t newPc) noexcept
{
    m_pc = newPc;
}

const std::uint32_t* CPU::registers() const noexcept
{
    return m_regs;
}

void CPU::step()
{
    const std::uint32_t curr_pc = m_pc;

    // ============================
    // Fetch
    // ============================
    //
    // Fetch a 32-bit instruction from memory at the current PC.
    // 若底层总线抛出异常，将在上层转换为架构级异常。
    std::uint32_t instr = 0u;
    try {
        instr = m_bus.read32(curr_pc);
    } catch (const std::runtime_error&) {
        m_pc = curr_pc; // 发生异常时 PC 指向出错指令
        raise_exception(EXC_ADDR_ERROR);
        enforceInvariants();
        return;
    }

    // Increment PC to point to the next instruction. For a simple
    // sequential flow, this is PC + 4. Branch/jump instructions in
    // the Execute stage may override this value.
    m_pc = curr_pc + 4U;

    // ============================
    // Decode
    // ============================
    // 解出通用寄存器字段
    const std::uint32_t rd = decode_rd(instr);
    const std::uint32_t rj = decode_rj(instr);
    const std::uint32_t rk = decode_rk(instr);

    // 各类 opcode 视图
    const std::uint32_t opc6  = decode_opcode6(instr);
    const std::uint32_t opc12 = decode_opcode_2ri12(instr);
    const std::uint32_t opc3  = decode_opcode_3r(instr);

    // 特殊全字编码指令：SYSCALL / ERTN（这里只实现 SYSCALL）
    // SYSCALL: 高 17 位固定为 0x002B0，其余 15 位为立即数
    if ( (instr & 0xFFFF8000u) == 0x002B0000u ) {
        // 主动触发系统调用异常
        m_pc = curr_pc + 4U;
        raise_exception(EXC_SYSCALL);
        enforceInvariants();
        return;
    }

    // ============================
    // Execute
    // ============================

    // 3R / 3R-like 指令：ADD.W / SUB.W / AND / OR / NOR / XOR / SLT / SLTU / SLL.W / SRL.W / SRA.W
    if (opc3 == OPC3_ADD_W  || opc3 == OPC3_SUB_W ||
        opc3 == OPC3_AND    || opc3 == OPC3_OR    ||
        opc3 == OPC3_XOR    || opc3 == OPC3_NOR   ||
        opc3 == OPC3_SLT    || opc3 == OPC3_SLTU  ||
        opc3 == OPC3_SLL_W  || opc3 == OPC3_SRL_W ||
        opc3 == OPC3_SRA_W) {

        const std::uint32_t lhs = get_reg(m_regs, rj);
        const std::uint32_t rhs = get_reg(m_regs, rk);

        std::uint32_t result = 0u;
        switch (opc3) {
        case OPC3_ADD_W:
            // add.w rd, rj, rk
            result = lhs + rhs;
            break;
        case OPC3_SUB_W:
            // sub.w rd, rj, rk
            result = lhs - rhs;
            break;
        case OPC3_AND:
            // and rd, rj, rk
            result = lhs & rhs;
            break;
        case OPC3_OR:
            // or rd, rj, rk
            result = lhs | rhs;
            break;
        case OPC3_XOR:
            // xor rd, rj, rk
            result = lhs ^ rhs;
            break;
        case OPC3_NOR:
            // nor rd, rj, rk  (按位或非)
            result = ~(lhs | rhs);
            break;
        case OPC3_SLT: {
            // slt rd, rj, rk  (有符号比较)
            const std::int32_t sl = static_cast<std::int32_t>(lhs);
            const std::int32_t sr = static_cast<std::int32_t>(rhs);
            result = (sl < sr) ? 1u : 0u;
            break;
        }
        case OPC3_SLTU:
            // sltu rd, rj, rk  (无符号比较)
            result = (lhs < rhs) ? 1u : 0u;
            break;
        case OPC3_SLL_W:
            // sll.w rd, rj, rk  (逻辑左移，移位量取 rk 低 5 位)
            result = lhs << (rhs & 0x1Fu);
            break;
        case OPC3_SRL_W:
            // srl.w rd, rj, rk  (逻辑右移)
            result = static_cast<std::uint32_t>(
                static_cast<std::uint32_t>(lhs) >> (rhs & 0x1Fu));
            break;
        case OPC3_SRA_W:
            // sra.w rd, rj, rk  (算术右移)
            result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(lhs) >> (rhs & 0x1Fu));
            break;
        default:
            break;
        }

        set_reg(m_regs, rd, result);
    }
    // 移位立即数指令：SLLI.W / SRLI.W / SRAI.W
    else if (opc3 == OPC3_SLLI_W ||
             opc3 == OPC3_SRLI_W ||
             opc3 == OPC3_SRAI_W) {
        const std::uint32_t src   = get_reg(m_regs, rj);
        const std::uint32_t shamt =
            static_cast<std::uint32_t>(decode_imm12(instr)) & 0x1Fu; // ui5

        std::uint32_t result = 0u;
        switch (opc3) {
        case OPC3_SLLI_W:
            // slli.w rd, rj, ui5
            result = src << shamt;
            break;
        case OPC3_SRLI_W:
            // srli.w rd, rj, ui5  (逻辑右移)
            result = static_cast<std::uint32_t>(
                static_cast<std::uint32_t>(src) >> shamt);
            break;
        case OPC3_SRAI_W:
            // srai.w rd, rj, ui5  (算术右移)
            result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(src) >> shamt);
            break;
        default:
            break;
        }

        set_reg(m_regs, rd, result);
    }
    // 2RI12 指令：ADDI.W / 逻辑立即数 / 比较立即数 / 各类访存
    else if (opc12 == OPC2_ADDI_W ||
             opc12 == OPC2_ANDI   ||
             opc12 == OPC2_ORI    ||
             opc12 == OPC2_XORI   ||
             opc12 == OPC2_SLTI   ||
             opc12 == OPC2_SLTUI  ||
             opc12 == OPC2_LD_B   ||
             opc12 == OPC2_LD_H   ||
             opc12 == OPC2_LD_W   ||
             opc12 == OPC2_LD_BU  ||
             opc12 == OPC2_LD_HU  ||
             opc12 == OPC2_ST_B   ||
             opc12 == OPC2_ST_H   ||
             opc12 == OPC2_ST_W) {

        const std::uint32_t base = get_reg(m_regs, rj);
        const std::int32_t  simm = decode_imm12(instr);      // 符号扩展立即数
        const std::uint32_t uimm = decode_uimm12(instr);     // 零扩展立即数

        switch (opc12) {
        case OPC2_ADDI_W: {
            // addi.w rd, rj, si12
            const std::int32_t sum =
                static_cast<std::int32_t>(base) + simm;
            set_reg(m_regs, rd, static_cast<std::uint32_t>(sum));
            break;
        }
        case OPC2_ANDI:
            // andi rd, rj, ui12  (零扩展)
            set_reg(m_regs, rd, base & uimm);
            break;
        case OPC2_ORI:
            // ori rd, rj, ui12
            set_reg(m_regs, rd, base | uimm);
            break;
        case OPC2_XORI:
            // xori rd, rj, ui12
            set_reg(m_regs, rd, base ^ uimm);
            break;
        case OPC2_SLTI: {
            // slti rd, rj, si12  (有符号比较)
            const std::int32_t sl = static_cast<std::int32_t>(base);
            const std::int32_t sr = simm;
            set_reg(m_regs, rd, (sl < sr) ? 1u : 0u);
            break;
        }
        case OPC2_SLTUI: {
            // sltui rd, rj, si12  (无符号比较，立即数先符号扩展再解释为无符号)
            const std::uint32_t ul =
                static_cast<std::uint32_t>(base);
            const std::uint32_t ur =
                static_cast<std::uint32_t>(simm);
            set_reg(m_regs, rd, (ul < ur) ? 1u : 0u);
            break;
        }
        case OPC2_LD_B:
        case OPC2_LD_H:
        case OPC2_LD_W:
        case OPC2_LD_BU:
        case OPC2_LD_HU: {
            // 加载类：vaddr = rj + sign_ext(si12)
            const std::uint32_t addr =
                static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(base) + simm);

            if (opc12 == OPC2_LD_W) {
                const std::uint32_t value = m_bus.read32(addr);
                set_reg(m_regs, rd, value);
            } else {
                // 使用对齐的 32 位访问，再从中抽取字节/半字
                const std::uint32_t aligned =
                    addr & ~0x3u;
                const std::uint32_t word =
                    m_bus.read32(aligned);
                const std::uint32_t byteIndex =
                    addr & 0x3u;

                if (opc12 == OPC2_LD_B ||
                    opc12 == OPC2_LD_BU) {
                    const std::uint8_t b =
                        static_cast<std::uint8_t>(
                            (word >> (byteIndex * 8u)) & 0xFFu);
                    if (opc12 == OPC2_LD_B) {
                        const std::int8_t sb =
                            static_cast<std::int8_t>(b);
                        set_reg(m_regs, rd,
                                static_cast<std::uint32_t>(sb));
                    } else {
                        set_reg(m_regs, rd,
                                static_cast<std::uint32_t>(b));
                    }
                } else { // 半字
                    // 要求半字地址 2 字节对齐；简化实现
                    const std::uint32_t halfIndex =
                        (addr & 0x2u) >> 1u; // 0 或 1
                    const std::uint16_t h =
                        static_cast<std::uint16_t>(
                            (word >> (halfIndex * 16u)) & 0xFFFFu);
                    if (opc12 == OPC2_LD_H) {
                        const std::int16_t sh =
                            static_cast<std::int16_t>(h);
                        set_reg(m_regs, rd,
                                static_cast<std::uint32_t>(sh));
                    } else {
                        set_reg(m_regs, rd,
                                static_cast<std::uint32_t>(h));
                    }
                }
            }
            break;
        }
        case OPC2_ST_B:
        case OPC2_ST_H:
        case OPC2_ST_W: {
            // 存储类：vaddr = rj + sign_ext(si12)
            const std::uint32_t addr =
                static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(base) + simm);
            const std::uint32_t value = get_reg(m_regs, rd);

            if (opc12 == OPC2_ST_W) {
                m_bus.write32(addr, value);
            } else {
                const std::uint32_t aligned =
                    addr & ~0x3u;
                const std::uint32_t byteIndex =
                    addr & 0x3u;

                std::uint32_t word =
                    m_bus.read32(aligned);

                if (opc12 == OPC2_ST_B) {
                    const std::uint32_t mask =
                        ~(0xFFu << (byteIndex * 8u));
                    const std::uint32_t ins =
                        (value & 0xFFu) << (byteIndex * 8u);
                    word = (word & mask) | ins;
                } else { // ST.H
                    // 简化：要求半字地址 2 字节对齐
                    const std::uint32_t halfIndex =
                        (addr & 0x2u) >> 1u; // 0 或 1
                    const std::uint32_t mask =
                        ~(0xFFFFu << (halfIndex * 16u));
                    const std::uint32_t ins =
                        (value & 0xFFFFu) << (halfIndex * 16u);
                    word = (word & mask) | ins;
                }

                m_bus.write32(aligned, word);
            }
            break;
        }
        default:
            break;
        }
    }
    // 1RI21 高位立即数：LU12I.W rd, si20
    else if (opc6 == OPC_LU12I_W) {
        // imm20: 假定位于 [24:5]，先作为 20 位有符号数，再左移 12 位
        const std::uint32_t raw20 =
            extract_bits(instr, 5u, 20u);
        const std::int32_t si20 =
            sign_extend<20>(raw20);

        const std::uint32_t value =
            static_cast<std::uint32_t>(si20) << 12u;

        set_reg(m_regs, rd, value);
    }
    // 1RI21 PC 相对寻址：PCADDU12I rd, si20
    else if (opc6 == OPC_PCADDU12I) {
        // rd = PC + sign_ext(si20 << 12)
        const std::uint32_t raw20 =
            extract_bits(instr, 5u, 20u);
        const std::uint32_t shifted =
            (raw20 << 12u) & 0x0FFFF'F000u;
        const std::int32_t offset =
            sign_extend<32>(shifted);
        const std::int32_t pcSigned =
            static_cast<std::int32_t>(m_pc);
        const std::int32_t sum =
            pcSigned + offset;
        set_reg(m_regs, rd,
                static_cast<std::uint32_t>(sum));
    }
    // 2RI16 分支与跳转：BEQ / BNE / BLT / BGE / JIRL / B / BL
    else if (opc6 == OPC_BEQ ||
             opc6 == OPC_BNE ||
             opc6 == OPC_BLT ||
             opc6 == OPC_BGE ||
             opc6 == OPC_JIRL ||
             opc6 == OPC_B   ||
             opc6 == OPC_BL) {

        const std::uint32_t oldPcPlus4 = m_pc;

        if (opc6 == OPC_BEQ || opc6 == OPC_BNE ||
            opc6 == OPC_BLT || opc6 == OPC_BGE ||
            opc6 == OPC_JIRL) {

            const std::uint32_t raw16 =
                extract_bits(instr, 10u, 16u);
            // 左移 2 位后按 18 位符号扩展得到字节偏移
            const std::int32_t offsetBytes =
                sign_extend<18>(raw16 << 2u);

            const std::uint32_t lhs = get_reg(m_regs, rj);
            const std::uint32_t rhs = get_reg(m_regs, rd);

            bool take = false;
            switch (opc6) {
            case OPC_BEQ:
                take = (lhs == rhs);
                break;
            case OPC_BNE:
                take = (lhs != rhs);
                break;
            case OPC_BLT: {
                const std::int32_t sl =
                    static_cast<std::int32_t>(lhs);
                const std::int32_t sr =
                    static_cast<std::int32_t>(rhs);
                take = (sl < sr);
                break;
            }
            case OPC_BGE: {
                const std::int32_t sl =
                    static_cast<std::int32_t>(lhs);
                const std::int32_t sr =
                    static_cast<std::int32_t>(rhs);
                take = (sl >= sr);
                break;
            }
            case OPC_JIRL:
                take = true;
                break;
            default:
                break;
            }

            if (take) {
                if (opc6 == OPC_JIRL) {
                    // rd = PC + 4
                    set_reg(m_regs, rd, oldPcPlus4);
                    const std::uint32_t base =
                        get_reg(m_regs, rj);
                    const std::int32_t sum =
                        static_cast<std::int32_t>(base) +
                        offsetBytes;
                    m_pc = static_cast<std::uint32_t>(sum);
                } else {
                    const std::int32_t pcSigned =
                        static_cast<std::int32_t>(m_pc);
                    const std::int32_t target =
                        pcSigned + offsetBytes;
                    m_pc = static_cast<std::uint32_t>(target);
                }
            }
        } else {
            // B / BL: I26 绝对偏移，相对 PC
            const std::uint32_t low10 =
                extract_bits(instr, 0u, 10u);
            const std::uint32_t high16 =
                extract_bits(instr, 10u, 16u);
            const std::uint32_t raw26 =
                (high16 << 10u) | low10;

            // 左移 2 位得到 28 位偏移，然后按 28 位符号扩展
            const std::uint32_t shifted =
                raw26 << 2u;
            const std::int32_t offsetBytes =
                sign_extend<28>(shifted);

            if (opc6 == OPC_BL) {
                // r1 = PC + 4
                set_reg(m_regs, 1u, oldPcPlus4);
            }

            const std::int32_t pcSigned =
                static_cast<std::int32_t>(m_pc);
            const std::int32_t target =
                pcSigned + offsetBytes;
            m_pc = static_cast<std::uint32_t>(target);
        }
    }
    else {
        // 其他指令暂未实现：触发非法指令异常
        m_pc = curr_pc + 4U;
        raise_exception(EXC_ILLEGAL_INSTR);
    }

    // Enforce architectural invariants after executing the instruction.
    enforceInvariants();
}

void CPU::enforceInvariants() noexcept
{
    // regs[0] is hard-wired to zero in many RISC architectures.
    // We ensure that no instruction can leave it with a non-zero value.
    m_regs[0] = 0U;
}

void CPU::raise_exception(std::uint32_t ex_code) noexcept
{
    // 固定异常入口基址，可根据需要增加按异常码划分偏移。
    constexpr std::uint32_t EXC_BASE = 0x1C00'0000u;
    constexpr std::uint32_t EXC_STRIDE = 0x100u;

    m_epc   = m_pc;      // 保存当前 PC
    m_estat = ex_code;   // 记录异常原因

    // 简单策略：不同异常码跳转到不同的入口偏移
    const std::uint32_t offset = ex_code * EXC_STRIDE;
    m_pc = EXC_BASE + offset;
}

} // namespace loongarch

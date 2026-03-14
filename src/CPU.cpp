/**
 * @file CPU.cpp
 * @brief Basic LoongArch CPU core skeleton implementation.
 */

#include "CPU.h"
#include "decoder.h"
#include <stdexcept>
namespace loongarch
{

namespace
{

// Exception codes (sample values, adjust per ISA)
constexpr std::uint32_t EXC_ILLEGAL_INSTR = 1u;
constexpr std::uint32_t EXC_ADDR_ERROR    = 2u;
constexpr std::uint32_t EXC_SYSCALL       = 3u;

// Hardware interrupt code (MSB=1 means interrupt, not sync exception)
constexpr std::uint32_t EXC_TIMER_INT     = 0x80u;

// Helper: safe GPR read
inline std::uint32_t get_reg(const std::uint32_t (&regs)[32],
                             std::uint32_t        idx) noexcept
{
    return (idx < 32u) ? regs[idx] : 0u;
}

// Helper: safe GPR write (x0 enforced by enforceInvariants)
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
    // ============================
    // Interrupt check (before fetch)
    // ============================
    // If an external interrupt is pending and CRMD IE bit allows,
    // skip current instruction and jump to interrupt handler entry.
    if (m_interrupt_pending && (m_crmd & 0x1u)) {
        m_interrupt_pending = false;
        raise_exception(m_interrupt_code);
        enforceInvariants();
        return;
    }

    const std::uint32_t curr_pc = m_pc;

    // ============================
    // Fetch
    // ============================
    //
    // Fetch a 32-bit instruction from memory at the current PC.
    // Bus exceptions are caught and converted to architectural exceptions.
    std::uint32_t instr = 0u;
    try {
        const std::uint32_t fetch_paddr = translate_address(curr_pc, AccessType::FETCH);
        instr = m_bus.read32(fetch_paddr);
    } catch (const std::runtime_error& /*unused*/) {
        m_pc = curr_pc; // On exception, PC points to faulting instruction
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
    // Decode register fields
    const std::uint32_t rd = decode_rd(instr);
    const std::uint32_t rj = decode_rj(instr);
    const std::uint32_t rk = decode_rk(instr);

    // Various opcode views
    const std::uint32_t opc6  = decode_opcode6(instr);
    const std::uint32_t opc12 = decode_opcode_2ri12(instr);
    const std::uint32_t opc3  = decode_opcode_3r(instr);

    // Full-word encoding: SYSCALL / ERTN (only SYSCALL implemented)
    // SYSCALL: upper 17 bits fixed 0x002B0, lower 15 bits are imm
    if ( (instr & 0xFFFF8000u) == 0x002B0000u ) {
        // Trigger syscall exception
        m_pc = curr_pc + 4U;
        raise_exception(EXC_SYSCALL);
        enforceInvariants();
        return;
    }

    // ============================
    // Execute
    // ============================

    try {

    // 3R instructions: ADD.W / SUB.W / AND / OR / NOR / XOR / SLT / SLTU / SLL.W / SRL.W / SRA.W
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
            // nor rd, rj, rk  (bitwise NOR)
            result = ~(lhs | rhs);
            break;
        case OPC3_SLT: {
            // slt rd, rj, rk  (signed compare)
            const std::int32_t sl = static_cast<std::int32_t>(lhs);
            const std::int32_t sr = static_cast<std::int32_t>(rhs);
            result = (sl < sr) ? 1u : 0u;
            break;
        }
        case OPC3_SLTU:
            // sltu rd, rj, rk  (unsigned compare)
            result = (lhs < rhs) ? 1u : 0u;
            break;
        case OPC3_SLL_W:
            // sll.w rd, rj, rk  (logical left shift, amount = rk[4:0])
            result = lhs << (rhs & 0x1Fu);
            break;
        case OPC3_SRL_W:
            // srl.w rd, rj, rk  (logical right shift)
            result = static_cast<std::uint32_t>(
                static_cast<std::uint32_t>(lhs) >> (rhs & 0x1Fu));
            break;
        case OPC3_SRA_W:
            // sra.w rd, rj, rk  (arithmetic right shift)
            result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(lhs) >> (rhs & 0x1Fu));
            break;
        default:
            break;
        }

        set_reg(m_regs, rd, result);
    }
    // Shift-immediate: SLLI.W / SRLI.W / SRAI.W
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
            // srli.w rd, rj, ui5  (logical right shift)
            result = static_cast<std::uint32_t>(
                static_cast<std::uint32_t>(src) >> shamt);
            break;
        case OPC3_SRAI_W:
            // srai.w rd, rj, ui5  (arithmetic right shift)
            result = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(src) >> shamt);
            break;
        default:
            break;
        }

        set_reg(m_regs, rd, result);
    }
    // 2RI12: ADDI.W / logical-imm / compare-imm / loads-stores
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
        const std::int32_t  simm = decode_imm12(instr);      // sign-extended immediate
        const std::uint32_t uimm = decode_uimm12(instr);     // zero-extended immediate

        switch (opc12) {
        case OPC2_ADDI_W: {
            // addi.w rd, rj, si12
            const std::int32_t sum =
                static_cast<std::int32_t>(base) + simm;
            set_reg(m_regs, rd, static_cast<std::uint32_t>(sum));
            break;
        }
        case OPC2_ANDI:
            // andi rd, rj, ui12  (zero-extended)
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
            // slti rd, rj, si12  (signed compare)
            const std::int32_t sl = static_cast<std::int32_t>(base);
            const std::int32_t sr = simm;
            set_reg(m_regs, rd, (sl < sr) ? 1u : 0u);
            break;
        }
        case OPC2_SLTUI: {
            // sltui rd, rj, si12  (unsigned compare, imm sign-extended then unsigned)
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
            // Load: vaddr = rj + sign_ext(si12)
            const std::uint32_t addr =
                static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(base) + simm);
            const std::uint32_t paddr = translate_address(addr, AccessType::LOAD);

            if (opc12 == OPC2_LD_W) {
                const std::uint32_t value = m_bus.read32(paddr);
                set_reg(m_regs, rd, value);
            } else {
                // Use aligned 32-bit access, extract byte/halfword
                const std::uint32_t aligned =
                    paddr & ~0x3u;
                const std::uint32_t word =
                    m_bus.read32(aligned);
                const std::uint32_t byteIndex =
                    paddr & 0x3u;

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
                } else { // halfword
                    // halfword addr 2-byte aligned; simplified
                    const std::uint32_t halfIndex =
                        (addr & 0x2u) >> 1u; // 0 or 1
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
            // Store: vaddr = rj + sign_ext(si12)
            const std::uint32_t addr =
                static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(base) + simm);
            const std::uint32_t value = get_reg(m_regs, rd);
            const std::uint32_t paddr = translate_address(addr, AccessType::STORE);

            if (opc12 == OPC2_ST_W) {
                m_bus.write32(paddr, value);
            } else {
                const std::uint32_t aligned =
                    paddr & ~0x3u;
                const std::uint32_t byteIndex =
                    paddr & 0x3u;

                std::uint32_t word =
                    m_bus.read32(aligned);

                if (opc12 == OPC2_ST_B) {
                    const std::uint32_t mask =
                        ~(0xFFu << (byteIndex * 8u));
                    const std::uint32_t ins =
                        (value & 0xFFu) << (byteIndex * 8u);
                    word = (word & mask) | ins;
                } else { // ST.H
                    // Simplified: halfword address must be 2-byte aligned
                    const std::uint32_t halfIndex =
                        (addr & 0x2u) >> 1u; // 0 or 1
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
    // 1RI21 upper immediate: LU12I.W rd, si20
    else if (opc6 == OPC_LU12I_W) {
        // imm20: assumed [24:5], 20-bit signed, then left-shift 12
        const std::uint32_t raw20 =
            extract_bits(instr, 5u, 20u);
        const std::int32_t si20 =
            sign_extend<20>(raw20);

        const std::uint32_t value =
            static_cast<std::uint32_t>(si20) << 12u;

        set_reg(m_regs, rd, value);
    }
    // 1RI21 PC-relative: PCADDU12I rd, si20
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
    // 2RI16 branch/jump: BEQ / BNE / BLT / BGE / JIRL / B / BL
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
            // left-shift 2, then sign-extend to 18-bit byte offset
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
            // B / BL: I26 offset relative to PC
            const std::uint32_t low10 =
                extract_bits(instr, 0u, 10u);
            const std::uint32_t high16 =
                extract_bits(instr, 10u, 16u);
            const std::uint32_t raw26 =
                (high16 << 10u) | low10;

            // left-shift 2 -> 28-bit offset, sign-extend 28
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
        // Unimplemented instruction: raise illegal instruction exception
        m_pc = curr_pc + 4U;
        raise_exception(EXC_ILLEGAL_INSTR);
    }

    } catch (const std::runtime_error& /*unused*/) {
        m_pc = curr_pc; // rollback PC to point to faulting load/store
        raise_exception(EXC_ADDR_ERROR);
        enforceInvariants();
        return;
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
    // Fixed exception handler base; offset by exception code.
    constexpr std::uint32_t EXC_BASE = 0x1C00'0000u;
    constexpr std::uint32_t EXC_STRIDE = 0x100u;

    m_epc   = m_pc;      // Save current PC
    m_estat = ex_code;   // Record exception cause

    // Simple: each exception code jumps to a different entry offset
    const std::uint32_t offset = ex_code * EXC_STRIDE;
    m_pc = EXC_BASE + offset;

    // Disable global interrupt on exception entry to prevent nesting
    m_crmd &= ~0x1u;
}

void CPU::signalInterrupt(std::uint32_t code) noexcept
{
    m_interrupt_pending = true;
    m_interrupt_code    = code;
}

std::uint32_t CPU::translate_address(std::uint32_t vaddr, AccessType type)
{
    // m_crmd bit 3 is PG (Paging) bit
    bool pg = (m_crmd & (1u << 3)) != 0;
    if (!pg) {
        return vaddr;
    }

    std::uint32_t pd_idx = (vaddr >> 22) & 0x3FFu;
    std::uint32_t pt_idx = (vaddr >> 12) & 0x3FFu;
    std::uint32_t offset = vaddr & 0xFFFu;

    std::uint32_t pde_addr = (m_pgdl & 0xFFFFF000u) + (pd_idx * 4u);
    std::uint32_t pde = m_bus.read32(pde_addr);

    // Assume bit 0 is Valid bit
    if ((pde & 0x1u) == 0) {
        throw std::runtime_error("MMU PDE Invalid");
    }

    std::uint32_t pt_base = (pde & 0xFFFFF000u);
    std::uint32_t pte_addr = pt_base + (pt_idx * 4u);
    std::uint32_t pte = m_bus.read32(pte_addr);

    if ((pte & 0x1u) == 0) {
        throw std::runtime_error("MMU PTE Invalid");
    }

    std::uint32_t ppn = (pte & 0xFFFFF000u);
    return ppn | offset;
}

} // namespace loongarch

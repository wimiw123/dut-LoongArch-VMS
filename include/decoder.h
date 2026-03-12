#pragma once

#include <cstdint>

namespace loongarch
{

// ==========================
// 通用位段/符号扩展工具
// ==========================

/// 提取 [lsb, lsb + width - 1] 这段位域（0 为最低位）
constexpr inline std::uint32_t extract_bits(std::uint32_t value,
                                            unsigned      lsb,
                                            unsigned      width) noexcept
{
    const std::uint32_t mask = (width == 32u)
        ? 0xFFFF'FFFFu
        : ((1u << width) - 1u);

    return (value >> lsb) & mask;
}

/// 将宽度为 Bits 的无符号值做 32 位有符号扩展
template <unsigned Bits>
constexpr inline std::int32_t sign_extend(std::uint32_t value) noexcept
{
    static_assert(Bits > 0 && Bits <= 32, "Bits must be in (0, 32]");

    const std::uint32_t mask = (Bits == 32u)
        ? 0xFFFF'FFFFu
        : ((1u << Bits) - 1u);

    value &= mask;

    if constexpr (Bits == 32u) {
        return static_cast<std::int32_t>(value);
    } else {
        const std::uint32_t sign_bit = 1u << (Bits - 1u);
        if (value & sign_bit) {
            // 负数：补齐高位 1
            value |= (~mask);
        }
        return static_cast<std::int32_t>(value);
    }
}

// ==========================
// LoongArch 指令字段提取
// （按你给定的 bit 编号）
// ==========================

/// 通用 6 位 opcode（主要用于 2RI16 / 1RI21）
constexpr inline std::uint32_t decode_opcode6(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 26u, 6u);
}

/// 2RI12 指令的 10 位 opcode（bit 31-22）
constexpr inline std::uint32_t decode_opcode_2ri12(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 22u, 10u);
}

/// 3R 指令的 17 位 opcode（bit 31-15）
constexpr inline std::uint32_t decode_opcode_3r(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 15u, 17u);
}

/// rd: 目标寄存器，bit 4:0
constexpr inline std::uint32_t decode_rd(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 0u, 5u);
}

/// rj: 源寄存器 1，bit 9:5
constexpr inline std::uint32_t decode_rj(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 5u, 5u);
}

/// rk: 源寄存器 2，bit 14:10
constexpr inline std::uint32_t decode_rk(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 10u, 5u);
}

/// ra: bit 19:15
constexpr inline std::uint32_t decode_ra(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 15u, 5u);
}

// ==========================
// 各种格式的有符号立即数
// ==========================

/// imm12: 12 位无符号立即数，位于 bit 21:10（用于 ANDI/ORI/XORI 等零扩展）
constexpr inline std::uint32_t decode_uimm12(std::uint32_t instr) noexcept
{
    return extract_bits(instr, 10u, 12u);
}

/// imm8:  8 位有符号立即数，位于 bit 17:10（2RI8）
constexpr inline std::int32_t decode_imm8(std::uint32_t instr) noexcept
{
    const std::uint32_t raw = extract_bits(instr, 10u, 8u);
    return sign_extend<8>(raw);
}

/// imm12: 12 位有符号立即数，位于 bit 21:10（2RI12）
constexpr inline std::int32_t decode_imm12(std::uint32_t instr) noexcept
{
    const std::uint32_t raw = extract_bits(instr, 10u, 12u);
    return sign_extend<12>(raw);
}

/// imm14: 14 位有符号立即数，位于 bit 23:10（2RI14）
constexpr inline std::int32_t decode_imm14(std::uint32_t instr) noexcept
{
    const std::uint32_t raw = extract_bits(instr, 10u, 14u);
    return sign_extend<14>(raw);
}

/// imm16: 16 位有符号立即数，位于 bit 25:10（2RI16）
constexpr inline std::int32_t decode_imm16(std::uint32_t instr) noexcept
{
    const std::uint32_t raw = extract_bits(instr, 10u, 16u);
    return sign_extend<16>(raw);
}

/// imm21: 21 位有符号立即数，格式 {inst[9:0], inst[25:10]}（1RI21）
/// 实际使用时通常还需左移 2 位（乘以 4 字节）。
constexpr inline std::int32_t decode_imm21(std::uint32_t instr) noexcept
{
    const std::uint32_t low10  = extract_bits(instr, 0u, 10u);
    const std::uint32_t high16 = extract_bits(instr, 10u, 16u);
    const std::uint32_t concat = (high16 << 10u) | low10;
    const std::uint32_t raw    = concat & ((1u << 21u) - 1u);
    return sign_extend<21>(raw);
}

/// imm26: 26 位有符号立即数，格式 {inst[9:0], inst[25:10]}（I26）
/// 实际使用时同样通常需要左移 2 位（乘以 4 字节）。
constexpr inline std::int32_t decode_imm26(std::uint32_t instr) noexcept
{
    const std::uint32_t low10  = extract_bits(instr, 0u, 10u);
    const std::uint32_t high16 = extract_bits(instr, 10u, 16u);
    const std::uint32_t concat = (high16 << 10u) | low10;
    const std::uint32_t raw    = concat & ((1u << 26u) - 1u);
    return sign_extend<26>(raw);
}

// ==========================
// 指令 opcode 常量映射
// ==========================

// [Type: 2RI16] - Opcode [31:26]
constexpr std::uint32_t OPC_JIRL = 0x13u;
constexpr std::uint32_t OPC_B    = 0x14u;
constexpr std::uint32_t OPC_BL   = 0x15u;
constexpr std::uint32_t OPC_BEQ  = 0x16u;
constexpr std::uint32_t OPC_BNE  = 0x17u;
constexpr std::uint32_t OPC_BLT  = 0x18u;
constexpr std::uint32_t OPC_BGE  = 0x19u;
constexpr std::uint32_t OPC_BLTU = 0x1Au;
constexpr std::uint32_t OPC_BGEU = 0x1Bu;

// [Type: 1RI21] - Opcode [31:26]
constexpr std::uint32_t OPC_LU12I_W = 0x05u;
constexpr std::uint32_t OPC_PCADDI  = 0x06u;

// [Type: 2RI12] - Opcode [31:22]
constexpr std::uint32_t OPC2_ADDI_W = 0x00Au;
constexpr std::uint32_t OPC2_ANDI   = 0x00Du;
constexpr std::uint32_t OPC2_ORI    = 0x00Eu;
constexpr std::uint32_t OPC2_XORI   = 0x00Fu;
constexpr std::uint32_t OPC2_SLTI   = 0x008u;
constexpr std::uint32_t OPC2_SLTUI  = 0x009u;
constexpr std::uint32_t OPC2_LD_B   = 0x0A0u;
constexpr std::uint32_t OPC2_LD_H   = 0x0A1u;
constexpr std::uint32_t OPC2_LD_W   = 0x0A2u;
constexpr std::uint32_t OPC2_ST_B   = 0x0A4u;
constexpr std::uint32_t OPC2_ST_H   = 0x0A5u;
constexpr std::uint32_t OPC2_ST_W   = 0x0A6u;
constexpr std::uint32_t OPC2_LD_BU  = 0x0A8u;
constexpr std::uint32_t OPC2_LD_HU  = 0x0A9u;

// [Type: 3R] - Opcode [31:15]
constexpr std::uint32_t OPC3_ADD_W = 0x00020u;
constexpr std::uint32_t OPC3_SUB_W = 0x00022u;
constexpr std::uint32_t OPC3_SLL_W = 0x0002Eu;
constexpr std::uint32_t OPC3_SRL_W = 0x0002Fu;
constexpr std::uint32_t OPC3_SRA_W = 0x00030u;
constexpr std::uint32_t OPC3_AND   = 0x00029u;
constexpr std::uint32_t OPC3_OR    = 0x0002Au;
constexpr std::uint32_t OPC3_XOR   = 0x0002Bu;
constexpr std::uint32_t OPC3_SLT   = 0x00024u;
constexpr std::uint32_t OPC3_SLTU  = 0x00025u;

// Supplementary 3R / shift-immediate style opcodes [31:15]
constexpr std::uint32_t OPC3_SLLI_W = 0x00001u;
constexpr std::uint32_t OPC3_SRLI_W = 0x00004u;
constexpr std::uint32_t OPC3_SRAI_W = 0x00008u;

// NOR opcode (按位或非)，使用单独 3R 编码
constexpr std::uint32_t OPC3_NOR   = 0x00028u;

// PC 相对寻址类（PCADDU12I），Opcode [31:26] = 0x0E
constexpr std::uint32_t OPC_PCADDU12I = 0x0Eu;

} // namespace loongarch

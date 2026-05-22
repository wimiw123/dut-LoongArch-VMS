#pragma once

#include <cstdint>

namespace loongarch
{

// ==========================
// 通用位段/符号扩展工具 (Bit manipulation tools)
// ==========================

constexpr inline std::uint32_t extract_bits(std::uint32_t value, unsigned lsb, unsigned width) noexcept
{
    const std::uint32_t mask = (width == 32u) ? 0xFFFF'FFFFu : ((1u << width) - 1u);
    return (value >> lsb) & mask;
}

template <unsigned Bits>
constexpr inline std::int32_t sign_extend(std::uint32_t value) noexcept
{
    static_assert(Bits > 0 && Bits <= 32, "Bits must be in (0, 32]");
    const std::uint32_t mask = (Bits == 32u) ? 0xFFFF'FFFFu : ((1u << Bits) - 1u);
    value &= mask;
    if constexpr (Bits == 32u) {
        return static_cast<std::int32_t>(value);
    } else {
        const std::uint32_t sign_bit = 1u << (Bits - 1u); 
        if (value & sign_bit) {
            value |= (~mask);
        }
        return static_cast<std::int32_t>(value);
    }
}

// ==========================
// 分层操作码提取器 (Hierarchical Opcode Selectors)
// ==========================

constexpr inline std::uint32_t decode_op6(std::uint32_t instr) noexcept  { return extract_bits(instr, 26u, 6u); }
constexpr inline std::uint32_t decode_op7(std::uint32_t instr) noexcept  { return extract_bits(instr, 25u, 7u); }
constexpr inline std::uint32_t decode_op11(std::uint32_t instr) noexcept { return extract_bits(instr, 21u, 11u); }
constexpr inline std::uint32_t decode_op10(std::uint32_t instr) noexcept { return extract_bits(instr, 22u, 10u); }
constexpr inline std::uint32_t decode_op12(std::uint32_t instr) noexcept { return extract_bits(instr, 20u, 12u); }
constexpr inline std::uint32_t decode_op14(std::uint32_t instr) noexcept { return extract_bits(instr, 18u, 14u); }
constexpr inline std::uint32_t decode_op17(std::uint32_t instr) noexcept { return extract_bits(instr, 15u, 17u); }
constexpr inline std::uint32_t decode_op22(std::uint32_t instr) noexcept { return extract_bits(instr, 10u, 22u); }

// ==========================
// 寄存器字段提取器 (Register Field Extractors)
// ==========================

constexpr inline std::uint32_t decode_rd(std::uint32_t instr) noexcept { return extract_bits(instr, 0u, 5u); }
constexpr inline std::uint32_t decode_rj(std::uint32_t instr) noexcept { return extract_bits(instr, 5u, 5u); }
constexpr inline std::uint32_t decode_rk(std::uint32_t instr) noexcept { return extract_bits(instr, 10u, 5u); }
constexpr inline std::uint32_t decode_ra(std::uint32_t instr) noexcept { return extract_bits(instr, 15u, 5u); }

// ==========================
// 立即数提取器 (Immediate Extractors)
// ==========================

constexpr inline std::uint32_t decode_uimm12(std::uint32_t instr) noexcept { return extract_bits(instr, 10u, 12u); }
constexpr inline std::int32_t  decode_imm8(std::uint32_t instr) noexcept   { return sign_extend<8>(extract_bits(instr, 10u, 8u)); }
constexpr inline std::int32_t  decode_imm12(std::uint32_t instr) noexcept  { return sign_extend<12>(extract_bits(instr, 10u, 12u)); }
constexpr inline std::int32_t  decode_imm14(std::uint32_t instr) noexcept  { return sign_extend<14>(extract_bits(instr, 10u, 14u)); }
constexpr inline std::int32_t  decode_imm16(std::uint32_t instr) noexcept  { return sign_extend<16>(extract_bits(instr, 10u, 16u)); }
constexpr inline std::int32_t  decode_imm20(std::uint32_t instr) noexcept  { return sign_extend<20>(extract_bits(instr, 5u, 20u)); }

constexpr inline std::int32_t decode_imm21(std::uint32_t instr) noexcept
{
    const std::uint32_t low16  = extract_bits(instr, 10u, 16u);
    const std::uint32_t high5  = extract_bits(instr, 0u, 5u);
    const std::uint32_t concat = (high5 << 16u) | low16;
    return sign_extend<21>(concat & 0x1FFFFFu);
}

constexpr inline std::int32_t decode_imm26(std::uint32_t instr) noexcept
{
    const std::uint32_t low16  = extract_bits(instr, 10u, 16u);
    const std::uint32_t high10 = extract_bits(instr, 0u, 10u);
    const std::uint32_t concat = (high10 << 16u) | low16;
    return sign_extend<26>(concat & 0x3FFFFFFu);
}

// ==========================
// 操作码常量 (Opcode Constants)
// ==========================

// == OPC6: 2RI16, 1RI21, etc. ==
constexpr std::uint32_t OPC6_BNEZ = 0x11u;
constexpr std::uint32_t OPC6_BEQZ = 0x10u;
constexpr std::uint32_t OPC6_JIRL = 0x13u;
constexpr std::uint32_t OPC6_B    = 0x14u;
constexpr std::uint32_t OPC6_BL   = 0x15u;
constexpr std::uint32_t OPC6_BEQ  = 0x16u;
constexpr std::uint32_t OPC6_BNE  = 0x17u;
constexpr std::uint32_t OPC6_BLT  = 0x18u;
constexpr std::uint32_t OPC6_BGE  = 0x19u;
constexpr std::uint32_t OPC6_BLTU = 0x1Au;
constexpr std::uint32_t OPC6_BGEU = 0x1Bu;
constexpr std::uint32_t OPC6_BC   = 0x12u;

// == OPC7: 1RI20, etc. ==
constexpr std::uint32_t OPC7_LU12I_W   = 0x0Au; // 0001010
constexpr std::uint32_t OPC7_PCADDU12I = 0x0Eu; // 0001110
constexpr std::uint32_t OPC7_PCADDI    = 0x0Cu; // 0001100

// == OPC10: 2RI12, etc. ==
constexpr std::uint32_t OPC10_ADDI_W = 0x00Au;
constexpr std::uint32_t OPC2_ADDI_W  = OPC10_ADDI_W;
constexpr std::uint32_t OPC10_ANDI   = 0x00Du;
constexpr std::uint32_t OPC10_ORI    = 0x00Eu;
constexpr std::uint32_t OPC10_XORI   = 0x00Fu;
constexpr std::uint32_t OPC10_SLTI   = 0x008u;
constexpr std::uint32_t OPC10_SLTUI  = 0x009u;
constexpr std::uint32_t OPC10_LD_B   = 0x0A0u;
constexpr std::uint32_t OPC10_LD_H   = 0x0A1u;
constexpr std::uint32_t OPC10_LD_W   = 0x0A2u;
constexpr std::uint32_t OPC10_ST_B   = 0x0A4u;
constexpr std::uint32_t OPC10_ST_H   = 0x0A5u;
constexpr std::uint32_t OPC10_ST_W   = 0x0A6u;
constexpr std::uint32_t OPC10_LD_BU  = 0x0A8u;
constexpr std::uint32_t OPC10_LD_HU  = 0x0A9u;

// == OPC17: 3R arithmetic, shifting ==
constexpr std::uint32_t OPC17_ADD_W  = 0x00020u;
constexpr std::uint32_t OPC17_SUB_W  = 0x00022u;
constexpr std::uint32_t OPC17_SLL_W  = 0x0002Eu;
constexpr std::uint32_t OPC17_SRL_W  = 0x0002Fu;
constexpr std::uint32_t OPC17_SRA_W  = 0x00030u;
constexpr std::uint32_t OPC17_AND    = 0x00029u;
constexpr std::uint32_t OPC17_OR     = 0x0002Au;
constexpr std::uint32_t OPC17_XOR    = 0x0002Bu;
constexpr std::uint32_t OPC17_NOR    = 0x00028u;
constexpr std::uint32_t OPC17_SLT    = 0x00024u;
constexpr std::uint32_t OPC17_SLTU   = 0x00025u;
constexpr std::uint32_t OPC17_MUL_W   = 0x00038u;
constexpr std::uint32_t OPC17_MULH_W  = 0x00039u;
constexpr std::uint32_t OPC17_MULH_WU = 0x0003Au;
constexpr std::uint32_t OPC17_DIV_W   = 0x00040u;
constexpr std::uint32_t OPC17_MOD_W   = 0x00041u;
constexpr std::uint32_t OPC17_DIV_WU  = 0x00042u;
constexpr std::uint32_t OPC17_MOD_WU  = 0x00043u;

// Shifting with 5-bit immediate
constexpr std::uint32_t OPC17_SLLI_W = 0x00081u;
constexpr std::uint32_t OPC17_SRLI_W = 0x00089u;
constexpr std::uint32_t OPC17_SRAI_W = 0x00091u;

// Misc integer helpers used by the runtime-compiled C programs.
constexpr std::uint32_t OPC11_BSTRPICK_W = 0x003u;
constexpr std::uint32_t OPC22_EXT_W_H    = 0x016u;
constexpr std::uint32_t OPC22_EXT_W_B    = 0x017u;
constexpr std::uint32_t OPC17_BREAK      = 0x00054u;

} // namespace loongarch

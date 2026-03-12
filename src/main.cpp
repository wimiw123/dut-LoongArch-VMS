/**
 * @file main.cpp
 * @brief Minimal test harness for the LoongArch CPU + UART.
 *
 * This program installs a small hand-written LoongArch32 program
 * into memory which configures a base register with the UART MMIO
 * address and then stores a single character to it. If everything
 * is wired correctly, you should see a character printed via UART
 * (std::cout).
 */

#include "Memory.h"
#include "Uart.h"
#include "Bus.h"
#include "CPU.h"
#include "decoder.h"

#include <cstdint>

using namespace loongarch;

namespace
{

// Helper to encode 2RI12 format instructions:
// [31:22] opcode (10 bits)
// [21:10] imm12  (12 bits, signed)
// [9:5]   rj
// [4:0]   rd
constexpr std::uint32_t encode_2ri12(std::uint32_t opcode10,
                                     std::uint32_t rd,
                                     std::uint32_t rj,
                                     std::int32_t  imm12) noexcept
{
    const std::uint32_t uimm = static_cast<std::uint32_t>(imm12) & 0xFFFu;
    return (opcode10 << 22) |
           (uimm << 10)     |
           ((rj & 0x1Fu) << 5) |
           (rd & 0x1Fu);
}

} // namespace

int main()
{
    Memory memory;
    Uart   uart;
    Bus    bus(memory, uart);
    CPU    cpu(bus);

    // Program layout (LoongArch32, using only ADDI.W and ST.W):
    //
    // r1 := 0x00001000 (UART base, see Uart::PhysicalBase)
    //   addi.w r1, r0, 0x400
    //   addi.w r1, r1, 0x400
    //   addi.w r1, r1, 0x400
    //   addi.w r1, r1, 0x400   ; r1 = 4 * 0x400 = 0x1000
    //
    // r2 := 'H'
    //   addi.w r2, r0, 72
    //
    // [r1 + 0] := r2
    //   st.w   r2, r1, 0
    //
    // Encodings computed according to the 2RI12 format and the
    // opcode map in decoder.h.

    constexpr std::uint32_t OPC_ADDI_W = OPC2_ADDI_W;
    constexpr std::uint32_t OPC_ST_W   = OPC2_ST_W;

    // addi.w r1, r0, 0x400
    const std::uint32_t instr0 =
        encode_2ri12(OPC_ADDI_W, /*rd=*/1u, /*rj=*/0u, /*imm12=*/0x400);
    // addi.w r1, r1, 0x400
    const std::uint32_t instr1 =
        encode_2ri12(OPC_ADDI_W, /*rd=*/1u, /*rj=*/1u, /*imm12=*/0x400);
    const std::uint32_t instr2 = instr1;
    const std::uint32_t instr3 = instr1;

    // addi.w r2, r0, 'H' (72)
    const std::uint32_t instr4 =
        encode_2ri12(OPC_ADDI_W, /*rd=*/2u, /*rj=*/0u, /*imm12=*/72);

    // st.w r2, r1, 0
    const std::uint32_t instr5 =
        encode_2ri12(OPC_ST_W, /*rd=*/2u, /*rj=*/1u, /*imm12=*/0);

    // Install program into memory starting at address 0.
    memory.write32(0x00u, instr0);
    memory.write32(0x04u, instr1);
    memory.write32(0x08u, instr2);
    memory.write32(0x0Cu, instr3);
    memory.write32(0x10u, instr4);
    memory.write32(0x14u, instr5);

    cpu.setPC(0x00u);

    // Execute a few steps; 6 are enough for this tiny program.
    for (int i = 0; i < 8; ++i) {
        cpu.step();
    }

    return 0;
}


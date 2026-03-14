/**
 * @file main.cpp
 * @brief Test harness for LoongArch CPU + MMU Paging.
 */

#include "Memory.h"
#include "Uart.h"
#include "Timer.h"
#include "Bus.h"
#include "CPU.h"
#include "decoder.h"

#include <cstdint>
#include <cstdio>
#include <iostream>

using namespace loongarch;

namespace
{

// Helper to encode 2RI12 format instructions:
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

void test_mmu()
{
    std::printf("=== LoongArch MMU / Paging Test ===\n");

    Memory memory;
    Uart   uart;
    Timer  timer;
    Bus    bus(memory, uart, timer);
    CPU    cpu(bus);

    // 1. Setup Page Tables
    // Map Virtual Address 0x12345000 to Physical Address 0x0000A000
    // VADDR: 0x12345000 -> VPN: 0x12345, Offset: 0x000
    // PD Index: VPN [19:10] = 0x12345 >> 10 = 0x048
    // PT Index: VPN [9:0]   = 0x12345 & 0x3FF = 0x345

    const std::uint32_t PD_BASE = 0x00100000u;
    const std::uint32_t PT_BASE = 0x00200000u;
    const std::uint32_t PPN     = 0x0000A000u; // Physical address base

    std::printf("[MMU] Creating Page Directory at physical 0x%08X\n", PD_BASE);
    std::printf("[MMU] Creating Page Table at physical 0x%08X\n", PT_BASE);
    std::printf("[MMU] Mapping Virtual 0x12345000 -> Physical 0x0000A000\n");

    // Write PDE at PD_BASE + PD_Idx * 4
    std::uint32_t pde_addr = PD_BASE + (0x048 * 4u);
    memory.write32(pde_addr, PT_BASE | 0x1u); // Set Valid bit (0x1)

    // Write PTE at PT_BASE + PT_Idx * 4
    std::uint32_t pte_addr = PT_BASE + (0x345 * 4u);
    memory.write32(pte_addr, PPN | 0x1u); // Set Valid bit (0x1)

    // 2. Put instructions in mapped physical memory
    // ADDI.W r1, r0, 42
    const std::uint32_t instr_addi = encode_2ri12(OPC2_ADDI_W, 1u, 0u, 42);
    memory.write32(PPN + 0, instr_addi);

    // 3. Configure CPU
    // Enable PG (bit 3) and set PGDL
    std::uint32_t crmd = cpu.getCRMD();
    crmd |= (1u << 3); // Set PG bit
    cpu.setCRMD(crmd);
    cpu.setPGDL(PD_BASE);
    cpu.setPC(0x12345000u);

    std::printf("\n[MMU] Executing mapped instruction at VADDR 0x12345000\n");
    cpu.step();

    // Check if r1 has 42
    if (cpu.registers()[1] == 42) {
        std::printf("[MMU] SUCCESS: Fetched and executed instruction correctly. r1 = %u\n", cpu.registers()[1]);
    } else {
        std::printf("[MMU] FAILURE: Expected r1 = 42, got %u\n", cpu.registers()[1]);
    }

    std::printf("[MMU] PC is now: 0x%08X\n", cpu.getPC());

    // 4. Test Page Fault
    std::printf("\n[MMU] Testing Page Fault by fetching from unmapped VADDR 0x20000000\n");
    cpu.setPC(0x20000000u);
    cpu.step();

    std::printf("[MMU] After page fault, PC is: 0x%08X\n", cpu.getPC());
    // Should jump to exception handler (0x1C000000 + EXC_ADDR_ERROR (2) * 0x100 = 0x1C000200)
    if (cpu.getPC() == 0x1C000200u) {
        std::printf("[MMU] SUCCESS: Page fault correctly routed to exception handler.\n");
    } else {
        std::printf("[MMU] FAILURE: Expected PC = 0x1C000200, got 0x%08X\n", cpu.getPC());
    }
}

int main()
{
    test_mmu();
    return 0;
}

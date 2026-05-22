/**
 * @file main.cpp
 * @brief LoongArch CPU + MMU 分页测试程序。
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

/**
 * 运行判定说明（MMU/Paging 模块）：
 * 1) 终端应出现 "SUCCESS: Fetched and executed instruction correctly"；
 * 2) 且 r1=42、PC 前进到下一条；
 * 3) 页故障测试应出现 "SUCCESS: Page fault correctly routed to exception handler."；
 * 4) main 返回 0。
 * 原因：分别验证了地址翻译成功路径与缺页异常路径都正确。
 */
using namespace loongarch;

namespace
{

// 2RI12 格式指令编码辅助函数：
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

} // 匿名命名空间

void test_mmu()
{
    std::printf("=== LoongArch MMU / Paging Test ===\n");

    Memory memory;
    Uart   uart;
    Timer  timer;
    TestDevice testDevice;
    Bus    bus(memory, uart, timer, testDevice);
    CPU    cpu(bus);

    // 1. 构建页表
    // 将虚拟地址 0x12345000 映射到物理地址 0x0000A000
    // VADDR: 0x12345000 -> VPN: 0x12345, 偏移: 0x000
    // 页目录索引: VPN [19:10] = 0x12345 >> 10 = 0x048
    // 页表索引: VPN [9:0]   = 0x12345 & 0x3FF = 0x345

    const std::uint32_t PD_BASE = 0x00100000u;
    const std::uint32_t PT_BASE = 0x00200000u;
    const std::uint32_t PPN     = 0x0000A000u; // 物理页基址

    std::printf("[MMU] Creating Page Directory at physical 0x%08X\n", PD_BASE);
    std::printf("[MMU] Creating Page Table at physical 0x%08X\n", PT_BASE);
    std::printf("[MMU] Mapping Virtual 0x12345000 -> Physical 0x0000A000\n");

    // 在 PD_BASE + PD_Idx*4 写入 PDE
    std::uint32_t pde_addr = PD_BASE + (0x048 * 4u);
    memory.write32(pde_addr, PT_BASE | 0x1u); // 设置有效位 (0x1)

    // 在 PT_BASE + PT_Idx*4 写入 PTE
    std::uint32_t pte_addr = PT_BASE + (0x345 * 4u);
    memory.write32(pte_addr, PPN | 0x1u); // 设置有效位 (0x1)

    // 2. 在映射后的物理内存写入指令
    // ADDI.W r1, r0, 42
    const std::uint32_t instr_addi = encode_2ri12(OPC2_ADDI_W, 1u, 0u, 42);
    memory.write32(PPN + 0, instr_addi);

    // 3. 配置 CPU
    // 使能分页位 PG（bit3），并设置 PGDL
    std::uint32_t crmd = cpu.getCRMD();
    crmd |= (1u << 3); // 置位 PG
    cpu.setCRMD(crmd);
    cpu.setPGDL(PD_BASE);
    cpu.setPC(0x12345000u);

    std::printf("\n[MMU] Executing mapped instruction at VADDR 0x12345000\n");
    cpu.step();

    // 检查 r1 是否为 42
    if (cpu.registers()[1] == 42) {
        std::printf("[MMU] SUCCESS: Fetched and executed instruction correctly. r1 = %u\n", cpu.registers()[1]);
    } else {
        std::printf("[MMU] FAILURE: Expected r1 = 42, got %u\n", cpu.registers()[1]);
    }

    std::printf("[MMU] PC is now: 0x%08X\n", cpu.getPC());

    // 4. 测试缺页异常
    std::printf("\n[MMU] Testing Page Fault by fetching from unmapped VADDR 0x20000000\n");
    cpu.setPC(0x20000000u);
    cpu.step();

    std::printf("[MMU] After page fault, PC is: 0x%08X\n", cpu.getPC());
    // 期望跳转到异常入口（0x1C000000 + EXC_ADDR_ERROR(2) * 0x100 = 0x1C000200）
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

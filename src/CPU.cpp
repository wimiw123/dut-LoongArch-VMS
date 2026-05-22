#include "CPU.h"

#include "decoder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace loongarch
{

namespace
{

constexpr std::uint32_t EXC_SYSCALL = 0x0Bu;
constexpr std::uint32_t EXC_BREAK = 0x0Cu;
constexpr std::uint32_t EXC_ILLEGAL_INSTR = 0x0Du;
constexpr std::uint32_t EXC_ADDR_ERROR = 0x02u;
constexpr std::uint32_t EXC_UNKNOWN = 0xFFu;

constexpr std::uint32_t OPC_CSR_SYSTEM = 0x01u;
constexpr std::uint32_t INSTR_NOP = 0x03400000u;
constexpr std::uint32_t INSTR_ERTN = 0x06483800u;

enum class Op
{
    Invalid,
    FetchFault,
    Nop,
    Syscall,
    Break,
    Ertn,
    Lu12iW,
    Pcaddu12i,
    Pcaddi,
    BstrpickW,
    ExtWH,
    ExtWB,
    SlliW,
    SrliW,
    SraiW,
    AddW,
    SubW,
    And,
    Or,
    Xor,
    Nor,
    Slt,
    Sltu,
    SllW,
    SrlW,
    SraW,
    MulW,
    MulhW,
    MulhWu,
    DivW,
    ModW,
    DivWu,
    ModWu,
    AddiW,
    Andi,
    Ori,
    Xori,
    Slti,
    Sltui,
    LdB,
    LdBu,
    LdH,
    LdHu,
    LdW,
    StB,
    StH,
    StW,
    Beq,
    Bne,
    Blt,
    Bge,
    Bltu,
    Bgeu,
    B,
    Bl,
    Jirl,
    Bnez,
    Beqz,
    Csr,
};

enum class UnitKind
{
    None,
    Alu,
    MulDiv,
    Memory,
    Branch,
    System,
};

struct InstructionInfo
{
    Op op{Op::Invalid};
    UnitKind unit{UnitKind::System};
    std::uint32_t raw{0};
    std::uint32_t pc{0};
    std::uint32_t rd{0};
    std::uint32_t rj{0};
    std::uint32_t rk{0};

    std::int32_t imm12{0};
    std::uint32_t uimm12{0};
    std::int32_t imm16{0};
    std::int32_t imm20{0};
    std::int32_t imm21{0};
    std::int32_t imm26{0};
    std::uint32_t csr_addr{0};

    bool has_dest{false};
    std::uint8_t dest_reg{0};

    bool src1_used{false};
    std::uint8_t src1_reg{0};
    bool src2_used{false};
    std::uint8_t src2_reg{0};
    bool extra_used{false};
    std::uint8_t extra_reg{0};

    bool is_branch{false};
    bool uses_dynamic_prediction{false};
    bool is_load{false};
    bool is_store{false};
    std::uint8_t memory_size{0};
    bool memory_signed{false};

    int latency{1};
    int serial_cost{5};
};

struct ExecuteResult
{
    bool exception{false};
    std::uint32_t exception_code{0};

    bool has_result{false};
    std::uint32_t result_value{0};

    bool branch_taken{false};
    std::uint32_t next_pc{0};

    bool has_memory{false};
    std::uint32_t mem_addr{0};
    std::uint32_t mem_value{0};
    std::uint8_t mem_size{0};

    bool csr_write{false};
    std::uint32_t csr_addr{0};
    std::uint32_t csr_value{0};
};

struct ROBEntry
{
    std::uint64_t seq{0};
    InstructionInfo inst{};
    bool predicted_taken{false};
    std::uint32_t predicted_next_pc{0};

    bool ready{false};
    bool exception{false};
    std::uint32_t exception_code{0};

    bool has_result{false};
    std::uint32_t result_value{0};
    bool branch_taken{false};
    std::uint32_t next_pc{0};

    bool has_memory{false};
    std::uint32_t mem_addr{0};
    std::uint32_t mem_value{0};
    std::uint8_t mem_size{0};

    bool csr_write{false};
    std::uint32_t csr_addr{0};
    std::uint32_t csr_value{0};

    bool completed_before_older{false};

    std::uint64_t fetch_cycle{0};
    std::uint64_t rename_cycle{0};
    std::uint64_t issue_cycle{0};
    std::uint64_t writeback_cycle{0};
    std::uint64_t commit_cycle{0};
};

struct ReservationStation
{
    std::uint64_t seq{0};
    InstructionInfo inst{};

    bool src1_needed{false};
    bool src1_ready{true};
    std::uint32_t src1_value{0};
    std::int64_t src1_tag{-1};

    bool src2_needed{false};
    bool src2_ready{true};
    std::uint32_t src2_value{0};
    std::int64_t src2_tag{-1};

    bool extra_needed{false};
    bool extra_ready{true};
    std::uint32_t extra_value{0};
    std::int64_t extra_tag{-1};

    std::uint64_t earliest_issue_cycle{0};
};

struct FunctionalUnitState
{
    bool busy{false};
    UnitKind unit{UnitKind::None};
    std::uint64_t seq{0};
    std::uint64_t start_cycle{0};
    std::uint64_t complete_cycle{0};
    std::uint32_t src1_value{0};
    std::uint32_t src2_value{0};
    std::uint32_t extra_value{0};
};

struct BranchPredictorEntry
{
    std::uint8_t counter{1};
    bool target_valid{false};
    std::uint32_t target{0};
};

struct FetchedInstruction
{
    std::uint64_t seq{0};
    InstructionInfo inst{};
    bool predicted_taken{false};
    std::uint32_t predicted_next_pc{0};
};

inline std::uint32_t get_reg(const std::uint32_t *regs, std::size_t index)
{
    return (index < 32u) ? regs[index] : 0u;
}

inline void set_reg(std::uint32_t *regs, std::size_t index, std::uint32_t value)
{
    if (index > 0u && index < 32u)
    {
        regs[index] = value;
    }
}

inline std::uint32_t add_signed_offset(std::uint32_t base, std::int32_t offset)
{
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(base) + offset);
}

inline std::uint32_t predictor_index(std::uint32_t pc)
{
    return (pc >> 2u) & 0x7Fu;
}

inline bool is_muldiv_op(Op op)
{
    return op == Op::MulW || op == Op::MulhW || op == Op::MulhWu || op == Op::DivW ||
           op == Op::ModW || op == Op::DivWu || op == Op::ModWu;
}

inline bool is_branch_op(Op op)
{
    return op == Op::Beq || op == Op::Bne || op == Op::Blt || op == Op::Bge ||
           op == Op::Bltu || op == Op::Bgeu || op == Op::B || op == Op::Bl ||
           op == Op::Jirl || op == Op::Bnez || op == Op::Beqz || op == Op::Ertn;
}

inline std::uint32_t truncate_to_size(std::uint32_t value, std::uint8_t size)
{
    switch (size)
    {
    case 1:
        return value & 0xFFu;
    case 2:
        return value & 0xFFFFu;
    default:
        return value;
    }
}

InstructionInfo finalize_instruction(InstructionInfo info, int latency)
{
    info.latency = latency;
    info.serial_cost = 4 + latency;
    return info;
}

InstructionInfo make_fetch_fault_instruction(std::uint32_t pc)
{
    InstructionInfo info{};
    info.op = Op::FetchFault;
    info.unit = UnitKind::System;
    info.pc = pc;
    info.latency = 1;
    info.serial_cost = 5;
    return info;
}

InstructionInfo decode_instruction(std::uint32_t instr, std::uint32_t pc)
{
    InstructionInfo info{};
    info.raw = instr;
    info.pc = pc;
    info.rd = decode_rd(instr);
    info.rj = decode_rj(instr);
    info.rk = decode_rk(instr);
    info.imm12 = decode_imm12(instr);
    info.uimm12 = decode_uimm12(instr);
    info.imm16 = decode_imm16(instr);
    info.imm20 = decode_imm20(instr);
    info.imm21 = decode_imm21(instr);
    info.imm26 = decode_imm26(instr);
    info.csr_addr = extract_bits(instr, 10u, 14u);

    const std::uint32_t opc11 = decode_op11(instr);
    const std::uint32_t opc7 = decode_op7(instr);
    const std::uint32_t opc10 = decode_op10(instr);
    const std::uint32_t opc17 = decode_op17(instr);
    const std::uint32_t opc22 = decode_op22(instr);
    const std::uint32_t opc6 = decode_op6(instr);

    if ((instr & 0xFFFF8000u) == 0x002B0000u)
    {
        info.op = Op::Syscall;
        info.unit = UnitKind::System;
        return finalize_instruction(info, 1);
    }

    if (instr == INSTR_ERTN)
    {
        info.op = Op::Ertn;
        info.unit = UnitKind::Branch;
        info.is_branch = true;
        return finalize_instruction(info, 1);
    }

    if (instr == INSTR_NOP)
    {
        info.op = Op::Nop;
        info.unit = UnitKind::System;
        return finalize_instruction(info, 1);
    }

    switch (opc7)
    {
    case OPC7_LU12I_W:
        info.op = Op::Lu12iW;
        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        return finalize_instruction(info, 1);
    case OPC7_PCADDU12I:
        info.op = Op::Pcaddu12i;
        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        return finalize_instruction(info, 1);
    case OPC7_PCADDI:
        info.op = Op::Pcaddi;
        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        return finalize_instruction(info, 1);
    default:
        break;
    }

    if (opc11 == OPC11_BSTRPICK_W)
    {
        info.op = Op::BstrpickW;
        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        info.src1_used = true;
        info.src1_reg = static_cast<std::uint8_t>(info.rj);
        return finalize_instruction(info, 1);
    }

    if (opc22 == OPC22_EXT_W_H)
    {
        info.op = Op::ExtWH;
        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        info.src1_used = true;
        info.src1_reg = static_cast<std::uint8_t>(info.rj);
        return finalize_instruction(info, 1);
    }

    if (opc22 == OPC22_EXT_W_B)
    {
        info.op = Op::ExtWB;
        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        info.src1_used = true;
        info.src1_reg = static_cast<std::uint8_t>(info.rj);
        return finalize_instruction(info, 1);
    }

    switch (opc17)
    {
    case OPC17_SLLI_W:
        info.op = Op::SlliW;
        break;
    case OPC17_SRLI_W:
        info.op = Op::SrliW;
        break;
    case OPC17_SRAI_W:
        info.op = Op::SraiW;
        break;
    case OPC17_ADD_W:
        info.op = Op::AddW;
        break;
    case OPC17_SUB_W:
        info.op = Op::SubW;
        break;
    case OPC17_AND:
        info.op = Op::And;
        break;
    case OPC17_OR:
        info.op = Op::Or;
        break;
    case OPC17_XOR:
        info.op = Op::Xor;
        break;
    case OPC17_NOR:
        info.op = Op::Nor;
        break;
    case OPC17_SLT:
        info.op = Op::Slt;
        break;
    case OPC17_SLTU:
        info.op = Op::Sltu;
        break;
    case OPC17_SLL_W:
        info.op = Op::SllW;
        break;
    case OPC17_SRL_W:
        info.op = Op::SrlW;
        break;
    case OPC17_SRA_W:
        info.op = Op::SraW;
        break;
    case OPC17_MUL_W:
        info.op = Op::MulW;
        break;
    case OPC17_MULH_W:
        info.op = Op::MulhW;
        break;
    case OPC17_MULH_WU:
        info.op = Op::MulhWu;
        break;
    case OPC17_DIV_W:
        info.op = Op::DivW;
        break;
    case OPC17_MOD_W:
        info.op = Op::ModW;
        break;
    case OPC17_DIV_WU:
        info.op = Op::DivWu;
        break;
    case OPC17_MOD_WU:
        info.op = Op::ModWu;
        break;
    case OPC17_BREAK:
        info.op = Op::Break;
        info.unit = UnitKind::System;
        return finalize_instruction(info, 1);
    default:
        break;
    }

    if (info.op != Op::Invalid)
    {
        info.unit = is_muldiv_op(info.op) ? UnitKind::MulDiv : UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        info.src1_used = true;
        info.src1_reg = static_cast<std::uint8_t>(info.rj);
        if (info.op == Op::SlliW || info.op == Op::SrliW || info.op == Op::SraiW)
        {
            return finalize_instruction(info, 1);
        }
        info.src2_used = true;
        info.src2_reg = static_cast<std::uint8_t>(info.rk);
        return finalize_instruction(info, is_muldiv_op(info.op) ? ((info.op == Op::MulW || info.op == Op::MulhW || info.op == Op::MulhWu) ? 3 : 12) : 1);
    }

    switch (opc10)
    {
    case OPC10_ADDI_W:
        info.op = Op::AddiW;
        break;
    case OPC10_ANDI:
        info.op = Op::Andi;
        break;
    case OPC10_ORI:
        info.op = Op::Ori;
        break;
    case OPC10_XORI:
        info.op = Op::Xori;
        break;
    case OPC10_SLTI:
        info.op = Op::Slti;
        break;
    case OPC10_SLTUI:
        info.op = Op::Sltui;
        break;
    case OPC10_LD_B:
        info.op = Op::LdB;
        break;
    case OPC10_LD_BU:
        info.op = Op::LdBu;
        break;
    case OPC10_LD_H:
        info.op = Op::LdH;
        break;
    case OPC10_LD_HU:
        info.op = Op::LdHu;
        break;
    case OPC10_LD_W:
        info.op = Op::LdW;
        break;
    case OPC10_ST_B:
        info.op = Op::StB;
        break;
    case OPC10_ST_H:
        info.op = Op::StH;
        break;
    case OPC10_ST_W:
        info.op = Op::StW;
        break;
    default:
        break;
    }

    if (info.op != Op::Invalid)
    {
        if (info.op == Op::LdB || info.op == Op::LdBu || info.op == Op::LdH || info.op == Op::LdHu ||
            info.op == Op::LdW || info.op == Op::StB || info.op == Op::StH || info.op == Op::StW)
        {
            info.unit = UnitKind::Memory;
            info.src1_used = true;
            info.src1_reg = static_cast<std::uint8_t>(info.rj);
            info.is_load = info.op == Op::LdB || info.op == Op::LdBu || info.op == Op::LdH ||
                           info.op == Op::LdHu || info.op == Op::LdW;
            info.is_store = !info.is_load;
            if (info.is_load)
            {
                info.has_dest = true;
                info.dest_reg = static_cast<std::uint8_t>(info.rd);
            }
            else
            {
                info.extra_used = true;
                info.extra_reg = static_cast<std::uint8_t>(info.rd);
            }

            if (info.op == Op::LdB || info.op == Op::LdBu || info.op == Op::StB)
            {
                info.memory_size = 1;
            }
            else if (info.op == Op::LdH || info.op == Op::LdHu || info.op == Op::StH)
            {
                info.memory_size = 2;
            }
            else
            {
                info.memory_size = 4;
            }

            info.memory_signed = info.op == Op::LdB || info.op == Op::LdH;
            return finalize_instruction(info, 2);
        }

        info.unit = UnitKind::Alu;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        info.src1_used = true;
        info.src1_reg = static_cast<std::uint8_t>(info.rj);
        return finalize_instruction(info, 1);
    }

    switch (opc6)
    {
    case OPC6_BEQ:
        info.op = Op::Beq;
        break;
    case OPC6_BNE:
        info.op = Op::Bne;
        break;
    case OPC6_BLT:
        info.op = Op::Blt;
        break;
    case OPC6_BGE:
        info.op = Op::Bge;
        break;
    case OPC6_BLTU:
        info.op = Op::Bltu;
        break;
    case OPC6_BGEU:
        info.op = Op::Bgeu;
        break;
    case OPC6_B:
        info.op = Op::B;
        break;
    case OPC6_BL:
        info.op = Op::Bl;
        break;
    case OPC6_JIRL:
        info.op = Op::Jirl;
        break;
    case OPC6_BNEZ:
        info.op = Op::Bnez;
        break;
    case OPC6_BEQZ:
        info.op = Op::Beqz;
        break;
    case OPC_CSR_SYSTEM:
        info.op = Op::Csr;
        break;
    default:
        break;
    }

    if (info.op == Op::Csr)
    {
        info.unit = UnitKind::System;
        info.has_dest = true;
        info.dest_reg = static_cast<std::uint8_t>(info.rd);
        if (info.rj == 1u)
        {
            info.src1_used = true;
            info.src1_reg = static_cast<std::uint8_t>(info.rd);
        }
        else if (info.rj != 0u)
        {
            info.src1_used = true;
            info.src1_reg = static_cast<std::uint8_t>(info.rd);
            info.src2_used = true;
            info.src2_reg = static_cast<std::uint8_t>(info.rj);
        }
        return finalize_instruction(info, 1);
    }

    if (info.op != Op::Invalid)
    {
        info.unit = UnitKind::Branch;
        info.is_branch = true;
        info.uses_dynamic_prediction = info.op == Op::Beq || info.op == Op::Bne ||
                                       info.op == Op::Blt || info.op == Op::Bge ||
                                       info.op == Op::Bltu || info.op == Op::Bgeu ||
                                       info.op == Op::Bnez || info.op == Op::Beqz ||
                                       info.op == Op::Jirl;

        if (info.op == Op::Bl)
        {
            info.has_dest = true;
            info.dest_reg = 1u;
        }
        else if (info.op == Op::Jirl)
        {
            info.has_dest = true;
            info.dest_reg = static_cast<std::uint8_t>(info.rd);
            info.src1_used = true;
            info.src1_reg = static_cast<std::uint8_t>(info.rj);
            return finalize_instruction(info, 1);
        }

        if (info.op == Op::Beq || info.op == Op::Bne || info.op == Op::Blt || info.op == Op::Bge ||
            info.op == Op::Bltu || info.op == Op::Bgeu)
        {
            info.src1_used = true;
            info.src1_reg = static_cast<std::uint8_t>(info.rj);
            info.src2_used = true;
            info.src2_reg = static_cast<std::uint8_t>(info.rd);
        }
        else if (info.op == Op::Bnez || info.op == Op::Beqz)
        {
            info.src1_used = true;
            info.src1_reg = static_cast<std::uint8_t>(info.rj);
        }

        return finalize_instruction(info, 1);
    }

    return finalize_instruction(info, 1);
}

struct Prediction
{
    bool taken{false};
    std::uint32_t next_pc{0};
};

} // namespace

struct AdvancedState
{
    static constexpr std::size_t ROB_CAPACITY = 24;
    static constexpr std::size_t RS_CAPACITY = 16;

    std::deque<ROBEntry> rob;
    std::vector<ReservationStation> rs;
    std::optional<FetchedInstruction> if_stage;
    std::optional<FetchedInstruction> id_stage;

    FunctionalUnitState alu{};
    FunctionalUnitState muldiv{};
    FunctionalUnitState mem{};
    FunctionalUnitState branch{};
    FunctionalUnitState system{};

    std::array<BranchPredictorEntry, 128> predictor{};
    std::array<std::int64_t, 32> rat{};
    std::uint32_t fetch_pc{0};
    std::uint64_t next_seq{1};

    AdvancedState()
    {
        rat.fill(-1);
    }
};

namespace
{

Prediction predict_next_pc(const AdvancedState &state, const InstructionInfo &info)
{
    Prediction prediction{};
    prediction.next_pc = info.pc + 4u;

    if (info.op == Op::B || info.op == Op::Bl)
    {
        prediction.taken = true;
        prediction.next_pc = add_signed_offset(info.pc, info.imm26 << 2);
        return prediction;
    }

    if (info.op == Op::Ertn)
    {
        prediction.taken = false;
        prediction.next_pc = info.pc + 4u;
        return prediction;
    }

    if (info.op == Op::Jirl)
    {
        const auto &entry = state.predictor[predictor_index(info.pc)];
        if (entry.target_valid)
        {
            prediction.taken = true;
            prediction.next_pc = entry.target;
        }
        return prediction;
    }

    if (!info.uses_dynamic_prediction)
    {
        return prediction;
    }

    const auto &entry = state.predictor[predictor_index(info.pc)];
    if (entry.counter >= 2u && entry.target_valid)
    {
        prediction.taken = true;
        prediction.next_pc = entry.target;
    }

    return prediction;
}

ROBEntry *find_rob_entry(AdvancedState &state, std::uint64_t seq)
{
    for (auto &entry : state.rob)
    {
        if (entry.seq == seq)
        {
            return &entry;
        }
    }
    return nullptr;
}

const ROBEntry *find_rob_entry(const AdvancedState &state, std::uint64_t seq)
{
    for (const auto &entry : state.rob)
    {
        if (entry.seq == seq)
        {
            return &entry;
        }
    }
    return nullptr;
}

bool operands_ready(const ReservationStation &entry)
{
    return (!entry.src1_needed || entry.src1_ready) && (!entry.src2_needed || entry.src2_ready) &&
           (!entry.extra_needed || entry.extra_ready);
}

std::size_t inflight_count(const AdvancedState &state)
{
    return state.rob.size() + (state.if_stage ? 1u : 0u) + (state.id_stage ? 1u : 0u);
}

void cancel_if_younger(FunctionalUnitState &fu, std::uint64_t seq)
{
    if (fu.busy && fu.seq > seq)
    {
        fu = FunctionalUnitState{};
    }
}

CoreMode parse_core_mode_from_env()
{
    const char *raw = std::getenv("LOONGARCH_CORE_MODE");
    if (raw == nullptr)
    {
        return CoreMode::AdvancedTomasulo;
    }

    std::string value(raw);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (value == "baseline" || value == "seq" || value == "sequential" || value == "simple")
    {
        return CoreMode::BaselineSequential;
    }
    return CoreMode::AdvancedTomasulo;
}

} // namespace

CPU::CPU(Device &bus) : m_bus(bus), m_core_mode(parse_core_mode_from_env()), m_advanced(std::make_unique<AdvancedState>())
{
    reset(0);
}

CPU::~CPU() = default;
CPU::CPU(CPU &&) noexcept = default;

Device &CPU::bus() noexcept
{
    return m_bus;
}

const Device &CPU::bus() const noexcept
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
    if (m_advanced)
    {
        *m_advanced = AdvancedState{};
        m_advanced->fetch_pc = newPc;
    }
}

const std::uint32_t *CPU::registers() const noexcept
{
    return m_regs;
}

void CPU::setCRMD(std::uint32_t value) noexcept
{
    m_crmd = value;
}

std::uint32_t CPU::getCRMD() const noexcept
{
    return m_crmd;
}

void CPU::setPGDL(std::uint32_t value) noexcept
{
    m_pgdl = value;
}

std::uint32_t CPU::getPGDL() const noexcept
{
    return m_pgdl;
}

std::uint64_t CPU::getCycleCount() const noexcept
{
    return m_cycle_count;
}

CoreMode CPU::getCoreMode() const noexcept
{
    return m_core_mode;
}

const char *CPU::getCoreModeName() const noexcept
{
    return (m_core_mode == CoreMode::BaselineSequential) ? "baseline" : "advanced";
}

const CoreMetrics &CPU::getCoreMetrics() const noexcept
{
    return m_metrics;
}

std::uint32_t CPU::read_u8(std::uint32_t vaddr)
{
    const std::uint32_t paddr = translate_address(vaddr, AccessType::LOAD);
    const std::uint32_t word = m_bus.read32(paddr & ~0x3u);
    const std::uint32_t shift = (paddr & 0x3u) * 8u;
    return (word >> shift) & 0xFFu;
}

std::uint32_t CPU::read_u16(std::uint32_t vaddr)
{
    const std::uint32_t lo = read_u8(vaddr);
    const std::uint32_t hi = read_u8(vaddr + 1u);
    return lo | (hi << 8u);
}

std::uint32_t CPU::read_u32(std::uint32_t vaddr)
{
    if ((vaddr & 0x3u) == 0u)
    {
        const std::uint32_t paddr = translate_address(vaddr, AccessType::LOAD);
        return m_bus.read32(paddr);
    }

    return read_u8(vaddr) | (read_u8(vaddr + 1u) << 8u) | (read_u8(vaddr + 2u) << 16u) |
           (read_u8(vaddr + 3u) << 24u);
}

void CPU::write_u8(std::uint32_t vaddr, std::uint32_t value)
{
    const std::uint32_t paddr = translate_address(vaddr, AccessType::STORE);
    const std::uint32_t aligned = paddr & ~0x3u;
    const std::uint32_t shift = (paddr & 0x3u) * 8u;
    std::uint32_t word = m_bus.read32(aligned);
    word = (word & ~(0xFFu << shift)) | ((value & 0xFFu) << shift);
    m_bus.write32(aligned, word);
}

void CPU::write_u16(std::uint32_t vaddr, std::uint32_t value)
{
    write_u8(vaddr, value & 0xFFu);
    write_u8(vaddr + 1u, (value >> 8u) & 0xFFu);
}

void CPU::write_u32(std::uint32_t vaddr, std::uint32_t value)
{
    if ((vaddr & 0x3u) == 0u)
    {
        const std::uint32_t paddr = translate_address(vaddr, AccessType::STORE);
        m_bus.write32(paddr, value);
        return;
    }

    write_u8(vaddr, value & 0xFFu);
    write_u8(vaddr + 1u, (value >> 8u) & 0xFFu);
    write_u8(vaddr + 2u, (value >> 16u) & 0xFFu);
    write_u8(vaddr + 3u, (value >> 24u) & 0xFFu);
}

void CPU::enforceInvariants() noexcept
{
    m_regs[0] = 0u;
}

std::uint32_t CPU::getReg(std::size_t index) const noexcept
{
    return (index < 32u) ? m_regs[index] : 0u;
}

void CPU::setReg(std::size_t index, std::uint32_t value) noexcept
{
    if (index > 0u && index < 32u)
    {
        m_regs[index] = value;
    }
}

void CPU::signalInterrupt(std::uint32_t code) noexcept
{
    m_interrupt_pending = true;
    m_interrupt_code = code;
}

void CPU::raise_exception(std::uint32_t ex_code) noexcept
{
    m_epc = m_pc;
    m_estat = ex_code;
    m_pc = 0x1C000000u + (ex_code * 0x100u);
    m_crmd &= ~0x1u;
}

std::uint32_t CPU::translate_address(std::uint32_t vaddr, AccessType)
{
    if ((m_crmd & (1u << 3u)) == 0u)
    {
        return vaddr;
    }

    const std::uint32_t pd_index = (vaddr >> 22u) & 0x3FFu;
    const std::uint32_t pt_index = (vaddr >> 12u) & 0x3FFu;
    const std::uint32_t page_off = vaddr & 0xFFFu;

    const std::uint32_t pde = m_bus.read32(m_pgdl + pd_index * 4u);
    if ((pde & 0x1u) == 0u)
    {
        throw std::runtime_error("CPU: invalid PDE");
    }

    const std::uint32_t pt_base = pde & ~0xFFFu;
    const std::uint32_t pte = m_bus.read32(pt_base + pt_index * 4u);
    if ((pte & 0x1u) == 0u)
    {
        throw std::runtime_error("CPU: invalid PTE");
    }

    return (pte & ~0xFFFu) | page_off;
}

void CPU::reset(std::uint32_t resetPc) noexcept
{
    std::fill(std::begin(m_regs), std::end(m_regs), 0u);
    std::fill(std::begin(m_fregs), std::end(m_fregs), 0.0);
    std::fill(std::begin(m_csr), std::end(m_csr), 0u);

    m_pc = resetPc;
    m_cycle_count = 0u;
    m_epc = 0u;
    m_estat = 0u;
    m_crmd = 1u;
    m_ecfg = 0xFFFF'FFFFu;
    m_interrupt_pending = false;
    m_interrupt_code = 0u;
    m_pgdl = 0u;
    m_fcsr = 0u;
    m_fcc = 0u;
    m_llbit = false;
    m_lladdr = 0u;
    m_metrics = {};

    if (m_advanced)
    {
        *m_advanced = AdvancedState{};
        m_advanced->fetch_pc = resetPc;
    }
}

void CPU::step()
{
    if (m_core_mode == CoreMode::BaselineSequential)
    {
        stepBaseline();
    }
    else
    {
        stepAdvanced();
    }
}

void CPU::stepBaseline()
{
    const std::uint32_t curr_pc = m_pc;

    if (m_interrupt_pending && (m_crmd & 0x1u) != 0u)
    {
        m_estat = m_interrupt_code;
        m_interrupt_pending = false;
        raise_exception(m_interrupt_code ? m_interrupt_code : EXC_UNKNOWN);
        ++m_cycle_count;
        ++m_metrics.committed_instructions;
        enforceInvariants();
        return;
    }

    std::uint32_t raw = 0u;
    try
    {
        raw = m_bus.read32(translate_address(curr_pc, AccessType::FETCH));
    }
    catch (...)
    {
        m_pc = curr_pc;
        raise_exception(EXC_ADDR_ERROR);
        ++m_cycle_count;
        ++m_metrics.committed_instructions;
        enforceInvariants();
        return;
    }

    const InstructionInfo info = decode_instruction(raw, curr_pc);
    ExecuteResult result{};
    result.next_pc = curr_pc + 4u;

    auto finish = [&]() {
        ++m_cycle_count;
        ++m_metrics.committed_instructions;
        m_metrics.serialized_cycles += static_cast<std::uint64_t>(info.serial_cost);
        if (info.is_branch)
        {
            ++m_metrics.branch_instructions;
        }
        enforceInvariants();
    };

    const std::uint32_t src1 = info.src1_used ? get_reg(m_regs, info.src1_reg) : 0u;
    const std::uint32_t src2 = info.src2_used ? get_reg(m_regs, info.src2_reg) : 0u;
    const std::uint32_t extra = info.extra_used ? get_reg(m_regs, info.extra_reg) : 0u;

    try
    {
        switch (info.op)
        {
        case Op::Nop:
            break;
        case Op::Lu12iW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(info.imm20 << 12);
            break;
        case Op::Pcaddu12i:
            result.has_result = true;
            result.result_value = add_signed_offset(curr_pc, info.imm20 << 12);
            break;
        case Op::Pcaddi:
            result.has_result = true;
            result.result_value = add_signed_offset(curr_pc, info.imm20 << 2);
            break;
        case Op::BstrpickW: {
            const std::uint32_t lsb = (info.raw >> 10u) & 0x1Fu;
            const std::uint32_t msb = (info.raw >> 16u) & 0x1Fu;
            const std::uint32_t width = (msb >= lsb) ? (msb - lsb + 1u) : 0u;
            const std::uint32_t mask =
                (width == 0u) ? 0u : (width >= 32u ? 0xFFFF'FFFFu : ((1u << width) - 1u));
            result.has_result = true;
            result.result_value = (src1 >> lsb) & mask;
            break;
        }
        case Op::ExtWH:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>(static_cast<std::int16_t>(src1 & 0xFFFFu));
            break;
        case Op::ExtWB:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>(static_cast<std::int8_t>(src1 & 0xFFu));
            break;
        case Op::SlliW:
            result.has_result = true;
            result.result_value = src1 << ((info.raw >> 10u) & 0x1Fu);
            break;
        case Op::SrliW:
            result.has_result = true;
            result.result_value = src1 >> ((info.raw >> 10u) & 0x1Fu);
            break;
        case Op::SraiW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(src1) >> ((info.raw >> 10u) & 0x1Fu));
            break;
        case Op::AddW:
            result.has_result = true;
            result.result_value = src1 + src2;
            break;
        case Op::SubW:
            result.has_result = true;
            result.result_value = src1 - src2;
            break;
        case Op::And:
            result.has_result = true;
            result.result_value = src1 & src2;
            break;
        case Op::Or:
            result.has_result = true;
            result.result_value = src1 | src2;
            break;
        case Op::Xor:
            result.has_result = true;
            result.result_value = src1 ^ src2;
            break;
        case Op::Nor:
            result.has_result = true;
            result.result_value = ~(src1 | src2);
            break;
        case Op::Slt:
            result.has_result = true;
            result.result_value =
                (static_cast<std::int32_t>(src1) < static_cast<std::int32_t>(src2)) ? 1u : 0u;
            break;
        case Op::Sltu:
            result.has_result = true;
            result.result_value = (src1 < src2) ? 1u : 0u;
            break;
        case Op::SllW:
            result.has_result = true;
            result.result_value = src1 << (src2 & 0x1Fu);
            break;
        case Op::SrlW:
            result.has_result = true;
            result.result_value = src1 >> (src2 & 0x1Fu);
            break;
        case Op::SraW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(src1) >> (src2 & 0x1Fu));
            break;
        case Op::MulW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(static_cast<std::int32_t>(src1)) *
                static_cast<std::int32_t>(src2));
            break;
        case Op::MulhW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                (static_cast<std::int64_t>(static_cast<std::int32_t>(src1)) *
                 static_cast<std::int32_t>(src2)) >>
                32);
            break;
        case Op::MulhWu:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>((static_cast<std::uint64_t>(src1) *
                                            static_cast<std::uint64_t>(src2)) >>
                                           32);
            break;
        case Op::DivW:
            result.has_result = true;
            if (src2 == 0u)
            {
                result.result_value = 0u;
            }
            else if (src1 == 0x80000000u && src2 == 0xFFFF'FFFFu)
            {
                result.result_value = 0x80000000u;
            }
            else
            {
                result.result_value = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(src1) / static_cast<std::int32_t>(src2));
            }
            break;
        case Op::ModW:
            result.has_result = true;
            if (src2 == 0u)
            {
                result.result_value = 0u;
            }
            else if (src1 == 0x80000000u && src2 == 0xFFFF'FFFFu)
            {
                result.result_value = 0u;
            }
            else
            {
                result.result_value = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(src1) % static_cast<std::int32_t>(src2));
            }
            break;
        case Op::DivWu:
            result.has_result = true;
            result.result_value = (src2 == 0u) ? 0u : (src1 / src2);
            break;
        case Op::ModWu:
            result.has_result = true;
            result.result_value = (src2 == 0u) ? 0u : (src1 % src2);
            break;
        case Op::AddiW:
            result.has_result = true;
            result.result_value = add_signed_offset(src1, info.imm12);
            break;
        case Op::Andi:
            result.has_result = true;
            result.result_value = src1 & info.uimm12;
            break;
        case Op::Ori:
            result.has_result = true;
            result.result_value = src1 | info.uimm12;
            break;
        case Op::Xori:
            result.has_result = true;
            result.result_value = src1 ^ info.uimm12;
            break;
        case Op::Slti:
            result.has_result = true;
            result.result_value =
                (static_cast<std::int32_t>(src1) < info.imm12) ? 1u : 0u;
            break;
        case Op::Sltui:
            result.has_result = true;
            result.result_value = (src1 < static_cast<std::uint32_t>(info.imm12)) ? 1u : 0u;
            break;
        case Op::LdB:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int8_t>(read_u8(add_signed_offset(src1, info.imm12))));
            break;
        case Op::LdBu:
            result.has_result = true;
            result.result_value = read_u8(add_signed_offset(src1, info.imm12));
            break;
        case Op::LdH:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int16_t>(read_u16(add_signed_offset(src1, info.imm12))));
            break;
        case Op::LdHu:
            result.has_result = true;
            result.result_value = read_u16(add_signed_offset(src1, info.imm12));
            break;
        case Op::LdW:
            result.has_result = true;
            result.result_value = read_u32(add_signed_offset(src1, info.imm12));
            break;
        case Op::StB:
            write_u8(add_signed_offset(src1, info.imm12), extra);
            break;
        case Op::StH:
            write_u16(add_signed_offset(src1, info.imm12), extra);
            break;
        case Op::StW:
            write_u32(add_signed_offset(src1, info.imm12), extra);
            break;
        case Op::Beq:
            result.branch_taken = (src1 == src2);
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm16 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Bne:
            result.branch_taken = (src1 != src2);
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm16 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Blt:
            result.branch_taken =
                static_cast<std::int32_t>(src1) < static_cast<std::int32_t>(src2);
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm16 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Bge:
            result.branch_taken =
                static_cast<std::int32_t>(src1) >= static_cast<std::int32_t>(src2);
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm16 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Bltu:
            result.branch_taken = src1 < src2;
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm16 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Bgeu:
            result.branch_taken = src1 >= src2;
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm16 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::B:
            result.branch_taken = true;
            result.next_pc = add_signed_offset(curr_pc, info.imm26 << 2);
            break;
        case Op::Bl:
            result.branch_taken = true;
            result.next_pc = add_signed_offset(curr_pc, info.imm26 << 2);
            result.has_result = true;
            result.result_value = curr_pc + 4u;
            break;
        case Op::Jirl:
            result.branch_taken = true;
            result.next_pc = add_signed_offset(src1, info.imm16 << 2);
            result.has_result = true;
            result.result_value = curr_pc + 4u;
            break;
        case Op::Bnez:
            result.branch_taken = src1 != 0u;
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm21 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Beqz:
            result.branch_taken = src1 == 0u;
            result.next_pc = result.branch_taken ? add_signed_offset(curr_pc, info.imm21 << 2)
                                                 : curr_pc + 4u;
            break;
        case Op::Csr: {
            const std::uint32_t old = m_csr[info.csr_addr & 0x3FFFu];
            result.has_result = true;
            result.result_value = old;
            if (info.rj == 1u)
            {
                result.csr_write = true;
                result.csr_addr = info.csr_addr & 0x3FFFu;
                result.csr_value = src1;
            }
            else if (info.rj != 0u)
            {
                result.csr_write = true;
                result.csr_addr = info.csr_addr & 0x3FFFu;
                result.csr_value = (old & ~src2) | (src1 & src2);
            }
            break;
        }
        case Op::Ertn:
            result.branch_taken = true;
            result.next_pc = m_epc;
            break;
        case Op::Syscall:
            result.exception = true;
            result.exception_code = EXC_SYSCALL;
            result.next_pc = curr_pc;
            break;
        case Op::Break:
            result.exception = true;
            result.exception_code = EXC_BREAK;
            result.next_pc = curr_pc;
            break;
        case Op::FetchFault:
            result.exception = true;
            result.exception_code = EXC_ADDR_ERROR;
            result.next_pc = curr_pc;
            break;
        case Op::Invalid:
        default:
            result.exception = true;
            result.exception_code = EXC_ILLEGAL_INSTR;
            result.next_pc = curr_pc;
            break;
        }
    }
    catch (...)
    {
        result.exception = true;
        result.exception_code = EXC_ADDR_ERROR;
        result.next_pc = curr_pc;
    }

    if (result.exception)
    {
        m_pc = curr_pc;
        raise_exception(result.exception_code);
        finish();
        return;
    }

    if (result.csr_write)
    {
        m_csr[result.csr_addr] = result.csr_value;
    }
    if (result.has_result && info.has_dest)
    {
        set_reg(m_regs, info.dest_reg, result.result_value);
    }

    m_pc = result.next_pc;
    if (info.op == Op::Ertn)
    {
        m_crmd |= 0x1u;
    }

    finish();
}

void CPU::stepAdvanced()
{
    auto &state = *m_advanced;

    auto squash_younger_than = [&](std::uint64_t seq, std::uint32_t next_fetch_pc) {
        std::size_t squashed = 0u;
        while (!state.rob.empty() && state.rob.back().seq > seq)
        {
            ++squashed;
            state.rob.pop_back();
        }

        auto it = state.rs.begin();
        while (it != state.rs.end())
        {
            if (it->seq > seq)
            {
                ++squashed;
                it = state.rs.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (state.if_stage && state.if_stage->seq > seq)
        {
            ++squashed;
            state.if_stage.reset();
        }
        if (state.id_stage && state.id_stage->seq > seq)
        {
            ++squashed;
            state.id_stage.reset();
        }

        cancel_if_younger(state.alu, seq);
        cancel_if_younger(state.muldiv, seq);
        cancel_if_younger(state.mem, seq);
        cancel_if_younger(state.branch, seq);
        cancel_if_younger(state.system, seq);

        state.rat.fill(-1);
        for (const auto &entry : state.rob)
        {
            if (entry.inst.has_dest && entry.inst.dest_reg != 0u)
            {
                state.rat[entry.inst.dest_reg] = static_cast<std::int64_t>(entry.seq);
            }
        }

        state.fetch_pc = next_fetch_pc;
        m_metrics.pipeline_flushes += 1u;
        m_metrics.speculative_instructions_squashed += squashed;
    };

    auto clear_pipeline = [&]() {
        state.rob.clear();
        state.rs.clear();
        state.if_stage.reset();
        state.id_stage.reset();
        state.alu = FunctionalUnitState{};
        state.muldiv = FunctionalUnitState{};
        state.mem = FunctionalUnitState{};
        state.branch = FunctionalUnitState{};
        state.system = FunctionalUnitState{};
        state.rat.fill(-1);
        state.fetch_pc = m_pc;
    };

    auto wake_dependents = [&](std::uint64_t seq, std::uint32_t value) {
        for (auto &entry : state.rs)
        {
            if (!entry.src1_ready && entry.src1_tag == static_cast<std::int64_t>(seq))
            {
                entry.src1_ready = true;
                entry.src1_value = value;
            }
            if (!entry.src2_ready && entry.src2_tag == static_cast<std::int64_t>(seq))
            {
                entry.src2_ready = true;
                entry.src2_value = value;
            }
            if (!entry.extra_ready && entry.extra_tag == static_cast<std::int64_t>(seq))
            {
                entry.extra_ready = true;
                entry.extra_value = value;
            }
        }
    };

    auto execute_with_values = [&](const InstructionInfo &info, std::uint32_t src1, std::uint32_t src2,
                                   std::uint32_t extra) -> ExecuteResult {
        ExecuteResult result{};
        result.next_pc = info.pc + 4u;

        switch (info.op)
        {
        case Op::Nop:
            break;
        case Op::FetchFault:
            result.exception = true;
            result.exception_code = EXC_ADDR_ERROR;
            result.next_pc = info.pc;
            break;
        case Op::Syscall:
            result.exception = true;
            result.exception_code = EXC_SYSCALL;
            result.next_pc = info.pc;
            break;
        case Op::Break:
            result.exception = true;
            result.exception_code = EXC_BREAK;
            result.next_pc = info.pc;
            break;
        case Op::Lu12iW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(info.imm20 << 12);
            break;
        case Op::Pcaddu12i:
            result.has_result = true;
            result.result_value = add_signed_offset(info.pc, info.imm20 << 12);
            break;
        case Op::Pcaddi:
            result.has_result = true;
            result.result_value = add_signed_offset(info.pc, info.imm20 << 2);
            break;
        case Op::BstrpickW: {
            const std::uint32_t lsb = (info.raw >> 10u) & 0x1Fu;
            const std::uint32_t msb = (info.raw >> 16u) & 0x1Fu;
            const std::uint32_t width = (msb >= lsb) ? (msb - lsb + 1u) : 0u;
            const std::uint32_t mask =
                (width == 0u) ? 0u : (width >= 32u ? 0xFFFF'FFFFu : ((1u << width) - 1u));
            result.has_result = true;
            result.result_value = (src1 >> lsb) & mask;
            break;
        }
        case Op::ExtWH:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>(static_cast<std::int16_t>(src1 & 0xFFFFu));
            break;
        case Op::ExtWB:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>(static_cast<std::int8_t>(src1 & 0xFFu));
            break;
        case Op::SlliW:
            result.has_result = true;
            result.result_value = src1 << ((info.raw >> 10u) & 0x1Fu);
            break;
        case Op::SrliW:
            result.has_result = true;
            result.result_value = src1 >> ((info.raw >> 10u) & 0x1Fu);
            break;
        case Op::SraiW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(src1) >> ((info.raw >> 10u) & 0x1Fu));
            break;
        case Op::AddW:
            result.has_result = true;
            result.result_value = src1 + src2;
            break;
        case Op::SubW:
            result.has_result = true;
            result.result_value = src1 - src2;
            break;
        case Op::And:
            result.has_result = true;
            result.result_value = src1 & src2;
            break;
        case Op::Or:
            result.has_result = true;
            result.result_value = src1 | src2;
            break;
        case Op::Xor:
            result.has_result = true;
            result.result_value = src1 ^ src2;
            break;
        case Op::Nor:
            result.has_result = true;
            result.result_value = ~(src1 | src2);
            break;
        case Op::Slt:
            result.has_result = true;
            result.result_value =
                (static_cast<std::int32_t>(src1) < static_cast<std::int32_t>(src2)) ? 1u : 0u;
            break;
        case Op::Sltu:
            result.has_result = true;
            result.result_value = (src1 < src2) ? 1u : 0u;
            break;
        case Op::SllW:
            result.has_result = true;
            result.result_value = src1 << (src2 & 0x1Fu);
            break;
        case Op::SrlW:
            result.has_result = true;
            result.result_value = src1 >> (src2 & 0x1Fu);
            break;
        case Op::SraW:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>(static_cast<std::int32_t>(src1) >> (src2 & 0x1Fu));
            break;
        case Op::MulW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                static_cast<std::int64_t>(static_cast<std::int32_t>(src1)) *
                static_cast<std::int32_t>(src2));
            break;
        case Op::MulhW:
            result.has_result = true;
            result.result_value = static_cast<std::uint32_t>(
                (static_cast<std::int64_t>(static_cast<std::int32_t>(src1)) *
                 static_cast<std::int32_t>(src2)) >>
                32);
            break;
        case Op::MulhWu:
            result.has_result = true;
            result.result_value =
                static_cast<std::uint32_t>((static_cast<std::uint64_t>(src1) *
                                            static_cast<std::uint64_t>(src2)) >>
                                           32);
            break;
        case Op::DivW:
            result.has_result = true;
            if (src2 == 0u)
            {
                result.result_value = 0u;
            }
            else if (src1 == 0x80000000u && src2 == 0xFFFF'FFFFu)
            {
                result.result_value = 0x80000000u;
            }
            else
            {
                result.result_value = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(src1) / static_cast<std::int32_t>(src2));
            }
            break;
        case Op::ModW:
            result.has_result = true;
            if (src2 == 0u)
            {
                result.result_value = 0u;
            }
            else if (src1 == 0x80000000u && src2 == 0xFFFF'FFFFu)
            {
                result.result_value = 0u;
            }
            else
            {
                result.result_value = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(src1) % static_cast<std::int32_t>(src2));
            }
            break;
        case Op::DivWu:
            result.has_result = true;
            result.result_value = (src2 == 0u) ? 0u : (src1 / src2);
            break;
        case Op::ModWu:
            result.has_result = true;
            result.result_value = (src2 == 0u) ? 0u : (src1 % src2);
            break;
        case Op::AddiW:
            result.has_result = true;
            result.result_value = add_signed_offset(src1, info.imm12);
            break;
        case Op::Andi:
            result.has_result = true;
            result.result_value = src1 & info.uimm12;
            break;
        case Op::Ori:
            result.has_result = true;
            result.result_value = src1 | info.uimm12;
            break;
        case Op::Xori:
            result.has_result = true;
            result.result_value = src1 ^ info.uimm12;
            break;
        case Op::Slti:
            result.has_result = true;
            result.result_value =
                (static_cast<std::int32_t>(src1) < info.imm12) ? 1u : 0u;
            break;
        case Op::Sltui:
            result.has_result = true;
            result.result_value = (src1 < static_cast<std::uint32_t>(info.imm12)) ? 1u : 0u;
            break;
        case Op::LdB:
        case Op::LdBu:
        case Op::LdH:
        case Op::LdHu:
        case Op::LdW: {
            const std::uint32_t addr = add_signed_offset(src1, info.imm12);
            result.has_memory = true;
            result.mem_addr = addr;
            result.mem_size = info.memory_size;
            if (info.op == Op::LdB)
            {
                result.has_result = true;
                result.result_value = static_cast<std::uint32_t>(
                    static_cast<std::int8_t>(read_u8(addr)));
            }
            else if (info.op == Op::LdBu)
            {
                result.has_result = true;
                result.result_value = read_u8(addr);
            }
            else if (info.op == Op::LdH)
            {
                result.has_result = true;
                result.result_value = static_cast<std::uint32_t>(
                    static_cast<std::int16_t>(read_u16(addr)));
            }
            else if (info.op == Op::LdHu)
            {
                result.has_result = true;
                result.result_value = read_u16(addr);
            }
            else
            {
                result.has_result = true;
                result.result_value = read_u32(addr);
            }
            break;
        }
        case Op::StB:
        case Op::StH:
        case Op::StW:
            result.has_memory = true;
            result.mem_addr = add_signed_offset(src1, info.imm12);
            result.mem_size = info.memory_size;
            result.mem_value = truncate_to_size(extra, info.memory_size);
            break;
        case Op::Beq:
            result.branch_taken = (src1 == src2);
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm16 << 2) : info.pc + 4u;
            break;
        case Op::Bne:
            result.branch_taken = (src1 != src2);
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm16 << 2) : info.pc + 4u;
            break;
        case Op::Blt:
            result.branch_taken =
                static_cast<std::int32_t>(src1) < static_cast<std::int32_t>(src2);
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm16 << 2) : info.pc + 4u;
            break;
        case Op::Bge:
            result.branch_taken =
                static_cast<std::int32_t>(src1) >= static_cast<std::int32_t>(src2);
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm16 << 2) : info.pc + 4u;
            break;
        case Op::Bltu:
            result.branch_taken = src1 < src2;
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm16 << 2) : info.pc + 4u;
            break;
        case Op::Bgeu:
            result.branch_taken = src1 >= src2;
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm16 << 2) : info.pc + 4u;
            break;
        case Op::B:
            result.branch_taken = true;
            result.next_pc = add_signed_offset(info.pc, info.imm26 << 2);
            break;
        case Op::Bl:
            result.branch_taken = true;
            result.next_pc = add_signed_offset(info.pc, info.imm26 << 2);
            result.has_result = true;
            result.result_value = info.pc + 4u;
            break;
        case Op::Jirl:
            result.branch_taken = true;
            result.next_pc = add_signed_offset(src1, info.imm16 << 2);
            result.has_result = true;
            result.result_value = info.pc + 4u;
            break;
        case Op::Bnez:
            result.branch_taken = src1 != 0u;
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm21 << 2) : info.pc + 4u;
            break;
        case Op::Beqz:
            result.branch_taken = src1 == 0u;
            result.next_pc =
                result.branch_taken ? add_signed_offset(info.pc, info.imm21 << 2) : info.pc + 4u;
            break;
        case Op::Csr: {
            const std::uint32_t old = m_csr[info.csr_addr & 0x3FFFu];
            result.has_result = true;
            result.result_value = old;
            if (info.rj == 1u)
            {
                result.csr_write = true;
                result.csr_addr = info.csr_addr & 0x3FFFu;
                result.csr_value = src1;
            }
            else if (info.rj != 0u)
            {
                result.csr_write = true;
                result.csr_addr = info.csr_addr & 0x3FFFu;
                result.csr_value = (old & ~src2) | (src1 & src2);
            }
            break;
        }
        case Op::Ertn:
            result.branch_taken = true;
            result.next_pc = m_epc;
            break;
        case Op::Invalid:
        default:
            result.exception = true;
            result.exception_code = EXC_ILLEGAL_INSTR;
            result.next_pc = info.pc;
            break;
        }

        return result;
    };

    auto try_issue_to_fu = [&](FunctionalUnitState &fu, const ReservationStation &rs) {
        fu.busy = true;
        fu.unit = rs.inst.unit;
        fu.seq = rs.seq;
        fu.start_cycle = m_cycle_count;
        fu.complete_cycle = m_cycle_count + static_cast<std::uint64_t>(rs.inst.latency);
        fu.src1_value = rs.src1_value;
        fu.src2_value = rs.src2_value;
        fu.extra_value = rs.extra_value;
    };

    auto rob_has_older_pending_store = [&](std::uint64_t seq, std::uint32_t load_addr) {
        for (const auto &entry : state.rob)
        {
            if (entry.seq == seq)
            {
                break;
            }
            if (!entry.inst.is_store)
            {
                continue;
            }
            if (!entry.ready || !entry.has_memory)
            {
                return true;
            }
            if (entry.mem_addr == load_addr)
            {
                return true;
            }
        }
        return false;
    };

    auto try_forward_store_value = [&](std::uint64_t seq, std::uint32_t load_addr,
                                       std::uint8_t size, std::uint32_t &value) {
        for (auto it = state.rob.rbegin(); it != state.rob.rend(); ++it)
        {
            if (it->seq >= seq)
            {
                continue;
            }
            if (!it->inst.is_store)
            {
                continue;
            }
            if (!it->ready || !it->has_memory)
            {
                return false;
            }
            if (it->mem_addr == load_addr)
            {
                value = truncate_to_size(it->mem_value, size);
                return true;
            }
        }
        return false;
    };

    auto complete_fu = [&](FunctionalUnitState &fu) {
        if (!fu.busy || fu.complete_cycle > m_cycle_count)
        {
            return;
        }

        ROBEntry *entry = find_rob_entry(state, fu.seq);
        if (entry == nullptr)
        {
            fu = FunctionalUnitState{};
            return;
        }

        ExecuteResult result{};
        try
        {
            if (entry->inst.is_load)
            {
                const std::uint32_t addr = add_signed_offset(fu.src1_value, entry->inst.imm12);
                std::uint32_t forwarded = 0u;
                if (try_forward_store_value(entry->seq, addr, entry->inst.memory_size, forwarded))
                {
                    result.has_result = true;
                    if (entry->inst.memory_signed && entry->inst.memory_size == 1u)
                    {
                        result.result_value =
                            static_cast<std::uint32_t>(static_cast<std::int8_t>(forwarded));
                    }
                    else if (entry->inst.memory_signed && entry->inst.memory_size == 2u)
                    {
                        result.result_value =
                            static_cast<std::uint32_t>(static_cast<std::int16_t>(forwarded));
                    }
                    else
                    {
                        result.result_value = forwarded;
                    }
                    result.has_memory = true;
                    result.mem_addr = addr;
                    result.mem_size = entry->inst.memory_size;
                    ++m_metrics.load_store_forwardings;
                }
                else
                {
                    result = execute_with_values(entry->inst, fu.src1_value, fu.src2_value, fu.extra_value);
                }
            }
            else
            {
                result = execute_with_values(entry->inst, fu.src1_value, fu.src2_value, fu.extra_value);
            }
        }
        catch (...)
        {
            result.exception = true;
            result.exception_code = EXC_ADDR_ERROR;
            result.next_pc = entry->inst.pc;
        }

        entry->ready = true;
        entry->exception = result.exception;
        entry->exception_code = result.exception_code;
        entry->has_result = result.has_result;
        entry->result_value = result.result_value;
        entry->branch_taken = result.branch_taken;
        entry->next_pc = result.next_pc;
        entry->has_memory = result.has_memory;
        entry->mem_addr = result.mem_addr;
        entry->mem_value = result.mem_value;
        entry->mem_size = result.mem_size;
        entry->csr_write = result.csr_write;
        entry->csr_addr = result.csr_addr;
        entry->csr_value = result.csr_value;
        entry->writeback_cycle = m_cycle_count;

        for (const auto &older : state.rob)
        {
            if (older.seq == entry->seq)
            {
                break;
            }
            if (!older.ready)
            {
                entry->completed_before_older = true;
                break;
            }
        }

        if (entry->has_result)
        {
            wake_dependents(entry->seq, entry->result_value);
        }

        ++m_metrics.executed_instructions;

        if (entry->inst.uses_dynamic_prediction)
        {
            ++m_metrics.dynamic_branch_predictions;
            auto &bp = state.predictor[predictor_index(entry->inst.pc)];
            if (entry->inst.op == Op::Jirl)
            {
                bp.counter = 3u;
                bp.target_valid = true;
                bp.target = entry->next_pc;
            }
            else
            {
                if (entry->branch_taken)
                {
                    bp.counter = std::min<std::uint8_t>(3u, static_cast<std::uint8_t>(bp.counter + 1u));
                    bp.target_valid = true;
                    bp.target = entry->next_pc;
                }
                else
                {
                    bp.counter = (bp.counter == 0u) ? 0u : static_cast<std::uint8_t>(bp.counter - 1u);
                }
            }

            if (entry->predicted_next_pc == entry->next_pc)
            {
                ++m_metrics.branch_prediction_hits;
            }
            else
            {
                ++m_metrics.branch_prediction_misses;
                squash_younger_than(entry->seq, entry->next_pc);
            }
        }
        else if (entry->inst.op == Op::B || entry->inst.op == Op::Bl || entry->inst.op == Op::Ertn)
        {
            if (entry->predicted_next_pc != entry->next_pc)
            {
                squash_younger_than(entry->seq, entry->next_pc);
            }
        }

        fu = FunctionalUnitState{};
    };

    if (m_interrupt_pending && (m_crmd & 0x1u) != 0u && state.rob.empty() && !state.if_stage && !state.id_stage &&
        state.rs.empty() && !state.alu.busy && !state.muldiv.busy && !state.mem.busy &&
        !state.branch.busy && !state.system.busy)
    {
        m_estat = m_interrupt_code;
        m_interrupt_pending = false;
        raise_exception(m_interrupt_code ? m_interrupt_code : EXC_UNKNOWN);
        ++m_cycle_count;
        ++m_metrics.committed_instructions;
        enforceInvariants();
        state.fetch_pc = m_pc;
        return;
    }

    while (true)
    {
        m_metrics.rob_occupancy_samples += state.rob.size();
        m_metrics.rs_occupancy_samples += state.rs.size();
        m_metrics.inflight_occupancy_samples += inflight_count(state);
        m_metrics.max_rob_occupancy = std::max<std::uint64_t>(m_metrics.max_rob_occupancy, state.rob.size());
        m_metrics.max_rs_occupancy = std::max<std::uint64_t>(m_metrics.max_rs_occupancy, state.rs.size());
        m_metrics.max_inflight_instructions =
            std::max<std::uint64_t>(m_metrics.max_inflight_instructions, inflight_count(state));

        bool committed_this_call = false;

        if (!state.rob.empty() && state.rob.front().ready)
        {
            ROBEntry entry = state.rob.front();
            state.rob.pop_front();

            if (entry.completed_before_older)
            {
                ++m_metrics.out_of_order_completions;
            }

            if (entry.inst.is_branch)
            {
                ++m_metrics.branch_instructions;
            }

            m_metrics.serialized_cycles += static_cast<std::uint64_t>(entry.inst.serial_cost);
            ++m_metrics.committed_instructions;
            entry.commit_cycle = m_cycle_count;

            if (entry.exception)
            {
                m_pc = entry.inst.pc;
                raise_exception(entry.exception_code);
                clear_pipeline();
            }
            else
            {
                if (entry.csr_write)
                {
                    m_csr[entry.csr_addr] = entry.csr_value;
                }

                if (entry.has_result && entry.inst.has_dest)
                {
                    set_reg(m_regs, entry.inst.dest_reg, entry.result_value);
                }

                if (entry.inst.is_store)
                {
                    if (entry.mem_size == 1u)
                    {
                        write_u8(entry.mem_addr, entry.mem_value);
                    }
                    else if (entry.mem_size == 2u)
                    {
                        write_u16(entry.mem_addr, entry.mem_value);
                    }
                    else
                    {
                        write_u32(entry.mem_addr, entry.mem_value);
                    }
                }

                m_pc = entry.next_pc;
                if (entry.inst.op == Op::Ertn)
                {
                    m_crmd |= 0x1u;
                }
            }

            if (entry.inst.has_dest && entry.inst.dest_reg != 0u &&
                state.rat[entry.inst.dest_reg] == static_cast<std::int64_t>(entry.seq))
            {
                state.rat[entry.inst.dest_reg] = -1;
            }

            enforceInvariants();
            committed_this_call = true;
        }

        complete_fu(state.alu);
        complete_fu(state.muldiv);
        complete_fu(state.mem);
        complete_fu(state.branch);
        complete_fu(state.system);

        if (!state.rob.empty() && state.id_stage &&
            (state.rob.size() >= AdvancedState::ROB_CAPACITY || state.rs.size() >= AdvancedState::RS_CAPACITY))
        {
            if (state.rob.size() >= AdvancedState::ROB_CAPACITY)
            {
                ++m_metrics.rob_full_stalls;
            }
            if (state.rs.size() >= AdvancedState::RS_CAPACITY)
            {
                ++m_metrics.rs_full_stalls;
            }
            ++m_metrics.decode_stalls;
        }
        else if (state.id_stage)
        {
            FetchedInstruction fetched = *state.id_stage;
            state.id_stage.reset();

            ROBEntry rob_entry{};
            rob_entry.seq = fetched.seq;
            rob_entry.inst = fetched.inst;
            rob_entry.predicted_taken = fetched.predicted_taken;
            rob_entry.predicted_next_pc = fetched.predicted_next_pc;
            rob_entry.fetch_cycle = m_cycle_count;
            rob_entry.rename_cycle = m_cycle_count;
            rob_entry.next_pc = fetched.inst.pc + 4u;

            state.rob.push_back(rob_entry);
            ++m_metrics.decoded_instructions;

            ReservationStation rs{};
            rs.seq = fetched.seq;
            rs.inst = fetched.inst;
            rs.earliest_issue_cycle = m_cycle_count + 1u;

            auto bind_source = [&](bool used, std::uint8_t reg, bool &needed, bool &ready,
                                   std::uint32_t &value, std::int64_t &tag) {
                needed = used;
                if (!used)
                {
                    ready = true;
                    value = 0u;
                    tag = -1;
                    return;
                }

                const std::int64_t mapped = state.rat[reg];
                if (mapped < 0)
                {
                    ready = true;
                    value = get_reg(m_regs, reg);
                    tag = -1;
                    return;
                }

                const ROBEntry *producer = find_rob_entry(state, static_cast<std::uint64_t>(mapped));
                if (producer != nullptr && producer->ready && producer->has_result)
                {
                    ready = true;
                    value = producer->result_value;
                    tag = -1;
                }
                else
                {
                    ready = false;
                    value = 0u;
                    tag = mapped;
                }
            };

            bind_source(fetched.inst.src1_used, fetched.inst.src1_reg, rs.src1_needed, rs.src1_ready,
                        rs.src1_value, rs.src1_tag);
            bind_source(fetched.inst.src2_used, fetched.inst.src2_reg, rs.src2_needed, rs.src2_ready,
                        rs.src2_value, rs.src2_tag);
            bind_source(fetched.inst.extra_used, fetched.inst.extra_reg, rs.extra_needed, rs.extra_ready,
                        rs.extra_value, rs.extra_tag);

            state.rs.push_back(rs);

            if (fetched.inst.has_dest && fetched.inst.dest_reg != 0u)
            {
                state.rat[fetched.inst.dest_reg] = static_cast<std::int64_t>(fetched.seq);
                ++m_metrics.register_renames;
            }
        }

        if (!state.if_stage && state.rob.size() < AdvancedState::ROB_CAPACITY)
        {
            FetchedInstruction fetched{};
            fetched.seq = state.next_seq++;

            try
            {
                const std::uint32_t raw = m_bus.read32(translate_address(state.fetch_pc, AccessType::FETCH));
                fetched.inst = decode_instruction(raw, state.fetch_pc);
            }
            catch (...)
            {
                fetched.inst = make_fetch_fault_instruction(state.fetch_pc);
            }

            const Prediction prediction = predict_next_pc(state, fetched.inst);
            fetched.predicted_taken = prediction.taken;
            fetched.predicted_next_pc = prediction.next_pc;
            state.fetch_pc = prediction.next_pc;
            state.if_stage = fetched;
            ++m_metrics.fetched_instructions;
        }

        if (!state.id_stage && state.if_stage)
        {
            state.id_stage = state.if_stage;
            state.if_stage.reset();
        }

        std::size_t issue_index = state.rs.size();
        for (std::size_t i = 0; i < state.rs.size(); ++i)
        {
            const auto &rs = state.rs[i];
            if (!operands_ready(rs) || rs.earliest_issue_cycle > m_cycle_count)
            {
                continue;
            }

            if (rs.inst.is_load)
            {
                const std::uint32_t load_addr = add_signed_offset(rs.src1_value, rs.inst.imm12);
                if (rob_has_older_pending_store(rs.seq, load_addr))
                {
                    ++m_metrics.load_store_order_stalls;
                    continue;
                }
            }

            FunctionalUnitState *fu = nullptr;
            switch (rs.inst.unit)
            {
            case UnitKind::Alu:
                if (!state.alu.busy)
                {
                    fu = &state.alu;
                }
                break;
            case UnitKind::MulDiv:
                if (!state.muldiv.busy)
                {
                    fu = &state.muldiv;
                }
                break;
            case UnitKind::Memory:
                if (!state.mem.busy)
                {
                    fu = &state.mem;
                }
                break;
            case UnitKind::Branch:
                if (!state.branch.busy)
                {
                    fu = &state.branch;
                }
                break;
            case UnitKind::System:
                if (!state.system.busy)
                {
                    fu = &state.system;
                }
                break;
            case UnitKind::None:
            default:
                break;
            }

            if (fu == nullptr)
            {
                continue;
            }

            issue_index = i;
            try_issue_to_fu(*fu, rs);
            if (ROBEntry *entry = find_rob_entry(state, rs.seq); entry != nullptr)
            {
                entry->issue_cycle = m_cycle_count;
            }
            ++m_metrics.issued_instructions;
            break;
        }

        if (issue_index < state.rs.size())
        {
            state.rs.erase(state.rs.begin() + static_cast<std::ptrdiff_t>(issue_index));
        }
        else if (!state.rs.empty())
        {
            ++m_metrics.issue_stalls;
        }

        ++m_cycle_count;

        if (committed_this_call)
        {
            return;
        }
    }
}

void CPU::dumpRegisters() const
{
    for (int i = 0; i < 32; ++i)
    {
        std::cout << "r" << std::dec << i << ": 0x" << std::hex << std::setw(8)
                  << std::setfill('0') << m_regs[i] << " ";
        if ((i + 1) % 4 == 0)
        {
            std::cout << "\n";
        }
    }
}

void CPU::dumpState() const
{
    std::cout << "PC: 0x" << std::hex << m_pc << " Cycles: " << std::dec << m_cycle_count
              << " Mode: " << getCoreModeName() << "\n";
}

} // namespace loongarch

#pragma once

#include "Device.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace loongarch
{

enum class AccessType
{
    FETCH,
    LOAD,
    STORE,
};

enum class CoreMode
{
    BaselineSequential,
    AdvancedTomasulo,
};

struct CoreMetrics
{
    std::uint64_t fetched_instructions{0};
    std::uint64_t decoded_instructions{0};
    std::uint64_t issued_instructions{0};
    std::uint64_t executed_instructions{0};
    std::uint64_t committed_instructions{0};
    std::uint64_t serialized_cycles{0};

    std::uint64_t branch_instructions{0};
    std::uint64_t dynamic_branch_predictions{0};
    std::uint64_t branch_prediction_hits{0};
    std::uint64_t branch_prediction_misses{0};
    std::uint64_t pipeline_flushes{0};
    std::uint64_t speculative_instructions_squashed{0};

    std::uint64_t register_renames{0};
    std::uint64_t out_of_order_completions{0};
    std::uint64_t load_store_forwardings{0};

    std::uint64_t rob_full_stalls{0};
    std::uint64_t rs_full_stalls{0};
    std::uint64_t decode_stalls{0};
    std::uint64_t issue_stalls{0};
    std::uint64_t load_store_order_stalls{0};

    std::uint64_t rob_occupancy_samples{0};
    std::uint64_t rs_occupancy_samples{0};
    std::uint64_t inflight_occupancy_samples{0};
    std::uint64_t max_rob_occupancy{0};
    std::uint64_t max_rs_occupancy{0};
    std::uint64_t max_inflight_instructions{0};
};

struct AdvancedState;

class CPU
{
  public:
    explicit CPU(Device &bus);
    ~CPU();

    CPU(const CPU &) = delete;
    CPU &operator=(const CPU &) = delete;
    CPU(CPU &&) noexcept;
    CPU &operator=(CPU &&) = delete;

    [[nodiscard]] Device &bus() noexcept;
    [[nodiscard]] const Device &bus() const noexcept;

    [[nodiscard]] std::uint32_t getPC() const noexcept;
    void setPC(std::uint32_t newPc) noexcept;

    [[nodiscard]] const std::uint32_t *registers() const noexcept;
    [[nodiscard]] std::uint32_t getReg(std::size_t index) const noexcept;
    void setReg(std::size_t index, std::uint32_t value) noexcept;

    void setCRMD(std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t getCRMD() const noexcept;

    void setPGDL(std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t getPGDL() const noexcept;

    void reset(std::uint32_t resetPc = 0) noexcept;
    void step();

    void signalInterrupt(std::uint32_t code) noexcept;

    [[nodiscard]] std::uint64_t getCycleCount() const noexcept;
    [[nodiscard]] CoreMode getCoreMode() const noexcept;
    [[nodiscard]] const char *getCoreModeName() const noexcept;
    [[nodiscard]] const CoreMetrics &getCoreMetrics() const noexcept;

    void dumpRegisters() const;
    void dumpState() const;

  private:
    Device &m_bus;
    std::uint32_t m_regs[32]{};
    std::uint32_t m_pc{0};

    std::uint32_t m_epc{0};
    std::uint32_t m_estat{0};
    std::uint32_t m_crmd{1};
    std::uint32_t m_ecfg{0xFFFF'FFFFu};

    bool m_interrupt_pending{false};
    std::uint32_t m_interrupt_code{0};
    std::uint32_t m_pgdl{0};
    std::uint64_t m_cycle_count{0};

    double m_fregs[32]{};
    std::uint32_t m_fcsr{0};
    std::uint8_t m_fcc{0};
    std::uint32_t m_csr[16384]{};
    bool m_llbit{false};
    std::uint32_t m_lladdr{0};

    CoreMode m_core_mode{CoreMode::AdvancedTomasulo};
    CoreMetrics m_metrics{};
    std::unique_ptr<AdvancedState> m_advanced;

    void stepBaseline();
    void stepAdvanced();

    void enforceInvariants() noexcept;
    void raise_exception(std::uint32_t ex_code) noexcept;
    std::uint32_t translate_address(std::uint32_t vaddr, AccessType type);

    [[nodiscard]] std::uint32_t read_u8(std::uint32_t vaddr);
    [[nodiscard]] std::uint32_t read_u16(std::uint32_t vaddr);
    [[nodiscard]] std::uint32_t read_u32(std::uint32_t vaddr);
    void write_u8(std::uint32_t vaddr, std::uint32_t value);
    void write_u16(std::uint32_t vaddr, std::uint32_t value);
    void write_u32(std::uint32_t vaddr, std::uint32_t value);
};

} // namespace loongarch

#pragma once

#include "Device.h" // 继承自 Device 基础设备类
#include <cstdint>

namespace loongarch
{

/**
 * @brief 测试专用的外设虚拟设备（Test MMIO Device）。
 * 
 * 这是一个极简的、不存在于现实中的虚拟硬件！它仅仅是为了我们在测试 CPU 时，
 * 让跑在里面的测试软件能够通过往特定地址“写信”，来跟我们外部的主体测试程序交流。
 * 例如：如果你往测试设备的特定门牌写个状态码，外面就知道“阿，程序的这一关测试通过了！”。
 */
class TestDevice : public Device
{
  public:
    // 这台设备挂在总线上的基地址（门牌号）。根据龙芯教学版的约定，它在物理地址的末端区域。
    static constexpr std::uint32_t BASE_ADDR = 0x1FFFF000u;
    
    // 这台测试设备占地多大空间？答：0x1000（即 4096 字节 = 4KB）
    static constexpr std::uint32_t SIZE = 0x1000u;

    /// @copydoc Device::read32
    /// 重写读取操作（目前在这个测试设备里，不管读什么都返回0，没啥用）
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;
    
    /// @copydoc Device::write32
    /// 重写写入操作，这是核心！
    /// 如果程序向它写数据，它就能捕捉到信号：
    /// 它用来接收测试程序发出的halt（停机）命令和 exit_code（退出结果号）。
    void write32(std::uint32_t addr, std::uint32_t value) override;

    /// @return 询问该设备：里面的测试程序发出过“我跑完了请停下(halt)”的申请了吗？
    [[nodiscard]] bool halted() const noexcept
    {
        return m_halted;
    }
    
    /// @return 提取出测试程序在停下前丢给我们的退出码（0为成功，借此验证用例对不对）。
    [[nodiscard]] std::uint32_t exitCode() const noexcept
    {
        return m_exit_code;
    }

    /// 重新清点重置这个测试器，擦除之前的记录。
    void reset() noexcept
    {
        m_halted = false;
        m_exit_code = 0;
    }

  private:
    bool m_halted{false};          // 记录内部是否有停机请求的私有状态标志
    std::uint32_t m_exit_code{0};  // 记录那份退场的证明（退出码）
};

} // namespace loongarch
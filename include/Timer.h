/**
 * @file Timer.h
 * @brief 带有中断生成能力的内存映射定时器（Timer）外设设备。
 *
 * 这也是个继承自 Device 的类。定时器的工作原理就像是一个秒表和闹钟的结合体。
 * 它的心跳（tick）由主循环推着走（CPU每次执行完一条指令就推它一下）。
 * 当它跳动的次数达到设定好的阈值（threshold）时，就会触发一个闹钟（生成挂起的中断信号），
 * 这样 CPU 就知道时间到了，该去处理中断任务了。
 *
 * 它的MMIO（内存映射输入输出）寄存器长这样（通过不同的地址偏移控制闹钟）：
 *   偏移量（门牌加几）
 *   Offset 0x00 – TVAL (只读)   : 看看当前秒表已经走到哪个数字了（返回秒表数目的低32位）
 *   Offset 0x04 – TCFG (读/写)  : 设置闹铃的阈值（比如填入1000，就是每走1000步响一次）
 *   Offset 0x08 – TCTL (读/写)  : 定时器的遥控器，最低位(bit 0)是开关按钮（1就是开启闹钟，0就是关闭）
 *   Offset 0x0C – TCLR (只写)   : 这是一个关闹钟按键！当你处理完中断后，想清静，就往这个地址随便写点啥，之前的报警声就撤销了。
 */

#pragma once

#include "Device.h" // 只要是挂在总线上的外设，都得继承 Device

#include <cstdint>

namespace loongarch
{

class Timer final : public Device
{
  public:
    Timer() = default;
    ~Timer() override = default;

    // 定时器这种硬件也是全球唯一的，禁用复制
    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;
    Timer(Timer &&) = default;
    Timer &operator=(Timer &&) = default;

    /// \brief 定时器在系统内存映射地图中的固定实体门牌号(基地址)。
    static constexpr std::uint32_t PhysicalBase = 0x1FE0'0100u;
    /// \brief 这种定时器一共有4个功能小抽屉（每个占4个字节），所以它的总占地大小是 16 字节。
    static constexpr std::uint32_t RangeSizeBytes = 16u;

    // ── 硬件驱动和 CPU 会调用的接口（MMIO interface） ──────────────────────────────────────────────
    
    // 如果 CPU 对定时器地址发出 read32，就能看到闹铃状态或时间值
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;
    
    // 如果 CPU 对定时器地址发出 write32，就能设定阈值、拨动开关或清中断
    void write32(std::uint32_t addr, std::uint32_t value) override;

    // ── 模拟器本体大循环会调用的后台黑盒接口（Simulation interface） ────────────────────────────────────────
    /// 让内部分内部秒表向前“滴答”跳过一个单位周期。
    /// 如果你拨开了开关（enabled=true），并且秒表计数走到了之前设定的闹铃数字，
    /// 那它便会翻起一个牌子（m_interruptPending 设为 true），大呼“起中断了喂！”。
    void tick();

    /// @return 外界可以用此询问：“闹钟响没响啊？” 返回 true 就是有中断正等着被接见。
    [[nodiscard]] bool pending() const noexcept;

    /// 撤下等待接见的中断牌子（一般在 CPU 处理完了之后掉用此方法）。
    void clearPending() noexcept;

  private:
    std::uint64_t m_ticks{0};             // 目前滴答了多少下（64位超级大数字）
    std::uint32_t m_threshold{0};         // 闹铃的数字。等于 0 代表闹铃处于罢工状态，不会响
    bool m_enabled{false};                // 这台定时器的总电源开关状态
    bool m_interruptPending{false};       // “我响了！请处理中断！” —— 这个标志代表当前是否有尚未处理的中断
};

} // namespace loongarch

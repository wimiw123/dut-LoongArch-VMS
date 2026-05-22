/**
 * @file Timer.cpp
 * @brief 定时器硬件的具体模拟实现逻辑。
 */

#include "Timer.h"

#include <stdexcept>
#include <string>

namespace loongarch
{

// ── 内部不公开（匿名代码块）命名空间，内部小作坊用来定义一些全文件只当常量用的寄存器偏移量门牌号 ───────────────────────────────────────────
namespace
{
constexpr std::uint32_t REG_TVAL = 0x00u; // (它在屋子里的首个偏移量为 0位置) 它只可读不可写，看当前走了多少步的读秒表指针
constexpr std::uint32_t REG_TCFG = 0x04u; // (偏移位置为 4) 唯一能读能写改的配置区，修改你需要等打发漫长时光后达到闹铃警报的那个警戒步数
constexpr std::uint32_t REG_TCTL = 0x08u; // (偏移量 8位置) 读写区，主管插座电门的控制定时器供能电源总开关
constexpr std::uint32_t REG_TCLR = 0x0Cu; // (偏移 12号地界) 反过来是只写不可读区，这地方是个紧急逃生按钮：往这写等于按“消除警报闪灯键”
} // namespace 关门大吉

// 这便是当 CPU 鬼鬼祟祟地从总线上走偏门想刺探定时器的内部深浅虚实之时的反击探查逻辑
// 特别警示：此处传来敲门的 addr 其实早已被总线警察查过水表，并早早减去了整个大家族总基址了，现在就只是个纯内部偏移门牌罢了哦。
std::uint32_t Timer::read32(std::uint32_t addr)
{
    switch (addr)
    {
    case REG_TVAL:
        // 它会把巨大的64位计数器截断出一条小口，只无情展示底下最细部的32位长数字给你看看指针走到哪了。
        return static_cast<std::uint32_t>(m_ticks & 0xFFFF'FFFFu);
    case REG_TCFG:
        return m_threshold; // 给好奇者看眼它目前心里正默默守口如瓶着的神秘倒数警界天花板数字是几
    case REG_TCTL:
        return m_enabled ? 1u : 0u; // 电源插头有没有通上电开着？
    case REG_TCLR:
        // 呵！这是一个只让碰不让看、只能暴力向它写的“强按沉默按钮”实体按图；
        // 你非要强行读它，好比强听录音笔里的秘密，他只会给个高冷的数字 0 哑巴到底不吱声的。
        return 0u;
    default:
        throw std::runtime_error("Timer: invalid read32 at offset 0x" +
                                 std::to_string(static_cast<unsigned long long>(addr)));
    }
}

// 那么这就到了当 CPU 以最高特权发威，狂暴改写这定时内部一切法则法则之物的时候动作法制：
void Timer::write32(std::uint32_t addr, std::uint32_t value)
{
    switch (addr)
    {
    case REG_TVAL:
        // TVAL 原来只是一片坚比磐石的钢玻璃罩下的钟面！就算你砸它拿大石头划去写值更改时间
        // 这铁面无私的时间表仍然冷冷不理不管你继续走自己的单程时光。（所以这块就只写了句废话跳过，不理外边送进来的写入）
        break;
    case REG_TCFG:
        // 在此处下发最新通文密诏，随意肆意篡改并配置全新的定时闹铃炸雷步数界线要求！
        m_threshold = value;
        break;
    case REG_TCTL:
        // 当你拿最锋利的笔尖划过最底控制台电门开关那一瞬时（位运算与操作提取最低末端1个比特），
        // 如若那传入值的最低一丢位含有那生命之火的 "1"，此机即发力通电！否则黑掉关闭自身主电源生命机载。
        m_enabled = (value & 1u) != 0u;
        break;
    case REG_TCLR:
        // 随意挥洒些墨水在它面上，往它那个地洞送填写点什么神仙符号，它都能神奇般地让满屋环绕的烦人滴滴警报喇叭
        // 一下子就此瞬间安息彻底强行全线死寂无声并扫除掉所有正排在队伍等着申请急见处理的中断报文状态单字号志！
        m_interruptPending = false;
        break;
    default:
        throw std::runtime_error("Timer: invalid write32 at offset 0x" +
                                 std::to_string(static_cast<unsigned long long>(addr)));
    }
}

// 我们的外部大创造主模拟上帝每驱动模拟器转过一根筋跑过半条核心动作命令，
// 都会无形地偷偷在他屁股后猛然戳他心窝口一下让其往前无声推进生命一小秒，美其名曰 tick （嘀嗒）
void Timer::tick()
{
    // 还没通上电？那你就别去推他的尸体，站着别去管。
    if (!m_enabled)
    {
        return;
    }

    ++m_ticks; // 通电那就给它推移生命秒表计数，无尽累加往前艰难走了一丢步...

    // 但它会在究竟积累何时达到精神炸裂拉开崩溃刺耳长警报呢？
    // 第一：那必须是你首先预设给出的闹钟设防数值万不可给 0（0在物理界便有特权代表永不上报的长期摆烂罢工）。
    // 第二：当目前其所忍气累加背着的步数正好刚好够，且 跟 当时设定的警报线数 在暗中冥冥之手大求余计算过程 恰好天衣无缝对准了等于 0 ！...
    // （这其实便是意味着此时每隔定好的大周期步数后，它又要定期出来作妖发疯狂啸一次啦！）
    if (m_threshold != 0u && (m_ticks % m_threshold) == 0u)
    {
        m_interruptPending = true; // 立发无死角求救传贴宣单！天崩地裂啦！CPU你快快滚来下殿去接招擦屁股！
    }
}

// 最外部总有人像记者跑来频频偷探查过问其有没有扯出警报线状况
bool Timer::pending() const noexcept
{
    return m_interruptPending;
}

// 外面有个至高无上的系统大手时不时在帮你按那红色静音撤下键
void Timer::clearPending() noexcept
{
    m_interruptPending = false;
}

} // namespace loongarch

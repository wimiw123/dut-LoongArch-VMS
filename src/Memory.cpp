/**
 * @file Memory.cpp
 * @brief 最简单直接的物理内存模拟实现文件。
 */

#include "Memory.h"

#include <stdexcept>
#include <string>

namespace loongarch
{

// 内存出厂初始化
Memory::Memory(std::size_t sizeBytes) : m_data() // 唤醒那个空的 vector 动态大数组
{
    // 如果开机发现你想要个 0容量 的内存，那这电脑没法用，直接异常伺候
    if (sizeBytes == 0U)
    {
        throw std::runtime_error("Memory: sizeBytes must be greater than zero");
    }

    // 这句就是向真正的电脑操作系统说：“借我 sizeBytes 这么大的运行内存用一下”。
    // resize 会一口气开拓出一大片连着的物理磁盘空间，并贴心地给每格字节都洗刷干净初始化成 0（清空内存防乱码）
    m_data.resize(sizeBytes, static_cast<std::uint8_t>(0U));
}

// 查看内存条容量大小
std::size_t Memory::size() const noexcept
{
    return m_data.size();
}

// 从模拟内存里读取 32位数据（4个小字节拼凑版）
std::uint32_t Memory::read32(std::uint32_t addr)
{
    // 本次访问要找多宽的目标？32位正好是 4个字节
    constexpr std::size_t accessSize = sizeof(std::uint32_t); 
    
    // 第一步：先让前台保安查身份，看地址到底有没有越位？有没有非分之想要求不对齐的错乱访问！
    checkAlignedAndInRange(addr, accessSize);

    // 类型转换强制转化一下，为了等下能在 vector 里面查精准位置(下标)
    const auto base = static_cast<std::size_t>(addr);

    std::uint32_t value = 0;
    // 小端序(Little-Endian)拼积木大法：从低地址向高地址依次装配！
    // 假设内存里此时存着 [AA] [BB] [CC] [DD]，要拼凑成 D D C C B B A A 还原输出。
    // 第 0 号字节被原封不动地强行塞在最右边底部
    value |= static_cast<std::uint32_t>(m_data[base]);
    // 第 1 号要被往左边挪动 8 位（也就是往左跨越了一整格）再拼上去
    value |= static_cast<std::uint32_t>(m_data[base + 1U]) << 8U;
    // 第 2 号也往左狂挪 16 位拼上去
    value |= static_cast<std::uint32_t>(m_data[base + 2U]) << 16U;
    // 第 3 号是最左的顶级部位，往往左闪现挪动到了 24 位后拼上！组合成最终大 boss 完整32位字返回！
    value |= static_cast<std::uint32_t>(m_data[base + 3U]) << 24U;

    return value;
}

// 向模拟内存写入纯 32位（4个并列字节）数据
void Memory::write32(std::uint32_t addr, std::uint32_t value)
{
    constexpr std::size_t accessSize = sizeof(std::uint32_t);
    // 第一步同样去过大门前台安检
    checkAlignedAndInRange(addr, accessSize);

    const auto base = static_cast<std::size_t>(addr);

    // 这次是把一个大 boss “大卸八块”劈成 4 瓣塞进小方格里（由于平台习惯，也继续用小端序劈开）
    // 先用 & 0xFF 掩码抠下最右边的底层 8位 放进内存里最前面（0基址）的那个小格子里
    m_data[base] = static_cast<std::uint8_t>(value & 0xFFU);
    // 然后眼光看向右移走的次低位 8位后再扣走它的右心，塞进第2个格子里里
    m_data[base + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    // ...以此继续拆迁大楼类推
    m_data[base + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    m_data[base + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

// 模拟系统中最硬核无私的异常保安大门检查系统（内存越权与越界报错拦截）
void Memory::checkAlignedAndInRange(std::uint32_t addr, std::size_t accessSize) const
{
    if (accessSize == 0U)
    {
        throw std::runtime_error("Memory: accessSize must be greater than zero");
    }

    if ((addr % accessSize) != 0U)
    {
        throw std::runtime_error("Memory: unaligned access at address 0x" +
                                 std::to_string(static_cast<unsigned long long>(addr)));
    }

    const auto base = static_cast<std::size_t>(addr);
    if (accessSize > m_data.size() || base > m_data.size() - accessSize)
    {
        throw std::runtime_error("Memory: out-of-range access at address 0x" +
                                 std::to_string(static_cast<unsigned long long>(addr)));
    }
}

} // namespace loongarch

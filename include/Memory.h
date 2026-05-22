/**
 * @file Memory.h
 * @brief 简单的物理内存设备实现代码。
 *
 * 这个头文件声明了一个用连续的大数组来假装（模拟）出来的 RAM（运行内存）设备。
 * 它继承了 Device 接口，并作为 LoongArch 模拟器的主要物理内存来使用。
 */

#pragma once // 确保头文件防重复包含

// 引入相关的标准库
#include "Device.h" // 继承自基础设备接口
#include <cstddef>  // 引入 std::size_t（用于表示大小的无符号类型）
#include <cstdint>  // 引入 uint8_t, uint32_t 等固定宽度的整型
#include <vector>   // 引入动态数组 vector，这个类底层就是用它装字节的

namespace loongarch
{

/**
 * @brief 简单的基于字节寻址的物理内存设备。
 *
 * 这块内部由 C++ 里的 `std::vector<uint8_t>`（存了无数个1字节的动态数组）作为幕后存储器。
 * 它可以支持标准的 32位 小端序、并且是对齐的内存读写访问。
 *
 * - 它的默认容量被设定为 16 MiB，但其实可以在创建的时候灵活定制。
 * - 警告：由于我们每次要求的是 32位（4字节）的数据，所以给它的访问地址 addr 必须是 4 的倍数（这叫 4字节对齐）！
 *   如果你给的地址没对齐或超出了规定的内存大小地盘，它会大发雷霆扔出 `std::runtime_error` 异常。
 */
class Memory final : public Device // 最终版本，不让别人再继承和修改了
{
  public:
    /// 提供一个公开说明：这个模拟内存的出厂默认大小是 16 MiB (即 16 * 1024 * 1024 字节)
    static constexpr std::size_t DefaultSizeBytes = static_cast<std::size_t>(16u) * 1024u * 1024u;

    /**
     * @brief 造一块新内存设备。
     *
     * @param sizeBytes 告诉内存你想让它有多大容量（单位是字节）。如果不指定，它就会使用 DefaultSizeBytes（也就是16MB）。
     *        大小必须是非0正数，调用的人以后要保证自己读写的地址别超出这个自定范围。
     *
     * @throws std::runtime_error 如果有人调皮要求造一块容量为 0 的内存，当场报错。
     */
    explicit Memory(std::size_t sizeBytes = DefaultSizeBytes);

    // 删除掉拷贝构造和赋值等危操作，防止有人不知情下将这块大内存原地复制两份占用极大数据
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    // 允许通过移动构造转移使用权
    Memory(Memory &&) = default;
    Memory &operator=(Memory &&) = default;

    /// @return 告诉外界我这块物理内存实际上到底有多少个字节。
    [[nodiscard]] std::size_t size() const noexcept;

    /// @copydoc Device::read32
    /// （覆盖父类的实现）实现从这块内存读特定地址处（地址必须4对齐）读出4个字节并组装成一个 32位数据返回。
    [[nodiscard]] std::uint32_t read32(std::uint32_t addr) override;

    /// @copydoc Device::write32
    /// （覆盖父类的实现）实现向这块内存特定地址处写入一个被拆分为4个字节的 32位数据。
    void write32(std::uint32_t addr, std::uint32_t value) override;

  private:
    // 这里是这片虚拟内存真正的心脏：
    // std::vector 是 C++ 中的动态数组。这里放了 sizeBytes 那么多个 8位无符号整数（uint8_t 就是 1个字节）。
    // 它等于是在真实机器的堆上申请了一大片连续的内存，来“冒充”我们模拟器的机器 RAM 内存芯片。
    std::vector<std::uint8_t> m_data;

    /// 内部保安大叔：检查负责监督别人传进来的 32位访问请求地址 addr。
    /// 第一是不是 4的倍数对齐了？第二是不是超出了我们这栋内存大楼的最大容量门牌号？
    void checkAlignedAndInRange(std::uint32_t addr, std::size_t accessSize) const;
};

} // namespace loongarch

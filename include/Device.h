/**
 * @file Device.h
 * @brief LoongArch 模拟器中所有硬件设备的抽象基类。
 *
 * 这个头文件定义了一个公共的、基于 32位 内存映射访问机制的接口模板。
 * 只要是挂在模拟总线上的设备（比如内存、串口等），都必须继承这个类并实现这些接口。
 */

#pragma once // 确保此头文件在一次编译中只被包含一次

#include <cstdint> // 引入标准整数类型，如 std::uint32_t

namespace loongarch // 这些代码属于 LoongArch 命名空间
{

/**
 * @brief 所有内存映射设备（Memory-mapped devices）的抽象基类。
 *
 * 所谓的“内存映射设备”，就是指 CPU 把这块硬件当成普通的内存地址来读写。
 * 一个设备会向外暴露一组 32位 的读写接口，CPU 使用该设备自己的物理地址（偏移量）来访问它。
 *
 * 任何继承这个类的具体设备（如真实内存器、串口控制器等）都必须确保：
 * 它们的实现能安全处理模拟器发起的合法访问（比如内存对齐、不越界等）；
 * 如果收到了违规非法的访问请求，它们应该通过抛出异常的方式来抗议（报错）。
 */
class Device
{
  public:
    // 默认的构造函数，不需要特别做什么事
    Device() = default;
    
    // 虚析构函数（virtual ~Device）。这是C++里很基础的一点：基类的析构函数必须是虚函数！
    // 这样当别人通过 Device 的指针去删除一个子类（例如 Memory）对象时，
    // 子类的析构代码才能被正确触发，防止内存泄漏。
    virtual ~Device() = default;

    // 禁用设备的拷贝构造。在物理世界里，你不能轻易地用等号把一块真实的串口芯片“复制”成两块对吧？
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    
    // 允许设备的移动语义。比如可以把设备的控制权从一个变量安全转移给另一个变量
    Device(Device &&) = default;
    Device &operator=(Device &&) = default;

    /**
     * @brief 从该设备中读取一个 32位 小端序（little-endian）的值。
     * 
     * @note "= 0" 表示这是一个纯虚函数（pure virtual function）。
     * 这就等于签订了一个强制契约，任何继承 Device 的底层设备，必须自己掏代码把这个读取方法实现了，
     * 否则它就不能被生造（实例化）出来。
     *
     * @param addr 要在这个设备上的哪个地址进行读取。
     * @return 最终读出交回给调用者的 32 位数值。
     *
     * @throws std::runtime_error 如果遇到非法的地址（比如你设备小，他非要读一万里外的内存，或未对齐的地址），可能抛出异常。
     */
    [[nodiscard]] virtual std::uint32_t read32(std::uint32_t addr) = 0;

    /**
     * @brief 向该设备中写入一个 32位 小端序（little-endian）的值。
     *
     * 同上，纯虚函数，子类必须实现。
     * 
     * @param addr 你打算在这个设备的哪个地址写入数据。
     * @param value 要写过去的 32位 实际数据。
     *
     * @throws std::runtime_error 如果遇到越界等情况同样也可能抛出异常。
     */
    virtual void write32(std::uint32_t addr, std::uint32_t value) = 0;
};

} // namespace loongarch

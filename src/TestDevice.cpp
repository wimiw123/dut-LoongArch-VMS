// 测试设备专用实现代码

#include "TestDevice.h"
#include <stdexcept>

namespace loongarch
{

std::uint32_t TestDevice::read32(std::uint32_t addr)
{
    // 算出该次找上的相对门牌号
    const std::uint32_t offset = addr - BASE_ADDR;

    switch (offset)
    {
    case 0x0:
        // 如果想问它偏移量恰好为 0 的首排寄存格子，它就会老实回答当前记载着的“退出码记录”
        return m_exit_code;
    case 0x4:
        // 别人若查岗在偏移量为 4 的格子里，它则反映此时到底机缘停下没停下（给出 1为停机死，0为还没有）
        return m_halted ? 1u : 0u;
    default:
        // 如果想问的地方不是0也不是4，纯属查岗找茬瞎指路，这黑盒子脾气差直接报错怼回去！
        throw std::runtime_error("TestDevice: invalid read offset");
    }
}

void TestDevice::write32(std::uint32_t addr, std::uint32_t value)
{
    const std::uint32_t offset = addr - BASE_ADDR;

    switch (offset)
    {
    case 0x0:
        // 如果跑在机器体内的测试专用汇编小程序，想下地府交差往我首个格子这里死命写一个遗言数字的时候
        // 那么我就懂了！大体记录下这条绝笔数字当作“程序归天的退出身份码”（存入m_exit_code库中）
        m_exit_code = value;
        // 并且瞬间向监控室外拉响物理红灯警报器：各位！有人拍下了那该死的结束进程按钮！！！（拉高停机线）
        m_halted = true;
        break;
    default:
        throw std::runtime_error("TestDevice: invalid write offset");
    }
}

} // namespace loongarch
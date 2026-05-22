#include "ProgramLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace loongarch
{

ProgramLoader::ProgramLoader(Memory &memory) noexcept : m_memory(memory)
{
}

// 加载十六进制的文本指令文件
std::uint32_t ProgramLoader::loadHexFile(const std::string &path, std::uint32_t load_addr)
{
    // 尝试打开文件
    std::ifstream fin(path);
    if (!fin)
    {
        throw std::runtime_error("ProgramLoader: cannot open file: " + path);
    }

    std::string line;
    std::uint32_t count = 0;         // 记数装了多少条指令了
    std::uint32_t addr = load_addr;  // 装载指针：目前填坑填到内存的哪里了

    // 逐行读取文件内容
    while (std::getline(fin, line))
    {
        // 忽略空行
        if (line.empty())
        {
            continue;
        }

        // 提取每行的第一个有效连续字符词团
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        if (token.empty())
        {
            continue;
        }

        // 以 '#' 开头的是注释说明，忽略不管
        if (token[0] == '#')
        {
            continue;
        }

        // 走到这一步，token 里装的就是写成16进制字符串样式的真枪实弹的 32位机器码！
        std::uint32_t word = 0;
        std::stringstream ss;
        ss << std::hex << token; // 挂上 16进制滤镜读进来
        ss >> word;              // 转换成了代码能认的纯数字！

        if (ss.fail())
        {
            throw std::runtime_error("ProgramLoader: invalid hex word: " + token);
        }

        // 塞进物理模拟主存中去
        m_memory.write32(static_cast<std::uint32_t>(addr), word);
        addr += 4; // 装完一条指令，往下挪 4个字节的坑位
        ++count;
    }

    return count;
}

// 加载纯纯的二进制文件
std::size_t ProgramLoader::loadBinFile(const std::string &path, std::uint32_t load_addr)
{
    // 使用 std::ios::binary 挂挡：禁止它乱翻译换行符等，要读原始数据
    std::ifstream fin(path, std::ios::binary);
    if (!fin)
    {
        throw std::runtime_error("ProgramLoader: cannot open file: " + path);
    }

    // 这一招是奇技淫巧：直接把文件里所有的二进制原始数据一口气兜底吸到 string 字符串大麻袋里来！
    std::string data((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());

    if (data.empty())
    {
        throw std::runtime_error("ProgramLoader: binary file is empty: " + path);
    }

    // 龙芯机器码死规矩：所有的指令长度都必须是刚好占 4个字节。所以如果总容量余4不为0就是损坏的。
    if (data.size() % 4 != 0)
    {
        throw std::runtime_error("ProgramLoader: binary size is not a multiple of 4 bytes: " +
                                 path);
    }

    std::uint32_t addr = load_addr;
    // 每次迈 4 步（处理 4 个小字节拼凑成一条完整大指令）
    for (std::size_t i = 0; i < data.size(); i += 4)
    {
        // 取出当前进度切片指针
        const auto *p = reinterpret_cast<const unsigned char *>(data.data() + i);
        // 手搓小端序拼图（最左边的小字节放在最低权位，以此向高进）：
        std::uint32_t word =
            static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
            (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);

        // 塞进去！
        m_memory.write32(static_cast<std::uint32_t>(addr), word);
        addr += 4;
    }

    return data.size();
}

// 自动挡装载机
std::size_t ProgramLoader::loadFileAuto(const std::string &path, std::uint32_t load_addr)
{
    // 如果文件名有后缀而且末尾4位叫 ".hex"，就去用上面的 loadHexFile 工具
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".hex")
    {
        return loadHexFile(path, load_addr);
    }

    // 如果是 ".bin"，使用二进制粗粒度推土机法
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".bin")
    {
        return loadBinFile(path, load_addr);
    }

    throw std::runtime_error("ProgramLoader: unsupported file extension: " + path);
}

} // namespace loongarch

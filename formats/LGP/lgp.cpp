#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include "../tool_common.hpp"
struct LgpSection
{
    char name;
    std::uint32_t count;
    std::uint32_t recordSize;
    std::uint64_t offset;
    std::uint64_t size;
};
class LgpFile
{
public:
    explicit LgpFile(const std::string& path)
        : path(path), data(tool::readFile(path))
    {
        if (data.size() < 0x18)
            fail("truncated 0x18-byte header");
        for (std::size_t i = 0; i < header.size(); ++i)
            header[i] = tool::readLe32(data, i * 4, "LGP header word");
        static constexpr std::array<char, 4> names{'A', 'B', 'C', 'D'};
        static constexpr std::array<std::size_t, 4> countWords{2, 3, 5, 4};
        static constexpr std::array<std::uint32_t, 4> recordSizes{0x94, 0x110, 0x44, 0xA8};
        std::uint64_t cursor = 0x18;
        for (std::size_t i = 0; i < sections.size(); ++i)
        {
            auto& section = sections[i];
            section.name = names[i];
            section.count = header[countWords[i]];
            section.recordSize = recordSizes[i];
            section.offset = cursor;
            section.size = tool::checkedMultiply(section.count, section.recordSize, "LGP section size");
            const std::uint64_t end = tool::checkedAdd(cursor, section.size, "LGP section end");
            if (end > data.size())
            {
                fail(std::string("section ") + section.name + " is truncated at file offset 0x" + hex(cursor));
            }
            cursor = end;
        }
        consumedSize = cursor;
    }
    void inspect() const
    {
        std::cout << "file: " << path << '\n';
        std::cout << "size: 0x" << std::hex << std::uppercase << data.size()
                  << std::dec << " (" << data.size() << ")\n";
        std::cout << "header:";
        for (const std::uint32_t word : header)
            std::cout << ' ' << word;
        std::cout << '\n';
        for (const auto& section : sections)
        {
            std::cout << "section " << section.name
                      << ": count=" << section.count
                      << " recordSize=0x" << std::hex << std::uppercase << section.recordSize
                      << " range=[0x" << section.offset << ",0x" << section.offset + section.size << ')'
                      << " size=0x" << section.size << std::dec << '\n';
        }
        std::cout << "trailing: 0x" << std::hex << std::uppercase
                  << data.size() - consumedSize << std::dec << " bytes\n";
    }
private:
    std::string path;
    tool::Bytes data;
    std::array<std::uint32_t, 6> header{};
    std::array<LgpSection, 4> sections{};
    std::uint64_t consumedSize{};
    [[noreturn]] void fail(const std::string& message) const
    {
        throw std::runtime_error(path + ": " + message);
    }
    static std::string hex(std::uint64_t value)
    {
        std::ostringstream output;
        output << std::hex << std::uppercase << value;
        return output.str();
    }
};
static void printUsage(const char* argv0)
{
    std::cerr << "usage:\n"
              << "  " << argv0 << " inspect <input.lgp>\n"
              << "  " << argv0 << " validate <input.lgp>\n";
}
int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printUsage(argv[0]);
        return 1;
    }
    const std::string command = argv[1];
    if (command != "inspect" && command != "validate")
    {
        printUsage(argv[0]);
        return 1;
    }
    try
    {
        const LgpFile lgp(argv[2]);
        if (command == "inspect")
            lgp.inspect();
        else
            std::cout << "valid: " << argv[2] << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

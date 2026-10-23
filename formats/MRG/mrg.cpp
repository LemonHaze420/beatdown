#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../tool_common.hpp"
struct MrgEntry
{
    std::uint32_t offset;
    std::uint32_t length;
};
class MrgFile
{
public:
    explicit MrgFile(const std::string& path)
        : path(path), data(tool::readFile(path)), fileName(std::filesystem::path(path).filename().string())
    {
        const std::uint32_t count = tool::readLe32(data, 0, "MRG entry count");
        const std::uint64_t tableSize = tool::checkedMultiply(count, 8, "MRG directory size");
        directoryEnd = tool::checkedAdd(4, tableSize, "MRG directory end");
        if (directoryEnd > data.size())
            fail("truncated entry directory");
        entries.reserve(count);
        std::uint32_t previousOffset = 0;
        std::uint64_t previousNonemptyEnd = directoryEnd;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const std::uint64_t directoryOffset = 4 + static_cast<std::uint64_t>(i) * 8;
            MrgEntry entry{
                tool::readLe32(data, directoryOffset, "MRG entry offset"),
                tool::readLe32(data, directoryOffset + 4, "MRG entry length")
            };
            if (entry.offset < directoryEnd)
                fail("entry " + std::to_string(i) + " begins inside the directory");
            if (i != 0 && entry.offset < previousOffset)
                fail("entry " + std::to_string(i) + " has a decreasing physical offset");
            if (!tool::spanWithin(entry.offset, entry.length, data.size()))
                fail("entry " + std::to_string(i) + " is out of range");
            if (entry.length != 0 && entry.offset < previousNonemptyEnd)
                fail("entry " + std::to_string(i) + " overlaps the preceding nonempty entry");
            if (entry.length != 0)
                previousNonemptyEnd = static_cast<std::uint64_t>(entry.offset) + entry.length;
            previousOffset = entry.offset;
            entries.push_back(entry);
        }
        validateProfile();
    }
    void inspect() const
    {
        std::cout << "file: " << path << '\n'
                  << "profile: " << profileName() << '\n'
                  << "size: 0x" << std::hex << std::uppercase << data.size()
                  << std::dec << " (" << data.size() << ")\n"
                  << "entries: " << entries.size() << '\n'
                  << "directory: [0x0,0x" << std::hex << std::uppercase << directoryEnd << ")\n";
        std::uint64_t previousEnd = directoryEnd;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            const auto& entry = entries[i];
            if (entry.offset > previousEnd)
            {
                std::cout << "gap before entry " << std::dec << i << ": [0x"
                          << std::hex << std::uppercase << previousEnd << ",0x" << entry.offset
                          << ") size=0x" << entry.offset - previousEnd << '\n';
            }
            std::cout << "entry " << std::dec << i
                      << ": offset=0x" << std::hex << std::uppercase << entry.offset
                      << " length=0x" << entry.length;
            if (entry.length >= 4)
            {
                const std::uint32_t chunkId = tool::readLe32(data, entry.offset, "entry chunk ID");
                std::cout << " firstU32=0x" << chunkId;
            }
            std::cout << '\n';
            if (entry.length != 0)
                previousEnd = static_cast<std::uint64_t>(entry.offset) + entry.length;
        }
        if (previousEnd < data.size())
        {
            std::cout << "trailing gap: [0x" << std::hex << std::uppercase << previousEnd
                      << ",0x" << data.size() << ") size=0x" << data.size() - previousEnd << '\n';
        }
        else
            std::cout << "trailing gap: 0x0 bytes\n";
    }
private:
    std::string path;
    tool::Bytes data;
    std::string fileName;
    std::vector<MrgEntry> entries;
    std::uint64_t directoryEnd{};
    std::string profileName() const
    {
        if (fileName == "ITEM.MRG")
            return "ITEM";
        if (fileName == "CSHOP.MRG" || fileName == "SHOP.MRG")
            return "SHOP";
        if (fileName == "CITEM.MRG" || fileName.starts_with("CITEM_"))
            return "CITEM";
        return "unknown";
    }
    void validateProfile() const
    {
        const std::string profile = profileName();
        if (profile == "ITEM" && entries.size() != 8)
            fail("ITEM profile requires 8 entries");
        if ((profile == "SHOP" || profile == "CITEM") && entries.size() != 3)
            fail(profile + " profile requires 3 entries");
        if (!entries.empty() && entries[0].length >= 4)
        {
            const std::uint32_t chunkId = tool::readLe32(data, entries[0].offset, "entry-0 chunk ID");
            if ((profile == "CITEM" || fileName == "CSHOP.MRG") && chunkId != 0x16)
                fail(profile + " entry 0 is not a texture dictionary chunk (0x16)");
            if ((fileName == "ITEM.MRG" || fileName == "SHOP.MRG") && chunkId != 0x23)
                fail(profile + " entry 0 is not the observed proprietary chunk 0x23");
        }
    }
    [[noreturn]] void fail(const std::string& message) const
    {
        throw std::runtime_error(path + ": " + message);
    }
};
static void printUsage(const char* argv0)
{
    std::cerr << "usage:\n"
              << "  " << argv0 << " inspect <input.mrg>\n"
              << "  " << argv0 << " validate <input.mrg>\n";
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
        const MrgFile mrg(argv[2]);
        if (command == "inspect")
            mrg.inspect();
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

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "../tool_common.hpp"
struct DatOwner
{
    std::uint32_t npcIndex;
    std::uint32_t stageIndex;
};
struct DatSpan
{
    std::uint64_t begin;
    std::uint64_t end;
    std::string description;
};
class DatFile
{
public:
    explicit DatFile(const std::string& path)
        : path(path), data(tool::readFile(path))
    {
        if (data.size() < 128 * 4)
            fail("truncated 128-entry NPC root table");
        std::vector<DatSpan> spans{{0, 128 * 4, "NPC root table"}};
        std::map<std::uint32_t, std::vector<std::uint32_t>> tableOwners;
        for (std::uint32_t npc = 0; npc < rootOffsets.size(); ++npc)
        {
            const std::uint32_t offset = tool::readLe32(data, npc * 4, "NPC table offset");
            rootOffsets[npc] = offset;
            if (offset == 0)
                continue;
            if ((offset & 3) != 0)
                fail("NPC " + std::to_string(npc) + " table offset is not four-byte aligned");
            if (!tool::spanWithin(offset, 56 * 4, data.size()))
                fail("NPC " + std::to_string(npc) + " table is out of range at 0x" + hex(offset));
            tableOwners[offset].push_back(npc);
        }
        for (const auto& [offset, owners] : tableOwners)
        {
            spans.push_back({offset, offset + 56 * 4, "NPC stage table at 0x" + hex(offset)});
            for (const std::uint32_t npc : owners)
            {
                for (std::uint32_t stage = 0; stage < 56; ++stage)
                {
                    const std::uint32_t recordOffset =
                        tool::readLe32(data, offset + stage * 4, "stage record offset");
                    if (recordOffset == 0)
                        continue;
                    if ((recordOffset & 3) != 0)
                    {
                        fail("NPC " + std::to_string(npc) + ", stage " + std::to_string(stage) +
                             " record offset is not four-byte aligned");
                    }
                    if (!tool::spanWithin(recordOffset, 0x200, data.size()))
                    {
                        fail("NPC " + std::to_string(npc) + ", stage " + std::to_string(stage) +
                             " record is out of range at 0x" + hex(recordOffset));
                    }
                    recordOwners[recordOffset].push_back({npc, stage});
                }
            }
        }
        for (const auto& [offset, owners] : recordOwners)
        {
            (void)owners;
            spans.push_back({offset, offset + 0x200, "NPC position record at 0x" + hex(offset)});
        }
        std::sort(spans.begin(), spans.end(), [](const DatSpan& left, const DatSpan& right)
        {
            if (left.begin != right.begin)
                return left.begin < right.begin;
            return left.end < right.end;
        });
        for (std::size_t i = 1; i < spans.size(); ++i)
        {
            if (spans[i].begin < spans[i - 1].end)
            {
                fail(spans[i - 1].description + " overlaps " + spans[i].description);
            }
        }
        populatedRootCount = 0;
        for (const std::uint32_t offset : rootOffsets)
            populatedRootCount += offset != 0;
        uniqueTableCount = tableOwners.size();
        for (const auto& [offset, owners] : recordOwners)
        {
            (void)offset;
            logicalRecordCount += owners.size();
            if (owners.size() > 1)
                ++aliasedRecordCount;
        }
    }
    void inspect() const
    {
        std::cout << "file: " << path << '\n'
                  << "size: 0x" << std::hex << std::uppercase << data.size()
                  << std::dec << " (" << data.size() << ")\n"
                  << "NPC slots: 128\n"
                  << "populated NPC roots: " << populatedRootCount << '\n'
                  << "unique stage tables: " << uniqueTableCount << '\n'
                  << "stage slots per table: 56\n"
                  << "logical record references: " << logicalRecordCount << '\n'
                  << "unique physical records: " << recordOwners.size() << '\n'
                  << "aliased physical records: " << aliasedRecordCount << '\n';
        for (const auto& [offset, owners] : recordOwners)
        {
            if (owners.size() < 2)
                continue;
            std::cout << "alias 0x" << std::hex << std::uppercase << offset << std::dec << ':';
            for (const auto& owner : owners)
                std::cout << " (npc=" << owner.npcIndex << ",stage=" << owner.stageIndex << ')';
            std::cout << '\n';
        }
    }
    void dump(std::uint32_t npcIndex, std::uint32_t stageIndex) const
    {
        if (npcIndex >= rootOffsets.size())
            fail("NPC index is outside 0..127");
        if (stageIndex >= 56)
            fail("stage index is outside 0..55");
        const std::uint32_t tableOffset = rootOffsets[npcIndex];
        if (tableOffset == 0)
            fail("NPC " + std::to_string(npcIndex) + " is absent");
        const std::uint32_t recordOffset =
            tool::readLe32(data, tableOffset + stageIndex * 4, "stage record offset");
        if (recordOffset == 0)
            fail("NPC " + std::to_string(npcIndex) + ", stage " + std::to_string(stageIndex) + " is absent");
        const auto ownerIt = recordOwners.find(recordOffset);
        std::cout << "record: npc=" << npcIndex << " stage=" << stageIndex
                  << " offset=0x" << std::hex << std::uppercase << recordOffset
                  << " size=0x200 owners=" << std::dec << ownerIt->second.size() << '\n';
        for (std::uint32_t row = 0; row < 7; ++row)
        {
            std::cout << "row " << row << ':';
            for (std::uint32_t column = 0; column < 32; ++column)
            {
                const std::uint64_t valueOffset = recordOffset + (row * 32 + column) * 2;
                std::cout << ' ' << tool::readLeS16(data, valueOffset, "NPC position value");
            }
            std::cout << '\n';
        }
        std::cout << "unknown suffix: [0x" << std::hex << std::uppercase
                  << recordOffset + 0x1C0 << ",0x" << recordOffset + 0x200 << ")\n";
    }
private:
    std::string path;
    tool::Bytes data;
    std::array<std::uint32_t, 128> rootOffsets{};
    std::map<std::uint32_t, std::vector<DatOwner>> recordOwners;
    std::size_t populatedRootCount{};
    std::size_t uniqueTableCount{};
    std::size_t logicalRecordCount{};
    std::size_t aliasedRecordCount{};
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
              << "  " << argv0 << " inspect <input.dat>\n"
              << "  " << argv0 << " validate <input.dat>\n"
              << "  " << argv0 << " dump <input.dat> <npc-index> <stage-index>\n";
}
int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printUsage(argv[0]);
        return 1;
    }
    const std::string command = argv[1];
    if ((command == "inspect" || command == "validate") && argc != 3)
    {
        printUsage(argv[0]);
        return 1;
    }
    if (command == "dump" && argc != 5)
    {
        printUsage(argv[0]);
        return 1;
    }
    if (command != "inspect" && command != "validate" && command != "dump")
    {
        printUsage(argv[0]);
        return 1;
    }
    try
    {
        const DatFile dat(argv[2]);
        if (command == "inspect")
            dat.inspect();
        else if (command == "validate")
            std::cout << "valid: " << argv[2] << '\n';
        else
        {
            const std::uint64_t npc = tool::parseUnsigned(argv[3], "NPC index");
            const std::uint64_t stage = tool::parseUnsigned(argv[4], "stage index");
            if (npc > UINT32_MAX || stage > UINT32_MAX)
                throw std::runtime_error("index exceeds 32-bit range");
            dat.dump(static_cast<std::uint32_t>(npc), static_cast<std::uint32_t>(stage));
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../tool_common.hpp"
struct RwChunk
{
    std::uint32_t id;
    std::uint32_t dataSize;
    std::uint32_t libraryId;
    std::uint64_t offset;
    std::uint64_t end;
};
struct OlRecord
{
    std::uint32_t objectType;
    std::uint32_t flagsOrId;
    std::int32_t payloadReference;
    std::array<std::uint8_t, 12> rawName;
    std::uint32_t rawScaleBits;
    std::uint64_t payloadOffset{};
    std::uint32_t payloadChunkId{};
};
class OlFile
{
public:
    explicit OlFile(const std::string& path)
        : path(path), data(tool::readFile(path))
    {
        textureChunk = readChunk(0, "texture dictionary");
        if (textureChunk.id != 0x16)
            fail("first top-level chunk is not a texture dictionary (0x16)");
        directoryChunk = readChunk(textureChunk.end, "object directory");
        if (directoryChunk.id != 0x79032003)
            fail("second top-level chunk is not the object directory (0x79032003)");
        const std::uint64_t payload = directoryChunk.offset + 12;
        const std::uint32_t count = tool::readLe32(data, payload, "OL object count");
        const std::uint64_t recordsSize = tool::checkedMultiply(count, 0x1C, "OL directory records");
        const std::uint64_t expectedSize = tool::checkedAdd(4, recordsSize, "OL directory payload");
        if (expectedSize != directoryChunk.dataSize)
            fail("object directory length does not equal 4 + count * 0x1C");
        records.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const std::uint64_t offset = payload + 4 + static_cast<std::uint64_t>(i) * 0x1C;
            OlRecord record{};
            record.objectType = tool::readLe32(data, offset, "OL object type");
            record.flagsOrId = tool::readLe32(data, offset + 4, "OL flags/ID");
            record.payloadReference = tool::readLeS32(data, offset + 8, "OL payload reference");
            for (std::size_t j = 0; j < record.rawName.size(); ++j)
                record.rawName[j] = data[static_cast<std::size_t>(offset + 12 + j)];
            record.rawScaleBits = tool::readLe32(data, offset + 24, "OL raw scale bits");
            if (record.payloadReference > static_cast<std::int32_t>(i))
                fail("record " + std::to_string(i) + " has a future payload reference");
            records.push_back(record);
        }
        std::uint64_t cursor = directoryChunk.end;
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            auto& record = records[i];
            if (record.payloadReference != static_cast<std::int32_t>(i))
                continue;
            const RwChunk chunk = readChunk(cursor, "owned object payload");
            const std::uint32_t expectedId = expectedChunkId(record.objectType, i);
            if (chunk.id != expectedId)
            {
                fail("record " + std::to_string(i) + " type " + std::to_string(record.objectType) +
                     " owns chunk 0x" + hex(chunk.id) + ", expected 0x" + hex(expectedId));
            }
            record.payloadOffset = chunk.offset;
            record.payloadChunkId = chunk.id;
            cursor = chunk.end;
            ++ownedPayloadCount;
        }
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const int owner = resolveOwner(i);
            if (owner < 0)
                continue;
            const auto& record = records[i];
            const auto& ownerRecord = records[static_cast<std::size_t>(owner)];
            const std::uint32_t expectedId = expectedChunkId(record.objectType, i);
            if (ownerRecord.payloadChunkId != expectedId)
            {
                fail("record " + std::to_string(i) + " resolves to an incompatible owned payload");
            }
        }
        if (cursor != data.size())
            fail("unclaimed trailing bytes begin at 0x" + hex(cursor));
    }
    void inspect() const
    {
        std::cout << "file: " << path << '\n'
                  << "size: 0x" << std::hex << std::uppercase << data.size()
                  << std::dec << " (" << data.size() << ")\n";
        printChunk("texture", textureChunk);
        printChunk("directory", directoryChunk);
        std::cout << "records: " << records.size() << '\n'
                  << "owned payloads: " << ownedPayloadCount << '\n';
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const auto& record = records[i];
            std::cout << "record " << i
                      << ": type=" << record.objectType
                      << " flagsOrId=0x" << std::hex << std::uppercase << record.flagsOrId
                      << " reference=" << std::dec << record.payloadReference
                      << " name=" << escapedName(record.rawName)
                      << " rawScaleBits=0x" << std::hex << std::uppercase << record.rawScaleBits;
            if (record.payloadReference == static_cast<std::int32_t>(i))
            {
                std::cout << " payloadOffset=0x" << record.payloadOffset
                          << " chunkId=0x" << record.payloadChunkId;
            }
            else if (record.payloadReference >= 0)
                std::cout << " resolvedOwner=" << std::dec << resolveOwner(i);
            std::cout << std::dec << '\n';
        }
    }
private:
    std::string path;
    tool::Bytes data;
    RwChunk textureChunk{};
    RwChunk directoryChunk{};
    std::vector<OlRecord> records;
    std::size_t ownedPayloadCount{};
    RwChunk readChunk(std::uint64_t offset, const char* description) const
    {
        if (!tool::spanWithin(offset, 12, data.size()))
            fail(std::string("truncated ") + description + " chunk header at 0x" + hex(offset));
        RwChunk chunk{};
        chunk.id = tool::readLe32(data, offset, "RenderWare chunk ID");
        chunk.dataSize = tool::readLe32(data, offset + 4, "RenderWare chunk size");
        chunk.libraryId = tool::readLe32(data, offset + 8, "RenderWare library ID");
        chunk.offset = offset;
        chunk.end = tool::checkedAdd(offset + 12, chunk.dataSize, "RenderWare chunk end");
        if (chunk.end > data.size())
            fail(std::string(description) + " chunk at 0x" + hex(offset) + " is truncated");
        return chunk;
    }
    std::uint32_t expectedChunkId(std::uint32_t objectType, std::size_t recordIndex) const
    {
        switch (objectType)
        {
        case 0:
            return 0x14;
        case 1:
            return 0x10;
        case 2:
            return 0x78032010;
        default:
            fail("record " + std::to_string(recordIndex) + " has unsupported object type " +
                 std::to_string(objectType));
        }
    }
    int resolveOwner(std::size_t recordIndex) const
    {
        std::int32_t reference = records[recordIndex].payloadReference;
        if (reference < 0)
            return -1;
        for (std::size_t depth = 0; depth <= records.size(); ++depth)
        {
            if (reference < 0 || static_cast<std::size_t>(reference) >= records.size())
                fail("record " + std::to_string(recordIndex) + " has an invalid payload reference chain");
            const auto& target = records[static_cast<std::size_t>(reference)];
            if (target.payloadReference == reference)
                return reference;
            reference = target.payloadReference;
        }
        fail("record " + std::to_string(recordIndex) + " has a cyclic payload reference chain");
    }
    static std::string escapedName(const std::array<std::uint8_t, 12>& bytes)
    {
        std::ostringstream output;
        output << '"';
        for (const std::uint8_t value : bytes)
        {
            if (value == 0)
                break;
            if (std::isprint(value) && value != '\\' && value != '"')
                output << static_cast<char>(value);
            else
                output << "\\x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(value);
        }
        output << '"';
        return output.str();
    }
    static void printChunk(const char* name, const RwChunk& chunk)
    {
        std::cout << name << " chunk: id=0x" << std::hex << std::uppercase << chunk.id
                  << " offset=0x" << chunk.offset
                  << " dataSize=0x" << chunk.dataSize
                  << " end=0x" << chunk.end
                  << " libraryId=0x" << chunk.libraryId << std::dec << '\n';
    }
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
              << "  " << argv0 << " inspect <input.ol>\n"
              << "  " << argv0 << " validate <input.ol>\n";
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
        const OlFile ol(argv[2]);
        if (command == "inspect")
            ol.inspect();
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

#include <algorithm>
#include <array>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "xaf.h"
#include "../common.h"
constexpr std::size_t COPY_BUFFER_SIZE = 0x10000;
static std::uint64_t alignUp(std::uint64_t value, std::uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
static bool copyBytes(std::istream& input, std::ostream& output,
                      std::uint64_t count)
{
    std::array<std::uint8_t, COPY_BUFFER_SIZE> buffer{};
    while (count > 0)
    {
        const std::size_t size = count > buffer.size()
                               ? buffer.size()
                               : static_cast<std::size_t>(count);
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(size));
        if (!input)
        {
            printf("ERROR: Could not read file data.\n");
            return false;
        }
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(size));
        if (!output)
        {
            printf("ERROR: Could not write file data.\n");
            return false;
        }
        count -= size;
    }
    return true;
}
static bool copyRange(std::ifstream& input, std::uint64_t offset,
                      std::ostream& output, std::uint64_t count)
{
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input)
    {
        printf("ERROR: Could not seek to 0x%" PRIx64 ".\n", offset);
        return false;
    }
    return copyBytes(input, output, count);
}
static bool writePadding(std::ostream& output, std::uint64_t count)
{
    std::array<std::uint8_t, 0x1000> zeros{};
    while (count > 0)
    {
        const std::size_t size = count > zeros.size()
                               ? zeros.size()
                               : static_cast<std::size_t>(count);
        output.write(reinterpret_cast<const char*>(zeros.data()),
                     static_cast<std::streamsize>(size));
        if (!output)
        {
            printf("ERROR: Could not write file data.\n");
            return false;
        }
        count -= size;
    }
    return true;
}
static bool normalizeArchivePath(std::string_view input, std::string& output)
{
    std::string path(input);
    std::replace(path.begin(), path.end(), '\\', '/');
    output.clear();
    std::size_t begin = 0;
    while (begin <= path.size())
    {
        const std::size_t end = path.find('/', begin);
        const std::size_t length =
            (end == std::string::npos ? path.size() : end) - begin;
        const std::string_view part(path.data() + begin, length);
        if (!part.empty() && part != ".")
        {
            const bool drive = output.empty() && part.size() == 2 &&
                std::isalpha(static_cast<unsigned char>(part[0])) && part[1] == ':';
            if (part == ".." && !output.empty())
            {
                printf("ERROR: Unsafe parent traversal in archive path '%s'.\n",
                       path.c_str());
                return false;
            }
            if (!drive && part != "..")
            {
                if (!output.empty())
                    output += '/';
                output += part;
            }
        }
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    if (output.empty())
    {
        printf("ERROR: Archive path is empty.\n");
        return false;
    }
    return true;
}
bool readXaf(const std::filesystem::path& path, XafArchive& xaf)
{
    std::error_code error;
    xaf = {};
    xaf.fileSize = std::filesystem::file_size(path, error);
    if (error)
    {
        printf("ERROR: Could not get the size of '%s': %s\n",
               path.string().c_str(), error.message().c_str());
        return false;
    }
    if (xaf.fileSize < XAF_HEADER_SIZE)
    {
        printf("ERROR: Invalid XAF header.\n");
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        printf("ERROR: Could not open '%s'.\n", path.string().c_str());
        return false;
    }
    std::array<std::uint8_t, XAF_HEADER_SIZE> headerData{};
    input.read(reinterpret_cast<char*>(headerData.data()),
               static_cast<std::streamsize>(headerData.size()));
    if (!input)
    {
        printf("ERROR: Could not read '%s'.\n", path.string().c_str());
        return false;
    }
    xaf.header.magic = read32(headerData.data());
    xaf.header.flags = read32(headerData.data() + 4);
    std::memcpy(xaf.header.signature.data(), headerData.data() + 8, 28);
    xaf.header.reserved0 = read32(headerData.data() + 36);
    xaf.header.entryCountPlusOne = read32(headerData.data() + 40);
    xaf.header.entryCount = read32(headerData.data() + 44);
    xaf.header.archiveSize = read32(headerData.data() + 48);
    for (std::size_t i = 0; i < xaf.header.reserved.size(); ++i)
        xaf.header.reserved[i] = read32(headerData.data() + 52 + i * 4);
    if (xaf.header.magic != XAF_MAGIC)
    {
        printf("ERROR: Invalid XAF magic 0x%08" PRIx32 ".\n", xaf.header.magic);
        return false;
    }
    if (static_cast<std::uint64_t>(xaf.header.entryCount) + 1 !=
        xaf.header.entryCountPlusOne)
    {
        printf("ERROR: Invalid XAF entry count.\n");
        return false;
    }
    if (xaf.header.archiveSize != xaf.fileSize)
    {
        printf("ERROR: XAF archive size does not match the file size.\n");
        return false;
    }
    xaf.tableSize =
        XAF_HEADER_SIZE +
        static_cast<std::uint64_t>(xaf.header.entryCount) *
            XAF_DESCRIPTION_SIZE +
        static_cast<std::uint64_t>(xaf.header.entryCount) * XAF_ENTRY_SIZE;
    if (xaf.tableSize > xaf.fileSize)
    {
        printf("ERROR: XAF tables exceed the file size.\n");
        return false;
    }
    std::vector<std::uint8_t> tableData(
        static_cast<std::size_t>(xaf.tableSize - XAF_HEADER_SIZE));
    if (!tableData.empty())
    {
        input.read(reinterpret_cast<char*>(tableData.data()),
                   static_cast<std::streamsize>(tableData.size()));
    }
    if (!input)
    {
        printf("ERROR: Could not read '%s'.\n", path.string().c_str());
        return false;
    }
    xaf.descs.resize(xaf.header.entryCount);
    xaf.entries.resize(xaf.header.entryCount);
    for (std::size_t i = 0; i < xaf.descs.size(); ++i)
    {
        const std::uint8_t* data =
            tableData.data() + i * XAF_DESCRIPTION_SIZE;
        xaf.descs[i].offset = read32(data);
        xaf.descs[i].length = read32(data + 4);
    }
    const std::size_t entryTable =
        xaf.descs.size() * XAF_DESCRIPTION_SIZE;
    std::set<std::string> paths;
    for (std::size_t i = 0; i < xaf.entries.size(); ++i)
    {
        const std::uint8_t* data =
            tableData.data() + entryTable + i * XAF_ENTRY_SIZE;
        XafEntry& entry = xaf.entries[i];
        entry.metaOffset = read32(data);
        entry.metaLength = read32(data + 4);
        std::memcpy(entry.rawPath.data(), data + 8, entry.rawPath.size());
        if (entry.rawPath[0] == 0 ||
            std::memchr(entry.rawPath.data(), 0, entry.rawPath.size()) == nullptr)
        {
            printf("ERROR: Entry %zu has an invalid XAF path.\n", i);
            return false;
        }
        if (!normalizeArchivePath(
                reinterpret_cast<const char*>(entry.rawPath.data()), entry.path))
        {
            return false;
        }
        if (!paths.insert(entry.path).second)
        {
            printf("ERROR: Duplicate XAF path '%s'.\n", entry.path.c_str());
            return false;
        }
    }
    xaf.payloadEnd = xaf.tableSize;
    for (std::size_t i = 0; i < xaf.descs.size(); ++i)
    {
        const std::uint64_t offset = xaf.descs[i].offset;
        const std::uint64_t end = offset + xaf.descs[i].length;
        if ((offset & (XAF_ALIGN - 1)) != 0)
        {
            printf("ERROR: Description %zu is not 0x%x-aligned.\n", i,
                   XAF_ALIGN);
            return false;
        }
        if (offset < xaf.tableSize)
        {
            printf("ERROR: Description %zu starts inside the XAF tables.\n", i);
            return false;
        }
        if (end > xaf.fileSize)
        {
            printf("ERROR: Description %zu exceeds the XAF file size.\n", i);
            return false;
        }
        if (i > 0 &&
            offset < static_cast<std::uint64_t>(
                         xaf.descs[i - 1].offset) +
                     xaf.descs[i - 1].length)
        {
            printf("ERROR: Description %zu overlaps the previous entry.\n", i);
            return false;
        }
        xaf.payloadEnd = std::max(xaf.payloadEnd, end);
    }
    std::uint64_t firstMetaOffset = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = 0; i < xaf.entries.size(); ++i)
    {
        const std::uint64_t offset = xaf.entries[i].metaOffset;
        const std::uint64_t length = xaf.entries[i].metaLength;
        if (offset == 0 && length == 0)
            continue;
        if (offset == 0)
        {
            printf("ERROR: Entry %zu has metadata without an offset.\n", i);
            return false;
        }
        if (offset >= xaf.fileSize || offset + length > xaf.fileSize)
        {
            printf("ERROR: Entry %zu metadata exceeds the XAF file size.\n", i);
            return false;
        }
        firstMetaOffset = std::min(firstMetaOffset, offset);
    }
    if (firstMetaOffset != std::numeric_limits<std::uint64_t>::max())
    {
        if ((firstMetaOffset & (XAF_ALIGN - 1)) != 0)
        {
            printf("ERROR: XAF auxiliary tail is not 0x%x-aligned.\n", XAF_ALIGN);
            return false;
        }
        if (firstMetaOffset < xaf.payloadEnd)
        {
            printf("ERROR: XAF auxiliary tail overlaps payload data.\n");
            return false;
        }
        xaf.auxOffset = firstMetaOffset;
    }
    return true;
}
bool extractXaf(const std::filesystem::path& inputPath,
                const std::filesystem::path& outputPath)
{
    XafArchive xaf;
    if (!readXaf(inputPath, xaf))
        return false;
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        printf("ERROR: Could not open '%s'.\n", inputPath.string().c_str());
        return false;
    }
    const std::filesystem::path outputDirectory =
        outputPath / inputPath.stem();
    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);
    if (error)
    {
        printf("ERROR: Could not create '%s': %s\n",
               outputDirectory.string().c_str(), error.message().c_str());
        return false;
    }
    for (std::size_t i = 0; i < xaf.entries.size(); ++i)
    {
        const std::filesystem::path path = outputDirectory / xaf.entries[i].path;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            printf("ERROR: Could not create '%s': %s\n",
                   path.parent_path().string().c_str(), error.message().c_str());
            return false;
        }
        std::ofstream output(path, std::ios::binary);
        if (!output)
        {
            printf("ERROR: Could not open '%s'.\n", path.string().c_str());
            return false;
        }
        if (!copyRange(input, xaf.descs[i].offset, output,
                       xaf.descs[i].length))
            return false;
    }
    printf("Extracted %zu files from %s to %s.\n", xaf.entries.size(),
           inputPath.string().c_str(), outputDirectory.string().c_str());
    return true;
}
bool replaceXaf(const std::filesystem::path& archivePath,
                std::string_view archiveEntry,
                const std::filesystem::path& replacementPath,
                const std::filesystem::path& outputPath)
{
    XafArchive xaf;
    if (!readXaf(archivePath, xaf))
        return false;
    std::string entryPath;
    if (!normalizeArchivePath(archiveEntry, entryPath))
        return false;
    std::size_t targetIndex = xaf.entries.size();
    for (std::size_t i = 0; i < xaf.entries.size(); ++i)
    {
        if (xaf.entries[i].path == entryPath)
        {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex == xaf.entries.size())
    {
        printf("ERROR: '%s' does not contain '%s'.\n",
               archivePath.string().c_str(), entryPath.c_str());
        return false;
    }
    std::error_code error;
    const std::uint64_t replacementSize =
        std::filesystem::file_size(replacementPath, error);
    if (error)
    {
        printf("ERROR: Could not get the size of '%s': %s\n",
               replacementPath.string().c_str(), error.message().c_str());
        return false;
    }
    if (replacementSize > std::numeric_limits<std::uint32_t>::max())
    {
        printf("ERROR: Replacement file is too large for an XAF entry.\n");
        return false;
    }
    std::vector<XafDescription> descs(xaf.descs.size());
    std::uint64_t cursor =
        xaf.descs.empty() ? xaf.tableSize : xaf.descs.front().offset;
    for (std::size_t i = 0; i < descs.size(); ++i)
    {
        if (i > 0)
            cursor = alignUp(cursor, XAF_ALIGN);
        if (cursor > std::numeric_limits<std::uint32_t>::max())
        {
            printf("ERROR: Rewritten XAF description exceeds the offset range.\n");
            return false;
        }
        const std::uint64_t length =
            i == targetIndex ? replacementSize : xaf.descs[i].length;
        descs[i].offset = static_cast<std::uint32_t>(cursor);
        descs[i].length = static_cast<std::uint32_t>(length);
        cursor += descs[i].length;
    }
    const std::uint64_t auxSize =
        xaf.auxOffset == 0 ? 0 : xaf.fileSize - xaf.auxOffset;
    const std::uint64_t newAuxOffset =
        auxSize == 0 ? 0 : alignUp(cursor, XAF_ALIGN);
    const std::uint64_t finalSize =
        newAuxOffset == 0 ? cursor : newAuxOffset + auxSize;
    if (finalSize > std::numeric_limits<std::uint32_t>::max())
    {
        printf("ERROR: Rewritten XAF exceeds the archive size field.\n");
        return false;
    }
    std::ifstream original(archivePath, std::ios::binary);
    std::ifstream replacement(replacementPath, std::ios::binary);
    std::ofstream output(outputPath, std::ios::binary);
    if (!original)
    {
        printf("ERROR: Could not open '%s'.\n", archivePath.string().c_str());
        return false;
    }
    if (!replacement)
    {
        printf("ERROR: Could not open '%s'.\n", replacementPath.string().c_str());
        return false;
    }
    if (!output)
    {
        printf("ERROR: Could not open '%s'.\n", outputPath.string().c_str());
        return false;
    }
    std::vector<std::uint8_t> tableData(
        static_cast<std::size_t>(xaf.tableSize));
    write32(tableData.data() + 0, xaf.header.magic);
    write32(tableData.data() + 4, xaf.header.flags);
    std::memcpy(tableData.data() + 8, xaf.header.signature.data(), 28);
    write32(tableData.data() + 36, xaf.header.reserved0);
    write32(tableData.data() + 40, xaf.header.entryCountPlusOne);
    write32(tableData.data() + 44, xaf.header.entryCount);
    write32(tableData.data() + 48, static_cast<std::uint32_t>(finalSize));
    write32(tableData.data() + 52, xaf.header.reserved[0]);
    write32(tableData.data() + 56, xaf.header.reserved[1]);
    write32(tableData.data() + 60, xaf.header.reserved[2]);
    for (std::size_t i = 0; i < descs.size(); ++i)
    {
        std::uint8_t* data =
            tableData.data() + XAF_HEADER_SIZE + i * XAF_DESCRIPTION_SIZE;
        write32(data, descs[i].offset);
        write32(data + 4, descs[i].length);
    }
    const std::size_t entryTable =
        XAF_HEADER_SIZE + descs.size() * XAF_DESCRIPTION_SIZE;
    for (std::size_t i = 0; i < xaf.entries.size(); ++i)
    {
        const XafEntry& entry = xaf.entries[i];
        std::uint8_t* data =
            tableData.data() + entryTable + i * XAF_ENTRY_SIZE;
        std::uint32_t metaOffset = entry.metaOffset;
        if (xaf.auxOffset != 0 && metaOffset >= xaf.auxOffset)
        {
            metaOffset = static_cast<std::uint32_t>(
                newAuxOffset + metaOffset - xaf.auxOffset);
        }
        write32(data, metaOffset);
        write32(data + 4, entry.metaLength);
        std::memcpy(data + 8, entry.rawPath.data(), entry.rawPath.size());
    }
    output.write(reinterpret_cast<const char*>(tableData.data()),
                 static_cast<std::streamsize>(tableData.size()));
    if (!output)
    {
        printf("ERROR: Could not write '%s'.\n", outputPath.string().c_str());
        return false;
    }
    std::uint64_t outputPosition = xaf.tableSize;
    if (!descs.empty())
    {
        const std::uint64_t reservedSize =
            descs.front().offset - xaf.tableSize;
        if (!copyRange(original, xaf.tableSize, output, reservedSize))
            return false;
        outputPosition = descs.front().offset;
    }
    for (std::size_t i = 0; i < descs.size(); ++i)
    {
        if (!writePadding(output, descs[i].offset - outputPosition))
            return false;
        outputPosition = descs[i].offset;
        if (i == targetIndex)
        {
            if (!copyBytes(replacement, output, replacementSize))
                return false;
            outputPosition += replacementSize;
        }
        else
        {
            if (!copyRange(original, xaf.descs[i].offset, output,
                           xaf.descs[i].length))
            {
                return false;
            }
            outputPosition += xaf.descs[i].length;
        }
    }
    if (xaf.auxOffset != 0)
    {
        if (!writePadding(output, newAuxOffset - outputPosition) ||
            !copyRange(original, xaf.auxOffset, output, auxSize))
        {
            return false;
        }
        outputPosition = newAuxOffset + auxSize;
    }
    if (outputPosition != finalSize)
    {
        printf("ERROR: Wrote %" PRIu64 " bytes but expected %" PRIu64 ".\n",
               outputPosition, finalSize);
        return false;
    }
    output.flush();
    if (!output)
    {
        printf("ERROR: Could not write '%s'.\n", outputPath.string().c_str());
        return false;
    }
    printf("Replaced %s in %s -> %s.\n", entryPath.c_str(),
           archivePath.string().c_str(), outputPath.string().c_str());
    return true;
}

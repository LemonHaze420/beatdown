#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "xaf.h"

static std::string lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

static bool inspect(const std::filesystem::path& inputPath)
{
    XafArchive xaf;
    if (!readXaf(inputPath, xaf))
        return false;

    printf("file: %s\n", inputPath.string().c_str());
    printf("magic: 0x%08" PRIx32 "\n", xaf.header.magic);
    printf("flags: 0x%08" PRIx32 "\n", xaf.header.flags);
    printf("signature: %s\n", xaf.header.signature.data());
    printf("reserved0: 0x%08" PRIx32 "\n", xaf.header.reserved0);
    printf("entryCountPlusOne: %" PRIu32 "\n", xaf.header.entryCountPlusOne);
    printf("entryCount: %" PRIu32 "\n", xaf.header.entryCount);
    printf("archiveSize: %" PRIu32 "\n", xaf.header.archiveSize);
    printf("fileSize: %" PRIu64 "\n", xaf.fileSize);
    printf("tableSize: %" PRIu64 "\n", xaf.tableSize);
    printf("payloadEnd: %" PRIu64 "\n", xaf.payloadEnd);

    if (!xaf.descs.empty())
    {
        const XafDescription& last = xaf.descs.back();
        printf("firstPayloadOffset: %" PRIu32 "\n",
               xaf.descs.front().offset);
        printf("lastPayloadEnd: %" PRIu64 "\n",
               static_cast<std::uint64_t>(last.offset) + last.length);
    }

    if (xaf.auxOffset != 0)
    {
        printf("auxTailOffset: %" PRIu64 "\n", xaf.auxOffset);
        printf("auxTailSize: %" PRIu64 "\n", xaf.fileSize - xaf.auxOffset);
    }
    else
    {
        printf("auxTailOffset: none\n");
        printf("auxTailSize: 0\n");
    }

    bool heading = false;
    for (std::size_t i = 0; i < xaf.entries.size(); ++i)
    {
        const XafEntry& entry = xaf.entries[i];
        if (entry.metaOffset == 0 && entry.metaLength == 0)
            continue;
        if (!heading)
        {
            printf("auxiliaryEntries:\n");
            heading = true;
        }
        printf("  [%zu] metaOffset=%" PRIu32 " metaLength=%" PRIu32
               " path=%s\n", i, entry.metaOffset, entry.metaLength,
               reinterpret_cast<const char*>(entry.rawPath.data()));
    }

    return true;
}

static bool extract(const std::filesystem::path& inputPath,
                    const std::filesystem::path& outputPath)
{
    std::error_code error;
    const std::filesystem::file_status status =
        std::filesystem::status(inputPath, error);
    if (error)
    {
        printf("ERROR: Could not inspect '%s': %s\n",
               inputPath.string().c_str(), error.message().c_str());
        return false;
    }

    if (!std::filesystem::is_directory(status))
        return extractXaf(inputPath, outputPath);

    std::vector<std::filesystem::path> archives;
    std::filesystem::directory_iterator iterator(inputPath, error);
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end)
    {
        const bool regular = iterator->is_regular_file(error);
        if (error)
            break;
        if (regular && lower(iterator->path().extension().string()) == ".xaf")
        {
            archives.push_back(iterator->path());
        }
        iterator.increment(error);
    }
    if (error)
    {
        printf("ERROR: Could not read directory '%s': %s\n",
               inputPath.string().c_str(), error.message().c_str());
        return false;
    }

    std::sort(archives.begin(), archives.end());
    if (archives.empty())
    {
        printf("ERROR: No XAF archives found in '%s'.\n",
               inputPath.string().c_str());
        return false;
    }
    for (const std::filesystem::path& archive : archives)
    {
        if (!extractXaf(archive, outputPath))
            return false;
    }
    return true;
}

static void printUsage()
{
    printf("./xaf input.xaf\n");
    printf("./xaf extract input.xaf output_directory\n");
    printf("./xaf replace input.xaf archive_path replacement_file output.xaf\n");
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printUsage();
        return 0;
    }

    const std::string command = argv[1];
    if (argc == 2 && (command == "-h" || command == "--help"))
    {
        printUsage();
        return 0;
    }
    if (argc == 4 && command == "extract")
        return extract(argv[2], argv[3]) ? 0 : 1;
    if (argc == 6 && command == "replace")
    {
        const std::filesystem::path inputPath = argv[2];
        const std::filesystem::path replacementPath = argv[4];
        const std::filesystem::path outputPath = argv[5];
        if (inputPath.lexically_normal() == outputPath.lexically_normal() ||
            replacementPath.lexically_normal() == outputPath.lexically_normal())
        {
            printf("ERROR: Input and output paths are the same.\n");
            return 1;
        }
        return replaceXaf(inputPath, argv[3], replacementPath, outputPath) ? 0 : 1;
    }
    if (argc == 2)
        return inspect(argv[1]) ? 0 : 1;

    printUsage();
    return 1;
}

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include "text_table.h"
static bool readFile(const std::filesystem::path& path, TextTableBytes& data)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        printf("ERROR: Could not open '%s'.\n", path.string().c_str());
        return false;
    }
    const std::streamoff end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
                       std::numeric_limits<std::size_t>::max())
    {
        printf("ERROR: Invalid file size.\n");
        return false;
    }
    data.resize(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!data.empty())
        input.read(reinterpret_cast<char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    if (!input)
    {
        printf("ERROR: Could not read '%s'.\n", path.string().c_str());
        return false;
    }
    return true;
}
static bool writeFile(const std::filesystem::path& path,
                      const TextTableBytes& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        printf("ERROR: Could not open '%s'.\n", path.string().c_str());
        return false;
    }
    if (!data.empty())
        output.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
    if (!output)
    {
        printf("ERROR: Could not write '%s'.\n", path.string().c_str());
        return false;
    }
    return true;
}
static bool loadTable(const std::filesystem::path& path, TextTable& table)
{
    TextTableBytes data;
    if (!readFile(path, data))
        return false;
    return readTextTable(data, table);
}
static bool inspect(const std::filesystem::path& path)
{
    TextTableBytes data;
    if (!readFile(path, data))
        return false;
    TextTable table;
    if (!readTextTable(data, table))
        return false;
    printf("file: %s\n", path.string().c_str());
    printf("fileSize: %zu\n", data.size());
    printf("entryCount: %zu\n", table.records.size());
    printf("encoding: %u (%s)\n", table.encoding,
           textTableEncodingName(table.encoding));
    std::size_t suppressed = 0;
    std::size_t payload = 0;
    for (const TextTableRecord& record : table.records)
    {
        if (record.suppressed)
            ++suppressed;
        else
            payload += record.text.size() + (record.terminated ? 1u : 0u);
    }
    printf("payloadBytes: %zu\n", payload);
    printf("suppressedRecords: %zu\n", suppressed);
    if (!table.records.empty())
        printf("idRange: %u..%u\n", table.records.front().textId,
               table.records.back().textId);
    printf("tailPadding:");
    for (const std::uint8_t value : table.tailPadding)
        printf(" %02x", value);
    if (table.tailPadding.empty())
        printf(" none");
    printf("\n");
    return true;
}
static bool validate(const std::filesystem::path& path)
{
    TextTableBytes data;
    if (!readFile(path, data))
        return false;
    TextTable table;
    if (!readTextTable(data, table))
        return false;
    TextTableBytes rebuilt;
    if (!writeTextTable(table, rebuilt))
        return false;
    if (rebuilt != data)
    {
        printf("ERROR: '%s' does not round-trip byte-identically.\n",
               path.string().c_str());
        return false;
    }
    std::string source;
    if (!writeTextTableSource(table, source))
        return false;
    TextTable parsed;
    if (!readTextTableSource(source, parsed))
        return false;
    TextTableBytes fromSource;
    if (!writeTextTable(parsed, fromSource))
        return false;
    if (fromSource != data)
    {
        printf("ERROR: '%s' does not survive a source round-trip.\n",
               path.string().c_str());
        return false;
    }
    printf("%s: %zu records, encoding %u (%s), binary and source round-trips "
           "are byte-identical.\n",
           path.string().c_str(), table.records.size(), table.encoding,
           textTableEncodingName(table.encoding));
    return true;
}
static bool extract(const std::filesystem::path& inputPath,
                    const std::filesystem::path& outputPath)
{
    TextTable table;
    if (!loadTable(inputPath, table))
        return false;
    std::string source;
    if (!writeTextTableSource(table, source))
        return false;
    TextTableBytes data(source.begin(), source.end());
    return writeFile(outputPath, data);
}
static bool build(const std::filesystem::path& inputPath,
                  const std::filesystem::path& outputPath)
{
    TextTableBytes raw;
    if (!readFile(inputPath, raw))
        return false;
    const std::string source(raw.begin(), raw.end());
    TextTable table;
    if (!readTextTableSource(source, table))
        return false;
    TextTableBytes data;
    if (!writeTextTable(table, data))
        return false;
    return writeFile(outputPath, data);
}
static bool dump(const std::filesystem::path& path, std::uint16_t textId)
{
    TextTable table;
    if (!loadTable(path, table))
        return false;
    const TextTableRecord* const record = findTextTableRecord(table, textId);
    if (!record)
    {
        printf("ERROR: No record with id %u.\n", textId);
        return false;
    }
    if (record->suppressed)
    {
        printf("id %u is suppressed (byteLength 0).\n", textId);
        return true;
    }
    printf("%s\n", record->text.c_str());
    return true;
}
static bool parseTextId(const char* argument, std::uint16_t& textId)
{
    char* end = nullptr;
    const unsigned long value = std::strtoul(argument, &end, 10);
    if (!end || *end != '\0' || value > std::numeric_limits<std::uint16_t>::max())
    {
        printf("ERROR: Invalid text id '%s'.\n", argument);
        return false;
    }
    textId = static_cast<std::uint16_t>(value);
    return true;
}
static void printUsage()
{
    printf("./text_table input.bin\n");
    printf("./text_table inspect input.bin\n");
    printf("./text_table validate input.bin\n");
    printf("./text_table dump input.bin text_id\n");
    printf("./text_table extract input.bin output.txt\n");
    printf("./text_table build input.txt output.bin\n");
}
int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printUsage();
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "-h" ||
                      std::string(argv[1]) == "--help"))
    {
        printUsage();
        return 0;
    }
    if (argc == 2)
        return inspect(argv[1]) ? 0 : 1;
    const std::string command = argv[1];
    if (argc == 3 && command == "inspect")
        return inspect(argv[2]) ? 0 : 1;
    if (argc == 3 && command == "validate")
        return validate(argv[2]) ? 0 : 1;
    if (argc == 4 && command == "dump")
    {
        std::uint16_t textId = 0;
        if (!parseTextId(argv[3], textId))
            return 1;
        return dump(argv[2], textId) ? 0 : 1;
    }
    if (argc == 4 && (command == "extract" || command == "build"))
    {
        const std::filesystem::path inputPath = argv[2];
        const std::filesystem::path outputPath = argv[3];
        if (inputPath.lexically_normal() == outputPath.lexically_normal())
        {
            printf("ERROR: Input and output paths are the same.\n");
            return 1;
        }
        if (command == "extract")
            return extract(inputPath, outputPath) ? 0 : 1;
        return build(inputPath, outputPath) ? 0 : 1;
    }
    printUsage();
    return 1;
}

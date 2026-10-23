#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include "csv.h"
static bool readFile(const std::filesystem::path& path, CsvBytes& data)
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
static bool writeFile(const std::filesystem::path& path, const CsvBytes& data)
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
static std::string lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
static bool isCharacterCsv(const std::filesystem::path& path)
{
    const std::string name = lower(path.filename().string());
    return name == "man.csv" || name == "woman.csv";
}
static bool parseIndex(const char* text, const char* description,
                       std::size_t& value)
{
    if (text[0] == '\0' || text[0] == '-')
    {
        printf("ERROR: Invalid %s '%s'.\n", description, text);
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text, &end, 0);
    if (errno == ERANGE || end == text || *end != '\0' ||
        parsed > std::numeric_limits<std::size_t>::max())
    {
        printf("ERROR: Invalid %s '%s'.\n", description, text);
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}
static std::string escape(const std::string& text)
{
    static const char HEX[] = "0123456789ABCDEF";
    std::string output = "\"";
    for (unsigned char c : text)
    {
        if (c == '\\' || c == '"')
        {
            output.push_back('\\');
            output.push_back(static_cast<char>(c));
        }
        else if (c == '\t')
        {
            output += "\\t";
        }
        else if (c >= 0x20 && c < 0x7F)
        {
            output.push_back(static_cast<char>(c));
        }
        else
        {
            output += "\\x";
            output.push_back(HEX[c >> 4]);
            output.push_back(HEX[c & 0x0F]);
        }
    }
    output.push_back('"');
    return output;
}
static bool inspect(const std::filesystem::path& path)
{
    CsvBytes data;
    if (!readFile(path, data))
        return false;
    CsvFile source;
    std::size_t definitionCount = 0;
    std::size_t categoryCount = 0;
    const bool character = isCharacterCsv(path);
    if (character)
    {
        CharacterCustomizationCsv csv;
        if (!readCharacterCustomizationCsv(data, csv))
            return false;
        source = std::move(csv.source);
        definitionCount = csv.definitions.size();
        categoryCount = csv.categoryDefinitions.size();
    }
    else if (!readCsv(data, source))
    {
        return false;
    }
    std::size_t comments = 0;
    std::size_t dataRows = 0;
    std::size_t minimumFields = std::numeric_limits<std::size_t>::max();
    std::size_t maximumFields = 0;
    for (const CsvRow& row : source.rows)
    {
        if (row.comment)
        {
            ++comments;
            continue;
        }
        ++dataRows;
        if (row.fields.size() < minimumFields)
            minimumFields = row.fields.size();
        if (row.fields.size() > maximumFields)
            maximumFields = row.fields.size();
    }
    if (dataRows == 0)
        minimumFields = 0;
    printf("file: %s\n", path.string().c_str());
    printf("profile: %s\n", character ? "character-customization" : "generic");
    printf("size: %zu bytes\n", data.size());
    printf("lineEndings: CRLF\n");
    printf("rows: %zu\n", source.rows.size());
    printf("commentRows: %zu\n", comments);
    printf("dataRows: %zu\n", dataRows);
    printf("fieldCountRange: %zu..%zu\n", minimumFields, maximumFields);
    if (character)
    {
        printf("runtimeRowSize: 0x70\n");
        printf("definitions: %zu\n", definitionCount);
        printf("categories: %zu\n", categoryCount);
    }
    return true;
}
static bool dump(const std::filesystem::path& path, std::size_t rowIndex)
{
    CsvBytes data;
    if (!readFile(path, data))
        return false;
    CsvFile source;
    if (isCharacterCsv(path))
    {
        CharacterCustomizationCsv csv;
        if (!readCharacterCustomizationCsv(data, csv))
            return false;
        source = std::move(csv.source);
    }
    else if (!readCsv(data, source))
    {
        return false;
    }
    if (rowIndex >= source.rows.size())
    {
        printf("ERROR: Row index %zu is outside the file.\n", rowIndex);
        return false;
    }
    const CsvRow& row = source.rows[rowIndex];
    printf("row: %zu\n", rowIndex);
    printf("comment: %s\n", row.comment ? "yes" : "no");
    if (row.comment)
    {
        printf("text: %s\n", escape(row.commentText).c_str());
        return true;
    }
    printf("fields: %zu\n", row.fields.size());
    for (std::size_t column = 0; column < row.fields.size(); ++column)
    {
        const char* name = isCharacterCsv(path)
                         ? characterCustomizationColumnName(column)
                         : nullptr;
        if (name)
            printf("  [%02zu] %-24s %s\n", column, name,
                   escape(row.fields[column]).c_str());
        else
            printf("  [%02zu] %s\n", column,
                   escape(row.fields[column]).c_str());
    }
    return true;
}
static bool replace(const std::filesystem::path& inputPath,
                    std::size_t rowIndex, std::size_t columnIndex,
                    const std::filesystem::path& replacementPath,
                    const std::filesystem::path& outputPath)
{
    CsvBytes data;
    CsvBytes replacement;
    if (!readFile(inputPath, data) || !readFile(replacementPath, replacement))
        return false;
    CsvFile source;
    if (isCharacterCsv(inputPath))
    {
        CharacterCustomizationCsv csv;
        if (!readCharacterCustomizationCsv(data, csv))
            return false;
        source = std::move(csv.source);
    }
    else if (!readCsv(data, source))
    {
        return false;
    }
    if (rowIndex >= source.rows.size() || source.rows[rowIndex].comment)
    {
        printf("ERROR: Row %zu is not a data row in the file.\n", rowIndex);
        return false;
    }
    if (columnIndex >= source.rows[rowIndex].fields.size())
    {
        printf("ERROR: Column %zu is outside row %zu.\n", columnIndex, rowIndex);
        return false;
    }
    if (replacement.empty())
        source.rows[rowIndex].fields[columnIndex].clear();
    else
    {
        source.rows[rowIndex].fields[columnIndex].assign(
            reinterpret_cast<const char*>(replacement.data()), replacement.size());
    }
    CsvBytes output;
    if (!writeCsv(source, output))
        return false;
    if (isCharacterCsv(inputPath))
    {
        CharacterCustomizationCsv checked;
        if (!readCharacterCustomizationCsv(output, checked))
            return false;
    }
    return writeFile(outputPath, output);
}
static void printUsage()
{
    printf("./csv input.csv\n");
    printf("./csv inspect input.csv\n");
    printf("./csv validate input.csv\n");
    printf("./csv dump input.csv row_index\n");
    printf("./csv replace input.csv row_index column_index replacement_file output.csv\n");
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
        return inspect(argv[2]) ? 0 : 1;
    if (argc == 4 && command == "dump")
    {
        std::size_t row = 0;
        if (!parseIndex(argv[3], "row index", row))
            return 1;
        return dump(argv[2], row) ? 0 : 1;
    }
    if (argc == 7 && command == "replace")
    {
        std::size_t row = 0;
        std::size_t column = 0;
        if (!parseIndex(argv[3], "row index", row) ||
            !parseIndex(argv[4], "column index", column))
        {
            return 1;
        }
        const std::filesystem::path inputPath = argv[2];
        const std::filesystem::path replacementPath = argv[5];
        const std::filesystem::path outputPath = argv[6];
        if (inputPath.lexically_normal() == outputPath.lexically_normal() ||
            replacementPath.lexically_normal() == outputPath.lexically_normal())
        {
            printf("ERROR: Input and output paths are the same.\n");
            return 1;
        }
        return replace(inputPath, row, column, replacementPath, outputPath) ? 0 : 1;
    }
    printUsage();
    return 1;
}

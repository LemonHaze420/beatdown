#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
using TextTableBytes = std::vector<std::uint8_t>;
constexpr std::size_t TEXT_TABLE_HEADER_SIZE = 0x08;
constexpr std::size_t TEXT_TABLE_ENTRY_SIZE = 0x08;

constexpr std::size_t TEXT_TABLE_PAYLOAD_ALIGN = 4;

enum TextTableLanguage : std::uint8_t
{
    TEXT_TABLE_LANGUAGE_JAPANESE = 0,
    TEXT_TABLE_LANGUAGE_ENGLISH = 1,
    TEXT_TABLE_LANGUAGE_FRENCH = 2,
    TEXT_TABLE_LANGUAGE_GERMAN = 3,
    TEXT_TABLE_LANGUAGE_SPANISH = 4,
    TEXT_TABLE_LANGUAGE_ITALIAN = 5
};
constexpr std::size_t TEXT_TABLE_LANGUAGE_COUNT = 6;
struct TextTableRecord
{
    std::uint16_t textId = 0;

    bool suppressed = false;

    bool terminated = true;

    std::string text;
};
struct TextTable
{
    std::uint16_t encoding = 0;
    std::vector<TextTableRecord> records;

    TextTableBytes tailPadding;
};
bool readTextTable(const TextTableBytes& data, TextTable& table);
bool writeTextTable(const TextTable& table, TextTableBytes& data);
bool readTextTableSource(const std::string& source, TextTable& table);
bool writeTextTableSource(const TextTable& table, std::string& source);
const TextTableRecord* findTextTableRecord(const TextTable& table,
                                           std::uint16_t textId);
const char* textTableEncodingName(std::uint16_t encoding);
const char* textTableLanguageName(std::uint8_t language);
char textTableLanguageSuffix(std::uint8_t language);
bool textTableEncodingForLanguage(std::uint8_t language,
                                  std::uint16_t& encoding);

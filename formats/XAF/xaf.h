#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

constexpr std::uint32_t XAF_MAGIC = 0x00464158;
constexpr std::uint32_t XAF_ALIGN = 0x800;
constexpr std::size_t XAF_HEADER_SIZE = 0x40;
constexpr std::size_t XAF_DESCRIPTION_SIZE = 0x08;
constexpr std::size_t XAF_ENTRY_SIZE = 0x104;
constexpr std::size_t XAF_PATH_SIZE = 0xFC;

struct XafDescription
{
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct XafEntry
{
    std::uint32_t metaOffset = 0;
    std::uint32_t metaLength = 0;
    std::array<std::uint8_t, XAF_PATH_SIZE> rawPath{};
    std::string path;
};

struct XafHeader
{
    std::uint32_t magic = 0;
    std::uint32_t flags = 0;
    std::array<char, 29> signature{};
    std::uint32_t reserved0 = 0;
    std::uint32_t entryCountPlusOne = 0;
    std::uint32_t entryCount = 0;
    std::uint32_t archiveSize = 0;
    std::array<std::uint32_t, 3> reserved{};
};

struct XafArchive
{
    std::uint64_t fileSize = 0;
    std::uint64_t tableSize = 0;
    std::uint64_t payloadEnd = 0;
    std::uint64_t auxOffset = 0;
    XafHeader header{};
    std::vector<XafDescription> descs;
    std::vector<XafEntry> entries;
};

bool readXaf(const std::filesystem::path& path, XafArchive& xaf);
bool extractXaf(const std::filesystem::path& inputPath,
                const std::filesystem::path& outputPath);
bool replaceXaf(const std::filesystem::path& archivePath,
                std::string_view archiveEntry,
                const std::filesystem::path& replacementPath,
                const std::filesystem::path& outputPath);

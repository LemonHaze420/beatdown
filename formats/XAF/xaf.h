#pragma once
#include "xaf_format.h"
#include <filesystem>
#include <string_view>

bool readXaf(const std::filesystem::path& path, XafArchive& xaf);
bool extractXaf(const std::filesystem::path& inputPath,
                const std::filesystem::path& outputPath);
bool replaceXaf(const std::filesystem::path& archivePath,
                std::string_view archiveEntry,
                const std::filesystem::path& replacementPath,
                const std::filesystem::path& outputPath);

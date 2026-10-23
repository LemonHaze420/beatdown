#ifndef XAF_RUNTIME_H
#define XAF_RUNTIME_H
#include <stdint.h>
#include <string>
#include <vector>
struct XafEntryInfo
{
    std::string name;
    uint32_t offset;
    uint32_t length;
};
bool listXafEntries(const char* archivePath,
                    std::vector<XafEntryInfo>& entries);
bool readXafEntry(const char* archivePath, const XafEntryInfo& entry,
                  std::vector<uint8_t>& data);
bool readXafEntry(const char* archivePath, const char* entryName,
                  std::vector<uint8_t>& data);
bool readXafEntryByIndex(const char* archivePath, uint32_t entryIndex,
                         std::vector<uint8_t>& data);
bool extractPackedBundleEntry(const std::vector<uint8_t>& bundle,
                              uint32_t entryIndex,
                              std::vector<uint8_t>& data);
#endif

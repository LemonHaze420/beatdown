#include "xaf_runtime.h"

#include <stdio.h>
#include <string.h>

namespace
{

const uint32_t XAF_MAGIC = 0x00464158;
const uint32_t XAF_HEADER_SIZE = 0x40;
const uint32_t XAF_DESCRIPTION_SIZE = 8;
const uint32_t XAF_ENTRY_SIZE = 0x104;
const uint32_t XAF_PATH_SIZE = 0xfc;

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

bool readAt(FILE* file, uint32_t offset, void* data, size_t size)
{
    return fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
           fread(data, 1, size, file) == size;
}

}

bool
listXafEntries(const char* archivePath, std::vector<XafEntryInfo>& entries)
{
    entries.clear();
    if(archivePath == NULL)
        return false;

    FILE* file = fopen(archivePath, "rb");
    if(file == NULL)
        return false;
    uint8_t header[XAF_HEADER_SIZE];
    if(!readAt(file, 0, header, sizeof(header)) || read32(header) != XAF_MAGIC)
    {
        fclose(file);
        return false;
    }

    const uint32_t count = read32(header + 0x2c);
    const uint32_t archiveSize = read32(header + 0x30);
    const uint64_t tableSize = XAF_HEADER_SIZE +
        static_cast<uint64_t>(count) *
        (XAF_DESCRIPTION_SIZE + XAF_ENTRY_SIZE);
    if(count == 0 || count > 0x10000 || tableSize > archiveSize)
    {
        fclose(file);
        return false;
    }

    entries.reserve(count);
    const uint32_t entryTable = XAF_HEADER_SIZE + count * XAF_DESCRIPTION_SIZE;
    for(uint32_t i = 0; i < count; ++i)
    {
        uint8_t description[XAF_DESCRIPTION_SIZE];
        uint8_t entry[XAF_ENTRY_SIZE];
        if(!readAt(file, XAF_HEADER_SIZE + i * XAF_DESCRIPTION_SIZE,
                   description, sizeof(description)) ||
           !readAt(file, entryTable + i * XAF_ENTRY_SIZE,
                   entry, sizeof(entry)))
        {
            entries.clear();
            fclose(file);
            return false;
        }
        entry[8 + XAF_PATH_SIZE - 1] = 0;
        XafEntryInfo info;
        info.name = reinterpret_cast<const char*>(entry + 8);
        info.offset = read32(description);
        info.length = read32(description + 4);
        if(info.length == 0 ||
           static_cast<uint64_t>(info.offset) + info.length > archiveSize)
        {
            entries.clear();
            fclose(file);
            return false;
        }
        entries.push_back(info);
    }
    fclose(file);
    return true;
}

bool
readXafEntry(const char* archivePath, const XafEntryInfo& entry,
             std::vector<uint8_t>& data)
{
    data.clear();
    if(archivePath == NULL || entry.length == 0)
        return false;
    FILE* file = fopen(archivePath, "rb");
    if(file == NULL)
        return false;
    data.resize(entry.length);
    const bool ok = readAt(file, entry.offset, data.data(), data.size());
    fclose(file);
    if(!ok)
        data.clear();
    return ok;
}

bool
readXafEntry(const char* archivePath, const char* entryName,
             std::vector<uint8_t>& data)
{
    data.clear();
    if(archivePath == NULL || entryName == NULL)
        return false;

    FILE* file = fopen(archivePath, "rb");
    if(file == NULL)
    {
        printf("ERROR: Could not open XAF archive '%s'.\n", archivePath);
        return false;
    }

    uint8_t header[XAF_HEADER_SIZE];
    if(!readAt(file, 0, header, sizeof(header)) || read32(header) != XAF_MAGIC)
    {
        printf("ERROR: Invalid XAF header in '%s'.\n", archivePath);
        fclose(file);
        return false;
    }

    const uint32_t entryCount = read32(header + 0x2c);
    const uint32_t archiveSize = read32(header + 0x30);
    const uint64_t tableSize = XAF_HEADER_SIZE +
        static_cast<uint64_t>(entryCount) *
        (XAF_DESCRIPTION_SIZE + XAF_ENTRY_SIZE);
    if(entryCount == 0 || entryCount > 0x10000 || tableSize > archiveSize)
    {
        printf("ERROR: Invalid XAF table in '%s'.\n", archivePath);
        fclose(file);
        return false;
    }

    uint32_t foundIndex = entryCount;
    uint8_t entry[XAF_ENTRY_SIZE];
    const uint32_t entryTable =
        XAF_HEADER_SIZE + entryCount * XAF_DESCRIPTION_SIZE;
    for(uint32_t i = 0; i < entryCount; i++)
    {
        if(!readAt(file, entryTable + i * XAF_ENTRY_SIZE,
                   entry, sizeof(entry)))
            break;
        entry[8 + XAF_PATH_SIZE - 1] = 0;
        if(strcmp(reinterpret_cast<const char*>(entry + 8), entryName) == 0)
        {
            foundIndex = i;
            break;
        }
    }

    if(foundIndex == entryCount)
    {
        printf("ERROR: XAF entry '%s' was not found in '%s'.\n",
               entryName, archivePath);
        fclose(file);
        return false;
    }

    uint8_t description[XAF_DESCRIPTION_SIZE];
    if(!readAt(file,
               XAF_HEADER_SIZE + foundIndex * XAF_DESCRIPTION_SIZE,
               description, sizeof(description)))
    {
        printf("ERROR: Could not read XAF entry descriptor in '%s'.\n",
               archivePath);
        fclose(file);
        return false;
    }

    const uint32_t offset = read32(description);
    const uint32_t length = read32(description + 4);
    if(length == 0 || static_cast<uint64_t>(offset) + length > archiveSize)
    {
        printf("ERROR: Invalid XAF entry span for '%s'.\n", entryName);
        fclose(file);
        return false;
    }

    data.resize(length);
    const bool read = readAt(file, offset, data.data(), data.size());
    fclose(file);
    if(!read)
    {
        printf("ERROR: Could not read XAF entry '%s'.\n", entryName);
        data.clear();
        return false;
    }
    return true;
}

bool
readXafEntryByIndex(const char* archivePath, uint32_t entryIndex,
                    std::vector<uint8_t>& data)
{
    // The runtime addresses stage resources by archive index, not by name
    // (`BeginXafEntryLoadByIndex` 0x0029b270), so the index path is primary
    // for stage loading.
    std::vector<XafEntryInfo> entries;
    if(!listXafEntries(archivePath, entries) || entryIndex >= entries.size())
    {
        printf("ERROR: XAF entry %u is out of range in '%s'.\n",
               static_cast<unsigned>(entryIndex), archivePath);
        return false;
    }
    return readXafEntry(archivePath, entries[entryIndex], data);
}

bool
extractPackedBundleEntry(const std::vector<uint8_t>& bundle,
                         uint32_t entryIndex,
                         std::vector<uint8_t>& data)
{
    data.clear();
    if(bundle.size() < 8)
        return false;

    const uint32_t entryCount = read32(bundle.data());
    const uint64_t tableEnd = 4 + static_cast<uint64_t>(entryCount) * 4;
    if(entryIndex >= entryCount || tableEnd > bundle.size())
        return false;

    const uint32_t encodedOffset = read32(bundle.data() + 4 + entryIndex * 4);
    const uint32_t storage = encodedOffset >> 28;
    const uint32_t offset = encodedOffset & 0x0fffffff;
    uint32_t end = static_cast<uint32_t>(bundle.size());
    if(entryIndex + 1 < entryCount)
        end = read32(bundle.data() + 8 + entryIndex * 4) & 0x0fffffff;
    if(offset < tableEnd || offset >= end || end > bundle.size())
        return false;

    if(storage == 0)
    {
        data.assign(bundle.begin() + offset, bundle.begin() + end);
        return true;
    }
    if(storage != 8 || end - offset < 4)
        return false;

    const uint32_t outputSize = read32(bundle.data() + offset);
    if(outputSize == 0 || outputSize > 0x1000000)
        return false;
    data.resize(outputSize);

    uint8_t window[0x1000];
    memset(window, 0, sizeof(window));
    uint32_t writeCursor = 0xfee;
    uint32_t source = offset + 4;
    uint32_t output = 0;
    uint32_t flags = 0;
    while(output < outputSize)
    {
        flags >>= 1;
        if((flags & 0x100) == 0)
        {
            if(source >= end)
                goto fail;
            flags = bundle[source++] | 0xff00;
        }

        if((flags & 1) != 0)
        {
            if(source >= end)
                goto fail;
            const uint8_t value = bundle[source++];
            data[output++] = value;
            window[writeCursor] = value;
            writeCursor = (writeCursor + 1) & 0xfff;
        }
        else
        {
            if(source + 2 > end)
                goto fail;
            const uint8_t first = bundle[source++];
            const uint8_t second = bundle[source++];
            const uint32_t windowOffset = first | ((second & 0xf0) << 4);
            const uint32_t length = (second & 0x0f) + 3;
            for(uint32_t i = 0; i < length && output < outputSize; i++)
            {
                const uint8_t value = window[(windowOffset + i) & 0xfff];
                data[output++] = value;
                window[writeCursor] = value;
                writeCursor = (writeCursor + 1) & 0xfff;
            }
        }
    }
    return true;

fail:
    data.clear();
    return false;
}

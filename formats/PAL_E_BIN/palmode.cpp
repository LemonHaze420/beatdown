#include "palmode.h"
#include "../common.h"
#include "../RWS/rws.h"
#include <stdio.h>
#include <string.h>
namespace
{
size_t paletteBytesForDepth(uint32_t depth)
{
    if(depth == 4)
        return 64;
    if(depth == 8)
        return 1024;
    return 0;
}

bool readImage(const std::vector<uint8_t>& data, size_t offset,
               PalModeImage& image, size_t& end, std::string* error)
{
    if(offset + 12 > data.size())
    {
        if(error != NULL)
            *error = "image header runs past the end of the package";
        return false;
    }
    const uint8_t* header = data.data() + offset;
    const uint32_t type = read32(header);
    const uint32_t length = read32(header + 4);
    if (type != RW_CHUNK_IMAGE)
    {
        if(error != NULL)
            *error = "record is not a RenderWare Image chunk";
        return false;
    }
    if(length < 28 || offset + 12 + length > data.size())
    {
        if(error != NULL)
            *error = "image payload runs past the end of the package";
        return false;
    }
    const size_t structOffset = offset + 12;
    if(read32(data.data() + structOffset) != 0x01 ||
       read32(data.data() + structOffset + 4) != 16)
    {
        if(error != NULL)
            *error = "image is missing its 16-byte struct child";
        return false;
    }
    const uint8_t* fields = data.data() + structOffset + 12;
    image.width = read32(fields);
    image.height = read32(fields + 4);
    image.depth = read32(fields + 8);
    image.pitchBytes = read32(fields + 12);
    if(image.width == 0 || image.height == 0 || image.pitchBytes == 0)
    {
        if(error != NULL)
            *error = "image has a zero dimension";
        return false;
    }
    image.chunkOffset = structOffset;
    image.chunkLength = length;
    image.rasterOffset = structOffset + 28;
    image.rasterSize =
        static_cast<size_t>(image.pitchBytes) * image.height;
    image.paletteSize = paletteBytesForDepth(image.depth);
    image.paletteOffset = image.rasterOffset + image.rasterSize;
    if(static_cast<size_t>(length) !=
       28 + image.rasterSize + image.paletteSize)
    {
        if(error != NULL)
            *error = "image length does not match its raster and palette";
        return false;
    }
    end = offset + 12 + length;
    return true;
}
}
const char*
palModeFileForLanguage(int language)
{
    switch(language)
    {
    case 2: return "PAL_F.BIN";
    case 3: return "PAL_G.BIN";
    case 4: return "PAL_S.BIN";
    case 5: return "PAL_I.BIN";
    }
    return "PAL_E.BIN";
}
bool
loadPalModePackage(const char* path, PalModePackage& package,
                   std::string* error)
{
    package.valid = false;
    package.data.clear();
    memset(package.images, 0, sizeof(package.images));
    if(path == NULL)
    {
        if(error != NULL)
            *error = "no package path";
        return false;
    }
    FILE* input = fopen(path, "rb");
    if(input == NULL)
    {
        if(error != NULL)
            *error = "package could not be opened";
        return false;
    }
    fseek(input, 0, SEEK_END);
    const long size = ftell(input);
    fseek(input, 0, SEEK_SET);
    if(size <= 0)
    {
        fclose(input);
        if(error != NULL)
            *error = "package is empty";
        return false;
    }
    package.data.resize(static_cast<size_t>(size));
    const bool read =
        fread(package.data.data(), 1, package.data.size(), input) ==
        package.data.size();
    if(fclose(input) != 0 || !read)
    {
        package.data.clear();
        if(error != NULL)
            *error = "package could not be read";
        return false;
    }
    size_t offset = 0;
    for(int i = 0; i < PALMODE_IMAGE_COUNT; ++i)
    {
        size_t end = 0;
        if(!readImage(package.data, offset, package.images[i], end, error))
        {
            package.data.clear();
            return false;
        }
        offset = end;
    }

    if(offset != package.data.size())
    {
        package.data.clear();
        if(error != NULL)
            *error = "package has trailing data after its three images";
        return false;
    }
    package.valid = true;
    return true;
}
bool
decodePalModeImage(const PalModePackage& package, int imageId,
                   std::vector<uint8_t>& rgba)
{
    rgba.clear();
    if(!package.valid || imageId < 0 || imageId >= PALMODE_IMAGE_COUNT)
        return false;
    const PalModeImage& image = package.images[imageId];
    if(image.depth != 8 || image.paletteSize != 1024)
        return false;
    if(image.paletteOffset + image.paletteSize > package.data.size())
        return false;
    const uint8_t* raster = package.data.data() + image.rasterOffset;
    const uint8_t* palette = package.data.data() + image.paletteOffset;
    rgba.resize(static_cast<size_t>(image.width) * image.height * 4);
    for(uint32_t y = 0; y < image.height; ++y)
    {
        const uint8_t* row = raster + static_cast<size_t>(y) * image.pitchBytes;
        uint8_t* out = rgba.data() +
            static_cast<size_t>(y) * image.width * 4;
        for(uint32_t x = 0; x < image.width; ++x)
        {
            const uint8_t* entry = palette + static_cast<size_t>(row[x]) * 4;
            out[x * 4 + 0] = entry[0];
            out[x * 4 + 1] = entry[1];
            out[x * 4 + 2] = entry[2];
            out[x * 4 + 3] = entry[3];
        }
    }
    return true;
}

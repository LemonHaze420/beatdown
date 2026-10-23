#ifndef PALMODE_H
#define PALMODE_H
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

const int PALMODE_IMAGE_COUNT = 3;

enum PalModeImageId
{
    PALMODE_IMAGE_MESS_E = 0,
    PALMODE_IMAGE_VSCRN = 1,
    PALMODE_IMAGE_TEST = 2
};

struct PalModeImage
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t pitchBytes;

    size_t chunkOffset;
    size_t chunkLength;

    size_t rasterOffset;
    size_t rasterSize;
    size_t paletteOffset;
    size_t paletteSize;
};
struct PalModePackage
{
    std::vector<uint8_t> data;
    PalModeImage images[PALMODE_IMAGE_COUNT];
    bool valid;
    PalModePackage() : valid(false) {}
};

const char* palModeFileForLanguage(int language);

bool loadPalModePackage(const char* path, PalModePackage& package,
                        std::string* error = NULL);

bool decodePalModeImage(const PalModePackage& package, int imageId,
                        std::vector<uint8_t>& rgba);
#endif

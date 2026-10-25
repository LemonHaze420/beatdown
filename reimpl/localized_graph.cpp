#include "localized_graph.h"
#include "formats/common.h"

#include <string.h>

namespace
{

const LocalizedTitleGraphPair TITLE_PAIRS[LOCALIZED_GRAPH_LANGUAGE_COUNT] =
{
    { 0x01a, 0x172 }, { 0x01a, 0x173 }, { 0x01c, 0x173 },
    { 0x01d, 0x173 }, { 0x01b, 0x173 }, { 0x01e, 0x173 }
};

const uint16_t MENU_ALL_INDICES[LOCALIZED_GRAPH_LANGUAGE_COUNT] =
{
    600, 600, 602, 603, 601, 604
};

const uint16_t STORY_PARTS_INDICES[LOCALIZED_GRAPH_LANGUAGE_COUNT] =
{
    540, 540, 542, 543, 541, 544
};

const uint16_t INFO_INDICES[LOCALIZED_GRAPH_LANGUAGE_COUNT]
                           [LOCALIZED_GRAPH_INFO_FAMILY_COUNT] =
{
    { 551, 556, 561, 566, 571 },
    { 551, 556, 561, 566, 571 },
    { 553, 558, 563, 568, 573 },
    { 554, 559, 564, 569, 574 },
    { 552, 557, 562, 567, 572 },
    { 555, 560, 565, 570, 575 }
};

const uint16_t MEMORY_CARD_INDICES[LOCALIZED_GRAPH_LANGUAGE_COUNT] =
{
    577, 577, 579, 580, 578, 581
};

const uint16_t CREDITS_REQUESTS[LOCALIZED_GRAPH_CREDITS_REQUEST_COUNT] =
{
    0x055, 0x149, 0x027, 0x026, 0xffff
};

const LocalizedInfoSpriteLayout INFO_SPRITES
    [LOCALIZED_GRAPH_INFO_SIDE_COUNT][LOCALIZED_GRAPH_INFO_SPRITE_COUNT] =
{
    {
        { 0,  20.0f, 105.0f,   0.0f,  96.0f, 29.0f, 32.0f, 256.0f, 256.0f },
        { 1,  30.0f, 104.0f, 154.0f,   0.0f, 21.0f,  8.0f, 256.0f, 128.0f },
        { 0,  49.0f, 120.0f,  29.0f, 112.0f, 73.0f, 16.0f, 256.0f, 256.0f },
        { 1, 143.0f, 121.0f, 154.0f,   8.0f, 21.0f,  8.0f, 256.0f, 128.0f },
        { 0,  49.0f, 110.0f, 102.0f,  96.0f, 77.0f, 16.0f, 256.0f, 256.0f },
        { 1,  49.0f, 110.0f,   0.0f,   0.0f, 77.0f, 16.0f, 256.0f, 128.0f }
    },
    {
        { 0, 591.0f, 105.0f,   0.0f,  96.0f, 29.0f, 32.0f, 256.0f, 256.0f },
        { 1, 589.0f, 104.0f, 154.0f,   0.0f, 21.0f,  8.0f, 256.0f, 128.0f },
        { 0, 497.0f, 120.0f,  29.0f, 112.0f, 73.0f, 16.0f, 256.0f, 256.0f },
        { 1, 479.0f, 121.0f, 154.0f,   8.0f, 21.0f,  8.0f, 256.0f, 128.0f },
        { 0, 491.0f, 110.0f, 102.0f,  96.0f, 77.0f, 16.0f, 256.0f, 256.0f },
        { 1, 491.0f, 110.0f,   0.0f,   0.0f, 77.0f, 16.0f, 256.0f, 128.0f }
    }
};

bool decompressMenuLzss(const uint8_t* input, size_t inputSize,
                        uint8_t* output, size_t outputSize)
{
    uint8_t window[0x1000] = {};
    size_t inputOffset = 0;
    size_t outputOffset = 0;
    uint32_t windowOffset = 0xfee;
    while(outputOffset < outputSize)
    {
        if(inputOffset >= inputSize)
            return false;
        const uint8_t flags = input[inputOffset++];
        for(uint32_t bit = 1; bit != 0x100 && outputOffset < outputSize;
            bit <<= 1)
        {
            if((flags & bit) != 0)
            {
                if(inputOffset >= inputSize)
                    return false;
                const uint8_t value = input[inputOffset++];
                output[outputOffset++] = value;
                window[windowOffset] = value;
                windowOffset = (windowOffset + 1) & 0xfff;
                continue;
            }

            if(inputOffset + 2 > inputSize)
                return false;
            const uint8_t low = input[inputOffset++];
            const uint8_t highAndLength = input[inputOffset++];
            uint32_t source = low |
                (static_cast<uint32_t>(highAndLength & 0xf0) << 4);
            const uint32_t length = (highAndLength & 0x0f) + 3;
            for(uint32_t index = 0;
                index < length && outputOffset < outputSize; ++index)
            {
                const uint8_t value = window[source];
                source = (source + 1) & 0xfff;
                output[outputOffset++] = value;
                window[windowOffset] = value;
                windowOffset = (windowOffset + 1) & 0xfff;
            }
        }
    }
    return true;
}

}

uint8_t
localizedGraphLanguage(uint8_t language)
{
    return language < LOCALIZED_GRAPH_LANGUAGE_COUNT ? language : 1;
}

LocalizedTitleGraphPair
localizedTitleGraphPair(uint8_t language)
{
    return TITLE_PAIRS[localizedGraphLanguage(language)];
}

uint16_t
localizedMenuAllGraphIndex(uint8_t language)
{
    return MENU_ALL_INDICES[localizedGraphLanguage(language)];
}

LocalizedStorySelectGraphSet
localizedStorySelectGraphSet(uint8_t language)
{

    LocalizedStorySelectGraphSet result =
    {
        { 534, 535, 536, 537, 538 },
        539,
        STORY_PARTS_INDICES[localizedGraphLanguage(language)],
        545
    };
    return result;
}

uint16_t
localizedInfoTextureGraphIndex(uint8_t language, int family)
{
    if(family < 0 || family >= LOCALIZED_GRAPH_INFO_FAMILY_COUNT)
        return 0xffff;
    return INFO_INDICES[localizedGraphLanguage(language)][family];
}

LocalizedMemoryCardGraphSet
localizedMemoryCardGraphSet(uint8_t language)
{
    const LocalizedMemoryCardGraphSet result =
    {
        576, MEMORY_CARD_INDICES[localizedGraphLanguage(language)]
    };
    return result;
}

const uint16_t*
localizedCreditsGraphRequests(void)
{
    return CREDITS_REQUESTS;
}

const LocalizedInfoSpriteLayout*
localizedInfoSpriteLayouts(int side)
{
    if(side < 0 || side >= LOCALIZED_GRAPH_INFO_SIDE_COUNT)
        return NULL;
    return INFO_SPRITES[side];
}

bool
extractLocalizedMenuMember(const uint8_t* bytes, size_t size,
                           uint32_t memberIndex,
                           std::vector<uint8_t>& output)
{
    output.clear();
    if(bytes == NULL || size < 8)
        return false;
    const uint32_t count = read32(bytes);
    if(count == 0 || count > 0x10000 || memberIndex >= count ||
       static_cast<uint64_t>(count + 1) * 4 > size)
        return false;

    const uint32_t encoded = read32(bytes + 4 + memberIndex * 4);
    if((encoded >> 28) != 8)
        return false;
    const uint32_t offset = encoded & 0x0fffffff;
    uint32_t end = static_cast<uint32_t>(size);
    if(memberIndex + 1 < count)
    {
        const uint32_t next = read32(bytes + 8 + memberIndex * 4);
        if((next >> 28) != 8)
            return false;
        end = next & 0x0fffffff;
    }
    if(offset < (count + 1) * 4 || offset + 4 > end || end > size)
        return false;

    const uint32_t outputSize = read32(bytes + offset);
    if(outputSize == 0 || outputSize > 0x1000000)
        return false;
    output.resize(outputSize);
    if(!decompressMenuLzss(bytes + offset + 4, end - offset - 4,
                           output.data(), output.size()))
    {
        output.clear();
        return false;
    }
    return true;
}

bool
localizedGraphImageStream(const uint8_t* bytes, size_t size,
                          uint32_t* width, uint32_t* height)
{
    if(width != NULL)
        *width = 0;
    if(height != NULL)
        *height = 0;
    if(bytes == NULL || size < 40 || read32(bytes) != 0x18)
        return false;
    const uint32_t rootLength = read32(bytes + 4);
    if(static_cast<uint64_t>(rootLength) + 12 != size ||
       read32(bytes + 12) != 1 || read32(bytes + 16) != 16)
        return false;
    const uint32_t imageWidth = read32(bytes + 24);
    const uint32_t imageHeight = read32(bytes + 28);
    const uint32_t depth = read32(bytes + 32);
    const uint32_t pitch = read32(bytes + 36);
    if(imageWidth == 0 || imageHeight == 0 || imageWidth > 4096 ||
       imageHeight > 4096 ||
       (depth != 4 && depth != 8 && depth != 16 &&
        depth != 24 && depth != 32))
        return false;
    const uint32_t paletteSize = depth == 4 ? 64 : depth == 8 ? 1024 : 0;
    if(static_cast<uint64_t>(40) +
       static_cast<uint64_t>(pitch) * imageHeight + paletteSize != size)
        return false;
    if(width != NULL)
        *width = imageWidth;
    if(height != NULL)
        *height = imageHeight;
    return true;
}

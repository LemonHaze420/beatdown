#ifndef REIMPL_LOCALIZED_GRAPH_H
#define REIMPL_LOCALIZED_GRAPH_H

#include <stddef.h>
#include <stdint.h>

#include <vector>

const int LOCALIZED_GRAPH_LANGUAGE_COUNT = 6;
const int LOCALIZED_GRAPH_INFO_FAMILY_COUNT = 5;
const int LOCALIZED_GRAPH_MENU_MEMBER_COUNT = 82;
const int LOCALIZED_GRAPH_CREDITS_REQUEST_COUNT = 5;
const int LOCALIZED_GRAPH_INFO_SIDE_COUNT = 2;
const int LOCALIZED_GRAPH_INFO_SPRITE_COUNT = 6;

struct LocalizedTitleGraphPair
{
    uint16_t brand;
    uint16_t title;
};

struct LocalizedStorySelectGraphSet
{
    uint16_t portraits[5];
    uint16_t font;
    uint16_t localizedParts;
    uint16_t commonParts;
};

struct LocalizedMemoryCardGraphSet
{
    uint16_t font;
    uint16_t localizedParts;
};

struct LocalizedInfoSpriteLayout
{
    uint8_t texture;
    float x;
    float y;
    float sourceX;
    float sourceY;
    float sourceWidth;
    float sourceHeight;
    float atlasWidth;
    float atlasHeight;
};

uint8_t localizedGraphLanguage(uint8_t language);
LocalizedTitleGraphPair localizedTitleGraphPair(uint8_t language);
uint16_t localizedMenuAllGraphIndex(uint8_t language);
LocalizedStorySelectGraphSet localizedStorySelectGraphSet(uint8_t language);
uint16_t localizedInfoTextureGraphIndex(uint8_t language, int family);
LocalizedMemoryCardGraphSet localizedMemoryCardGraphSet(uint8_t language);
const uint16_t* localizedCreditsGraphRequests(void);
const LocalizedInfoSpriteLayout* localizedInfoSpriteLayouts(int side);

bool extractLocalizedMenuMember(const uint8_t* bytes, size_t size,
                                uint32_t memberIndex,
                                std::vector<uint8_t>& output);

bool localizedGraphImageStream(const uint8_t* bytes, size_t size,
                               uint32_t* width = NULL,
                               uint32_t* height = NULL);

#endif

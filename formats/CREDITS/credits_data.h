#ifndef CREDITS_DATA_H
#define CREDITS_DATA_H
#include <stdint.h>
const int CREDITS_TEXT_ROW_COUNT = 215;
const int CREDITS_LOGO_COUNT = 4;
const float CREDITS_SCROLL_DISTANCE = 11024.0f;
struct CreditsTextRow
{
    float y;
    float x;
    uint8_t style;
    uint8_t reserved;
    uint16_t textId;
};
struct CreditsLogoLayout
{
    float sourceX;
    float sourceY;
    float sourceWidth;
    float sourceHeight;
    float textureWidth;
    float textureHeight;
    float reserved;
};
extern const CreditsTextRow CREDITS_TEXT_ROWS[CREDITS_TEXT_ROW_COUNT];
extern const uint16_t CREDITS_LOGO_GRAPH_INDICES[CREDITS_LOGO_COUNT];
extern const CreditsLogoLayout CREDITS_LOGO_LAYOUTS[CREDITS_LOGO_COUNT];
#endif

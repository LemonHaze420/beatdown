#ifndef MENU_FONT_H
#define MENU_FONT_H
#include <stdint.h>

const int MENU_GLYPH_COUNT = 224;
const int MENU_GLYPH_FIRST = 0x20;

const float MENU_CELL_WIDTH = 32.0f;
const float MENU_CELL_HEIGHT = 42.0f;

const float MENU_ATLAS_WIDTH = 512.0f;
const float MENU_ATLAS_HEIGHT = 588.0f;

const int MENU_ATLAS_COLUMNS = 16;

const int MENU_FONT_ATLAS_ENTRY = 0x164;
const int MENU_FONT_ATLAS_ROWS = 14;

const int MENU_TAB_CELLS = 4;
struct MenuGlyphMetrics
{

    int16_t bearing;

    int16_t advance;
};

enum RetainedFontPage
{
    RETAINED_FONT_PAGE_BASIC = 0,
    RETAINED_FONT_PAGE_KANJI = 1,
    RETAINED_FONT_PAGE_CONTROLS = 5,
    RETAINED_FONT_PAGE_BATTLE = 6
};
const int RETAINED_FONT_BASIC_ATLAS_ENTRY = 0x179;
const int RETAINED_FONT_KANJI_ATLAS_ENTRY = 0x17a;
const int RETAINED_FONT_CONTROLS_ATLAS_ENTRY = 0x20;
const int RETAINED_FONT_BASIC_ATLAS_WIDTH = 512;
const int RETAINED_FONT_BASIC_ATLAS_HEIGHT = 512;
const int RETAINED_FONT_KANJI_ATLAS_WIDTH = 1024;
const int RETAINED_FONT_KANJI_ATLAS_HEIGHT = 1024;
const int RETAINED_FONT_CONTROLS_ATLAS_WIDTH = 256;
const int RETAINED_FONT_CONTROLS_ATLAS_HEIGHT = 128;
const int RETAINED_FONT_BATTLE_ATLAS_WIDTH = 64;
const int RETAINED_FONT_BATTLE_ATLAS_HEIGHT = 96;

const float RETAINED_FONT_LINE_PITCH = 24.0f;
const float RETAINED_FONT_ADVANCE_SCALE = 0.95f;
struct RetainedFontGlyph
{
    uint16_t codepoint;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint8_t page;
    int8_t yOffset;
};

uint16_t retainedFontCodepoint(uint32_t codepoint);

uint16_t retainedFontBoundCodepoint(uint32_t codepoint,
                                    uint8_t controllerType,
                                    bool japanese);

uint32_t retainedFontNextCodepoint(const unsigned char*& cursor);
bool retainedFontGlyphForCodepoint(uint32_t codepoint,
                                   RetainedFontGlyph* glyph);
bool retainedFontPageSize(uint8_t page, int* width, int* height);
float retainedFontGlyphAdvance(const RetainedFontGlyph& glyph);

struct RetainedFontTextExtent
{
    float width;
    float height;
};
RetainedFontTextExtent retainedFontTextExtent(const char* text,
                                              float scaleX,
                                              float scaleY);

bool retainedFontShiftJisCell(uint8_t lead, uint8_t trail,
                              uint16_t* glyphIndex);
extern const MenuGlyphMetrics MENU_GLYPH_METRICS[MENU_GLYPH_COUNT];

struct MenuTextLayout
{
    float letterSpacing;
    float lineGap;
    float cellWidth;
    float cellHeight;

    bool proportional;
};
MenuTextLayout menuTextLayoutDefaults(void);

float menuGlyphAdvance(unsigned char character, const MenuTextLayout& layout);

float menuTextWidth(const char* text, const MenuTextLayout& layout);
float menuTextHeight(const char* text, const MenuTextLayout& layout);

bool menuGlyphCell(unsigned char character, const MenuTextLayout& layout,
                   float* x, float* y);
#endif

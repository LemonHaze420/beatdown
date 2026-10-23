#include "menu_font.h"
#include <stddef.h>
namespace
{
#include "retained_font_data.inc"
const MenuGlyphMetrics* metricsFor(unsigned char character)
{
    const int index = (int)character - MENU_GLYPH_FIRST;
    if(index < 0 || index >= MENU_GLYPH_COUNT)
        return NULL;
    return &MENU_GLYPH_METRICS[index];
}

bool isSkipped(unsigned char character)
{
    return character == 7 || character == 8 || character == 11 ||
        character == 12 || character == 13;
}
const RetainedFontGlyph*
findSortedGlyph(const RetainedFontGlyph* glyphs, int count,
                uint16_t codepoint)
{
    int first = 0;
    int last = count;
    while(first < last)
    {
        const int middle = first + (last - first) / 2;
        if(glyphs[middle].codepoint < codepoint)
            first = middle + 1;
        else
            last = middle;
    }
    return first < count && glyphs[first].codepoint == codepoint ?
        &glyphs[first] : NULL;
}
const RetainedFontGlyph*
findLinearGlyph(const RetainedFontGlyph* glyphs, int count,
                uint16_t codepoint)
{
    for(int index = 0; index < count; ++index)
        if(glyphs[index].codepoint == codepoint)
            return &glyphs[index];
    return NULL;
}
bool isRetainedPaletteMarker(const unsigned char* cursor)
{
    return (cursor[0] == '$' || cursor[0] == '@') &&
        cursor[1] >= '0' && cursor[1] <= '9';
}
bool isRetainedEncodedNewline(const unsigned char* cursor)
{
    return cursor[0] == '\\' && cursor[1] == 'n';
}
}
MenuTextLayout
menuTextLayoutDefaults(void)
{
    MenuTextLayout layout;
    layout.letterSpacing = 0.0f;
    layout.lineGap = 0.0f;
    layout.cellWidth = MENU_CELL_WIDTH;
    layout.cellHeight = MENU_CELL_HEIGHT;
    layout.proportional = true;
    return layout;
}
float
menuGlyphAdvance(unsigned char character, const MenuTextLayout& layout)
{
    if(character == 0 || isSkipped(character) || character == '\n')
        return 0.0f;
    if(character == '\t')
        return layout.cellWidth * (float)MENU_TAB_CELLS;
    if(character == ' ')
    {

        return layout.proportional ?
            (float)MENU_GLYPH_METRICS[0].advance : layout.cellWidth;
    }
    if(character <= ' ')
        return 0.0f;
    if(layout.proportional)
    {
        const MenuGlyphMetrics* metrics = metricsFor(character);
        if(metrics == NULL)
            return 0.0f;
        return (float)metrics->advance + layout.letterSpacing;
    }
    return layout.cellWidth + layout.letterSpacing;
}
float
menuTextWidth(const char* text, const MenuTextLayout& layout)
{
    if(text == NULL)
        return -1.0f;
    float widest = 0.0f;
    float line = 0.0f;
    for(const unsigned char* cursor = (const unsigned char*)text;
        *cursor != 0; ++cursor)
    {
        if(*cursor == '\n')
        {
            if(line > widest)
                widest = line;
            line = 0.0f;
            continue;
        }
        line += menuGlyphAdvance(*cursor, layout);
    }
    return line > widest ? line : widest;
}
float
menuTextHeight(const char* text, const MenuTextLayout& layout)
{
    if(text == NULL)
        return -1.0f;

    float height = layout.cellHeight;
    for(const unsigned char* cursor = (const unsigned char*)text;
        *cursor != 0; ++cursor)
        if(*cursor == '\n')
            height += layout.cellHeight + layout.lineGap;
    return height;
}
bool
menuGlyphCell(unsigned char character, const MenuTextLayout& layout,
              float* x, float* y)
{
    if(character <= ' ' || metricsFor(character) == NULL)
        return false;

    const int index = (int)character - MENU_GLYPH_FIRST;
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    for(int step = 0; step < index; ++step)
    {
        if((step % MENU_ATLAS_COLUMNS) == MENU_ATLAS_COLUMNS - 1 ||
           cursorX + layout.cellWidth > MENU_ATLAS_WIDTH)
        {
            cursorX = 0.0f;
            cursorY += layout.cellHeight;
        }
        else
            cursorX += layout.cellWidth;
    }
    if(x != NULL)
        *x = cursorX;
    if(y != NULL)
        *y = cursorY;
    return true;
}
uint32_t
retainedFontNextCodepoint(const unsigned char*& cursor)
{
    const uint32_t first = *cursor++;
    if(first < 0x80)
        return first;
    uint32_t codepoint;
    int continuationCount;
    uint32_t minimum;
    if(first >= 0xc2 && first <= 0xdf)
    {
        codepoint = first & 0x1f;
        continuationCount = 1;
        minimum = 0x80;
    }
    else if(first >= 0xe0 && first <= 0xef)
    {
        codepoint = first & 0x0f;
        continuationCount = 2;
        minimum = 0x800;
    }
    else if(first >= 0xf0 && first <= 0xf4)
    {
        codepoint = first & 0x07;
        continuationCount = 3;
        minimum = 0x10000;
    }
    else
        return first;
    const unsigned char* continuation = cursor;
    for(int index = 0; index < continuationCount; ++index)
    {
        if((continuation[index] & 0xc0) != 0x80)
            return first;
        codepoint = (codepoint << 6) | (continuation[index] & 0x3f);
    }
    if(codepoint < minimum || codepoint > 0x10ffff ||
       (codepoint >= 0xd800 && codepoint <= 0xdfff))
        return first;
    cursor += continuationCount;
    return codepoint;
}
uint16_t
retainedFontCodepoint(uint32_t codepoint)
{
    return retainedFontBoundCodepoint(codepoint, 0, false);
}
uint16_t
retainedFontBoundCodepoint(uint32_t codepoint,
                           uint8_t controllerType,
                           bool japanese)
{

    if(codepoint == 0x3000)
        return 0x20;
    if(codepoint >= 0xff01 && codepoint <= 0xff5e)
        return static_cast<uint16_t>(codepoint - 0xfee0);

    if(codepoint >= 0xa0 && codepoint <= 0xa5)
        return static_cast<uint16_t>(0x2460 + codepoint - 0xa0);
    if(codepoint >= 0x90 && codepoint <= 0x93)
        return static_cast<uint16_t>(0x2190 + codepoint - 0x90);
    int action = -1;
    if(codepoint == 0x3006 || codepoint == 0x86)
        action = 0;
    else if(codepoint == 0x25ce || codepoint == 0x8e)
        action = 1;
    else if(codepoint == 0x25c7 || codepoint == 0x87)
        action = 2;
    else if(codepoint == 0x25bd || codepoint == 0xbd)
        action = 3;
    if(action >= 0)
    {
        static const uint16_t BINDINGS[6][4] =
        {
            { 0x100, 0x040, 0x080, 0x200 },
            { 0x100, 0x080, 0x040, 0x200 },
            { 0x080, 0x100, 0x040, 0x200 },
            { 0x080, 0x200, 0x100, 0x040 },
            { 0x040, 0x200, 0x100, 0x080 },
            { 0x040, 0x080, 0x100, 0x200 }
        };
        static const uint8_t JAPANESE_TYPE[6] = { 3, 0, 4, 1, 2, 5 };
        static const uint16_t FALLBACK[4] =
        {
            0x00d7, 0x25cb, 0x25a1, 0x25b3
        };
        uint16_t binding = 0;
        if(controllerType < 6)
        {
            const uint8_t type =
                japanese ? JAPANESE_TYPE[controllerType] : controllerType;
            binding = BINDINGS[type][action];
        }
        switch(binding)
        {
        case 0x200: return 0x25cb;
        case 0x100: return 0x25a1;
        case 0x080: return 0x00d7;
        case 0x040: return 0x25b3;
        default:    return FALLBACK[action];
        }
    }
    return codepoint <= 0xffff ?
        static_cast<uint16_t>(codepoint) : static_cast<uint16_t>('?');
}
bool
retainedFontGlyphForCodepoint(uint32_t codepoint,
                              RetainedFontGlyph* glyph)
{
    const uint16_t internal = retainedFontCodepoint(codepoint);
    const RetainedFontGlyph* found = findLinearGlyph(
        RETAINED_FONT_CONTROLS_GLYPHS,
        RETAINED_FONT_CONTROLS_GLYPH_COUNT, internal);
    if(found == NULL)
    {
        found = findSortedGlyph(RETAINED_FONT_BATTLE_GLYPHS,
                                RETAINED_FONT_BATTLE_GLYPH_COUNT, internal);
    }
    if(found == NULL && internal >= 0x4e00 && internal <= 0x9fa5)
    {
        found = findSortedGlyph(RETAINED_FONT_KANJI_GLYPHS,
                                RETAINED_FONT_KANJI_GLYPH_COUNT, internal);
    }
    if(found == NULL)
    {
        found = findSortedGlyph(RETAINED_FONT_BASIC_GLYPHS,
                                RETAINED_FONT_BASIC_GLYPH_COUNT, internal);
    }
    if(found == NULL)
        return false;
    if(glyph != NULL)
        *glyph = *found;
    return true;
}
bool
retainedFontPageSize(uint8_t page, int* width, int* height)
{
    int pageWidth;
    int pageHeight;
    switch(page)
    {
    case RETAINED_FONT_PAGE_BASIC:
        pageWidth = RETAINED_FONT_BASIC_ATLAS_WIDTH;
        pageHeight = RETAINED_FONT_BASIC_ATLAS_HEIGHT;
        break;
    case RETAINED_FONT_PAGE_KANJI:
        pageWidth = RETAINED_FONT_KANJI_ATLAS_WIDTH;
        pageHeight = RETAINED_FONT_KANJI_ATLAS_HEIGHT;
        break;
    case RETAINED_FONT_PAGE_CONTROLS:
        pageWidth = RETAINED_FONT_CONTROLS_ATLAS_WIDTH;
        pageHeight = RETAINED_FONT_CONTROLS_ATLAS_HEIGHT;
        break;
    case RETAINED_FONT_PAGE_BATTLE:
        pageWidth = RETAINED_FONT_BATTLE_ATLAS_WIDTH;
        pageHeight = RETAINED_FONT_BATTLE_ATLAS_HEIGHT;
        break;
    default:
        return false;
    }
    if(width != NULL)
        *width = pageWidth;
    if(height != NULL)
        *height = pageHeight;
    return true;
}
float
retainedFontGlyphAdvance(const RetainedFontGlyph& glyph)
{

    return static_cast<float>(glyph.width) * RETAINED_FONT_ADVANCE_SCALE;
}
RetainedFontTextExtent
retainedFontTextExtent(const char* text, float scaleX, float scaleY)
{
    RetainedFontTextExtent extent = { 0.0f, 0.0f };
    if(text == NULL || text[0] == 0 || !(scaleX > 0.0f) ||
       !(scaleY > 0.0f))
        return extent;
    float line = 0.0f;
    float widest = 0.0f;
    float linePitch = RETAINED_FONT_LINE_PITCH;
    float totalHeight = 0.0f;
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        if(isRetainedPaletteMarker(cursor))
        {
            cursor += 2;
            continue;
        }
        if(*cursor == '\n' || isRetainedEncodedNewline(cursor))
        {
            cursor += *cursor == '\n' ? 1 : 2;
            if(line > widest)
                widest = line;
            line = 0.0f;
            totalHeight += linePitch;
            linePitch = RETAINED_FONT_LINE_PITCH;
            continue;
        }
        const uint32_t codepoint = retainedFontNextCodepoint(cursor);
        if(codepoint == '\r')
            continue;
        RetainedFontGlyph glyph;
        if(!retainedFontGlyphForCodepoint(codepoint, &glyph) &&
           !retainedFontGlyphForCodepoint('?', &glyph))
            continue;
        line += retainedFontGlyphAdvance(glyph);
        if(glyph.height > linePitch)
            linePitch = static_cast<float>(glyph.height);
    }
    if(line > widest)
        widest = line;
    extent.width = widest * scaleX;
    extent.height = (totalHeight + linePitch) * scaleY;
    return extent;
}
bool
retainedFontShiftJisCell(uint8_t lead, uint8_t trail,
                         uint16_t* glyphIndex)
{
    int row;
    if(lead >= 0x81 && lead <= 0x9f)
        row = (lead - 0x81) * 2;
    else if(lead >= 0xe0 && lead <= 0xef)
        row = (lead - 0xc1) * 2;
    else
        return false;
    int cell;
    if(trail >= 0x40 && trail <= 0x7e)
        cell = trail - 0x40;
    else if(trail >= 0x80 && trail <= 0x9e)
        cell = trail - 0x41;
    else if(trail >= 0x9f && trail <= 0xfc)
    {
        cell = trail - 0x9f;
        ++row;
    }
    else
        return false;
    if(glyphIndex != NULL)
        *glyphIndex = static_cast<uint16_t>(row * 0x5e + cell);
    return true;
}

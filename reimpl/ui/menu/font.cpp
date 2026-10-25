#include "ui/menu/font.h"

#include "character/character.h"
#include "sprite_text.h"

#include <rw.h>
#include <skeleton.h>

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "formats/XAF/xaf_runtime.h"

namespace
{

const char* GRAPH_ARCHIVE_PATH = "raw/XAF/GRAPH.XAF";
const char* MENU_FONT_ATLAS_NAME = "SYSFONT01.STX";
const char* RETAINED_BASIC_ATLAS_NAME = "UNICODEFONT.STX";
const char* RETAINED_KANJI_ATLAS_NAME = "UNICODEFONT_KANJI01.STX";
const char* RETAINED_CONTROLS_ATLAS_NAME = "BUTTON.STX";
const float VIRTUAL_SCREEN_WIDTH = 640.0f;
const float VIRTUAL_SCREEN_HEIGHT = 480.0f;

rw::Texture* g_menuFontTexture;
rw::Texture* g_retainedFontTextures[7];

ImTextureRef textureRef(rw::Texture* texture)
{
    return ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(texture)));
}

unsigned char atlasCharacter(uint32_t codepoint)
{

    if(codepoint <= 0xff)
        return static_cast<unsigned char>(codepoint);
    return '?';
}

float codepointAdvance(uint32_t codepoint)
{
    const MenuTextLayout layout = menuTextLayoutDefaults();
    return menuGlyphAdvance(atlasCharacter(codepoint), layout);
}

void drawTextPass(const char* text, float x, float y,
                  float scaleX, float scaleY,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float screenScaleX =
        static_cast<float>(sk::globals.width) / VIRTUAL_SCREEN_WIDTH;
    const float screenScaleY =
        static_cast<float>(sk::globals.height) / VIRTUAL_SCREEN_HEIGHT;
    const float lineStart = origin.x + x * screenScaleX;
    float penX = lineStart;
    float penY = origin.y + y * screenScaleY;
    const ImU32 tint = IM_COL32(r, g, b, a);
    const ImTextureRef atlas = textureRef(g_menuFontTexture);

    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        const uint32_t codepoint = retainedFontNextCodepoint(cursor);
        if(codepoint == '\n')
        {
            penX = lineStart;
            penY += MENU_CELL_HEIGHT * scaleY * screenScaleY;
            continue;
        }

        const unsigned char character = atlasCharacter(codepoint);
        const float advance = codepointAdvance(codepoint);
        if(character > ' ')
        {
            const int glyphIndex =
                static_cast<int>(character) - MENU_GLYPH_FIRST;
            if(glyphIndex >= 0 && glyphIndex < MENU_GLYPH_COUNT)
            {

                const MenuGlyphMetrics& metrics =
                    MENU_GLYPH_METRICS[glyphIndex];
                const int column = glyphIndex % MENU_ATLAS_COLUMNS;
                const int row = glyphIndex / MENU_ATLAS_COLUMNS;
                const float sourceLeft =
                    column * MENU_CELL_WIDTH + metrics.bearing;
                const float sourceTop = row * MENU_CELL_HEIGHT;
                const float sourceRight = sourceLeft + metrics.advance;
                const float sourceBottom = sourceTop + MENU_CELL_HEIGHT;
                if(metrics.advance > 0 && sourceRight <= MENU_ATLAS_WIDTH &&
                   sourceBottom <= MENU_ATLAS_HEIGHT)
                {
                    drawList->AddImage(
                        atlas, ImVec2(penX, penY),
                        ImVec2(penX + metrics.advance * scaleX * screenScaleX,
                               penY + MENU_CELL_HEIGHT * scaleY * screenScaleY),
                        ImVec2(sourceLeft / MENU_ATLAS_WIDTH,
                               sourceTop / MENU_ATLAS_HEIGHT),
                        ImVec2(sourceRight / MENU_ATLAS_WIDTH,
                               sourceBottom / MENU_ATLAS_HEIGHT), tint);
                }
            }
        }
        penX += advance * scaleX * screenScaleX;
    }
}

rw::Texture* loadRetainedTexture(uint8_t page)
{
    if(page >= sizeof(g_retainedFontTextures) /
                   sizeof(g_retainedFontTextures[0]))
        return NULL;
    if(g_retainedFontTextures[page] != NULL)
        return g_retainedFontTextures[page];

    int entryIndex;
    const char* expectedName;
    if(page == RETAINED_FONT_PAGE_BASIC)
    {
        entryIndex = RETAINED_FONT_BASIC_ATLAS_ENTRY;
        expectedName = RETAINED_BASIC_ATLAS_NAME;
    }
    else if(page == RETAINED_FONT_PAGE_KANJI)
    {
        entryIndex = RETAINED_FONT_KANJI_ATLAS_ENTRY;
        expectedName = RETAINED_KANJI_ATLAS_NAME;
    }
    else if(page == RETAINED_FONT_PAGE_CONTROLS)
    {
        entryIndex = RETAINED_FONT_CONTROLS_ATLAS_ENTRY;
        expectedName = RETAINED_CONTROLS_ATLAS_NAME;
    }
    else
        return NULL;

    int expectedWidth;
    int expectedHeight;
    std::vector<XafEntryInfo> entries;
    if(!retainedFontPageSize(page, &expectedWidth, &expectedHeight) ||
       !listXafEntries(GRAPH_ARCHIVE_PATH, entries) || entryIndex < 0 ||
       static_cast<size_t>(entryIndex) >= entries.size() ||
       entries[entryIndex].name != expectedName)
        return NULL;

    rw::Texture* texture = loadStxTexture(
        GRAPH_ARCHIVE_PATH, entries[entryIndex].name.c_str());
    if(texture == NULL || texture->raster == NULL ||
       texture->raster->width != expectedWidth ||
       texture->raster->height != expectedHeight)
    {
        if(texture != NULL)
            texture->destroy();
        return NULL;
    }
    texture->setAddressU(rw::Texture::CLAMP);
    texture->setAddressV(rw::Texture::CLAMP);
    g_retainedFontTextures[page] = texture;
    return texture;
}

bool isPaletteMarker(const unsigned char* cursor)
{
    return (cursor[0] == '$' || cursor[0] == '@') &&
        cursor[1] >= '0' && cursor[1] <= '9';
}

bool isEncodedNewline(const unsigned char* cursor)
{
    return cursor[0] == '\\' && cursor[1] == 'n';
}

void drawRetainedTextPass(const char* text, float x, float y,
                          float scaleX, float scaleY,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                          bool shadowPass)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float screenScaleX =
        static_cast<float>(sk::globals.width) / VIRTUAL_SCREEN_WIDTH;
    const float screenScaleY =
        static_cast<float>(sk::globals.height) / VIRTUAL_SCREEN_HEIGHT;
    const float lineStart = origin.x + x * screenScaleX;
    float penX = lineStart;
    float penY = origin.y + y * screenScaleY;
    float linePitch = RETAINED_FONT_LINE_PITCH;
    bool highlighted = false;

    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        if(isPaletteMarker(cursor))
        {
            highlighted = cursor[1] == '8';
            cursor += 2;
            continue;
        }
        if(*cursor == '\n' || isEncodedNewline(cursor))
        {
            cursor += *cursor == '\n' ? 1 : 2;
            penX = lineStart;
            penY += linePitch * scaleY * screenScaleY;
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
        if(glyph.height > linePitch)
            linePitch = static_cast<float>(glyph.height);

        if(retainedFontCodepoint(codepoint) != ' ')
        {
            rw::Texture* texture = loadRetainedTexture(glyph.page);
            int atlasWidth;
            int atlasHeight;
            if(texture != NULL &&
               retainedFontPageSize(glyph.page, &atlasWidth, &atlasHeight))
            {
                const uint8_t drawR = shadowPass ? 0 :
                    (highlighted ? 0xff : r);
                const uint8_t drawG = shadowPass ? 0 :
                    (highlighted ? 0xff : g);
                const uint8_t drawB = shadowPass ? 0 :
                    (highlighted ? 0xff : b);
                const float top = penY +
                    glyph.yOffset * scaleY * screenScaleY;
                drawList->AddImage(
                    textureRef(texture),
                    ImVec2(penX, top),
                    ImVec2(penX + glyph.width * scaleX * screenScaleX,
                           top + glyph.height * scaleY * screenScaleY),
                    ImVec2(static_cast<float>(glyph.x) / atlasWidth,
                           static_cast<float>(glyph.y) / atlasHeight),
                    ImVec2(static_cast<float>(glyph.x + glyph.width) /
                               atlasWidth,
                           static_cast<float>(glyph.y + glyph.height) /
                               atlasHeight),
                    IM_COL32(drawR, drawG, drawB, a));
            }
        }
        penX += retainedFontGlyphAdvance(glyph) *
            scaleX * screenScaleX;
    }
}

MenuTextExtent measureRetainedTextInternal(const char* text,
                                           float scaleX, float scaleY)
{
    const RetainedFontTextExtent retained =
        retainedFontTextExtent(text, scaleX, scaleY);
    MenuTextExtent extent = { retained.width, retained.height };
    return extent;
}

}

bool
loadMenuFont(void)
{
    if(g_menuFontTexture != NULL)
        return true;

    std::vector<XafEntryInfo> entries;
    if(!listXafEntries(GRAPH_ARCHIVE_PATH, entries) ||
       MENU_FONT_ATLAS_ENTRY < 0 ||
       static_cast<size_t>(MENU_FONT_ATLAS_ENTRY) >= entries.size())
        return false;

    const XafEntryInfo& entry = entries[MENU_FONT_ATLAS_ENTRY];
    if(entry.name != MENU_FONT_ATLAS_NAME)
        return false;
    rw::Texture* texture = loadStxTexture(GRAPH_ARCHIVE_PATH,
                                          entry.name.c_str());
    if(texture == NULL || texture->raster == NULL ||
       texture->raster->width != static_cast<int32_t>(MENU_ATLAS_WIDTH) ||
       texture->raster->height != 512)
    {
        if(texture != NULL)
            texture->destroy();
        return false;
    }
    texture->setAddressU(rw::Texture::CLAMP);
    texture->setAddressV(rw::Texture::CLAMP);
    g_menuFontTexture = texture;
    return true;
}

bool
loadRetainedMenuFont(void)
{

    return loadRetainedTexture(RETAINED_FONT_PAGE_BASIC) != NULL;
}

void
drawMenuText(const char* text, float x, float y,
             float scaleX, float scaleY,
             uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool shadow)
{
    if(text == NULL || text[0] == 0 || !(scaleX > 0.0f) ||
       !(scaleY > 0.0f) || a == 0 || ImGui::GetCurrentContext() == NULL ||
       !loadMenuFont())
        return;

    if(shadow)
    {

        drawTextPass(text, x + SPRITE_TEXT_SHADOW_X,
                     y + SPRITE_TEXT_SHADOW_Y, scaleX, scaleY, 0, 0, 0, a);
    }
    drawTextPass(text, x, y, scaleX, scaleY, r, g, b, a);
}

void
drawMenuText(const char* text, float x, float y, float scale,
             uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool shadow)
{
    drawMenuText(text, x, y, scale, scale, r, g, b, a, shadow);
}

void
drawRetainedMenuText(const char* text, float x, float y,
                     float scaleX, float scaleY,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool shadow)
{
    if(text == NULL || text[0] == 0 || !(scaleX > 0.0f) ||
       !(scaleY > 0.0f) || a == 0 || ImGui::GetCurrentContext() == NULL ||
       !loadRetainedMenuFont())
        return;

    if(shadow)
    {
        drawRetainedTextPass(text, x + SPRITE_TEXT_SHADOW_X,
                             y + SPRITE_TEXT_SHADOW_Y,
                             scaleX, scaleY, r, g, b, a, true);
    }
    drawRetainedTextPass(text, x, y, scaleX, scaleY,
                         r, g, b, a, false);
}

void
drawRetainedMenuText(const char* text, float x, float y, float scale,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool shadow)
{
    drawRetainedMenuText(text, x, y, scale, scale,
                         r, g, b, a, shadow);
}

MenuTextExtent
measureMenuText(const char* text, float scaleX, float scaleY)
{
    MenuTextExtent extent = { 0.0f, 0.0f };
    if(text == NULL || text[0] == 0 || !(scaleX > 0.0f) ||
       !(scaleY > 0.0f))
        return extent;

    float widest = 0.0f;
    float line = 0.0f;
    int lineCount = 1;
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        const uint32_t codepoint = retainedFontNextCodepoint(cursor);
        if(codepoint == '\n')
        {
            if(line > widest)
                widest = line;
            line = 0.0f;
            ++lineCount;
        }
        else
            line += codepointAdvance(codepoint);
    }
    if(line > widest)
        widest = line;
    extent.width = widest * scaleX;
    extent.height = MENU_CELL_HEIGHT * scaleY * lineCount;
    return extent;
}

float
measureMenuText(const char* text, float scale)
{
    return measureMenuText(text, scale, scale).width;
}

MenuTextExtent
measureRetainedMenuText(const char* text, float scaleX, float scaleY)
{
    MenuTextExtent extent = { 0.0f, 0.0f };
    if(text == NULL || text[0] == 0 || !(scaleX > 0.0f) ||
       !(scaleY > 0.0f))
        return extent;
    return measureRetainedTextInternal(text, scaleX, scaleY);
}

float
measureRetainedMenuText(const char* text, float scale)
{
    return measureRetainedMenuText(text, scale, scale).width;
}

#include "menu_system.h"

#include "app_ui.h"
#include "game.h"
#include "menu_font.h"
#include "hud_prompt.h"
#include "credits.h"
#include "localized_graph.h"

#include "../formats/CHARACTER_DB/character_records.h"
#include "../formats/MENU_LAYOUT/menu_layout.h"
#include "../formats/MENU_LAYOUT/title_select_artwork.h"
#include "../formats/XAF/xaf_runtime.h"

#include <rw.h>
#include <skeleton.h>

#include <math.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

namespace
{

const float SCREEN_WIDTH = 640.0f;
const uint8_t TEXT_RED = 236;
const uint8_t TEXT_GREEN = 232;
const uint8_t TEXT_BLUE = 218;
const uint8_t SELECTED_RED = 240;
const uint8_t SELECTED_GREEN = 199;
const uint8_t SELECTED_BLUE = 56;
const uint8_t DISABLED_LEVEL = 96;

const char* GRAPH_ARCHIVE_PATH = "raw/XAF/GRAPH.XAF";
const float FRONTEND_HEIGHT = 480.0f;
const float ART_FONT_CELL = 32.0f;
const float ART_FONT_METRIC_SCALE = 0.5f;

struct ArtworkPackage
{
    ArtworkPackage(void)
    : entryIndex(UINT16_MAX), loadAttempted(false)
    {
    }

    uint16_t entryIndex;
    bool loadAttempted;
    std::vector<rw::Texture*> images;
};

struct InlineTextGlyph
{
    int cell;
    bool special;
    bool highlighted;
};

ArtworkPackage g_titleLogo;
ArtworkPackage g_titleBrand;
ArtworkPackage g_characterParts;
ArtworkPackage g_characterSharedParts;
ArtworkPackage g_characterFont;
ArtworkPackage g_characterPortraits[TITLE_SELECT_CHARACTER_COUNT];
ArtworkPackage g_selectModeParts;
ArtworkPackage g_selectModeFont;
ArtworkPackage g_menuAll;
ArtworkPackage g_infoTextures[LOCALIZED_GRAPH_INFO_FAMILY_COUNT];
ArtworkPackage g_memoryCardFont;
ArtworkPackage g_memoryCardParts;
ArtworkPackage g_creditLogos[CREDITS_LOGO_COUNT];

MenuPulse g_titlePulse = { MENU_PULSE_STEP * 2, MENU_PULSE_STEP };
float g_titlePulseFrames;
int g_selectModeFrame;
float g_selectModeFrames;

const char* presentationText(uint16_t textId, const char* hostLiteral)
{
    const char* text = g_game.localizedText(textId);
    return text != NULL ? text : hostLiteral;
}

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

rw::Raster* readArtworkRaster(const uint8_t* chunk, uint32_t length)
{
    if(length < 28 || read32(chunk) != rw::ID_STRUCT ||
       read32(chunk + 4) != 16)
        return NULL;

    const uint32_t width = read32(chunk + 12);
    const uint32_t height = read32(chunk + 16);
    const uint32_t depth = read32(chunk + 20);
    const uint32_t pitch = read32(chunk + 24);
    if(width == 0 || height == 0 || width > 4096 || height > 4096 ||
       (depth != 4 && depth != 8 && depth != 16 &&
        depth != 24 && depth != 32))
        return NULL;

    const uint64_t pixelSize = static_cast<uint64_t>(pitch) * height;
    const uint32_t paletteSize = depth == 4 ? 64 : depth == 8 ? 1024 : 0;
    if(28 + pixelSize + paletteSize != length)
        return NULL;

    rw::Image* image = rw::Image::create(static_cast<int32_t>(width),
                                         static_cast<int32_t>(height),
                                         static_cast<int32_t>(depth));
    if(image == NULL)
        return NULL;
    image->allocate();
    if(image->pixels == NULL || (paletteSize != 0 && image->palette == NULL) ||
       static_cast<uint32_t>(image->stride) != pitch)
    {
        image->destroy();
        return NULL;
    }

    memcpy(image->pixels, chunk + 28, static_cast<size_t>(pixelSize));
    if(paletteSize != 0)
    {
        memcpy(image->palette, chunk + 28 + pixelSize, paletteSize);
        const uint32_t colourCount = depth == 4 ? 16 : 256;
        for(uint32_t i = 0; i < colourCount; ++i)
        {
            const uint32_t alpha =
                static_cast<uint32_t>(image->palette[i * 4 + 3]) * 2;
            image->palette[i * 4 + 3] =
                static_cast<uint8_t>(alpha > 255 ? 255 : alpha);
        }
    }
    image->convertTo32();
    rw::Raster* raster = rw::Raster::createFromImage(image);
    image->destroy();
    return raster;
}

rw::Texture* createArtworkTexture(const uint8_t* bytes, size_t size)
{
    if(bytes == NULL || size < 12 || read32(bytes) != rw::ID_IMAGE)
        return NULL;
    const uint32_t length = read32(bytes + 4);
    if(static_cast<uint64_t>(length) + 12 != size)
        return NULL;
    rw::Raster* raster = readArtworkRaster(bytes + 12, length);
    if(raster == NULL)
        return NULL;
    rw::Texture* texture = rw::Texture::create(raster);
    if(texture == NULL)
    {
        raster->destroy();
        return NULL;
    }
    texture->setAddressU(rw::Texture::CLAMP);
    texture->setAddressV(rw::Texture::CLAMP);
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}

void clearArtworkPackage(ArtworkPackage& package)
{
    for(size_t i = 0; i < package.images.size(); ++i)
        package.images[i]->destroy();
    package.images.clear();
    package.entryIndex = UINT16_MAX;
    package.loadAttempted = false;
}

bool loadArtworkPackage(ArtworkPackage& package, uint16_t entryIndex)
{
    if(package.entryIndex != entryIndex)
        clearArtworkPackage(package);
    package.entryIndex = entryIndex;
    if(!package.images.empty())
        return true;
    if(package.loadAttempted)
        return false;
    package.loadAttempted = true;

    std::vector<uint8_t> data;

    if(!readXafEntryByIndex(GRAPH_ARCHIVE_PATH, entryIndex, data))
        return false;

    size_t offset = 0;
    while(offset + 12 <= data.size())
    {
        const uint32_t type = read32(data.data() + offset);
        const uint32_t length = read32(data.data() + offset + 4);
        if(static_cast<uint64_t>(offset) + 12 + length > data.size())
            break;
        if(type == rw::ID_IMAGE)
        {
            rw::Raster* raster =
                readArtworkRaster(data.data() + offset + 12, length);
            if(raster == NULL)
            {
                clearArtworkPackage(package);
                package.entryIndex = entryIndex;
                package.loadAttempted = true;
                return false;
            }
            rw::Texture* texture = rw::Texture::create(raster);
            if(texture == NULL)
            {
                raster->destroy();
                clearArtworkPackage(package);
                package.entryIndex = entryIndex;
                package.loadAttempted = true;
                return false;
            }
            texture->setAddressU(rw::Texture::CLAMP);
            texture->setAddressV(rw::Texture::CLAMP);
            texture->setFilter(rw::Texture::LINEAR);
            package.images.push_back(texture);
        }
        offset += 12 + length;
    }
    return !package.images.empty();
}

bool loadMenuAllPackage(uint8_t language)
{
    const uint16_t entryIndex = localizedMenuAllGraphIndex(language);
    if(g_menuAll.entryIndex != entryIndex)
        clearArtworkPackage(g_menuAll);
    g_menuAll.entryIndex = entryIndex;
    if(g_menuAll.images.size() == LOCALIZED_GRAPH_MENU_MEMBER_COUNT)
        return true;
    if(g_menuAll.loadAttempted)
        return false;
    g_menuAll.loadAttempted = true;

    std::vector<uint8_t> data;
    if(!readXafEntryByIndex(GRAPH_ARCHIVE_PATH, entryIndex, data))
        return false;
    for(uint32_t member = 0;
        member < LOCALIZED_GRAPH_MENU_MEMBER_COUNT; ++member)
    {
        std::vector<uint8_t> image;
        rw::Texture* texture = NULL;
        if(extractLocalizedMenuMember(
               data.data(), data.size(), member, image))
            texture = createArtworkTexture(image.data(), image.size());
        if(texture == NULL)
        {
            clearArtworkPackage(g_menuAll);
            g_menuAll.entryIndex = entryIndex;
            g_menuAll.loadAttempted = true;
            return false;
        }
        g_menuAll.images.push_back(texture);
    }
    return true;
}

rw::Texture* artworkImage(ArtworkPackage& package, uint16_t entryIndex,
                          int imageIndex)
{
    if(imageIndex < 0 || !loadArtworkPackage(package, entryIndex) ||
       static_cast<size_t>(imageIndex) >= package.images.size())
        return NULL;
    return package.images[imageIndex];
}

ImTextureRef artworkTextureRef(rw::Texture* texture)
{
    return ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(texture)));
}

void drawArtworkQuad(rw::Texture* texture, float x, float y, float width,
                     float height, float u0, float v0, float u1, float v1,
                     uint8_t r = 0xff, uint8_t g = 0xff, uint8_t b = 0xff,
                     uint8_t a = 0xff)
{
    if(texture == NULL || texture->raster == NULL ||
       ImGui::GetCurrentContext() == NULL || width <= 0.0f || height <= 0.0f)
        return;
    const ImVec2 origin = ImGui::GetWindowPos();
    const float scaleX = static_cast<float>(sk::globals.width) / SCREEN_WIDTH;
    const float scaleY =
        static_cast<float>(sk::globals.height) / FRONTEND_HEIGHT;
    ImGui::GetWindowDrawList()->AddImage(
        artworkTextureRef(texture),
        ImVec2(origin.x + x * scaleX, origin.y + y * scaleY),
        ImVec2(origin.x + (x + width) * scaleX,
               origin.y + (y + height) * scaleY),
        ImVec2(u0, v0), ImVec2(u1, v1), IM_COL32(r, g, b, a));
}

void drawArtworkImage(ArtworkPackage& package, uint16_t entryIndex,
                      const TitleSelectSpriteLayout& layout,
                      uint8_t alpha = 0xff)
{
    drawArtworkQuad(artworkImage(package, entryIndex, layout.imageIndex),
                    layout.x, layout.y, layout.width, layout.height,
                    0.0f, 0.0f, 1.0f, 1.0f, 0xff, 0xff, 0xff, alpha);
}

uint32_t decodeCodepoint(const unsigned char*& cursor)
{
    const uint32_t first = *cursor++;
    if(first < 0x80)
        return first;
    if((first & 0xe0) == 0xc0 && (cursor[0] & 0xc0) == 0x80)
    {
        const uint32_t value = ((first & 0x1f) << 6) | (cursor[0] & 0x3f);
        ++cursor;
        return value;
    }
    if((first & 0xf0) == 0xe0 && (cursor[0] & 0xc0) == 0x80 &&
       (cursor[1] & 0xc0) == 0x80)
    {
        const uint32_t value = ((first & 0x0f) << 12) |
            ((cursor[0] & 0x3f) << 6) | (cursor[1] & 0x3f);
        cursor += 2;
        return value;
    }
    return '?';
}

void decodeText(const char* text, std::vector<uint32_t>& codepoints)
{
    codepoints.clear();
    if(text == NULL)
        return;
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
        codepoints.push_back(decodeCodepoint(cursor));
}

uint32_t foldCodepoint(uint32_t codepoint)
{
    if(codepoint >= 0xff01 && codepoint <= 0xff5e)
        return codepoint - 0xfee0;
    if(codepoint == 0x3000)
        return ' ';
    return codepoint;
}

float artGlyphAdvance(int cell, bool special, float scale)
{
    if(special)
        return ART_FONT_CELL * scale;
    if(cell < 0 || cell >= TITLE_SELECT_NAME_GLYPH_COUNT)
        return ART_FONT_CELL * 0.5f * scale;
    return TITLE_SELECT_NAME_GLYPHS[cell].advance *
        ART_FONT_METRIC_SCALE * scale;
}

void buildInlineGlyphs(const char* text, std::vector<InlineTextGlyph>& glyphs)
{
    std::vector<uint32_t> codepoints;
    decodeText(text, codepoints);
    glyphs.clear();
    bool highlighted = false;
    for(size_t i = 0; i < codepoints.size(); ++i)
    {
        uint32_t codepoint = foldCodepoint(codepoints[i]);
        if(codepoint == '$' && i + 1 < codepoints.size() &&
           (codepoints[i + 1] == '8' || codepoints[i + 1] == '0'))
        {
            highlighted = codepoints[++i] == '8';
            continue;
        }
        if((codepoint == 'L' || codepoint == 'R') &&
           i + 1 < codepoints.size() &&
           foldCodepoint(codepoints[i + 1]) == '2')
        {
            InlineTextGlyph glyph =
            {
                codepoint == 'L' ? 7 : 10, true, true
            };
            glyphs.push_back(glyph);
            ++i;
            continue;
        }
        int special = -1;
        if(codepoint == 0x2461)
            special = 7;
        else if(codepoint == 0x2464)
            special = 10;
        else if(codepoint == 0x25ce)
            special = 5;
        InlineTextGlyph glyph;
        glyph.special = special >= 0;
        glyph.highlighted = highlighted || glyph.special;
        glyph.cell = glyph.special ? special :
            (codepoint >= 0x20 && codepoint < 0x80 ?
                static_cast<int>(codepoint - 0x20) :
                static_cast<int>('?' - 0x20));
        glyphs.push_back(glyph);
    }
}

float measureArtworkText(const char* text, float scale)
{
    std::vector<InlineTextGlyph> glyphs;
    buildInlineGlyphs(text, glyphs);
    float width = 0.0f;
    for(size_t i = 0; i < glyphs.size(); ++i)
        width += artGlyphAdvance(glyphs[i].cell, glyphs[i].special, scale);
    return width;
}

void drawPackageText(ArtworkPackage& package, uint16_t entryIndex,
                     const char* text, float x, float y, float scale,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     bool centered)
{
    if(!loadArtworkPackage(package, entryIndex))
        return;
    std::vector<InlineTextGlyph> glyphs;
    buildInlineGlyphs(text, glyphs);
    if(centered)
        x -= measureArtworkText(text, scale) * 0.5f;
    for(size_t i = 0; i < glyphs.size(); ++i)
    {
        const InlineTextGlyph& glyph = glyphs[i];
        const bool characterAtlas =
            entryIndex == CHARACTER_SELECT_FONT_ARCHIVE_INDEX &&
            package.images.size() >= 3;
        const int image = characterAtlas ? (glyph.special ? 1 : 2) : 0;
        if(static_cast<size_t>(image) < package.images.size())
        {
            const int column = glyph.cell & 0x0f;
            const int row = glyph.cell >> 4;
            const float rowCount = characterAtlas ? 8.0f : 16.0f;
            const float u0 = column / 16.0f;
            const float v0 = row / rowCount;
            const float u1 = (column + 1) / 16.0f;
            const float v1 = (row + 1) / rowCount;
            const float bearing = glyph.special ? 0.0f :
                TITLE_SELECT_NAME_GLYPHS[glyph.cell].bearing *
                    ART_FONT_METRIC_SCALE * scale;
            drawArtworkQuad(package.images[image], x + bearing, y,
                            ART_FONT_CELL * scale, ART_FONT_CELL * scale,
                            u0, v0, u1, v1,
                            glyph.highlighted ? 0xff : r,
                            glyph.highlighted ? 0xff : g,
                            glyph.highlighted ? 0xff : b, a);
        }
        x += artGlyphAdvance(glyph.cell, glyph.special, scale);
    }
}

void drawSelectModeText(const char* text, float x, float y, float scale,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                        bool centered)
{
    rw::Texture* texture = artworkImage(
        g_selectModeFont, SELECT_MODE_FONT_ARCHIVE_INDEX, 0);
    if(texture == NULL || text == NULL)
        return;

    std::vector<uint32_t> codepoints;
    decodeText(text, codepoints);
    const MenuTextLayout layout = menuTextLayoutDefaults();
    float width = 0.0f;
    for(size_t i = 0; i < codepoints.size(); ++i)
    {
        const uint32_t folded = foldCodepoint(codepoints[i]);
        const unsigned char character = folded <= 0xff ?
            static_cast<unsigned char>(folded) :
            static_cast<unsigned char>('?');
        width += menuGlyphAdvance(character, layout) * scale;
    }
    if(centered)
        x -= width * 0.5f;

    for(size_t i = 0; i < codepoints.size(); ++i)
    {
        const uint32_t folded = foldCodepoint(codepoints[i]);
        const unsigned char character = folded <= 0xff ?
            static_cast<unsigned char>(folded) :
            static_cast<unsigned char>('?');
        const float advance = menuGlyphAdvance(character, layout);
        const int glyphIndex =
            static_cast<int>(character) - MENU_GLYPH_FIRST;
        if(character > ' ' && glyphIndex >= 0 &&
           glyphIndex < MENU_GLYPH_COUNT)
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
                drawArtworkQuad(
                    texture, x, y, metrics.advance * scale,
                    MENU_CELL_HEIGHT * scale,
                    sourceLeft / MENU_ATLAS_WIDTH,
                    sourceTop / MENU_ATLAS_HEIGHT,
                    sourceRight / MENU_ATLAS_WIDTH,
                    sourceBottom / MENU_ATLAS_HEIGHT,
                    r, g, b, a);
            }
        }
        x += advance * scale;
    }
}

void drawArtworkText(const char* text, float x, float y, float scale,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                     bool centered = false)
{
    drawPackageText(g_characterFont, CHARACTER_SELECT_FONT_ARCHIVE_INDEX,
                    text, x, y, scale, r, g, b, a, centered);
}

void drawCharacterNameText(const char* text, float x, float y,
                           float scaleX, float scaleY, uint8_t alpha)
{
    rw::Texture* texture = artworkImage(
        g_characterFont, CHARACTER_SELECT_FONT_ARCHIVE_INDEX,
        CHARACTER_SELECT_NAME_ATLAS_IMAGE);
    if(texture == NULL || text == NULL)
        return;

    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        const uint32_t codepoint = foldCodepoint(decodeCodepoint(cursor));
        const int glyphIndex =
            codepoint >= 0x20 && codepoint < 0x7f ?
                static_cast<int>(codepoint - 0x20) :
                static_cast<int>('?' - 0x20);
        if(glyphIndex < 0 || glyphIndex >= TITLE_SELECT_NAME_GLYPH_COUNT)
            continue;

        const TitleSelectGlyphMetric& metric =
            TITLE_SELECT_NAME_GLYPHS[glyphIndex];
        if(codepoint != ' ')
        {

            const int column =
                glyphIndex % CHARACTER_SELECT_NAME_ATLAS_COLUMNS;
            const int row =
                glyphIndex / CHARACTER_SELECT_NAME_ATLAS_COLUMNS;
            const float sourceLeft =
                column * CHARACTER_SELECT_NAME_CELL_WIDTH + metric.bearing;
            const float sourceTop =
                row * CHARACTER_SELECT_NAME_CELL_HEIGHT;
            const float sourceRight = sourceLeft + metric.advance;
            const float sourceBottom =
                sourceTop + CHARACTER_SELECT_NAME_CELL_HEIGHT;
            drawArtworkQuad(
                texture, x, y, metric.advance * scaleX,
                CHARACTER_SELECT_NAME_CELL_HEIGHT * scaleY,
                sourceLeft / CHARACTER_SELECT_NAME_ATLAS_WIDTH,
                sourceTop / CHARACTER_SELECT_NAME_ATLAS_HEIGHT,
                sourceRight / CHARACTER_SELECT_NAME_ATLAS_WIDTH,
                sourceBottom / CHARACTER_SELECT_NAME_ATLAS_HEIGHT,
                0xff, 0xff, 0xff, alpha);
        }
        x += metric.advance * scaleX;
    }
}

void drawCharacterName(
    const char* name, const StoryCharacterSelectPresentation& presentation)
{
    const float y = CHARACTER_SELECT_ARROW_Y;
    const uint8_t alpha = presentation.primaryAlpha;
    rw::Texture* orangeFont = artworkImage(
        g_characterFont, CHARACTER_SELECT_FONT_ARCHIVE_INDEX, 1);
    drawArtworkQuad(orangeFont, presentation.leftArrowX + 147.0f, y,
                    CHARACTER_SELECT_ARROW_SIZE,
                    CHARACTER_SELECT_ARROW_SIZE,
                    1.0f / 16.0f, 0.0f, 2.0f / 16.0f, 1.0f / 8.0f,
                    0xff, 0xff, 0xff, alpha);

    drawCharacterNameText(name, CHARACTER_SELECT_NAME_X, y,
                          CHARACTER_SELECT_NAME_SCALE_X,
                          CHARACTER_SELECT_NAME_SCALE_Y, alpha);

    drawArtworkQuad(orangeFont, presentation.rightArrowX + 147.0f, y,
                    CHARACTER_SELECT_ARROW_SIZE,
                    CHARACTER_SELECT_ARROW_SIZE,
                    2.0f / 16.0f, 0.0f, 3.0f / 16.0f, 1.0f / 8.0f,
                    0xff, 0xff, 0xff, alpha);
}

void replaceHighlightedText(const char* source,
                            const std::vector<std::string>& replacements,
                            std::string& output, bool discardTail)
{
    output.clear();
    if(source == NULL)
        return;
    const char* cursor = source;
    size_t replacement = 0;
    while(*cursor != 0)
    {
        const char* begin = strstr(cursor, "$8");
        if(begin == NULL || replacement >= replacements.size())
        {
            if(!discardTail)
                output.append(cursor);
            break;
        }
        output.append(cursor, static_cast<size_t>(begin - cursor));
        output.append("$8");
        output.append(replacements[replacement++]);
        const char* end = strstr(begin + 2, "$0");
        if(end == NULL)
            break;
        output.append("$0");
        cursor = end + 2;
        if(discardTail && replacement == replacements.size())
            break;
    }
}

int clampCharacterSelection(int selection)
{
    if(selection < 0)
        return 0;
    if(selection >= TITLE_SELECT_CHARACTER_COUNT)
        return TITLE_SELECT_CHARACTER_COUNT - 1;
    return selection;
}

void drawCenteredTextAt(const char* text, float centerX, float y, float scale,
                        uint8_t r, uint8_t g, uint8_t b)
{
    const float x = centerX - measureMenuText(text, scale) * 0.5f;
    drawMenuText(text, x, y, scale, r, g, b, 255, true);
}

void drawCenteredText(const char* text, float y, float scale,
                      uint8_t r, uint8_t g, uint8_t b)
{
    drawCenteredTextAt(text, SCREEN_WIDTH * 0.5f, y, scale, r, g, b);
}

float fittedTextScale(const char* text, float maximumWidth)
{
    const float width = measureMenuText(text, 1.0f);
    return width > maximumWidth ? maximumWidth / width : 1.0f;
}

void makeCommandPrompt(char* output, size_t outputSize, bool backAvailable)
{
    const char* select = presentationText(GAME_TEXT_SELECT, "SELECT");
    if(backAvailable)
    {
        const char* back = presentationText(GAME_TEXT_BACK, "BACK");
        snprintf(output, outputSize,
                 "ENTER / A: %s    ESC / B: %s", select, back);
    }
    else
        snprintf(output, outputSize, "ENTER / A: %s", select);
}

struct VersusRowText
{
    uint16_t left;
    uint16_t separator;
    uint16_t right;
    const char* fallback;
};

const VersusRowText VERSUS_ROWS[VERSUS_BATTLE_TYPE_COUNT] =
{
    { 0x9a53, 0x9a52, 0x9a55, "PLAYER vs CPU" },
    { 0x9a53, 0x9a52, 0x9a54, "PLAYER vs PLAYER" },
    { 0x9a55, 0x9a52, 0x9a56, "CPU vs CPU" }
};

void composeVersusRow(int index, char* output, size_t outputSize)
{
    const VersusRowText& row = VERSUS_ROWS[index];
    const char* left = g_game.localizedText(row.left);
    const char* separator = g_game.localizedText(row.separator);
    const char* right = g_game.localizedText(row.right);
    if(left != NULL && separator != NULL && right != NULL)
        snprintf(output, outputSize, "%s %s %s", left, separator, right);
    else
        snprintf(output, outputSize, "%s", row.fallback);
}

}

void
updateMenuSystemPresentation(float deltaSeconds)
{
    const float frameRate = g_game.nominalFrameRateHz();
    if(deltaSeconds <= 0.0f || frameRate <= 0.0f)
        return;

    if(g_game.screen() == GAME_SCREEN_MAIN_MENU)
    {
        menuPulseAdvanceElapsed(
            g_titlePulse, g_titlePulseFrames, deltaSeconds * frameRate);
    }
    else
        g_titlePulseFrames = 0.0f;

    if(g_game.screen() == GAME_SCREEN_VERSUS_TYPE_SELECT)
    {
        g_selectModeFrames += deltaSeconds * frameRate;
        while(g_selectModeFrames >= 1.0f)
        {
            ++g_selectModeFrame;
            g_selectModeFrames -= 1.0f;
        }
    }
    else
        g_selectModeFrames = 0.0f;
}

TitleSelectColour
titleMenuSelectedColour(int pulse)
{
    if(pulse < 0)
        pulse = 0;
    if(pulse > 0xff)
        pulse = 0xff;
    const uint8_t redAndAlpha = static_cast<uint8_t>(
        pulse * TITLE_MENU_SELECTED_PULSE_SCALE / 0xff);
    const TitleSelectColour colour =
    {
        redAndAlpha, TITLE_MENU_SELECTED_GREEN,
        TITLE_MENU_SELECTED_BLUE, redAndAlpha
    };
    return colour;
}

uint8_t
selectModeHighlightLevel(int frame)
{
    const int fullCycle = SELECT_MODE_SELECTED_HALF_CYCLE_FRAMES * 2;
    int phase = frame % fullCycle;
    if(phase < 0)
        phase += fullCycle;
    if(phase > SELECT_MODE_SELECTED_HALF_CYCLE_FRAMES)
        phase = fullCycle - phase;
    return static_cast<uint8_t>(
        SELECT_MODE_SELECTED_DARK +
        (SELECT_MODE_SELECTED_LIGHT - SELECT_MODE_SELECTED_DARK) * phase /
            SELECT_MODE_SELECTED_HALF_CYCLE_FRAMES);
}

void
drawTitleArtwork(float scale, uint8_t alpha)
{
    if(scale <= 0.0f || alpha == 0)
        return;

    const LocalizedTitleGraphPair pair =
        localizedTitleGraphPair(g_game.options().language);

    rw::Texture* logo = artworkImage(g_titleLogo, pair.title, 0);
    const float width = TITLE_LOGO_WIDTH * scale;
    const float height = TITLE_LOGO_HEIGHT * scale;
    drawArtworkQuad(logo, TITLE_LOGO_CENTER_X - width * 0.5f,
                    TITLE_LOGO_CENTER_Y - height * 0.5f,
                    width, height, 0.0f, 0.0f, 1.0f, 1.0f,
                    0xff, 0xff, 0xff, alpha);

    rw::Texture* brand = artworkImage(g_titleBrand, pair.brand, 0);
    drawArtworkQuad(
        brand, TITLE_BRAND_CENTER_X - TITLE_BRAND_WIDTH * 0.5f,
        TITLE_BRAND_CENTER_Y - TITLE_BRAND_HEIGHT * 0.5f,
        TITLE_BRAND_WIDTH, TITLE_BRAND_HEIGHT,
        0.0f, 0.0f, 1.0f, 1.0f, 0xff, 0xff, 0xff, alpha);
}

void
drawTitleMenuScreen(void)
{
    drawTitleArtwork(1.0f, 0xff);

    loadMenuAllPackage(g_game.options().language);
    if(!loadMenuFont())
        return;

    static const int itemIndices[5] =
    {
        MENU_ITEM_NEW_GAME, MENU_ITEM_LOAD_GAME, MENU_ITEM_VS_NORMAL,
        MENU_ITEM_VS_CUSTOM, MENU_ITEM_OPTIONS
    };
    static const uint16_t fallbackIds[5] =
    {
        GAME_TEXT_NEW_GAME, GAME_TEXT_LOAD_GAME,
        GAME_TEXT_VS_MODE_NORMAL, GAME_TEXT_VS_MODE_CUSTOM,
        GAME_TEXT_TITLE_OPTIONS
    };
    static const char* fallbacks[5] =
    {
        "NEW GAME", "LOAD GAME", "VS MODE NORMAL",
        "VS MODE CUSTOM", "OPTIONS"
    };

    for(int i = 0; i < 5; ++i)
    {
        const MenuItemLayout& item = MENU_ITEM_LAYOUT[itemIndices[i]];
        const char* text = item.textId != 0 ?
            presentationText(item.textId, fallbacks[i]) :
            presentationText(fallbackIds[i], fallbacks[i]);
        TitleSelectColour colour = TITLE_MENU_ENABLED_COLOUR;
        if(!g_game.mainMenuItemEnabled(i))
            colour = TITLE_MENU_DISABLED_COLOUR;
        else if(i == g_game.mainMenuSelection())
            colour = titleMenuSelectedColour(g_titlePulse.value);
        const float x =
            item.x - measureMenuText(text, item.scale) * 0.5f;
        const float y =
            item.y - MENU_CELL_HEIGHT * item.scale * 0.5f;
        drawMenuText(text, x, y, item.scale, colour.r, colour.g, colour.b,
                     colour.a, true);
    }
}

void
drawCharacterSelectScreen(void)
{
    const int selection =
        clampCharacterSelection(g_game.characterSelection());
    const StoryCharacterSelectPresentation& presentation =
        g_game.storyCharacterSelectPresentation();
    const LocalizedStorySelectGraphSet resources =
        localizedStorySelectGraphSet(g_game.options().language);
    const uint16_t partsIndex = resources.localizedParts;

    drawArtworkImage(g_characterParts, partsIndex,
                     CHARACTER_SELECT_BACKGROUND, presentation.primaryAlpha);
    drawArtworkImage(g_characterParts, partsIndex,
                     CHARACTER_SELECT_NAME_PLATE, presentation.primaryAlpha);
    drawArtworkImage(g_characterSharedParts, resources.commonParts,
                     CHARACTER_SELECT_POLAROID, presentation.primaryAlpha);

    const TitleSelectCharacterLayout& selected =
        TITLE_SELECT_CHARACTERS[selection];

    drawArtworkImage(g_characterPortraits[selection],
                     resources.portraits[selection], selected.small,
                     presentation.primaryAlpha);

    if(presentation.outgoingSelection >= 0 &&
       presentation.outgoingSelection < TITLE_SELECT_CHARACTER_COUNT &&
       presentation.outgoingPortraitAlpha != 0)
    {
        const int outgoing = presentation.outgoingSelection;
        const TitleSelectSpriteLayout& layout =
            TITLE_SELECT_CHARACTERS[outgoing].portrait;
        drawArtworkQuad(
            artworkImage(g_characterPortraits[outgoing],
                         resources.portraits[outgoing], layout.imageIndex),
            presentation.outgoingPortraitX, layout.y,
            layout.width, layout.height, 0.0f, 0.0f, 1.0f, 1.0f,
            0xff, 0xff, 0xff, presentation.outgoingPortraitAlpha);
    }

    const TitleSelectSpriteLayout& portrait = selected.portrait;
    drawArtworkQuad(
        artworkImage(g_characterPortraits[selection],
                     resources.portraits[selection], portrait.imageIndex),
        presentation.incomingPortraitX, portrait.y,
        portrait.width * presentation.portraitScale,
        portrait.height * presentation.portraitScale,
        0.0f, 0.0f, 1.0f, 1.0f,
        0xff, 0xff, 0xff, presentation.portraitAlpha);

    drawArtworkImage(g_characterParts, partsIndex,
                     CHARACTER_SELECT_TITLE, presentation.primaryAlpha);

    const char* name = presentationText(
        selected.nameTextId, Game::characterName(selection));
    drawCharacterName(name, presentation);

    const CharacterRecord* defaults =
        characterRecordForId(selected.persistentDefaultId);
    int level = defaults != NULL ? defaults->level : 1;
    int health = defaults != NULL ? defaults->maxHealth : 0;
    uint32_t cash = defaults != NULL ?
        static_cast<uint32_t>(defaults->cash) : 0;
    const PersistentGameRecord* persistent =
        g_game.characterRecord(selected.canonicalCharacterId);
    if(persistent != NULL)
    {
        if(persistent->level > 0)
            level = persistent->level;
        if(persistent->maxHealth > 0)
            health = persistent->maxHealth;
        cash = persistent->cash < 0 ? 0 :
            static_cast<uint32_t>(persistent->cash);
    }

    char levelText[16];
    char healthText[16];
    char cashText[24];
    snprintf(levelText, sizeof(levelText), "%d", level);
    snprintf(healthText, sizeof(healthText), "%d", health);
    snprintf(cashText, sizeof(cashText), "$ %u", cash);
    std::vector<std::string> replacements;
    replacements.push_back(levelText);
    replacements.push_back(healthText);
    replacements.push_back(cashText);
    std::string line;
    const char* primaryTemplate = presentationText(
        CHARACTER_SELECT_STAT_TEMPLATE_IDS[0], "");
    if(strstr(primaryTemplate, "$8") != NULL)
        replaceHighlightedText(primaryTemplate, replacements, line, false);
    else
    {
        char fallback[160];
        snprintf(fallback, sizeof(fallback),
                 "Level $8%s$0/Life $8%s$0/Cash on hand $8%s$0",
                 levelText, healthText, cashText);
        line = fallback;
    }
    drawRetainedMenuText(line.c_str(), presentation.statsX,
                         CHARACTER_SELECT_STATS_Y[0], 1.0f,
                         CHARACTER_SELECT_STATS_COLOUR.r,
                         CHARACTER_SELECT_STATS_COLOUR.g,
                         CHARACTER_SELECT_STATS_COLOUR.b,
                         static_cast<uint8_t>(
                             CHARACTER_SELECT_STATS_COLOUR.a *
                             presentation.primaryAlpha / 0xff), false);

    const char* leftItem = presentationText(
        selected.leftItemTextId, "B knuckles");
    const char* rightItem = presentationText(
        selected.rightItemTextId, "Chicken");
    replacements.clear();
    replacements.push_back(leftItem);
    replacements.push_back(rightItem);
    const char* itemTemplate = presentationText(
        CHARACTER_SELECT_STAT_TEMPLATE_IDS[1], "");
    if(strstr(itemTemplate, "$8") != NULL)
        replaceHighlightedText(itemTemplate, replacements, line, false);
    else
    {
        line = "\xe2\x91\xa1 item: $8";
        line += leftItem;
        line += "$0 / \xe2\x91\xa4 item: $8";
        line += rightItem;
        line += "$0";
    }
    drawRetainedMenuText(line.c_str(), presentation.statsX,
                         CHARACTER_SELECT_STATS_Y[1], 1.0f,
                         CHARACTER_SELECT_STATS_COLOUR.r,
                         CHARACTER_SELECT_STATS_COLOUR.g,
                         CHARACTER_SELECT_STATS_COLOUR.b,
                         static_cast<uint8_t>(
                             CHARACTER_SELECT_STATS_COLOUR.a *
                             presentation.primaryAlpha / 0xff), false);

    const char* specialty = presentationText(
        selected.specialtyTextId, "");
    replacements.clear();
    replacements.push_back(specialty);
    const char* specialtyTemplate = presentationText(
        CHARACTER_SELECT_STAT_TEMPLATE_IDS[2], "");
    if(strstr(specialtyTemplate, "$8") != NULL)
        replaceHighlightedText(specialtyTemplate, replacements, line, true);
    else
    {
        line = "Specialty: $8";
        line += specialty;
        line += "$0";
    }
    line += " ";
    line += presentationText(selected.specialtyGlyphTextId, "");
    drawRetainedMenuText(line.c_str(), presentation.statsX,
                         CHARACTER_SELECT_STATS_Y[2], 1.0f,
                         CHARACTER_SELECT_STATS_COLOUR.r,
                         CHARACTER_SELECT_STATS_COLOUR.g,
                         CHARACTER_SELECT_STATS_COLOUR.b,
                         static_cast<uint8_t>(
                             CHARACTER_SELECT_STATS_COLOUR.a *
                             presentation.primaryAlpha / 0xff), false);

    if(presentation.primaryAlpha != 0)
    {

        hudPromptClear();
        hudPromptAddTextId(
            63.0f, 452.0f, HUD_PROMPT_DPAD_VERTICAL, 0x28a2);
        hudPromptAddTextId(
            195.0f, 452.0f, HUD_PROMPT_CONFIRM, 0x28a3);
        hudPromptAddTextId(
            459.0f, 452.0f, HUD_PROMPT_CANCEL, 0xcb87);
        hudPromptSetGate(0, 1);
        hudPromptDraw();
    }
}

void
releaseMenuSystemArtwork(void)
{
    clearArtworkPackage(g_titleLogo);
    clearArtworkPackage(g_titleBrand);
    clearArtworkPackage(g_characterParts);
    clearArtworkPackage(g_characterSharedParts);
    clearArtworkPackage(g_characterFont);
    for(int i = 0; i < TITLE_SELECT_CHARACTER_COUNT; ++i)
        clearArtworkPackage(g_characterPortraits[i]);
    clearArtworkPackage(g_selectModeParts);
    clearArtworkPackage(g_selectModeFont);
    clearArtworkPackage(g_menuAll);
    for(int family = 0; family < LOCALIZED_GRAPH_INFO_FAMILY_COUNT; ++family)
        clearArtworkPackage(g_infoTextures[family]);
    clearArtworkPackage(g_memoryCardFont);
    clearArtworkPackage(g_memoryCardParts);
    for(int index = 0; index < CREDITS_LOGO_COUNT; ++index)
        clearArtworkPackage(g_creditLogos[index]);
}

bool
prepareLocalizedInfoFamily(uint8_t language, int family)
{
    if(family < 0 || family >= LOCALIZED_GRAPH_INFO_FAMILY_COUNT)
        return false;
    const uint16_t index =
        localizedInfoTextureGraphIndex(language, family);
    return loadArtworkPackage(g_infoTextures[family], index);
}

void
drawLocalizedInfoPresentation(const Game& game)
{
    if(game.screen() != GAME_SCREEN_GAMEPLAY ||
       game.options().gaugeDisplay == 0 ||
       !prepareLocalizedInfoFamily(game.options().language, 0) ||
       g_infoTextures[0].images.size() < 2)
        return;

    for(int side = 0; side < LOCALIZED_GRAPH_INFO_SIDE_COUNT; ++side)
    {
        const LocalizedInfoSpriteLayout* sprites =
            localizedInfoSpriteLayouts(side);
        for(int index = 0; index < LOCALIZED_GRAPH_INFO_SPRITE_COUNT; ++index)
        {
            const LocalizedInfoSpriteLayout& sprite = sprites[index];
            drawArtworkQuad(
                g_infoTextures[0].images[sprite.texture],
                sprite.x, sprite.y, sprite.sourceWidth, sprite.sourceHeight,
                sprite.sourceX / sprite.atlasWidth,
                sprite.sourceY / sprite.atlasHeight,
                (sprite.sourceX + sprite.sourceWidth) / sprite.atlasWidth,
                (sprite.sourceY + sprite.sourceHeight) / sprite.atlasHeight);
        }
    }
}

void
drawBattleEndNotice(const Game& game)
{
    const BattleEndNoticeRuntime& notice = game.battleEndNotice();
    if(notice.state == BATTLE_END_NOTICE_INACTIVE || notice.opacity <= 0 ||
       !prepareLocalizedInfoFamily(game.options().language, 0) ||
       g_infoTextures[0].images.empty())
        return;
    const BattleEndNoticeLayout& layout = BATTLE_END_NOTICE_LAYOUT;
    drawArtworkQuad(g_infoTextures[0].images[0],
        layout.centerX - layout.width * 0.5f,
        layout.centerY - layout.height * 0.5f, layout.width, layout.height,
        layout.u0, layout.v0, layout.u1, layout.v1,
        255, 255, 255, static_cast<uint8_t>(notice.opacity * 255.0f));
}

bool
drawMemoryCardArtwork(uint8_t language)
{
    const LocalizedMemoryCardGraphSet resources =
        localizedMemoryCardGraphSet(language);
    const bool fontLoaded =
        loadArtworkPackage(g_memoryCardFont, resources.font);
    const bool partsLoaded =
        loadArtworkPackage(g_memoryCardParts, resources.localizedParts);
    if(!fontLoaded || !partsLoaded || g_memoryCardParts.images.empty())
        return false;

    drawArtworkQuad(g_memoryCardParts.images[0], 0.0f, 16.0f,
                    640.0f, 480.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    return true;
}

void
releaseCreditsArtwork(void)
{
    for(int index = 0; index < CREDITS_LOGO_COUNT; ++index)
        clearArtworkPackage(g_creditLogos[index]);
}

void
drawCreditsSequenceScreen(const CreditsSequenceState& state)
{
    if(state.phase == CREDITS_PHASE_POST_EXIT_WAIT ||
       state.phase == CREDITS_PHASE_FINISHED)
    {
        releaseCreditsArtwork();
        return;
    }

    const uint16_t* requests = localizedCreditsGraphRequests();
    for(int index = 0; index < CREDITS_LOGO_COUNT; ++index)
        loadArtworkPackage(g_creditLogos[index], requests[index]);

    CreditsBackdropDraw backdrops[CREDITS_BACKDROP_COUNT];
    const int backdropCount = creditsBackdropDraws(
        state, backdrops, CREDITS_BACKDROP_COUNT);
    if(ImGui::GetCurrentContext() != NULL)
    {
        const ImVec2 origin = ImGui::GetWindowPos();
        const float scaleX =
            static_cast<float>(sk::globals.width) / SCREEN_WIDTH;
        const float scaleY =
            static_cast<float>(sk::globals.height) / FRONTEND_HEIGHT;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for(int index = 0; index < backdropCount; ++index)
        {
            const CreditsBackdropDraw& draw = backdrops[index];
            const ImVec2 minimum(
                origin.x + (draw.x - draw.width * 0.5f) * scaleX,
                origin.y + (draw.y - draw.height * 0.5f) * scaleY);
            const ImVec2 maximum(
                origin.x + (draw.x + draw.width * 0.5f) * scaleX,
                origin.y + (draw.y + draw.height * 0.5f) * scaleY);
            drawList->AddRectFilledMultiColor(
                minimum, maximum,
                IM_COL32(draw.red, draw.green, draw.blue,
                         draw.cornerAlpha[0]),
                IM_COL32(draw.red, draw.green, draw.blue,
                         draw.cornerAlpha[1]),
                IM_COL32(draw.red, draw.green, draw.blue,
                         draw.cornerAlpha[3]),
                IM_COL32(draw.red, draw.green, draw.blue,
                         draw.cornerAlpha[2]));
        }
    }

    if(state.phase == CREDITS_PHASE_NOTICE)
    {
        const char* text = g_game.localizedText(creditsNoticeTextId(state));
        if(text != NULL)
            drawCenteredTextAt(text, 320.0f, 240.0f, 1.0f,
                               0xc8, 0xc8, 0xc8);
    }

    CreditsTextDraw textDraws[CREDITS_TEXT_HANDLE_CAPACITY];
    const int textCount = creditsActiveText(
        state, textDraws, CREDITS_TEXT_HANDLE_CAPACITY);
    for(int index = 0; index < textCount; ++index)
    {
        const CreditsTextDraw& draw = textDraws[index];
        const char* text = g_game.localizedText(draw.textId);
        if(text == NULL)
            continue;
        const float x = draw.centered ?
            draw.x - measureMenuText(text, draw.scale) * 0.5f : draw.x;
        drawMenuText(text, x, draw.y, draw.scale,
                     draw.red, draw.green, draw.blue, draw.alpha, true);
    }
    CreditsTextDraw bottom;
    if(creditsBottomText(state, bottom))
    {
        const char* text = g_game.localizedText(bottom.textId);
        if(text != NULL)
        {
            const float x = bottom.centered ?
                bottom.x - measureMenuText(text, bottom.scale) * 0.5f :
                bottom.x;
            drawMenuText(text, x, bottom.y, bottom.scale,
                         bottom.red, bottom.green, bottom.blue,
                         bottom.alpha, true);
        }
    }

    const int logoIndex = creditsLogoGraphIndex(state);
    const CreditsLogoLayout* layout = creditsLogoLayout(state);
    if(logoIndex >= 0 && layout != NULL &&
       state.currentLogoPage < CREDITS_LOGO_COUNT)
    {
        rw::Texture* texture = artworkImage(
            g_creditLogos[state.currentLogoPage],
            static_cast<uint16_t>(logoIndex), 0);
        drawArtworkQuad(
            texture, 0.0f, 0.0f, layout->sourceWidth, layout->sourceHeight,
            layout->sourceX / layout->textureWidth,
            layout->sourceY / layout->textureHeight,
            (layout->sourceX + layout->sourceWidth) / layout->textureWidth,
            (layout->sourceY + layout->sourceHeight) / layout->textureHeight,
            0xff, 0xff, 0xff, creditsLogoAlpha(state));
    }
}

void
drawLoadingScreen(void)
{
    const char* text = presentationText(
        GAME_TEXT_NOW_LOADING, "NOW LOADING");
    if(!loadMenuFont())
    {
        uiCenterText(text);
        return;
    }

    static uint8_t pulseCounter;
    ++pulseCounter;
    const uint8_t phase = pulseCounter & 0x1f;
    const uint8_t level = phase < 0x10 ?
        static_cast<uint8_t>(phase << 4) :
        static_cast<uint8_t>(0xff - ((phase - 0x10) << 4));
    const float scale = 0.7f;
    const float x = 593.0f - measureMenuText(text, scale) * 0.5f;
    drawMenuText(text, x, 439.0f, scale, level, level, level, 255, true);
}

void
drawVideoModeScreen(void)
{
    static const char* modes[GAME_VIDEO_MODE_COUNT] =
    {
        "50 Hz", "60 Hz", "TEST 60 Hz"
    };
    char prompt[256];
    makeCommandPrompt(prompt, sizeof(prompt), false);

    if(!loadMenuFont())
    {
        uiCenterText("SELECT VIDEO MODE");
        for(int i = 0; i < GAME_VIDEO_MODE_COUNT; ++i)
        {
            uiDrawMenuItem(modes[i], i == g_game.videoModeSelection(), true);
        }
        uiDrawCommandPrompt(false);
        return;
    }

    static const float rowCenters[GAME_VIDEO_MODE_COUNT] =
    {
        178.0f, 220.0f, 260.0f
    };
    const int selected = g_game.videoModeSelection();
    char label[64];
    snprintf(label, sizeof(label), "> %s <", modes[selected]);
    const float scale = 0.65f;
    drawCenteredText(label,
                     rowCenters[selected] - MENU_CELL_HEIGHT * scale * 0.5f,
                     scale, SELECTED_RED, SELECTED_GREEN, SELECTED_BLUE);
    const float promptScale = fittedTextScale(prompt, 540.0f);
    drawMenuText(prompt, 52.0f, 412.0f, promptScale,
                 189, 194, 204, 255, true);
}

void
drawNoSaveDataScreen(void)
{
    const char* message = g_game.localizedText(25041);
    drawMemoryCardArtwork(g_game.options().language);
    char prompt[256];
    makeCommandPrompt(prompt, sizeof(prompt), false);

    if(!loadRetainedMenuFont() || message == NULL)
        return;

    const float messageX =
        332.0f - measureRetainedMenuText(message, 1.0f) * 0.5f;
    drawRetainedMenuText(message, messageX, 107.0f, 1.0f,
                         TEXT_RED, TEXT_GREEN, TEXT_BLUE, 255, true);

    const float promptWidth = measureRetainedMenuText(prompt, 1.0f);
    const float promptScale =
        promptWidth > 540.0f ? 540.0f / promptWidth : 1.0f;
    drawRetainedMenuText(prompt, 52.0f, 412.0f, promptScale,
                         189, 194, 204, 255, true);
}

void
drawVersusTypeScreen(void)
{
    int language = g_game.options().language;
    if(language < 0 || language >= TITLE_SELECT_LANGUAGE_COUNT)
        language = 1;
    const uint16_t partsIndex =
        SELECT_MODE_PARTS_ARCHIVE_BY_LANGUAGE[language];

    if(!loadArtworkPackage(g_selectModeParts, partsIndex) ||
       !loadArtworkPackage(g_selectModeFont, SELECT_MODE_FONT_ARCHIVE_INDEX))
    {
        uiCenterText("SELECT MODE");
        for(int i = 0; i < VERSUS_BATTLE_TYPE_COUNT; ++i)
        {
            char label[128];
            composeVersusRow(i, label, sizeof(label));
            uiDrawMenuItem(label,
                           i == g_game.versusBattleTypeSelection(),
                           g_game.versusBattleTypeItemEnabled(i));
        }
        uiDrawCommandPrompt(true);
        return;
    }

    drawArtworkImage(g_selectModeParts, partsIndex, SELECT_MODE_BACKGROUND);
    drawArtworkImage(g_selectModeParts, partsIndex, SELECT_MODE_TITLE);

    const uint8_t selectedLevel =
        selectModeHighlightLevel(g_selectModeFrame);
    for(int i = 0; i < VERSUS_BATTLE_TYPE_COUNT; ++i)
    {
        char label[128];
        composeVersusRow(i, label, sizeof(label));
        const bool selected = i == g_game.versusBattleTypeSelection();
        const bool enabled = g_game.versusBattleTypeItemEnabled(i);
        const uint8_t level = !enabled ? DISABLED_LEVEL :
            (selected ? selectedLevel : SELECT_MODE_UNSELECTED_GREY);
        drawSelectModeText(label, SELECT_MODE_ROW_CENTER_X,
                           SELECT_MODE_ROW_Y[i], 1.0f,
                           level, level, level, 0xff, true);
    }

    const char* description = presentationText(
        SELECT_MODE_DESCRIPTION_TEXT_ID, "Select a battle type.");
    hudPromptSelectContext(2);
    hudPromptSetPlainText(52.0f, 412.0f, description, 0.8f, 0.8f,
                          0xc0, 0xc0, 0x80, 0xff);
    hudPromptDraw();
}

void
drawResultsScreen(void)
{
    const char* title = presentationText(GAME_TEXT_RESULTS, "RESULTS");
    const char* outcome = g_game.battleOutcome() == BATTLE_OUTCOME_PLAYER_WON ?
        "YOU WIN" :
        presentationText(GAME_TEXT_YOU_LOSE, "YOU LOSE");
    const char* action = presentationText(
        GAME_TEXT_RETRY_BATTLE, "RETRY BATTLE");
    if(g_game.battleOutcome() == BATTLE_OUTCOME_PLAYER_WON)
        action = g_game.mode() == GAME_MODE_STORY ? "CONTINUE" : "MAIN MENU";
    const char* controls =
        "ENTER / A: SELECT    ESC / B: RETURN TO TITLE";

    if(!loadMenuFont())
    {
        uiCenterText(title);
        uiCenterText(outcome);
        uiCenterText(action);
        uiCenterText(controls);
        return;
    }

    drawCenteredText(title, 192.0f, 1.0f,
                     TEXT_RED, TEXT_GREEN, TEXT_BLUE);
    drawCenteredText(outcome, 230.0f, 1.0f,
                     TEXT_RED, TEXT_GREEN, TEXT_BLUE);
    drawCenteredText(action, 306.0f, 1.0f,
                     SELECTED_RED, SELECTED_GREEN, SELECTED_BLUE);

    const float promptScale = fittedTextScale(controls, 600.0f);
    drawCenteredText(controls, 410.0f, promptScale, 189, 194, 204);
}

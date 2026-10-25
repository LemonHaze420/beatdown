#include "hud_prompt.h"

#include "game.h"

#include <rw.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "../formats/MENU_FONT/menu_font.h"
#include "../formats/XAF/xaf_runtime.h"

namespace
{

const char* GRAPH_ARCHIVE_PATH = "raw/XAF/GRAPH.XAF";
const char* DATA_ARCHIVE_PATH = "raw/XAF/DATA.XAF";

const uint32_t BUTTON_ENTRY_INDEX = 0x20;
const float BUTTON_ATLAS_WIDTH = 256.0f;
const float BUTTON_ATLAS_HEIGHT = 128.0f;

const uint32_t PANEL_BORDER_ENTRY_INDEX = 0x192f;
const int PANEL_BORDER_WIDTH = 64;
const int PANEL_BORDER_HEIGHT = 128;

const uint32_t GLYPH_ATLAS_ENTRY_INDEX = RETAINED_FONT_BASIC_ATLAS_ENTRY;
const int GLYPH_ATLAS_TEXTURE_WIDTH = RETAINED_FONT_BASIC_ATLAS_WIDTH;
const int GLYPH_ATLAS_TEXTURE_HEIGHT = RETAINED_FONT_BASIC_ATLAS_HEIGHT;

const int PANEL_TILE_CAPACITY = 64;

const uint8_t PROMPT_BLINK_ALWAYS = 0xff;

struct PromptSlot
{
    float x;
    float y;
    int glyphIndex;
    uint8_t blink;
    bool used;
    char text[64];
};
struct PromptPlainText
{
    float x;
    float y;
    float scaleX;
    float scaleY;
    HudRgba colour;
    bool used;
    char text[128];
};

struct HudPromptState
{
    rw::Texture* buttonAtlas;
    rw::Texture* panelBorder;
    rw::Texture* unicodeAtlas;
    rw::Texture* kanjiAtlas;
    bool loadAttempted;
    bool runtimeInitialized;
    uint8_t suppress;
    uint8_t enable;
    float blinkPhase;
    bool renderPending;
    PromptPlainText plainText;

    PromptSlot slots[HUD_PROMPT_SLOT_COUNT];
};

HudPromptState g_prompt = {};

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

rw::Raster* readImageRaster(const uint8_t* chunk, uint32_t length)
{
    if(length < 28 || read32(chunk) != rw::ID_STRUCT || read32(chunk + 4) != 16)
        return NULL;

    const uint32_t width = read32(chunk + 12);
    const uint32_t height = read32(chunk + 16);
    const uint32_t depth = read32(chunk + 20);
    const uint32_t pitch = read32(chunk + 24);
    if(width == 0 || height == 0 || width > 4096 || height > 4096 ||
       (depth != 4 && depth != 8 && depth != 16 && depth != 24 && depth != 32))
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

rw::Texture* loadIndexedStxTexture(const char* archive, uint32_t entryIndex,
                                   const char* name, int expectedWidth,
                                   int expectedHeight)
{
    std::vector<uint8_t> data;
    if(!readXafEntryByIndex(archive, entryIndex, data) || data.size() < 12 ||
       data.size() > UINT32_MAX || read32(data.data()) != rw::ID_IMAGE)
        return NULL;

    const uint32_t imageLength = read32(data.data() + 4);
    if(static_cast<uint64_t>(imageLength) + 12 != data.size())
        return NULL;
    rw::Raster* raster = readImageRaster(data.data() + 12, imageLength);
    if(raster == NULL || raster->width != expectedWidth ||
       raster->height != expectedHeight)
    {
        if(raster != NULL)
            raster->destroy();
        return NULL;
    }

    rw::Texture* texture = rw::Texture::create(raster);
    if(texture == NULL)
    {
        raster->destroy();
        return NULL;
    }
    strncpy(texture->name, name, sizeof(texture->name) - 1);
    texture->name[sizeof(texture->name) - 1] = '\0';
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}

bool emitQuad(rw::Texture* texture, float x, float y, float width,
              float height, float u0, float v0, float u1, float v1,
              const HudRgba& colour)
{
    if(width <= 0.0f || height <= 0.0f || rw::engine == NULL ||
       rw::engine->currentCamera == NULL)
        return false;
    if(texture != NULL && texture->raster == NULL)
        return false;

    rw::Camera* camera = static_cast<rw::Camera*>(rw::engine->currentCamera);
    if(camera->frameBuffer == NULL || camera->nearPlane <= 0.0f)
        return false;

    const float frameWidth = static_cast<float>(camera->frameBuffer->width);
    const float frameHeight = static_cast<float>(camera->frameBuffer->height);
    const float horizontalScale = frameWidth / HUD_VIRTUAL_WIDTH;
    const float verticalScale = frameHeight / HUD_VIRTUAL_HEIGHT;
    const float viewportScale =
        horizontalScale < verticalScale ? horizontalScale : verticalScale;
    const float viewportX =
        (frameWidth - HUD_VIRTUAL_WIDTH * viewportScale) * 0.5f;
    const float viewportY =
        (frameHeight - HUD_VIRTUAL_HEIGHT * viewportScale) * 0.5f;

    const float left = viewportX + x * viewportScale;
    const float right = viewportX + (x + width) * viewportScale;
    const float top = viewportY + hudVirtualY(y) * viewportScale;
    const float bottom = viewportY + hudVirtualY(y + height) * viewportScale;

    const float cornerX[4] = { left, right, right, left };
    const float cornerY[4] = { top, top, bottom, bottom };
    const float cornerU[4] = { u0, u1, u1, u0 };
    const float cornerV[4] = { v0, v0, v1, v1 };

    using namespace rw::RWDEVICE;
    Im2DVertex vertices[4];
    float xOffset = 0.0f;
    float yOffset = 0.0f;
#ifdef RWHALFPIXEL
    xOffset = -0.5f;
    yOffset = 0.5f;
#endif
    const float reciprocalZ = 1.0f / camera->nearPlane;
    const float z = rw::im2d::GetNearZ();
    for(int index = 0; index < 4; ++index)
    {
        vertices[index].setScreenX(cornerX[index] + xOffset);
        vertices[index].setScreenY(cornerY[index] + yOffset);
        vertices[index].setScreenZ(z);
        vertices[index].setCameraZ(camera->nearPlane);
        vertices[index].setRecipCameraZ(reciprocalZ);
        vertices[index].setColor(colour.r, colour.g, colour.b, colour.a);
        vertices[index].setU(cornerU[index], reciprocalZ);
        vertices[index].setV(cornerV[index], reciprocalZ);
    }
    static rw::uint16 indices[6] = { 0, 1, 2, 0, 2, 3 };

    const void* oldTexture = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    const uint32_t oldFilter = rw::GetRenderState(rw::TEXTUREFILTER);
    const uint32_t oldAddressU = rw::GetRenderState(rw::TEXTUREADDRESSU);
    const uint32_t oldAddressV = rw::GetRenderState(rw::TEXTUREADDRESSV);
    const uint32_t oldVertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
    const uint32_t oldZTest = rw::GetRenderState(rw::ZTESTENABLE);
    const uint32_t oldZWrite = rw::GetRenderState(rw::ZWRITEENABLE);
    const uint32_t oldCull = rw::GetRenderState(rw::CULLMODE);
    const uint32_t oldSourceBlend = rw::GetRenderState(rw::SRCBLEND);
    const uint32_t oldDestinationBlend = rw::GetRenderState(rw::DESTBLEND);

    rw::SetRenderStatePtr(rw::TEXTURERASTER,
                          texture != NULL ? texture->raster : NULL);
    rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::LINEAR);
    rw::SetRenderState(rw::TEXTUREADDRESSU, rw::Texture::CLAMP);
    rw::SetRenderState(rw::TEXTUREADDRESSV, rw::Texture::CLAMP);
    rw::SetRenderState(rw::VERTEXALPHA, 1);
    rw::SetRenderState(rw::ZTESTENABLE, 0);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);

    rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
    rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
    rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, vertices, 4,
                                     indices, 6);

    rw::SetRenderStatePtr(rw::TEXTURERASTER, const_cast<void*>(oldTexture));
    rw::SetRenderState(rw::TEXTUREFILTER, oldFilter);
    rw::SetRenderState(rw::TEXTUREADDRESSU, oldAddressU);
    rw::SetRenderState(rw::TEXTUREADDRESSV, oldAddressV);
    rw::SetRenderState(rw::VERTEXALPHA, oldVertexAlpha);
    rw::SetRenderState(rw::ZTESTENABLE, oldZTest);
    rw::SetRenderState(rw::ZWRITEENABLE, oldZWrite);
    rw::SetRenderState(rw::CULLMODE, oldCull);
    rw::SetRenderState(rw::SRCBLEND, oldSourceBlend);
    rw::SetRenderState(rw::DESTBLEND, oldDestinationBlend);
    return true;
}

bool emitButtonGlyph(int glyphIndex, float x, float y, float width,
                     float height, uint8_t alpha)
{
    if(glyphIndex < 0 || glyphIndex >= HUD_BUTTON_GLYPH_COUNT)
        return false;
    const HudButtonGlyph& rect = HUD_BUTTON_GLYPHS[glyphIndex];
    const HudRgba tint = { 255, 255, 255, alpha };
    return emitQuad(g_prompt.buttonAtlas, x, y, width, height,
                    rect.u / BUTTON_ATLAS_WIDTH, rect.v / BUTTON_ATLAS_HEIGHT,
                    (rect.u + rect.width) / BUTTON_ATLAS_WIDTH,
                    (rect.v + rect.height) / BUTTON_ATLAS_HEIGHT, tint);
}

void ensurePromptRuntimeState(void)
{
    if(g_prompt.runtimeInitialized)
        return;
    g_prompt.runtimeInitialized = true;
    g_prompt.enable = 1;
    for(int i = 0; i < HUD_PROMPT_SLOT_COUNT; ++i)
        g_prompt.slots[i].blink = PROMPT_BLINK_ALWAYS;
}

bool retainedHudGlyph(uint32_t codepoint, RetainedFontGlyph* glyph)
{
    const uint16_t resolved = retainedFontBoundCodepoint(
        codepoint, g_game.options().controllerTypeP1,
        g_game.options().language == HUD_LANGUAGE_JAPANESE);
    return retainedFontGlyphForCodepoint(resolved, glyph);
}

float retainedHudAdvance(uint32_t codepoint)
{
    RetainedFontGlyph glyph;
    return retainedHudGlyph(codepoint, &glyph) ?
        retainedFontGlyphAdvance(glyph) : 0.0f;
}

void emitRetainedHudGlyph(uint32_t codepoint, float x, float y,
                          float scaleX, float scaleY,
                          const HudRgba& colour)
{
    RetainedFontGlyph glyph;
    if(!retainedHudGlyph(codepoint, &glyph) ||
       retainedFontCodepoint(codepoint) == ' ')
        return;

    rw::Texture* texture = NULL;
    int atlasWidth = 0;
    int atlasHeight = 0;
    if(glyph.page == RETAINED_FONT_PAGE_BASIC)
    {
        texture = g_prompt.unicodeAtlas;
        atlasWidth = RETAINED_FONT_BASIC_ATLAS_WIDTH;
        atlasHeight = RETAINED_FONT_BASIC_ATLAS_HEIGHT;
    }
    else if(glyph.page == RETAINED_FONT_PAGE_KANJI)
    {
        texture = g_prompt.kanjiAtlas;
        atlasWidth = RETAINED_FONT_KANJI_ATLAS_WIDTH;
        atlasHeight = RETAINED_FONT_KANJI_ATLAS_HEIGHT;
    }
    else if(glyph.page == RETAINED_FONT_PAGE_CONTROLS)
    {
        texture = g_prompt.buttonAtlas;
        atlasWidth = RETAINED_FONT_CONTROLS_ATLAS_WIDTH;
        atlasHeight = RETAINED_FONT_CONTROLS_ATLAS_HEIGHT;
    }
    if(texture == NULL)
        return;

    emitQuad(texture, x, y + glyph.yOffset * scaleY,
             glyph.width * scaleX, glyph.height * scaleY,
             static_cast<float>(glyph.x) / atlasWidth,
             static_cast<float>(glyph.y) / atlasHeight,
             static_cast<float>(glyph.x + glyph.width) / atlasWidth,
             static_cast<float>(glyph.y + glyph.height) / atlasHeight,
             colour);
}

}

float
hudPromptLineHeight(float scaleY)
{
    return RETAINED_FONT_LINE_PITCH * scaleY;
}

bool
hudPromptLoad(void)
{
    ensurePromptRuntimeState();
    if(!g_prompt.loadAttempted)
    {
        g_prompt.loadAttempted = true;
        g_prompt.buttonAtlas = loadIndexedStxTexture(
            GRAPH_ARCHIVE_PATH, BUTTON_ENTRY_INDEX, "BUTTON.STX",
            static_cast<int>(BUTTON_ATLAS_WIDTH),
            static_cast<int>(BUTTON_ATLAS_HEIGHT));
        g_prompt.panelBorder = loadIndexedStxTexture(
            DATA_ARCHIVE_PATH, PANEL_BORDER_ENTRY_INDEX, "WIN_TEST.STX",
            PANEL_BORDER_WIDTH, PANEL_BORDER_HEIGHT);
        g_prompt.unicodeAtlas = loadIndexedStxTexture(
            GRAPH_ARCHIVE_PATH, GLYPH_ATLAS_ENTRY_INDEX, "UNICODEFONT.STX",
            GLYPH_ATLAS_TEXTURE_WIDTH, GLYPH_ATLAS_TEXTURE_HEIGHT);
        g_prompt.kanjiAtlas = loadIndexedStxTexture(
            GRAPH_ARCHIVE_PATH, RETAINED_FONT_KANJI_ATLAS_ENTRY,
            "UNICODEFONT_KANJI01.STX", RETAINED_FONT_KANJI_ATLAS_WIDTH,
            RETAINED_FONT_KANJI_ATLAS_HEIGHT);
    }
    return g_prompt.buttonAtlas != NULL && g_prompt.panelBorder != NULL &&
           g_prompt.unicodeAtlas != NULL && g_prompt.kanjiAtlas != NULL;
}

void
hudPromptRelease(void)
{
    if(g_prompt.buttonAtlas != NULL)
        g_prompt.buttonAtlas->destroy();
    if(g_prompt.panelBorder != NULL)
        g_prompt.panelBorder->destroy();
    if(g_prompt.unicodeAtlas != NULL)
        g_prompt.unicodeAtlas->destroy();
    if(g_prompt.kanjiAtlas != NULL)
        g_prompt.kanjiAtlas->destroy();
    g_prompt.buttonAtlas = NULL;
    g_prompt.panelBorder = NULL;
    g_prompt.unicodeAtlas = NULL;
    g_prompt.kanjiAtlas = NULL;
    g_prompt.loadAttempted = false;
}

void
hudDrawPanel(const HudPanelRecord& panel)
{
    if(!hudPromptLoad())
        return;

    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    hudPanelInterior(panel, &left, &top, &right, &bottom);
    const HudRgba interior = { HUD_PANEL_INTERIOR_R, HUD_PANEL_INTERIOR_G,
                               HUD_PANEL_INTERIOR_B, panel.alpha };
    emitQuad(NULL, left, top, right - left, bottom - top, 0.0f, 0.0f, 1.0f,
             1.0f, interior);

    HudPanelTile tiles[PANEL_TILE_CAPACITY];
    const int count = hudPanelTiles(panel, tiles, PANEL_TILE_CAPACITY);
    const HudRgba tint = { 255, 255, 255, panel.alpha };
    for(int i = 0; i < count; ++i)
        emitQuad(g_prompt.panelBorder, tiles[i].x, tiles[i].y, tiles[i].width,
                 tiles[i].height, tiles[i].u0, tiles[i].v0, tiles[i].u1,
                 tiles[i].v1, tint);
}

void
hudDrawBottomBand(void)
{

    emitQuad(NULL, HUD_BAND_CENTRE_X - HUD_BAND_WIDTH * 0.5f,
             HUD_BAND_CENTRE_Y - HUD_BAND_HEIGHT * 0.5f, HUD_BAND_WIDTH,
             HUD_BAND_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, HUD_BAND_COLOURS[0]);
}

void
hudPromptClear(void)
{
    for(int i = 0; i < HUD_PROMPT_SLOT_COUNT; ++i)
    {
        g_prompt.slots[i].used = false;
        g_prompt.slots[i].blink = PROMPT_BLINK_ALWAYS;
        g_prompt.slots[i].text[0] = '\0';
    }
    g_prompt.plainText.used = false;
    g_prompt.plainText.text[0] = '\0';

}

bool
hudPromptAdd(float x, float y, HudPromptKind kind, const char* text)
{

    if(kind < 0 || kind >= HUD_PROMPT_SLOT_COUNT || text == NULL)
        return false;
    const int glyphIndex =
        hudPromptGlyphIndex(kind, g_game.options().language);
    if(glyphIndex < 0 || glyphIndex >= HUD_BUTTON_GLYPH_COUNT)
        return false;

    PromptSlot& slot = g_prompt.slots[kind];
    slot.x = x;
    slot.y = y;
    slot.glyphIndex = glyphIndex;
    slot.blink = PROMPT_BLINK_ALWAYS;
    slot.used = true;
    strncpy(slot.text, text, sizeof(slot.text) - 1);
    slot.text[sizeof(slot.text) - 1] = '\0';
    return true;
}

bool
hudPromptAddTextId(float x, float y, HudPromptKind kind, uint16_t textId)
{
    return hudPromptAdd(x, y, kind, g_game.localizedText(textId));
}
void
hudPromptSetPlainText(float x, float y, const char* text, float scaleX,
                      float scaleY, uint8_t r, uint8_t g, uint8_t b,
                      uint8_t a)
{
    g_prompt.plainText.x = x;
    g_prompt.plainText.y = y;
    g_prompt.plainText.scaleX = scaleX;
    g_prompt.plainText.scaleY = scaleY;
    g_prompt.plainText.colour = { r, g, b, a };
    g_prompt.plainText.used = text != NULL && text[0] != '\0';
    if(!g_prompt.plainText.used)
    {
        g_prompt.plainText.text[0] = '\0';
        return;
    }
    strncpy(g_prompt.plainText.text, text,
            sizeof(g_prompt.plainText.text) - 1);
    g_prompt.plainText.text[sizeof(g_prompt.plainText.text) - 1] = '\0';
}

void
hudPromptSelectContext(int context)
{
    HudPromptEntry entries[HUD_PROMPT_CONTEXT_MAX_ENTRIES];
    const int count =
        hudPromptContext(context, entries, HUD_PROMPT_CONTEXT_MAX_ENTRIES);
    hudPromptClear();
    for(int i = 0; i < count; ++i)
        hudPromptAddTextId(entries[i].x, entries[i].y,
                           static_cast<HudPromptKind>(entries[i].kind),
                           entries[i].textId);
}

void
hudPromptSetGate(uint8_t suppress, uint8_t enable)
{
    ensurePromptRuntimeState();
    g_prompt.suppress = suppress;
    g_prompt.enable = enable;
}

void
hudPromptSetSlotBlink(HudPromptKind kind, uint8_t blink)
{
    ensurePromptRuntimeState();
    if(kind >= 0 && kind < HUD_PROMPT_SLOT_COUNT)
        g_prompt.slots[kind].blink = blink;
}

void
hudPromptUpdate(float deltaSeconds)
{
    ensurePromptRuntimeState();
    if(deltaSeconds <= 0.0f)
        return;
    g_prompt.blinkPhase += deltaSeconds;
    while(g_prompt.blinkPhase >= 1.0f)
        g_prompt.blinkPhase -= 1.0f;
}

void
hudPromptDraw(void)
{
    ensurePromptRuntimeState();
    g_prompt.renderPending = true;
}

void
hudPromptFlush(void)
{
    ensurePromptRuntimeState();
    if(!g_prompt.renderPending)
        return;
    g_prompt.renderPending = false;

    if(!hudPromptGateVisible(g_prompt.suppress, g_prompt.enable) ||
       !hudPromptLoad())
        return;

    bool any = false;
    for(int i = 0; i < HUD_PROMPT_SLOT_COUNT && !any; ++i)
        any = g_prompt.slots[i].used;
    if(!any)
        return;

    hudDrawPanel(HUD_PROMPT_BAR_PANEL);
    if(g_prompt.plainText.used)
    {
        const PromptPlainText& plain = g_prompt.plainText;
        hudDrawPromptText(plain.x, plain.y, plain.text,
                          plain.scaleX, plain.scaleY,
                          plain.colour.r, plain.colour.g, plain.colour.b,
                          plain.colour.a, false);
    }

    const HudRgba label = { HUD_PROMPT_LABEL_R, HUD_PROMPT_LABEL_G,
                            HUD_PROMPT_LABEL_B, 0xff };
    for(int i = 0; i < HUD_PROMPT_SLOT_COUNT; ++i)
    {
        const PromptSlot& slot = g_prompt.slots[i];
        if(!slot.used ||
           !hudPromptBlinkVisible(slot.blink, g_prompt.blinkPhase))
            continue;
        const HudButtonGlyph& rect = HUD_BUTTON_GLYPHS[slot.glyphIndex];
        const float glyphWidth = rect.width * HUD_PROMPT_SCALE;
        const float glyphHeight = rect.height * HUD_PROMPT_SCALE;

        emitButtonGlyph(slot.glyphIndex,
                        slot.x - glyphWidth * 0.5f,
                        slot.y - glyphHeight * 0.5f,
                        glyphWidth, glyphHeight, 0xff);
        hudDrawPromptText(
            slot.x + HUD_PROMPT_LABEL_OFFSET_X,
            slot.y + HUD_PROMPT_LABEL_OFFSET_Y -
                hudPromptLineHeight(HUD_PROMPT_SCALE) * 0.5f,
            slot.text, HUD_PROMPT_SCALE, HUD_PROMPT_SCALE, label.r, label.g,
            label.b, label.a, false);
    }
}

float
hudMeasurePromptText(const char* folded, float scaleX, float scaleY)
{
    (void)scaleY;
    if(folded == NULL)
        return 0.0f;
    float width = 0.0f;
    float widest = 0.0f;
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(folded);
    while(*cursor != 0)
    {
        const uint32_t codepoint = retainedFontNextCodepoint(cursor);
        if(codepoint == '\r')
            continue;
        if(codepoint == '\n')
        {
            if(width > widest)
                widest = width;
            width = 0.0f;
            continue;
        }
        width += retainedHudAdvance(codepoint) * scaleX;
    }
    return width > widest ? width : widest;
}

void
hudDrawPromptText(float x, float y, const char* folded, float scaleX,
                  float scaleY, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                  bool rightAlign)
{
    if(folded == NULL || folded[0] == 0 || !hudPromptLoad())
        return;

    const auto measureLine = [scaleX](const unsigned char* line)
    {
        float width = 0.0f;
        while(*line != 0)
        {
            const uint32_t codepoint = retainedFontNextCodepoint(line);
            if(codepoint == '\r')
                continue;
            if(codepoint == '\n')
                break;
            width += retainedHudAdvance(codepoint) * scaleX;
        }
        return width;
    };

    const HudRgba colour = { r, g, b, a };
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(folded);
    float penX = rightAlign ? x - measureLine(cursor) : x;
    float penY = y;
    while(*cursor != 0)
    {
        const uint32_t codepoint = retainedFontNextCodepoint(cursor);
        if(codepoint == '\r')
            continue;
        if(codepoint == '\n')
        {
            penY += hudPromptLineHeight(scaleY);
            penX = rightAlign ? x - measureLine(cursor) : x;
            continue;
        }
        emitRetainedHudGlyph(codepoint, penX, penY,
                             scaleX, scaleY, colour);
        penX += retainedHudAdvance(codepoint) * scaleX;
    }
}

void
hudDrawBottomLine(int bodyRow)
{
    if(bodyRow < 0 || bodyRow >= HUD_ADVICE_BODY_COUNT || !hudPromptLoad())
        return;

    const HudAdviceBody& row = HUD_ADVICE_BODY[bodyRow];
    if(row.layoutFlag != 1)
        return;
    const HudRgba& colour = HUD_ADVICE_BODY_COLOUR;
    hudDrawBottomBand();
    hudDrawPromptText(
        HUD_ADVICE_BOTTOM_X,
        HUD_ADVICE_BOTTOM_Y - hudPromptLineHeight(row.scaleY) * 0.5f,
        g_game.localizedText(row.textId), row.scaleX, row.scaleY,
        colour.r, colour.g, colour.b, colour.a, true);
}

void
hudDrawBattlePromptLine(void)
{

    hudDrawBottomLine(2);
}

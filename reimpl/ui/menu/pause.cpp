#include "ui/menu/pause.h"

#include "app/ui.h"
#include "ui/hud/battle.h"
#include "character/character.h"
#include "game/game.h"
#include "ui/hud/prompt.h"
#include "ui/menu/font.h"

#include "formats/MENU_LAYOUT/options_menu.h"
#include "formats/MENU_LAYOUT/pause_results_layout.h"
#include "formats/MOVE_TBL/move_tables.h"
#include "formats/XAF/xaf_runtime.h"

#include <rw.h>
#include <skeleton.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{

struct TextColour
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

const float SCREEN_CENTRE_X = 320.0f;
const float OPTIONS_VALUE_X = 330.0f;

const TextColour PAUSE_PROMPT_COLOUR = { 0xc0, 0xc0, 0x80, 0xc0 };

const char* GRAPH_ARCHIVE_PATH = "raw/XAF/GRAPH.XAF";
const uint8_t PAUSE_L2_ATLAS_GLYPH = 0xa1;
const uint8_t PAUSE_R2_ATLAS_GLYPH = 0xa4;
const int PAUSE_SELECTED_MIN = 0x40;
const int PAUSE_SELECTED_MAX = 0xff;
const int PAUSE_SELECTED_STEP = 12;
const int RESULT_SELECTED_MIN = 0x00;
const int RESULT_SELECTED_MAX = 0xff;
const int RESULT_SELECTED_STEP = 16;
const TextColour MOVE_LIST_NAME_COLOUR = { 0x7f, 0xbf, 0xaa, 0xff };
const TextColour MOVE_LIST_MUTED_COLOUR = { 0xb4, 0xb4, 0xb4, 0xff };
const TextColour MOVE_LIST_COMMAND_COLOUR = { 0xc0, 0xc0, 0x80, 0xff };
const float MOVE_LIST_NAME_X = 48.0f;
const float MOVE_LIST_COMMAND_X = 592.0f;
const float MOVE_LIST_FIRST_NAME_Y = 124.0f;
const float MOVE_LIST_FIRST_COMMAND_Y = 148.0f;
const float MOVE_LIST_ROW_PITCH = 54.0f;
const float MOVE_LIST_FOOTER_Y = 410.0f;
const float MOVE_LIST_PROMPT_Y = 452.0f;

const TextColour OPTIONS_LABEL_COLOUR = { 0x71, 0xee, 0xc8, 0xc0 };
const TextColour OPTIONS_VALUE_COLOUR = { 0xff, 0xff, 0x5f, 0xc0 };
const TextColour OPTIONS_HEADING_COLOUR = { 0xe0, 0xe0, 0xe0, 0xc0 };
const TextColour OPTIONS_SELECTED_COLOUR = { 0xff, 0xff, 0xe0, 0x80 };

const TextColour DISABLED_COLOUR = { 0x80, 0x80, 0x80, 0x80 };

const int OPTION_PULSE_STEP = 0x10;
const int OPTION_PULSE_MAX = 0xff;
const int OPTION_SELECTED_ALPHA_MAX = 0x80;
const uint16_t VOLUME_TEXT_FIRST = 9030;
const uint16_t VOLUME_TEXT_MAXIMUM = 9045;
int g_optionPulse;
int g_optionPulseStep = OPTION_PULSE_STEP;
int g_pausePulse = 0xb7;
int g_pausePulseStep = PAUSE_SELECTED_STEP;
int g_resultPulse = RESULT_SELECTED_MAX;
int g_resultPulseStep = -RESULT_SELECTED_STEP;
int g_resultSelection;
PauseEquippedMoveSlot g_pauseEquippedSlots[2] =
{

    { 2, 1, true },
    { 1, 2, true }
};
rw::Texture* g_resultBannerTexture;
uint8_t g_resultBannerLanguage = 0xff;
bool g_resultBannerLoadAttempted;
rw::Texture* g_moveListLogoTexture;
rw::Texture* g_moveListPartsTexture;
uint8_t g_moveListLogoLanguage = 0xff;
bool g_moveListLogoLoadAttempted;
bool g_moveListPartsLoadAttempted;

const char* presentationText(uint16_t textId, const char* hostLiteral)
{
    const char* text = g_game.localizedText(textId);
    return text != NULL ? text : hostLiteral;
}

const char* const BATTLE_PAUSE_ROW_ENGLISH[BATTLE_PAUSE_ROW_COUNT] =
{
    "ＲＥＳＵＭＥ",
    "ＭＯＶＥ　ＬＩＳＴ",
    "ＲＥＳＴＡＲＴ　ＴＨＩＳ　ＭＡＴＣＨ",
    "ＲＥＴＵＲＮ　ＴＯ　ＭＯＤＥ　ＳＥＬＥＣＴ",
    "ＲＥＴＵＲＮ　ＴＯ　ＣＨＡＲＡＣＴＥＲ　ＳＥＬＥＣＴ",
    "ＱＵＩＴ　ＧＡＭＥ"
};

const char* const BATTLE_PAUSE_DESCRIPTION_ENGLISH[BATTLE_PAUSE_ROW_COUNT] =
{
    "Ｒｅｔｕｒｎ　ｔｏ　ｂａｔｔｌｅ．",
    "Ｃｈｅｃｋ　ｙｏｕｒ　ｍｏｖｅ　ｌｉｓｔ．",
    "Ｒｅｓｔａｒｔ　ｔｈｉｓ　ｍａｔｃｈ．",
    "Ｒｅｔｕｒｎ　ｔｏ　ｍｏｄｅ　ｓｅｌｅｃｔ．",
    "Ｒｅｔｕｒｎ　ｔｏ　ｃｈａｒａｃｔｅｒ　ｓｅｌｅｃｔ．",
    "Ｒｅｔｕｒｎ　ｔｏ　ｔｉｔｌｅ　ｓｃｒｅｅｎ．"
};

bool battlePauseRows(void)
{
    return g_game.mode() != GAME_MODE_STORY;
}

int pauseRowCount(void)
{
    return battlePauseRows() ? BATTLE_PAUSE_ROW_COUNT : PAUSE_ROW_COUNT;
}

uint16_t pauseRowTextId(int row)
{
    return battlePauseRows() ?
        BATTLE_PAUSE_ROW_TEXT_IDS[row] : PAUSE_ROW_TEXT_IDS[row];
}

const char* pauseRowEnglishText(int row)
{
    if(battlePauseRows())
        return BATTLE_PAUSE_ROW_ENGLISH[row];
    return PAUSE_TEXT_SPRITES[PAUSE_SPRITE_RESUME + row].liveEnglishText;
}

uint16_t pauseDescriptionTextId(int row)
{
    return battlePauseRows() ?
        BATTLE_PAUSE_DESCRIPTION_TEXT_IDS[row] :
        PAUSE_DESCRIPTION_TEXT_IDS[row];
}

const char* pauseDescriptionEnglishText(int row)
{
    return battlePauseRows() ?
        BATTLE_PAUSE_DESCRIPTION_ENGLISH[row] :
        PAUSE_DESCRIPTION_ENGLISH[row];
}

void drawText(const char* text, float x, float y, float scale,
              const TextColour& colour, bool shadow)
{
    drawMenuText(text, x, y, scale, colour.r, colour.g, colour.b, colour.a,
                 shadow);
}

void drawCenteredText(const char* text, float centreX, float topY, float scale,
                      const TextColour& colour, bool shadow)
{
    drawText(text, centreX - measureMenuText(text, scale) * 0.5f,
             topY, scale, colour, shadow);
}

void drawRightAlignedText(const char* text, float rightX, float topY,
                          float scale, const TextColour& colour)
{
    drawText(text, rightX - measureMenuText(text, scale), topY, scale,
             colour, false);
}

uint8_t advanceOptionsSelectedAlpha(void)
{

    g_optionPulse += g_optionPulseStep;
    if(g_optionPulse > OPTION_PULSE_MAX)
    {
        g_optionPulse = OPTION_PULSE_MAX;
        g_optionPulseStep = -OPTION_PULSE_STEP;
    }
    else if(g_optionPulse < 0)
    {
        g_optionPulse = 0;
        g_optionPulseStep = OPTION_PULSE_STEP;
    }
    return static_cast<uint8_t>(
        g_optionPulse * OPTION_SELECTED_ALPHA_MAX / OPTION_PULSE_MAX);
}

uint8_t advancePauseSelectedGrey(void)
{

    const uint8_t current = static_cast<uint8_t>(g_pausePulse);
    g_pausePulse += g_pausePulseStep;
    if(g_pausePulse >= PAUSE_SELECTED_MAX)
    {
        g_pausePulse = PAUSE_SELECTED_MAX;
        g_pausePulseStep = -PAUSE_SELECTED_STEP;
    }
    else if(g_pausePulse <= PAUSE_SELECTED_MIN)
    {
        g_pausePulse = PAUSE_SELECTED_MIN;
        g_pausePulseStep = PAUSE_SELECTED_STEP;
    }
    return current;
}

void drawCommandPrompt(OptionPromptStyle style)
{
    hudPromptClear();
    if(style == OPTION_PROMPT_SELECT_OR_BACK)
    {
        hudPromptAddTextId(63.0f, 452.0f, HUD_PROMPT_DPAD, 0x28a2);
        hudPromptAddTextId(195.0f, 452.0f, HUD_PROMPT_CONFIRM, 0x28d4);
    }
    else
    {
        hudPromptAddTextId(63.0f, 452.0f, HUD_PROMPT_DPAD_LEFT, 0x28a2);
        if(style == OPTION_PROMPT_MOVIE_OR_BACK)
            hudPromptAddTextId(
                195.0f, 452.0f, HUD_PROMPT_CONFIRM, 0x28d4);
    }
    hudPromptAddTextId(459.0f, 452.0f, HUD_PROMPT_CANCEL, 0x28a6);
    hudPromptSetGate(0, 1);
    hudPromptDraw();
}

const char* optionRowValue(const OptionEntry& entry, char* buffer, size_t size)
{
    if(!optionEntryHasValue(entry))
        return NULL;
    const int value = g_game.optionValue(entry);
    if(entry.valueFormat == OPTION_VALUE_SIGNED_DECIMAL)
    {
        snprintf(buffer, size, "%d", value);
        return buffer;
    }
    const char* literal = optionValueLiteral(entry, value);
    const uint16_t textId = optionValueTextId(entry, value);
    return textId != 0 ? presentationText(textId, literal) : literal;
}

const char* optionsHeadingFallback(int page, bool inGame)
{
    static const char* const HEADINGS[OPTION_PAGE_COUNT] =
    {
        "OPTIONS", "AUDIO CONFIG.", "CONTROL CONFIG.",
        "SCREEN ADJUST", "BRIGHTNESS", "MOVIE VIEWER"
    };
    if(page == GAME_OPTIONS_PAGE_MAIN && inGame)
        return "CONFIG.";
    return HEADINGS[page];
}

uint32_t nextUtf8Codepoint(const unsigned char*& cursor)
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
    for(int i = 0; i < continuationCount; ++i)
    {
        if((continuation[i] & 0xc0) != 0x80)
            return first;
        codepoint = (codepoint << 6) | (continuation[i] & 0x3f);
    }
    if(codepoint < minimum || codepoint > 0x10ffff ||
       (codepoint >= 0xd800 && codepoint <= 0xdfff))
        return first;
    cursor += continuationCount;
    return codepoint;
}

void encodeAtlasText(const char* text, char* output, size_t capacity)
{
    if(capacity == 0)
        return;
    if(text == NULL)
    {
        output[0] = 0;
        return;
    }

    size_t written = 0;
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0 && written + 1 < capacity)
    {
        uint32_t codepoint = nextUtf8Codepoint(cursor);
        if(codepoint == 0x3000 || codepoint == 0x00a0)
            codepoint = ' ';
        else if(codepoint >= 0xff01 && codepoint <= 0xff5e)
            codepoint -= 0xfee0;
        else if(codepoint >= 0x2460 && codepoint <= 0x2465)
        {

            codepoint = 0xa0 + codepoint - 0x2460;
        }
        else if(codepoint == 0x2019)
            codepoint = '\'';
        else if(codepoint == 0x201c || codepoint == 0x201d)
            codepoint = '"';
        else if(codepoint == 0xffe5)
            codepoint = '\\';
        if(codepoint > 0xff)
            codepoint = '?';
        output[written++] = static_cast<char>(codepoint);
    }
    output[written] = 0;
}

void drawAlignedAtlasText(const char* text, float anchorX, float anchorY,
                          float scaleX, float scaleY, uint8_t alignment,
                          const PauseResultsColour& colour)
{
    char atlasText[512];
    encodeAtlasText(text, atlasText, sizeof(atlasText));
    const MenuTextExtent extent = measureMenuText(atlasText, scaleX, scaleY);
    float x = anchorX;
    float y = anchorY;
    if(alignment == PAUSE_TEXT_MIDDLE_LEFT)
        y -= extent.height * 0.5f;
    else if(alignment == PAUSE_TEXT_MIDDLE_CENTRE)
    {
        x -= extent.width * 0.5f;
        y -= extent.height * 0.5f;
    }
    else if(alignment == PAUSE_TEXT_MIDDLE_RIGHT)
    {
        x -= extent.width;
        y -= extent.height * 0.5f;
    }
    else if(alignment == PAUSE_TEXT_TOP_CENTRE)
        x -= extent.width * 0.5f;

    drawMenuText(atlasText, x, y, scaleX, scaleY,
                 colour.r, colour.g, colour.b, colour.a, false);
}

void drawPauseLayoutText(const PauseTextSpriteLayout& layout,
                         const char* text,
                         const PauseResultsColour& colour)
{
    drawAlignedAtlasText(text, layout.x, layout.y,
                         layout.scaleX, layout.scaleY,
                         layout.alignment, colour);
}

int originalPauseSelection(int hostSelection, int rowCount)
{
    return hostSelection >= 0 && hostSelection < rowCount ?
        hostSelection : -1;
}

bool buildEquippedAtlasLine(int slotIndex, uint8_t buttonGlyph,
                            char* output, size_t capacity)
{
    if(slotIndex < 0 || slotIndex >= 2 || capacity < 2 ||
       !g_pauseEquippedSlots[slotIndex].active)
        return false;
    const uint16_t textId = pauseEquippedTextId(
        g_pauseEquippedSlots[slotIndex].type,
        g_pauseEquippedSlots[slotIndex].id);
    if(textId == 0)
        return false;

    const PauseTextSpriteLayout& layout =
        PAUSE_TEXT_SPRITES[PAUSE_SPRITE_LEFT_EQUIPPED + slotIndex];
    const unsigned char* fallbackCursor =
        reinterpret_cast<const unsigned char*>(layout.liveEnglishText);
    nextUtf8Codepoint(fallbackCursor);
    const char* itemName = presentationText(
        textId, reinterpret_cast<const char*>(fallbackCursor));
    char atlasName[256];
    encodeAtlasText(itemName, atlasName, sizeof(atlasName));
    const size_t nameLength = strlen(atlasName);
    if(nameLength + 2 > capacity)
        return false;
    output[0] = static_cast<char>(buttonGlyph);
    memcpy(output + 1, atlasName, nameLength + 1);
    return true;
}

uint8_t advanceResultSelectedGrey(void)
{
    const uint8_t current = static_cast<uint8_t>(g_resultPulse);
    g_resultPulse += g_resultPulseStep;
    if(g_resultPulse >= RESULT_SELECTED_MAX)
    {
        g_resultPulse = RESULT_SELECTED_MAX;
        g_resultPulseStep = -RESULT_SELECTED_STEP;
    }
    else if(g_resultPulse <= RESULT_SELECTED_MIN)
    {
        g_resultPulse = RESULT_SELECTED_MIN;
        g_resultPulseStep = RESULT_SELECTED_STEP;
    }
    return current;
}

bool loadResultBannerTexture(uint8_t language)
{
    const ResultBannerVariant& variant = resultBannerVariant(language);
    if(g_resultBannerTexture != NULL &&
       g_resultBannerLanguage == language)
        return true;
    if(g_resultBannerTexture != NULL)
    {
        g_resultBannerTexture->destroy();
        g_resultBannerTexture = NULL;
    }
    if(g_resultBannerLanguage != language)
        g_resultBannerLoadAttempted = false;
    g_resultBannerLanguage = language;
    if(g_resultBannerLoadAttempted)
        return false;
    g_resultBannerLoadAttempted = true;

    std::vector<XafEntryInfo> entries;
    if(!listXafEntries(GRAPH_ARCHIVE_PATH, entries) ||
       variant.entryIndex >= entries.size())
        return false;
    const XafEntryInfo& entry = entries[variant.entryIndex];
    if(entry.name != variant.entryName)
        return false;

    rw::Texture* texture = loadStxTexture(
        GRAPH_ARCHIVE_PATH, entry.name.c_str());
    if(texture == NULL || texture->raster == NULL ||
       texture->raster->width !=
           static_cast<int32_t>(RESULT_BANNER_LAYOUT.textureWidth) ||
       texture->raster->height !=
           static_cast<int32_t>(RESULT_BANNER_LAYOUT.textureHeight))
    {
        if(texture != NULL)
            texture->destroy();
        return false;
    }
    texture->setAddressU(rw::Texture::CLAMP);
    texture->setAddressV(rw::Texture::CLAMP);
    texture->setFilter(rw::Texture::LINEAR);
    g_resultBannerTexture = texture;
    return true;
}

void drawResultBanner(void)
{
    if(ImGui::GetCurrentContext() == NULL ||
       !loadResultBannerTexture(g_game.options().language))
        return;

    const ImVec2 origin = ImGui::GetWindowPos();
    const float scaleX =
        static_cast<float>(sk::globals.width) / 640.0f;
    const float scaleY =
        static_cast<float>(sk::globals.height) / 480.0f;
    const float left = RESULT_BANNER_LAYOUT.centreX -
        RESULT_BANNER_LAYOUT.sourceWidth * 0.5f;
    const float top = RESULT_BANNER_LAYOUT.centreY -
        RESULT_BANNER_LAYOUT.sourceHeight * 0.5f;
    const ImTextureRef texture = ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(g_resultBannerTexture)));
    ImGui::GetWindowDrawList()->AddImage(
        texture,
        ImVec2(origin.x + left * scaleX, origin.y + top * scaleY),
        ImVec2(origin.x +
                   (left + RESULT_BANNER_LAYOUT.sourceWidth) * scaleX,
               origin.y +
                   (top + RESULT_BANNER_LAYOUT.sourceHeight) * scaleY),
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, RESULT_BANNER_LAYOUT.sourceHeight /
                        RESULT_BANNER_LAYOUT.textureHeight));
}

void drawResultSelector(void)
{
    if(!loadMenuFont())
        return;
    const uint8_t selectedGrey = advanceResultSelectedGrey();
    for(int i = 0; i < RESULT_CHOICE_COUNT; ++i)
    {
        const ResultChoiceLayout& choice = RESULT_CHOICES[i];
        PauseResultsColour colour = choice.normalColour;
        if(i == g_resultSelection)
        {
            colour = choice.selectedColour;
            colour.r = selectedGrey;
            colour.g = selectedGrey;
            colour.b = selectedGrey;
        }
        drawAlignedAtlasText(
            presentationText(choice.textId, choice.englishText),
            choice.x, choice.y, choice.scaleX, choice.scaleY,
            choice.alignment, colour);
    }

    const ResultChoiceLayout& selected = RESULT_CHOICES[g_resultSelection];
    drawAlignedAtlasText(
        presentationText(selected.descriptionTextId,
                         selected.englishDescription),
        RESULT_DESCRIPTION_LAYOUT.x, RESULT_DESCRIPTION_LAYOUT.y,
        RESULT_DESCRIPTION_LAYOUT.scaleX, RESULT_DESCRIPTION_LAYOUT.scaleY,
        PAUSE_TEXT_TOP_CENTRE, RESULT_DESCRIPTION_LAYOUT.colour);
}

struct MoveListLogoVariant
{
    uint16_t entryIndex;
    const char* entryName;
    float sourceWidth;
};

MoveListLogoVariant moveListLogoVariant(uint8_t language)
{
    switch(language)
    {
    case GAME_LANGUAGE_FRENCH:
        return { 0x120, "MEN_LOGO02_FRA.STX", 136.0f };
    case GAME_LANGUAGE_GERMAN:
        return { 0x121, "MEN_LOGO02_GER.STX", 174.0f };
    case GAME_LANGUAGE_SPANISH:
        return { 0x11f, "MEN_LOGO02_ESP.STX", 112.0f };
    case GAME_LANGUAGE_ITALIAN:
        return { 0x122, "MEN_LOGO02_ITA.STX", 182.0f };
    default:
        return { 0x11e, "MEN_LOGO02.STX", 181.0f };
    }
}

rw::Texture* loadMoveListGraphTexture(
    uint16_t entryIndex, const char* entryName, bool& attempted)
{
    if(attempted)
        return NULL;
    attempted = true;
    std::vector<XafEntryInfo> entries;
    if(!listXafEntries(GRAPH_ARCHIVE_PATH, entries) ||
       entryIndex >= entries.size() ||
       entries[entryIndex].name != entryName)
        return NULL;
    rw::Texture* texture =
        loadStxTexture(GRAPH_ARCHIVE_PATH, entryName);
    if(texture == NULL || texture->raster == NULL)
    {
        if(texture != NULL)
            texture->destroy();
        return NULL;
    }
    texture->setAddressU(rw::Texture::CLAMP);
    texture->setAddressV(rw::Texture::CLAMP);
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}

bool loadMoveListTextures(uint8_t language)
{
    if(g_moveListLogoLanguage != language)
    {
        if(g_moveListLogoTexture != NULL)
            g_moveListLogoTexture->destroy();
        g_moveListLogoTexture = NULL;
        g_moveListLogoLanguage = language;
        g_moveListLogoLoadAttempted = false;
    }
    const MoveListLogoVariant logo = moveListLogoVariant(language);
    if(g_moveListLogoTexture == NULL)
        g_moveListLogoTexture = loadMoveListGraphTexture(
            logo.entryIndex, logo.entryName, g_moveListLogoLoadAttempted);
    if(g_moveListPartsTexture == NULL)
        g_moveListPartsTexture = loadMoveListGraphTexture(
            0x132, "MEN_PARTS11.STX", g_moveListPartsLoadAttempted);
    return g_moveListLogoTexture != NULL && g_moveListPartsTexture != NULL;
}

void drawMoveListTexture(rw::Texture* texture, float x, float y,
                         float width, float height,
                         float sourceWidth, float sourceHeight)
{
    if(texture == NULL || texture->raster == NULL ||
       ImGui::GetCurrentContext() == NULL)
        return;
    const ImVec2 origin = ImGui::GetWindowPos();
    const float scaleX =
        static_cast<float>(sk::globals.width) / 640.0f;
    const float scaleY =
        static_cast<float>(sk::globals.height) / 480.0f;
    const ImTextureRef image = ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(texture)));
    ImGui::GetWindowDrawList()->AddImage(
        image,
        ImVec2(origin.x + x * scaleX, origin.y + y * scaleY),
        ImVec2(origin.x + (x + width) * scaleX,
               origin.y + (y + height) * scaleY),
        ImVec2(0.0f, 0.0f),
        ImVec2(sourceWidth / texture->raster->width,
               sourceHeight / texture->raster->height));
}

bool moveListNameMuted(int styleId, int entryIndex, const char* command)
{
    if(styleId < 17 || styleId > 21)
        return true;
    if((styleId == 18 &&
        (entryIndex == 0x2d || entryIndex == 0x1d ||
         entryIndex == 0x1c)) ||
       (styleId == 19 && entryIndex == 0x14) ||
       (styleId == 20 && entryIndex == 0x2b))
        return false;

    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(command);
    while(cursor != NULL && *cursor != 0)
    {

        if(*cursor == 0x91 || *cursor == 0x93)
            return true;
        ++cursor;
    }
    return false;
}

void drawMoveListScreen(void)
{
    if(!loadMenuFont())
        return;

    const int styleId = g_game.pauseMoveListStyle();
    const MoveListStyle* style = moveListStyle(styleId);
    if(style == NULL)
        return;
    uint8_t visible[128] = {};
    const int count = g_game.pauseMoveListVisibleEntries(
        visible, static_cast<int>(sizeof(visible)));
    int first = g_game.pauseMoveListFirstVisible();
    if(first < 0)
        first = 0;
    if(first > count)
        first = count;

    const MoveListLogoVariant logo =
        moveListLogoVariant(g_game.options().language);
    if(loadMoveListTextures(g_game.options().language))
    {
        drawMoveListTexture(g_moveListLogoTexture, 50.0f, 63.0f,
                            logo.sourceWidth, 64.0f,
                            logo.sourceWidth, 64.0f);
        for(int row = 0; row <= MOVE_LIST_VISIBLE_ROW_COUNT; ++row)
            drawMoveListTexture(
                g_moveListPartsTexture, 20.480011f,
                116.0f + row * MOVE_LIST_ROW_PITCH,
                128.0f, 16.0f, 128.0f, 16.0f);
    }
    else
    {
        const PauseResultsColour colour = { 0xff, 0xff, 0x5f, 0xff };
        drawAlignedAtlasText("MOVE LIST", 50.0f, 63.0f,
                             1.0f, 1.0f, PAUSE_TEXT_TOP_LEFT, colour);
    }

    const int rows = count - first < MOVE_LIST_VISIBLE_ROW_COUNT ?
        count - first : MOVE_LIST_VISIBLE_ROW_COUNT;
    for(int row = 0; row < rows; ++row)
    {
        const int entryIndex = visible[first + row];
        const char* name = g_game.localizedText(
            static_cast<uint16_t>(style->nameTextFirst + entryIndex));
        const char* command = g_game.localizedText(
            static_cast<uint16_t>(style->commandTextFirst + entryIndex));
        if(name == NULL)
            name = "";
        if(command == NULL)
            command = "";
        const TextColour& nameColour =
            moveListNameMuted(styleId, entryIndex, command) ?
                MOVE_LIST_MUTED_COLOUR : MOVE_LIST_NAME_COLOUR;
        const PauseResultsColour nameRgba =
            { nameColour.r, nameColour.g, nameColour.b, nameColour.a };
        const PauseResultsColour commandRgba =
            { MOVE_LIST_COMMAND_COLOUR.r, MOVE_LIST_COMMAND_COLOUR.g,
              MOVE_LIST_COMMAND_COLOUR.b, MOVE_LIST_COMMAND_COLOUR.a };
        drawAlignedAtlasText(
            name, MOVE_LIST_NAME_X,
            MOVE_LIST_FIRST_NAME_Y + row * MOVE_LIST_ROW_PITCH,
            1.0f, 1.0f, PAUSE_TEXT_MIDDLE_LEFT, nameRgba);
        drawAlignedAtlasText(
            command, MOVE_LIST_COMMAND_X,
            MOVE_LIST_FIRST_COMMAND_Y + row * MOVE_LIST_ROW_PITCH,
            0.7f, 0.7f, PAUSE_TEXT_MIDDLE_RIGHT, commandRgba);
    }

    const PauseResultsColour labelColour = { 0xb4, 0xb4, 0xb4, 0xff };
    drawAlignedAtlasText(
        presentationText(0x27ef, "1 ON 1 ONLY"),
        592.0f, 98.0f, 0.7f, 0.7f,
        PAUSE_TEXT_MIDDLE_RIGHT, labelColour);

    const int16_t characterId = g_game.activeRecordId(0);
    const char* characterName = g_game.localizedText(
        battleHudDisplayNameTextId(characterId));
    if(characterName == NULL || *characterName == 0)
        characterName = "Raven";
    char footer[256];
    snprintf(footer, sizeof(footer), "%s's moves : %d",
             characterName, count);
    const PauseResultsColour footerColour = { 0xc0, 0xc0, 0x80, 0xff };
    drawAlignedAtlasText(
        footer, SCREEN_CENTRE_X, MOVE_LIST_FOOTER_Y,
        0.8f, 0.8f, PAUSE_TEXT_TOP_CENTRE, footerColour);

    drawAlignedAtlasText(
        presentationText(0x28a2, "SELECT"),
        63.0f, MOVE_LIST_PROMPT_Y, 0.65f, 0.65f,
        PAUSE_TEXT_MIDDLE_LEFT, footerColour);
    drawAlignedAtlasText(
        presentationText(0x28a7, "BACK"),
        459.0f, MOVE_LIST_PROMPT_Y, 0.65f, 0.65f,
        PAUSE_TEXT_MIDDLE_RIGHT, footerColour);

    const PauseResultsColour arrowColour = { 0xff, 0xff, 0x5f, 0xff };
    if(first > 0)
        drawAlignedAtlasText(
            "\x91", 312.0f, 110.0f, 1.0f, 1.0f,
            PAUSE_TEXT_MIDDLE_CENTRE, arrowColour);
    if(first + rows < count)
        drawAlignedAtlasText(
            "\x93", 312.0f, 382.0f, 1.0f, 1.0f,
            PAUSE_TEXT_MIDDLE_CENTRE, arrowColour);
}

void drawPauseOverlay(void)
{

    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(PAUSE_OVERLAY_COLOUR.r, PAUSE_OVERLAY_COLOUR.g,
                 PAUSE_OVERLAY_COLOUR.b, PAUSE_OVERLAY_COLOUR.a));
}

}

void
setPauseEquippedMoveSlots(const PauseEquippedMoveSlot& left,
                          const PauseEquippedMoveSlot& right)
{
    g_pauseEquippedSlots[0] = left;
    g_pauseEquippedSlots[1] = right;
}

void
setResultScreenSelection(int selection)
{
    if(selection < 0)
        selection = 0;
    if(selection >= RESULT_CHOICE_COUNT)
        selection = RESULT_CHOICE_COUNT - 1;
    g_resultSelection = selection;
}

int
resultScreenSelection(void)
{
    return g_resultSelection;
}

ResultChoiceAction
resultScreenAction(void)
{
    return RESULT_CHOICES[g_resultSelection].action;
}

void
drawPauseScreen(void)
{
    drawPauseOverlay();
    if(g_game.pauseMoveListActive())
    {
        drawMoveListScreen();
        return;
    }
    if(!loadMenuFont())
        return;

    const PauseTextSpriteLayout& heading =
        PAUSE_TEXT_SPRITES[PAUSE_SPRITE_HEADING];
    drawPauseLayoutText(
        heading,
        presentationText(heading.textId, heading.liveEnglishText),
        heading.colour);

    const int rows = pauseRowCount();
    const int selected = originalPauseSelection(
        g_game.pauseSelection(), rows);
    const uint8_t selectedGrey = advancePauseSelectedGrey();
    for(int row = 0; row < rows; ++row)
    {
        PauseTextSpriteLayout layout =
            PAUSE_TEXT_SPRITES[PAUSE_SPRITE_MOVE_LIST];
        layout.y = 174.0f + 34.0f * row;
        const PauseResultsColour colour =
            row == selected ?
            PauseResultsColour{
                selectedGrey, selectedGrey, selectedGrey,
                PAUSE_TEXT_SPRITES[PAUSE_SPRITE_MOVE_LIST].colour.a
            } :
            PAUSE_TEXT_SPRITES[PAUSE_SPRITE_MOVE_LIST].colour;
        drawPauseLayoutText(
            layout,
            presentationText(pauseRowTextId(row),
                             pauseRowEnglishText(row)),
            colour);
    }

    const int descriptionRow = selected >= 0 ? selected : 0;
    const PauseTextSpriteLayout& description =
        PAUSE_TEXT_SPRITES[PAUSE_SPRITE_DESCRIPTION];
    drawPauseLayoutText(
        description,
        presentationText(pauseDescriptionTextId(descriptionRow),
                         pauseDescriptionEnglishText(descriptionRow)),
        description.colour);

    char equippedLine[256];
    const PauseTextSpriteLayout& left =
        PAUSE_TEXT_SPRITES[PAUSE_SPRITE_LEFT_EQUIPPED];
    if(buildEquippedAtlasLine(0, PAUSE_L2_ATLAS_GLYPH,
                              equippedLine, sizeof(equippedLine)))
    {
        hudDrawPromptText(
            left.x, left.y - MENU_CELL_HEIGHT * left.scaleY * 0.5f,
            equippedLine, left.scaleX, left.scaleY,
            left.colour.r, left.colour.g, left.colour.b, left.colour.a,
            false);
    }
    const PauseTextSpriteLayout& right =
        PAUSE_TEXT_SPRITES[PAUSE_SPRITE_RIGHT_EQUIPPED];
    if(buildEquippedAtlasLine(1, PAUSE_R2_ATLAS_GLYPH,
                              equippedLine, sizeof(equippedLine)))
    {
        hudDrawPromptText(
            right.x, right.y - MENU_CELL_HEIGHT * right.scaleY * 0.5f,
            equippedLine, right.scaleX, right.scaleY,
            right.colour.r, right.colour.g, right.colour.b, right.colour.a,
            true);
    }

    hudPromptSelectContext(10);
    hudPromptDraw();
}

void
drawRecoveredResultsScreen(void)
{
    if(g_game.resultSelectorVisible())
        drawResultSelector();
    else if(g_game.mode() == GAME_MODE_STORY &&
            g_game.battleOutcome() == BATTLE_OUTCOME_PLAYER_WON)
        drawResultBanner();
}

void
releaseMenuPauseResources(void)
{
    if(g_resultBannerTexture != NULL)
        g_resultBannerTexture->destroy();
    g_resultBannerTexture = NULL;
    g_resultBannerLanguage = 0xff;
    g_resultBannerLoadAttempted = false;
    if(g_moveListLogoTexture != NULL)
        g_moveListLogoTexture->destroy();
    if(g_moveListPartsTexture != NULL)
        g_moveListPartsTexture->destroy();
    g_moveListLogoTexture = NULL;
    g_moveListPartsTexture = NULL;
    g_moveListLogoLanguage = 0xff;
    g_moveListLogoLoadAttempted = false;
    g_moveListPartsLoadAttempted = false;
}

void
drawOptionsScreen(void)
{
    const int page = static_cast<int>(g_game.optionsPage());
    if(!loadMenuFont() || page < 0 || page >= OPTION_PAGE_COUNT)
    {
        uiDrawOptionsFallback();
        return;
    }

    if(page == GAME_OPTIONS_PAGE_BRIGHTNESS)
    {
        uiDrawOptionsFallback();
        return;
    }

    const OptionPage& current = OPTION_PAGES[page];
    const bool inGame = g_game.optionsOpenedFromPause();
    const uint16_t headingId = inGame ? current.headingInGame :
                                        current.headingTitle;
    const char* heading = presentationText(
        headingId, optionsHeadingFallback(page, inGame));
    drawCenteredText(heading, OPTIONS_HEADING_X, OPTIONS_HEADING_Y,
                     OPTIONS_HEADING_SCALE, OPTIONS_HEADING_COLOUR, false);

    TextColour selectedColour = OPTIONS_SELECTED_COLOUR;
    selectedColour.a = advanceOptionsSelectedAlpha();
    const int selected = g_game.optionsSelection();
    int itemCount = g_game.optionsItemCount();
    if(itemCount > current.entryCount)
        itemCount = current.entryCount;
    char valueBuffer[32];
    int visibleIndex = 0;
    for(int i = 0; i < itemCount; ++i)
    {
        const OptionEntry& entry = current.entries[i];
        if(!g_game.optionsItemVisible(i))
            continue;
        const char* label = entry.textId != 0 ?
            presentationText(entry.textId, entry.literal) : entry.literal;
        const bool selectedRow = i == selected;
        const TextColour& labelColour =
            selectedRow ? selectedColour : OPTIONS_LABEL_COLOUR;
        const TextColour& valueColour =
            selectedRow ? selectedColour : OPTIONS_VALUE_COLOUR;
        const float y = optionsRowY(visibleIndex++);
        drawRightAlignedText(label, OPTIONS_LABEL_RIGHT_X, y,
                             OPTIONS_ROW_SCALE, labelColour);

        const char* value = optionRowValue(
            entry, valueBuffer, sizeof(valueBuffer));
        if(value != NULL)
            drawText(value, OPTIONS_VALUE_X, y, OPTIONS_ROW_SCALE,
                     valueColour, false);
    }

    const OptionEntry& selectedEntry = current.entries[selected];
    const char* help = presentationText(
        optionHelpTextId(selectedEntry, inGame), NULL);
    if(help != NULL)
        drawCenteredText(help, SCREEN_CENTRE_X, 410.0f, 0.55f,
                         PAUSE_PROMPT_COLOUR, false);
    drawCommandPrompt(selectedEntry.promptStyle);
}

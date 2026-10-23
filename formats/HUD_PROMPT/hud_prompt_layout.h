#ifndef HUD_PROMPT_LAYOUT_H
#define HUD_PROMPT_LAYOUT_H
#include <stddef.h>
#include <stdint.h>
#include "hud_prompt_tables.h"

const float HUD_VIRTUAL_WIDTH = 640.0f;
const float HUD_VIRTUAL_HEIGHT = 480.0f;

const float HUD_FIELD_HEIGHT_PAL = 256.0f;
const float HUD_FIELD_HEIGHT_NTSC = 224.0f;
const float HUD_FIELD_Y_BIAS = -16.0f;
const float HUD_FIELD_Y_BIAS_PAL = 6.0f;

float hudFieldY(float y, bool ntsc, float verticalScale);

float hudVirtualY(float y);

struct HudPanelRecord
{
    float x;
    float y;

    int16_t cellSize;
    int16_t cols;
    int16_t rows;
    uint8_t alpha;
    int32_t renderKey;
};

extern const HudPanelRecord HUD_PROMPT_BAR_PANEL;

extern const HudPanelRecord HUD_ADVICE_PANEL;

struct HudPanelTile
{
    float x;
    float y;
    float width;
    float height;
    float u0;
    float v0;
    float u1;
    float v1;
};

int hudPanelTileCount(const HudPanelRecord& panel);

int hudPanelTiles(const HudPanelRecord& panel, HudPanelTile* out, int capacity);

void hudPanelInterior(const HudPanelRecord& panel, float* left, float* top,
                      float* right, float* bottom);

const uint8_t HUD_PANEL_INTERIOR_R = 4;
const uint8_t HUD_PANEL_INTERIOR_G = 13;
const uint8_t HUD_PANEL_INTERIOR_B = 22;

const float HUD_BAND_CENTRE_X = 320.0f;
const float HUD_BAND_CENTRE_Y = 452.0f;
const float HUD_BAND_WIDTH = 644.0f;
const float HUD_BAND_HEIGHT = 32.0f;

enum HudPromptKind
{
    HUD_PROMPT_DPAD = 0,
    HUD_PROMPT_DPAD_VERTICAL = 1,
    HUD_PROMPT_CONFIRM = 2,
    HUD_PROMPT_CANCEL = 3,
    HUD_PROMPT_SQUARE = 4,
    HUD_PROMPT_TRIANGLE = 5,
    HUD_PROMPT_L1 = 6,
    HUD_PROMPT_R1 = 7,
    HUD_PROMPT_L2 = 8,
    HUD_PROMPT_R2 = 9,
    HUD_PROMPT_START = 10,
    HUD_PROMPT_DPAD_LEFT = 11,
    HUD_PROMPT_DPAD_RIGHT = 12,
    HUD_PROMPT_DPAD_UP = 13,
    HUD_PROMPT_DPAD_DOWN = 14,
    HUD_PROMPT_DPAD_ALT = 15,
    HUD_PROMPT_KIND_COUNT = 16
};

const uint8_t HUD_LANGUAGE_JAPANESE = 0;

int hudPromptGlyphIndex(int kind, uint8_t language);

struct HudPromptEntry
{
    float x;
    float y;
    uint8_t kind;

    uint16_t textId;
};
const float HUD_PROMPT_LABEL_OFFSET_X = 14.0f;
const float HUD_PROMPT_LABEL_OFFSET_Y = -2.0f;
const float HUD_PROMPT_SCALE = 0.8f;

const uint8_t HUD_PROMPT_LABEL_R = 0xff;
const uint8_t HUD_PROMPT_LABEL_G = 0xff;
const uint8_t HUD_PROMPT_LABEL_B = 0x5f;
const int HUD_PROMPT_LABEL_GROUP = 7001;

const int HUD_PROMPT_GLYPH_RENDER_KEY = 7002;

const int HUD_PROMPT_SLOT_COUNT = 16;

bool hudPromptGateVisible(uint8_t suppress, uint8_t enable);
bool hudPromptBlinkVisible(uint8_t blink, float phase);

float hudRetainedBlockTop(float centreY, int lineCount, float scaleY);
float hudRetainedCentredLineLeft(float centreX, float lineWidth);

bool hudAdviceUsesGenericHeading(int bodyRow);

const int HUD_PROMPT_CONTEXT_COUNT = 11;
const int HUD_PROMPT_CONTEXT_MAX_ENTRIES = 4;

int hudPromptContext(int context, HudPromptEntry* out, int capacity);

enum HudPromptContextFlags
{
    HUD_PROMPT_CONTEXT_RESTORE = 1,
    HUD_PROMPT_CONTEXT_SAVE = 2,
    HUD_PROMPT_CONTEXT_QUERY_SAVED = 4,
    HUD_PROMPT_CONTEXT_QUERY_CURRENT = 8
};

const float HUD_ADVICE_HEADING_X = 320.0f;
const float HUD_ADVICE_HEADING_Y = 198.0f;
const float HUD_ADVICE_BODY_X = 320.0f;
const float HUD_ADVICE_BODY_Y = 254.0f;
const float HUD_ADVICE_BOTTOM_X = 600.0f;
const float HUD_ADVICE_BOTTOM_Y = 450.0f;

const uint16_t HUD_ADVICE_HEADING_TEXT_ID = 30241;
const uint16_t HUD_ADVICE_HEADING_GENERIC_TEXT_ID = 30243;

const uint16_t HUD_BATTLE_PROMPT_TEXT_ID = 30271;
const uint16_t HUD_BATTLE_PROMPT_TEXT_ID_ALT = 30232;

size_t hudFoldDisplayText(const char* utf8, char* out, size_t capacity);

uint8_t hudFoldCodePoint(uint32_t codePoint);
#endif

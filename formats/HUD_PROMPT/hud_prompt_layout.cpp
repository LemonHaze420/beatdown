#include "hud_prompt_layout.h"
namespace
{

const int PANEL_GROUP_TOP_LEFT = 0x00;
const int PANEL_GROUP_TOP_MIDDLE = 0x04;
const int PANEL_GROUP_TOP_RIGHT = 0x08;
const int PANEL_GROUP_MIDDLE_LEFT = 0x0c;
const int PANEL_GROUP_MIDDLE_RIGHT = 0x10;
const int PANEL_GROUP_BOTTOM_LEFT = 0x14;
const int PANEL_GROUP_BOTTOM_MIDDLE = 0x18;
const int PANEL_GROUP_BOTTOM_RIGHT = 0x1c;

const uint32_t FOLD_SPACE_MARKER = 0xffffffffu;
int panelGroup(int row, int column, int rows, int columns)
{
    const bool first = column == 0;
    const bool last = column == columns - 1;
    if(row == 0)
        return first ? PANEL_GROUP_TOP_LEFT
                     : last ? PANEL_GROUP_TOP_RIGHT : PANEL_GROUP_TOP_MIDDLE;
    if(row == rows - 1)
        return first ? PANEL_GROUP_BOTTOM_LEFT
                     : last ? PANEL_GROUP_BOTTOM_RIGHT
                            : PANEL_GROUP_BOTTOM_MIDDLE;
    return first ? PANEL_GROUP_MIDDLE_LEFT : PANEL_GROUP_MIDDLE_RIGHT;
}
void fillTile(const HudPanelRecord& panel, int row, int column,
              HudPanelTile& tile)
{
    const float cell = static_cast<float>(panel.cellSize);
    tile.x = panel.x + cell * static_cast<float>(column);
    tile.y = panel.y + cell * static_cast<float>(row);
    tile.width = cell;
    tile.height = cell;

    const int group = panelGroup(row, column, panel.rows, panel.cols);
    tile.u0 = HUD_PANEL_UV[group].u;
    tile.v0 = HUD_PANEL_UV[group].v;
    tile.u1 = HUD_PANEL_UV[group + 3].u;
    tile.v1 = HUD_PANEL_UV[group + 3].v;
}
struct PromptContext
{
    int count;
    HudPromptEntry entries[HUD_PROMPT_CONTEXT_MAX_ENTRIES];
};

const PromptContext PROMPT_CONTEXTS[HUD_PROMPT_CONTEXT_COUNT] =
{
    { 1, { { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 } } },
    { 2, { { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 3, { {  63.0f, 452.0f, HUD_PROMPT_DPAD, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 3, { {  63.0f, 452.0f, HUD_PROMPT_DPAD_VERTICAL, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 3, { {  63.0f, 452.0f, HUD_PROMPT_DPAD_LEFT, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 4, { {  63.0f, 452.0f, HUD_PROMPT_DPAD_LEFT, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 327.0f, 452.0f, HUD_PROMPT_START, 30218 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 4, { {  63.0f, 452.0f, HUD_PROMPT_DPAD_LEFT, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 327.0f, 452.0f, HUD_PROMPT_SQUARE, 10413 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 4, { {  63.0f, 452.0f, HUD_PROMPT_DPAD, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 327.0f, 452.0f, HUD_PROMPT_START, 30218 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 4, { {  63.0f, 452.0f, HUD_PROMPT_DPAD, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 327.0f, 452.0f, HUD_PROMPT_SQUARE, 10413 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 4, { {  63.0f, 452.0f, HUD_PROMPT_DPAD_VERTICAL, 10402 },
           { 195.0f, 452.0f, HUD_PROMPT_CONFIRM, 10403 },
           { 327.0f, 452.0f, HUD_PROMPT_SQUARE, 10413 },
           { 459.0f, 452.0f, HUD_PROMPT_CANCEL, 10406 } } },
    { 2, { { 195.0f, 452.0f, HUD_PROMPT_DPAD, 10402 },
           { 327.0f, 452.0f, HUD_PROMPT_CONFIRM, 10452 } } }
};

const int PROMPT_GLYPH[HUD_PROMPT_KIND_COUNT] =
{
    0x1f, 0x1e, -1, -1, 0x04, 0x02, 0x08, 0x0a,
    0x0c, 0x0e, 0x16, 0x11, 0x12, 0x13, 0x14, 0x15
};
uint32_t packSequence(const unsigned char* cursor, int bytes)
{
    uint32_t value = 0;
    for(int i = 0; i < bytes; ++i)
        value = (value << 8) | cursor[i];
    return value;
}
}
const HudPanelRecord HUD_PROMPT_BAR_PANEL = { -32.0f, 390.0f, 32, 22, 3,
                                              0xd8, 7000 };
const HudPanelRecord HUD_ADVICE_PANEL = { 38.0f, 156.0f, 32, 17, 6,
                                          0xd8, 9999 };
float
hudFieldY(float y, bool ntsc, float verticalScale)
{
    const float height = ntsc ? HUD_FIELD_HEIGHT_NTSC : HUD_FIELD_HEIGHT_PAL;
    const float bias = ntsc ? 0.0f : HUD_FIELD_Y_BIAS_PAL;
    return (y + HUD_FIELD_Y_BIAS + bias) * (height / HUD_VIRTUAL_HEIGHT) *
           verticalScale;
}
float
hudVirtualY(float y)
{

    return y + HUD_FIELD_Y_BIAS + HUD_FIELD_Y_BIAS_PAL;
}
bool
hudPromptGateVisible(uint8_t suppress, uint8_t enable)
{
    return suppress == 0 && enable != 0;
}
bool
hudPromptBlinkVisible(uint8_t blink, float phase)
{
    if(blink == 1)
        return phase < 0.5f;
    if(blink == 0)
        return phase >= 0.5f;
    return true;
}
float
hudRetainedBlockTop(float centreY, int lineCount, float scaleY)
{
    if(lineCount < 0)
        lineCount = 0;
    return centreY -
        static_cast<float>(lineCount) * 24.0f * scaleY * 0.5f;
}
float
hudRetainedCentredLineLeft(float centreX, float lineWidth)
{
    return centreX - lineWidth * 0.5f;
}
bool
hudAdviceUsesGenericHeading(int bodyRow)
{
    return bodyRow == 0x11;
}
int
hudPanelTileCount(const HudPanelRecord& panel)
{
    if(panel.cols < 2 || panel.rows < 2)
        return 0;
    return (panel.cols + panel.rows - 2) * 2;
}
int
hudPanelTiles(const HudPanelRecord& panel, HudPanelTile* out, int capacity)
{
    const int count = hudPanelTileCount(panel);
    if(out == NULL || count == 0 || capacity < count)
        return 0;
    int written = 0;

    for(int row = 0; row < panel.rows; row += panel.rows - 1)
        for(int column = 0; column < panel.cols; ++column)
            fillTile(panel, row, column, out[written++]);

    for(int row = 1; row < panel.rows - 1; ++row)
        for(int column = 0; column < panel.cols; column += panel.cols - 1)
            fillTile(panel, row, column, out[written++]);
    return written;
}
void
hudPanelInterior(const HudPanelRecord& panel, float* left, float* top,
                 float* right, float* bottom)
{
    const float cell = static_cast<float>(panel.cellSize);
    if(left != NULL)
        *left = panel.x + cell;
    if(top != NULL)
        *top = panel.y + cell;
    if(right != NULL)
        *right = panel.x + cell * static_cast<float>(panel.cols - 1);
    if(bottom != NULL)
        *bottom = panel.y + cell * static_cast<float>(panel.rows - 1);
}
int
hudPromptGlyphIndex(int kind, uint8_t language)
{
    if(kind < 0 || kind >= HUD_PROMPT_KIND_COUNT)
        return -1;
    const bool japanese = language == HUD_LANGUAGE_JAPANESE;
    if(kind == HUD_PROMPT_CONFIRM)
        return japanese ? 0x00 : 0x06;
    if(kind == HUD_PROMPT_CANCEL)
        return japanese ? 0x06 : 0x02;
    return PROMPT_GLYPH[kind];
}
int
hudPromptContext(int context, HudPromptEntry* out, int capacity)
{
    if(context < 0 || context >= HUD_PROMPT_CONTEXT_COUNT || out == NULL)
        return 0;
    const PromptContext& source = PROMPT_CONTEXTS[context];
    if(capacity < source.count)
        return 0;
    for(int i = 0; i < source.count; ++i)
        out[i] = source.entries[i];
    return source.count;
}
uint8_t
hudFoldCodePoint(uint32_t codePoint)
{

    if(codePoint == 0x20 || codePoint == 0x3000)
        return 0x20;
    if(codePoint == 0x0a)
        return 0x0a;
    if(codePoint == 0x2019)
        return '\'';
    if(codePoint >= 0xff00 && codePoint <= 0xff3f)
        return static_cast<uint8_t>(0x80 + (codePoint - 0xff00) - 0x60);
    if(codePoint >= 0xff40 && codePoint <= 0xff5d)
        return static_cast<uint8_t>(0x80 + (codePoint - 0xff40) - 0x20);
    if(codePoint >= 0x00a0 && codePoint <= 0x00bf)
        return static_cast<uint8_t>(codePoint);
    if(codePoint >= 0x00c0 && codePoint <= 0x00ff)
        return static_cast<uint8_t>(codePoint);
    if(codePoint >= 0x0150 && codePoint <= 0x0153)
        return static_cast<uint8_t>(0x80 + (codePoint - 0x0150));
    if(codePoint < 0x0800)
        return static_cast<uint8_t>(0x80 | (codePoint & 0x3f));

    return static_cast<uint8_t>(0x80 | (codePoint & 0x3f));
}
size_t
hudFoldDisplayText(const char* utf8, char* out, size_t capacity)
{
    if(out == NULL || capacity == 0)
        return 0;
    if(utf8 == NULL)
    {
        out[0] = '\0';
        return 0;
    }
    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(utf8);
    size_t written = 0;
    while(*cursor != 0 && written + 1 < capacity)
    {
        const unsigned char lead = cursor[0];
        uint32_t packed;
        int consumed;
        if(lead == 0x20)
        {
            packed = FOLD_SPACE_MARKER;
            consumed = 1;
        }
        else if(lead == 0x81 && cursor[1] == 0x40)
        {
            packed = FOLD_SPACE_MARKER;
            consumed = 2;
        }
        else if(lead == 0x0a)
        {
            packed = 0x0a;
            consumed = 1;
        }
        else if(lead == 0x5c && cursor[1] == 0x6e)
        {
            packed = 0x5c6e;
            consumed = 2;
        }
        else if(lead == 0x24 || lead == 0x40)
        {

            ++cursor;
            continue;
        }
        else if((lead == 0xc2 && cursor[1] >= 0xa0 && cursor[1] <= 0xbf) ||
                (lead == 0xc3 && cursor[1] >= 0x80 && cursor[1] <= 0xbf) ||
                (lead == 0xc4 && cursor[1] >= 0x80 && cursor[1] <= 0xbf) ||
                (lead == 0xc5 && cursor[1] >= 0x80 && cursor[1] <= 0xbf))
        {
            packed = packSequence(cursor, 2);
            consumed = 2;
        }
        else
        {

            if(cursor[1] == 0 || cursor[2] == 0)
                break;
            packed = packSequence(cursor, 3);
            consumed = 3;
        }
        unsigned char emitted;
        if(packed == FOLD_SPACE_MARKER)
            emitted = 0x20;
        else if(packed == 0x0a || packed == 0x5c6e)
            emitted = 0x0a;
        else if(packed == 0xe28099u)
            emitted = '\'';
        else if(packed >= 0xefbc80u && packed <= 0xefbcbfu)
            emitted = static_cast<unsigned char>((packed & 0xff) - 0x60);
        else if(packed >= 0xefbd80u && packed <= 0xefbd9du)
            emitted = static_cast<unsigned char>((packed & 0xff) - 0x20);
        else if(packed >= 0xc2a0u && packed <= 0xc2bfu)
            emitted = static_cast<unsigned char>(packed & 0xff);
        else if(packed >= 0xc380u && packed <= 0xc3bfu)
            emitted = static_cast<unsigned char>((packed & 0xff) + 0x40);
        else if(packed >= 0xc580u && packed <= 0xc5bfu &&
                (packed - 0xc440u) >= 0x150u && (packed - 0xc440u) <= 0x153u)
            emitted = static_cast<unsigned char>(0x80 + (packed - 0xc440u) -
                                                 0x150u);
        else
            emitted = static_cast<unsigned char>(packed & 0xff);
        out[written++] = static_cast<char>(emitted);
        cursor += consumed;
    }
    out[written] = '\0';
    return written;
}

#include "ui/hud/advice.h"

#include "game/game.h"
#include "ui/hud/prompt.h"

#include <string.h>

namespace
{

int countLines(const char* text)
{
    if(text == NULL || text[0] == 0)
        return 0;
    int lines = 1;
    for(const char* cursor = text; *cursor != 0; ++cursor)
        if(*cursor == '\n')
            ++lines;
    return lines;
}

void drawCentredBlock(const char* text, float centreX, float centreY,
                      float scaleX, float scaleY, const HudRgba& colour)
{
    const int lines = countLines(text);
    if(lines == 0)
        return;
    const float pitch = hudPromptLineHeight(scaleY);

    float top = hudRetainedBlockTop(centreY, lines, scaleY);

    const char* cursor = text;
    while(cursor != NULL && *cursor != 0)
    {
        const char* newline = strchr(cursor, '\n');
        char line[128];
        const size_t length =
            newline != NULL ? static_cast<size_t>(newline - cursor)
                            : strlen(cursor);
        const size_t copied =
            length < sizeof(line) - 1 ? length : sizeof(line) - 1;
        memcpy(line, cursor, copied);
        line[copied] = '\0';

        const float width = hudMeasurePromptText(line, scaleX, scaleY);
        hudDrawPromptText(centreX - width * 0.5f, top, line, scaleX, scaleY,
                          colour.r, colour.g, colour.b, colour.a, false);
        top += pitch;
        cursor = newline != NULL ? newline + 1 : NULL;
    }
}

}

void
hudDrawAdvicePanel(int bodyRow)
{
    if(!hudAdviceRowIsPanelBody(bodyRow) || !hudPromptLoad())
        return;

    hudDrawPanel(HUD_ADVICE_PANEL);

    const uint16_t headingId = hudAdviceUsesGenericHeading(bodyRow)
                                   ? HUD_ADVICE_HEADING_GENERIC_TEXT_ID
                                   : HUD_ADVICE_HEADING_TEXT_ID;
    drawCentredBlock(g_game.localizedText(headingId),
                     HUD_ADVICE_HEADING_X, HUD_ADVICE_HEADING_Y, 1.0f, 1.0f,
                     HUD_ADVICE_HEADING_COLOUR);

    const HudAdviceBody& row = HUD_ADVICE_BODY[bodyRow];
    drawCentredBlock(g_game.localizedText(row.textId), HUD_ADVICE_BODY_X,
                     HUD_ADVICE_BODY_Y, row.scaleX, row.scaleY,
                     HUD_ADVICE_BODY_COLOUR);
}

void
hudDrawAdvice(void)
{
    switch(hudAdvicePhase())
    {
    case HUD_ADVICE_PHASE_PANEL:

        hudDrawAdvicePanel(hudAdviceCurrentRow());
        break;
    case HUD_ADVICE_PHASE_BOTTOM_LINE:
        hudDrawBottomLine(hudAdviceCurrentRow());
        break;
    case HUD_ADVICE_PHASE_IDLE:
        break;
    }
}

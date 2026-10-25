#include "ui/hud/advice_sequence.h"

namespace
{

const int ADVICE_GROUP_ROW_CAPACITY = 4;

struct AdviceSequence
{
    HudAdvicePhase phase;
    int group;
    int rowCount;
    int rowIndex;
    int rows[ADVICE_GROUP_ROW_CAPACITY];
    int bottomRow;
    float remaining;
};

AdviceSequence g_advice = { HUD_ADVICE_PHASE_IDLE, -1, 0, 0, { 0 }, -1, 0.0f };

}

bool
hudAdviceRowIsPanelBody(int bodyRow)
{
    if(bodyRow < 0 || bodyRow >= HUD_ADVICE_BODY_COUNT)
        return false;
    return HUD_ADVICE_BODY[bodyRow].layoutFlag == 0;
}

int
hudAdviceBodyRows(int groupIndex, int* rows, int capacity)
{
    if(rows == NULL || capacity <= 0)
        return 0;
    int written = 0;
    for(int i = 0; i < HUD_ADVICE_BODY_COUNT && written < capacity; ++i)
        if(HUD_ADVICE_BODY[i].groupIndex == groupIndex &&
           HUD_ADVICE_BODY[i].layoutFlag == 0)
            rows[written++] = i;
    return written;
}

int
hudAdviceBottomLineRow(int groupIndex)
{
    for(int i = 0; i < HUD_ADVICE_BODY_COUNT; ++i)
        if(HUD_ADVICE_BODY[i].groupIndex == groupIndex &&
           HUD_ADVICE_BODY[i].layoutFlag == 1)
            return i;
    return -1;
}

void
hudAdviceReset(void)
{
    g_advice.phase = HUD_ADVICE_PHASE_IDLE;
    g_advice.group = -1;
    g_advice.rowCount = 0;
    g_advice.rowIndex = 0;
    g_advice.bottomRow = -1;
    g_advice.remaining = 0.0f;
}

void
hudAdviceShowGroup(int groupIndex)
{
    hudAdviceReset();
    g_advice.group = groupIndex;
    g_advice.rowCount = hudAdviceBodyRows(groupIndex, g_advice.rows,
                                          ADVICE_GROUP_ROW_CAPACITY);
    g_advice.bottomRow = hudAdviceBottomLineRow(groupIndex);
    if(g_advice.rowCount != 0)
    {
        g_advice.phase = HUD_ADVICE_PHASE_PANEL;
        return;
    }

    if(g_advice.bottomRow >= 0)
    {
        g_advice.phase = HUD_ADVICE_PHASE_BOTTOM_LINE;
        g_advice.remaining = HUD_ADVICE_BODY[g_advice.bottomRow].duration;
    }
}

void
hudAdviceConfirm(void)
{
    if(g_advice.phase == HUD_ADVICE_PHASE_IDLE)
        return;
    if(g_advice.phase == HUD_ADVICE_PHASE_PANEL &&
       g_advice.rowIndex + 1 < g_advice.rowCount)
    {
        ++g_advice.rowIndex;
        return;
    }
    if(g_advice.phase == HUD_ADVICE_PHASE_PANEL && g_advice.bottomRow >= 0)
    {
        g_advice.phase = HUD_ADVICE_PHASE_BOTTOM_LINE;
        g_advice.remaining = HUD_ADVICE_BODY[g_advice.bottomRow].duration;
        return;
    }

    if(g_advice.phase == HUD_ADVICE_PHASE_PANEL)
        hudAdviceReset();
}

void
hudAdviceUpdate(float seconds)
{
    if(g_advice.phase != HUD_ADVICE_PHASE_BOTTOM_LINE || !(seconds > 0.0f))
        return;
    g_advice.remaining -= seconds;
    if(g_advice.remaining <= 0.0f)
        hudAdviceReset();
}

HudAdvicePhase
hudAdvicePhase(void)
{
    return g_advice.phase;
}

bool
hudAdvicePanelActive(void)
{
    return g_advice.phase == HUD_ADVICE_PHASE_PANEL;
}

bool
hudAdviceBottomLineActive(void)
{
    return g_advice.phase == HUD_ADVICE_PHASE_BOTTOM_LINE;
}

int
hudAdviceCurrentRow(void)
{
    if(g_advice.phase == HUD_ADVICE_PHASE_PANEL)
        return g_advice.rows[g_advice.rowIndex];
    if(g_advice.phase == HUD_ADVICE_PHASE_BOTTOM_LINE)
        return g_advice.bottomRow;
    return -1;
}

uint16_t
hudAdviceCurrentTextId(void)
{
    const int row = hudAdviceCurrentRow();
    return row < 0 ? 0 : HUD_ADVICE_BODY[row].textId;
}

float
hudAdviceRemainingSeconds(void)
{
    return g_advice.phase == HUD_ADVICE_PHASE_BOTTOM_LINE ? g_advice.remaining
                                                          : 0.0f;
}

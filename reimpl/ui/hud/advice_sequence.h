#ifndef REIMPL_HUD_ADVICE_SEQUENCE_H
#define REIMPL_HUD_ADVICE_SEQUENCE_H

#include <stdint.h>

#include "formats/HUD_PROMPT/hud_prompt_layout.h"

enum HudAdvicePhase
{
    HUD_ADVICE_PHASE_IDLE,
    HUD_ADVICE_PHASE_PANEL,
    HUD_ADVICE_PHASE_BOTTOM_LINE
};

int hudAdviceBodyRows(int groupIndex, int* rows, int capacity);

int hudAdviceBottomLineRow(int groupIndex);

bool hudAdviceRowIsPanelBody(int bodyRow);

void hudAdviceShowGroup(int groupIndex);

void hudAdviceConfirm(void);

void hudAdviceUpdate(float seconds);

void hudAdviceReset(void);

HudAdvicePhase hudAdvicePhase(void);

bool hudAdvicePanelActive(void);

bool hudAdviceBottomLineActive(void);

int hudAdviceCurrentRow(void);

uint16_t hudAdviceCurrentTextId(void);

float hudAdviceRemainingSeconds(void);

#endif

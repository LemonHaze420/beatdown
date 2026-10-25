#ifndef REIMPL_MENU_PAUSE_H
#define REIMPL_MENU_PAUSE_H

#include <stdint.h>

#include "formats/MENU_LAYOUT/pause_results_layout.h"

struct PauseEquippedMoveSlot
{
    uint32_t type;
    uint16_t id;
    bool active;
};

void setPauseEquippedMoveSlots(const PauseEquippedMoveSlot& left,
                               const PauseEquippedMoveSlot& right);

void setResultScreenSelection(int selection);
int resultScreenSelection(void);
ResultChoiceAction resultScreenAction(void);

void drawPauseScreen(void);
void drawOptionsScreen(void);
void drawRecoveredResultsScreen(void);
void releaseMenuPauseResources(void);

#endif

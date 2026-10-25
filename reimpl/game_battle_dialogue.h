#ifndef GAME_BATTLE_DIALOGUE_H
#define GAME_BATTLE_DIALOGUE_H

#include <stdint.h>

struct BattleDialogueMessage
{
    uint16_t voice = 0;
    uint16_t text = 0;
};

BattleDialogueMessage battleDialogueMessage(int group, int event,
                                            bool fullHealth, uint8_t gender);

#endif

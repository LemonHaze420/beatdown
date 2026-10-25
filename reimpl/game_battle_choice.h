#ifndef GAME_BATTLE_CHOICE_H
#define GAME_BATTLE_CHOICE_H

#include <stdint.h>

const int BATTLE_CHOICE_OPTION_CAPACITY = 5;
struct BattleChoiceDescriptor
{
    int optionCount = 0;
    int cancelIndex = -1;
    bool enabled[BATTLE_CHOICE_OPTION_CAPACITY] = {};
    uint16_t textIds[BATTLE_CHOICE_OPTION_CAPACITY] = {};
};
struct BattleChoiceRuntime
{
    int phase = 0;
    uint32_t flags = 0;
    int selectedIndex = 0;
    int resultCode = -1;
    float phaseElapsedSeconds = 0;
    float inputElapsedSeconds = 0;
    float opacity = 0;
};
struct BattleChoiceInput
{
    float scaledDeltaSeconds = 0;
    bool pauseGuardEnabled = true;
    bool confirm = false;
    bool cancel = false;
    bool up = false;
    bool down = false;
};
struct BattleChoiceEffects
{
    int soundCue = -1;
    int result = -1;
};

bool initializeBattleChoice(BattleChoiceRuntime& runtime,
                            const BattleChoiceDescriptor& descriptor);
BattleChoiceEffects updateBattleChoice(BattleChoiceRuntime& runtime,
    const BattleChoiceDescriptor& descriptor, const BattleChoiceInput& input);

#endif

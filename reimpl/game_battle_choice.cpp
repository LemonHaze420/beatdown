#include "game_battle_choice.h"
#include <algorithm>

bool initializeBattleChoice(BattleChoiceRuntime& runtime,
                            const BattleChoiceDescriptor& descriptor)
{
    runtime = {};
    if(descriptor.optionCount <= 0 ||
       descriptor.optionCount > BATTLE_CHOICE_OPTION_CAPACITY)
        return false;
    for(int index = 0; index < descriptor.optionCount; ++index)
        if(descriptor.enabled[index])
        {
            runtime.selectedIndex = index;
            return true;
        }
    return false;
}

BattleChoiceEffects updateBattleChoice(BattleChoiceRuntime& runtime,
    const BattleChoiceDescriptor& descriptor, const BattleChoiceInput& input)
{
    BattleChoiceEffects effects;
    float elapsed = runtime.inputElapsedSeconds;
    bool admitInput = false;
    bool finished = false;
    switch(runtime.phase)
    {
    case 0:
        runtime.opacity = 0;
        elapsed = 0;
        ++runtime.phase;
        break;
    case 1:
        runtime.opacity = std::clamp(elapsed / 0.5f, 0.0f, 1.0f);
        if(elapsed > 0.5f)
        {
            elapsed = 0;
            ++runtime.phase;
        }
        break;
    case 2:
        runtime.opacity = 1;
        admitInput = (runtime.flags & 2u) == 0;
        if(!admitInput)
        {
            elapsed = 0;
            ++runtime.phase;
        }
        break;
    case 3:
        runtime.opacity = 1.0f - std::clamp(elapsed / 0.3f, 0.0f, 1.0f);
        if(elapsed > 0.3f)
        {
            elapsed = 0;
            ++runtime.phase;
        }
        break;
    case 4:
        runtime.opacity = 0;
        finished = true;
        break;
    default:
        break;
    }
    if(admitInput && input.pauseGuardEnabled)
    {
        bool confirm = input.confirm;
        bool up = input.up;
        bool down = input.down;
        if(input.cancel && descriptor.cancelIndex >= 0)
        {
            runtime.selectedIndex = std::min(descriptor.cancelIndex, descriptor.optionCount);
            confirm = true;
            up = down = false;
        }
        if(confirm)
        {
            runtime.flags |= 2u;
            effects.soundCue = 5;
        }
        else if(up || down)
        {
            int selected = runtime.selectedIndex;
            for(int count = 0; count < descriptor.optionCount; ++count)
            {
                selected += up ? -1 : 1;
                if(selected < 0)
                    selected = descriptor.optionCount - 1;
                else if(selected >= descriptor.optionCount)
                    selected = 0;
                if(descriptor.enabled[selected])
                {
                    runtime.selectedIndex = selected;
                    effects.soundCue = 8;
                    break;
                }
            }
        }
    }
    runtime.phaseElapsedSeconds += input.scaledDeltaSeconds;
    runtime.inputElapsedSeconds = elapsed + input.scaledDeltaSeconds;
    if(finished)
    {
        runtime.resultCode = runtime.selectedIndex;
        effects.result = runtime.resultCode;
    }
    return effects;
}

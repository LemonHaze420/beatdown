#include "game/battle/end.h"
#include <algorithm>

BattleEndEffects updateBattleEnd(BattleEndRuntime& runtime,
                                 const BattleEndInput& input)
{
    BattleEndEffects effects;
    switch(runtime.state)
    {
    case BATTLE_END_START:
        effects.initialize = true;
        runtime.elapsedSeconds = 0;
        runtime.state = BATTLE_END_DELAY;
        break;
    case BATTLE_END_DELAY:
        if(runtime.elapsedSeconds >= 3.0f)
        {
            effects.startNotice = true;
            runtime.elapsedSeconds = 0;
            runtime.state = BATTLE_END_WAIT_NOTICE;
        }
        break;
    case BATTLE_END_WAIT_NOTICE:
        if(!input.noticeTaskActive)
        {
            runtime.elapsedSeconds = 0;
            runtime.state = BATTLE_END_BEFORE_FADE;
        }
        break;
    case BATTLE_END_BEFORE_FADE:
        if(runtime.elapsedSeconds >= 0.8f)
        {
            effects.startFade = true;
            runtime.state = BATTLE_END_WAIT_FADE;
        }
        break;
    case BATTLE_END_WAIT_FADE:
        if(input.fadeReady)
        {
            effects.restore = true;
            runtime.state = BATTLE_END_COMPLETE;
        }
        break;
    case BATTLE_END_COMPLETE:
        effects.complete = true;
        return effects;
    }
    runtime.elapsedSeconds += input.scaledDeltaSeconds;
    effects.lockOccupiedActorActions = true;
    return effects;
}

void startBattleEndNotice(BattleEndNoticeRuntime& runtime)
{
    runtime = {};
    runtime.state = BATTLE_END_NOTICE_INITIALIZE;
}

void updateBattleEndNotice(BattleEndNoticeRuntime& runtime, float scaledSeconds)
{
    switch(runtime.state)
    {
    case BATTLE_END_NOTICE_INITIALIZE:
        runtime.progress = 0;
        runtime.opacity = 0;
        runtime.state = BATTLE_END_NOTICE_FADE_IN;
        break;
    case BATTLE_END_NOTICE_FADE_IN:
        runtime.progress += scaledSeconds * 2.0f;
        runtime.opacity = std::clamp(runtime.progress, 0.0f, 1.0f);
        if(runtime.progress > 1.0f)
        {
            runtime.progress = 3.0f;
            runtime.state = BATTLE_END_NOTICE_HOLD;
        }
        break;
    case BATTLE_END_NOTICE_HOLD:
        runtime.progress -= scaledSeconds;
        if(runtime.progress <= 0)
        {
            runtime.progress = 1.0f;
            runtime.state = BATTLE_END_NOTICE_FADE_OUT;
        }
        break;
    case BATTLE_END_NOTICE_FADE_OUT:
        runtime.progress -= scaledSeconds * 4.0f;
        runtime.opacity = std::clamp(runtime.progress, 0.0f, 1.0f);
        if(runtime.progress < 0)
            runtime.state = BATTLE_END_NOTICE_INACTIVE;
        break;
    case BATTLE_END_NOTICE_INACTIVE:
        break;
    }
}

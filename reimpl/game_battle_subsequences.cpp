#include "game.h"
#include "scene_task.h"

bool Game::updateBattleSpecial(void)
{

    BattleSubsequenceRuntime& runtime = battleController.special;
    GameSlotRecord* attacker = mutableCombatSlotByIndex(battleController.flow.attacker);
    GameSlotRecord* target = attacker != nullptr ?
        mutableCombatSlotByIndex(attacker->linkedSlotIndex) : nullptr;
    if(attacker == nullptr || target == nullptr)
        return false;
    const auto call = [this](BattleSequenceOperation operation, int actor = -1,
        int value = 0, int value2 = 0, int value3 = 0, float seconds = 0,
        uint32_t address = 0)
    {
        const BattleSequenceCommand command = {
            operation, actor, value, value2, value3, seconds, address };
        return consumeBattleSequenceCommand(battleController.sequence, command);
    };
    const auto advance = [&runtime]()
    {
        ++runtime.state;
        runtime.elapsedSeconds = 0;
        runtime.flags = 0;
    };
    switch(runtime.state)
    {
    case 0:
    {
        const int relation = battleRelation(*attacker, *target);
        if(relation < 0 || relation >= 5)
            return false;
        runtime.resources = BATTLE_FOLLOWUP_RESOURCES[1][relation];
        call(BATTLE_SEQUENCE_RESOURCE_START, -1, 0, 0, 0, 0, runtime.resources);
        call(BATTLE_SEQUENCE_SET_ACTION, 0, 0x79a, 0, 1);
        call(BATTLE_SEQUENCE_SET_ACTION, 1, 0x79b, 0, 1);
        call(BATTLE_SEQUENCE_SET_CAMERA_MODE, -1, 1, 0x1f);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 1);
        battleTimeScale = 0.85f;
        persistent.story.scratch005f333c = 0;
        advance();
        break;
    }
    case 1:
        if((runtime.flags & 1u) == 0 && call(BATTLE_SEQUENCE_RESOURCE_POLL,
                -1, 0, 0, 0, 0, runtime.resources))
            runtime.flags |= 1u;
        if(runtime.elapsedSeconds >= 1.5f)
            runtime.flags |= 0x10u;
        if((runtime.flags & 0x100u) == 0 && (runtime.flags & 0x10u) != 0)
        {
            call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 9, 0);
            runtime.flags |= 0x100u;
        }
        if((runtime.flags & 0x100u) != 0 && call(BATTLE_SEQUENCE_FLOW_READY))
            runtime.flags |= 0x200u;
        if((runtime.flags & 0x211u) == 0x211u)
            advance();
        break;
    case 2:
        call(BATTLE_SEQUENCE_REPOSITION, -1, 1);
        call(BATTLE_SEQUENCE_DIALOGUE, 1, 0xd);
        call(BATTLE_SEQUENCE_SET_ACTION, 0, 1, 1, 1);
        call(BATTLE_SEQUENCE_SET_ACTION, 1, 0x186, 0, 1);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);

        call(BATTLE_SEQUENCE_RETURN_WEAPON, 0);
        call(BATTLE_SEQUENCE_RETURN_WEAPON, 1);
        call(BATTLE_SEQUENCE_RESET_ATTACHMENTS);
        battleTimeScale = 1;
        persistent.story.scratch005f333c = 0;
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_SET_FADE, -1, 4, 7, -1);
        advance();
        break;
    case 3:
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        if(runtime.elapsedSeconds >= 3 && !sceneEventSoundChannelActive[2])
            advance();
        break;
    case 4:
        call(BATTLE_SEQUENCE_RESET_CAMERA);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);

        battleController.sceneTransitionActive = true;
        battleController.sceneTransitionComplete = false;
        battleController.sceneTransitionKind = 2;
        battleController.sceneTransitionArguments[0] = target->resourceProfileId;
        persistent.story.gameStateFlags |= 0x8000000u;
        runtime.delaySeconds = 0;
        advance();
        break;
    case 5:
        if((runtime.flags & 1u) == 0)
        {
            if(call(BATTLE_SEQUENCE_SCENE_TRANSITION_COMPLETE))
            {
                call(BATTLE_SEQUENCE_STOP_SCENE_TRANSITION);
                runtime.delaySeconds = 0.14f;
                runtime.flags |= 1u;
            }
        }
        else
        {
            runtime.delaySeconds -= battleController.rawDeltaSeconds;
            if(runtime.delaySeconds <= 0)
            {
                runtime.delaySeconds = 0;
                advance();
            }
        }
        break;
    case 6:
        advance();
        break;
    case 7:
        return true;
    default:
        return false;
    }
    runtime.elapsedSeconds += battleController.rawDeltaSeconds;
    return false;
}

bool Game::updateBattleCinematic(void)
{

    BattleSubsequenceRuntime& runtime = battleController.cinematic;
    GameSlotRecord* attacker = mutableCombatSlotByIndex(battleController.flow.attacker);
    GameSlotRecord* target = attacker != nullptr ?
        mutableCombatSlotByIndex(attacker->linkedSlotIndex) : nullptr;
    if(attacker == nullptr || target == nullptr)
        return false;
    const auto call = [this](BattleSequenceOperation operation, int actor = -1,
        int value = 0, int value2 = 0, int value3 = 0)
    {
        const BattleSequenceCommand command = {
            operation, actor, value, value2, value3, 0, 0 };
        return consumeBattleSequenceCommand(battleController.sequence, command);
    };
    const auto advance = [&runtime](int state)
    {
        runtime.state = state;
        runtime.elapsedSeconds = 0;
    };
    switch(runtime.state)
    {
    case 0:
    case 1:
        advance(runtime.state + 1);
        break;
    case 2:
        call(BATTLE_SEQUENCE_DIALOGUE, 1, 6);
        call(BATTLE_SEQUENCE_SET_ACTION, 0, 1, 1, 1);
        call(BATTLE_SEQUENCE_SET_ACTION, 1, 0x186, 0, 1);
        call(BATTLE_SEQUENCE_CAMDAT, 1, 0x39);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 1);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
        if((persistent.story.playerConditionFlags & 1u) == 0 &&
           nextGameRandom(random) % 100u > 69)
        {
            runtime.variant |= 1;
            if(!getStorySaveFlag(persistent.story, 0x38))
            {
                setStorySaveFlag(persistent.story, 0x38, true);
                setStorySaveFlag(persistent.story, 0x39, true);
            }
        }
        advance(3);
        break;
    case 3:
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        if(runtime.elapsedSeconds >= 4)
            advance(4);
        break;
    case 4:
        call(BATTLE_SEQUENCE_RESET_CAMERA);
        if((runtime.variant & 1) == 0)
        {
            call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 6, 0);
            runtime.state = 200;
        }
        else
            runtime.state = 100;
        break;
    case 100:
        call(BATTLE_SEQUENCE_RESET_CAMERA);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
        battleController.conditionOpponentCode = 0xb;
        battleController.conditionOpponentCharacterId = attacker->nativeCharacterId;
        persistent.story.playerConditionFlags |= 1u;
        trackedOpponentCondition = true;
        temporarySlowActive = false;
        battleController.cinematicPromptVisible = true;
        advance(101);
        break;
    case 101:
        if(runtime.elapsedSeconds >= 0.6f &&
           (heldInput & (GAME_INPUT_CONFIRM | GAME_INPUT_GUARD)) != 0)
        {
            GameSoundEvent event = {};
            event.kind = GAME_SOUND_EVENT_EVENT_CUE;
            event.cue = 0xe;
            enqueueSoundEvent(event);
            advance(102);
        }
        break;
    case 102:
        if(runtime.elapsedSeconds >= 0.25f)
        {
            setSceneCharacterSelectCommand(3);
            advance(200);
        }
        break;
    case 200:
        if(sceneCharacterSelectReady(1))
        {
            battleController.cinematicPromptVisible = false;
            call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
            call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 1);
            advance(201);
        }
        break;
    case 201:
        return true;
    default:
        return false;
    }
    runtime.elapsedSeconds += battleController.rawDeltaSeconds;
    return false;
}

#include "game.h"
#include "game_battle_selection.h"
#include "game_combat_links.h"
#include "character_records.h"

int Game::selectBattleSequenceAction(void)
{
    const BattleFlowRuntime& flow = battleController.flow;
    GameSlotRecord* attacker = mutableCombatSlotByIndex(flow.attacker);
    GameSlotRecord* target = attacker != nullptr ?
        mutableCombatSlotByIndex(attacker->linkedSlotIndex) : nullptr;
    if(attacker == nullptr || target == nullptr)
        return 0;
    BattleActionSelectionInput input;
    if(stageNavigation.combatSurface != nullptr)
    {
        GameCombatSurfaceContact contact;
        if(stageNavigation.combatSurface(stageNavigation.combatSurfaceContext,
                                         target->position, 40.0f, &contact))
        {
            const unsigned stage = selectedStageId();
            const unsigned material = contact.material + 18u;
            if(stage < COMBAT_SURFACE_STAGE_COUNT && material < COMBAT_SURFACE_MATERIAL_COUNT)
            {
                target->combatSurfaceContact = contact;
                target->combatSurfaceValid = true;
                target->combatSurfaceId = COMBAT_SURFACE_PROPERTIES[stage][material] & 0x3ffu;
                input.surfaceContact = true;
            }
        }
    }
    input.armed = (attacker->actorActionFlags & 0x4000u) != 0;
    for(int index = 0; index < 2; ++index)
    {
        input.moveSlots[index] = attacker->moveSlots[index];
        input.moveSlotActive[index] = attacker->moveSlotActive[index];
    }
    return ::selectBattleAction(battleController.sequence, input, random);
}

int Game::battleSequencePrideDamage(void)
{
    const BattleSequenceRuntime& sequence = battleController.sequence;
    const GameSlotRecord* attacker = combatSlotByIndex(battleController.flow.attacker);
    const GameSlotRecord* target = attacker != nullptr ?
        combatSlotByIndex(attacker->linkedSlotIndex) : nullptr;
    if(attacker == nullptr || target == nullptr)
        return 0;
    const CharacterRecord* defaults = characterRecordForId(
        static_cast<uint16_t>(target->characterId));
    if(defaults == nullptr)
        return 0;
    BattleOutcomePrideInput input;
    input.phase = sequence.battleMode;
    input.outcome = sequence.outcomeMode;
    input.reactionType = sequence.reactionType;
    input.usesWeapon = sequence.actionVariant != 0;
    input.characterModifiers[0] = defaults->outcomePrideModifiers[0];
    input.characterModifiers[1] = defaults->outcomePrideModifiers[1];
    const int slot = sequence.actionSelector;
    if(input.usesWeapon && slot < 2)
    {
        const WeaponInstance* instance = weaponManager.instance(
            ensureCombatWorldWeapon(attacker->ownedWorldWeapons[slot]));
        const WeaponDefinition* definition = instance != nullptr ?
            weaponManager.definition(instance->objectId) : nullptr;
        if(definition != nullptr)
            input.weaponClass = definition->rawWords[32] & 0xffu;
    }
    return battleOutcomePrideDamage(input, random);
}

bool Game::prepareBattlePresentation(bool confirmationOnly)
{
    BattleSequenceRuntime& sequence = battleController.sequence;
    battleController.choiceVisible = confirmationOnly || (sequence.flags & 2u) != 0;
    battleController.choiceDescriptor = {};
    if(!battleController.choiceVisible)
        return true;
    BattleChoiceDescriptor& descriptor = battleController.choiceDescriptor;
    const bool confirm = confirmationOnly || sequence.battleMode == 3;
    descriptor.optionCount = confirm ? 2 : 5;
    descriptor.cancelIndex = -1;
    if(confirm)
    {
        descriptor.textIds[0] = BATTLE_SEQUENCE_TEXT_IDS[9];
        descriptor.textIds[1] = BATTLE_SEQUENCE_TEXT_IDS[10];
        descriptor.enabled[0] = descriptor.enabled[1] = true;
    }
    else
    {
        for(int index = 0; index < 4; ++index)
            descriptor.textIds[index] = BATTLE_SEQUENCE_TEXT_IDS[5 + index];
        descriptor.textIds[4] = BATTLE_SEQUENCE_TEXT_IDS[11];
        for(int index = 0; index < 5; ++index)
            descriptor.enabled[index] = true;
        const bool restricted = (battleController.encounterFlags & 2u) != 0;
        descriptor.enabled[0] = restricted || battleController.choiceGate_0x5f3dac == 0;
        descriptor.enabled[1] = descriptor.enabled[2] = !restricted;
        descriptor.enabled[3] = !restricted && (battleController.encounterFlags & 0x20u) == 0;
    }
    return initializeBattleChoice(battleController.choice, descriptor);
}

bool Game::updateBattlePresentation(void)
{
    if(!battleController.choiceVisible)
        return true;
    BattleChoiceInput input;
    input.scaledDeltaSeconds = scaledFrameDeltaSeconds;
    input.pauseGuardEnabled = battleController.pauseGuardEnabled;
    input.confirm = (pressedInput & (GAME_INPUT_CONFIRM | GAME_INPUT_GUARD)) != 0;
    input.cancel = (pressedInput & (GAME_INPUT_CANCEL | GAME_INPUT_GRAB)) != 0;
    input.up = (pressedInput & GAME_INPUT_UP) != 0;
    input.down = (pressedInput & GAME_INPUT_DOWN) != 0;
    const BattleChoiceEffects effects = updateBattleChoice(battleController.choice,
        battleController.choiceDescriptor, input);
    if(effects.soundCue >= 0)
    {
        GameSoundEvent event = {};
        event.kind = GAME_SOUND_EVENT_EVENT_CUE;
        event.cue = static_cast<uint8_t>(effects.soundCue);
        enqueueSoundEvent(event);
    }
    if(effects.result < 0)
        return false;
    battleController.sequence.pendingOutcomeMode = effects.result;
    battleController.sequence.choiceResult = effects.result;
    return true;
}

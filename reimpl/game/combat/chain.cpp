#include "game/game.h"
#include "game/combat/input.h"
#include "game/internal.h"
#include "character_records.h"

const uint8_t* Game::combatTrainingLevels(const GameSlotRecord& slot) const
{

    if(currentMode == GAME_MODE_VERSUS_NORMAL)
        return game_detail::NORMAL_VERSUS_TRAINING_LEVELS;
    const GameSaveImage& image = currentMode == GAME_MODE_VERSUS_CUSTOM &&
        secondaryProfileLoaded && slot.slotIndex != 0 ?
        secondarySaveImage : primarySaveImage;
    return image.payload.coreGameStateBlock + GAME_SAVE_CORE_TRAINING_LEVELS_OFFSET;
}

MovePersonalActionContext
Game::combatPersonalActionContext(const GameSlotRecord& actor) const
{
    const int style = combatStyle(actor);
    MovePersonalActionContext personalContext = {};
    personalContext.levels = combatTrainingLevels(actor);
    personalContext.archetype =
        style >= 0 && style < MOVE_CHARACTER_COUNT ?
            MOVE_CHARACTER_RECORDS[style].archetype : -1;
    personalContext.playerIndex = actor.playerIndex;
    personalContext.resourceProfileId = actor.resourceProfileId;
    personalContext.characterIdZero = actor.nativeCharacterId == 0;
    personalContext.secondaryGameState =
        (persistent.story.gameStateFlags & 0x2u) != 0;
    personalContext.storySpecial =
        currentMode == GAME_MODE_STORY &&
        !personalContext.secondaryGameState &&
        actor.playerIndex > 1 &&
        actor.linkedSlotIndex == 0 &&
        (actor.flags & 1u) != 0 &&
        persistent.story.progression.phase >
            STORY_PROGRESSION_PHASE_CHAPTER_1 &&
        persistent.story.progression.phase <
            STORY_PROGRESSION_PHASE_FINALE &&
        persistent.story.subBattleId == 0;
    personalContext.interactionRecovery =
        (persistent.story.battleFlags &
         BATTLE_REWARD_INTERACTION_FLAG) != 0 &&
        personalContext.characterIdZero &&
        actor.currentHealth >
            static_cast<int>(static_cast<float>(actor.maxHealth) * 0.15f);
    return personalContext;
}

int combatWeaponChainSelector(int behavior, int category, int objectId)
{
    switch(behavior)
    {
    case 14: return 8;
    case 17: return category == 23 ? 23 : 7;
    case 6:
        if(category == 26) return 27;
        if(category == 27) return 24;
        return category == 13 ? 9 : 5;
    case 10:
        if(category == 10) return 19;
        return category == 19 || category == 7 || category == 3 ? 4 : 3;
    case 5: return category == 36 ? 26 : 25;
    case 4: return category == 11 || category == 18 ? 18 : 17;
    case 3:
        return category == 26 || category == 21 || category == 7 || category == 4 ? 16 : 15;
    case 13:
    case 2:
        if(objectId == 31)
        {
            if(category == 22 || category == 26) return 22;
            return category == 13 ? 21 : 20;
        }
        return category == 34 || category == 26 || category == 17 || category == 7 || category == 2 ? 12 : 11;
    case 1:
        if(category == 34) return 14;
        if(category == 14) return 13;
        if(category == 26 || category == 20 || category == 7 || category == 5) return 10;
        return category == 36 || category == 33 || category == 6 ? 2 : 1;
    default: return 0;
    }
}

void
Game::recordCombatInputCommand(GameSlotRecord& slot, int command, bool fresh)
{
    if(!fresh || command < 0)
        return;
    CombatCommandRuntime& input = slot.combatCommand;
    input.command = static_cast<int16_t>(command);
    if(command == 0x4b)
        input.bufferedCount = 0;
    if((slot.actorBehaviorFlags & 6u) != 0)
        ++input.bufferedCount;
    else
    {
        input.bufferedCount = 0;
        input.actionCount = 1;
        input.actions[0] = static_cast<int16_t>(command);
    }
}

int
Game::combatInputMove(GameSlotRecord& slot, int command,
                       bool facingTarget, float chargeSeconds)
{
    if(command == 0x4c || command == 0x4d)
        return combatOwnedWeaponInputMove(slot, command);
    bool armedInput = false;
    const int armedMove = combatWeaponInputMove(slot, command, armedInput);
    if(armedInput)
    {
        slot.combatCommand.preserveInputFacing = true;
        return armedMove;
    }
    if(command >= 0)
    {
        const int personal = movePersonalActionForStyle(
            combatStyle(slot), command, combatPersonalActionContext(slot));
        if(personal > 0)
            return personal;
    }
    const int action = moveForAction(command);
    return action > 0 ? stanceMotion(slot, action) : -1;
}

bool
Game::consumeCombatInputChain(GameSlotRecord& slot, int argument)
{

    CombatCommandRuntime& input = slot.combatCommand;
    const PersistentGameRecord* record = characterRecord(
        static_cast<uint16_t>(slot.persistentRecordId));
    const int limit = record != nullptr ? record->field_0x384 + 1 : 0;
    if(slot.comboStep == 0 || slot.comboStep == limit ||
       input.bufferedCount == 0 || (slot.actorActionFlags & 4u) != 0)
        return false;
    if(input.command < 0 || input.actionCount >= MOVE_ACTION_CHAIN_CAPACITY)
    {
        --input.bufferedCount;
        return false;
    }

    int command = input.command;
    bool allowSingleFallback =
        (command >= 0x21 && command <= 0x28) || command == 0x39 ||
        command == 0x3a || (command >= 0x84 && command <= 0x87);
    if(command == 0x2f && (slot.actorTargetingFlags & 0x180000u) != 0)
    {
        const GameSlotRecord* linked = combatSlotByIndex(slot.linkedSlotIndex);
        if(linked != nullptr)
        {
            bool grounded;
            if(!combatGroundedState(*linked, grounded))
                return false;
            command = moveThrowBaseAction(grounded, (slot.comboStep & 1u) != 0);
            allowSingleFallback = true;
        }
    }
    input.actions[input.actionCount++] = static_cast<int16_t>(command);
    const MovePersonalActionContext context = combatPersonalActionContext(slot);
    const int style = combatStyle(slot);
    const int personalId = style >= 0 && style < MOVE_CHARACTER_COUNT ?
        MOVE_CHARACTER_RECORDS[style].personalMovesetId : 0;
    const WeaponInstance* held = (slot.actorActionFlags & 0x4000u) != 0 ?
        weaponManager.equippedBy(slot.slotIndex) : nullptr;
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    const bool weapon = definition != nullptr &&
        definition->rawWords[1] != 0 && definition->rawWords[1] != 0xbu;
    int move = 0;
    if(weapon)
    {
        const int selector = combatWeaponChainSelector(
            definition->behaviorId, personalId, held->objectId);
        if(selector > 0 && weaponManager.canUse(held->managerId))
            move = moveActionChain(MOVE_WEAPON_CHAIN_FAMILIES[selector],
                input.actions, input.actionCount, context);
    }
    else if(personalId > 0 && personalId <= MOVE_LIST_STYLE_COUNT)
    {
        move = moveActionChain(MOVE_PERSONAL_CHAIN_FAMILIES[personalId],
            input.actions, input.actionCount, context);
        if(move == 0 && allowSingleFallback)
            move = movePersonalActionForStyle(style, command, context);
    }
    if(move > 0)
    {
        queueCombatPair(slot, move, moveRecord(move)->linkedMoveId);
        ++slot.comboStep;
        if(weapon)
            --input.bufferedCount;
        if(argument != 0 && slot.comboStep > 2)
            slot.comboStep = 1;
        return true;
    }
    --input.bufferedCount;
    return false;
}

int Game::prepareCombatInputAction(
    GameSlotRecord& slot, int command, int move)
{
    prepareCombatOwnedWeaponInput(slot, command, move);
    prepareCombatWeaponInput(slot, command, move);
    if(command == 0x1e)
    {
        const int pickup = selectCombatWorldItem(slot);
        if(pickup > 0)
            return pickup;
    }
    if((command == 0x4c || command == 0x4d) &&
       (slot.actorActionFlags & 0x4000u) == 0)
    {
        slot.selectedEquippedSlot = static_cast<uint8_t>(command - 0x4c);
        slot.selectedWorldItemIndex = -1;
    }
    return move;
}

#include "game/game.h"
#include "game/combat/gauge.h"
#include "ui/menu/inventory.h"
#include "scene/task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera/camera.h"
#include "sound_cues.h"
#include "stage/stage.h"
#include "ui/hud/advice.h"
#include "formats/MENU_LAYOUT/pause_results_layout.h"
#include "formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game/internal.h"

using namespace game_detail;

void setGameSlotSelectedAction(GameSlotRecord& slot, int selectedActionId,
                               bool restartStateTask)
{
    const MoveRecord* record = moveRecord(selectedActionId);
    const int16_t actionId = record != NULL ?
        static_cast<int16_t>(selectedActionId) : -1;
    const bool changed = slot.selectedActionId != actionId;
    slot.selectedActionId = actionId;
    slot.actorBehaviorFlags = record != NULL ? record->behaviorFlags : 0;
    if(!changed && !restartStateTask)
        return;

    slot.actionElapsedSeconds = 0.0f;

    slot.motionControlFlags = (slot.motionControlFlags & ~0x1000000u) |
        ((slot.actorStatusFlags & 0x400000u) << 2);
    slot.actorStatusFlags = 0;
    slot.poseStateWord = 0;
    slot.combatRecoveryState = 0;
    slot.receivedDamageTaskActive = false;
    if(actionId != 4)
        slot.pendingReceivedDamage = false;

    slot.actorActionFlags &= 0x001df07cu;
    if((slot.actorActionFlags & 0x10u) != 0)
        slot.actorActionFlags |= 0x80u;
    slot.actorTargetingFlags &= 0x00a41010u;

    const int handler = actionStateHandler(actionId);
    slot.stateHandler = static_cast<int8_t>(handler);
    slot.statePhase = handler >= 0 ? 1 : 0;
    memset(slot.stateWork, 0, sizeof(slot.stateWork));
    slot.inputHold.endFrame = 0;
    slot.combatCommand.command = -1;
    if((slot.actorBehaviorFlags & 2u) == 0)
        slot.combatCommand.bufferedCount = 0;
}

bool queueGameSlotCommandFollowUp(GameSlotRecord& slot, int commandIndex)
{
    const int moveId = moveCommandFollowUp(commandIndex);
    if(moveId < 0)
        return false;
    slot.queuedMoveId = static_cast<int16_t>(moveId);
    return true;
}

int
Game::combatStanceCategory(const GameSlotRecord& slot) const
{

    const CharacterRecord* record =
        characterRecordForId(static_cast<uint16_t>(slot.characterId));
    if(record == NULL || record->combatStyleId >= MOVE_CHARACTER_COUNT)
        return 0;
    return MOVE_CHARACTER_RECORDS[record->combatStyleId].personalMovesetId;
}

int
Game::stanceMotion(const GameSlotRecord& slot, int actionIndex) const
{

    const CharacterRecord* const record =
        characterRecordForId(static_cast<uint16_t>(slot.characterId));
    if(record == NULL)
        return -1;
    return resolveCharacterMove(record->combatStyleId, -actionIndex);
}

int
Game::selectCombatantStanceAction(const GameSlotRecord& slot) const
{

    const int category = combatStanceCategory(slot);
    const bool targeted = (slot.actorTargetingFlags & 0x40000u) != 0;
    bool generic = true;
    int action;

    if((slot.actorActionFlags & 0x4000u) == 0)
    {
        if(slot.currentHealth == slot.maxHealth && slot.damageGauge == 0)
            action = stanceMotion(slot, 0x149);
        else if(!targeted)
        {

            const GameSlotRecord* const linked =
                combatSlotByIndex(slot.linkedSlotIndex);
            const bool linkedTargeted = linked != NULL &&
                (linked->actorTargetingFlags & 0x40000u) != 0;
            action = stanceMotion(slot, linkedTargeted ? 0x149 : 0x14b);
        }
        else
        {
            action = stanceMotion(slot, 0x14c);
            generic = false;
        }
    }
    else
    {

        const WeaponInstance* held = weaponManager.equippedBy(slot.slotIndex);
        const WeaponDefinition* definition = held != NULL ?
            weaponManager.definition(held->objectId) : NULL;
        const int code = definition != NULL ? definition->behaviorId : 0x12;
        const int motion = code >= 0 && code < 24 ?
            COMBAT_ARMED_STANCE_MOTIONS[code] : COMBAT_ARMED_STANCE_MOTIONS[0x12];
        action = stanceMotion(slot, motion);
        if(targeted && code == 0x0b)
            action = 0x45c;
        else if(targeted && code == 0x0a)
            action = 0x28d;
        else if(code == 1 && (category == 7 || category == 14))
            action = 0x71;
        generic = false;
    }

    if((slot.flags & 0x2000u) != 0)
        return action;

    if(generic && category >= 1 && category <= 8)
        action = 0x790 + category;

    if(generic)
    {
        switch(slot.resourceProfileId)
        {
        case 0x98: action = 0x69e; break;
        case 0x6e: action = 0x401; break;
        case 0x6d: action = 0x69f; break;
        case 0x6a: action = 0x5d9; break;
        case 0x3c: action = 0x368; break;
        case 0x4e: action = 0x5da; break;
        case 0x48:
        case 0x4d: action = 0x5db; break;
        case 0x4c: action = 0x43f; break;
        case 0x49:
        case 0x4b: action = 0x5cf; break;
        case 0x4a: action = 0x45b; break;
        case 0x9d:
        case 0x6c: action = 0x3df; break;
        case 0x9c:
        case 0x6b: action = 0x3e2; break;
        case 0x99:
        case 0x44: action = 0x3d3; break;
        case 0x9a:
        case 0x35: action = 0x3e1; break;
        case 0x39: action = 0x3d4; break;
        case 0xb9:
        case 0x3f: action = 0x297; break;
        case 0x9b:
        case 0x16: action = 0x3e0; break;
        default: break;
        }
    }
    return action;
}

void
Game::startLinkedCombatantAction(GameSlotRecord& slot)
{

    const int linkedMove = actionStateLinkedMoveId(slot.selectedActionId);
    if(linkedMove == 0)
        return;
    GameSlotRecord* const partner =
        mutableCombatSlotByIndex(slot.linkedSlotIndex);
    if(partner == NULL || partner == &slot)
        return;
    if(moveRecord(linkedMove) == NULL)
        return;
    startCombatMove(*partner, linkedMove, partner->action);

    runCombatStateTask(*partner);
}

void
Game::applyInitialCombatantStance(GameSlotRecord& slot)
{

    if((slot.flags & 0x1u) == 0)
        return;
    const int action = selectCombatantStanceAction(slot);
    if(action <= 0 || moveRecord(action) == NULL)
        return;
    startCombatMove(slot, action, slot.action);
    setLocomotionMove(slot, action);
}

void
Game::updateCombatantStance(GameSlotRecord& slot)
{

    if(scaledFrameDeltaSeconds == 0.0f)
        return;
    if((slot.flags & 0x1u) == 0)
        return;

    if(slot.stateHandler != COMBAT_STATE_UPDATE_COMBATANT_DECISION)
        return;
    const int action = selectCombatantStanceAction(slot);

    if(action <= 0 || action == slot.appliedMotionId)
        return;
    if(moveRecord(action) == NULL)
        return;
    startCombatMove(slot, action, slot.action);

    runCombatStateTask(slot);

    setLocomotionMove(slot, action);
}

void
Game::runCombatStateTask(GameSlotRecord& slot)
{

    if(scaledFrameDeltaSeconds == 0.0f)
        return;
    if(slot.stateHandler == COMBAT_STATE_UPDATE_COMBATANT_AIRBORNE_MOVEMENT)
    {
        updateCombatSurfaceMovement(slot);
        return;
    }
    if(slot.stateHandler == COMBAT_STATE_UPDATE_COMBAT_ACTION_TASK ||
       slot.stateHandler == COMBAT_STATE_UPDATE_COMBATANT_CROUCHED_GUARD)
    {
        updateCombatGuard(slot, slot.stateHandler ==
            COMBAT_STATE_UPDATE_COMBATANT_CROUCHED_GUARD);
        return;
    }
    if(slot.stateHandler == COMBAT_STATE_APPLY_COMBAT_DAMAGE_AND_REACTION &&
       slot.statePhase == 1 && slot.pendingReceivedDamage)
    {
        applyPendingCombatReceivedDamage(slot);
        return;
    }
    const uint32_t recoveryPressed = sampleCombatRecoveryInput(slot);
    if(slot.stateHandler == COMBAT_STATE_UPDATE_COMBATANT_INPUT_HOLD)
    {
        updateCombatInputHold(slot, recoveryPressed);
        return;
    }
    if(slot.statePhase != 1)
    {
        updateCombatStatePhases(slot, false);

        if(slot.action == COMBAT_ACTION_DODGE &&
           slot.combatPosePublished && slot.scheduledMotionComplete)
            startAction(slot, COMBAT_ACTION_IDLE);
        return;
    }
    if(slot.stateHandler < 0)
        return;

    if(slot.stateHandler != COMBAT_STATE_UPDATE_COMBATANT_HEALTH_TIER_ACTION)
        setCombatActorMotion(slot, slot.selectedActionId);
    slot.appliedMotionId = slot.selectedActionId;
    slot.animationCueFlags = 0;
    slot.actorTargetingFlags &= 0xffffffefu;
    switch(slot.stateHandler)
    {
    case COMBAT_STATE_COMPLETE_COMBATANT_ANIMATION:
    {
        const int remapped = stanceMotion(slot, 0x1b5);
        slot.stateWork[0] =
            slot.appliedMotionId == remapped || slot.appliedMotionId == 0x2dd ? 0 : 1;
        break;
    }
    case COMBAT_STATE_UPDATE_COMBATANT_REPEAT_DELAY:
    case COMBAT_STATE_UPDATE_COMBATANT_BATTLE_TARGET_REQUEST:
    case COMBAT_STATE_QUEUE_COMBATANT_FOLLOWUP_ACTION:
    case COMBAT_STATE_RESOLVE_COMBATANT_LANDING_DAMAGE:

        break;
    case COMBAT_STATE_RESTORE_COMBATANT_ACTION:

        break;
    case COMBAT_STATE_APPLY_COMBATANT_ATTACK_FLAG:

        slot.actorActionFlags |= 1u;
        break;
    case COMBAT_STATE_APPLY_COMBATANT_GAUGE_MODIFIER:

        slot.comboStep = 0;
        applyCombatGaugeModifier(slot);
        slot.actorActionFlags &= 0xffffff07u;
        slot.actorTargetingFlags |= 0x200u;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_INPUT_HOLD:

        break;
    case COMBAT_STATE_UPDATE_COMBATANT_MOVEMENT:

        slot.actorActionFlags |= 1u;
        slot.actorActionFlags &= 0xfffe7fffu;
        slot.actorTargetingFlags &= 0xff5fffefu;
        slot.comboStep = 0;
        slot.velocity = {};
        break;
    case COMBAT_STATE_APPLY_COMBAT_DAMAGE_AND_REACTION:

        slot.actorActionFlags |= 1u;
        break;
    case COMBAT_STATE_APPLY_COMBATANT_THROW_FACING:

        slot.actorTargetingFlags |= 0x10u;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_DECISION:
        slot.inputHold.accumulatedPresses = 0;
        combatInputHoldSuccessCount = 0;

        slot.poseStateWord |= 1u;
        slot.actorActionFlags |= 1u;
        slot.actorActionFlags &= 0xfffe4fffu;
        slot.actorTargetingFlags &= 0xff5fffefu;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_IDLE:

        slot.actorActionFlags |= 1u;
        slot.actorActionFlags &= 0xfffe7fffu;
        slot.actorTargetingFlags &= 0xff5fffefu;
        break;
    case COMBAT_STATE_START_LINKED_COMBATANT_ACTION:

        startLinkedCombatantAction(slot);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_AIRBORNE_SPIN:

        startLinkedCombatantAction(slot);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_STAND_UP:

        slot.actorStatusFlags |= 8u;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_THROW_RELEASE:

        slot.actorActionFlags |= 1u;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_HIT_RECOIL:

        slot.actorActionFlags |= 1u;
        slot.actorActionFlags &= 0xfffe7fffu;
        slot.actorTargetingFlags &= 0xff5fffefu;
        slot.actorBehaviorFlags = 1u;
        slot.poseStateWord = 1u;
        break;
    case COMBAT_STATE_RESET_COMBAT_ACTION_TASK:
        slot.inputHold.accumulatedPresses = 0;
        combatInputHoldSuccessCount = 0;

        slot.poseStateWord |= 3u;
        slot.actorActionFlags |= 1u;
        slot.actorActionFlags &= 0xfffe4fffu;
        slot.actorTargetingFlags &= 0xff5fffefu;
        break;
    case COMBAT_STATE_INITIALIZE_COMBATANT_DEFEAT_ACTION:

        slot.currentHealth = 0;
        slot.actorActionFlags |= 4u;
        if(GameSlotRecord* linked = mutableCombatSlotByIndex(slot.linkedSlotIndex))
            linked->actorActionFlags |= 4u;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_TARGET_REQUEST:

        slot.actorActionFlags &= 0xffffff07u;
        slot.actorTargetingFlags |= 0x200u;
        slot.comboStep = 0;
        slot.tiredGauge = slot.tiredGaugeMaximum;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_BLOCK_RECOIL:
        initializeCombatRecovery(slot);
        forceDropCombatWeapon(slot, true);

        break;
    case COMBAT_STATE_UPDATE_COMBATANT_GET_UP:

        break;
    case COMBAT_STATE_START_COMBATANT_CURRENT_ACTION_ANIMATION:

        break;
    case COMBAT_STATE_UPDATE_COMBATANT_WAKE_ROLL:

        break;
    case COMBAT_STATE_UPDATE_COMBATANT_HEALTH_TIER_ACTION:
    {

        const int variant = slot.selectedActionId == 0x15 ? 1 : 0;
        const float maximum = static_cast<float>(slot.maxHealth);
        const int16_t health = slot.currentHealth;
        int tier = 3;
        if(health > static_cast<int16_t>(maximum * 0.75f))
            tier = 0;
        else if(health > static_cast<int16_t>(maximum * 0.5f))
            tier = 1;
        else if(health > static_cast<int16_t>(maximum * 0.05f))
            tier = 2;
        const int motion = GAME_COMBAT_STATE_HEALTH_TIER_MOTIONS[variant][tier];
        if(moveRecord(motion) != NULL)
            setCombatActorMotion(slot, motion);
        break;
    }
    case COMBAT_STATE_UPDATE_COMBATANT_WEAPON_SWING:

        if(slot.selectedActionId != moveForAction(1))
        {
            slot.statePhase = 99;
            return;
        }
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_APPROACH:
    case COMBAT_STATE_UPDATE_COMBAT_ACTION_TASK:
    case COMBAT_STATE_UPDATE_COMBATANT_DAMAGE_ACTION:

        slot.actorActionFlags |= 1u;
        break;
    }
    ++slot.statePhase;
    updateCombatStatePhases(slot, true);
}

void
Game::startAction(GameSlotRecord& slot, CombatAction action,
                   bool selectNativeAction)
{
    const bool changed = slot.action != action;

    if(changed && action != COMBAT_ACTION_MOVE)
    {

        slot.behaviourDecisionHistoryMask |=
            slot.behaviourDecisionMask;
        slot.behaviourDecisionMask = 0;
        slot.actorActionFlags &= 0x001df07cu;
        if((slot.actorActionFlags & 0x10u) != 0)
            slot.actorActionFlags |= 0x80u;

        slot.actorTargetingFlags &= 0x00a41010u;
    }
    if(changed && (action == COMBAT_ACTION_IDLE ||
                   action == COMBAT_ACTION_MOVE))
    {
        slot.recoveredMoveId = -1;

        slot.scheduledMotionValid = false;
        slot.scheduledMotionComplete = false;
        slot.combatPosePublished = false;
        slot.playingAnimationFrame = 0.0f;
    }
    slot.action = action;
    slot.activeHitBlock = 0;
    slot.hitChannelMask = 0;
    if(action != COMBAT_ACTION_MOVE)
        slot.poseStateWord = 0;
    if(action != COMBAT_ACTION_MOVE && action != COMBAT_ACTION_DODGE)
    {
        slot.velocity.x = 0.0f;
        slot.velocity.z = 0.0f;
    }

    switch(action)
    {
    case COMBAT_ACTION_LIGHT_ATTACK:
    case COMBAT_ACTION_HEAVY_ATTACK:
    case COMBAT_ACTION_GRAB:
    case COMBAT_ACTION_DODGE:

        break;
    case COMBAT_ACTION_STAGGER:
        if(selectNativeAction)
            slot.actorActionFlags |= 0x20u;
        slot.poseStateWord = 1;
        break;
    case COMBAT_ACTION_KNOCKDOWN:
        if(selectNativeAction)
            slot.actorActionFlags |= 0x40u;
        break;
    case COMBAT_ACTION_GUARD:
    {
        if(!selectNativeAction)
            break;

        const CharacterRecord* const character =
            characterRecordForId(static_cast<uint16_t>(slot.characterId));
        const int actionIndex =
            moveForAction(MOVE_INPUT_COMMAND_GUARD_HELD);
        const int moveId = character != NULL && actionIndex > 0 ?
            resolveCharacterMove(
                character->combatStyleId, -actionIndex) : -1;
        const int selectedMove = moveId > 0 ?
            moveId : MOVE_RECORD_DEFAULT_ACTION;
        setGameSlotSelectedAction(slot, selectedMove);
        if(slot.recoveredMoveId != selectedMove)
        {
            slot.recoveredMoveId = static_cast<int16_t>(selectedMove);
            slot.scheduledMotionValid = false;
            slot.scheduledMotionComplete = false;
            slot.combatPosePublished = false;
            slot.playingAnimationFrame = 0.0f;
            slot.activeHitBlock = 0;
            slot.hitChannelMask = 0;
        }
        break;
    }
    case COMBAT_ACTION_DEFEATED:

        slot.velocity.x = 0.0f;
        slot.velocity.z = 0.0f;
        if(selectNativeAction)
            slot.actorActionFlags |= 0x80u;
        break;
    case COMBAT_ACTION_VICTORY:

        break;
    case COMBAT_ACTION_IDLE:
    {
        if(!selectNativeAction)
            break;
        const CharacterRecord* const character =
            characterRecordForId(static_cast<uint16_t>(slot.characterId));

        const int moveId = character != NULL ?
            resolveCharacterMove(
                character->combatStyleId, -MOVE_CHARACTER_ACTION_IDLE) : 1;
        setLocomotionMove(slot, moveId > 0 ? moveId : 1);
        setGameSlotSelectedAction(slot, MOVE_RECORD_DEFAULT_ACTION);
        break;
    }
    default:
        break;
    }
}

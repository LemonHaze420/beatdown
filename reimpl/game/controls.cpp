#include "game/game.h"
#include "game/combat/input.h"
#include "game/input_directions.h"
#include "action_state_handlers.h"

int Game::updateCombatInput(GameSlotRecord& actor, MoveInputHistory& history,
    uint32_t pad, float worldX, float worldZ, float deltaSeconds,
    bool buttonPressed)
{
    prepareCombatInputPolicy(actor);
    const uint32_t code = combatHistoryCode(actor, pad, worldX, worldZ);
    const int trigger = buttonPressed ? 0 :
        combatInputTrigger(pad, actor.combatCommand.previousPad);
    actor.combatCommand.previousPad = pad;
    history.update(code, deltaSeconds);
    const GameSlotRecord* target = combatSlotByIndex(actor.linkedSlotIndex);
    const bool active = combatantActive(actor);
    const uint32_t facing = (actor.motionControlFlags & 0x4000u) != 0 ?
        MOVE_INPUT_RELATION_TOWARD : actor.facingRelation;
    const int raw = trigger == -2 || !combatInputScanAllowed(actor) ? -1 :
        moveInputRecognise(history, facing, actor.facingDirections,
            actor.actorBehaviorFlags, active ? target->actorBehaviorFlags : 0,
            actor.impactSelectorState, active ? target->impactSelectorState : 0,
            actor.actorTargetingFlags, active ? target->actorTargetingFlags : 0,
            trigger);
    recordCombatInputCommand(actor, raw, trigger != -2);
    const int command = rewriteCombatInputCommand(actor, raw);
    const int move = command >= 0 ? dispatchCombatInput(actor, command) : -1;
    if(&actor == &persistent.player)
    {
        combatInputChargeSeconds = history.age(0);
        combatInputLastCommand = raw;
        combatInputLastActionIndex = command >= 0 ? command : MOVE_ACTION_INDEX_RESOLVED;
        combatInputLastMoveId = move;
    }
    return move;
}

void Game::updateHumanCombatantInput(GameSlotRecord& actor,
    MoveInputHistory& history, uint32_t held, uint32_t pressed,
    float leftX, float leftZ, float rightX, float rightZ, float deltaSeconds)
{
    if(actor.currentHealth <= 0 || (actor.flags & 1u) == 0)
        return;
    float x, z, magnitude;
    readMovementInput(leftX, leftZ, held | pressed, x, z, magnitude);
    float worldX, worldZ;
    cameraRelativeAxes(x, z, worldX, worldZ);
    const uint32_t pad = combatStickDirectionCode(leftX, leftZ, rightX, rightZ,
        combatButtonCode(held | pressed) | combatDirectionCode(held | pressed));
    updateCombatInput(actor, history, pad, worldX, worldZ, deltaSeconds,
        (combatButtonCode(pressed) & ~CPU_COMBAT_RUN_BUTTON) != 0);

    if(combatActionPairs[actor.slotIndex].selfMotionId != 0)
        return;
    updateCombatLocomotion(actor, pad, worldX, worldZ, magnitude);
}

void Game::updatePlayerMovement(float deltaSeconds)
{
    updateCombatFacingFields();
    GameSlotRecord& actor = persistent.player;
    if(persistent.story.playerStopped)
    {
        actor.velocity.x = actor.velocity.z = 0.0f;
        if(actor.action == COMBAT_ACTION_MOVE)
            startAction(actor, COMBAT_ACTION_IDLE);
        return;
    }
    updateHumanCombatantInput(actor, combatInputHistory, heldInput, pressedInput,
        playerMovementX, playerMovementZ, playerLookX, playerLookZ, deltaSeconds);
}

void Game::updateHumanOpponentMovement(float deltaSeconds)
{
    if(enemy.playerIndex >= STAGE_AI_CONTEXT_COUNT)
        return;
    updateHumanCombatantInput(enemy, stageAiInputHistories[enemy.playerIndex],
        secondHeldInput, secondPressedInput, secondPlayerMovementX,
        secondPlayerMovementZ, secondPlayerLookX, secondPlayerLookZ, deltaSeconds);
}

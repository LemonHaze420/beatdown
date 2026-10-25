#include "game/game.h"
#include "game/internal.h"
#include "action_state_handlers.h"
#include <algorithm>
#include <math.h>

void Game::updateCombatStatePhases(GameSlotRecord& slot, bool entered)
{
    const uint32_t held = slot.playerIndex == 0 ? heldInput | pressedInput :
        (slot.playerIndex == 1 ? secondHeldInput | secondPressedInput : 0);
    const uint32_t pressed = slot.playerIndex == 0 ? pressedInput :
        (slot.playerIndex == 1 ? secondPressedInput : 0);
    const uint32_t released = slot.playerIndex == 0 ? releasedInput :
        (slot.playerIndex == 1 ? secondReleasedInput : 0);

    switch(slot.stateHandler)
    {
    case COMBAT_STATE_UPDATE_COMBATANT_HIT_RECOIL:
    {

        const float x = slot.locomotion.displacementX;
        const float z = slot.locomotion.displacementZ;
        const float decay = powf(COMBAT_RUN_STOP_DECAY, scaledFrameDeltaSeconds * 60.0f);
        slot.locomotion.displacementX *= decay;
        slot.locomotion.displacementZ *= decay;
        moveFighter(slot, x, z);
        if(x != 0.0f || z != 0.0f)
            slot.headingDegrees = atan2f(x, z) * 57.29577951308232f;
        slot.velocity.x = slot.velocity.z = 0.0f;
        break;
    }
    case COMBAT_STATE_APPLY_COMBAT_DAMAGE_AND_REACTION:
        updateCombatReceivedDamage(slot, entered);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_GET_UP:
        updateCombatGetUp(slot, entered);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_BATTLE_TARGET_REQUEST:

        if((slot.animationCueFlags & 2u) != 0)
            queueBattleSequenceActorTarget(slot);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_BLOCK_RECOIL:
        if(!entered && slot.statePhase == 2)
            updateCombatRecovery(slot);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_AIRBORNE_SPIN:
        if(entered)
            slot.spinPhase = static_cast<int16_t>(slot.spinPhase + 1);
        else if(slot.statePhase == 2)
            updateCombatSpin(slot);
        break;
    case COMBAT_STATE_RESOLVE_COMBATANT_LANDING_DAMAGE:
        updateCombatLandingDamage(slot);
        break;
    case COMBAT_STATE_QUEUE_COMBATANT_FOLLOWUP_ACTION:
        updateCombatWorldItemPickup(slot);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_STAND_UP:
        updateCombatItemUse(slot, entered);
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_WEAPON_SWING:

        if(!entered && slot.statePhase == 2 &&
           slot.playingAnimationFrame >= slot.hitWindowStartFrame)
        {
            GameSlotRecord* const target =
                mutableCombatSlotByIndex(slot.linkedSlotIndex);
            if(target != NULL && (target->impactSelectorState & 4u) == 0)
            {

                const float progress = std::clamp(
                    (slot.field_0x439c - 10.0f) / 40.0f, 0.0f, 1.0f);
                const int16_t amount = static_cast<int16_t>(
                    static_cast<int>(((1.0f - progress) * 0.4f + 0.1f) *
                                     target->maxPrideGauge));
                const int remaining = target->prideGauge - amount;
                target->prideGauge = static_cast<int16_t>(
                    remaining < 0 ? 0 : remaining);
                slot.participantScore += PARTICIPANT_SCORE_INTIMIDATION;
            }
            slot.statePhase = 3;
        }
        break;
    case COMBAT_STATE_COMPLETE_COMBATANT_ANIMATION:

        if(slot.stateWork[0] == 0 && slot.playingAnimationFrame >= 18.0f)
        {
            if((held & 15u) != 0)
            {
                slot.actorUpdateFlags |= 4u;
                const int action = stanceMotion(slot, 2);
                if(action > 0)
                    startCombatMove(slot, action, COMBAT_ACTION_MOVE);
            }
            else
                slot.actorUpdateFlags &= ~4u;
        }
        if((released & 15u) != 0)
            slot.stateWork[0] = 1;
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_REPEAT_DELAY:

        if(entered)
        {
            if(slot.slotIndex != 0)
            {
                slot.statePhase = 3;
                break;
            }
            if(combatRepeatCount < 1000)
                ++combatRepeatCount;
            combatRepeatDelaySeconds = static_cast<float>(combatRepeatCount - 1);
            if(combatRepeatDelaySeconds > 16.0f)
                combatRepeatDelaySeconds = 16.0f;
        }
        else if(slot.statePhase == 2)
        {
            combatRepeatDelaySeconds -=
                scaledFrameDeltaSeconds * slot.animationTimeScale;
            if(combatRepeatDelaySeconds <= 0.0f)
                slot.statePhase = 3;
        }
        else if(slot.statePhase == 3)
        {
            const int action = stanceMotion(slot, 1);
            if(action > 0)
                startCombatMove(slot, action, COMBAT_ACTION_IDLE);
        }
        break;
    case COMBAT_STATE_UPDATE_COMBATANT_WAKE_ROLL:

        if((pressed & GAME_INPUT_ATTACK) != 0)
            slot.stateWork[0] = 1;
        if(slot.stateWork[1] == 0 && (slot.animationCueFlags & 2u) != 0)
        {
            slot.stateWork[1] = 1;
            ++slot.statePhase;
            if(slot.stateWork[0] != 0)
                queueCombatPair(slot, -0xce, 0);
        }
        break;
    default:
        break;
    }
}

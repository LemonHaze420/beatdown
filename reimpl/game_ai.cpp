#include "game.h"
#include "game_combat_input.h"
#include "menu_inventory.h"
#include "scene_task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera.h"
#include "sound_cues.h"
#include "stage.h"
#include "hud_advice.h"
#include "../formats/MENU_LAYOUT/pause_results_layout.h"
#include "../formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game_internal.h"

using namespace game_detail;

namespace game_detail
{

std::int16_t combatTargetAttackClass(const GameSlotRecord& target)
{
    if(target.recoveredMoveId < 0 ||
       target.recoveredMoveId >= MOVE_RECORD_COUNT)
        return 0;
    const MoveHitBlock& hit =
        MOVE_RECORDS[target.recoveredMoveId].hitBlocks[0];
    std::int16_t result = 0;
    if((hit.flagsB & 0x0004) != 0)
        result = 1;
    else if((hit.flagsB & 0x0008) != 0)
        result = 2;
    if((hit.flagsC & 0x00000004) != 0)
        result = static_cast<std::int16_t>(result + 3);
    else if((hit.flagsC & 0x00000002) != 0)
        result = static_cast<std::int16_t>(result + 6);
    return result;
}

std::uint32_t drawCombatAiRandom(void* context)
{
    if(context == NULL)
        return 0;
    return nextGameRandom(*static_cast<GameRandom*>(context));
}

std::int16_t firstSetCombatState(std::uint32_t flags)
{
    for(std::int16_t bit = 0; bit < 32; ++bit)
    {
        if((flags & (std::uint32_t(1) << bit)) != 0)
            return bit;
    }
    return 0;
}

std::int16_t firstSetTargetDirection(std::uint32_t flags)
{
    for(std::int16_t bit = 0; bit < 3; ++bit)
    {
        if((flags & (std::uint32_t(1) << bit)) != 0)
            return static_cast<std::int16_t>(bit + 1);
    }
    return 0;
}

}

void
Game::loadStageAiResources(void)
{
    stageAiResourceLoaded = true;
    stageAiDecisionValid = false;
    stageAiDiagnosticText.clear();
}

void
Game::initializeStageAiBattleContexts(
    uint8_t battleMode, bool forceProfile44Matchup)
{
    if(!stageAiResourceLoaded)
        return;
    for(std::size_t i = 0; i < STAGE_AI_CONTEXT_COUNT; ++i)
        stageAiInputHistories[i].reset();
    LgpAiRandomSource randomSource = {drawCombatAiRandom, &random};
    if(currentMode != GAME_MODE_STORY)
    {
        GameSlotRecord* const combatants[2] =
        {
            &persistent.player, &enemy
        };
        for(int i = 0; i < 2; ++i)
        {
            GameSlotRecord& combatant = *combatants[i];
            if((combatant.playerIndex & ~1u) == 0)
                continue;
            const uint8_t profile = combatAiProfileForCharacter(
                static_cast<uint16_t>(combatant.characterId),
                COMBAT_AI_PRIMARY_OPPONENT);
            StageAiAsyncContext* context =
                mutableStageAiContext(combatant.playerIndex);
            if(context == NULL ||
               !requestStageAiContext(
                   context, profile, combatant.playerIndex,
                   combatant.slotIndex, true,
                   static_cast<uint8_t>(persistent.options.difficulty),
                   randomSource))
                continue;
            combatant.motionControlFlags |= 0x10000u;
            combatant.actorActionFlags |= 1u;
        }
        return;
    }
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        StageActorRuntime& actor = stageActors[i];
        if(!actor.occupied || !actor.battleAdmitted ||
           (actor.slot.flags & 1u) == 0)
            continue;
        const PersistentGameRecord* record =
            stageActorPersistentRecord(i);
        const bool activeRoster =
            stageActorIsActiveRoster(actor.authoredCharacterId);
        CombatAiProfileSelection selection = {};
        selection.characterId = actor.authoredCharacterId;
        selection.persistentProfileId = record != NULL ?
            static_cast<uint16_t>(record->aiProfileId) :
            combatAiProfileForCharacter(
                actor.authoredCharacterId,
                COMBAT_AI_PRIMARY_OPPONENT);
        selection.role = actor.primaryEncounter ?
            COMBAT_AI_PRIMARY_OPPONENT : COMBAT_AI_AMBIENT_OPPONENT;
        selection.relationshipState = record != NULL ?
            record->relationshipState : 0;
        selection.profileCategory =
            combatAiProfileCategory(actor.authoredCharacterId);
        selection.battleMode = battleMode;
        selection.activeRoster = activeRoster;
        selection.forceProfile44Matchup =
            actor.primaryEncounter && forceProfile44Matchup;
        const uint8_t profile = resolveCombatAiProfile(selection);
        StageAiAsyncContext* context =
            mutableStageAiContext(actor.slot.playerIndex);
        if(context == NULL ||
           !requestStageAiContext(
               context, profile, actor.slot.playerIndex,
               actor.slot.slotIndex, true,
               static_cast<uint8_t>(persistent.options.difficulty),
               randomSource))
            continue;
        actor.battleActive = true;
        actor.generatorState = STAGE_ACTOR_GENERATOR_ALIVE;
        actor.slot.motionControlFlags |= 0x10000u;
        actor.slot.actorActionFlags |= 1u;
        for(int opponentIndex = 0;
            opponentIndex < activeOpponentCount; ++opponentIndex)
        {
            GameSlotRecord& combatant = opponentIndex == 0 ?
                enemy : additionalEnemies[opponentIndex - 1];
            if(combatant.characterId !=
               static_cast<int16_t>(actor.authoredCharacterId))
                continue;
            combatant.motionControlFlags |= 0x10000u;
            combatant.actorActionFlags |= 1u;
        }
    }
    if(allyActive)
    {

        const uint8_t profile = combatAiProfileForCharacter(
            static_cast<uint16_t>(ally.characterId),
            COMBAT_AI_PARTY_PARTNER, 2);
        StageAiAsyncContext* const context =
            mutableStageAiContext(ally.playerIndex);
        if(context != NULL &&
           requestStageAiContext(
               context, profile, ally.playerIndex, ally.slotIndex, true,
               static_cast<uint8_t>(persistent.options.difficulty),
               randomSource))
        {
            ally.motionControlFlags |= 0x10000u;
            ally.actorActionFlags |= 1u;
        }
    }
}

void
Game::teardownStageAiBattleContexts(void)
{
    for(std::size_t i = 0; i < STAGE_AI_CONTEXT_COUNT; ++i)
        releaseStageAiContext(&stageAiContexts[i]);
    for(std::size_t i = 0; i < STAGE_AI_CONTEXT_COUNT; ++i)
        stageAiInputHistories[i].reset();
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        StageActorRuntime& actor = stageActors[i];
        actor.battleAdmitted = false;
        actor.battleActive = false;
        actor.primaryEncounter = false;
        actor.generatorState = STAGE_ACTOR_GENERATOR_NONE;
        actor.slot.motionControlFlags &= ~0x10000u;
        actor.slot.actorActionFlags &= ~1u;
    }
}

void
Game::updateStageAiGenerators(float deltaSeconds)
{
    if((persistent.story.battleFlags & 0x200u) == 0)
        return;
    if(deltaSeconds < 0.0f)
        deltaSeconds = 0.0f;

    bool becameEligible[GAME_STAGE_ACTOR_CAPACITY] = {};
    for(int opponentIndex = 0;
        opponentIndex < activeOpponentCount; ++opponentIndex)
    {
        GameSlotRecord& combatant = opponentIndex == 0 ?
            enemy : additionalEnemies[opponentIndex - 1];
        StageActorRuntime* actor = findStageActor(
            static_cast<uint16_t>(combatant.characterId));
        if(actor == NULL)
            continue;
        actor->slot.currentHealth = combatant.currentHealth;
        actor->slot.maxHealth = combatant.maxHealth;
        actor->slot.position = combatant.position;
        actor->slot.headingDegrees = combatant.headingDegrees;
        if(combatant.currentHealth <= 0 &&
           actor->generatorState == STAGE_ACTOR_GENERATOR_ALIVE)
        {
            actor->generatorState = STAGE_ACTOR_GENERATOR_ELIGIBLE;
            actor->battleActive = false;
            actor->visible = false;
            actor->slot.motionControlFlags &= ~0x10000u;
            becameEligible[actor - stageActors] = true;
        }
    }

    for(int generatorIndex = 0;
        generatorIndex < STORY_GENERATOR_COUNT; ++generatorIndex)
    {
        StoryGeneratorState& generator =
            persistent.story.generators[generatorIndex];
        if(generator.remainingCount <= 0 ||
           generator.stageId != stageActorStageId)
            continue;
        if(generator.timerSeconds > 0.0f)
        {
            generator.timerSeconds -= deltaSeconds;
            if(generator.timerSeconds > 0.0f)
                continue;
        }

        int actorIndex = -1;
        for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
        {
            if(stageActors[i].occupied &&
               stageActors[i].generatorState ==
                   STAGE_ACTOR_GENERATOR_ELIGIBLE &&
               !becameEligible[i])
            {
                actorIndex = i;
                break;
            }
        }
        if(actorIndex < 0)
        {

            --generator.remainingCount;
            continue;
        }

        StageActorRuntime& actor = stageActors[actorIndex];
        PersistentGameRecord* record =
            stageActorPersistentRecord(actorIndex);
        const int16_t maximum = record != NULL &&
            record->maxHealth > 0 ? record->maxHealth :
            actor.slot.maxHealth;
        actor.slot.maxHealth = maximum;
        actor.slot.currentHealth = maximum;
        actor.slot.position = {
            generator.position[0], generator.position[1],
            generator.position[2]};
        actor.slot.headingDegrees = generator.headingDegrees;
        actor.slot.flags |= 1u;
        actor.slot.motionControlFlags |= 0x10000u;
        actor.slot.actorActionFlags |= 1u;
        actor.visible = true;
        actor.battleActive = true;
        actor.battleAdmitted = true;
        actor.generatorState = STAGE_ACTOR_GENERATOR_ALIVE;
        if(record != NULL)
            record->currentHealth = maximum;

        for(int opponentIndex = 0;
            opponentIndex < activeOpponentCount; ++opponentIndex)
        {
            GameSlotRecord& combatant = opponentIndex == 0 ?
                enemy : additionalEnemies[opponentIndex - 1];
            if(combatant.characterId ==
               static_cast<int16_t>(actor.authoredCharacterId))
                combatant = actor.slot;
        }
        StoryActorState* storyActor = findStoryActor(
            persistent.story, actor.authoredCharacterId, true);
        if(storyActor != NULL)
        {
            storyActor->position[0] = actor.slot.position.x;
            storyActor->position[1] = actor.slot.position.y;
            storyActor->position[2] = actor.slot.position.z;
            storyActor->headingDegrees = actor.slot.headingDegrees;
            storyActor->currentHealth = maximum;
            storyActor->active = 1;
            storyActor->visible = 1;
            storyActor->defeated = 0;
        }
        --generator.remainingCount;
        generator.timerSeconds =
            static_cast<float>(nextRandom() & 0x3fffu) * 0.0001f;
    }
}

bool Game::stageAiGeneratorsPending(void) const
{
    for(int i = 0; i < STORY_GENERATOR_COUNT; ++i)
    {
        const StoryGeneratorState& generator =
            persistent.story.generators[i];
        if(generator.remainingCount > 0 &&
           generator.stageId == stageActorStageId)
            return true;
    }
    return false;
}

bool
Game::updateStageAiOpponent(GameSlotRecord& opponent, GameSlotRecord& target,
                            float deltaSeconds)
{
    StageAiAsyncContext* context =
        mutableStageAiContext(opponent.playerIndex);
    if(context == NULL || !stageAiResourceLoaded)
        return false;

    CombatAiUpdateInput input = {};
    input.slotIndex = opponent.slotIndex;
    input.difficulty =
        static_cast<std::uint8_t>(persistent.options.difficulty);
    input.selfState = firstSetCombatState(opponent.actorActionFlags);
    input.targetState = firstSetCombatState(target.actorActionFlags);
    input.selfHealth = opponent.currentHealth;
    input.selfMaxHealth = opponent.maxHealth;
    input.selfDamageGauge = opponent.damageGauge;
    input.selfDamageGaugeMaximum =
        opponent.damageGaugeMaximum > 0 ?
            opponent.damageGaugeMaximum : opponent.maxHealth;
    input.targetHealth = target.currentHealth;
    input.targetMaxHealth = target.maxHealth;
    input.targetAttackClass = combatTargetAttackClass(target);
    input.targetDirection =
        firstSetTargetDirection(target.actorTargetingFlags);
    input.selfDirection = combatSelfDirectionClass(opponent, target);
    input.selfPosition = {
        opponent.position.x, opponent.position.y, opponent.position.z};
    input.targetPosition = {
        target.position.x, target.position.y, target.position.z};

    if((opponent.actorTargetingFlags & 0x8000u) != 0 &&
       opponent.linkedSlotIndex == target.slotIndex &&
       target.currentHealth > 0)
        input.conditionBits |= std::uint64_t(1) << 0;
    if((opponent.actorTargetingFlags & 0x4000) != 0)
        input.conditionBits |= std::uint64_t(1) << 1;
    if((opponent.actorTargetingFlags & 0x10000) != 0)
        input.conditionBits |= std::uint64_t(1) << 2;
    if((opponent.actorActionFlags & 0x100) != 0 ||
       input.selfState != 1)
        input.conditionBits |= std::uint64_t(1) << 3;
    if((opponent.actorActionFlags & 0x4000) != 0)
        input.conditionBits |= std::uint64_t(1) << 14;

    const PersistentGameRecord* aiRecord = NULL;
    if(currentMode == GAME_MODE_STORY && opponent.playerIndex >= 3)
    {
        aiRecord = stageActorPersistentRecord(
            static_cast<int>(opponent.playerIndex) - 3);
    }
    else if(opponent.persistentRecordId >= 0)
    {
        aiRecord = characterRecord(
            static_cast<uint16_t>(opponent.persistentRecordId));
    }
    if((input.conditionBits & (std::uint64_t(1) << 14)) == 0 &&
       aiRecord != NULL)
    {
        for(const PersistentMoveSlot& moveSlot : aiRecord->moveSlots)
        {
            if(moveSlot.type == 2 && moveSlot.id != -1)
            {
                input.conditionBits |= std::uint64_t(1) << 15;
                break;
            }
        }
    }

    const bool wasRequested =
        context->state == STAGE_AI_CONTEXT_REQUESTED;
    StageAiTickGate gate = {};
    gate.occupied = (opponent.flags & 1u) != 0;
    gate.battleActive =
        (opponent.motionControlFlags & 0x10000u) != 0;
    gate.scaledDeltaNonzero = deltaSeconds != 0.0f;
    gate.cpuControlled = (opponent.playerIndex & ~1u) != 0;
    CombatAiDecision decision = {};
    std::string error;
    if(!updateStageAiContext(
           context, gate, input, &decision, &error))
    {
        stageAiDiagnosticText =
            "combat AI decision failed: " + error;
        return false;
    }
    stageAiLastDecision = decision;
    stageAiDecisionValid = true;
    if(wasRequested)
    {
        LgpAiRandomSource randomSource = {drawCombatAiRandom, &random};
        if(!completeStageAiContextLoad(
               context, "raw/XAF", randomSource, &error))
        {
            stageAiDiagnosticText =
                "combat AI profile load failed: " + error;
            return false;
        }
        return true;
    }

    if((context->runtime.vm.bitState & 0x80u) != 0)
    {
        opponent.inputHold.flags |= 2u;
        context->runtime.vm.bitState &= ~std::uint64_t(0x80u);
    }

    char diagnostic[160];
    snprintf(diagnostic, sizeof(diagnostic),
             "combat AI profile=%u record=%d input=0x%04x",
             static_cast<unsigned>(context->profileId),
             decision.selectedRecord,
             static_cast<unsigned>(decision.logicalInput));
    stageAiDiagnosticText = diagnostic;

    CpuCombatInputContext padContext = {};
    padContext.deltaX = target.position.x - opponent.position.x;
    padContext.deltaY =
        (target.groundHeightValid ? target.groundHeight : 0.0f) -
        (opponent.groundHeightValid ? opponent.groundHeight : 0.0f);
    padContext.deltaZ = target.position.z - opponent.position.z;
    padContext.cameraForwardX = movementCameraForwardX;
    padContext.cameraForwardZ = movementCameraForwardZ;
    padContext.actorHeadingDegrees = opponent.headingDegrees;
    padContext.actorUpdateFlags = opponent.actorUpdateFlags;

    padContext.controllerLayout = persistent.options.controllerTypeP2;
    padContext.alternateLayoutOrder =
        persistent.options.language == GAME_LANGUAGE_JAPANESE;
    updateCpuCombatInput(opponent.cpuInput, decision.logicalInput,
                         decision.channels.data(), padContext);
    const CpuCombatInputState& pad = opponent.cpuInput;
    if(pad.clearMovementFacing)
        opponent.actorUpdateFlags &= ~4u;
    if(pad.grabRequested)
        opponent.actorRuntimeFlags |= 0x100u;

    float movementX = 0.0f;
    float movementZ = 0.0f;
    if((pad.currentButtons & 15u) != 0)
    {
        float localX = pad.leftX;
        float localZ = pad.leftY;
        if((pad.rawButtons & 15u) != 0)
        {
            localX = ((pad.rawButtons & 1u) != 0 ? 1.0f : 0.0f) -
                ((pad.rawButtons & 2u) != 0 ? 1.0f : 0.0f);
            localZ = ((pad.rawButtons & 4u) != 0 ? 1.0f : 0.0f) -
                ((pad.rawButtons & 8u) != 0 ? 1.0f : 0.0f);
        }
        cameraRelativeAxes(localX, localZ, movementX, movementZ);
    }
    const float movementLength =
        sqrtf(movementX * movementX + movementZ * movementZ);
    decision.moving = movementLength > 0.0001f;
    decision.running = (pad.rawButtons & CPU_COMBAT_RUN_BUTTON) != 0;
    decision.verticalSeparation = fabsf(padContext.deltaY) >= 35.0f;
    decision.movementDirection = {};
    if(decision.moving)
    {
        decision.movementDirection.x = movementX / movementLength;
        decision.movementDirection.z = movementZ / movementLength;
    }
    if(pad.buttonsCleared)
        decision.action = COMBAT_AI_ACTION_NONE;
    stageAiLastDecision = decision;
    const int playerIndex = opponent.playerIndex;
    int resolvedMoveId = -1;
    if(playerIndex >= 0 && playerIndex < static_cast<int>(STAGE_AI_CONTEXT_COUNT))
        resolvedMoveId = updateCombatInput(opponent, stageAiInputHistories[playerIndex],
            pad.canonicalButtons, movementX, movementZ, deltaSeconds);
    if(decision.action != COMBAT_AI_ACTION_NONE)
    {
        snprintf(diagnostic, sizeof(diagnostic),
            "combat AI profile=%u record=%d input=0x%04x command=%d move=%d",
            static_cast<unsigned>(context->profileId), decision.selectedRecord,
            static_cast<unsigned>(decision.logicalInput),
            opponent.combatCommand.command, resolvedMoveId);
        stageAiDiagnosticText = diagnostic;
    }
    if(resolvedMoveId > 0)
    {
        stageAiLastDecision.exactMoveUnavailable = false;
        return true;
    }
    updateCombatLocomotion(opponent, pad.canonicalButtons, movementX, movementZ,
        sqrtf(pad.leftX * pad.leftX + pad.leftY * pad.leftY));
    return true;
}

void
Game::updateEnemyAI(GameSlotRecord& opponent, GameSlotRecord& target,
                    float deltaSeconds)
{
    if(opponent.currentHealth == 0)
        return;

    if(!updateStageAiOpponent(opponent, target, deltaSeconds))
    {
        opponent.velocity.x = 0.0f;
        opponent.velocity.z = 0.0f;
    }
}

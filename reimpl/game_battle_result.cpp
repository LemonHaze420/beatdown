#include "game.h"
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

void
Game::resetBattleResultController(void)
{
    battleResult = {};
    battleFeedbackModeValue = 0;
    battleFinisherPhase = BATTLE_FINISHER_INACTIVE;
    battleFinisherElapsedSeconds = 0.0f;
    battleFinisherAttackerSlot = 0;
    battleFinisherTargetSlot = 0;
    battleFinisherSelection = {};
    battleFeedbackRawSecondsRemaining = 0.0f;
    battleEndSequenceActive = false;
    battleEndSequenceElapsedSeconds = 0.0f;
    battleEndRuntime = {};
    battleEndNoticeRuntime = {};
    pendingBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleResult.transitionType = 0;
    battleResult.commandIndex = 0;
    battleResult.terminalCode = battleResultTerminalCode(0);
    battleResult.actorIndices[0] = 0;
    battleResult.actorIndices[1] = 1;
    battleResult.sideActiveCounts[0] = 1;
    battleResult.sideActiveCounts[1] = static_cast<uint8_t>(
        activeOpponentCount > 0 ? activeOpponentCount : 0);
    battleResult.sideThresholds[0] = 1;
    battleResult.sideThresholds[1] = 1;
    battleResult.state = BATTLE_RESULT_CONTROLLER_IDLE;
    battleResult.selectedIndex = 0;
    battleResult.confirmationSelection = -1;
    battleResult.flowReady = true;
    battleResult.transitionConfigured = false;
    battleResult.pendingMessageVoiceTrack = -1;
}

uint8_t
Game::battleResultTerminalCode(uint8_t commandIndex)
{

    return commandIndex < 16 ? GAME_BATTLE_RESULT_TERMINAL_CODES[commandIndex] : 0;
}

bool
Game::configureBattleResultTransition(uint8_t transitionType,
                                      uint8_t commandIndex)
{
    if(transitionType >= BATTLE_RESULT_TRANSITION_TYPE_COUNT ||
       currentBattleOutcome != BATTLE_OUTCOME_IN_PROGRESS)
        return false;
    battleResult.transitionType = transitionType;
    battleResult.commandIndex = commandIndex;
    battleResult.terminalCode = battleResultTerminalCode(commandIndex);
    battleResult.transitionConfigured = true;
    return true;
}

void
Game::configureBattleResultActors(uint8_t firstActorIndex,
                                  uint8_t secondActorIndex)
{
    battleResult.actorIndices[0] = firstActorIndex;
    battleResult.actorIndices[1] = secondActorIndex;
}

bool
Game::configureBattleResultSide(int side, uint8_t activeCount,
                                int8_t threshold)
{
    if(side < 0 || side >= 2)
        return false;
    battleResult.sideActiveCounts[side] = activeCount;
    battleResult.sideThresholds[side] = threshold;
    return true;
}

void
Game::setBattleResultFlowReady(bool ready)
{
    battleResult.flowReady = ready;
}

bool
Game::setBattleResultSoundPending(int channel, bool pending)
{
    if(channel < 0 || channel >= BATTLE_RESULT_SOUND_CHANNEL_COUNT)
        return false;
    battleResult.soundPending[channel] = pending;
    return true;
}

bool
Game::pollEventMessageVoice(int32_t* trackIndex)
{
    if(trackIndex == NULL || battleResult.pendingMessageVoiceTrack < 0)
        return false;
    *trackIndex = battleResult.pendingMessageVoiceTrack;
    battleResult.pendingMessageVoiceTrack = -1;
    return true;
}

void
Game::setBattleRewardSuppressed(bool suppressed)
{
    battleResult.rewardSuppressed = suppressed;
}

void
Game::startBattleResultSelector(void)
{
    battleResult.state = BATTLE_RESULT_CONTROLLER_SELECTOR_READINESS;
    battleResult.readinessSecondsRemaining =
        BATTLE_RESULT_READINESS_DELAY_SECONDS;
    battleResult.inputSecondsRemaining = BATTLE_RESULT_INPUT_DELAY_SECONDS;
    battleResult.selectedIndex = 0;
    battleResult.confirmationSelection = -1;
    battleResult.selectorVisible = false;
    battleResult.confirmationActive = false;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
}

void
Game::dispatchBattleExperienceRewards(void)
{
    if(battleResult.rewardDispatched ||
       persistent.story.battleResultCode != 6)
        return;

    battleResult.rewardDispatched = true;
    lastBattleExperience = 0;
    lastBattleLevelsGained = 0;
    if(battleResult.rewardSuppressed)
    {
        battleSupplementalExperience = 0;
        return;
    }

    const int16_t playerRecordId = activeRecordId(0);
    const PersistentGameRecord* const playerRecord =
        playerRecordId >= 0 ?
            characterRecord(static_cast<uint16_t>(playerRecordId)) : NULL;
    const int playerLevel = playerRecord == NULL ? 0 : playerRecord->level;
    const bool interactionScore =
        (persistent.story.battleFlags &
         BATTLE_REWARD_INTERACTION_FLAG) != 0;
    int opponentLevels[BATTLE_REWARD_ROSTER_SIZE] = {};
    int opponentCount = 0;
    if(!interactionScore)
    {
        while(opponentCount < BATTLE_REWARD_ROSTER_SIZE &&
              battleRewardOpponentIds[opponentCount] >= 0)
        {
            opponentLevels[opponentCount] =
                battleRewardCharacterLevel(
                    battleRewardOpponentIds[opponentCount]);
            ++opponentCount;
        }
    }

    int reward = battleExperienceReward(
        opponentLevels, battleScoreMultipliers, opponentCount,
        playerLevel, battleComboBonus, battleSupplementalExperience,
        interactionScore);
    battleSupplementalExperience = 0;
    if(persistent.options.difficulty == 1)
        reward = static_cast<int>(
            static_cast<float>(reward) * 1.2f);

    lastBattleExperience = reward;
    lastBattleLevelsGained = applyBattleExperienceTo(
        playerRecordId >= 0 ?
            characterRecord(static_cast<uint16_t>(playerRecordId)) : NULL,
        reward);
    for(size_t i = 1; i <= 2; ++i)
    {
        const int16_t partyRecordId = activeRecordId(i);
        if(partyRecordId >= 0)
            applyPartyBattleExperience(
                static_cast<uint16_t>(partyRecordId), reward);
    }
}

void
Game::cleanupBattleResultSession(void)
{

    matchSetupFlagValue = false;
    cancelAllRumble();
    resetBehaviourPresentation();
    sceneSequencer.stop();
    soundEventHead = 0;
    soundEventCount = 0;
    pendingDamageCameraSettle = 0.0f;
    pendingMoveCameraSettle = 0.0f;
    battleResult.selectorVisible = false;
    battleResult.confirmationActive = false;
}

void
Game::reloadBattleForRetry(void)
{
    auto resetSlot = [this](GameSlotRecord& slot)
    {
        const CharacterId character =
            static_cast<CharacterId>(slot.characterId);
        const uint8_t slotIndex = slot.slotIndex;
        const uint8_t playerIndex = slot.playerIndex;
        const uint8_t linkedSlotIndex = slot.linkedSlotIndex;
        const int16_t nativeCharacterId = slot.nativeCharacterId;
        const float dynamicAttackMultiplier = slot.dynamicAttackMultiplier;
        const int16_t maximumHealth = slot.maxHealth;
        const int16_t npcPositionRowIndex = slot.npcPositionRowIndex;
        const float interactionRange = slot.interactionRange;
        const Vec3 position = slot.position;
        const float headingDegrees = slot.headingDegrees;

        initializeGameSlot(slot, character, slotIndex);
        slot.playerIndex = playerIndex;
        slot.linkedSlotIndex = linkedSlotIndex;
        slot.nativeCharacterId = nativeCharacterId;
        slot.dynamicAttackMultiplier = dynamicAttackMultiplier;
        slot.maxHealth = maximumHealth;
        slot.currentHealth = maximumHealth;
        for(int i = 0; i < 4; ++i)
            slot.bodyPartHealth[i] =
                static_cast<int16_t>(maximumHealth * 0.2f);
        slot.npcPositionRowIndex = npcPositionRowIndex;
        slot.interactionRange = interactionRange;
        slot.position = position;
        slot.headingDegrees = headingDegrees;
        initialiseSlotPride(slot, character);
    };

    resetSlot(persistent.player);
    if(allyActive)
        resetSlot(ally);
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        GameSlotRecord& slot =
            i == 0 ? enemy : additionalEnemies[i - 1];
        resetSlot(slot);
    }
    projectGameSlotCombatToPersistentRecord(persistent.player);
    if(allyActive)
        projectGameSlotCombatToPersistentRecord(ally);

    loadStageAiResources();
    initializeStageAiBattleContexts();

    currentBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
    battleElapsedSeconds = 0.0f;
    battleComboBonus = 0;
    battleSupplementalExperience = 0;
    lastBattleExperience = 0;
    lastBattleLevelsGained = 0;
    targetOpponentIndex = 0;
    persistent.story.battleActive = 1;
    persistent.story.battleResultCode = -1;
    persistent.story.resultDisplayActive = 0;
    persistent.story.stageTransition.postBattleFlagsApplied = 0;
    storyEncounterCompletionPending = false;
    frameCounter = static_cast<uint32_t>(persistent.stageId);
    seedGameRandom(random, frameCounter);
    resetBattleResultController();
    battleResult.sideActiveCounts[0] = allyActive ? 2 : 1;
    initializeCombatProjectileGroups();
    combatDamageEnabled = true;
    currentScreen = GAME_SCREEN_GAMEPLAY;
    if(gameFlowTask != NULL)
        gameFlowTask->state = 8;
    synchronizeBattleStoryState();
}

void
Game::updateBattleResultController(void)
{
    switch(battleResult.state)
    {
    case BATTLE_RESULT_CONTROLLER_DISPATCH_TRANSITION:
    {

        battleFeedbackModeValue = 0;
        BattleResultControllerState transitionState =
            BATTLE_RESULT_CONTROLLER_COMPLETE;
        switch(battleResult.transitionType)
        {
        case 0:
        case 8:
            transitionState = BATTLE_RESULT_CONTROLLER_SELECTOR_READINESS;
            break;
        case 1:
        case 2:
        case 3:
        case 5:
        case 7:
        case 9:
        case 10:
            transitionState = BATTLE_RESULT_CONTROLLER_POST_BATTLE_SCENE;
            break;
        case 4:
            transitionState = BATTLE_RESULT_CONTROLLER_POST_BATTLE_SCENE;
            break;
        case 6:
            transitionState = BATTLE_RESULT_CONTROLLER_TERMINAL_DRAIN;
            break;
        default:
            break;
        }
        if(transitionState == BATTLE_RESULT_CONTROLLER_SELECTOR_READINESS)
        {

            installMemberChangeSceneSequenceCaption();
            startSceneSequence(
                battleResult.actorIndices[0] == 0 ? 2 : 4,
                battleResult.actorIndices[0],
                battleResult.actorIndices[1]);
            if(gameFlowTask != NULL)
                gameFlowTask->state = 0x28;
        }

        if(transitionState != BATTLE_RESULT_CONTROLLER_POST_BATTLE_SCENE &&
           currentMode == GAME_MODE_STORY &&
           currentBattleOutcome == BATTLE_OUTCOME_PLAYER_WON)
        {
            battleResult.state = BATTLE_RESULT_CONTROLLER_STORY_HANDOFF;
            battleResult.readinessSecondsRemaining = 1.0f;
            updateBattleResultController();
            return;
        }
        if(transitionState == BATTLE_RESULT_CONTROLLER_SELECTOR_READINESS)
        {
            startBattleResultSelector();
            updateBattleResultController();
        }
        else
        {
            battleResult.state = transitionState;
            battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
        }
        return;
    }
    case BATTLE_RESULT_CONTROLLER_POST_BATTLE_SCENE:
    {
        const bool firstSideReady =
            static_cast<int>(battleResult.sideActiveCounts[0]) >=
                static_cast<int>(battleResult.sideThresholds[0]);
        const bool secondSideReady =
            static_cast<int>(battleResult.sideActiveCounts[1]) >=
                static_cast<int>(battleResult.sideThresholds[1]);
        if(firstSideReady || secondSideReady)
        {
            const int sequenceId =
                battleResult.transitionType == 4 ? 9 : 2;

            battleResult.pendingMessageVoiceTrack = 0x154;

            installMemberChangeSceneSequenceCaption();
            startSceneSequence(
                sequenceId, battleResult.actorIndices[0],
                battleResult.actorIndices[1]);
            if(gameFlowTask != NULL)
                gameFlowTask->state = 0x26;
            if(currentMode == GAME_MODE_STORY &&
               currentBattleOutcome == BATTLE_OUTCOME_PLAYER_WON)
            {
                battleResult.state =
                    BATTLE_RESULT_CONTROLLER_STORY_HANDOFF;
                battleResult.readinessSecondsRemaining = 1.0f;
            }
            else
                startBattleResultSelector();
        }
        else
        {
            battleResult.state = BATTLE_RESULT_CONTROLLER_COMPLETE;
            battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
            if(gameFlowTask != NULL)
                gameFlowTask->state = 0x0c;
        }
        return;
    }
    case BATTLE_RESULT_CONTROLLER_SELECTOR_READINESS:
        if(battleResult.readinessSecondsRemaining > 0.0f)
        {
            battleResult.readinessSecondsRemaining -= scaledFrameDeltaSeconds;
            if(battleResult.readinessSecondsRemaining > 0.0f)
                return;
            battleResult.readinessSecondsRemaining = 0.0f;
        }
        if(!battleResult.flowReady)
            return;
        battleResult.selectorVisible = true;
        battleResult.selectedIndex = 0;
        battleResult.inputSecondsRemaining =
            BATTLE_RESULT_INPUT_DELAY_SECONDS;
        battleResult.state =
            BATTLE_RESULT_CONTROLLER_SELECTOR_INPUT_DELAY;
        currentScreen = GAME_SCREEN_RESULTS;
        return;
    case BATTLE_RESULT_CONTROLLER_SELECTOR_INPUT_DELAY:
        if(battleResult.inputSecondsRemaining > 0.0f)
        {
            battleResult.inputSecondsRemaining -= scaledFrameDeltaSeconds;
            if(battleResult.inputSecondsRemaining > 0.0f)
                return;
            battleResult.inputSecondsRemaining = 0.0f;
        }
        battleResult.state = BATTLE_RESULT_CONTROLLER_SELECTOR_ACTIVE;
        return;
    case BATTLE_RESULT_CONTROLLER_SELECTOR_ACTIVE:
        if((pressedInput & GAME_INPUT_UP) != 0)
            battleResult.selectedIndex =
                wrapIndex(battleResult.selectedIndex - 1,
                          RESULT_CHOICE_COUNT);
        else if((pressedInput & GAME_INPUT_DOWN) != 0)
            battleResult.selectedIndex =
                wrapIndex(battleResult.selectedIndex + 1,
                          RESULT_CHOICE_COUNT);
        if((pressedInput & GAME_INPUT_CONFIRM) == 0)
            return;
        if(battleResult.selectedIndex == 0)
        {
            battleResult.terminalCode = static_cast<uint8_t>(
                RESULT_CHOICES[battleResult.selectedIndex].action);
            battleResult.state = BATTLE_RESULT_CONTROLLER_TERMINAL_DRAIN;
        }
        else
        {
            battleResult.confirmationSelection =
                battleResult.selectedIndex;
            battleResult.confirmationActive = true;
            battleResult.state = BATTLE_RESULT_CONTROLLER_CONFIRMATION;
        }
        return;
    case BATTLE_RESULT_CONTROLLER_CONFIRMATION:
        if((pressedInput & GAME_INPUT_CANCEL) != 0)
        {
            battleResult.confirmationSelection = -1;
            battleResult.confirmationActive = false;
            battleResult.state = BATTLE_RESULT_CONTROLLER_SELECTOR_ACTIVE;
        }
        else if((pressedInput & GAME_INPUT_CONFIRM) != 0)
        {
            battleResult.terminalCode = static_cast<uint8_t>(
                RESULT_CHOICES[battleResult.confirmationSelection].action);
            battleResult.confirmationSelection = -1;
            battleResult.confirmationActive = false;
            battleResult.state = BATTLE_RESULT_CONTROLLER_TERMINAL_DRAIN;
        }
        return;
    case BATTLE_RESULT_CONTROLLER_TERMINAL_DRAIN:
        if(!battleResult.flowReady)
            return;
        for(int i = 0; i < BATTLE_RESULT_SOUND_CHANNEL_COUNT; ++i)
            if(battleResult.soundPending[i])
                return;
        cleanupBattleResultSession();
        switch(battleResult.terminalCode)
        {
        case 0:
            battleResult.state = BATTLE_RESULT_CONTROLLER_RETRY_CLEANUP;
            break;
        case 1:
            battleResult.state = BATTLE_RESULT_CONTROLLER_COMPLETE;
            switchGameFlowTaskHandler(updateVersusMatchSetupFlow, 7);
            break;
        case 2:
            battleResult.state = BATTLE_RESULT_CONTROLLER_COMPLETE;
            switchGameFlowTaskHandler(updateVersusMatchSetupFlow, 5);
            break;
        default:
            battleResult.state = BATTLE_RESULT_CONTROLLER_COMPLETE;
            clearSecondaryProfile();
            handleQuit();
            break;
        }
        return;
    case BATTLE_RESULT_CONTROLLER_RETRY_CLEANUP:
        battleResult.state = BATTLE_RESULT_CONTROLLER_RETRY_RELOAD;
        return;
    case BATTLE_RESULT_CONTROLLER_RETRY_RELOAD:
        reloadBattleForRetry();
        return;
    case BATTLE_RESULT_CONTROLLER_STORY_HANDOFF:
    {
        if(battleResult.readinessSecondsRemaining > 0.0f)
        {
            battleResult.readinessSecondsRemaining -= scaledFrameDeltaSeconds;
            if(battleResult.readinessSecondsRemaining > 0.0f)
                return;
            battleResult.readinessSecondsRemaining = 0.0f;
        }
        dispatchBattleExperienceRewards();
        if(!persistent.story.stageTransition.postBattleFlagsApplied)
        {
            const GameSlotRecord& defeated =
                targetOpponentIndex == 0 ? enemy :
                    additionalEnemies[targetOpponentIndex - 1];
            applyStoryPostBattleFlags(
                persistent.story,
                static_cast<uint16_t>(defeated.characterId));
            persistent.story.stageTransition.postBattleFlagsApplied = 1;
        }
        StoryStageTransition& transition =
            persistent.story.stageTransition;
        transition.lifecycle = STORY_TRANSITION_POST_BATTLE;
        transition.postBattleState = 0;
        transition.controllerFlags |= 1;
        transition.elapsedSeconds = 0.0f;
        persistent.story.stageTransitionPending = 1;
        storyEncounterCompletionPending = true;
        persistent.story.resultDisplayActive = 1;
        battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
        currentScreen = GAME_SCREEN_RESULTS;
        battleResult.state = BATTLE_RESULT_CONTROLLER_COMPLETE;
        return;
    }
    default:
        return;
    }
}

void Game::setCamdatTimeScale(float value)
{
    camdatTimeScale = value;
}

float Game::timeScale(void) const
{
    return battleTimeScale * camdatTimeScale;
}

int Game::battleFeedbackMode(void) const
{
    return battleFeedbackModeValue;
}

float
Game::scaleFrameDelta(float rawSeconds) const
{
    const float conditionScale =
        temporarySlowActive && persistent.story.battleActive ? 0.5f : 1.0f;
    const float scaled =
        rawSeconds * battleTimeScale * camdatTimeScale * conditionScale;

    return scaled > 1.0f ? 1.0f / 60.0f : scaled;
}

void
Game::requestBattleCompletion(BattleOutcome outcome)
{
    if(currentBattleOutcome != BATTLE_OUTCOME_IN_PROGRESS ||
       battleEndSequenceActive ||
       battleFinisherPhase != BATTLE_FINISHER_INACTIVE)
    {
        return;
    }
    if(currentMode != GAME_MODE_STORY)
    {
        finishBattle(outcome);
        return;
    }
    if(persistent.story.battleActive == 0 || activeOpponentCount <= 0)
        return;

    pendingBattleOutcome = outcome;
    battleEndSequenceActive = true;
    battleEndSequenceElapsedSeconds = 0.0f;
    battleEndRuntime = {};
    battleEndNoticeRuntime = {};
    battleFeedbackModeValue = 1;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
}

bool
Game::shouldRunBattleFinisher(const GameSlotRecord& attacker,
                              const GameSlotRecord& target) const
{
    if(currentMode != GAME_MODE_STORY ||
       currentBattleOutcome != BATTLE_OUTCOME_IN_PROGRESS ||
       persistent.story.battleActive == 0 ||
       battleFinisherPhase != BATTLE_FINISHER_INACTIVE ||
       battleEndSequenceActive)
    {
        return false;
    }

    if(target.prideGauge != 0)
        return false;

    if((persistent.story.battleFlags &
        BATTLE_REWARD_INTERACTION_FLAG) != 0)
    {
        return true;
    }

    return target.slotIndex == 0 &&
           target.nativeCharacterId == 0 &&
           persistent.story.subBattleId == 0 &&
           !(persistent.stageId == 19 && attacker.nativeCharacterId == 0x19);
}

int
Game::selectBattleFinisherVariant(const GameSlotRecord& attacker,
                                  const GameSlotRecord& target)
{
    BattleFinisherSelectionInput input = {};
    input.targetHealthRatio = target.maxHealth > 0 ?
        static_cast<float>(target.currentHealth) /
            static_cast<float>(target.maxHealth) : 0.0f;
    const PersistentGameRecord* const attackerRecord =
        characterRecord(static_cast<uint16_t>(attacker.persistentRecordId));
    const PersistentGameRecord* const targetRecord =
        characterRecord(static_cast<uint16_t>(target.persistentRecordId));
    input.attackerGender = attackerRecord != nullptr ? attackerRecord->gender : 0;
    input.targetGender = targetRecord != nullptr ? targetRecord->gender : 0;
    input.armed = (attacker.actorActionFlags & 0x4000u) != 0;
    for(int index = 0; index < 2; ++index)
    {
        input.moveSlots[index] = attacker.moveSlots[index];
        input.moveSlotActive[index] = attacker.moveSlotActive[index];
    }
    battleFinisherSelection = ::selectBattleFinisherVariant(input, random);
    return battleFinisherSelection.variant;
}

int
Game::battleRelation(const GameSlotRecord& attacker,
                     const GameSlotRecord& target) const
{
    const auto motionVariant = [this](const GameSlotRecord& actor)
    {
        const PersistentGameRecord* record = characterRecord(
            static_cast<uint16_t>(actor.persistentRecordId));
        int style = record != nullptr ? record->combatStyleId : combatStyle(actor);
        if(style < 0 || style >= MOVE_CHARACTER_COUNT)
            style = 0;
        return static_cast<int>(MOVE_CHARACTER_RECORDS[style].locomotionVariant);
    };
    return battleFinisherRelation(motionVariant(attacker), motionVariant(target));
}

int
Game::battleFinisherMove(int variant, const GameSlotRecord& attacker,
                         const GameSlotRecord& target) const
{
    if(variant < 2 || variant >= 16)
        return 0;
    return BATTLE_FINISHER_DEFINITIONS[variant][1].moves[battleRelation(attacker, target)];
}

void
Game::startBattleFinisher(GameSlotRecord& attacker,
                          GameSlotRecord& target)
{
    if(!shouldRunBattleFinisher(attacker, target))
        return;
    battleFinisherAttackerSlot = attacker.slotIndex;
    battleFinisherTargetSlot = target.slotIndex;
    selectBattleFinisherVariant(attacker, target);
    const BattleFinisherDefinition& definition =
        BATTLE_FINISHER_DEFINITIONS[battleFinisherSelection.variant][1];
    updateBattleResourceSequence(battleResources, false,
        BATTLE_RESOURCE_ROWS[definition.resourceVariant][battleRelation(attacker, target)],
        battleResourceCallbacks);
    battleFinisherElapsedSeconds = 0.0f;
    battleFinisherPhase = BATTLE_FINISHER_PRE_BLOW;
    pendingBattleOutcome =
        attacker.slotIndex == 0 ||
        (allyActive && attacker.slotIndex == ally.slotIndex) ?
            BATTLE_OUTCOME_PLAYER_WON : BATTLE_OUTCOME_PLAYER_LOST;

    startCombatMove(attacker, 1462, attacker.action);
    startLinkedCombatantAction(attacker);
    battleFeedbackModeValue = 1;
    battleTimeScale = BATTLE_TIME_SCALE_FINISHER;
}

void
Game::updateBattleFinisher(float scaledSeconds, float rawSeconds)
{
    GameSlotRecord* const attacker =
        mutableCombatSlotByIndex(battleFinisherAttackerSlot);
    GameSlotRecord* const target =
        mutableCombatSlotByIndex(battleFinisherTargetSlot);
    if(attacker == NULL || target == NULL || attacker == target)
    {
        battleFinisherPhase = BATTLE_FINISHER_INACTIVE;
        battleFeedbackModeValue = 0;
        battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
        finishBattle(pendingBattleOutcome);
        return;
    }

    switch(battleFinisherPhase)
    {
    case BATTLE_FINISHER_PRE_BLOW:
        battleFinisherElapsedSeconds += scaledSeconds;

        {
            const bool loaded = updateBattleResourceSequence(battleResources, true,
                battleResources.listAddress, battleResourceCallbacks);
            if(battleFinisherElapsedSeconds < BATTLE_FINISHER_PRE_BLOW_SECONDS || !loaded)
                return;
        }
        battleFinisherElapsedSeconds = 0.0f;
        startCombatMove(
            *attacker,
            battleFinisherMove(battleFinisherSelection.variant, *attacker, *target),
            attacker->action);
        startLinkedCombatantAction(*attacker);

        if(battleFinisherSelection.usesMoveSlot != 0)
        {
            attacker->heldWorldWeapon = attacker->ownedWorldWeapons[
                battleFinisherSelection.moveSlotIndex];
            const int16_t managerId = ensureCombatWorldWeapon(attacker->heldWorldWeapon);
            if(managerId >= 0)
                weaponManager.pickup(managerId, attacker->slotIndex);
            updateCombatWeaponBehaviour(*attacker, MOVE_BEHAVIOUR_WEAPON_ACTION, 1);
            updateCombatWeaponBehaviour(*attacker, MOVE_BEHAVIOUR_WEAPON_VISIBLE, 1);
        }
        battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
        battleFeedbackModeValue = 1;
        reduceGameSlotHealth(*target, 9999);
        battleFinisherPhase = BATTLE_FINISHER_POST_BLOW;
        return;

    case BATTLE_FINISHER_POST_BLOW:
    {
        battleFinisherElapsedSeconds += scaledSeconds;
        if(battleFinisherElapsedSeconds < BATTLE_FINISHER_POST_BLOW_SECONDS)
            return;
        battleFinisherElapsedSeconds = 0.0f;
        const int attackerMove = actorMotionIdFromAction(*attacker, 1);
        if(attackerMove > 0)
            startCombatMove(*attacker, attackerMove, COMBAT_ACTION_IDLE);
        else
            startAction(*attacker, COMBAT_ACTION_IDLE);
        const int targetMove = actorMotionIdFromAction(
            *target, fabsf(target->velocity.y) < 0.001f ? 0x6f : 0x70);
        if(targetMove > 0)
            startCombatMove(*target, targetMove, COMBAT_ACTION_DEFEATED);
        else
            startAction(*target, COMBAT_ACTION_DEFEATED);
        if(attacker->slotIndex == 0)
            queueBattleScoreMultiplier(1.5f);
        scoreDefeatedBattleOpponents();
        battleFeedbackModeValue = 0;
        if((persistent.story.battleFlags &
            BATTLE_REWARD_INTERACTION_FLAG) != 0 &&
           attacker->slotIndex == 0)
        {
            battleFeedbackModeValue = 2;
            battleFeedbackRawSecondsRemaining =
                BATTLE_FEEDBACK_MODE_2_SECONDS;
            battleFinisherPhase = BATTLE_FINISHER_FEEDBACK_RECOVERY;
            return;
        }
        battleFinisherPhase = BATTLE_FINISHER_INACTIVE;
        finishBattle(pendingBattleOutcome);
        return;
    }

    case BATTLE_FINISHER_FEEDBACK_RECOVERY:
        battleFeedbackRawSecondsRemaining -= rawSeconds;
        if(battleFeedbackRawSecondsRemaining > 0.0f)
            return;
        battleFeedbackRawSecondsRemaining = 0.0f;
        battleFeedbackModeValue = 0;
        battleFinisherPhase = BATTLE_FINISHER_INACTIVE;
        finishBattle(pendingBattleOutcome);
        return;

    default:
        return;
    }
}

const BattleEndRuntime& Game::battleEndState(void) const
{
    return battleEndRuntime;
}

const BattleEndNoticeRuntime& Game::battleEndNotice(void) const
{
    return battleEndNoticeRuntime;
}

void
Game::updateBattleEndSequence(float scaledSeconds)
{
    if(!battleEndSequenceActive)
        return;

    updateBattleEndNotice(battleEndNoticeRuntime, scaledSeconds);
    BattleEndInput input;
    input.scaledDeltaSeconds = scaledSeconds;
    input.noticeTaskActive = battleEndNoticeRuntime.state != BATTLE_END_NOTICE_INACTIVE;
    input.fadeReady = sceneCharacterSelectReady(1);
    const BattleEndEffects effects = updateBattleEnd(battleEndRuntime, input);
    battleEndSequenceElapsedSeconds = battleEndRuntime.elapsedSeconds;
    if(effects.initialize)
    {
        battleFeedbackModeValue = 0;
        setStorySaveFlag(persistent.story, 0x02bc, true);
    }
    if(effects.startNotice)
    {
        startBattleEndNotice(battleEndNoticeRuntime);
        GameSoundEvent event = {};
        event.kind = GAME_SOUND_EVENT_EVENT_CUE;
        event.cue = 0x16;
        enqueueSoundEvent(event);
    }
    if(effects.startFade)
    {
        setSceneCharacterSelectCommand(3);
        setSceneCharacterSelectOverlayColor(0, 0, 0);
    }
    if(effects.restore)
    {
        battleFeedbackModeValue = 1;
        setStorySaveFlag(persistent.story, 0x02bc, false);
    }
    if(effects.lockOccupiedActorActions)
        for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
        {
            GameSlotRecord* slot = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
            if(slot != nullptr && (slot->flags & 1u) != 0)
                slot->actorActionFlags |= 4u;
        }
    if(effects.complete)
    {
        battleEndSequenceActive = false;
        battleEndSequenceElapsedSeconds = 0;
        finishBattle(pendingBattleOutcome);
    }
}

void
Game::finishBattle(BattleOutcome outcome)
{
    if(currentBattleOutcome != BATTLE_OUTCOME_IN_PROGRESS)
        return;

    if(currentMode == GAME_MODE_STORY &&
       (persistent.story.battleActive == 0 || activeOpponentCount <= 0))
        return;
    combatDamageEnabled = false;
    currentBattleOutcome = outcome;
    projectGameSlotCombatToPersistentRecord(persistent.player);
    if(allyActive)
        projectGameSlotCombatToPersistentRecord(ally);
    const bool storyVictory =
        currentMode == GAME_MODE_STORY &&
        outcome == BATTLE_OUTCOME_PLAYER_WON;
    battleResult.state =
        battleResult.transitionConfigured || storyVictory ?
            BATTLE_RESULT_CONTROLLER_DISPATCH_TRANSITION :
            BATTLE_RESULT_CONTROLLER_COMPLETE;
    battleResult.selectedIndex = 0;
    battleResult.confirmationSelection = -1;
    battleResult.selectorVisible = false;
    battleResult.confirmationActive = false;
    battleResult.rewardDispatched = false;
    battleFinisherPhase = BATTLE_FINISHER_INACTIVE;
    battleFeedbackRawSecondsRemaining = 0.0f;
    battleEndSequenceActive = false;
    battleEndSequenceElapsedSeconds = 0.0f;
    pendingBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
    battleFeedbackModeValue = 0;
    persistent.story.battleActive = 0;
    persistent.story.battleResultCode =
        outcome == BATTLE_OUTCOME_PLAYER_WON ? 6 : 1;
    synchronizeBattleStoryState();
    if(outcome == BATTLE_OUTCOME_PLAYER_WON)
    {
        for(int i = 0; i < activeOpponentCount; ++i)
        {
            GameSlotRecord& slot =
                i == 0 ? enemy : additionalEnemies[i - 1];
            startAction(slot, COMBAT_ACTION_DEFEATED);
        }
        if(allyActive)
            startAction(ally, COMBAT_ACTION_IDLE);
        startAction(persistent.player, COMBAT_ACTION_VICTORY);
    }
    else
    {
        startAction(persistent.player, COMBAT_ACTION_DEFEATED);
        startAction(targetOpponent(), COMBAT_ACTION_VICTORY);
    }
}

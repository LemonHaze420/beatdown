#include "game.h"
#include "game_combat_gauge.h"
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

float versusOpponentHeading(float playerHeading)
{
    float heading = fmodf(playerHeading + 180.0f, 360.0f);
    if(heading > 180.0f)
        heading -= 360.0f;
    if(heading < -180.0f)
        heading += 360.0f;
    return fabsf(heading) < 180.0f ? heading : 180.0f;
}

void applyVersusBattleFormation(uint16_t stageId,
                                GameSlotRecord& player,
                                GameSlotRecord& opponent)
{

    const VersusBattleAnchorRow& row =
        stageId < VERSUS_BATTLE_ANCHOR_COUNT ?
            VERSUS_BATTLE_ANCHORS[stageId] : VERSUS_BATTLE_ANCHORS[0];
    player.position = {
        static_cast<float>(row.xTenths) * 0.1f,
        static_cast<float>(row.yTenths) * 0.1f + 10.0f,
        static_cast<float>(row.zTenths) * 0.1f};
    player.headingDegrees =
        static_cast<float>(row.headingTenths) * 0.1f;

    const float radians =
        player.headingDegrees * 0.01745329251994329577f;
    opponent.position = {
        player.position.x + sinf(radians) * 25.0f,
        player.position.y,
        player.position.z + cosf(radians) * 25.0f};
    opponent.headingDegrees =
        versusOpponentHeading(player.headingDegrees);
}

void initializeGameSlot(GameSlotRecord& slot, CharacterId characterId, uint8_t slotIndex)
{
    memset(&slot, 0, sizeof(slot));
    initializeActorActionRuntime(slot.actorAction);
    clearCombatReceivedHitContext(slot.receivedContext);
    slot.characterId = static_cast<int16_t>(characterId);
    slot.nativeCharacterId = static_cast<int16_t>(characterId);
    slot.persistentRecordId = static_cast<int16_t>(characterId);
    slot.combatCommand.command = -1;
    slot.battleGroup = 2;
    slot.selectedItemCommand = -1;
    slot.selectedWorldItemIndex = -1;
    slot.heldWorldWeapon = slot.targetedWorldWeapon = -1;
    slot.ownedWorldWeapons[0] = slot.ownedWorldWeapons[1] = -1;
    slot.boundWorldWeapon = slot.defaultBoundWorldWeapon = -1;
    const CharacterRecord* defaults =
        characterRecordForId(static_cast<uint16_t>(characterId));
    const int16_t resourceId =
        defaults != NULL ? defaults->resourceId :
                           static_cast<int16_t>(characterId);
    slot.resourceProfileId = resolveBattleResourceProfile(
        static_cast<int16_t>(characterId), resourceId);
    slot.levelTerm =
        static_cast<float>(defaults != NULL ? defaults->level : 0) * 0.1f +
        11.0f;
    slot.maxHealth = defaultCharacterHealth(characterId);
    slot.currentHealth = slot.maxHealth;
    const int16_t shortGauge =
        static_cast<int16_t>(static_cast<float>(slot.maxHealth) * 0.2f);
    slot.damageGauge = shortGauge;
    slot.damageGaugeMaximum = shortGauge;
    slot.tiredGaugeMaximum =
        static_cast<int16_t>(static_cast<float>(slot.maxHealth) * 1.5f);
    initializeCombatGaugeRuntime(slot);
    for(int i = 0; i < 4; ++i)
    {
        slot.bodyPartHealth[i] = shortGauge;
        slot.auxiliaryGaugeMaximums[i] = shortGauge;
    }
    for(int i = 0; i < COMBAT_ATTACK_GAUGE_COUNT; ++i)
    {
        slot.attackGauges[i] = shortGauge;
        slot.attackGaugeMaximums[i] = shortGauge;
    }
    slot.slotIndex = slotIndex;
    slot.linkedSlotIndex = slotIndex == 0 ? 1 : 0;
    slot.playerIndex = slotIndex;
    slot.dynamicAttackMultiplier = 1.0f;
    slot.attackPowerMultiplier = 1.0f;
    slot.field_0x439c = 0.0f;
    slot.animationTimeScale = 1.0f;
    slot.contactRadiusScale = 1.0f;

    slot.flags = 0x5;

    slot.actorTargetingFlags = 0x1000;
    slot.impactSelectorState = 1;
    slot.impactSelectorStateValid = true;
    slot.interactionRange = 20.0f;
    slot.action = COMBAT_ACTION_IDLE;
    const CharacterRecord* const character =
        characterRecordForId(static_cast<uint16_t>(characterId));
    const int idleMoveId = character != NULL ?
        resolveCharacterMove(
            character->combatStyleId, -MOVE_CHARACTER_ACTION_IDLE) : 1;
    slot.locomotionMoveId =
        static_cast<int16_t>(idleMoveId > 0 ? idleMoveId : 1);
    slot.recoveredMoveId = -1;
    slot.queuedMoveId = -1;
    slot.lastHitAttackerSlot = -1;
    slot.lastHitMoveId = -1;
    slot.receivedContactIndex = -1;
    slot.receivedContactBone = -1;
    slot.npcPositionRowIndex = -1;

    slot.prideGauge = -1;
    slot.maxPrideGauge = -1;

    setGameSlotSelectedAction(slot, MOVE_RECORD_DEFAULT_ACTION);
}

}

void
Game::initializeBattle(bool preserveFieldEquipment)
{
    if(!preserveFieldEquipment)
    {
        resetCombatWeaponAttachments();
        battleResources = {};
    }
    resetBehaviourPresentation();
    installStageSceneSequenceCaption();
    sceneSequencer.setCaptionShadowEnabled(false);

    CharacterId playerCharacter =
        static_cast<CharacterId>(persistent.characterId);
    if(defaultCharacterHealth(playerCharacter) <= 0)
        playerCharacter = CHARACTER_RAVEN;
    const Vec3 livePlayerPosition = persistent.player.position;
    const float livePlayerHeading = persistent.player.headingDegrees;

    const int16_t liveWorldWeapon = persistent.player.heldWorldWeapon;
    const int16_t liveTargetWeapon = persistent.player.targetedWorldWeapon;
    const uint32_t liveArmed = persistent.player.actorActionFlags & 0x4000u;
    const int16_t liveOwnedWeapons[2] = { persistent.player.ownedWorldWeapons[0],
                                        persistent.player.ownedWorldWeapons[1] };
    const uint8_t liveMoveSlotActive[2] = { persistent.player.moveSlotActive[0],
                                          persistent.player.moveSlotActive[1] };
    const int16_t liveBound = persistent.player.boundWorldWeapon;
    const int16_t liveDefaultBound = persistent.player.defaultBoundWorldWeapon;
    const bool liveDefaultPending = persistent.player.defaultWeaponAttachmentPending;
    initializeGameSlot(persistent.player, playerCharacter, 0);
    projectPersistentRecordToGameSlot(
        static_cast<uint16_t>(playerCharacter), persistent.player);
    if(preserveFieldEquipment && combatItemScene != nullptr &&
       liveWorldWeapon >= 0 && liveWorldWeapon < WEAPON_MANAGER_CAPACITY)
    {
        const GameCombatWeaponBinding& binding = combatWeaponBindings[liveWorldWeapon];
        const WeaponInstance* weapon = weaponManager.instance(binding.managerId);
        if(binding.clump != nullptr && weapon != nullptr &&
           weapon->attachment.ownerSlot == persistent.player.slotIndex)
        {
            persistent.player.heldWorldWeapon = liveWorldWeapon;
            persistent.player.targetedWorldWeapon = liveTargetWeapon;
            persistent.player.actorActionFlags |= liveArmed;
        }
    }
    if(preserveFieldEquipment)
    {
        for(int index = 0; index < 2; ++index)
        {
            persistent.player.ownedWorldWeapons[index] = liveOwnedWeapons[index];
            persistent.player.moveSlotActive[index] = liveMoveSlotActive[index];
        }
        persistent.player.boundWorldWeapon = liveBound;
        persistent.player.defaultBoundWorldWeapon = liveDefaultBound;
        persistent.player.defaultWeaponAttachmentPending = liveDefaultPending;
        if(liveArmed == 0)
            persistent.player.heldWorldWeapon = liveWorldWeapon;
    }

    activeOpponentCount = 0;
    allyActive = false;
    memset(&ally, 0, sizeof(ally));
    memset(&enemy, 0, sizeof(enemy));
    memset(additionalEnemies, 0, sizeof(additionalEnemies));

    if(currentMode == GAME_MODE_STORY)
    {
        persistent.player.position = livePlayerPosition;
        persistent.player.headingDegrees = livePlayerHeading;
        refreshStoryProgressionFlags(persistent.story, true);
        recalculatePlayerMaxHealth();

        const uint16_t requestedId = persistent.story.battleId;
        StageActorRuntime* primary = findStageActor(requestedId);
        int primaryActorIndex = -1;
        for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
        {
            stageActors[i].battleAdmitted = false;
            stageActors[i].battleActive = false;
            stageActors[i].primaryEncounter = false;
            stageActors[i].slot.flags &= ~0x100000u;
            if(&stageActors[i] == primary)
                primaryActorIndex = i;
        }
        if(primary == NULL || !primary->occupied ||
           (primary->slot.flags & 1u) == 0 ||
           primaryActorIndex < 0)
        {
            persistent.story.battleActive = 0;
            persistent.story.activeOpponentRosterIndex = -1;
            persistent.story.activeOpponentCharacterId = 0xffffu;
            return;
        }

        PersistentGameRecord* primaryRecord =
            stageActorPersistentRecord(primaryActorIndex);
        if(primaryRecord != NULL)
            primaryRecord->flags_0x388 |= 0x80000000u;
        primary->slot.flags |= 0x100000u;
        int16_t candidates[BATTLE_ROSTER_CANDIDATE_LIMIT] =
        {
            static_cast<int16_t>(requestedId), -1, -1
        };
        const bool groupBattle = primaryRecord != NULL &&
            primaryRecord->encounterMode != 1 &&
            persistent.story.battleEventId != 0x57;
        if(groupBattle)
        {
            const int rosterCount = battleRosterMembers(
                static_cast<int16_t>(persistent.story.battleEventId),
                static_cast<int16_t>(requestedId), candidates,
                BATTLE_ROSTER_CANDIDATE_LIMIT);
            if(rosterCount <= 0)
            {
                candidates[0] = static_cast<int16_t>(requestedId);
                candidates[1] = -1;
                candidates[2] = -1;
            }
        }

        for(int candidateIndex = 0;
            candidateIndex < BATTLE_ROSTER_CANDIDATE_LIMIT;
            ++candidateIndex)
        {
            if(candidates[candidateIndex] < 0 ||
               activeOpponentCount >= MAX_GAME_OPPONENTS)
                continue;
            const uint16_t characterId =
                static_cast<uint16_t>(candidates[candidateIndex]);
            StageActorRuntime* actor = findStageActor(characterId);
            if(actor == NULL || !actor->occupied ||
               (actor->slot.flags & 1u) == 0)
                continue;
            const int actorIndex =
                static_cast<int>(actor - stageActors);
            PersistentGameRecord* record =
                stageActorPersistentRecord(actorIndex);
            if(record == NULL || record->currentHealth <= 0 ||
               (record->characterFlags & 1u) != 0)
                continue;

            const Vec3 transform = actor->slot.position;
            const float heading = actor->slot.headingDegrees;
            GameSlotRecord admitted = actor->slot;
            if(!applyBattlePersistentStats(
                   admitted, characterId,
                   static_cast<uint16_t>(actor->secondaryCharacterId),
                   false))
                continue;
            admitted.position = transform;
            admitted.headingDegrees = heading;
            admitted.linkedSlotIndex = 0;
            admitted.battleTeamOrder =
                static_cast<uint8_t>(activeOpponentCount);
            admitted.battleSide = 1;
            admitted.battleGroup = 1;

            admitted.battleActionState =
                groupBattle && characterId == requestedId ? 9 : 0x0f;
            if(characterId == requestedId)
                admitted.flags |= 0x100000u;
            else
                admitted.flags &= ~0x100000u;
            ++record->field_0x008;

            if(persistent.story.battlePlural)
            {
                const CharacterRecord* defaults =
                    characterRecordForId(characterId);
                if(defaults != NULL)
                {
                    record->prideGauge = defaults->initialPrideGauge;
                    admitted.prideGauge = defaults->initialPrideGauge;
                }
            }

            actor->slot = admitted;
            actor->battleAdmitted = true;
            actor->primaryEncounter = characterId == requestedId;
            if(activeOpponentCount == 0)
                enemy = admitted;
            else
                additionalEnemies[activeOpponentCount - 1] = admitted;
            ++activeOpponentCount;
        }

        if(activeOpponentCount <= 0)
        {
            persistent.story.battleActive = 0;
            persistent.story.activeOpponentRosterIndex = -1;
            persistent.story.activeOpponentCharacterId = 0xffffu;
            return;
        }

        const float dx =
            persistent.player.position.x - primary->slot.position.x;
        const float dz =
            persistent.player.position.z - primary->slot.position.z;
        if(dx != 0.0f || dz != 0.0f)
            primary->slot.headingDegrees =
                atan2f(dx, dz) * 57.295779513082320876f;
        for(int i = 0; i < activeOpponentCount; ++i)
        {
            GameSlotRecord& slot =
                i == 0 ? enemy : additionalEnemies[i - 1];
            if(slot.characterId == static_cast<int16_t>(requestedId))
                slot.headingDegrees = primary->slot.headingDegrees;
        }

        persistent.story.activeOpponentRosterIndex =
            static_cast<int16_t>(primaryActorIndex);
        persistent.story.activeOpponentCharacterId = requestedId;
        persistent.story.battleFlags |= 0x202u;
        persistent.player.linkedSlotIndex = enemy.slotIndex;
        spawnBattleAlly();
    }
    else
    {
        const CharacterId enemyCharacter = static_cast<CharacterId>(
            VERSUS_CHARACTER_IDS[selectedVersusCharacters[1]]);
        initializeGameSlot(enemy, enemyCharacter, 1);
        enemy.battleGroup = 1;
        if(selectedVersusBattleType == VERSUS_PLAYER_VS_PLAYER)
        {
            persistent.player.playerIndex = 0;
            enemy.playerIndex = 1;
        }
        else if(selectedVersusBattleType == VERSUS_CPU_VS_CPU)
        {
            persistent.player.playerIndex = 2;
            enemy.playerIndex = 3;
        }
        else if(versusHumanSideValue == 0)
        {
            persistent.player.playerIndex = 0;
            enemy.playerIndex = 3;
        }
        else
        {
            persistent.player.playerIndex = 2;
            enemy.playerIndex = 1;
        }
        applyVersusBattleFormation(
            persistent.stageId, persistent.player, enemy);
        persistent.player.linkedSlotIndex = 1;
        enemy.linkedSlotIndex = 0;
        initialiseSlotPride(persistent.player, playerCharacter);
        initialiseSlotPride(enemy, enemyCharacter);
        activeOpponentCount = 1;
    }

    bindNpcPositionRow(persistent.player);
    if(!preserveFieldEquipment)
        initializeCombatOwnedWeapons();
    initializeCombatProjectileGroups();
    captureBattleRewardRoster();
    battleComboBonus = 0;
    lastBattleExperience = 0;
    lastBattleLevelsGained = 0;
    battleSupplementalExperience = 0;
    storyEncounterCompletionPending = false;
    persistent.story.stageTransition.postBattleFlagsApplied = 0;
    targetOpponentIndex = 0;
    frameCounter = static_cast<uint32_t>(persistent.stageId);
    seedGameRandom(random, frameCounter);
    loadStageAiResources();
    initializeStageAiBattleContexts();

    if(currentMode != GAME_MODE_STORY)
    {
        for(int i = -1; i < activeOpponentCount; ++i)
        {
            GameSlotRecord& slot = i < 0 ? persistent.player :
                (i == 0 ? enemy : additionalEnemies[i - 1]);
            groundFighter(slot);
        }
    }
    pendingBattleSpawnValid = false;
    currentBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
    battleElapsedSeconds = 0.0f;
    resetBattleResultController();
    persistent.player.battleGroup = 0;
    initializeBattleController();
    battleResult.sideActiveCounts[0] = allyActive ? 2 : 1;
    selectedPauseItem = 0;
    pauseMoveListOpen = false;
    pauseMoveListFirst = 0;
    advicePendingValue = false;
    currentScreen = GAME_SCREEN_GAMEPLAY;
    switchGameFlowTaskHandler(updateGameplaySessionFlow, 1);
    persistent.story.battleActive = 1;
    persistent.story.battleId =
        static_cast<uint16_t>(enemy.characterId);
    persistent.story.battleResultCode = -1;

    applyInitialCombatantStance(persistent.player);
    if(allyActive)
        applyInitialCombatantStance(ally);
    applyInitialCombatantStance(enemy);
    for(int i = 1; i < activeOpponentCount; i++)
        applyInitialCombatantStance(additionalEnemies[i - 1]);
    synchronizeBattleStoryState();
}

GameSlotRecord&
Game::targetOpponent(void)
{
    return targetOpponentIndex == 0 ?
        enemy : additionalEnemies[targetOpponentIndex - 1];
}

const GameSlotRecord&
Game::targetOpponent(void) const
{
    return targetOpponentIndex == 0 ?
        enemy : additionalEnemies[targetOpponentIndex - 1];
}

void
Game::updateTargetOpponent(void)
{
    if((pressedInput & GAME_INPUT_CONFIRM) != 0 &&
       activeOpponentCount > 1)
    {
        for(int step = 1; step <= activeOpponentCount; step++)
        {
            const int candidate =
                (targetOpponentIndex + step) % activeOpponentCount;
            const GameSlotRecord& candidateSlot = candidate == 0 ?
                enemy : additionalEnemies[candidate - 1];
            if(candidateSlot.currentHealth > 0)
            {
                targetOpponentIndex = candidate;
                persistent.player.linkedSlotIndex = candidateSlot.slotIndex;
                persistent.player.actorTargetingFlags |= 0x1000;
                return;
            }
        }
    }

    if(targetOpponentIndex < 0 || targetOpponentIndex >= activeOpponentCount)
        targetOpponentIndex = 0;

    if((persistent.player.actorTargetingFlags & 0x1000) != 0 &&
       targetOpponent().currentHealth > 0)
        return;

    persistent.player.actorTargetingFlags &= ~0x1000u;
    float nearestDistance = 0.0f;
    int nearest = targetOpponentIndex;
    bool found = false;
    for(int i = 0; i < activeOpponentCount; i++)
    {
        const GameSlotRecord& candidate =
            i == 0 ? enemy : additionalEnemies[i - 1];
        if(candidate.currentHealth <= 0)
            continue;
        const float distance =
            horizontalDistanceSquared(persistent.player, candidate);
        if(!found || distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = i;
            found = true;
        }
    }
    if(found)
    {
        targetOpponentIndex = nearest;
        const GameSlotRecord& candidate = targetOpponent();
        persistent.player.linkedSlotIndex = candidate.slotIndex;
    }
}

bool
Game::allOpponentsDefeated(void) const
{
    if(stageAiGeneratorsPending())
        return false;
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        const GameSlotRecord& opponent =
            i == 0 ? enemy : additionalEnemies[i - 1];
        if(opponent.currentHealth <= 0)
            continue;
        if((persistent.story.battleFlags & 0x200u) == 0)
            return false;
        const GameSlotRecord& target = boundTargetOf(opponent);
        const float dx = opponent.position.x - target.position.x;
        const float dz = opponent.position.z - target.position.z;
        if(dx * dx + dz * dz <= 200.0f * 200.0f &&
           fabsf(opponent.position.y - target.position.y) <= 40.0f)
            return false;
    }
    return true;
}

void
Game::updateBattle(float deltaSeconds)
{
    if(persistent.story.countdownFrames > 0)
    {
        int frames = static_cast<int>(deltaSeconds * 60.0f + 0.5f);
        if(frames < 1)
            frames = 1;
        persistent.story.countdownFrames -= frames;
        if(persistent.story.countdownFrames < 0)
            persistent.story.countdownFrames = 0;
    }

    if(currentMode == GAME_MODE_STORY &&
       persistent.story.battleActive == 0 &&
       currentBattleOutcome == BATTLE_OUTCOME_IN_PROGRESS)
    {
        updatePlayerMovement(deltaSeconds);
        updateCombatMoveRuntime(persistent.player);
        drainCombatActionPairs();
        moveFighter(
            persistent.player,
            persistent.player.velocity.x * deltaSeconds,
            persistent.player.velocity.z * deltaSeconds);
        if(persistent.player.action != COMBAT_ACTION_MOVE)
        {
            const float damping = powf(0.02f, deltaSeconds);
            persistent.player.velocity.x *= damping;
            persistent.player.velocity.z *= damping;
            if(fabsf(persistent.player.velocity.x) < 0.01f)
                persistent.player.velocity.x = 0.0f;
            if(fabsf(persistent.player.velocity.z) < 0.01f)
                persistent.player.velocity.z = 0.0f;
        }
        clampFighterToArena(persistent.player);
        groundFighter(persistent.player);
        updateInteraction();
        synchronizeBattleStoryState();
        return;
    }
    if(currentBattleOutcome != BATTLE_OUTCOME_IN_PROGRESS)
    {
        updateBattleResultController();
        return;
    }
    if(battleFinisherPhase != BATTLE_FINISHER_INACTIVE)
    {
        runCombatStateTask(persistent.player);
        if(allyActive)
            runCombatStateTask(ally);
        runCombatStateTask(enemy);
        for(int i = 1; i < activeOpponentCount; ++i)
            runCombatStateTask(additionalEnemies[i - 1]);
        updateCombatMoveRuntime(persistent.player);
        if(allyActive)
            updateCombatMoveRuntime(ally);
        updateCombatMoveRuntime(enemy);
        for(int i = 1; i < activeOpponentCount; ++i)
            updateCombatMoveRuntime(additionalEnemies[i - 1]);
        drainCombatActionPairs();
        updateBattleFinisher(deltaSeconds, 0.0f);
        synchronizeBattleStoryState();
        return;
    }
    if(battleEndSequenceActive)
    {
        updateCombatMoveRuntime(persistent.player);
        if(allyActive)
            updateCombatMoveRuntime(ally);
        updateCombatMoveRuntime(enemy);
        for(int i = 1; i < activeOpponentCount; ++i)
            updateCombatMoveRuntime(additionalEnemies[i - 1]);
        drainCombatActionPairs();
        updateBattleEndSequence(deltaSeconds);
        synchronizeBattleStoryState();
        return;
    }

    battleElapsedSeconds += deltaSeconds;
    syncCombatTargetsFromStory();
    updateTargetOpponent();
    updateCombatDistanceFields();
    updateCombatFacingFields();

    runCombatStateTask(persistent.player);
    if(allyActive)
        runCombatStateTask(ally);
    runCombatStateTask(enemy);
    for(int i = 1; i < activeOpponentCount; i++)
        runCombatStateTask(additionalEnemies[i - 1]);

    updateCombatantStance(persistent.player);
    if(allyActive)
        updateCombatantStance(ally);
    updateCombatantStance(enemy);
    for(int i = 1; i < activeOpponentCount; i++)
        updateCombatantStance(additionalEnemies[i - 1]);

    if(currentMode == GAME_MODE_STORY)
    {
        updatePlayerMovement(deltaSeconds);

        updateStageAiOpponent(enemy, boundTarget(enemy), deltaSeconds);
        for(int i = 1; i < activeOpponentCount; i++)
            updateStageAiOpponent(additionalEnemies[i - 1],
                                  boundTarget(additionalEnemies[i - 1]),
                                  deltaSeconds);
    }
    else if(selectedVersusBattleType == VERSUS_PLAYER_VS_PLAYER)
    {
        updatePlayerMovement(deltaSeconds);
        updateHumanOpponentMovement(deltaSeconds);
    }
    else if(selectedVersusBattleType == VERSUS_CPU_VS_CPU)
    {
        updateEnemyAI(persistent.player, enemy, deltaSeconds);
        updateEnemyAI(enemy, persistent.player, deltaSeconds);
    }
    else if(versusHumanSideValue == 0)
    {
        updatePlayerMovement(deltaSeconds);
        updateEnemyAI(enemy, persistent.player, deltaSeconds);
    }
    else
    {
        updateEnemyAI(persistent.player, enemy, deltaSeconds);
        updateHumanOpponentMovement(deltaSeconds);
    }

    if(allyActive && ally.currentHealth > 0)
    {
        GameSlotRecord* const foe =
            const_cast<GameSlotRecord*>(allyEngagedTarget());
        if(foe != NULL)
            updateEnemyAI(ally, *foe, deltaSeconds);
    }

    updateCombatMoveRuntime(persistent.player);
    if(allyActive)
        updateCombatMoveRuntime(ally);
    updateCombatMoveRuntime(enemy);
    for(int i = 1; i < activeOpponentCount; ++i)
        updateCombatMoveRuntime(additionalEnemies[i - 1]);
    drainCombatActionPairs();
    updateCombatMotionRuntime();
    updateCombatInteractions();

    for(int i = -2; i < activeOpponentCount; i++)
    {
        if(i == -1 && !allyActive)
            continue;
        GameSlotRecord& slot = i < -1 ? persistent.player :
            (i < 0 ? ally :
             (i == 0 ? enemy : additionalEnemies[i - 1]));
        moveFighter(slot, slot.velocity.x * deltaSeconds,
                    slot.velocity.z * deltaSeconds);
        if(slot.action != COMBAT_ACTION_MOVE)
        {
            const float damping = powf(0.02f, deltaSeconds);
            slot.velocity.x *= damping;
            slot.velocity.z *= damping;
            if(fabsf(slot.velocity.x) < 0.01f)
                slot.velocity.x = 0.0f;
            if(fabsf(slot.velocity.z) < 0.01f)
                slot.velocity.z = 0.0f;
        }
        clampFighterToArena(slot);
        groundFighter(slot);
    }

    for(int i = 0; i < activeOpponentCount; i++)
        separateFighters(i == 0 ? enemy : additionalEnemies[i - 1]);
    for(int i = 0; i < activeOpponentCount; i++)
    {
        GameSlotRecord& first =
            i == 0 ? enemy : additionalEnemies[i - 1];
        for(int j = i + 1; j < activeOpponentCount; j++)
        {
            GameSlotRecord& second = additionalEnemies[j - 1];
            float x = second.position.x - first.position.x;
            float z = second.position.z - first.position.z;
            float distanceSquared = x * x + z * z;
            if(distanceSquared >= 4.5f * 4.5f)
                continue;
            if(distanceSquared < 0.0001f)
            {
                x = 1.0f;
                z = 0.0f;
                distanceSquared = 1.0f;
            }
            const float distance = sqrtf(distanceSquared);
            const float correction =
                (4.5f - distance) * 0.5f / distance;
            first.position.x -= x * correction;
            first.position.z -= z * correction;
            second.position.x += x * correction;
            second.position.z += z * correction;
        }
    }
    updateCombatDistanceFields();
    updateCombatFacingFields();
    updateInteraction();
    scoreDefeatedBattleOpponents();

    if(currentMode != GAME_MODE_STORY && battleController.active)
        updateBattleController(deltaSeconds);
    else if(allOpponentsDefeated())
        requestBattleCompletion(BATTLE_OUTCOME_PLAYER_WON);
    else if(persistent.player.currentHealth == 0)
        requestBattleCompletion(BATTLE_OUTCOME_PLAYER_LOST);
    synchronizeBattleStoryState();
}

void
Game::spawnBattleAlly(void)
{
    allyActive = false;
    memset(&ally, 0, sizeof(ally));

    if(currentMode != GAME_MODE_STORY)
        return;

    uint16_t actorId = 0xffffu;
    size_t partyIndex = STORY_PARTY_COUNT;
    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
    {
        if(persistent.story.party[i] == 0xffffu)
            continue;

        if(persistent.story.party[i] == persistent.player.characterId)
            continue;
        actorId = persistent.story.party[i];
        partyIndex = static_cast<size_t>(i);
        break;
    }
    if(actorId == 0xffffu)
        return;

    if(!validPlayableCharacter(actorId))
        return;
    const CharacterId character = static_cast<CharacterId>(actorId);
    if(gameData.characterStats(characterDatabaseIndex(character)) == NULL)
        return;

    PersistentGameRecord* const partyRecord = characterRecord(actorId);
    if(partyRecord != NULL && partyRecord->level == 0)
        initializePersistentRecord(*partyRecord, actorId);
    if(partyIndex < STORY_PARTY_COUNT &&
       actorId < GAME_SAVE_RECORD_COUNT)
        setActiveRecordId(partyIndex, static_cast<int16_t>(actorId));

    initializeGameSlot(ally, character, 1);

    ally.playerIndex = 2;
    ally.battleGroup = 0;

    ally.position = persistent.player.position;
    ally.position.x -= 6.0f;
    ally.position.z -= 4.0f;
    ally.headingDegrees = persistent.player.headingDegrees;

    ally.linkedSlotIndex = enemy.slotIndex;

    ally.battleActionState = 0x0f;
    initialiseSlotPride(ally, character);
    projectPersistentRecordToGameSlot(actorId, ally);
    bindNpcPositionRow(ally);
    allyActive = true;
}

const GameSlotRecord*
Game::combatSlotByIndex(uint8_t index) const
{
    if(index == persistent.player.slotIndex)
        return &persistent.player;
    if(allyActive && index == ally.slotIndex)
        return &ally;
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        const GameSlotRecord& opponent =
            i == 0 ? enemy : additionalEnemies[i - 1];
        if(opponent.slotIndex == index)
            return &opponent;
    }
    return NULL;
}

GameSlotRecord*
Game::mutableCombatSlotByIndex(uint8_t index)
{
    return const_cast<GameSlotRecord*>(
        static_cast<const Game*>(this)->combatSlotByIndex(index));
}

const GameSlotRecord&
Game::boundTargetOf(const GameSlotRecord& actor) const
{
    const GameSlotRecord* const bound =
        combatSlotByIndex(actor.linkedSlotIndex);
    if(bound == NULL || bound == &actor || bound->currentHealth == 0)
        return persistent.player;
    return *bound;
}

GameSlotRecord&
Game::boundTarget(const GameSlotRecord& actor)
{
    return const_cast<GameSlotRecord&>(boundTargetOf(actor));
}

void
Game::syncCombatTargetsFromStory(void)
{
    for(int i = -1; i < activeOpponentCount; ++i)
    {
        if(i < 0 && !allyActive)
            continue;
        GameSlotRecord& slot = i < 0 ? ally :
            (i == 0 ? enemy : additionalEnemies[i - 1]);
        const StoryActorState* const actor = findStoryActor(
            persistent.story, slot.characterId, false);
        if(actor == NULL || actor->targetActorId == 0 ||
           actor->targetActorId == 0xffffu)
            continue;

        for(int j = -1; j < activeOpponentCount; ++j)
        {
            if(j < 0 && !allyActive)
                continue;
            const GameSlotRecord& candidate = j < 0 ? ally :
                (j == 0 ? enemy : additionalEnemies[j - 1]);
            if(candidate.characterId != actor->targetActorId)
                continue;
            slot.linkedSlotIndex = candidate.slotIndex;
            break;
        }
        if(persistent.player.characterId == actor->targetActorId)
            slot.linkedSlotIndex = persistent.player.slotIndex;
    }
}

const GameSlotRecord*
Game::allyEngagedTarget(void) const
{
    if(!allyActive || ally.currentHealth == 0)
        return NULL;

    const GameSlotRecord* const bound = combatSlotByIndex(ally.linkedSlotIndex);
    if(bound != NULL && bound != &ally && bound != &persistent.player &&
       bound->currentHealth > 0)
        return bound;
    return const_cast<Game*>(this)->allyTarget();
}

GameSlotRecord*
Game::allyTarget(void)
{
    GameSlotRecord* best = NULL;
    float bestDistance = 0.0f;
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        GameSlotRecord& slot = i == 0 ? enemy : additionalEnemies[i - 1];
        if(slot.currentHealth == 0)
            continue;
        const float distance = horizontalDistanceSquared(ally, slot);
        if(best == NULL || distance < bestDistance)
        {
            best = &slot;
            bestDistance = distance;
        }
    }
    return best;
}

bool Game::hasAlly(void) const { return allyActive; }

const GameSlotRecord& Game::allySlot(void) const { return ally; }

BattleOutcome Game::battleOutcome(void) const { return currentBattleOutcome; }

void
Game::applyCamdatActorActionFlag(uint8_t actorSlot, bool enabled)
{
    GameSlotRecord* actor =
        const_cast<GameSlotRecord*>(combatSlotByIndex(actorSlot));
    if(actor == NULL)
        return;
    GameSlotRecord& linked = boundTarget(*actor);
    if(enabled)
    {
        actor->actorActionFlags |= 4u;
        linked.actorActionFlags |= 4u;
    }
    else
    {
        actor->actorActionFlags &= ~4u;
        linked.actorActionFlags &= ~4u;
    }
}

float Game::battleTime(void) const { return battleElapsedSeconds; }

bool Game::stageAiLoaded(void) const { return stageAiResourceLoaded; }

const char* Game::stageAiDiagnostic(void) const
{
    return stageAiDiagnosticText.c_str();
}

const CombatAiDecision& Game::stageAiDecision(void) const
{
    return stageAiLastDecision;
}

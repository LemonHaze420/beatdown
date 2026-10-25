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

void Game::resetInventoryUseState(void)
{
    memset(&inventoryUse, 0, sizeof(inventoryUse));
    inventoryUse.phase = INVENTORY_USE_LIST;
    inventoryUse.packedEntryIndex = UINT16_MAX;
    for(int i = 0; i < 3; ++i)
        inventoryUse.targetCharacterIds[i] = UINT16_MAX;
}

const InventoryUseState& Game::inventoryUseState(void) const
{
    return inventoryUse;
}

bool Game::inventoryActionRowEnabled(int row) const
{
    return inventoryUse.phase == INVENTORY_USE_ACTION &&
        actionRowEnabled(inventoryUse.actionClass, row);
}

bool Game::beginInventoryAction(uint16_t packedEntryIndex)
{
    const OwnedInventoryBlock& owned = ownedInventory();
    if(packedEntryIndex >= owned.count ||
       packedEntryIndex >= GAME_SAVE_INVENTORY_CAPACITY)
        return false;
    const uint32_t packed = owned.entries[packedEntryIndex];
    const uint16_t itemId = packedInventoryItemId(packed);
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    if(definition == NULL || !definition->consumable ||
       packedInventoryQuantity(packed) == 0)
        return false;
    resetInventoryUseState();
    inventoryUse.phase = INVENTORY_USE_ACTION;
    inventoryUse.packedEntryIndex = packedEntryIndex;
    inventoryUse.packedEntry = packed;
    inventoryUse.itemId = itemId;
    inventoryUse.variant = packedInventoryVariant(packed);
    inventoryUse.actionClass = 1;
    for(int row = 0; row < 3; ++row)
        if(actionRowEnabled(inventoryUse.actionClass, row))
        {
            inventoryUse.actionSelection = static_cast<uint8_t>(row);
            break;
        }
    return true;
}

void Game::populateInventoryTargets(void)
{
    inventoryUse.targetCount = 0;
    inventoryUse.targetSelection = 0;
    for(int row = 0; row < 3; ++row)
    {
        const int16_t id = activeRecordId(static_cast<size_t>(row));
        inventoryUse.targetCharacterIds[row] =
            row == 0 ? (id >= 0 ? static_cast<uint16_t>(id) :
                        persistent.characterId) :
            (id > 0 ? static_cast<uint16_t>(id) : UINT16_MAX);
        if(inventoryUse.targetCharacterIds[row] != UINT16_MAX)
            ++inventoryUse.targetCount;
    }
}

bool Game::moveInventoryActionCursor(int direction)
{
    const int step = direction < 0 ? -1 : 1;
    int row = inventoryUse.phase == INVENTORY_USE_ACTION ?
        inventoryUse.actionSelection : inventoryUse.targetSelection;
    if(inventoryUse.phase == INVENTORY_USE_ACTION ||
       inventoryUse.phase == INVENTORY_USE_TARGET)
        for(int i = 0; i < 3; ++i)
        {
            row = wrapIndex(row + step, 3);
            if((inventoryUse.phase == INVENTORY_USE_ACTION &&
                actionRowEnabled(inventoryUse.actionClass, row)) ||
               (inventoryUse.phase == INVENTORY_USE_TARGET &&
                inventoryUse.targetCharacterIds[row] != UINT16_MAX))
            {
                if(inventoryUse.phase == INVENTORY_USE_ACTION)
                    inventoryUse.actionSelection = static_cast<uint8_t>(row);
                else
                    inventoryUse.targetSelection = static_cast<uint8_t>(row);
                return true;
            }
        }
    if(inventoryUse.phase == INVENTORY_USE_CONFIRM)
    {
        inventoryUse.confirmSelection = static_cast<uint8_t>(
            wrapIndex(inventoryUse.confirmSelection + step, 2));
        return true;
    }
    return false;
}

bool Game::tryApplyTemporarySlowCondition(void)
{
    if(trackedOpponentCondition || inventoryConditionGameStateBitOne)
        return false;
    temporarySlowActive = true;
    temporarySlowElapsed = 0.0f;
    temporarySlowMode = 3;
    temporarySlowBattleSerial = frameCounter;
    if(!eventFlag(0x36))
    {
        setEventFlag(0x36, true);
        setEventFlag(0x37, true);
    }
    setEventFlag(0x3b, true);
    return true;
}

void Game::clearTemporarySlowCondition(void)
{
    temporarySlowActive = false;
    temporarySlowMode = 0;
    setEventFlag(0x3b, false);
    setEventFlag(0x37, false);
}

bool Game::temporarySlowConditionActive(void) const
{
    return temporarySlowActive;
}

float Game::temporarySlowConditionElapsed(void) const
{
    return temporarySlowElapsed;
}

uint8_t Game::temporarySlowConditionMode(void) const
{
    return temporarySlowMode;
}

void Game::setInventoryConditionBlockers(bool tracked, bool gameStateBitOne)
{
    trackedOpponentCondition = tracked;
    inventoryConditionGameStateBitOne = gameStateBitOne;
}

void Game::advanceTemporarySlowCondition(float seconds)
{
    if(!temporarySlowActive || seconds <= 0.0f)
        return;
    temporarySlowElapsed += seconds;
    if(temporarySlowElapsed > 45.0f)
    {
        clearTemporarySlowCondition();
        setEventFlag(0x3a, true);
    }
}

uint8_t Game::applyInventoryItemEffect(
    uint16_t itemId, uint16_t targetCharacterId, uint32_t randomValue)
{
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    return definition != NULL && definition->consumable ?
        applyInventoryItemEffect(definition->useEffect, targetCharacterId,
                                 randomValue) : 0;
}

uint8_t Game::applyInventoryItemEffect(
    const ItemUseEffect& effect, uint16_t targetCharacterId,
    uint32_t randomValue)
{
    PersistentGameRecord* record = characterRecord(targetCharacterId);
    if(record == NULL)
        return 0;
    GameSlotRecord* live = NULL;
    if(persistent.player.characterId ==
       static_cast<int16_t>(targetCharacterId))
        live = &persistent.player;
    else if(allyActive &&
            ally.characterId == static_cast<int16_t>(targetCharacterId))
        live = &ally;
    return applyInventoryItemEffect(effect, targetCharacterId, randomValue,
                                    live, true, true);
}

uint8_t Game::applyInventoryItemEffect(
    const ItemUseEffect& effect, uint16_t targetCharacterId,
    uint32_t randomValue, GameSlotRecord* live,
    bool persistHealth, bool persistPride)
{
    PersistentGameRecord* record = characterRecord(targetCharacterId);
    if(record == NULL)
        return 0;
    const bool playerTarget = record->characterId == 0;
    uint16_t heal = effect.healthAmount;
    bool slowCapable =
        (effect.flags & ITEM_EFFECT_MAY_SLOW_PLAYER) != 0;
    bool changed = false;
    if(effect.slowRule == ITEM_SLOW_UNLESS_RANDOM16_HEALS)
    {
        if(!playerTarget ||
           (randomValue & 0xffffu) < effect.healthChanceThreshold)
            slowCapable = false;
        else
            heal = 0;
    }
    if((effect.flags & ITEM_EFFECT_SET_ALL_RELATIONSHIPS) != 0)
        for(uint16_t id = 1; id < GAME_SAVE_RECORD_COUNT; ++id)
        {
            primarySaveImage.payload.persistentRecords[id].relationshipState = 1;
            persistent.story.characters[id].relationshipState = 1;
        }
    if(effect.maxHealthAmount != 0)
    {
        int maximum = (live != NULL ? live->maxHealth : record->maxHealth) +
            effect.maxHealthAmount;
        record->maxHealth =
            static_cast<int16_t>(maximum > 9999 ? 9999 : maximum);
        int growth = record->characterValues[2] + effect.maxHealthAmount;
        record->characterValues[2] =
            static_cast<int16_t>(growth > 9999 ? 9999 : growth);
        if(live != NULL)
            live->maxHealth = record->maxHealth;
        changed = true;
    }
    if((effect.flags & ITEM_EFFECT_RESTORE_PRIDE) != 0)
    {
        const CharacterRecord* defaults =
            characterRecordForId(targetCharacterId);
        if(defaults != NULL)
        {
            if(persistPride)
                record->prideGauge = defaults->initialPrideGauge;
            if(live != NULL)
                live->prideGauge = defaults->initialPrideGauge;
            if(persistPride && live == &persistent.player)
                persistent.story.pride = defaults->initialPrideGauge;
        }
        changed = true;
    }
    if(heal != 0)
    {
        int health = (live != NULL ? live->currentHealth :
                      record->currentHealth) + heal;
        if(health > record->maxHealth)
            health = record->maxHealth;
        if(persistHealth)
            record->currentHealth = static_cast<int16_t>(health);
        if(live != NULL)
            live->currentHealth = static_cast<int16_t>(health);
        if(health >= record->maxHealth)
        {
            memset(record->damageLevelsA, 0, sizeof(record->damageLevelsA));
            memset(record->damageLevelsB, 0, sizeof(record->damageLevelsB));
            if(live != NULL)
                memset(live->bodyPartDamage, 0, sizeof(live->bodyPartDamage));
        }
        changed = true;
    }
    if(slowCapable)
    {
        const bool shouldSlow =
            effect.slowRule == ITEM_SLOW_UNLESS_RANDOM16_HEALS ?
                playerTarget :
                effect.slowRule == ITEM_SLOW_RANDOM15_ABOVE_THRESHOLD &&
                effect.slowThreshold < (randomValue & 0x7fffu) &&
                playerTarget;
        if(shouldSlow)
            tryApplyTemporarySlowCondition();
        changed = true;
    }
    return changed ? effect.effectCode : 0;
}

void Game::setInventoryResultPrompt(
    int32_t selector, bool newlySlowed, bool alreadySlow)
{
    uint8_t p = 0;
    switch(selector)
    {
    case 1: p = newlySlowed ? 29 : 4; break;
    case 2: p = 5; break;
    case 93: p = 10; break; case 94: p = 11; break; case 95: p = 12; break;
    case 96: p = newlySlowed ? 30 : 13; break;
    case 97: p = newlySlowed ? 31 : 14; break;
    case 98: p = 15; break; case 100: case 102: p = 17; break;
    case 103: p = 18; break;
    case 108: p = newlySlowed ? 24 : 19; break;
    case 109: p = newlySlowed ? 25 : 20; break;
    case 110: p = newlySlowed ? 26 : 21; break;
    case 111: p = newlySlowed ? 27 : 22; break;
    case 112: p = alreadySlow ? 34 : (newlySlowed ? 28 : 23); break;
    }
    inventoryUse.resultPromptIndex = p;
    inventoryUse.resultTextCount = 1;
    inventoryUse.resultTextIds[1] = 0;
    switch(p)
    {
    case 4: inventoryUse.resultTextIds[0] = 0x3277; break;
    case 5: inventoryUse.resultTextIds[0] = 0x3265; break;
    case 10: inventoryUse.resultTextIds[0] = 0x3267; break;
    case 11: inventoryUse.resultTextIds[0] = 0x3268; break;
    case 12: inventoryUse.resultTextIds[0] = 0x3269; break;
    case 13: inventoryUse.resultTextIds[0] = 0x3271; break;
    case 14: inventoryUse.resultTextIds[0] = 0x3272; break;
    case 15: inventoryUse.resultTextIds[0] = 0x3266; break;
    case 17: inventoryUse.resultTextIds[0] = 0x3270; break;
    case 18: inventoryUse.resultTextIds[0] = 0x3276; break;
    case 19: inventoryUse.resultTextIds[0] = 0x3265; break;
    case 20: inventoryUse.resultTextIds[0] = 0x3279; break;
    case 21: case 22: case 23:
        inventoryUse.resultTextIds[0] = 0x3272; break;
    case 24: inventoryUse.resultTextIds[0] = 0x3265;
        inventoryUse.resultTextIds[1] = 0x3274; ++inventoryUse.resultTextCount; break;
    case 25: inventoryUse.resultTextIds[0] = 0x3279;
        inventoryUse.resultTextIds[1] = 0x327a; ++inventoryUse.resultTextCount; break;
    case 26: case 27: case 31:
        inventoryUse.resultTextIds[0] = 0x3272;
        inventoryUse.resultTextIds[1] = 0x3274; ++inventoryUse.resultTextCount; break;
    case 28: inventoryUse.resultTextIds[0] = 0x3274; break;
    case 29: inventoryUse.resultTextIds[0] = 0x3277;
        inventoryUse.resultTextIds[1] = 0x3274; ++inventoryUse.resultTextCount; break;
    case 30: inventoryUse.resultTextIds[0] = 0x3271;
        inventoryUse.resultTextIds[1] = 0x3274; ++inventoryUse.resultTextCount; break;
    case 34: inventoryUse.resultTextIds[0] = 0x3275; break;
    default: inventoryUse.resultTextCount = 0;
        inventoryUse.resultTextIds[0] = 0; break;
    }
    inventoryUse.resultGateFrames = selector == 103 ? 0 : 45;
}

bool Game::confirmInventoryAction(void)
{
    if(inventoryUse.phase == INVENTORY_USE_ACTION)
    {
        if(!actionRowEnabled(inventoryUse.actionClass,
                             inventoryUse.actionSelection))
            return false;
        if(inventoryUse.actionSelection == 1)
        {
            populateInventoryTargets();
            inventoryUse.phase = INVENTORY_USE_TARGET;
            return true;
        }
        if(inventoryUse.actionSelection == 2)
        {
            resetInventoryUseState();
            return true;
        }
        return false;
    }
    if(inventoryUse.phase == INVENTORY_USE_TARGET)
    {
        if(inventoryUse.targetCharacterIds[inventoryUse.targetSelection] ==
           UINT16_MAX)
            return false;
        inventoryUse.confirmSelection = 0;
        inventoryUse.phase = INVENTORY_USE_CONFIRM;
        return true;
    }
    if(inventoryUse.phase == INVENTORY_USE_CONFIRM)
    {
        if(inventoryUse.confirmSelection != 0)
        {
            inventoryUse.phase = INVENTORY_USE_TARGET;
            return true;
        }
        const OwnedInventoryBlock& owned = ownedInventory();
        const uint32_t currentPacked =
            inventoryUse.packedEntryIndex < owned.count ?
            owned.entries[inventoryUse.packedEntryIndex] : 0;
        if(inventoryUse.packedEntryIndex >= owned.count ||
           packedInventoryItemId(currentPacked) != inventoryUse.itemId ||
           packedInventoryVariant(currentPacked) != inventoryUse.variant ||
           packedInventoryQuantity(currentPacked) == 0)
        {
            resetInventoryUseState();
            return false;
        }
        const bool female = persistent.story.customization.genderSelector != 0;
        const InventoryItemDefinition* definition =
            gameData.item(inventoryUse.itemId, female);
        if(definition == NULL || !definition->consumable)
        {
            resetInventoryUseState();
            return false;
        }
        const uint16_t target =
            inventoryUse.targetCharacterIds[inventoryUse.targetSelection];
        const PersistentGameRecord* targetRecord = characterRecord(target);
        const bool oddCompanion =
            definition->useEffect.slowRule == ITEM_SLOW_UNLESS_RANDOM16_HEALS &&
            targetRecord != NULL && targetRecord->characterId != 0;
        const uint32_t randomValue =
            definition->useEffect.slowRule == ITEM_SLOW_NEVER || oddCompanion ?
                0 : nextRandom();
        inventoryUse.slowWasAlreadyActive = temporarySlowActive;
        inventoryUse.effectCode = applyInventoryItemEffect(
            definition->useEffect, target, randomValue);
        inventoryUse.newlySlowed =
            !inventoryUse.slowWasAlreadyActive && temporarySlowActive;
        setInventoryResultPrompt(definition->variantSelector,
                                 inventoryUse.newlySlowed,
                                 inventoryUse.slowWasAlreadyActive);
        inventoryUse.consumed = removeInventoryQuantity(
            inventoryUse.itemId, inventoryUse.variant, 1);
        inventoryUse.phase = INVENTORY_USE_RESULT;
        return inventoryUse.consumed;
    }
    if(inventoryUse.phase == INVENTORY_USE_RESULT &&
       inventoryUse.resultGateFrames == 0)
    {
        resetInventoryUseState();
        return true;
    }
    return false;
}

bool Game::cancelInventoryAction(void)
{
    switch(inventoryUse.phase)
    {
    case INVENTORY_USE_ACTION: resetInventoryUseState(); return true;
    case INVENTORY_USE_TARGET:
        inventoryUse.phase = INVENTORY_USE_ACTION; return true;
    case INVENTORY_USE_CONFIRM:
        inventoryUse.phase = INVENTORY_USE_TARGET; return true;
    case INVENTORY_USE_RESULT:
        if(inventoryUse.resultGateFrames == 0)
        {
            resetInventoryUseState();
            return true;
        }
        return false;
    default: return false;
    }
}

void Game::updateInventoryFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    if(task->state == 1)
    {
        task->state = 2;
        game.resetInventoryUseState();
    }
    if(game.inventoryUse.phase == INVENTORY_USE_RESULT &&
       game.inventoryUse.resultGateFrames != 0)
        --game.inventoryUse.resultGateFrames;
    const bool up = (game.pressedInput & GAME_INPUT_UP) != 0;
    const bool down = (game.pressedInput & GAME_INPUT_DOWN) != 0;
    const bool cancel =
        (game.pressedInput &
         (GAME_INPUT_CANCEL | GAME_INPUT_PAUSE | GAME_INPUT_START)) != 0;
    updateInventoryEquipmentPreview(game);
    const InventoryEquipmentState& equipment =
        inventoryEquipmentState(game);
    if(equipment.phase != INVENTORY_EQUIPMENT_INACTIVE)
    {
        if(cancel)
            cancelInventoryEquipmentPreview(game);
        else if(equipment.phase == INVENTORY_EQUIPMENT_PREVIEW_READY)
        {
            if((game.pressedInput & GAME_INPUT_LEFT) != 0)
                moveInventoryEquipmentVariant(game, -1);
            else if((game.pressedInput & GAME_INPUT_RIGHT) != 0)
                moveInventoryEquipmentVariant(game, 1);
            else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
                confirmInventoryEquipmentPreview(game);
        }
        return;
    }

    if(game.inventoryUse.phase == INVENTORY_USE_LIST)
    {
        const int count = inventoryMenuItemCount(game);
        if(count != 0 && (up || down))
            game.selectedInventoryItem = wrapIndex(
                game.selectedInventoryItem + (up ? -1 : 1), count);
        else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
        {
            InventoryMenuSelection selection = {};
            if(getInventoryMenuSelection(
                   game, game.selectedInventoryItem, selection))
            {
                if(selection.consumable != 0)
                    game.beginInventoryAction(selection.packedEntryIndex);
                else
                    beginInventoryEquipmentPreview(
                        game, game.selectedInventoryItem);
            }
        }
        else if(cancel)
        {
            game.currentScreen = GAME_SCREEN_PAUSE;
            game.switchGameFlowTaskHandler(updatePauseFlow, 3);
        }
        return;
    }
    if(up || down)
        game.moveInventoryActionCursor(up ? -1 : 1);
    else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
        game.confirmInventoryAction();
    else if(cancel)
        game.cancelInventoryAction();
    if(game.inventoryUse.phase == INVENTORY_USE_LIST)
    {
        const int count = inventoryMenuItemCount(game);
        if(count == 0)
            game.selectedInventoryItem = 0;
        else if(game.selectedInventoryItem >= count)
            game.selectedInventoryItem = count - 1;
    }
}

bool Game::purchaseInventoryItem(uint32_t itemId)
{
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    if(definition == NULL || definition->price < 0 ||
       persistent.story.money < definition->price ||
       itemId >= STORY_ITEM_COUNT ||
       definition->unlockTier > static_cast<int32_t>(persistent.stageId))
        return false;
    if(!addOwnedInventoryQuantity(
           primarySaveImage.payload.itemInventoryBlock,
           static_cast<uint16_t>(itemId), 0, 1))
        return false;
    persistent.story.money -= definition->price;
    projectRuntimeStateToPersistent(primarySaveImage);
    return true;
}

bool Game::sellInventoryItem(uint32_t itemId)
{
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    if(definition == NULL || definition->price < 0 ||
       itemId >= STORY_ITEM_COUNT ||
       !removeOwnedInventoryQuantity(
           primarySaveImage.payload.itemInventoryBlock,
           static_cast<uint16_t>(itemId), 0, 1))
        return false;
    persistent.story.money += definition->price / 2;
    projectRuntimeStateToPersistent(primarySaveImage);
    return true;
}

void Game::rebuildShopItems(void)
{
    shopItemIds.clear();
    std::vector<const InventoryItemDefinition*> definitions;
    const bool female = persistent.story.customization.genderSelector != 0;
    gameData.shopItems(shopCategoryValue, static_cast<int>(persistent.stageId),
                       female, definitions);
    for(size_t i = 0; i < definitions.size(); ++i)
        if(definitions[i] != NULL)
            shopItemIds.push_back(definitions[i]->itemId);
    if(shopSelectionValue < 0 ||
       shopSelectionValue >= static_cast<int>(shopItemIds.size()))
        shopSelectionValue = 0;
}

bool Game::openShop(int category)
{
    shopReturnScreen = currentScreen;
    if(shopReturnScreen == GAME_SCREEN_SHOP)
        shopReturnScreen = GAME_SCREEN_MAIN_MENU;
    shopCategoryValue = category;
    shopSelectionValue = 0;
    currentShopMode = GAME_SHOP_BUY;
    currentShopResult = GAME_SHOP_RESULT_NONE;
    rebuildShopItems();
    currentScreen = GAME_SCREEN_SHOP;
    switchGameFlowTaskHandler(updateShopFlow, 1);
    return true;
}

void Game::closeShop(void)
{
    currentShopResult = GAME_SHOP_RESULT_CANCELLED;
    currentScreen = shopReturnScreen;
    if(gameFlowTask != NULL)
    {
        if(currentScreen == GAME_SCREEN_GAMEPLAY)
            switchGameFlowTaskHandler(updateGameplaySessionFlow, 1);
        else
            switchGameFlowTaskHandler(updateTitleScreenFlow, 0x0c);
    }
}

bool Game::shopOpen(void) const
{
    return currentScreen == GAME_SCREEN_SHOP;
}

GameShopMode Game::shopMode(void) const { return currentShopMode; }

void Game::setShopMode(GameShopMode mode)
{
    currentShopMode = mode == GAME_SHOP_SELL ? GAME_SHOP_SELL : GAME_SHOP_BUY;
    currentShopResult = GAME_SHOP_RESULT_NONE;
}

int Game::shopCategory(void) const { return shopCategoryValue; }

void Game::setShopCategory(int category)
{
    shopCategoryValue = category;
    shopSelectionValue = 0;
    rebuildShopItems();
}

int Game::shopSelection(void) const { return shopSelectionValue; }

void Game::setShopSelection(int index)
{
    if(shopItemIds.empty())
        shopSelectionValue = 0;
    else
        shopSelectionValue = wrapIndex(index, static_cast<int>(shopItemIds.size()));
}

int Game::shopItemCount(void) const
{
    return static_cast<int>(shopItemIds.size());
}

uint32_t Game::shopItemId(int index) const
{
    if(index < 0 || index >= static_cast<int>(shopItemIds.size()))
        return 0;
    return shopItemIds[static_cast<size_t>(index)];
}

int32_t Game::shopItemPrice(int index) const
{
    const uint32_t itemId = shopItemId(index);
    if(itemId == 0 && (index < 0 || index >= shopItemCount()))
        return -1;
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    return definition == NULL ? -1 : definition->price;
}

uint16_t Game::shopItemInventoryCount(int index) const
{
    const uint32_t itemId = shopItemId(index);
    return inventoryCount(static_cast<uint16_t>(itemId));
}

bool Game::shopItemLocked(int index) const
{
    const uint32_t itemId = shopItemId(index);
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    return definition == NULL ||
        definition->unlockTier > static_cast<int32_t>(persistent.stageId);
}

GameShopResult Game::shopResult(void) const { return currentShopResult; }

GameShopResult Game::buyShopSelection(void)
{
    if(shopItemIds.empty())
        return currentShopResult = GAME_SHOP_RESULT_EMPTY;
    const uint32_t itemId = shopItemId(shopSelectionValue);
    const bool female = persistent.story.customization.genderSelector != 0;
    const InventoryItemDefinition* definition = gameData.item(itemId, female);
    if(definition == NULL ||
       definition->unlockTier > static_cast<int32_t>(persistent.stageId))
        return currentShopResult = GAME_SHOP_RESULT_LOCKED;
    if(persistent.story.money < definition->price)
        return currentShopResult = GAME_SHOP_RESULT_INSUFFICIENT_FUNDS;
    return currentShopResult = purchaseInventoryItem(itemId) ?
        GAME_SHOP_RESULT_SUCCESS : GAME_SHOP_RESULT_EMPTY;
}

GameShopResult Game::sellShopSelection(void)
{
    if(shopItemIds.empty())
        return currentShopResult = GAME_SHOP_RESULT_EMPTY;
    const uint32_t itemId = shopItemId(shopSelectionValue);
    if(itemId >= STORY_ITEM_COUNT ||
       inventoryCount(static_cast<uint16_t>(itemId)) == 0)
        return currentShopResult = GAME_SHOP_RESULT_EMPTY;
    return currentShopResult = sellInventoryItem(itemId) ?
        GAME_SHOP_RESULT_SUCCESS : GAME_SHOP_RESULT_EMPTY;
}

bool Game::advicePending(void) const { return advicePendingValue; }

const char* Game::adviceTitle(void) const
{
    return "< Advice : Group Battles >";
}

const char* Game::adviceBody(void) const
{
    return "Press the left analog stick lightly to walk, hard to run.";
}

void Game::dismissAdvice(void)
{
    advicePendingValue = false;
    currentScreen = GAME_SCREEN_GAMEPLAY;
    if(gameFlowTask != NULL)
        switchGameFlowTaskHandler(updateGameplaySessionFlow, 2);
}

uint16_t Game::inventoryCount(uint16_t itemId) const
{
    return ownedInventoryQuantity(
        primarySaveImage.payload.itemInventoryBlock, itemId, 0);
}

int Game::inventorySelection(void) const { return selectedInventoryItem; }

void Game::setInventoryEquipmentPresentationReady(bool ready)
{
    inventoryEquipmentReady = ready;
}

bool Game::inventoryEquipmentPresentationReady(void) const
{
    return inventoryEquipmentReady;
}

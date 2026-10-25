#ifndef MENU_INVENTORY_H
#define MENU_INVENTORY_H
#include "game_data.h"

class Game;

struct InventoryMenuSelection
{
    uint16_t itemId;
    uint16_t packedEntryIndex;
    uint16_t variantMask;
    uint16_t quantity;
    uint8_t variant;
    uint8_t consumable;
};
enum InventoryEquipmentPhase
{
    INVENTORY_EQUIPMENT_INACTIVE,
    INVENTORY_EQUIPMENT_PREVIEW_REBUILD,
    INVENTORY_EQUIPMENT_PREVIEW_READY,
    INVENTORY_EQUIPMENT_ROLLBACK_REBUILD
};

struct InventoryEquipmentState
{
    InventoryEquipmentPhase phase;
    uint16_t itemId;
    uint8_t variant;
    uint32_t changedGroupMask;
    uint64_t expectedFingerprint;
};

int inventoryMenuItemCount(const Game& game);
bool getInventoryMenuSelection(
    const Game& game, int index, InventoryMenuSelection& selection);
bool advanceInventoryMenuVariant(
    const Game& game, int index, int direction,
    InventoryMenuSelection& selection);
const InventoryEquipmentState& inventoryEquipmentState(const Game& game);
bool beginInventoryEquipmentPreview(Game& game, int index);
bool moveInventoryEquipmentVariant(Game& game, int direction);
bool confirmInventoryEquipmentPreview(Game& game);
bool cancelInventoryEquipmentPreview(Game& game);
void updateInventoryEquipmentPreview(Game& game);

void drawInventoryScreen(void);
void drawShopScreen(void);
void drawAdviceScreen(void);

#endif

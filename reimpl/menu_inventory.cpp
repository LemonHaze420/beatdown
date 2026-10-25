#include "menu_inventory.h"
#include "character.h"
#include "game.h"

#ifndef MENU_INVENTORY_CORE_ONLY
#include "app_ui.h"
#include "menu_font.h"

#include <rw.h>
#include <skeleton.h>
#endif

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <vector>

#ifndef MENU_INVENTORY_CORE_ONLY
#include "../formats/MENU_LAYOUT/menu_layout.h"
#endif

namespace
{

#ifndef MENU_INVENTORY_CORE_ONLY
const float VIRTUAL_SCREEN_WIDTH = 640.0f;
const float SHOP_BOARD_RIGHT = 355.0f;
const float SHOP_MODE_X = 46.0f;
const float SHOP_MODE_Y = 67.0f;
const float SHOP_CASH_Y = 72.0f;
const float LIST_NAME_X = 53.0f;
const float LIST_FIRST_NAME_Y = 162.0f;
const float LIST_ROW_PITCH = 58.0f;
const float LIST_PRICE_X = 102.0f;
const float LIST_PRICE_Y_OFFSET = 34.0f;
const float LIST_QUANTITY_RIGHT_X = 298.0f;
const float LIST_QUANTITY_Y_OFFSET = 29.0f;
const float LIST_NAME_MAX_WIDTH = 294.0f;
const float LIST_TEXT_SCALE = 0.52f;
const float SHOP_MODE_SCALE = 0.845f;
const float SHOP_CASH_SCALE = 0.78f;

const float RETAINED_TEXT_SCALE = 24.0f / MENU_CELL_HEIGHT;
const int LIST_VISIBLE_ROWS = 4;
const float HELP_X = 50.0f;
const float HELP_Y = 410.0f;
const float HELP_MAX_WIDTH = 540.0f;
const float HELP_MAX_HEIGHT = 38.0f;
const float PROMPT_Y = 454.0f;
const float IMGUI_SPACE_1 = 4.0f;
const float IMGUI_SPACE_2 = 8.0f;
const float IMGUI_SPACE_4 = 16.0f;

const uint16_t TEXT_BUY = 10611;
const uint16_t TEXT_SELL = 10613;
const uint16_t TEXT_SHOP_BUY_ITEM_HELP = 53009;
const uint16_t TEXT_SHOP_SELL_ITEM_HELP = 53021;
const uint16_t TEXT_ADVICE_GROUP_BATTLES = 30241;
const uint16_t TEXT_ADVICE_WALK_RUN = 30270;

const MenuItemColour HELP_COLOUR = { 0xff, 0xff, 0x80, 0xff };
const MenuItemColour HELP_ERROR_COLOUR = { 0xff, 0x40, 0x40, 0xff };
const MenuItemColour SHOP_HEADING_COLOUR = { 0xff, 0xff, 0xff, 0xff };
const MenuItemColour PROMPT_COLOUR = { 0xc0, 0xc0, 0x80, 0xff };
const MenuItemColour ADVICE_TITLE_COLOUR = { 0xff, 0xff, 0x5f, 0xc0 };
const MenuItemColour ADVICE_BODY_COLOUR = { 0xe0, 0xe0, 0xe0, 0xc0 };

int g_inventoryFirstVisible;
int g_shopFirstVisible;
#endif
uint8_t g_inventorySelectedVariants[1 << 12];
struct InventoryEquipmentTransaction
{
    InventoryEquipmentState publicState;
    CharacterCustomization snapshot;
    CharacterCustomization working;
    uint16_t variantMask;
    uint8_t originalVariant;
    int menuIndex;
};

InventoryEquipmentTransaction g_equipment;

#ifndef MENU_INVENTORY_CORE_ONLY
const char* presentationText(uint16_t textId, const char* hostLiteral)
{
    const char* text = g_game.localizedText(textId);
    return text != NULL ? text : hostLiteral;
}

float fittedTextScale(const char* text, float preferredScale,
                      float maximumWidth)
{
    if(text == NULL || text[0] == 0)
        return preferredScale;
    const float width = measureMenuText(text, preferredScale);
    if(width > maximumWidth && width > 0.0f)
        return preferredScale * maximumWidth / width;
    return preferredScale;
}

int textLineCount(const char* text)
{
    int lines = 1;
    if(text == NULL)
        return lines;
    for(const char* cursor = text; *cursor != 0; ++cursor)
        if(*cursor == '\n')
            ++lines;
    return lines;
}

float fittedBlockScale(const char* text, float preferredScale,
                       float maximumWidth, float maximumHeight)
{
    float scale = fittedTextScale(text, preferredScale, maximumWidth);
    const float height = MENU_CELL_HEIGHT * scale * textLineCount(text);
    if(height > maximumHeight && height > 0.0f)
        scale *= maximumHeight / height;
    return scale;
}

void drawText(const char* text, float x, float y, float scale,
              const MenuItemColour& colour, bool shadow)
{
    drawMenuText(text, x, y, scale, colour.r, colour.g, colour.b,
                 colour.a, shadow);
}

void drawCenteredText(const char* text, float y, float scale,
                      const MenuItemColour& colour, bool shadow)
{
    const float fitted = fittedTextScale(text, scale, 610.0f);
    const float x = (VIRTUAL_SCREEN_WIDTH -
                     measureMenuText(text, fitted)) * 0.5f;
    drawText(text, x, y, fitted, colour, shadow);
}

void drawRightAlignedText(const char* text, float right, float y, float scale,
                          const MenuItemColour& colour, bool shadow)
{
    drawText(text, right - measureMenuText(text, scale), y, scale,
             colour, shadow);
}

const MenuItemColour& listColour(bool selected, bool enabled)
{
    if(!enabled)
        return MENU_ITEM_COLOURS[MENU_COLOUR_DISABLED];
    if(selected)
        return MENU_ITEM_COLOURS[MENU_COLOUR_SELECTED];
    return MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED];
}

void drawListRow(const char* name, uint16_t quantity, int32_t price,
                 bool showPrice, int visibleRow,
                 bool selected, bool enabled)
{
    char renderedName[512];
    if(!enabled && selected)
        snprintf(renderedName, sizeof(renderedName), "[X] > %s <", name);
    else if(!enabled)
        snprintf(renderedName, sizeof(renderedName), "[X] %s", name);
    else if(selected)
        snprintf(renderedName, sizeof(renderedName), "> %s <", name);
    else
        snprintf(renderedName, sizeof(renderedName), "%s", name);

    const MenuItemColour& colour = listColour(selected, enabled);
    const float nameY = LIST_FIRST_NAME_Y + visibleRow * LIST_ROW_PITCH;
    const float nameScale = fittedTextScale(renderedName, LIST_TEXT_SCALE,
                                            LIST_NAME_MAX_WIDTH);
    drawText(renderedName, LIST_NAME_X, nameY, nameScale, colour, true);

    char quantityText[32];
    snprintf(quantityText, sizeof(quantityText), "x%u",
             static_cast<unsigned>(quantity));
    drawRightAlignedText(quantityText, LIST_QUANTITY_RIGHT_X,
                         nameY + LIST_QUANTITY_Y_OFFSET, LIST_TEXT_SCALE,
                         colour, true);

    if(showPrice)
    {
        char priceText[32];
        if(price < 0)
            snprintf(priceText, sizeof(priceText), "$ --");
        else
            snprintf(priceText, sizeof(priceText), "$ %d", price);
        drawText(priceText, LIST_PRICE_X,
                 nameY + LIST_PRICE_Y_OFFSET, LIST_TEXT_SCALE, colour, true);
    }
}

void drawHelp(const char* text, bool error)
{
    if(text == NULL || text[0] == 0)
        return;
    const float scale = fittedBlockScale(text, RETAINED_TEXT_SCALE,
                                         HELP_MAX_WIDTH, HELP_MAX_HEIGHT);
    drawText(text, HELP_X, HELP_Y, scale,
             error ? HELP_ERROR_COLOUR : HELP_COLOUR, true);
}

void drawPrompt(const char* prompt)
{
    drawCenteredText(prompt, PROMPT_Y, LIST_TEXT_SCALE, PROMPT_COLOUR, true);
}
#endif

void buildPackedOwnedInventory(
    const Game& game, std::vector<uint32_t>& packed)
{
    const OwnedInventoryBlock& owned = game.ownedInventory();
    const size_t count = owned.count > GAME_SAVE_INVENTORY_CAPACITY ?
        GAME_SAVE_INVENTORY_CAPACITY : owned.count;
    packed.assign(owned.entries, owned.entries + count);
}

void buildOwnedInventory(
    const Game& game, std::vector<InventoryOwnedItem>& items)
{
    std::vector<uint32_t> packed;
    buildPackedOwnedInventory(game, packed);
    const bool female =
        game.runtimeState().story.customization.genderSelector != 0;
    game.data().collectOwnedInventory(
        packed.empty() ? NULL : packed.data(), packed.size(), female, items);
    for(size_t index = 0; index < items.size(); ++index)
    {
        const uint16_t itemId =
            static_cast<uint16_t>(items[index].definition->itemId);
        uint8_t variant = g_inventorySelectedVariants[itemId];
        if((items[index].variantMask & (1u << variant)) == 0)
            variant = nextOwnedInventoryVariant(
                items[index].variantMask, variant, 1);
        g_inventorySelectedVariants[itemId] = variant;
        items[index].selectedVariant = variant;
    }
}

bool inventorySelectionAt(
    const Game& game, int index, InventoryMenuSelection& selection,
    const InventoryItemDefinition** definition)
{
    std::vector<InventoryOwnedItem> items;
    buildOwnedInventory(game, items);
    if(index < 0 || index >= static_cast<int>(items.size()))
        return false;
    const InventoryOwnedItem& item = items[index];
    selection.itemId = static_cast<uint16_t>(item.definition->itemId);
    selection.packedEntryIndex = UINT16_MAX;
    const OwnedInventoryBlock& owned = game.ownedInventory();
    const size_t packedCount =
        owned.count > GAME_SAVE_INVENTORY_CAPACITY ?
        GAME_SAVE_INVENTORY_CAPACITY : owned.count;
    for(size_t packedIndex = 0; packedIndex < packedCount; ++packedIndex)
    {
        const uint32_t packed = owned.entries[packedIndex];
        if(packedInventoryItemId(packed) == selection.itemId &&
           packedInventoryVariant(packed) == item.selectedVariant &&
           packedInventoryQuantity(packed) != 0)
        {
            selection.packedEntryIndex =
                static_cast<uint16_t>(packedIndex);
            selection.quantity = packedInventoryQuantity(packed);
            break;
        }
    }
    if(selection.packedEntryIndex == UINT16_MAX)
        return false;
    selection.variantMask = item.variantMask;
    selection.variant = item.selectedVariant;
    selection.consumable = item.definition->consumable ? 1 : 0;
    if(definition != NULL)
        *definition = item.definition;
    return true;
}

#ifndef MENU_INVENTORY_CORE_ONLY
void updateInventoryWindow(int selection, int itemCount)
{
    if(itemCount <= LIST_VISIBLE_ROWS)
    {
        g_inventoryFirstVisible = 0;
        return;
    }
    if(selection < g_inventoryFirstVisible)
        g_inventoryFirstVisible = selection;
    else if(selection >= g_inventoryFirstVisible + LIST_VISIBLE_ROWS)
        g_inventoryFirstVisible = selection - LIST_VISIBLE_ROWS + 1;

    const int maximumFirst = itemCount - LIST_VISIBLE_ROWS;
    if(g_inventoryFirstVisible < 0)
        g_inventoryFirstVisible = 0;
    else if(g_inventoryFirstVisible > maximumFirst)
        g_inventoryFirstVisible = maximumFirst;
}

void drawInventoryOverlayLine(const char* text, int row, bool selected,
                              bool enabled, bool retained)
{
    if(!retained)
    {
        uiDrawMenuItem(text, selected, enabled);
        ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_2));
        return;
    }
    const MenuItemColour disabled = { 0x1c, 0x3b, 0x32, 0xff };
    const MenuItemColour normal = { 0x38, 0x77, 0x64, 0xff };
    const MenuItemColour active = { 0x71, 0xee, 0xc8, 0xff };
    drawCenteredText(text, 150.0f + row * 50.0f, 0.62f,
                     !enabled ? disabled : (selected ? active : normal),
                     true);
}

void drawInventoryUseOverlay(bool retained)
{
    const InventoryUseState& use = g_game.inventoryUseState();
    if(use.phase == INVENTORY_USE_ACTION)
    {
        const uint16_t textIds[3] = { 0x7866, 0x7865, 0xcbd0 };
        for(int row = 0; row < 3; ++row)
            drawInventoryOverlayLine(
                presentationText(textIds[row], ""), row,
                use.actionSelection == row,
                g_game.inventoryActionRowEnabled(row), retained);
        return;
    }
    if(use.phase == INVENTORY_USE_TARGET)
    {
        for(int row = 0; row < 3; ++row)
        {
            const bool present = use.targetCharacterIds[row] != UINT16_MAX;
            const uint16_t textId = present ?
                static_cast<uint16_t>(use.targetCharacterIds[row] + 0x2af9) :
                0xcb8d;
            drawInventoryOverlayLine(
                presentationText(textId, ""), row,
                use.targetSelection == row, present, retained);
        }
        return;
    }
    if(use.phase == INVENTORY_USE_CONFIRM)
    {
        if(retained)
            drawCenteredText(presentationText(0xcbd5, ""), 125.0f, 0.6f,
                             MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED], true);
        else
            uiCenterText(presentationText(0xcbd5, ""));
        const uint16_t textIds[2] = { 0x76c2, 0x76c3 };
        for(int row = 0; row < 2; ++row)
            drawInventoryOverlayLine(
                presentationText(textIds[row], ""), row,
                use.confirmSelection == row, true, retained);
        return;
    }
    if(use.phase == INVENTORY_USE_RESULT)
    {
        char result[1024] = {};
        for(uint8_t index = 0; index < use.resultTextCount; ++index)
        {
            const char* text =
                presentationText(use.resultTextIds[index], "");
            if(index != 0)
                strncat(result, "\n", sizeof(result) - strlen(result) - 1);
            strncat(result, text, sizeof(result) - strlen(result) - 1);
        }
        if(retained)
            drawCenteredText(result, 195.0f, 0.58f,
                             MENU_ITEM_COLOURS[MENU_COLOUR_SELECTED], true);
        else
            uiCenterText(result);
    }
}

void drawInventoryFallback(void)
{
    char line[256];
    uiCenterText(presentationText(GAME_TEXT_ITEMS, "ITEMS"));
    if(g_game.inventoryUseState().phase != INVENTORY_USE_LIST)
    {
        drawInventoryUseOverlay(false);
        return;
    }
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_4 * 2.0f));
    std::vector<InventoryOwnedItem> items;
    buildOwnedInventory(g_game, items);
    for(size_t i = 0; i < items.size(); ++i)
    {
        const InventoryItemDefinition& definition = *items[i].definition;
        const char* name = presentationText(
            static_cast<uint16_t>(definition.nameTextId), "");
        snprintf(line, sizeof(line), "%s  x%u", name,
                 static_cast<unsigned>(items[i].quantity));
        uiDrawMenuItem(line,
                       static_cast<int>(i) == g_game.inventorySelection(),
                       items[i].quantity != 0);
        ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_1));
    }
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_4));
    InventoryMenuSelection selection = {};
    const InventoryItemDefinition* selectedDefinition = NULL;
    if(inventorySelectionAt(
           g_game, g_game.inventorySelection(), selection,
           &selectedDefinition) &&
       selectedDefinition->helpTextId <= UINT16_MAX)
        uiCenterText(presentationText(
            static_cast<uint16_t>(selectedDefinition->helpTextId), ""));
    const char* action = presentationText(
        selection.consumable != 0 ? GAME_TEXT_USE : GAME_TEXT_SELECT, "");
    const char* back = presentationText(GAME_TEXT_BACK, "");
    snprintf(line, sizeof(line),
             "ENTER / A: %s    ESC / B: %s", action, back);
    ImGui::PushStyleColor(ImGuiCol_Text,
                         ImVec4(0.74f, 0.76f, 0.80f, 1.0f));
    uiCenterText(line);
    ImGui::PopStyleColor();
}

void drawAdviceFallback(void)
{
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_4 * 3.0f));
    uiCenterText(g_game.adviceTitle());
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_4 * 2.0f));
    uiCenterText(g_game.adviceBody());
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_4 * 3.0f));
    uiDrawCommandPrompt(false);
}

void drawShopFallback(void)
{
    const GameRuntimeState& state = g_game.runtimeState();
    char line[128];
    uiCenterText(presentationText(
        g_game.shopMode() == GAME_SHOP_BUY ? TEXT_BUY : TEXT_SELL, ""));
    snprintf(line, sizeof(line), "$%u", state.story.money);
    uiCenterText(line);
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_4 * 2.0f));
    for(int i = 0; i < g_game.shopItemCount(); ++i)
    {
        const uint32_t itemId = g_game.shopItemId(i);
        const bool female =
            state.story.customization.genderSelector != 0;
        const InventoryItemDefinition* definition =
            g_game.data().item(itemId, female);
        const char* name = definition == NULL ? "" :
            presentationText(
                static_cast<uint16_t>(definition->nameTextId), "");
        snprintf(line, sizeof(line), "%s  $%d  x%u", name,
                 g_game.shopItemPrice(i),
                 static_cast<unsigned>(g_game.shopItemInventoryCount(i)));
        uiDrawMenuItem(line, i == g_game.shopSelection(),
                       !g_game.shopItemLocked(i));
    }
    ImGui::Dummy(ImVec2(0.0f, IMGUI_SPACE_2));
    uiDrawCommandPrompt(true);
}

const InventoryItemDefinition* shopDefinition(uint32_t itemId)
{
    const bool female =
        g_game.runtimeState().story.customization.genderSelector != 0;
    return g_game.data().item(itemId, female);
}

const char* definitionText(uint32_t textId, const char* hostLiteral)
{
    if(textId > UINT16_MAX)
        return hostLiteral;
    return presentationText(static_cast<uint16_t>(textId), hostLiteral);
}

void updateShopWindow(int selection, int itemCount)
{
    if(itemCount <= LIST_VISIBLE_ROWS)
    {
        g_shopFirstVisible = 0;
        return;
    }
    if(selection < g_shopFirstVisible)
        g_shopFirstVisible = selection;
    else if(selection >= g_shopFirstVisible + LIST_VISIBLE_ROWS)
        g_shopFirstVisible = selection - LIST_VISIBLE_ROWS + 1;

    const int maximumFirst = itemCount - LIST_VISIBLE_ROWS;
    if(g_shopFirstVisible < 0)
        g_shopFirstVisible = 0;
    else if(g_shopFirstVisible > maximumFirst)
        g_shopFirstVisible = maximumFirst;
}
#endif

}
int inventoryMenuItemCount(const Game& game)
{
    std::vector<InventoryOwnedItem> items;
    buildOwnedInventory(game, items);
    return static_cast<int>(items.size());
}

bool getInventoryMenuSelection(
    const Game& game, int index, InventoryMenuSelection& selection)
{
    return inventorySelectionAt(game, index, selection, NULL);
}

bool advanceInventoryMenuVariant(
    const Game& game, int index, int direction,
    InventoryMenuSelection& selection)
{
    if(!inventorySelectionAt(game, index, selection, NULL))
        return false;
    selection.variant = nextOwnedInventoryVariant(
        selection.variantMask, selection.variant, direction);
    g_inventorySelectedVariants[selection.itemId] = selection.variant;
    return true;
}

const InventoryEquipmentState&
inventoryEquipmentState(const Game& game)
{
    (void)game;
    return g_equipment.publicState;
}

static const InventoryItemDefinition*
equipmentVariantDefinition(
    Game& game, const InventoryItemDefinition* base, uint8_t variant)
{
    if(base == NULL)
        return NULL;
    const bool female =
        game.runtimeState().story.customization.genderSelector != 0;
    const InventoryItemDefinition* child =
        game.data().itemVariant(
            base->itemId, base->variantSelector, variant, female);
    return child != NULL ? child : base;
}

bool beginInventoryEquipmentPreview(Game& game, int index)
{
    if(g_equipment.publicState.phase != INVENTORY_EQUIPMENT_INACTIVE)
        return false;
    InventoryMenuSelection selection = {};
    const InventoryItemDefinition* base = NULL;
    if(!inventorySelectionAt(game, index, selection, &base) ||
       selection.consumable != 0)
        return false;
    const InventoryItemDefinition* definition =
        equipmentVariantDefinition(game, base, selection.variant);
    if(definition == NULL)
        return false;

    InventoryEquipmentTransaction next = {};
    next.snapshot = game.runtimeState().story.customization;
    next.working = next.snapshot;
    next.variantMask = selection.variantMask;
    next.originalVariant = selection.variant;
    next.menuIndex = index;
    next.publicState.phase = INVENTORY_EQUIPMENT_PREVIEW_REBUILD;
    next.publicState.itemId = selection.itemId;
    next.publicState.variant = selection.variant;
    if(!applyInventoryEquipToCustomization(
           *definition, selection.variant, next.working,
           next.publicState.changedGroupMask))
        return false;
    next.publicState.expectedFingerprint =
        characterCustomizationFingerprint(next.working);
    if(next.publicState.expectedFingerprint ==
       characterCustomizationFingerprint(next.snapshot))
        return false;

    g_equipment = next;
    game.runtimeState().story.customization = g_equipment.working;
    game.setInventoryEquipmentPresentationReady(false);
    return true;
}

bool moveInventoryEquipmentVariant(Game& game, int direction)
{
    if(g_equipment.publicState.phase !=
           INVENTORY_EQUIPMENT_PREVIEW_READY ||
       direction == 0)
        return false;
    const uint8_t nextVariant = nextOwnedInventoryVariant(
        g_equipment.variantMask, g_equipment.publicState.variant,
        direction);
    if(nextVariant == g_equipment.publicState.variant)
        return false;

    const bool female =
        g_equipment.snapshot.genderSelector != 0;
    const InventoryItemDefinition* base = game.data().item(
        g_equipment.publicState.itemId, female);
    const InventoryItemDefinition* definition =
        equipmentVariantDefinition(game, base, nextVariant);
    if(definition == NULL)
        return false;
    CharacterCustomization working = g_equipment.snapshot;
    uint32_t changed = 0;
    if(!applyInventoryEquipToCustomization(
           *definition, nextVariant, working, changed))
        return false;
    const uint64_t fingerprint =
        characterCustomizationFingerprint(working);
    if(fingerprint ==
       characterCustomizationFingerprint(g_equipment.working))
        return false;

    g_equipment.working = working;
    g_equipment.publicState.variant = nextVariant;
    g_equipment.publicState.changedGroupMask = changed;
    g_equipment.publicState.expectedFingerprint = fingerprint;
    g_equipment.publicState.phase =
        INVENTORY_EQUIPMENT_PREVIEW_REBUILD;
    g_inventorySelectedVariants[g_equipment.publicState.itemId] =
        nextVariant;
    game.runtimeState().story.customization = working;
    game.setInventoryEquipmentPresentationReady(false);
    return true;
}

bool confirmInventoryEquipmentPreview(Game& game)
{
    if(g_equipment.publicState.phase !=
       INVENTORY_EQUIPMENT_PREVIEW_READY)
        return false;
    const int16_t characterId =
        game.runtimeState().player.characterId;
    if(characterId < 0 ||
       !game.commitCharacterCustomization(
           static_cast<uint16_t>(characterId), g_equipment.working))
        return false;
    game.runtimeState().story.customization = g_equipment.working;
    g_equipment = InventoryEquipmentTransaction();
    game.setInventoryEquipmentPresentationReady(false);
    return true;
}

bool cancelInventoryEquipmentPreview(Game& game)
{
    if(g_equipment.publicState.phase == INVENTORY_EQUIPMENT_INACTIVE ||
       g_equipment.publicState.phase ==
           INVENTORY_EQUIPMENT_ROLLBACK_REBUILD)
        return false;
    game.runtimeState().story.customization = g_equipment.snapshot;
    g_equipment.publicState.expectedFingerprint =
        characterCustomizationFingerprint(g_equipment.snapshot);
    g_equipment.publicState.phase =
        INVENTORY_EQUIPMENT_ROLLBACK_REBUILD;
    game.setInventoryEquipmentPresentationReady(false);
    return true;
}

void updateInventoryEquipmentPreview(Game& game)
{
    const InventoryEquipmentPhase phase =
        g_equipment.publicState.phase;
    if(phase != INVENTORY_EQUIPMENT_PREVIEW_REBUILD &&
       phase != INVENTORY_EQUIPMENT_ROLLBACK_REBUILD)
        return;
    if(!game.inventoryEquipmentPresentationReady())
        return;
    game.setInventoryEquipmentPresentationReady(false);
    if(phase == INVENTORY_EQUIPMENT_PREVIEW_REBUILD)
    {
        g_equipment.publicState.phase =
            INVENTORY_EQUIPMENT_PREVIEW_READY;
        return;
    }

    g_inventorySelectedVariants[g_equipment.publicState.itemId] =
        g_equipment.originalVariant;
    g_equipment = InventoryEquipmentTransaction();
}

#ifndef MENU_INVENTORY_CORE_ONLY
void drawInventoryScreen(void)
{
    const InventoryEquipmentState& equipment =
        inventoryEquipmentState(g_game);
    if(equipment.phase == INVENTORY_EQUIPMENT_PREVIEW_REBUILD ||
       equipment.phase == INVENTORY_EQUIPMENT_ROLLBACK_REBUILD)
    {
        g_game.setInventoryEquipmentPresentationReady(
            uiCharacterCustomizationReady(
                equipment.expectedFingerprint));
    }
    if(!loadMenuFont())
    {
        drawInventoryFallback();
        return;
    }

    drawCenteredText(presentationText(GAME_TEXT_ITEMS, "ITEMS"),
                     72.0f, 0.65f,
                     MENU_ITEM_COLOURS[MENU_COLOUR_SELECTED], true);
    if(g_game.inventoryUseState().phase != INVENTORY_USE_LIST)
    {
        drawInventoryUseOverlay(true);
        return;
    }

    std::vector<InventoryOwnedItem> items;
    buildOwnedInventory(g_game, items);
    const int selection = g_game.inventorySelection();
    updateInventoryWindow(selection, static_cast<int>(items.size()));
    for(int row = 0; row < LIST_VISIBLE_ROWS; ++row)
    {
        const int index = g_inventoryFirstVisible + row;
        if(index >= static_cast<int>(items.size()))
            break;
        const InventoryOwnedItem& item = items[index];
        const char* name = definitionText(
            item.definition->nameTextId, "");
        drawListRow(name, item.quantity, 0, false, row,
                    index == selection, item.quantity != 0);

        for(int variant = 0; variant < 16; ++variant)
        {
            if((item.variantMask & (1u << variant)) == 0)
                continue;
            const bool focused =
                item.selectedVariant == static_cast<uint8_t>(variant);
            drawText(focused ? "|" : ".",
                     196.0f + 9.0f * variant,
                     186.0f + LIST_ROW_PITCH * row, 0.35f,
                     focused ?
                         MENU_ITEM_COLOURS[MENU_COLOUR_SELECTED] :
                         MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED],
                     false);
        }
    }
    if(g_inventoryFirstVisible > 0)
        drawText("^", 338.0f, LIST_FIRST_NAME_Y, 0.4f,
                 MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED], true);
    if(g_inventoryFirstVisible + LIST_VISIBLE_ROWS <
       static_cast<int>(items.size()))
        drawText("v", 338.0f,
                 LIST_FIRST_NAME_Y +
                     (LIST_VISIBLE_ROWS - 1) * LIST_ROW_PITCH,
                 0.4f, MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED], true);

    InventoryMenuSelection selected = {};
    const InventoryItemDefinition* definition = NULL;
    if(inventorySelectionAt(g_game, selection, selected, &definition))
        drawHelp(definitionText(definition->helpTextId, ""),
                 selected.quantity == 0);
    if(equipment.phase == INVENTORY_EQUIPMENT_PREVIEW_REBUILD ||
       equipment.phase == INVENTORY_EQUIPMENT_ROLLBACK_REBUILD)
        drawHelp(presentationText(2368, "LOADING"), false);

    const char* back = presentationText(0x28a3, "");
    char prompt[512];
    if(equipment.phase == INVENTORY_EQUIPMENT_PREVIEW_READY)
    {
        snprintf(prompt, sizeof(prompt), "< / >: %s    ENTER / A: %s    %s",
                 presentationText(0x28a6, ""),
                 presentationText(0x28a2, ""), back);
    }
    else if(equipment.phase != INVENTORY_EQUIPMENT_INACTIVE)
        snprintf(prompt, sizeof(prompt), "%s", back);
    else
    {
        const char* action = presentationText(
            selected.consumable != 0 ? GAME_TEXT_USE : GAME_TEXT_SELECT, "");
        snprintf(prompt, sizeof(prompt),
                 "ENTER / A: %s    ESC / B: %s", action, back);
    }
    drawPrompt(prompt);
}

void drawShopScreen(void)
{
    if(!loadMenuFont())
    {
        drawShopFallback();
        return;
    }

    const GameRuntimeState& state = g_game.runtimeState();
    const GameShopMode mode = g_game.shopMode();
    const char* modeText = mode == GAME_SHOP_BUY ?
        presentationText(TEXT_BUY, "BUY") :
        presentationText(TEXT_SELL, "SELL");

    drawText(modeText, SHOP_MODE_X, SHOP_MODE_Y, SHOP_MODE_SCALE,
             SHOP_HEADING_COLOUR, true);
    char money[64];
    snprintf(money, sizeof(money), "$%u", state.story.money);
    drawRightAlignedText(money, SHOP_BOARD_RIGHT - 5.0f, SHOP_CASH_Y,
                         SHOP_CASH_SCALE, SHOP_HEADING_COLOUR, true);

    const int itemCount = g_game.shopItemCount();
    const int selection = g_game.shopSelection();
    updateShopWindow(selection, itemCount);

    for(int row = 0; row < LIST_VISIBLE_ROWS; ++row)
    {
        const int index = g_shopFirstVisible + row;
        if(index >= itemCount)
            break;

        const uint32_t itemId = g_game.shopItemId(index);
        const InventoryItemDefinition* definition = shopDefinition(itemId);
        const char* name = definition == NULL ? "" :
            definitionText(definition->nameTextId, "");
        int32_t price = g_game.shopItemPrice(index);

        if(mode == GAME_SHOP_SELL && price >= 0)
            price /= 2;
        drawListRow(name, g_game.shopItemInventoryCount(index), price, true,
                    row, index == selection,
                    !g_game.shopItemLocked(index));
    }

    if(g_shopFirstVisible > 0)
        drawText("^", 338.0f, LIST_FIRST_NAME_Y, 0.4f,
                 MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED], true);
    if(g_shopFirstVisible + LIST_VISIBLE_ROWS < itemCount)
        drawText("v", 338.0f,
                 LIST_FIRST_NAME_Y +
                     (LIST_VISIBLE_ROWS - 1) * LIST_ROW_PITCH,
                 0.4f, MENU_ITEM_COLOURS[MENU_COLOUR_ENABLED], true);

    const char* help = NULL;
    bool helpError = false;
    if(itemCount > 0 && selection >= 0 && selection < itemCount)
    {
        const uint32_t itemId = g_game.shopItemId(selection);
        const InventoryItemDefinition* definition = shopDefinition(itemId);
        if(definition != NULL)
            help = definitionText(definition->helpTextId, "");
        helpError = g_game.shopItemLocked(selection);
    }
    if(help == NULL || help[0] == 0)
    {
        help = mode == GAME_SHOP_BUY ?
            presentationText(TEXT_SHOP_BUY_ITEM_HELP,
                             "SELECT AN ITEM TO PURCHASE") :
            presentationText(TEXT_SHOP_SELL_ITEM_HELP,
                             "SELECT AN ITEM TO SELL");
    }

    drawHelp(help, helpError);

    const char* select = presentationText(GAME_TEXT_SELECT, "SELECT");
    const char* back = presentationText(GAME_TEXT_BACK, "BACK");
    char prompt[512];
    snprintf(prompt, sizeof(prompt),
             "ENTER / A: %s    ESC / B: %s", select, back);
    drawPrompt(prompt);
}

void drawAdviceScreen(void)
{
    if(!loadMenuFont())
    {
        drawAdviceFallback();
        return;
    }

    const char* title = presentationText(
        TEXT_ADVICE_GROUP_BATTLES, g_game.adviceTitle());
    const char* body = presentationText(
        TEXT_ADVICE_WALK_RUN, g_game.adviceBody());

    drawCenteredText(title, 198.0f, RETAINED_TEXT_SCALE,
                     ADVICE_TITLE_COLOUR, false);
    drawCenteredText(body, 254.0f, RETAINED_TEXT_SCALE,
                     ADVICE_BODY_COLOUR, false);

    const char* select = presentationText(GAME_TEXT_SELECT, "SELECT");
    char prompt[256];
    snprintf(prompt, sizeof(prompt), "ENTER / A: %s", select);
    drawPrompt(prompt);
}
#endif

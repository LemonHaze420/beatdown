#include "game/save.h"
#include "story_state.h"

#include <string.h>

namespace
{
uint16_t readLittle16(const uint8_t* bytes)
{
    return static_cast<uint16_t>(bytes[0]) |
        static_cast<uint16_t>(bytes[1] << 8);
}

uint16_t foldChecksum(uint32_t sum)
{
    sum = (sum & 0xffffu) + (sum >> 16);
    sum = (sum & 0xffffu) + (sum >> 16);
    return static_cast<uint16_t>(sum);
}

uint16_t foldedBlockSum(const void* data, size_t dataSize,
                        const MemoryCardBlockTrailer& trailer)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t sum = 0;
    for(size_t offset = 0; offset < dataSize; offset += 2)
        sum += readLittle16(bytes + offset);
    sum += trailer.validMarker;
    sum += trailer.checksum;
    return foldChecksum(sum);
}
}

uint16_t
memoryCardBlockChecksum(const void* data, size_t dataSize,
                        uint16_t validMarker)
{
    if(data == NULL || (dataSize & 1u) != 0)
        return 0;
    MemoryCardBlockTrailer trailer = { validMarker, 0 };
    return static_cast<uint16_t>(~foldedBlockSum(data, dataSize, trailer));
}

void
finalizeMemoryCardBlockChecksum(void* data, size_t dataSize,
                                MemoryCardBlockTrailer& trailer,
                                uint16_t validMarker)
{
    trailer.validMarker = validMarker;
    trailer.checksum = 0;
    trailer.checksum = memoryCardBlockChecksum(data, dataSize, validMarker);
}

MemoryCardBlockStatus
validateMemoryCardBlockChecksum(void* data, size_t dataSize,
                                MemoryCardBlockTrailer& trailer)
{
    if(data == NULL || (dataSize & 1u) != 0 ||
       foldedBlockSum(data, dataSize, trailer) != 0xffffu)
    {
        trailer.validMarker = 2;
        return MEMORY_CARD_BLOCK_CHECKSUM_ERROR;
    }
    return trailer.validMarker == 0 ? MEMORY_CARD_BLOCK_INACTIVE :
        MEMORY_CARD_BLOCK_VALID;
}

bool
validateGameSaveImage(GameSaveImage& image, GameSaveBlockStatuses* statuses)
{
    GameSaveBlockStatuses local = {};
    local.settings = static_cast<uint8_t>(validateMemoryCardBlockChecksum(
        &image.settings, sizeof(image.settings), image.settingsTrailer));
    bool haveSummary = false;
    for(size_t i = 0; i < GAME_SAVE_SUMMARY_COUNT; ++i)
    {
        local.summaries[i] = static_cast<uint8_t>(
            validateMemoryCardBlockChecksum(
                &image.summaries[i].summary,
                sizeof(image.summaries[i].summary),
                image.summaries[i].trailer));
        haveSummary = haveSummary ||
            local.summaries[i] == MEMORY_CARD_BLOCK_VALID;
    }
    local.payload = static_cast<uint8_t>(validateMemoryCardBlockChecksum(
        &image.payload, sizeof(image.payload), image.payloadTrailer));
    if(statuses != NULL)
        *statuses = local;
    return local.settings == MEMORY_CARD_BLOCK_VALID && haveSummary &&
        local.payload == MEMORY_CARD_BLOCK_VALID;
}

void
finalizeGameSaveImage(GameSaveImage& image, size_t summaryIndex)
{
    if(summaryIndex >= GAME_SAVE_SUMMARY_COUNT)
        summaryIndex = 0;
    finalizeMemoryCardBlockChecksum(
        &image.settings, sizeof(image.settings), image.settingsTrailer, 1);
    for(size_t i = 0; i < GAME_SAVE_SUMMARY_COUNT; ++i)
    {
        finalizeMemoryCardBlockChecksum(
            &image.summaries[i].summary,
            sizeof(image.summaries[i].summary),
            image.summaries[i].trailer,
            i == summaryIndex ? 1 : 0);
    }
    finalizeMemoryCardBlockChecksum(
        &image.payload, sizeof(image.payload), image.payloadTrailer, 1);
}

void
initializeGameSaveImage(GameSaveImage& image, size_t summaryIndex)
{
    memset(&image, 0, sizeof(image));
    image.payload.storyProgressionBlock.phase =
        STORY_PROGRESSION_PHASE_PROLOGUE;
    for(size_t i = 0; i < 4; ++i)
        image.payload.activeRecordIds[i] = GAME_SAVE_INACTIVE_RECORD_ID;
    if(summaryIndex >= GAME_SAVE_SUMMARY_COUNT)
        summaryIndex = 0;
    image.summaries[summaryIndex].summary.formatVersion =
        GAME_SAVE_SUMMARY_FORMAT;
    finalizeGameSaveImage(image, summaryIndex);
}

GameSaveStageResumeState
gameSaveStageResumeState(const GameSaveImage& image)
{
    GameSaveStageResumeState state = {};
    const uint8_t* core = image.payload.coreGameStateBlock;
    memcpy(&state.arrivalSelector,
           core + GAME_SAVE_CORE_STAGE_SELECTOR_OFFSET,
           sizeof(state.arrivalSelector));
    memcpy(&state.stageId, core + GAME_SAVE_CORE_STAGE_ID_OFFSET,
           sizeof(state.stageId));
    memcpy(state.position, core + GAME_SAVE_CORE_PLAYER_POSITION_OFFSET,
           sizeof(state.position));
    memcpy(&state.headingDegrees,
           core + GAME_SAVE_CORE_PLAYER_HEADING_OFFSET,
           sizeof(state.headingDegrees));
    return state;
}

void
setGameSaveStageResumeState(
    GameSaveImage& image, const GameSaveStageResumeState& state)
{
    uint8_t* core = image.payload.coreGameStateBlock;
    memcpy(core + GAME_SAVE_CORE_STAGE_SELECTOR_OFFSET,
           &state.arrivalSelector, sizeof(state.arrivalSelector));
    memcpy(core + GAME_SAVE_CORE_STAGE_ID_OFFSET,
           &state.stageId, sizeof(state.stageId));
    memcpy(core + GAME_SAVE_CORE_PLAYER_POSITION_OFFSET,
           state.position, sizeof(state.position));
    memcpy(core + GAME_SAVE_CORE_PLAYER_HEADING_OFFSET,
           &state.headingDegrees, sizeof(state.headingDegrees));
}

bool
settingsPrimaryProfileCharacterRecruited(
    const GlobalSettingsBlock& settings, uint16_t characterId)
{
    if(characterId >= GAME_SAVE_RECORD_COUNT)
        return false;
    return (settings.primaryProfileRecruitedCharacterBits[characterId / 8] &
            static_cast<uint8_t>(1u << (characterId & 7))) != 0;
}

void
setSettingsPrimaryProfileCharacterRecruited(
    GlobalSettingsBlock& settings, uint16_t characterId, bool recruited)
{
    if(characterId >= GAME_SAVE_RECORD_COUNT)
        return;
    uint8_t& value =
        settings.primaryProfileRecruitedCharacterBits[characterId / 8];
    const uint8_t mask = static_cast<uint8_t>(1u << (characterId & 7));
    if(recruited)
        value |= mask;
    else
        value &= static_cast<uint8_t>(~mask);
}

uint16_t
ownedInventoryQuantity(const OwnedInventoryBlock& inventory,
                       uint16_t itemId, int variant)
{
    const size_t count = inventory.count > GAME_SAVE_INVENTORY_CAPACITY ?
        GAME_SAVE_INVENTORY_CAPACITY : inventory.count;
    uint32_t quantity = 0;
    for(size_t i = 0; i < count; ++i)
    {
        const uint32_t entry = inventory.entries[i];
        if(packedInventoryItemId(entry) != itemId ||
           (variant >= 0 && packedInventoryVariant(entry) != variant))
            continue;
        quantity += packedInventoryQuantity(entry);
    }
    return static_cast<uint16_t>(quantity > 0xffffu ? 0xffffu : quantity);
}

bool
addOwnedInventoryQuantity(OwnedInventoryBlock& inventory,
                          uint16_t itemId, uint8_t variant,
                          uint16_t quantity)
{
    if(itemId > 0x0fffu || variant > 0x0fu)
        return false;
    if(quantity == 0)
        return true;
    if(inventory.count > GAME_SAVE_INVENTORY_CAPACITY)
        return false;
    for(size_t i = 0; i < inventory.count; ++i)
    {
        const uint32_t entry = inventory.entries[i];
        if(packedInventoryItemId(entry) != itemId ||
           packedInventoryVariant(entry) != variant)
            continue;
        const uint32_t updated =
            packedInventoryQuantity(entry) + quantity;
        inventory.entries[i] = packInventoryEntry(
            itemId, variant, static_cast<uint16_t>(updated));
        return true;
    }
    if(inventory.count >= GAME_SAVE_INVENTORY_CAPACITY)
        return false;
    inventory.entries[inventory.count++] =
        packInventoryEntry(itemId, variant, quantity);
    return true;
}

bool
removeOwnedInventoryQuantity(OwnedInventoryBlock& inventory,
                             uint16_t itemId, uint8_t variant,
                             uint16_t quantity)
{
    if(inventory.count > GAME_SAVE_INVENTORY_CAPACITY || quantity == 0)
        return quantity == 0;
    for(size_t i = 0; i < inventory.count; ++i)
    {
        const uint32_t entry = inventory.entries[i];
        if(packedInventoryItemId(entry) != itemId ||
           packedInventoryVariant(entry) != variant)
            continue;
        const uint16_t current = packedInventoryQuantity(entry);
        if(current < quantity)
            return false;
        if(current > quantity)
        {
            inventory.entries[i] = packInventoryEntry(
                itemId, variant, static_cast<uint16_t>(current - quantity));
            return true;
        }
        for(size_t j = i + 1; j < inventory.count; ++j)
            inventory.entries[j - 1] = inventory.entries[j];
        --inventory.count;
        inventory.entries[inventory.count] = 0;
        return true;
    }
    return false;
}

uint8_t
packedScenarioValue(const uint8_t bits[13], size_t scenarioIndex)
{
    if(bits == NULL || scenarioIndex >= GAME_SAVE_SCENARIO_COUNT)
        return 0;
    const size_t bit = scenarioIndex * 2;
    return static_cast<uint8_t>((bits[bit / 8] >> (bit & 7)) & 3u);
}

bool
setPackedScenarioValue(uint8_t bits[13], size_t scenarioIndex, uint8_t value)
{
    if(bits == NULL || scenarioIndex >= GAME_SAVE_SCENARIO_COUNT || value > 3)
        return false;
    const size_t bit = scenarioIndex * 2;
    const uint8_t shift = static_cast<uint8_t>(bit & 7);
    const uint8_t mask = static_cast<uint8_t>(3u << shift);
    bits[bit / 8] = static_cast<uint8_t>(
        (bits[bit / 8] & static_cast<uint8_t>(~mask)) |
        static_cast<uint8_t>(value << shift));
    return true;
}

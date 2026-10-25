#include "weapon.h"
#include "../common.h"
#include "weapon_combat_tables.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
namespace
{
#include "weapon_definitions.inc"
void setError(std::string* error, const std::string& message)
{
    if(error != NULL)
        *error = message;
}
bool checkedChunk(const std::vector<uint8_t>& data, size_t offset,
                 WeaponAssetChunk& chunk, std::string* error)
{
    if(offset > data.size() || data.size() - offset < 12)
    {
        setError(error, "truncated RenderWare chunk header");
        return false;
    }
    if (!readRwChunk(data.data(), data.size(), offset, chunk))
    {
        setError(error, "RenderWare chunk extends past the asset");
        return false;
    }
    return true;
}
uint32_t expectedChunkId(uint32_t objectType)
{
    switch(objectType)
    {
    case WEAPON_OBJECT_ATOMIC:
        return RW_CHUNK_ATOMIC;
    case WEAPON_OBJECT_CLUMP:
        return RW_CHUNK_CLUMP;
    case WEAPON_OBJECT_CUSTOM:
        return RW_CHUNK_CUSTOM;
    default:
        return 0;
    }
}
bool validateCustomChunk(const std::vector<uint8_t>& data,
                         const WeaponAssetChunk& chunk,
                         std::string* error)
{
    if(chunk.dataSize < 24)
    {
        setError(error, "custom weapon object has no nested payload header");
        return false;
    }
    WeaponAssetChunk nested;
    if(!checkedChunk(data, chunk.offset + 12, nested, error))
        return false;
    if (nested.id != RW_CHUNK_CUSTOM_PAYLOAD)
    {
        setError(error, "custom weapon object is missing ID 0x78032020");
        return false;
    }
    if(nested.end > chunk.end || nested.dataSize < 0x120)
    {
        setError(error, "custom weapon object payload is truncated");
        return false;
    }
    return true;
}
WeaponPose zeroPose(void)
{
    WeaponPose pose = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    return pose;
}
WeaponAttachment emptyAttachment(void)
{
    WeaponAttachment attachment;
    attachment.ownerSlot = 0xff;
    attachment.boneIndex = -1;
    attachment.relativePose = zeroPose();
    attachment.attached = false;
    return attachment;
}
}
WeaponObjectLibrary::WeaponObjectLibrary()
    : rawData(), texture{}, directory{}, records(), valid(false)
{
}
bool WeaponObjectLibrary::loadFile(const char* path, std::string* error)
{
    clear();
    if(path == NULL)
    {
        setError(error, "weapon asset path is null");
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if(!input)
    {
        setError(error, "unable to open weapon asset");
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if(length < 0 || static_cast<uint64_t>(length) >
       std::numeric_limits<size_t>::max())
    {
        setError(error, "unable to determine weapon asset size");
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::vector<uint8_t> fileData(static_cast<size_t>(length));
    if(!fileData.empty() &&
       !input.read(reinterpret_cast<char*>(fileData.data()), length))
    {
        setError(error, "unable to read weapon asset");
        return false;
    }
    std::string parseError;
    if(!loadMemory(fileData.data(), fileData.size(), &parseError))
    {
        setError(error, parseError);
        return false;
    }
    return true;
}
bool WeaponObjectLibrary::loadMemory(const uint8_t* data, size_t size,
                                     std::string* error)
{
    clear();
    std::vector<uint8_t> candidate;
    if(data == NULL && size != 0)
    {
        setError(error, "weapon asset data is null");
        return false;
    }
    if(size != 0)
        candidate.assign(data, data + size);
    if(candidate.size() < 24)
    {
        setError(error, "weapon asset is too small for its first chunk");
        return false;
    }
    WeaponAssetChunk candidateTexture;
    WeaponAssetChunk candidateDirectory;
    if(!checkedChunk(candidate, 0, candidateTexture, error))
        return false;
    if(candidateTexture.id != 0x16)
    {
        setError(error, "first weapon chunk is not a texture dictionary");
        return false;
    }
    if(!checkedChunk(candidate, candidateTexture.end, candidateDirectory,
                     error))
        return false;
    if (candidateDirectory.id != RW_CHUNK_OBJECT_DIRECTORY)
    {
        setError(error, "second weapon chunk is not the object directory");
        return false;
    }
    if(candidateDirectory.dataSize < 4)
    {
        setError(error, "weapon object directory is missing its count");
        return false;
    }
    const uint8_t* directoryData =
        candidate.data() + candidateDirectory.offset + 12;
    const uint32_t count = read32(directoryData);
    const uint64_t recordBytes = static_cast<uint64_t>(count) * 0x1c;
    if(recordBytes > std::numeric_limits<size_t>::max() - 4 ||
       candidateDirectory.dataSize != 4 +
       static_cast<size_t>(recordBytes))
    {
        setError(error, "weapon object directory length is not 4 + count * 0x1c");
        return false;
    }
    std::vector<WeaponObjectRecord> candidateRecords;
    candidateRecords.resize(count);
    for(uint32_t index = 0; index < count; ++index)
    {
        const uint8_t* source = directoryData + 4 + index * 0x1c;
        WeaponObjectRecord& record = candidateRecords[index];
        record.objectType = read32(source);
        record.objectFlags = read32(source + 4);
        record.payloadReference = readI32(source + 8);
        std::memcpy(record.nameSlot.data(), source + 12,
                    record.nameSlot.size());
        record.scaleBits = read32(source + 24);
        record.scale = record.scaleBits == 0 ? 1.0f : floatFromBits(record.scaleBits);
        record.ownerIndex = -1;
        record.payload = WeaponAssetChunk{};
        record.hasPayload = false;
        if(expectedChunkId(record.objectType) == 0)
        {
            std::ostringstream stream;
            stream << "weapon record " << index
                   << " has unsupported object type " << record.objectType;
            setError(error, stream.str());
            return false;
        }
        if(record.payloadReference > static_cast<int32_t>(index))
        {
            std::ostringstream stream;
            stream << "weapon record " << index
                   << " has a future payload reference";
            setError(error, stream.str());
            return false;
        }
    }
    size_t cursor = candidateDirectory.end;
    for(uint32_t index = 0; index < count; ++index)
    {
        WeaponObjectRecord& record = candidateRecords[index];
        if(record.payloadReference != static_cast<int32_t>(index))
            continue;
        WeaponAssetChunk chunk;
        if(!checkedChunk(candidate, cursor, chunk, error))
            return false;
        if(chunk.id != expectedChunkId(record.objectType))
        {
            std::ostringstream stream;
            stream << "weapon record " << index << " owns chunk 0x"
                   << std::hex << chunk.id << " instead of 0x"
                   << expectedChunkId(record.objectType);
            setError(error, stream.str());
            return false;
        }
        if(record.objectType == WEAPON_OBJECT_CUSTOM &&
           !validateCustomChunk(candidate, chunk, error))
            return false;
        record.ownerIndex = static_cast<int32_t>(index);
        record.payload = chunk;
        record.hasPayload = true;
        cursor = chunk.end;
    }
    for(uint32_t index = 0; index < count; ++index)
    {
        WeaponObjectRecord& record = candidateRecords[index];
        if(record.payloadReference < 0)
            continue;
        int32_t owner = static_cast<int32_t>(index);
        for(uint32_t depth = 0; depth <= count; ++depth)
        {
            if(owner < 0 || static_cast<uint32_t>(owner) >= count)
            {
                std::ostringstream stream;
                stream << "weapon record " << index
                       << " has an invalid payload reference chain";
                setError(error, stream.str());
                return false;
            }
            const WeaponObjectRecord& target = candidateRecords[owner];
            if(target.payloadReference == owner)
                break;
            owner = target.payloadReference;
            if(depth == count)
            {
                std::ostringstream stream;
                stream << "weapon record " << index
                       << " has a cyclic payload reference chain";
                setError(error, stream.str());
                return false;
            }
        }
        if(owner < 0 || static_cast<uint32_t>(owner) >= count ||
           !candidateRecords[owner].hasPayload)
        {
            std::ostringstream stream;
            stream << "weapon record " << index
                   << " resolves to an absent payload";
            setError(error, stream.str());
            return false;
        }
        const WeaponObjectRecord& ownerRecord = candidateRecords[owner];
        if(ownerRecord.payload.id != expectedChunkId(record.objectType))
        {
            std::ostringstream stream;
            stream << "weapon record " << index
                   << " resolves to an incompatible payload";
            setError(error, stream.str());
            return false;
        }
        record.ownerIndex = owner;
        record.payload = ownerRecord.payload;
        record.hasPayload = true;
    }
    if(cursor != candidate.size())
    {
        setError(error, "unclaimed bytes remain after the final weapon object");
        return false;
    }
    rawData.swap(candidate);
    texture = candidateTexture;
    directory = candidateDirectory;
    records.swap(candidateRecords);
    valid = true;
    return true;
}
void WeaponObjectLibrary::clear(void)
{
    rawData.clear();
    texture = WeaponAssetChunk{};
    directory = WeaponAssetChunk{};
    records.clear();
    valid = false;
}
bool WeaponObjectLibrary::loaded(void) const
{
    return valid;
}
size_t WeaponObjectLibrary::objectCount(void) const
{
    return records.size();
}
const WeaponObjectRecord* WeaponObjectLibrary::object(size_t objectId) const
{
    if(objectId >= records.size())
        return NULL;
    return &records[objectId];
}
const WeaponObjectRecord* WeaponObjectLibrary::owner(size_t objectId) const
{
    const WeaponObjectRecord* record = object(objectId);
    if(record == NULL || record->ownerIndex < 0)
        return NULL;
    return object(static_cast<size_t>(record->ownerIndex));
}
const uint8_t* WeaponObjectLibrary::bytes(size_t offset, size_t size) const
{
    if(offset > rawData.size() || size > rawData.size() - offset)
        return NULL;
    return rawData.data() + offset;
}
const std::vector<uint8_t>& WeaponObjectLibrary::data(void) const
{
    return rawData;
}
const WeaponAssetChunk& WeaponObjectLibrary::textureChunk(void) const
{
    return texture;
}
const WeaponAssetChunk& WeaponObjectLibrary::directoryChunk(void) const
{
    return directory;
}
WeaponManager::WeaponManager()
    : objectLibrary(), definitions(), instances(), liveCount(0)
{
    for(WeaponInstance& instanceValue : instances)
        clearInstance(instanceValue);
}
bool WeaponManager::loadFile(const char* path, std::string* error)
{
    if(!objectLibrary.loadFile(path, error))
        return false;
    initializeDefinitions();
    releaseAll();
    return true;
}
bool WeaponManager::loadMemory(const uint8_t* data, size_t size,
                               std::string* error)
{
    if(!objectLibrary.loadMemory(data, size, error))
        return false;
    initializeDefinitions();
    releaseAll();
    return true;
}
void WeaponManager::clear(void)
{
    releaseAll();
    definitions.clear();
    objectLibrary.clear();
}
bool WeaponManager::loaded(void) const
{
    return objectLibrary.loaded();
}
const WeaponObjectLibrary& WeaponManager::library(void) const
{
    return objectLibrary;
}
size_t WeaponManager::objectCount(void) const
{
    return objectLibrary.objectCount();
}
const WeaponObjectRecord* WeaponManager::object(uint16_t objectId) const
{
    return objectLibrary.object(objectId);
}
const WeaponDefinition* WeaponManager::definition(uint16_t objectId) const
{
    if(objectId >= definitions.size())
        return NULL;
    return &definitions[objectId];
}
int16_t WeaponManager::damageBonus(uint16_t objectId) const
{
    const WeaponDefinition* value = definition(objectId);
    return value == NULL ? 0 : value->damageBonus;
}
const WeaponInstance* WeaponManager::equippedBy(uint8_t ownerSlot) const
{
    for(size_t i = 0; i < instances.size(); ++i)
    {
        const WeaponInstance& value = instances[i];
        if(value.lifecycle != WEAPON_LIFECYCLE_EQUIPPED)
            continue;
        if(value.attachment.ownerSlot == ownerSlot)
            return &value;
    }
    return NULL;
}
const WeaponAmmunitionDefinition* weaponAmmunitionDefinition(uint32_t address)
{
    for(const WeaponAmmunitionDefinition& definition : WEAPON_AMMUNITION_DEFINITIONS)
        if(definition.address == address)
            return &definition;
    return NULL;
}

bool weaponInputFallback(int behaviorId, int command, int& move)
{
    if(behaviorId == 0)
        return false;
    const int table = behaviorId == 4 ? 0 : behaviorId == 5 ? 1 :
        behaviorId == 10 ? 2 : 3;
    for(const WeaponInputFallback& row : WEAPON_INPUT_FALLBACKS[table])
    {
        if(row.command == 0x88)
            return false;
        if(row.command == command)
        {
            move = static_cast<int16_t>(row.move);
            return true;
        }
    }
    return false;
}

int weaponEmptyAction(int behaviorId)
{
    return behaviorId >= 0 && behaviorId < 18 ? WEAPON_EMPTY_ACTIONS[behaviorId] : 0;
}

int weaponPickupAction(int behaviorId)
{
    return behaviorId >= 0 && behaviorId < 18 ?
        WEAPON_PICKUP_ACTIONS[behaviorId] : 0;
}

bool WeaponManager::canUse(int16_t managerId) const
{
    const WeaponInstance* held = instance(managerId);
    if(held == NULL)
        return false;
    const WeaponDefinition* row = definition(held->objectId);
    return row != NULL && (row->rawWords[29] == 0 ||
        (weaponAmmunitionDefinition(row->rawWords[29]) != NULL &&
         held->ammunition != 0));
}

bool WeaponManager::consumeAmmunition(int16_t managerId)
{
    const WeaponInstance* held = instance(managerId);
    if(held == NULL)
        return false;
    const WeaponDefinition* row = definition(held->objectId);
    if(row == NULL || weaponAmmunitionDefinition(row->rawWords[29]) == NULL ||
       held->ammunition == 0)
        return false;
    if(held->ammunition != 0xff)
        --instances[managerId].ammunition;
    return true;
}

int16_t WeaponManager::spawn(uint16_t objectId, const WeaponPose& pose)
{
    if(!validObjectId(objectId))
        return WEAPON_INVALID_MANAGER_ID;
    for(int16_t managerId = 0; managerId < WEAPON_MANAGER_CAPACITY;
        ++managerId)
    {
        WeaponInstance& instanceValue = instances[managerId];
        if(instanceValue.lifecycle != WEAPON_LIFECYCLE_FREE)
            continue;
        instanceValue.managerId = managerId;
        instanceValue.objectId = objectId;
        instanceValue.lifecycle = WEAPON_LIFECYCLE_SPAWNED;
        instanceValue.pose = pose;
        instanceValue.attachment = emptyAttachment();
        instanceValue.elapsedSeconds = 0.0f;
        const WeaponAmmunitionDefinition* ammunition =
            weaponAmmunitionDefinition(definitions[objectId].rawWords[29]);
        instanceValue.ammunition = ammunition != NULL ?
            static_cast<int16_t>(ammunition->rawWords[0] & 0xffffu) : 0;
        ++liveCount;
        return managerId;
    }
    return WEAPON_INVALID_MANAGER_ID;
}
bool WeaponManager::pickup(int16_t managerId, uint8_t ownerSlot)
{
    if(!validManagerId(managerId))
        return false;
    WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle != WEAPON_LIFECYCLE_SPAWNED &&
       instanceValue.lifecycle != WEAPON_LIFECYCLE_DROPPED)
        return false;
    instanceValue.lifecycle = WEAPON_LIFECYCLE_PICKED_UP;
    instanceValue.attachment.ownerSlot = ownerSlot;
    instanceValue.attachment.boneIndex = -1;
    instanceValue.attachment.relativePose = zeroPose();
    instanceValue.attachment.attached = false;
    return true;
}
bool WeaponManager::equip(int16_t managerId, uint8_t ownerSlot)
{
    if(!validManagerId(managerId))
        return false;
    WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle != WEAPON_LIFECYCLE_PICKED_UP &&
       instanceValue.lifecycle != WEAPON_LIFECYCLE_EQUIPPED)
        return false;
    instanceValue.lifecycle = WEAPON_LIFECYCLE_EQUIPPED;
    instanceValue.attachment.ownerSlot = ownerSlot;
    return true;
}
bool WeaponManager::attach(int16_t managerId, uint8_t ownerSlot,
                           int16_t boneIndex,
                           const WeaponPose& relativePose)
{
    if(!validManagerId(managerId))
        return false;
    WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle != WEAPON_LIFECYCLE_PICKED_UP &&
       instanceValue.lifecycle != WEAPON_LIFECYCLE_EQUIPPED)
        return false;
    instanceValue.lifecycle = WEAPON_LIFECYCLE_EQUIPPED;
    instanceValue.attachment.ownerSlot = ownerSlot;
    instanceValue.attachment.boneIndex = boneIndex;
    instanceValue.attachment.relativePose = relativePose;
    instanceValue.attachment.attached = true;
    return true;
}
bool WeaponManager::setAttachmentPose(int16_t managerId,
                                       const WeaponPose& pose)
{
    if(!validManagerId(managerId))
        return false;
    WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle != WEAPON_LIFECYCLE_EQUIPPED ||
       !instanceValue.attachment.attached)
        return false;
    instanceValue.attachment.relativePose = pose;
    return true;
}
bool WeaponManager::drop(int16_t managerId, const WeaponPose& pose)
{
    if(!validManagerId(managerId))
        return false;
    WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle != WEAPON_LIFECYCLE_SPAWNED &&
       instanceValue.lifecycle != WEAPON_LIFECYCLE_PICKED_UP &&
       instanceValue.lifecycle != WEAPON_LIFECYCLE_EQUIPPED &&
       instanceValue.lifecycle != WEAPON_LIFECYCLE_DROPPED)
        return false;
    instanceValue.lifecycle = WEAPON_LIFECYCLE_DROPPED;
    instanceValue.pose = pose;
    instanceValue.attachment = emptyAttachment();
    return true;
}
bool WeaponManager::despawn(int16_t managerId)
{
    return release(managerId);
}
bool WeaponManager::release(int16_t managerId)
{
    if(!validManagerId(managerId))
        return false;
    WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle == WEAPON_LIFECYCLE_FREE)
        return false;
    clearInstance(instanceValue);
    if(liveCount > 0)
        --liveCount;
    return true;
}
void WeaponManager::releaseAll(void)
{
    for(WeaponInstance& instanceValue : instances)
        clearInstance(instanceValue);
    liveCount = 0;
}
void WeaponManager::update(float deltaSeconds)
{
    if(!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        return;
    for(WeaponInstance& instanceValue : instances)
    {
        if(instanceValue.lifecycle != WEAPON_LIFECYCLE_FREE)
            instanceValue.elapsedSeconds += deltaSeconds;
    }
}
size_t WeaponManager::activeCount(void) const
{
    return liveCount;
}
const WeaponInstance* WeaponManager::instance(int16_t managerId) const
{
    if(!validManagerId(managerId))
        return NULL;
    const WeaponInstance& instanceValue = instances[managerId];
    if(instanceValue.lifecycle == WEAPON_LIFECYCLE_FREE)
        return NULL;
    return &instanceValue;
}
bool WeaponManager::render(const WeaponRenderSink& sink) const
{
    if(sink.callback == NULL)
        return false;
    for(const WeaponInstance& instanceValue : instances)
    {
        if(instanceValue.lifecycle == WEAPON_LIFECYCLE_FREE)
            continue;
        const WeaponObjectRecord* record = object(instanceValue.objectId);
        if(record == NULL || !record->hasPayload)
            return false;
        WeaponRenderItem item;
        item.managerId = instanceValue.managerId;
        item.objectId = instanceValue.objectId;
        item.objectType = record->objectType;
        item.objectFlags = record->objectFlags;
        item.scale = record->scale;
        item.pose = instanceValue.pose;
        item.chunkData = library().bytes(record->payload.offset,
                                         record->payload.end -
                                         record->payload.offset);
        item.chunkSize = record->payload.end - record->payload.offset;
        if(item.chunkData == NULL)
            return false;
        sink.callback(sink.userData, item);
    }
    return true;
}
void WeaponManager::saveState(std::vector<WeaponPersistentState>& states) const
{
    states.clear();
    states.reserve(liveCount);
    for(const WeaponInstance& instanceValue : instances)
    {
        if(instanceValue.lifecycle == WEAPON_LIFECYCLE_FREE)
            continue;
        WeaponPersistentState state;
        state.objectId = instanceValue.objectId;
        state.lifecycle = instanceValue.lifecycle;
        state.ownerSlot = instanceValue.attachment.ownerSlot;
        state.boneIndex = instanceValue.attachment.boneIndex;
        state.pose = instanceValue.pose;
        state.relativePose = instanceValue.attachment.relativePose;
        state.elapsedSeconds = instanceValue.elapsedSeconds;
        state.attached = instanceValue.attachment.attached;
        state.ammunition = instanceValue.ammunition;
        states.push_back(state);
    }
}
bool WeaponManager::restoreState(const std::vector<WeaponPersistentState>& states)
{
    if(states.size() > WEAPON_MANAGER_CAPACITY)
        return false;
    for(const WeaponPersistentState& state : states)
    {
        if(!validObjectId(state.objectId) ||
           state.lifecycle == WEAPON_LIFECYCLE_FREE ||
           state.lifecycle > WEAPON_LIFECYCLE_DROPPED ||
           !std::isfinite(state.elapsedSeconds) || state.elapsedSeconds < 0.0f)
            return false;
        if(state.attached && state.lifecycle != WEAPON_LIFECYCLE_EQUIPPED)
            return false;
    }
    const std::array<WeaponInstance, WEAPON_MANAGER_CAPACITY> oldInstances =
        instances;
    const size_t oldLiveCount = liveCount;
    releaseAll();
    for(const WeaponPersistentState& state : states)
    {
        const int16_t managerId = spawn(state.objectId, state.pose);
        if(managerId == WEAPON_INVALID_MANAGER_ID)
        {
            instances = oldInstances;
            liveCount = oldLiveCount;
            return false;
        }
        WeaponInstance& instanceValue = instances[managerId];
        instanceValue.lifecycle = state.lifecycle;
        instanceValue.attachment.ownerSlot = state.ownerSlot;
        instanceValue.attachment.boneIndex = state.boneIndex;
        instanceValue.attachment.relativePose = state.relativePose;
        instanceValue.attachment.attached = state.attached;
        instanceValue.elapsedSeconds = state.elapsedSeconds;
        instanceValue.ammunition = state.ammunition;
    }
    return true;
}
bool WeaponManager::validManagerId(int16_t managerId)
{
    return managerId >= 0 && managerId < WEAPON_MANAGER_CAPACITY;
}
bool WeaponManager::validObjectId(uint16_t objectId) const
{
    const WeaponObjectRecord* record = object(objectId);
    return objectLibrary.loaded() && record != NULL && record->hasPayload;
}
void WeaponManager::initializeDefinitions(void)
{
    definitions.clear();
    definitions.resize(objectCount());
    for(size_t objectId = 0; objectId < definitions.size(); ++objectId)
    {
        WeaponDefinition& definitionValue = definitions[objectId];
        definitionValue.objectId = static_cast<uint16_t>(objectId);
        definitionValue.behaviorId = 0;
        definitionValue.callbackAddress = 0;
        definitionValue.verticalOffset = 0.0f;
        definitionValue.damageBonus = 0;
        definitionValue.companion_0x62 = 0;
        definitionValue.rawWords.fill(0);
        if(objectId < 51)
        {
            for(size_t word = 0; word < definitionValue.rawWords.size();
                ++word)
                definitionValue.rawWords[word] =
                    kRecoveredWeaponDefinitionWords[objectId][word];
            definitionValue.callbackAddress =
                definitionValue.rawWords[0];
            definitionValue.behaviorId = static_cast<uint16_t>(
                definitionValue.rawWords[1]);
            definitionValue.damageBonus = static_cast<int16_t>(
                definitionValue.rawWords[24] & 0xffffu);
            definitionValue.companion_0x62 = static_cast<uint16_t>(
                (definitionValue.rawWords[24] >> 16) & 0xffffu);
            const float verticalOffset = floatFromBits(definitionValue.rawWords[21]);
            if(std::isfinite(verticalOffset))
                definitionValue.verticalOffset = verticalOffset;
        }
        else
        {
            const WeaponObjectRecord* record = object(objectId);
            if(record != NULL)
            {
                definitionValue.rawWords[0] = record->objectType;
                definitionValue.rawWords[1] = record->objectFlags;
                definitionValue.rawWords[2] = static_cast<uint32_t>(
                    record->payloadReference);
                definitionValue.rawWords[6] = record->scaleBits;
            }
        }
    }
}
void WeaponManager::clearInstance(WeaponInstance& instanceValue)
{
    instanceValue.managerId = WEAPON_INVALID_MANAGER_ID;
    instanceValue.objectId = 0;
    instanceValue.lifecycle = WEAPON_LIFECYCLE_FREE;
    instanceValue.pose = zeroPose();
    instanceValue.attachment = emptyAttachment();
    instanceValue.elapsedSeconds = 0.0f;
    instanceValue.ammunition = 0;
}

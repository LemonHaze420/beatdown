#ifndef WEAPON_H
#define WEAPON_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
enum WeaponObjectType : uint32_t
{
    WEAPON_OBJECT_ATOMIC = 0,
    WEAPON_OBJECT_CLUMP = 1,
    WEAPON_OBJECT_CUSTOM = 2
};
const int WEAPON_MANAGER_CAPACITY = 0x30;
const int WEAPON_INVALID_MANAGER_ID = -1;
struct WeaponAssetChunk
{
    uint32_t id;
    uint32_t dataSize;
    uint32_t libraryId;
    size_t offset;
    size_t end;
};
struct WeaponObjectRecord
{
    uint32_t objectType;
    uint32_t objectFlags;
    int32_t payloadReference;
    std::array<uint8_t, 12> nameSlot;
    uint32_t scaleBits;
    float scale;
    int32_t ownerIndex;
    WeaponAssetChunk payload;
    bool hasPayload;
};

class WeaponObjectLibrary
{
public:
    WeaponObjectLibrary();
    bool loadFile(const char* path, std::string* error = NULL);
    bool loadMemory(const uint8_t* data, size_t size,
                    std::string* error = NULL);
    void clear(void);
    bool loaded(void) const;
    size_t objectCount(void) const;
    const WeaponObjectRecord* object(size_t objectId) const;
    const WeaponObjectRecord* owner(size_t objectId) const;
    const uint8_t* bytes(size_t offset, size_t size) const;
    const std::vector<uint8_t>& data(void) const;
    const WeaponAssetChunk& textureChunk(void) const;
    const WeaponAssetChunk& directoryChunk(void) const;
private:
    std::vector<uint8_t> rawData;
    WeaponAssetChunk texture;
    WeaponAssetChunk directory;
    std::vector<WeaponObjectRecord> records;
    bool valid;
};
struct WeaponPose
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    float roll;
};

struct WeaponDefinition
{
    uint16_t objectId;
    uint16_t behaviorId;
    uint32_t callbackAddress;
    float verticalOffset;

    int16_t damageBonus;

    uint16_t companion_0x62;
    std::array<uint32_t, 33> rawWords;
};

struct WeaponAmmunitionDefinition
{
    uint32_t address;
    std::array<uint32_t, 8> rawWords;
};
struct WeaponInputFallback
{
    int32_t command;
    int32_t move;
};
const WeaponAmmunitionDefinition* weaponAmmunitionDefinition(uint32_t address);

bool weaponInputFallback(int behaviorId, int command, int& move);
int weaponEmptyAction(int behaviorId);
int weaponPickupAction(int behaviorId);

enum WeaponLifecycle : uint8_t
{
    WEAPON_LIFECYCLE_FREE = 0,
    WEAPON_LIFECYCLE_SPAWNED = 1,
    WEAPON_LIFECYCLE_PICKED_UP = 2,
    WEAPON_LIFECYCLE_EQUIPPED = 3,
    WEAPON_LIFECYCLE_DROPPED = 4
};
struct WeaponAttachment
{
    uint8_t ownerSlot;
    int16_t boneIndex;
    WeaponPose relativePose;
    bool attached;
};
struct WeaponInstance
{
    int16_t managerId;
    uint16_t objectId;
    WeaponLifecycle lifecycle;
    WeaponPose pose;
    WeaponAttachment attachment;
    float elapsedSeconds;
    int16_t ammunition;
};
struct WeaponPersistentState
{
    uint16_t objectId;
    WeaponLifecycle lifecycle;
    uint8_t ownerSlot;
    int16_t boneIndex;
    WeaponPose pose;
    WeaponPose relativePose;
    float elapsedSeconds;
    bool attached;
    int16_t ammunition;
};

struct WeaponSaveRecord
{
    uint16_t objectId;
    uint8_t lifecycle;
    uint8_t ownerSlot;
    int16_t boneIndex;
    uint8_t attached;
    uint8_t reserved;
    WeaponPose pose;
    WeaponPose relativePose;
    float elapsedSeconds;
};
struct WeaponRenderItem
{
    int16_t managerId;
    uint16_t objectId;
    uint32_t objectType;
    uint32_t objectFlags;
    float scale;
    WeaponPose pose;
    const uint8_t* chunkData;
    size_t chunkSize;
};
typedef void (*WeaponRenderCallback)(void* userData,
                                     const WeaponRenderItem& item);
struct WeaponRenderSink
{
    void* userData;
    WeaponRenderCallback callback;
};
class WeaponManager
{
public:
    WeaponManager();
    bool loadFile(const char* path, std::string* error = NULL);
    bool loadMemory(const uint8_t* data, size_t size,
                    std::string* error = NULL);
    void clear(void);
    bool loaded(void) const;
    const WeaponObjectLibrary& library(void) const;
    size_t objectCount(void) const;
    const WeaponObjectRecord* object(uint16_t objectId) const;
    const WeaponDefinition* definition(uint16_t objectId) const;

    int16_t damageBonus(uint16_t objectId) const;

    const WeaponInstance* equippedBy(uint8_t ownerSlot) const;
    bool canUse(int16_t managerId) const;

    bool consumeAmmunition(int16_t managerId);
    int16_t spawn(uint16_t objectId, const WeaponPose& pose);
    bool pickup(int16_t managerId, uint8_t ownerSlot);
    bool equip(int16_t managerId, uint8_t ownerSlot);
    bool attach(int16_t managerId, uint8_t ownerSlot, int16_t boneIndex,
                const WeaponPose& relativePose);
    bool setAttachmentPose(int16_t managerId, const WeaponPose& pose);
    bool drop(int16_t managerId, const WeaponPose& pose);
    bool despawn(int16_t managerId);
    bool release(int16_t managerId);
    void releaseAll(void);
    void update(float deltaSeconds);
    size_t activeCount(void) const;
    const WeaponInstance* instance(int16_t managerId) const;
    bool render(const WeaponRenderSink& sink) const;
    void saveState(std::vector<WeaponPersistentState>& states) const;
    bool restoreState(const std::vector<WeaponPersistentState>& states);
private:
    static bool validManagerId(int16_t managerId);
    bool validObjectId(uint16_t objectId) const;
    void initializeDefinitions(void);
    void clearInstance(WeaponInstance& instance);
    WeaponObjectLibrary objectLibrary;
    std::vector<WeaponDefinition> definitions;
    std::array<WeaponInstance, WEAPON_MANAGER_CAPACITY> instances;
    size_t liveCount;
};
#endif

#include "stage/stage.h"
#include "stage/weapon_internal.h"
#include "stage/weapon_grants.h"
#include "game/bits.h"
#include "formats/STAGE_OBJECT/stage_object_data.h"
#include "formats/OL/weapon.h"
#include "xaf_runtime.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{

struct SectionSpan
{
    uint32_t offset;
    uint32_t length;
};

uint16_t read16(const uint8_t* data)
{
    uint16_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

float readFloat(const uint8_t* data)
{
    float value;
    memcpy(&value, data, sizeof(value));
    return value;
}

using game_detail::floatFromBits;

const stage_object_data::Stage* recoveredStage(uint16_t stageId)
{
    if(stageId >= stage_object_data::STAGE_COUNT)
        return NULL;
    return &stage_object_data::STAGES[stageId];
}

bool recoveredDefinition(uint16_t stageId, uint32_t objectIndex,
                         uint8_t* type, uint16_t* flags)
{
    const stage_object_data::Stage* stage = recoveredStage(stageId);
    if(stage == NULL || objectIndex == 0 ||
       objectIndex > stage->definitionCount)
        return false;
    const size_t index = stage->firstDefinition + objectIndex - 1;
    if(type != NULL)
        *type = stage_object_data::DEFINITION_TYPES[index];
    if(flags != NULL)
        *flags = stage_object_data::DEFINITION_FLAGS[index];
    return true;
}

float recoveredHeightOffset(int selector)
{
    if(selector < 0 || selector >= stage_object_data::HEIGHT_COUNT)
        return 0.0f;
    return floatFromBits(stage_object_data::HEIGHT_OFFSET_BITS[selector]);
}

const size_t FSTAGE_NPC_ROOT_COUNT = 128;
const size_t FSTAGE_NPC_STAGE_COUNT = 56;
const size_t FSTAGE_NPC_RECORD_SIZE = 0x200;
const size_t FSTAGE_NPC_ROOT_BYTES =
    FSTAGE_NPC_ROOT_COUNT * sizeof(uint32_t);

void setV3d(rw::V3d& value, float x, float y, float z)
{
    value.x = x;
    value.y = y;
    value.z = z;
}

void clearV3d(rw::V3d& value)
{
    setV3d(value, 0.0f, 0.0f, 0.0f);
}

bool includeSphereBounds(bool& haveBounds, rw::V3d& minBounds,
                         rw::V3d& maxBounds, const rw::Sphere& sphere)
{
    const float minX = sphere.center.x - sphere.radius;
    const float minY = sphere.center.y - sphere.radius;
    const float minZ = sphere.center.z - sphere.radius;
    const float maxX = sphere.center.x + sphere.radius;
    const float maxY = sphere.center.y + sphere.radius;
    const float maxZ = sphere.center.z + sphere.radius;
    if(!haveBounds)
    {
        setV3d(minBounds, minX, minY, minZ);
        setV3d(maxBounds, maxX, maxY, maxZ);
        haveBounds = true;
        return true;
    }
    if(minX < minBounds.x)
        minBounds.x = minX;
    if(minY < minBounds.y)
        minBounds.y = minY;
    if(minZ < minBounds.z)
        minBounds.z = minZ;
    if(maxX > maxBounds.x)
        maxBounds.x = maxX;
    if(maxY > maxBounds.y)
        maxBounds.y = maxY;
    if(maxZ > maxBounds.z)
        maxBounds.z = maxZ;
    return true;
}

bool calculateClumpBounds(rw::Clump* clump, rw::V3d& minBounds,
                          rw::V3d& maxBounds, float& radius)
{
    bool haveBounds = false;
    radius = 0.0f;
    FORLIST(link, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(link);
        if(atomic == NULL || atomic->geometry == NULL)
            continue;
        const rw::Sphere& sphere = *atomic->getWorldBoundingSphere();
        includeSphereBounds(haveBounds, minBounds, maxBounds, sphere);
        if(sphere.radius > radius)
            radius = sphere.radius;
    }
    return haveBounds;
}

bool readDirectory(const std::vector<uint8_t>& data,
                   std::vector<SectionSpan>& sections)
{
    sections.clear();
    if(data.size() < 4)
        return false;
    const uint32_t count = read32(data.data());
    if(count < 9 || count > 10 || 4u + count * 8u > data.size())
        return false;

    sections.reserve(count);
    uint64_t previousEnd = 4u + count * 8u;
    for(uint32_t i = 0; i < count; ++i)
    {
        SectionSpan section;
        section.offset = read32(data.data() + 4u + i * 8u);
        section.length = read32(data.data() + 8u + i * 8u);
        const uint64_t end = static_cast<uint64_t>(section.offset) +
            section.length;
        if(end > data.size() ||
           (section.length != 0 && section.offset < previousEnd))
            return false;
        if(section.length != 0)
            previousEnd = end;
        sections.push_back(section);
    }
    return true;
}

rw::Clump* readClump(const uint8_t* data, uint32_t length)
{
    if(length < 12)
        return NULL;
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(data), length);
    if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
        return NULL;
    return rw::Clump::streamRead(&stream);
}

const uint8_t STAGE_BLEND_ENABLE_ALPHA = 0xfe;

bool isAlphaMaskedStageTexture(const FstageScene* scene,
                               const rw::Texture* texture)
{
    if(scene == NULL || texture == NULL)
        return false;
    for(size_t i = 0;
        i < scene->worldGeometry.alphaMaskedTextures.size(); ++i)
    {
        if(scene->worldGeometry.alphaMaskedTextures[i] == texture)
            return true;
    }
    return false;
}

void renderStageAtomic(rw::Atomic* atomic)
{

    rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
    rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);

    bool blended = false;
    const rw::Geometry* geometry = atomic->geometry;
    if(geometry != NULL && geometry->matList.numMaterials > 0)
    {
        blended = true;
        for(int32_t i = 0; i < geometry->matList.numMaterials && blended; ++i)
        {
            const rw::Material* material = geometry->matList.materials[i];
            blended = material != NULL && material->color.alpha != 0xff;
        }
    }
    rw::SetRenderState(rw::ZWRITEENABLE, blended ? 0 : 1);

    atomic->getPipeline()->render(atomic);

    if(blended)
        rw::SetRenderState(rw::ZWRITEENABLE, 1);
}

void prepareStageClump(FstageScene* scene, rw::Clump* clump,
                       bool worldSector = false)
{
    FORLIST(atomicLink, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
        rw::Geometry* geometry = atomic->geometry;
        if((geometry->flags & rw::Geometry::NATIVE) != 0)
            atomic->uninstance();
        atomic->pipeline = NULL;
        atomic->setRenderCB(renderStageAtomic);

        if(worldSector)
            geometry->flags &= ~rw::Geometry::LIGHT;
        else
            geometry->flags |= rw::Geometry::LIGHT;

        bool hasTexturedMaterial = false;
        for(int32_t i = 0; i < geometry->matList.numMaterials; ++i)
        {
            rw::Material* material = geometry->matList.materials[i];
            material->surfaceProps.ambient = 1.0f;
            material->surfaceProps.diffuse = 1.0f;
            if(material->color.alpha == 0xff &&
               isAlphaMaskedStageTexture(scene, material->texture))
            {

                material->color.alpha = STAGE_BLEND_ENABLE_ALPHA;
            }
            if(material->texture != NULL && material->texture->raster != NULL)
                hasTexturedMaterial = true;
        }
        if(!hasTexturedMaterial && geometry->matList.numMaterials > 0)
            atomic->setFlags(atomic->getFlags() & ~rw::Atomic::RENDER);
    }
}

bool loadTextureDictionary(const std::vector<uint8_t>& data,
                           const SectionSpan& section,
                           rw::TexDictionary*& dictionary)
{
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(data.data()) + section.offset,
                section.length);
    if(!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, NULL, NULL))
        return false;
    dictionary = rw::TexDictionary::streamRead(&stream);
    if(dictionary == NULL)
        return false;

    FORLIST(textureLink, dictionary->textures)
    {
        rw::Texture* texture = rw::Texture::fromDict(textureLink);
        texture->raster = rw::Raster::convertTexToCurrentPlatform(texture->raster);
        if(texture->raster == NULL)
            return false;
    }
    rw::TexDictionary::setCurrent(dictionary);
    return true;
}

bool addStageClump(FstageScene* scene, rw::Clump* clump)
{
    if(clump == NULL)
        return false;
    prepareStageClump(scene, clump);
    scene->world->addClump(clump);
    scene->clumps.push_back(clump);
    if(scene->clump == NULL)
        scene->clump = clump;
    return true;
}

void loadAnimations(const std::vector<uint8_t>& data,
                    const SectionSpan& section,
                    std::vector<rw::Animation*>& animations)
{
    if(section.length == 0)
        return;
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(data.data()) + section.offset,
                section.length);
    while(animations.size() < 16 &&
          rw::findChunk(&stream, rw::ID_ANIMANIMATION, NULL, NULL))
    {
        rw::Animation* animation = rw::Animation::streamRead(&stream);
        if(animation == NULL)
            break;
        animations.push_back(animation);
    }
}

bool loadObjectTemplates(FstageScene* scene, const SectionSpan& section)
{
    const uint8_t* data = scene->data.data() + section.offset;
    if(section.length < 4)
        return false;
    const uint32_t count = read32(data);
    if(4u + static_cast<uint64_t>(count) * 8u > section.length)
        return false;

    scene->objectTemplates.reserve(count);
    scene->objectBounds.reserve(count);
    for(uint32_t i = 0; i < count; ++i)
    {
        const uint32_t offset = read32(data + 4u + i * 8u);
        const uint32_t length = read32(data + 8u + i * 8u);
        if(static_cast<uint64_t>(offset) + length > section.length)
            return false;
        rw::Clump* clump = readClump(data + offset, length);
        if(clump == NULL)
            return false;
        prepareStageClump(scene, clump);
        if(clump->getFrame() != NULL)
            clump->getFrame()->updateObjects();
        FstageObjectBounds bounds;
        bounds.objectIndex = i + 1;
        bounds.radius = 0.0f;
        if(!calculateClumpBounds(clump, bounds.minBounds,
                                 bounds.maxBounds, bounds.radius))
        {
            clearV3d(bounds.minBounds);
            clearV3d(bounds.maxBounds);
        }
        scene->objectTemplates.push_back(clump);
        scene->objectBounds.push_back(bounds);
    }
    return true;
}

void setPlacementMetadata(const uint8_t* record, FstagePlacement& placement)
{
    placement.objectIndex = static_cast<int32_t>(read32(record));
    placement.translation.x = readFloat(record + 4);
    placement.translation.y = readFloat(record + 8);
    placement.translation.z = readFloat(record + 0x0c);
    placement.quaternion[0] = readFloat(record + 0x10);
    placement.quaternion[1] = readFloat(record + 0x14);
    placement.quaternion[2] = readFloat(record + 0x18);
    placement.quaternion[3] = readFloat(record + 0x1c);
    placement.visible = true;
}

void calculateTransformedBounds(rw::Clump* clump, FstagePlacement& placement)
{
    clump->getFrame()->updateObjects();
    float radius = 0.0f;
    if(!calculateClumpBounds(clump, placement.minBounds,
                             placement.maxBounds, radius))
    {
        clearV3d(placement.minBounds);
        clearV3d(placement.maxBounds);
    }
}

void placementMatrix(const uint8_t* record, rw::Matrix& matrix)
{
    const float x = readFloat(record + 0x10);
    const float y = readFloat(record + 0x14);
    const float z = readFloat(record + 0x18);
    const float w = readFloat(record + 0x1c);
    const float lengthSquared = x * x + y * y + z * z + w * w;
    if(lengthSquared <= 0.000001f)
        matrix.setIdentity();
    else
        matrix.rotate(rw::makeQuat(w, x, y, z), rw::COMBINEREPLACE);
    matrix.pos.x = readFloat(record + 4);
    matrix.pos.y = readFloat(record + 8);
    matrix.pos.z = readFloat(record + 0x0c);
    matrix.optimize();
}

bool instantiatePlacements(FstageScene* scene, const SectionSpan& section)
{
    const uint8_t* data = scene->data.data() + section.offset;
    if(section.length < 4 || data[0] != 'P' || data[1] != 'B')
        return false;
    const uint16_t count = read16(data + 2);
    if(4u + static_cast<uint64_t>(count) * 0x20u != section.length)
        return false;

    scene->placements.reserve(count);
    for(uint16_t i = 0; i < count; ++i)
    {
        const uint8_t* record = data + 4u + i * 0x20u;
        const int32_t objectIndex = static_cast<int32_t>(read32(record));
        if(objectIndex <= 0 ||
           static_cast<size_t>(objectIndex) > scene->objectTemplates.size())
            return false;
        rw::Clump* clump = scene->objectTemplates[objectIndex - 1]->clone();
        if(clump == NULL)
            return false;
        rw::Matrix matrix;
        placementMatrix(record, matrix);
        if(clump->getFrame() != NULL)
            clump->getFrame()->transform(&matrix, rw::COMBINEREPLACE);
        FstagePlacement placement;
        setPlacementMetadata(record, placement);
        placement.clump = clump;
        calculateTransformedBounds(clump, placement);
        scene->placements.push_back(placement);
        scene->world->addClump(clump);
        scene->clumps.push_back(clump);
        if(scene->clump == NULL)
            scene->clump = clump;
    }
    return true;
}

void includeSphere(bool& haveBounds, rw::V3d& center, float& radius,
                   const rw::Sphere& sphere)
{
    if(!haveBounds)
    {
        center = sphere.center;
        radius = sphere.radius;
        haveBounds = true;
        return;
    }

    const float x = sphere.center.x - center.x;
    const float y = sphere.center.y - center.y;
    const float z = sphere.center.z - center.z;
    const float distance = sqrtf(x * x + y * y + z * z);
    if(distance + sphere.radius <= radius)
        return;
    if(distance + radius <= sphere.radius)
    {
        center = sphere.center;
        radius = sphere.radius;
        return;
    }

    const float newRadius = (radius + distance + sphere.radius) * 0.5f;
    if(distance > 0.0f)
    {
        const float scale = (newRadius - radius) / distance;
        center.x += x * scale;
        center.y += y * scale;
        center.z += z * scale;
    }
    radius = newRadius;
}

void calculateBounds(FstageScene* scene)
{
    bool haveBounds = false;
    scene->haveBounds = false;
    scene->radius = 0.0f;
    clearV3d(scene->minBounds);
    clearV3d(scene->maxBounds);
    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        scene->clumps[i]->getFrame()->updateObjects();
        FORLIST(link, scene->clumps[i]->atomics)
        {
            const rw::Sphere& sphere =
                *rw::Atomic::fromClump(link)->getWorldBoundingSphere();
            includeSphere(haveBounds, scene->center, scene->radius, sphere);
            includeSphereBounds(scene->haveBounds, scene->minBounds,
                                scene->maxBounds, sphere);
        }
    }
    if(!haveBounds)
    {
        scene->center.x = scene->center.y = scene->center.z = 0.0f;
        scene->radius = 10.0f;
        scene->haveBounds = false;
    }
}
}

FstagePlacement::FstagePlacement()
    : objectIndex(0), visible(true), registeredCollider(false), clump(NULL)
{
    translation.x = translation.y = translation.z = 0.0f;
    quaternion[0] = quaternion[1] = quaternion[2] = 0.0f;
    quaternion[3] = 1.0f;
    minBounds.x = minBounds.y = minBounds.z = 0.0f;
    maxBounds.x = maxBounds.y = maxBounds.z = 0.0f;
}

FstageObjectBounds::FstageObjectBounds()
    : objectIndex(0), radius(0.0f)
{
    minBounds.x = minBounds.y = minBounds.z = 0.0f;
    maxBounds.x = maxBounds.y = maxBounds.z = 0.0f;
}

FstageNpcPositionRow::FstageNpcPositionRow()
{
    for(size_t i = 0; i < FSTAGE_NPC_LANE_COUNT; ++i)
        values[i] = 0;
}

int16_t FstageNpcPositionRow::kind() const
{
    return values[0];
}

int16_t FstageNpcPositionRow::selector() const
{
    return values[1];
}

int16_t FstageNpcPositionRow::interactionSelector() const
{
    return values[3];
}

FstageSpawnTransform::FstageSpawnTransform()
    : headingDegrees(0.0f)
{
    position.x = position.y = position.z = 0.0f;
}

FstageNpcSelectionState::FstageNpcSelectionState()
    : stageId(0), storyPhase(0), battleRoot(0),
      alternateOverlayMode(0), recruitmentMode(0),
      actorResourceProfile(-1), storyTransitionMode(0),
      phase11ModeFlags(0), gameStateBit1(false),
      suppressPostBattleOverlays(false), suppressPartyOverlay(false),
      trackedOpponent(false), resetRecruitmentRoot(false),
      encounterSubstate(false), phase10Stage31Suppress(false),
      phase11Stage49Override(false), userData(NULL), eventFlag(NULL),
      recruitmentState(NULL), baseSelectorAvailable(NULL),
      overlayDecision(NULL)
{
    partyRoots[0] = partyRoots[1] = -1;
}

FstageNpcActorBinding::FstageNpcActorBinding()
    : actorSlot(-1), npcPositionRowIndex(-1), characterId(0), active(false)
{
    position.x = position.y = position.z = 0.0f;
}

FstageNpcInteractionCandidate::FstageNpcInteractionCandidate()
    : kind(0), rowIndex(-1), actorSlot(-1), interactionSelector(0),
      characterId(0), radius(0.0f), distanceSquared(0.0f)
{
    position.x = position.y = position.z = 0.0f;
}

FstageObjectRuntimeHooks::FstageObjectRuntimeHooks()
    : userData(NULL), eventSet(NULL), sound(NULL), effect(NULL),
      weaponAction(NULL)
{
}

FstageObjectSpawnState::FstageObjectSpawnState()
    : storyPhase(0), enabledKindMask(7), gameStateBit1(false),
      userData(NULL), eventFlag(NULL), modelResolver(NULL)
{
}

FstageObjectInstance::FstageObjectInstance()
    : clump(NULL), hierarchy(NULL), pool(-1), eventFlag(0), health(0),
      acceptedHitCount(0), fadeCounter(0), stageId(0xffff),
      recordIndex(0xffff), state(0), definitionSubtype(0),
      renderControl(0), runtimeFlags(0), kind(0), sub(0), gateValue(-1),
      gateAuxiliary(0), alpha(0xff), phase(FSTAGE_OBJECT_INERT),
      registeredCollider(false), animationInitialized(false)
{
}

FstageObjectContact::FstageObjectContact()
    : damage(0), radius(0.0f)
{
    center.x = center.y = center.z = 0.0f;
    effectTransform.setIdentity();
}

FstageObjectInstallResult::FstageObjectInstallResult()
    : authored(0), eligible(0), instantiated(0), missingModel(0)
{
}

FstageWeaponManagerRecord::FstageWeaponManagerRecord()
    : selector(0), flags(0), managerId(FSTAGE_WEAPON_INVALID_ID),
      clump(NULL), linkedManagerId(FSTAGE_WEAPON_INVALID_ID)
{
}

FstageWeaponInstance::FstageWeaponInstance()
    : stageId(0xffff), recordIndex(0xffff), selector(0),
      primaryManagerId(FSTAGE_WEAPON_INVALID_ID),
      managerId(FSTAGE_WEAPON_INVALID_ID), ownerSlot(-1),
      attachmentSlot(-1), originalOwnerSlot(-1), originalMoveSlot(-1), flags(0),
      motionState(FSTAGE_WEAPON_MOTION_IDLE), groundY(0.0f),
      active(false), broken(false)
{
    velocity.x = velocity.y = velocity.z = 0.0f;
    angularVelocity.x = angularVelocity.y = angularVelocity.z = 0.0f;
    previousWorld.setIdentity();
}

FstageWeaponActorState::FstageWeaponActorState()
    : slotIndex(0), battleSide(0), actionFlags(0),
      heldWeapon(FSTAGE_WEAPON_INVALID_ID),
      targetedWeapon(FSTAGE_WEAPON_INVALID_ID),
      linkedHeldWeapon(FSTAGE_WEAPON_INVALID_ID)
{
    position.x = position.y = position.z = 0.0f;
    forward.x = forward.y = 0.0f;
    forward.z = 1.0f;
    attachmentBones[0] = attachmentBones[1] = NULL;
}

FstageWeaponManager::FstageWeaponManager()
    : managerCount(0), weaponCount(0)
{
}

FstageInteractionMarker::FstageInteractionMarker()
    : broadphaseAtomic(NULL), collisionAtomic(NULL), consumedTextId(-1),
      textId(0), markerKey(0), commandParameter(-1), side(0),
      contextValue(0)
{
}

FstageMarkerQueryState::FstageMarkerQueryState()
    : stageId(0), enabledTextMask((UINT64_C(1) << 56) - 1),
      userData(NULL), eventFlag(NULL)
{
}

FstageMarkerPrompt::FstageMarkerPrompt()
    : resolution(FSTAGE_MARKER_NONE), button(FSTAGE_MARKER_BUTTON_NONE),
      textId(0), commandParameter(-1), side(0), contextValue(0),
      displayTextId(-1), screenX(588.0f), screenY(450.0f)
{
}

FstageNpcPositionData::FstageNpcPositionData()
{
}

bool FstageNpcPositionData::load(const char* path)
{
    if(path == NULL)
        return false;
    FILE* file = fopen(path, "rb");
    if(file == NULL)
        return false;
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    const long fileLength = ftell(file);
    if(fileLength < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(fileLength));
    const size_t readCount = bytes.empty() ? 0 :
        fread(bytes.data(), 1, bytes.size(), file);
    fclose(file);
    if(readCount != bytes.size())
        return false;
    return parse(bytes.empty() ? NULL : bytes.data(), bytes.size());
}

bool FstageNpcPositionData::parse(const uint8_t* bytes, size_t length)
{
    if(bytes == NULL || length < FSTAGE_NPC_ROOT_BYTES)
        return false;
    const uint64_t fileLength = static_cast<uint64_t>(length);
    for(size_t rootIndex = 0; rootIndex < FSTAGE_NPC_ROOT_COUNT;
        ++rootIndex)
    {
        const uint32_t rootOffset = read32(bytes + rootIndex * 4);
        if(rootOffset == 0)
            continue;
        if((rootOffset & 3u) != 0 ||
           static_cast<uint64_t>(rootOffset) +
               FSTAGE_NPC_STAGE_COUNT * sizeof(uint32_t) > fileLength)
            return false;
        for(size_t stageIndex = 0; stageIndex < FSTAGE_NPC_STAGE_COUNT;
            ++stageIndex)
        {
            const uint32_t recordOffset =
                read32(bytes + rootOffset + stageIndex * 4);
            if(recordOffset == 0)
                continue;
            if((recordOffset & 3u) != 0 ||
               static_cast<uint64_t>(recordOffset) +
                   FSTAGE_NPC_RECORD_SIZE > fileLength)
                return false;
        }
    }
    source.assign(bytes, bytes + length);
    return true;
}

bool FstageNpcPositionData::selectRows(
    uint32_t tableIndex, uint32_t stageIndex,
    std::vector<FstageNpcPositionRow>& rows) const
{
    rows.clear();
    if(tableIndex >= FSTAGE_NPC_ROOT_COUNT ||
       stageIndex >= FSTAGE_NPC_STAGE_COUNT ||
       source.size() < FSTAGE_NPC_ROOT_BYTES)
        return false;
    const uint32_t rootOffset = read32(source.data() + tableIndex * 4);
    if(rootOffset == 0)
        return false;
    const uint32_t recordOffset =
        read32(source.data() + rootOffset + stageIndex * 4);
    if(recordOffset == 0)
        return false;
    if(static_cast<uint64_t>(recordOffset) + FSTAGE_NPC_RECORD_SIZE >
       source.size())
        return false;
    rows.resize(FSTAGE_NPC_ROW_COUNT);
    for(size_t rowIndex = 0; rowIndex < FSTAGE_NPC_ROW_COUNT; ++rowIndex)
    {
        const uint8_t* rowData =
            source.data() + recordOffset + rowIndex * 0x40;
        for(size_t lane = 0; lane < FSTAGE_NPC_LANE_COUNT; ++lane)
            rows[rowIndex].values[lane] =
                static_cast<int16_t>(read16(rowData + lane * 2));
    }
    return true;
}

const std::vector<uint8_t>& FstageNpcPositionData::bytes() const
{
    return source;
}

namespace
{

bool npcEvent(const FstageNpcSelectionState& state, uint16_t flag)
{
    return state.eventFlag != NULL &&
        state.eventFlag(flag, state.userData);
}

int16_t npcRecruitment(const FstageNpcSelectionState& state,
                       uint16_t characterId)
{
    if(state.recruitmentState == NULL)
        return 0;
    return state.recruitmentState(state.storyPhase, characterId,
                                  state.userData);
}

int16_t firstRecruitmentState(const FstageNpcSelectionState& state,
                              const uint16_t* characterIds, size_t count)
{
    for(size_t i = 0; i < count; ++i)
    {
        const int16_t value = npcRecruitment(state, characterIds[i]);
        if(value != 0)
            return value;
    }
    return 0;
}

void clearNpcRow(FstageNpcPositionRow& row)
{
    row = FstageNpcPositionRow();
}

void applyBaseNpcRows(const std::vector<FstageNpcPositionRow>& source,
                      const FstageNpcSelectionState& state,
                      std::vector<FstageNpcPositionRow>& destination)
{
    destination = source;
    for(size_t i = 0; i < destination.size(); ++i)
    {
        if(destination[i].kind() != 1 ||
           state.baseSelectorAvailable == NULL)
            continue;
        if(!state.baseSelectorAvailable(destination[i].selector(),
                                        state.userData))
            clearNpcRow(destination[i]);
    }
}

void applyOverlayNpcRows(const std::vector<FstageNpcPositionRow>& source,
                         const FstageNpcSelectionState& state,
                         bool battleLayer,
                         std::vector<FstageNpcPositionRow>& destination)
{
    if(source.size() != FSTAGE_NPC_ROW_COUNT ||
       destination.size() != FSTAGE_NPC_ROW_COUNT)
        return;
    for(size_t i = 0; i < FSTAGE_NPC_ROW_COUNT; ++i)
    {
        const int16_t kind = source[i].kind();
        if(kind == 4 || kind == 5)
        {
            clearNpcRow(destination[i]);
            continue;
        }
        if(kind == 2 || kind == 3)
        {
            destination[i] = source[i];
            continue;
        }
        if(kind != 1)
            continue;

        FstageNpcOverlayDecision decision = FSTAGE_NPC_OVERLAY_COPY;
        int16_t resolvedSelector = source[i].selector();
        if(state.overlayDecision != NULL)
            decision = state.overlayDecision(source[i],
                static_cast<uint32_t>(i), battleLayer, &resolvedSelector,
                state.userData);
        if(decision == FSTAGE_NPC_OVERLAY_COPY)
        {
            destination[i] = source[i];
            destination[i].values[1] = resolvedSelector;
        }
        else if(decision == FSTAGE_NPC_OVERLAY_CLEAR)
            clearNpcRow(destination[i]);
    }
}

bool loadNpcRoot(const FstageNpcPositionData& positions, int32_t root,
                 uint16_t stageId,
                 std::vector<FstageNpcPositionRow>& rows)
{
    rows.clear();
    return root >= 0 &&
        positions.selectRows(static_cast<uint32_t>(root), stageId, rows);
}

}

uint32_t fstageNpcRootForPhase(uint16_t phase)
{
    static const uint32_t roots[FSTAGE_NPC_PHASE_COUNT] =
    {
        0, 12, 13, 14, 15, 15, 16, 17, 18, 21, 23, 25
    };
    if(phase >= FSTAGE_NPC_PHASE_COUNT)
        return 0;
    return roots[phase];
}

uint32_t fstageNpcRootForSelection(const FstageNpcSelectionState& state)
{
    static const int16_t roots[FSTAGE_NPC_PHASE_COUNT][4] =
    {
        { 0, -1, -1, -1 },
        { 12, -1, -1, -1 },
        { 13, -1, -1, -1 },
        { 14, 14, -1, -1 },
        { 15, 15, -1, -1 },
        { 15, -1, -1, -1 },
        { 16, 17, 17, -1 },
        { 17, 18, 18, -1 },
        { 18, 19, 20, 20 },
        { 21, 22, 22, -1 },
        { 23, 4, 24, -1 },
        { 25, 26, 27, 4 }
    };
    if(state.storyPhase >= FSTAGE_NPC_PHASE_COUNT)
        return 0;

    const int16_t* phaseRoots = roots[state.storyPhase];
    int32_t root = phaseRoots[0];
    switch(state.storyPhase)
    {
    case 4:
        if(state.stageId != 0x33 && phaseRoots[1] >= 0)
            root = phaseRoots[1];
        break;
    case 5:
        if(npcRecruitment(state, 0x16) != 0)
            root = 0x1c;
        break;
    case 6:
        if(npcEvent(state, 0x0411) &&
           (state.stageId != 0x1a || !state.trackedOpponent) &&
           phaseRoots[1] >= 0)
            root = phaseRoots[1];
        if(npcEvent(state, 0x1f47) && phaseRoots[2] >= 0)
            root = phaseRoots[2];
        if(npcRecruitment(state, 0x16) != 0)
            root = 0x1c;
        break;
    case 7:
    {
        if(npcEvent(state, 0x1f7c) && phaseRoots[1] >= 0)
            root = phaseRoots[1];
        else if(state.actorResourceProfile == 0x35)
            root = 0x1e;
        if(state.recruitmentMode == 2 && state.stageId == 0x1e)
            root = 4;
        if(npcEvent(state, 0x1f48) && phaseRoots[2] >= 0)
            root = phaseRoots[2];
        const uint16_t ids[] = { 0x16, 0x35 };
        const int16_t recruitment = firstRecruitmentState(state, ids, 2);
        if(recruitment == 1)
            root = 0x1c;
        else if(recruitment == 2)
            root = npcEvent(state, 0x05f4) ? 4 : 0x1d;
        if(state.resetRecruitmentRoot)
            root = 0x1c;
        break;
    }
    case 8:
    {
        if(npcEvent(state, 0x1f49) && phaseRoots[1] >= 0)
            root = phaseRoots[1];
        if((npcEvent(state, 0x052c) || npcEvent(state, 0x03f7)) &&
           phaseRoots[2] >= 0)
            root = phaseRoots[2];
        if(npcEvent(state, 0x1f4a) && phaseRoots[3] >= 0)
            root = phaseRoots[3];
        const uint16_t ids[] = { 0x16, 0x6b, 0x6c, 0x35 };
        const int16_t recruitment = firstRecruitmentState(state, ids, 4);
        if(recruitment == 1)
            root = 0x1c;
        else if(recruitment == 2)
            root = npcEvent(state, 0x05f4) ? 4 : 0x1d;
        if(state.resetRecruitmentRoot)
            root = 0x1c;
        break;
    }
    case 9:
    {
        if(npcEvent(state, 0x077a) && state.stageId == 0x13 &&
           !npcEvent(state, 0x1f73))
            root = 4;
        if(npcEvent(state, 0x1f4b))
            root = state.stageId == 0x2d && phaseRoots[2] >= 0 ?
                phaseRoots[2] : phaseRoots[1];
        const uint16_t ids[] = { 0x16, 0x6b, 0x35 };
        const int16_t recruitment = firstRecruitmentState(state, ids, 3);
        if(recruitment == 1)
            root = 0x1c;
        else if(recruitment == 2)
            root = npcEvent(state, 0x05f4) ? 4 : 0x1d;
        break;
    }
    case 10:
    {
        if(npcEvent(state, 0x1f4c))
        {
            if(phaseRoots[2] >= 0)
                root = phaseRoots[2];
            if(state.stageId == 0x25 && !npcEvent(state, 0x1fa6) &&
               phaseRoots[1] >= 0)
                root = phaseRoots[1];
        }
        if(state.encounterSubstate && phaseRoots[1] >= 0)
            root = phaseRoots[1];
        const uint16_t ids[] = { 0x44, 0x6b, 0x35 };
        const int16_t recruitment = firstRecruitmentState(state, ids, 3);
        if(recruitment == 1)
            root = 0x1c;
        else if(recruitment == 2)
            root = npcEvent(state, 0x05f4) ? 4 : 0x1d;
        else if(recruitment == 3)
            root = 0;
        if(state.resetRecruitmentRoot)
            root = 0x1c;
        if(state.phase10Stage31Suppress && state.stageId == 0x1f)
            root = 0;
        if(npcEvent(state, 0x05f2) && state.stageId == 0x1f &&
           phaseRoots[2] >= 0)
            root = phaseRoots[2];
        break;
    }
    case 11:
        if(npcEvent(state, 0x1f4e) && phaseRoots[1] >= 0)
            root = phaseRoots[1];
        if(npcEvent(state, 0x1f50))
        {
            if(phaseRoots[2] >= 0)
                root = phaseRoots[2];
            if(state.stageId == 0x31 && state.phase11Stage49Override)
                root = 4;
        }
        if((state.phase11ModeFlags & 4) != 0 && phaseRoots[3] >= 0)
            root = phaseRoots[3];
        break;
    default:
        break;
    }

    if(state.stageId != 0x19 && state.storyTransitionMode != 0)
        root = 0;
    return root < 0 ? 0 : static_cast<uint32_t>(root);
}

bool selectFstageNpcPositionRecords(
    const FstageNpcPositionData& positions,
    const FstageNpcSelectionState& state,
    std::vector<FstageNpcPositionRow>& rows)
{
    rows.assign(FSTAGE_NPC_ROW_COUNT, FstageNpcPositionRow());
    if(state.stageId >= FSTAGE_NPC_STAGE_COUNT ||
       state.storyPhase >= FSTAGE_NPC_PHASE_COUNT)
        return false;
    if(state.gameStateBit1)
        return true;

    std::vector<FstageNpcPositionRow> layer;
    if(!loadNpcRoot(positions, 0, state.stageId, layer))
        return false;
    applyBaseNpcRows(layer, state, rows);

    int battleRoot = state.battleRoot;
    if(battleRoot < 0 || battleRoot > 3)
        battleRoot = 0;
    if(loadNpcRoot(positions, battleRoot, state.stageId, layer))
        applyOverlayNpcRows(layer, state, true, rows);

    if(state.suppressPostBattleOverlays)
        return true;

    int32_t partyRoot = -1;
    for(int i = 0; i < 2; ++i)
    {
        if(state.partyRoots[i] < 0)
            continue;
        if(partyRoot < 0 || state.stageId == 0x13)
            partyRoot = state.partyRoots[i];
    }
    if(state.suppressPartyOverlay)
        partyRoot = -1;
    if(state.stageId == 0x30 && state.partyRoots[0] == 0x1f &&
       !npcEvent(state, 0x073e))
        partyRoot = -1;

    static const int16_t phaseOverlayRoots[FSTAGE_NPC_PHASE_COUNT] =
    {
        0, 5, 5, 5, 5, 5, 6, 7, 8, 9, 10, 0
    };
    const int32_t storyRoot =
        static_cast<int32_t>(fstageNpcRootForSelection(state));

    if(state.alternateOverlayMode == 0)
    {
        if(loadNpcRoot(positions, phaseOverlayRoots[state.storyPhase],
                       state.stageId, layer))
            applyOverlayNpcRows(layer, state, false, rows);
        if(loadNpcRoot(positions, partyRoot, state.stageId, layer))
            applyOverlayNpcRows(layer, state, false, rows);
        if(state.storyPhase != 0 &&
           loadNpcRoot(positions, storyRoot, state.stageId, layer))
            applyOverlayNpcRows(layer, state, false, rows);
    }
    else
    {
        if((state.alternateOverlayMode == 7 || state.stageId == 0x19) &&
           state.storyPhase != 0 &&
           loadNpcRoot(positions, storyRoot, state.stageId, layer))
            applyOverlayNpcRows(layer, state, false, rows);
        if(loadNpcRoot(positions, partyRoot, state.stageId, layer))
            applyOverlayNpcRows(layer, state, false, rows);
    }
    return true;
}

bool getFstagePlayerSpawn(const FstageNpcPositionData& positions,
                          uint32_t stageIndex,
                          FstageSpawnTransform* transform)
{
    std::vector<FstageNpcPositionRow> rows;
    for(uint32_t root = 0; root < FSTAGE_NPC_ROOT_COUNT; ++root)
        if(positions.selectRows(root, stageIndex, rows) &&
           getFstagePlayerSpawn(rows, transform))
            return true;
    return false;
}

bool getFstagePlayerSpawn(const std::vector<FstageNpcPositionRow>& rows,
                          FstageSpawnTransform* transform)
{
    if(transform == NULL)
        return false;
    for(size_t i = 0; i < rows.size(); ++i)
        if(rows[i].kind() == 2)
            return getFstageSpawnTransform(rows[i], transform);
    return false;
}

bool getFstageSpawnTransform(const FstageNpcPositionRow& row,
                             FstageSpawnTransform* transform)
{
    if(transform == NULL)
        return false;
    if(row.kind() == 2 && row.selector() == 0)
    {
        transform->position.x = 2152.42f;
        transform->position.y = 76.64f;
        transform->position.z = -461.59f;
        transform->headingDegrees = 0.0f;
        return true;
    }
    transform->position.x = static_cast<float>(row.values[4]) * 0.1f;
    transform->position.y = static_cast<float>(row.values[5]) * 0.1f;
    transform->position.z = static_cast<float>(row.values[6]) * 0.1f;
    transform->headingDegrees = static_cast<float>(row.values[7]) * 0.1f;
    return true;
}

float fstageCharacterInteractionRadius(uint16_t characterId)
{
    if(characterId >= stage_object_data::CHARACTER_COUNT)
        return 0.0f;
    return floatFromBits(
        stage_object_data::CHARACTER_INTERACTION_RADIUS_BITS[characterId]);
}

int collectFstageNpcInteractionCandidates(
    const std::vector<FstageNpcPositionRow>& rows,
    const FstageNpcActorBinding* actors, int actorCount,
    const rw::V3d& playerPosition,
    FstageNpcInteractionCandidate* candidates, int capacity)
{
    if(candidates == NULL || capacity <= 0)
        return 0;
    int count = 0;
    const size_t rowCount = rows.size() < FSTAGE_NPC_ROW_COUNT ?
        rows.size() : FSTAGE_NPC_ROW_COUNT;
    for(size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        const FstageNpcPositionRow& row = rows[rowIndex];
        FstageNpcInteractionCandidate candidate;
        candidate.kind = row.kind();
        candidate.rowIndex = static_cast<int16_t>(rowIndex);
        candidate.interactionSelector = row.interactionSelector();
        if(row.kind() == 1)
        {
            const FstageNpcActorBinding* actor = NULL;
            for(int i = 0; i < actorCount; ++i)
            {
                if(actors != NULL && actors[i].active &&
                   actors[i].npcPositionRowIndex ==
                       static_cast<int16_t>(rowIndex))
                {
                    actor = &actors[i];
                    break;
                }
            }
            if(actor == NULL)
                continue;
            candidate.actorSlot = actor->actorSlot;
            candidate.characterId = actor->characterId;
            candidate.position = actor->position;
            candidate.radius =
                fstageCharacterInteractionRadius(actor->characterId);
        }
        else if(row.kind() == 3)
        {
            FstageSpawnTransform transform;
            getFstageSpawnTransform(row, &transform);
            candidate.position = transform.position;
            candidate.radius = 10.0f;
        }
        else
            continue;

        const float dx = candidate.position.x - playerPosition.x;
        const float dy = candidate.position.y - playerPosition.y;
        const float dz = candidate.position.z - playerPosition.z;
        if(fabsf(dy) >= 20.0f)
            continue;
        candidate.distanceSquared = dx * dx + dz * dz;
        if(candidate.distanceSquared > candidate.radius * candidate.radius)
            continue;

        int insertion = count;
        if(insertion > capacity - 1)
            insertion = capacity - 1;
        while(insertion > 0 &&
              candidates[insertion - 1].distanceSquared >
                  candidate.distanceSquared)
        {
            if(insertion < capacity)
                candidates[insertion] = candidates[insertion - 1];
            --insertion;
        }
        if(insertion < capacity)
            candidates[insertion] = candidate;
        if(count < capacity)
            ++count;
    }
    return count;
}

bool isFstageObjectSuppressed(uint16_t stageId, uint32_t objectIndex)
{
    if(stageId == 0x1a)
        return objectIndex == 0x0d;
    if(stageId == 0x19)
        return objectIndex == 4 || objectIndex == 0x20;
    if(stageId == 0x0d)
        return objectIndex == 0x0e;
    if(stageId == 4)
        return objectIndex == 0x0b || objectIndex == 0x0c;
    return stageId == 1 && objectIndex == 0x0c;
}

FstageScene::FstageScene()
    : textureDictionary(NULL), clump(NULL), worldClump(NULL),
      objectModelResolver(NULL), objectModelContext(NULL),
      hierarchy(NULL), activeAnimation(0), objectAnimationBase(0),
      world(NULL), radius(0.0f), haveBounds(false),
      profileStageId(0xffff),
      collisionProfileReady(false), authoredWorldVertexCount(0),
      authoredWorldTriangleCount(0)
{
    center.x = center.y = center.z = 0.0f;
    minBounds.x = minBounds.y = minBounds.z = 0.0f;
    maxBounds.x = maxBounds.y = maxBounds.z = 0.0f;
    for(size_t i = 0; i < 4; ++i)
        sectionClumps[i] = NULL;
}

bool FstageScene::hasBounds() const
{
    return haveBounds;
}

const std::vector<FstageObjectBounds>& FstageScene::objects() const
{
    return objectBounds;
}

const rw::V3d& FstageScene::boundsMinimum() const
{
    return minBounds;
}

const rw::V3d& FstageScene::boundsMaximum() const
{
    return maxBounds;
}

const std::vector<FstagePlacement>& FstageScene::placementRecords() const
{
    return placements;
}

const RwWorldGeometry& FstageScene::section2Collision() const
{
    return collisionGeometry;
}

const RwWorldGeometry& FstageScene::cameraStaticCollision() const
{
    return cameraStaticCollisionGeometry;
}

const std::vector<FstageObjectInstance>&
FstageScene::objectInstances() const
{
    return stageObjects;
}

const FstageWeaponManager& FstageScene::weaponManager() const
{
    return stageWeapons;
}

const std::vector<FstageInteractionMarker>&
FstageScene::markerRecords() const
{
    return interactionMarkers;
}

bool FstageScene::containsNavigationPosition(const rw::V3d& position,
                                             float margin) const
{
    if(!haveBounds)
        return false;
    if(margin < 0.0f)
        margin = 0.0f;
    float lowerX = minBounds.x + margin;
    float lowerY = minBounds.y + margin;
    float lowerZ = minBounds.z + margin;
    float upperX = maxBounds.x - margin;
    float upperY = maxBounds.y - margin;
    float upperZ = maxBounds.z - margin;
    if(lowerX > upperX)
        lowerX = upperX = (minBounds.x + maxBounds.x) * 0.5f;
    if(lowerY > upperY)
        lowerY = upperY = (minBounds.y + maxBounds.y) * 0.5f;
    if(lowerZ > upperZ)
        lowerZ = upperZ = (minBounds.z + maxBounds.z) * 0.5f;
    return position.x >= lowerX && position.x <= upperX &&
           position.y >= lowerY && position.y <= upperY &&
           position.z >= lowerZ && position.z <= upperZ;
}

void FstageScene::clampNavigationPosition(rw::V3d* position,
                                           float margin) const
{
    if(position == NULL || !haveBounds)
        return;
    if(margin < 0.0f)
        margin = 0.0f;
    float lowerX = minBounds.x + margin;
    float lowerY = minBounds.y + margin;
    float lowerZ = minBounds.z + margin;
    float upperX = maxBounds.x - margin;
    float upperY = maxBounds.y - margin;
    float upperZ = maxBounds.z - margin;
    if(lowerX > upperX)
        lowerX = upperX = (minBounds.x + maxBounds.x) * 0.5f;
    if(lowerY > upperY)
        lowerY = upperY = (minBounds.y + maxBounds.y) * 0.5f;
    if(lowerZ > upperZ)
        lowerZ = upperZ = (minBounds.z + maxBounds.z) * 0.5f;
    if(position->x < lowerX)
        position->x = lowerX;
    else if(position->x > upperX)
        position->x = upperX;
    if(position->y < lowerY)
        position->y = lowerY;
    else if(position->y > upperY)
        position->y = upperY;
    if(position->z < lowerZ)
        position->z = lowerZ;
    else if(position->z > upperZ)
        position->z = upperZ;
}

namespace
{

rw::Atomic* firstClumpAtomic(rw::Clump* clump)
{
    if(clump == NULL)
        return NULL;
    FORLIST(link, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(link);
        if(atomic != NULL && atomic->geometry != NULL)
            return atomic;
    }
    return NULL;
}

void recalculateGeometryBounds(RwWorldGeometry& geometry)
{
    geometry.haveBounds = false;
    geometry.minBounds = RwWorldVertex();
    geometry.maxBounds = RwWorldVertex();
    for(size_t i = 0; i < geometry.vertices.size(); ++i)
    {
        const RwWorldVertex& vertex = geometry.vertices[i];
        if(!geometry.haveBounds)
        {
            geometry.minBounds = geometry.maxBounds = vertex;
            geometry.haveBounds = true;
            continue;
        }
        if(vertex.x < geometry.minBounds.x)
            geometry.minBounds.x = vertex.x;
        if(vertex.y < geometry.minBounds.y)
            geometry.minBounds.y = vertex.y;
        if(vertex.z < geometry.minBounds.z)
            geometry.minBounds.z = vertex.z;
        if(vertex.x > geometry.maxBounds.x)
            geometry.maxBounds.x = vertex.x;
        if(vertex.y > geometry.maxBounds.y)
            geometry.maxBounds.y = vertex.y;
        if(vertex.z > geometry.maxBounds.z)
            geometry.maxBounds.z = vertex.z;
    }
}

bool stageObjectEventSuppressed(const stage_object_data::Record& record,
                                const FstageObjectSpawnState& state)
{
    return record.eventFlag > 5999 && record.eventFlag < 7500 &&
        state.eventFlag != NULL &&
        state.eventFlag(static_cast<uint16_t>(record.eventFlag),
                        state.userData);
}

int stageObjectPool(const stage_object_data::Stage& stage,
                    const stage_object_data::Record& record)
{
    if(record.kind == 1)
    {
        if(record.sub == 1)
            return stage.pools[3];
        if(record.sub == 2)
            return stage.pools[2];
        if(record.sub == 3)
            return stage.pools[0];
        return -1;
    }
    if(record.kind == 2 || record.kind == 3)
        return stage.pools[1];
    return -1;
}

void setStageObjectAlpha(FstageObjectInstance& instance, uint8_t alpha)
{
    instance.alpha = alpha;
    instance.renderControl |= 8;
    if(instance.clump == NULL)
        return;
    FORLIST(link, instance.clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(link);
        if(atomic == NULL || atomic->geometry == NULL)
            continue;
        for(int32_t i = 0;
            i < atomic->geometry->matList.numMaterials; ++i)
        {
            rw::Material* material =
                atomic->geometry->matList.materials[i];
            if(material != NULL)
                material->color.alpha = alpha;
        }
        if(alpha == 0)
            atomic->setFlags(atomic->getFlags() & ~rw::Atomic::RENDER);
    }
}

void rebuildFstageObjectGeometry(FstageScene* scene)
{
    if(scene == NULL)
        return;
    if(scene->authoredWorldVertexCount <= scene->worldGeometry.vertices.size())
        scene->worldGeometry.vertices.resize(scene->authoredWorldVertexCount);
    if(scene->authoredWorldTriangleCount <=
       scene->worldGeometry.triangles.size())
        scene->worldGeometry.triangles.resize(
            scene->authoredWorldTriangleCount);
    for(size_t i = 0; i < scene->stageObjects.size(); ++i)
    {
        if(scene->stageObjects[i].registeredCollider)
            appendClumpToWorldGeometry(
                &scene->worldGeometry, scene->stageObjects[i].clump);
    }
    recalculateGeometryBounds(scene->worldGeometry);
    if(scene->collisionProfileReady)
        buildFstageCollisionGeometry(scene, scene->profileStageId);
}

bool replaceFstageObject(FstageScene* scene, size_t index,
                         int16_t pool, FstageObjectPhase phase)
{
    if(scene == NULL || index >= scene->stageObjects.size() || pool <= 0 ||
       static_cast<size_t>(pool) > scene->objectTemplates.size())
        return false;
    FstageObjectInstance old = scene->stageObjects[index];
    if(old.clump == NULL || old.clump->getFrame() == NULL)
        return false;
    old.clump->getFrame()->updateObjects();
    const rw::Matrix world = *old.clump->getFrame()->getLTM();
    rw::Clump* replacement = scene->objectTemplates[pool - 1]->clone();
    if(replacement == NULL || replacement->getFrame() == NULL)
    {
        if(replacement != NULL)
            replacement->destroy();
        return false;
    }
    replacement->getFrame()->transform(&world, rw::COMBINEREPLACE);
    prepareStageClump(scene, replacement);
    scene->world->addClump(replacement);

    FstageObjectInstance next;
    next.clump = replacement;
    next.pool = pool;
    next.eventFlag = old.eventFlag;
    next.stageId = old.stageId;
    next.recordIndex = old.recordIndex;
    next.kind = old.kind;
    next.sub = old.sub;
    next.gateValue = old.gateValue;
    next.gateAuxiliary = old.gateAuxiliary;
    next.runtimeFlags = 2;
    next.phase = phase;
    next.hierarchy = phase == FSTAGE_OBJECT_BREAK_ANIMATION ?
        rw::HAnimHierarchy::get(replacement) : NULL;

    if(scene->world != NULL)
        scene->world->removeClump(old.clump);
    old.clump->destroy();
    scene->stageObjects[index] = next;
    rebuildFstageObjectGeometry(scene);
    return true;
}

void destroyFstageObject(FstageScene* scene, size_t index)
{
    if(scene == NULL || index >= scene->stageObjects.size())
        return;
    rw::Clump* clump = scene->stageObjects[index].clump;
    if(clump != NULL)
    {
        if(scene->world != NULL)
            scene->world->removeClump(clump);
        clump->destroy();
    }
    scene->stageObjects.erase(scene->stageObjects.begin() + index);
    rebuildFstageObjectGeometry(scene);
}

void initializeFstageBreakAnimation(FstageScene* scene,
                                    FstageObjectInstance& instance)
{
    if(instance.animationInitialized)
        return;
    instance.animationInitialized = true;
    instance.state |= 0x20;
    if(instance.hierarchy == NULL ||
       scene->objectAnimationBase >= scene->animations.size() ||
       instance.hierarchy->interpolator == NULL)
        return;
    instance.hierarchy->flags |=
        rw::HAnimHierarchy::UPDATEMODELLINGMATRICES |
        rw::HAnimHierarchy::UPDATELTMS;
    instance.hierarchy->attach();
    rw::Animation* animation =
        scene->animations[scene->objectAnimationBase];
    instance.hierarchy->interpolator->setCurrentAnim(animation);
    if(animation->duration <= 10.0f)
    {
        instance.hierarchy->interpolator->addTime(animation->duration);
        instance.state |= 0x10;
    }
    else
        instance.hierarchy->interpolator->addTime(10.0f);
    instance.hierarchy->updateMatrices();
}

bool fstageObjectContactAccepted(const FstageObjectInstance& instance,
                                 const FstageObjectContact& contact)
{
    if(contact.damage <= 0 || contact.radius < 0.0f ||
       instance.phase != FSTAGE_OBJECT_DESTRUCTIBLE ||
       !instance.registeredCollider || (instance.runtimeFlags & 2) != 0 ||
       instance.definitionSubtype == 0x17)
        return false;
    RwWorldVertex point;
    point.x = contact.center.x;
    point.y = contact.center.y;
    point.z = contact.center.z;
    return rwWorldCapsuleIntersects(
        instance.colliderGeometry, point, point, contact.radius);
}

void spawnFstageObjectDrop(FstageScene* scene, size_t index,
                           const rw::Matrix& sourceWorld)
{

    const FstageObjectInstance& source = scene->stageObjects[index];
    if((source.gateValue != 2 && source.gateValue != 3) ||
       scene->objectModelResolver == NULL)
        return;
    const stage_object_data::Stage* const stage =
        recoveredStage(source.stageId);
    if(stage == NULL || stage->pools[1] < 0)
        return;
    rw::Clump* const model = scene->objectModelResolver(
        source.gateValue, source.gateAuxiliary, scene->objectModelContext);
    if(model == NULL)
        return;
    rw::Clump* const clump = model->clone();
    if(clump == NULL || clump->getFrame() == NULL)
    {
        if(clump != NULL)
            clump->destroy();
        return;
    }
    rw::Matrix world = sourceWorld;
    if(source.gateValue == 2 && source.gateAuxiliary == 8)
        world.pos.y += 2.0f;
    clump->getFrame()->transform(&world, rw::COMBINEREPLACE);
    prepareStageClump(scene, clump);
    if(scene->world != NULL)
        scene->world->addClump(clump);
    FstageObjectInstance item;
    item.clump = clump;
    item.pool = stage->pools[1];
    item.eventFlag = -1;
    item.stageId = source.stageId;
    item.recordIndex = source.recordIndex;
    item.kind = source.gateValue;
    item.sub = source.gateAuxiliary;
    scene->stageObjects.push_back(item);
}

void runFstageDestructionScript(FstageScene* scene, size_t index)
{
    if(scene == NULL || index >= scene->stageObjects.size())
        return;
    const FstageObjectInstance& instance = scene->stageObjects[index];
    if(instance.eventFlag > 5999 && instance.eventFlag < 7500 &&
       scene->objectHooks.eventSet != NULL)
    {
        scene->objectHooks.eventSet(
            static_cast<uint16_t>(instance.eventFlag),
            scene->objectHooks.userData);
    }

    rw::Matrix effectWorld;
    effectWorld.setIdentity();
    if(instance.clump != NULL && instance.clump->getFrame() != NULL)
    {
        instance.clump->getFrame()->updateObjects();
        effectWorld = *instance.clump->getFrame()->getLTM();
    }
    const int16_t replacementPool =
        static_cast<int16_t>(instance.pool + 1);
    if(!replaceFstageObject(
           scene, index, replacementPool,
           FSTAGE_OBJECT_BREAK_ANIMATION))
        return;
    if(scene->objectHooks.sound != NULL)
        scene->objectHooks.sound(
            4, 0, 30, scene->objectHooks.userData);
    const rw::Matrix dropWorld = effectWorld;
    effectWorld.pos.y += 5.0f;
    if(scene->objectHooks.effect != NULL)
        scene->objectHooks.effect(
            91, effectWorld, 0, 0, scene->objectHooks.userData);
    spawnFstageObjectDrop(scene, index, dropWorld);
}

const FstageWeaponDescriptor* fstageWeaponDescriptor(uint16_t selector)
{
    for(size_t i = 0;
        i < sizeof(FSTAGE_WEAPON_DESCRIPTORS) /
            sizeof(FSTAGE_WEAPON_DESCRIPTORS[0]); ++i)
    {
        if(FSTAGE_WEAPON_DESCRIPTORS[i].selector == selector)
            return &FSTAGE_WEAPON_DESCRIPTORS[i];
    }
    return NULL;
}

bool fstageWeaponHasBrokenPair(uint16_t selector)
{
    switch(selector)
    {
    case 8: case 10: case 13: case 14: case 15: case 16:
    case 17: case 18: case 19: case 20: case 21: case 37:
        return true;
    default:
        return false;
    }
}

void setFstageWeaponRendered(rw::Clump* clump, bool rendered)
{
    if(clump == NULL)
        return;
    FORLIST(link, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(link);
        if(atomic == NULL)
            continue;
        uint32_t flags = atomic->getFlags();
        if(rendered)
            flags |= rw::Atomic::RENDER;
        else
            flags &= ~rw::Atomic::RENDER;
        atomic->setFlags(flags);
    }
}

int16_t allocateFstageWeaponManager(FstageScene* scene, uint16_t selector,
                                    rw::Clump* clump, uint16_t flags)
{
    for(int16_t i = 0; i < FSTAGE_WEAPON_CAPACITY; ++i)
    {
        if(scene->stageWeapons.records[i].managerId !=
           FSTAGE_WEAPON_INVALID_ID)
            continue;
        FstageWeaponManagerRecord record;
        record.selector = selector;
        record.flags = flags;
        record.managerId = i;
        record.clump = clump;
        scene->stageWeapons.records[i] = record;
        ++scene->stageWeapons.managerCount;
        return i;
    }
    return FSTAGE_WEAPON_INVALID_ID;
}

int16_t allocateFstageWeaponInstance(FstageScene* scene)
{
    for(int16_t i = 0; i < FSTAGE_WEAPON_CAPACITY; ++i)
    {
        if(!scene->stageWeapons.instances[i].active)
            return i;
    }
    return FSTAGE_WEAPON_INVALID_ID;
}

bool validFstageManagerId(const FstageScene* scene, int16_t managerId)
{
    return scene != NULL && managerId >= 0 &&
        managerId < FSTAGE_WEAPON_CAPACITY &&
        scene->stageWeapons.records[managerId].managerId == managerId;
}

rw::Frame* fstageWeaponFrame(FstageScene* scene,
                             const FstageWeaponInstance& weapon)
{
    if(!validFstageManagerId(scene, weapon.managerId))
        return NULL;
    rw::Clump* clump =
        scene->stageWeapons.records[weapon.managerId].clump;
    return clump == NULL ? NULL : clump->getFrame();
}

void releaseFstageManagerRecord(FstageScene* scene, int16_t managerId)
{
    if(!validFstageManagerId(scene, managerId))
        return;
    FstageWeaponManagerRecord& record =
        scene->stageWeapons.records[managerId];
    if(record.clump != NULL)
    {
        rw::Frame* frame = record.clump->getFrame();
        if(frame != NULL && frame->getParent() != NULL)
            frame->removeChild();
        if(scene->world != NULL)
            scene->world->removeClump(record.clump);
        record.clump->destroy();
    }
    record = FstageWeaponManagerRecord();
    if(scene->stageWeapons.managerCount > 0)
        --scene->stageWeapons.managerCount;
}

bool spawnFstageWeapon(FstageScene* scene, uint16_t stageId,
                       uint16_t recordIndex, uint16_t selector,
                       const rw::Matrix& world,
                       const FstageObjectSpawnState& state,
                       int16_t* spawnedId = NULL)
{
    if(scene == NULL || scene->world == NULL || state.modelResolver == NULL)
        return false;
    rw::Clump* source =
        state.modelResolver(4, static_cast<int8_t>(selector), state.userData);
    rw::Clump* clump = source == NULL ? NULL : source->clone();
    if(clump == NULL || clump->getFrame() == NULL)
    {
        if(clump != NULL)
            clump->destroy();
        return false;
    }
    clump->getFrame()->transform(&world, rw::COMBINEREPLACE);
    prepareStageClump(scene, clump);
    scene->world->addClump(clump);
    const int16_t managerId = allocateFstageWeaponManager(
        scene, selector, clump,
        FSTAGE_WEAPON_MANAGER_ACTIVE | FSTAGE_WEAPON_MANAGER_UPDATE);
    const int16_t weaponId = allocateFstageWeaponInstance(scene);
    if(managerId < 0 || weaponId < 0)
    {
        if(managerId >= 0)
            releaseFstageManagerRecord(scene, managerId);
        else
        {
            scene->world->removeClump(clump);
            clump->destroy();
        }
        return false;
    }

    if(fstageWeaponHasBrokenPair(selector))
    {
        rw::Clump* brokenSource =
            state.modelResolver(4, 0x29, state.userData);
        rw::Clump* broken =
            brokenSource == NULL ? NULL : brokenSource->clone();
        if(broken == NULL || broken->getFrame() == NULL)
        {
            if(broken != NULL)
                broken->destroy();
            releaseFstageManagerRecord(scene, managerId);
            return false;
        }
        rw::Matrix brokenWorld = world;
        brokenWorld.pos.y += 10.0f;
        broken->getFrame()->transform(
            &brokenWorld, rw::COMBINEREPLACE);
        prepareStageClump(scene, broken);
        setFstageWeaponRendered(broken, false);
        scene->world->addClump(broken);
        const int16_t linked = allocateFstageWeaponManager(
            scene, 0x29, broken, FSTAGE_WEAPON_MANAGER_UPDATE);
        if(linked < 0)
        {
            scene->world->removeClump(broken);
            broken->destroy();
            releaseFstageManagerRecord(scene, managerId);
            return false;
        }
        scene->stageWeapons.records[managerId].linkedManagerId = linked;
    }

    FstageWeaponInstance weapon;
    weapon.active = true;
    weapon.stageId = stageId;
    weapon.recordIndex = recordIndex;
    weapon.selector = selector;
    weapon.primaryManagerId = managerId;
    weapon.managerId = managerId;
    weapon.groundY = world.pos.y;
    weapon.previousWorld = world;
    scene->stageWeapons.instances[weaponId] = weapon;
    ++scene->stageWeapons.weaponCount;
    if(spawnedId != NULL)
        *spawnedId = weaponId;
    return true;
}

}

int16_t cloneFstageWeaponInstance(FstageScene* scene, uint16_t selector,
                                  const rw::Matrix& transform)
{
    if(scene == NULL)
        return FSTAGE_WEAPON_INVALID_ID;
    FstageObjectSpawnState state;
    state.modelResolver = scene->objectModelResolver;
    state.userData = scene->objectModelContext;
    int16_t id = FSTAGE_WEAPON_INVALID_ID;
    spawnFstageWeapon(scene, scene->profileStageId, 0xffff, selector,
                       transform, state, &id);
    return id;
}

rw::Frame* findFstageWeaponFrame(FstageScene* scene, int16_t weaponId)
{
    if(scene == NULL || weaponId < 0 || weaponId >= FSTAGE_WEAPON_CAPACITY)
        return NULL;
    const FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    return weapon.active ? fstageWeaponFrame(scene, weapon) : NULL;
}

rw::Matrix detachFstageWeaponFrame(rw::Frame* frame)
{
    rw::Matrix world;
    world.setIdentity();
    if(frame == NULL)
        return world;
    frame->updateObjects();
    world = *frame->getLTM();
    if(frame->getParent() != NULL)
        frame->removeChild();
    frame->transform(&world, rw::COMBINEREPLACE);
    return world;
}

namespace
{

void applyFstageWeaponAttachment(rw::Frame* frame,
                                 const uint32_t* position,
                                 const uint32_t* rotation,
                                 uint8_t slot)
{
    if(frame == NULL || slot > 1)
        return;
    const size_t offset = static_cast<size_t>(slot) * 3;
    rw::Matrix transform;
    rw::V3d axis;
    axis.x = 1.0f; axis.y = axis.z = 0.0f;
    transform.rotate(&axis, floatFromBits(rotation[offset]),
                     rw::COMBINEREPLACE);
    axis.y = 1.0f; axis.x = axis.z = 0.0f;
    transform.rotate(&axis, floatFromBits(rotation[offset + 1]),
                     rw::COMBINEPOSTCONCAT);
    axis.z = 1.0f; axis.x = axis.y = 0.0f;
    transform.rotate(&axis, floatFromBits(rotation[offset + 2]),
                     rw::COMBINEPOSTCONCAT);
    transform.pos.x = floatFromBits(position[offset]);
    transform.pos.y = floatFromBits(position[offset + 1]);
    transform.pos.z = floatFromBits(position[offset + 2]);
    transform.optimize();
    frame->transform(&transform, rw::COMBINEREPLACE);
}

void flattenFstageWeapon(rw::Frame* frame, float y)
{
    if(frame == NULL)
        return;
    frame->updateObjects();
    const rw::Matrix current = *frame->getLTM();
    const float yaw = atan2f(current.at.x, current.at.z) *
        57.29577951308232f;
    rw::Matrix flat;
    rw::V3d axisY;
    axisY.x = axisY.z = 0.0f;
    axisY.y = 1.0f;
    flat.rotate(&axisY, yaw, rw::COMBINEREPLACE);
    flat.pos = current.pos;
    flat.pos.y = y;
    flat.optimize();
    frame->transform(&flat, rw::COMBINEREPLACE);
}

}

void fstageWeaponAngularDelta(const rw::Matrix& current,
                              const rw::Matrix& previous,
                              rw::V3d& angular)
{
    const float currentYaw = atan2f(current.at.x, current.at.z) *
        57.29577951308232f;
    const float previousYaw = atan2f(previous.at.x, previous.at.z) *
        57.29577951308232f;
    const float currentPitch = atan2f(current.at.y, current.at.z) *
        57.29577951308232f;
    const float previousPitch = atan2f(previous.at.y, previous.at.z) *
        57.29577951308232f;
    angular.x = currentPitch - previousPitch;
    angular.y = currentYaw - previousYaw;
    angular.z = 0.0f;
}

namespace
{

int markerEventFlag(uint16_t stageId, int16_t markerKey)
{
    const stage_object_data::Stage* stage = recoveredStage(stageId);
    if(stage == NULL)
        return -1;
    if(stageId == 0x0d && markerKey == -0x1ce)
        return 0x03a3;
    for(uint16_t i = 0; i < stage->markerKeyCount; ++i)
    {
        if(stage_object_data::MARKER_KEYS[stage->firstMarkerKey + i] ==
           markerKey)
            return stage->markerEventFlagBase + i;
    }
    return -1;
}

int consumedMarkerText(int eventFlag)
{
    switch(eventFlag)
    {
    case 800: case 0x321: case 0x323: case 0x325: case 0x326:
    case 0x32b: case 0x32c: case 0x336: case 0x33d: case 0x33e:
    case 0x33f: case 0x340: case 0x342: case 0x343: case 0x344:
    case 0x348: case 0x34e: case 0x34f: case 0x350: case 0x351:
    case 0x352: case 0x354: case 0x355: case 0x358: case 0x35e:
    case 0x360: case 0x361: case 0x362: case 0x363: case 0x364:
    case 0x365: case 0x366: case 0x367: case 0x36b: case 0x36c:
    case 0x36d: case 0x370: case 0x373: case 0x375: case 0x376:
    case 0x377: case 0x37a: case 0x37b: case 0x37c: case 0x391:
    case 0x392: case 0x3a3:
        return 0x76f8;
    case 0x322: case 0x337: case 0x341: case 0x35f: case 0x369:
    case 0x36a: case 0x36f: case 0x371: case 0x372: case 0x378:
    case 0x37f: case 0x380: case 0x381: case 0x382: case 899:
    case 900: case 0x385: case 0x386: case 0x38c: case 0x38d:
    case 0x38e: case 0x397: case 0x399: case 0x39a:
        return 0x76f3;
    case 0x324:
        return 0x76fd;
    case 0x327: case 0x328: case 0x329: case 0x32a: case 0x32d:
    case 0x32e: case 0x32f: case 0x338: case 0x339: case 0x33a:
    case 0x33c: case 0x349: case 0x34a: case 0x34b: case 0x34d:
    case 0x356: case 0x35a: case 0x35b: case 0x35d: case 0x374:
    case 0x387: case 0x388: case 0x389: case 0x38f:
        return 0x76f9;
    case 0x330: case 0x332: case 0x333: case 0x334: case 0x353:
    case 0x357:
        return 0x76f4;
    case 0x335:
        return 0x76fe;
    case 0x345:
        return 0x76ff;
    case 0x346:
        return 0x7701;
    case 0x347:
        return 0x7700;
    case 0x359:
        return 0x76f7;
    case 0x368: case 0x39b:
        return 0x76f6;
    case 0x390: case 0x395: case 0x396: case 0x398:
        return 0x76f2;
    case 0x393: case 0x394: case 0x39c: case 0x39d: case 0x39e:
    case 0x39f:
        return 0x76fa;
    default:
        return -1;
    }
}

}

bool buildFstageCollisionGeometry(FstageScene* scene, uint16_t stageId)
{
    if(scene == NULL || scene->worldClump == NULL)
        return false;

    scene->combatSurfaceGeometry = RwWorldGeometry();
    appendAtomicToWorldGeometry(&scene->combatSurfaceGeometry,
                                firstClumpAtomic(scene->sectionClumps[0]));
    scene->collisionGeometry = scene->combatSurfaceGeometry;
    for(size_t i = 0; i < scene->placements.size(); ++i)
    {
        uint8_t type = 0;
        if(scene->placements[i].clump != NULL &&
           recoveredDefinition(stageId,
               static_cast<uint32_t>(scene->placements[i].objectIndex),
               &type, NULL) && type != 0)
            appendAtomicToWorldGeometry(&scene->collisionGeometry,
                firstClumpAtomic(scene->placements[i].clump));
    }
    for(size_t i = 0; i < scene->stageObjects.size(); ++i)
    {
        if(scene->stageObjects[i].registeredCollider)
            appendAtomicToWorldGeometry(&scene->collisionGeometry,
                firstClumpAtomic(scene->stageObjects[i].clump));
    }
    scene->profileStageId = stageId;
    scene->collisionProfileReady = true;
    return !scene->collisionGeometry.triangles.empty();
}

void clearFstageStageObjects(FstageScene* scene)
{
    if(scene == NULL)
        return;
    for(size_t i = 0; i < scene->stageObjects.size(); ++i)
    {
        rw::Clump* clump = scene->stageObjects[i].clump;
        if(clump == NULL)
            continue;
        if(scene->world != NULL)
            scene->world->removeClump(clump);
        clump->destroy();
    }
    scene->stageObjects.clear();
    for(int16_t i = 0; i < FSTAGE_WEAPON_CAPACITY; ++i)
        releaseFstageManagerRecord(scene, i);
    scene->stageWeapons = FstageWeaponManager();
    scene->objectHooks = FstageObjectRuntimeHooks();
    scene->objectModelResolver = NULL;
    scene->objectModelContext = NULL;
    rebuildFstageObjectGeometry(scene);
}

bool installFstageStageObjects(
    FstageScene* scene, uint16_t stageId,
    const FstageObjectSpawnState& state,
    FstageObjectInstallResult* result)
{
    FstageObjectInstallResult localResult;
    if(result == NULL)
        result = &localResult;
    *result = FstageObjectInstallResult();
    const stage_object_data::Stage* stage = recoveredStage(stageId);
    if(scene == NULL || stage == NULL || scene->world == NULL)
        return false;

    clearFstageStageObjects(scene);
    scene->objectHooks = state.runtime;
    scene->objectModelResolver = state.modelResolver;
    scene->objectModelContext = state.userData;
    buildFstageCollisionGeometry(scene, stageId);
    result->authored = stage->recordCount;
    int gateIndex = state.gameStateBit1 ? 11 :
        static_cast<int>(state.storyPhase) - 1;
    if(gateIndex < 0)
        gateIndex = 0;
    if(gateIndex > 10 && !state.gameStateBit1)
        gateIndex = 10;

    for(uint16_t recordIndex = 0; recordIndex < stage->recordCount;
        ++recordIndex)
    {
        const stage_object_data::Record& record =
            stage_object_data::RECORDS[stage->firstRecord + recordIndex];
        const stage_object_data::Gate& gate = record.gates[gateIndex];
        if(gate.value < 0 || stageObjectEventSuppressed(record, state))
            continue;

        const int pool = stageObjectPool(*stage, record);
        rw::Clump* source = NULL;
        float y = static_cast<float>(record.position[1]) * 0.1f;
        float ground = y;
        const float x = static_cast<float>(record.position[0]) * 0.1f;
        const float z = static_cast<float>(record.position[2]) * 0.1f;
        if(record.kind == 1)
        {
            if((state.enabledKindMask & 1) == 0 || pool <= 0 ||
               static_cast<size_t>(pool) > scene->objectTemplates.size())
                continue;
            source = scene->objectTemplates[pool - 1];
            uint8_t type = 0;
            uint16_t flags = 0;
            if(stageId == 1 && pool == 32)
            {
                type = 0;
                flags = 0;
            }
            else
                recoveredDefinition(stageId, pool, &type, &flags);
            if((flags & 0x40) != 0)
            {
                float floor = ground;
                if(rwWorldFloorNearHeight(
                       scene->worldGeometry, x, z, y, &floor) &&
                   static_cast<size_t>(pool) <=
                       scene->objectBounds.size())
                    y = floor -
                        scene->objectBounds[pool - 1].minBounds.y;
            }
            ++result->eligible;
        }
        else if(record.kind == 2 || record.kind == 3)
        {
            const uint8_t mask = record.kind == 2 ? 2 : 4;
            if((state.enabledKindMask & mask) == 0 || pool <= 0)
                continue;
            if(!rwWorldFloorNearHeight(
                   scene->worldGeometry, x, z, y, &ground))
                continue;
            ++result->eligible;
            if(state.modelResolver != NULL)
                source = state.modelResolver(record.kind, record.sub,
                                             state.userData);
            if(source == NULL)
            {
                ++result->missingModel;
                continue;
            }
            y = record.kind == 2 ? ground :
                ground + recoveredHeightOffset(record.sub) + 0.3f;
        }
        else if(record.kind == 4)
        {
            if(!rwWorldFloorNearHeight(
                   scene->worldGeometry, x, z, y, &ground))
                continue;
            ++result->eligible;
            y = ground + recoveredHeightOffset(record.sub) + 0.3f;
        }
        else
            continue;

        rw::Matrix matrix;
        rw::V3d axisY;
        axisY.x = axisY.z = 0.0f;
        axisY.y = 1.0f;
        matrix.rotate(&axisY,
            static_cast<float>(record.headingTenths) * 0.1f,
            rw::COMBINEREPLACE);
        matrix.pos.x = x;
        matrix.pos.y = y;
        matrix.pos.z = z;
        matrix.optimize();

        if(record.kind == 4)
        {
            if(!spawnFstageWeapon(
                   scene, stageId, recordIndex,
                   static_cast<uint16_t>(
                       static_cast<uint8_t>(record.sub)),
                   matrix, state))
            {
                ++result->missingModel;
                continue;
            }
            ++result->instantiated;
            continue;
        }

        rw::Clump* clump = source->clone();
        if(clump == NULL || clump->getFrame() == NULL)
        {
            if(clump != NULL)
                clump->destroy();
            ++result->missingModel;
            continue;
        }
        clump->getFrame()->transform(&matrix, rw::COMBINEREPLACE);
        prepareStageClump(scene, clump);
        scene->world->addClump(clump);

        FstageObjectInstance instance;
        instance.clump = clump;
        instance.pool = static_cast<int16_t>(pool);
        instance.eventFlag = record.eventFlag;
        instance.stageId = stageId;
        instance.recordIndex = recordIndex;
        instance.kind = record.kind;
        instance.sub = record.sub;
        instance.gateValue = gate.value;
        instance.gateAuxiliary = gate.auxiliary;
        uint8_t definitionType = 0;
        instance.registeredCollider =
            record.kind == 1 && !(stageId == 1 && pool == 32) &&
            recoveredDefinition(stageId, static_cast<uint32_t>(pool),
                                &definitionType, NULL) &&
            definitionType != 0;
        instance.health = definitionType == 2 ? 1 : 0;
        instance.phase = definitionType == 2 ?
            FSTAGE_OBJECT_DESTRUCTIBLE : FSTAGE_OBJECT_INERT;
        if(instance.registeredCollider)
            appendAtomicToWorldGeometry(
                &instance.colliderGeometry, firstClumpAtomic(clump));
        scene->stageObjects.push_back(instance);
        if(instance.registeredCollider)
        {
            appendClumpToWorldGeometry(&scene->worldGeometry, clump);
            appendAtomicToWorldGeometry(&scene->collisionGeometry,
                                        firstClumpAtomic(clump));
        }
        ++result->instantiated;
    }
    return true;
}

bool queryFstageWorldItem(const FstageScene* scene, int index,
                          FstageWorldItem* item)
{
    if(scene == NULL || index < 0 || item == NULL)
        return false;
    const stage_object_data::Stage* const stage =
        recoveredStage(scene->profileStageId);
    if(stage == NULL || stage->pools[1] < 0)
        return false;
    for(size_t i = 0; i < scene->stageObjects.size(); ++i)
    {
        const FstageObjectInstance& instance = scene->stageObjects[i];
        if(instance.pool != stage->pools[1] ||
           (instance.kind != 2 && instance.kind != 3) ||
           instance.clump == NULL || instance.clump->getFrame() == NULL)
            continue;
        if(index-- != 0)
            continue;
        item->recordIndex = instance.recordIndex;
        item->pool = instance.pool;
        item->kind = instance.kind;
        item->selector = instance.sub;
        instance.clump->getFrame()->updateObjects();
        item->position = instance.clump->getFrame()->getLTM()->pos;
        return true;
    }
    return false;
}

int findFstageWorldItem(const FstageScene* scene,
                        const rw::V3d& position)
{

    FstageWorldItem item;
    for(int i = 0; queryFstageWorldItem(scene, i, &item); ++i)
    {
        const float x = item.position.x - position.x;
        const float z = item.position.z - position.z;
        if(x * x + z * z < 100.0f)
            return i;
    }
    return -1;
}

bool removeFstageWorldItem(FstageScene* scene, int index)
{
    FstageWorldItem item;
    if(!queryFstageWorldItem(scene, index, &item))
        return false;
    for(size_t i = 0; i < scene->stageObjects.size(); ++i)
    {
        FstageObjectInstance& instance = scene->stageObjects[i];
        if(instance.recordIndex == item.recordIndex &&
           instance.pool == item.pool)
        {

            instance.state |= 0x8000;
            return true;
        }
    }
    return false;
}

size_t feedFstageObjectContacts(
    FstageScene* scene, const FstageObjectContact* contacts, size_t count)
{
    if(scene == NULL || contacts == NULL)
        return 0;
    size_t accepted = 0;
    for(size_t contactIndex = 0; contactIndex < count; ++contactIndex)
    {
        const FstageObjectContact& contact = contacts[contactIndex];
        for(size_t objectIndex = 0;
            objectIndex < scene->stageObjects.size(); ++objectIndex)
        {
            FstageObjectInstance& instance =
                scene->stageObjects[objectIndex];
            if(!fstageObjectContactAccepted(instance, contact))
                continue;
            instance.state |= 0x3000;
            ++instance.acceptedHitCount;
            if(instance.health > 0 && instance.health < 32000 &&
               (instance.state & 0x4000) == 0)
            {
                const int health =
                    static_cast<int>(instance.health) - contact.damage;
                instance.health =
                    static_cast<int16_t>(health < 0 ? 0 : health);
            }
            instance.runtimeFlags |= 2;
            if(scene->objectHooks.effect != NULL)
            {
                scene->objectHooks.effect(
                    6, contact.effectTransform, 0, 0,
                    scene->objectHooks.userData);
                scene->objectHooks.effect(
                    7, contact.effectTransform, 0, 0,
                    scene->objectHooks.userData);
            }
            ++accepted;
            runFstageDestructionScript(scene, objectIndex);
        }
    }
    return accepted;
}

bool completeFstageObjectAnimation(
    FstageScene* scene, uint16_t stageId, uint16_t recordIndex)
{
    if(scene == NULL)
        return false;
    for(size_t i = 0; i < scene->stageObjects.size(); ++i)
    {
        FstageObjectInstance& instance = scene->stageObjects[i];
        if(instance.stageId == stageId && instance.recordIndex == recordIndex &&
           instance.phase == FSTAGE_OBJECT_BREAK_ANIMATION)
        {
            instance.state |= 0x10;
            return true;
        }
    }
    return false;
}

int16_t queryFstageWeaponCandidate(
    FstageScene* scene, FstageWeaponActorState* actor, bool commit)
{
    if(scene == NULL || actor == NULL ||
       (actor->actionFlags & 0x4000) != 0)
        return FSTAGE_WEAPON_INVALID_ID;
    float nearestSquared = 550.0f * 550.0f;
    int16_t nearest = FSTAGE_WEAPON_INVALID_ID;
    for(int16_t i = 0; i < FSTAGE_WEAPON_CAPACITY; ++i)
    {
        FstageWeaponInstance& weapon =
            scene->stageWeapons.instances[i];
        if(!weapon.active || weapon.ownerSlot != -1 ||
           (weapon.flags & FSTAGE_WEAPON_HELD) != 0)
            continue;
        const FstageWeaponDescriptor* descriptor =
            fstageWeaponDescriptor(weapon.selector);
        if(descriptor == NULL || descriptor->radius == 0 ||
           !validFstageManagerId(scene, weapon.managerId))
            continue;
        const FstageWeaponManagerRecord& manager =
            scene->stageWeapons.records[weapon.managerId];
        if((manager.flags &
            (FSTAGE_WEAPON_MANAGER_ACTIVE |
             FSTAGE_WEAPON_MANAGER_UPDATE)) !=
            (FSTAGE_WEAPON_MANAGER_ACTIVE |
             FSTAGE_WEAPON_MANAGER_UPDATE) ||
           (manager.flags & FSTAGE_WEAPON_MANAGER_SUPPRESS_ONCE) != 0)
            continue;
        rw::Frame* frame = fstageWeaponFrame(scene, weapon);
        if(frame == NULL)
            continue;
        frame->updateObjects();
        const rw::V3d& position = frame->getLTM()->pos;
        if(!(position.y < actor->position.y))
            continue;
        const float x = position.x - actor->position.x;
        const float z = position.z - actor->position.z;
        const float distanceSquared = x * x + z * z;
        const float radius = floatFromBits(descriptor->radius);
        if(distanceSquared <= radius * radius &&
           distanceSquared < nearestSquared)
        {
            nearestSquared = distanceSquared;
            nearest = i;
        }
    }
    actor->targetedWeapon = nearest;
    if(commit && nearest >= 0)
    {
        FstageWeaponInstance& weapon =
            scene->stageWeapons.instances[nearest];
        actor->heldWeapon = nearest;
        weapon.ownerSlot = static_cast<int8_t>(actor->slotIndex);
        weapon.flags |= FSTAGE_WEAPON_HELD;
    }
    return nearest;
}

bool attachFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint8_t attachmentSlot)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || attachmentSlot > 1 ||
       actor->attachmentBones[attachmentSlot] == NULL)
        return false;
    FstageWeaponInstance& weapon =
        scene->stageWeapons.instances[weaponId];
    const FstageWeaponDescriptor* descriptor =
        fstageWeaponDescriptor(weapon.selector);
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || descriptor == NULL || frame == NULL)
        return false;
    actor->attachmentBones[attachmentSlot]->addChild(frame);
    applyFstageWeaponAttachment(frame, descriptor->position,
                                descriptor->rotation, attachmentSlot);
    frame->updateObjects();
    weapon.previousWorld = *frame->getLTM();
    weapon.ownerSlot = static_cast<int8_t>(actor->slotIndex);
    weapon.attachmentSlot = static_cast<int8_t>(attachmentSlot);
    weapon.flags = (weapon.flags & 0x60) | FSTAGE_WEAPON_HELD;
    weapon.velocity.x = weapon.velocity.y = weapon.velocity.z = 0.0f;
    weapon.angularVelocity.x = weapon.angularVelocity.y =
        weapon.angularVelocity.z = 0.0f;
    weapon.motionState = FSTAGE_WEAPON_MOTION_IDLE;
    actor->heldWeapon = weaponId;
    actor->actionFlags |= 0x4000;
    return true;
}

bool attachFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint8_t attachmentSlot,
    const WeaponDefinition& definition)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || attachmentSlot > 1 ||
       actor->attachmentBones[attachmentSlot] == NULL ||
       (actor->actionFlags & 0x4000) != 0 || actor->heldWeapon != weaponId)
        return false;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL || definition.objectId != weapon.selector)
        return false;
    if(frame->getParent() != NULL)
        frame->removeChild();
    actor->attachmentBones[attachmentSlot]->addChild(frame);
    applyFstageWeaponAttachment(frame, definition.rawWords.data() + 2,
                                definition.rawWords.data() + 8, attachmentSlot);
    frame->updateObjects();
    weapon.ownerSlot = static_cast<int8_t>(actor->slotIndex);
    weapon.attachmentSlot = static_cast<int8_t>(attachmentSlot);
    weapon.flags = (weapon.flags & 0x6d) | FSTAGE_WEAPON_ATTACHED;

    weapon.motionState = FSTAGE_WEAPON_MOTION_IDLE;
    actor->actionFlags |= 0x4000;
    return true;
}

bool switchFstageWeaponHand(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, int16_t hand,
    const WeaponDefinition& definition)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || actor->heldWeapon != weaponId ||
       (actor->actionFlags & 0x4000) == 0 || hand < -1 || hand > 1)
        return false;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    if(hand == -1)
        hand = weapon.attachmentSlot == 0 ? 1 : 0;
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL || definition.objectId != weapon.selector ||
       actor->attachmentBones[hand] == NULL)
        return false;
    if(frame->getParent() != NULL)
        frame->removeChild();
    actor->attachmentBones[hand]->addChild(frame);
    applyFstageWeaponAttachment(frame, definition.rawWords.data() + 2,
                                definition.rawWords.data() + 8,
                                static_cast<uint8_t>(hand));
    frame->updateObjects();
    weapon.attachmentSlot = static_cast<int8_t>(hand);
    return true;
}

bool snapshotFstageWeaponPose(
    FstageScene* scene, int16_t weaponId,
    const FstageWeaponActorState* actor)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || actor->heldWeapon != weaponId ||
       (actor->actionFlags & 0x4000) == 0)
        return false;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL)
        return false;
    frame->updateObjects();
    const rw::Matrix& world = *frame->getLTM();
    weapon.previousWorld.pos = world.pos;
    weapon.previousWorld.at = world.at;
    return true;
}

bool forceDropFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, const rw::V3d& bone21Forward)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || actor->heldWeapon != weaponId ||
       (actor->actionFlags & 0x4000) == 0)
        return false;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL)
        return false;
    detachFstageWeaponFrame(frame);
    weapon.velocity.x = bone21Forward.x * -0.2f;
    weapon.velocity.y = 0.0f;
    weapon.velocity.z = bone21Forward.z * -0.2f;
    weapon.angularVelocity.x = weapon.angularVelocity.y =
        weapon.angularVelocity.z = 0.0f;
    weapon.ownerSlot = -1;
    weapon.attachmentSlot = -1;
    weapon.flags = (weapon.flags & 0xee) | FSTAGE_WEAPON_POST_BREAK;
    weapon.motionState = FSTAGE_WEAPON_MOTION_BREAK_DROP;
    actor->heldWeapon = FSTAGE_WEAPON_INVALID_ID;
    actor->actionFlags &= ~0x4000u;
    if((actor->slotIndex == 0 && (weapon.flags & 0x20) != 0) ||
       (actor->slotIndex == 1 && (weapon.flags & 0x40) != 0))
        actor->linkedHeldWeapon = FSTAGE_WEAPON_INVALID_ID;
    return true;
}

bool tryForceDropFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, const WeaponDefinition& definition,
    const rw::V3d& bone21Forward)
{
    if(scene == NULL || weaponId < 0 || weaponId >= FSTAGE_WEAPON_CAPACITY ||
       definition.objectId != scene->stageWeapons.instances[weaponId].selector ||
       ((definition.rawWords[31] >> 16) & 0xff) == 0)
        return false;
    return forceDropFstageWeapon(scene, weaponId, actor, bone21Forward);
}

bool dropFstageWeapon(
    FstageScene* scene, int16_t weaponId, FstageWeaponActorState* actor)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY)
        return false;
    FstageWeaponInstance& weapon =
        scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL)
        return false;
    const int16_t held = actor->heldWeapon;
    detachFstageWeaponFrame(frame);
    flattenFstageWeapon(frame, weapon.groundY);
    if(held == actor->linkedHeldWeapon &&
       validFstageManagerId(scene, weapon.managerId))
        scene->stageWeapons.records[weapon.managerId].flags &=
            ~FSTAGE_WEAPON_MANAGER_UPDATE;
    weapon.flags &= 0x60;
    weapon.ownerSlot = -1;
    weapon.attachmentSlot = -1;
    weapon.motionState = FSTAGE_WEAPON_MOTION_IDLE;
    actor->heldWeapon = FSTAGE_WEAPON_INVALID_ID;
    actor->actionFlags &= ~0x4000u;
    return true;
}

bool tossFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint8_t weaponBehavior,
    const FstageWeaponTossContact* firstContact)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || actor->heldWeapon != weaponId ||
       (actor->actionFlags & 0x4000) == 0)
        return false;
    FstageWeaponInstance& weapon =
        scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL)
        return false;
    const rw::Matrix current = detachFstageWeaponFrame(frame);
    weapon.velocity.x =
        (current.pos.x - weapon.previousWorld.pos.x) * 0.25f;
    weapon.velocity.y = 0.5f;
    weapon.velocity.z =
        (current.pos.z - weapon.previousWorld.pos.z) * 0.25f;
    fstageWeaponAngularDelta(
        current, weapon.previousWorld, weapon.angularVelocity);
    if(firstContact != NULL)
    {
        const float fraction = 1.0f - firstContact->fraction;
        const rw::V3d& normal = firstContact->normal;
        const float projection = -(
            (current.pos.x - actor->position.x) * fraction * normal.x +
            (current.pos.y - actor->position.y) * fraction * normal.y +
            (current.pos.z - actor->position.z) * fraction * normal.z);
        const rw::V3d correction =
        {
            actor->position.x - (current.pos.x + normal.x * projection),
            0.0f,
            actor->position.z - (current.pos.z + normal.z * projection)
        };
        frame->translate(&correction, rw::COMBINEPOSTCONCAT);
    }
    const float angularScale =
        weaponBehavior == 5 ? 0.08f :
        (weaponBehavior == 4 ? 0.124f : 1.0f);
    weapon.angularVelocity.x *= angularScale;
    weapon.angularVelocity.y *= angularScale;
    weapon.angularVelocity.z *= angularScale;
    weapon.flags = (weapon.flags & ~FSTAGE_WEAPON_ATTACHED) |
        FSTAGE_WEAPON_NORMAL_TOSS;
    weapon.ownerSlot = -1;
    weapon.attachmentSlot = -1;
    weapon.motionState = FSTAGE_WEAPON_MOTION_TOSS;
    actor->heldWeapon = FSTAGE_WEAPON_INVALID_ID;
    actor->actionFlags &= ~0x4000u;
    if((actor->battleSide == 0 && (weapon.flags & 0x20) != 0) ||
       (actor->battleSide == 1 && (weapon.flags & 0x40) != 0))
        actor->linkedHeldWeapon = FSTAGE_WEAPON_INVALID_ID;
    return true;
}

bool throwFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, const rw::V3d& target)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || actor->heldWeapon != weaponId ||
       (actor->actionFlags & 0x4000) == 0)
        return false;
    FstageWeaponInstance& weapon =
        scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = fstageWeaponFrame(scene, weapon);
    if(!weapon.active || frame == NULL)
        return false;
    const rw::Matrix world = detachFstageWeaponFrame(frame);
    float x = target.x - world.pos.x;
    float z = target.z - world.pos.z;
    const float length = sqrtf(x * x + z * z);
    if(length > 0.0f)
    {
        x /= length;
        z /= length;
    }
    else
        x = z = 0.0f;
    weapon.velocity.x = x * 1.25f;
    weapon.velocity.y = 0.75f;
    weapon.velocity.z = z * 1.25f;
    weapon.angularVelocity.x = weapon.angularVelocity.y =
        weapon.angularVelocity.z = 0.0f;
    const float dotAt = x * world.at.x + z * world.at.z;
    if(dotAt < -0.5f || dotAt > 0.5f)
        weapon.angularVelocity.x = dotAt > 0.0f ? -5.0f : 5.0f;
    else
    {
        const float dotRight = x * world.right.x + z * world.right.z;
        weapon.angularVelocity.z = dotRight > 0.0f ? 5.0f : -5.0f;
    }
    weapon.flags = (weapon.flags & ~FSTAGE_WEAPON_ATTACHED) |
        FSTAGE_WEAPON_TARGET_THROW;
    weapon.attachmentSlot = -1;
    weapon.motionState = FSTAGE_WEAPON_MOTION_THROW;
    if((weapon.flags & 0x60) != 0)
        actor->linkedHeldWeapon = FSTAGE_WEAPON_INVALID_ID;
    return true;
}

bool breakFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint32_t randomU31,
    int8_t characterTerm, int8_t modifier)
{
    static const uint8_t baseThreshold[4] = { 0, 20, 50, 80 };
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY)
        return false;
    FstageWeaponInstance& weapon =
        scene->stageWeapons.instances[weaponId];
    const FstageWeaponDescriptor* descriptor =
        fstageWeaponDescriptor(weapon.selector);
    if(!weapon.active || weapon.broken || descriptor == NULL ||
       descriptor->breakClass == 0 || descriptor->breakClass > 3 ||
       !validFstageManagerId(scene, weapon.primaryManagerId))
        return false;
    const float threshold =
        static_cast<float>(baseThreshold[descriptor->breakClass] +
                           static_cast<int>(characterTerm)) +
        static_cast<float>(-10 * static_cast<int>(modifier));
    if(static_cast<float>(randomU31 % 100) >= threshold)
        return false;
    FstageWeaponManagerRecord& primary =
        scene->stageWeapons.records[weapon.primaryManagerId];
    const int16_t linked = primary.linkedManagerId;
    if(linked <= 0 || !validFstageManagerId(scene, linked))
        return false;
    rw::Frame* oldFrame = fstageWeaponFrame(scene, weapon);
    rw::Frame* parent = oldFrame == NULL ? NULL : oldFrame->getParent();
    rw::Matrix world;
    world.setIdentity();
    if(oldFrame != NULL)
    {
        oldFrame->updateObjects();
        world = *oldFrame->getLTM();
    }
    if(oldFrame != NULL)
        detachFstageWeaponFrame(oldFrame);
    primary.flags &= ~static_cast<uint16_t>(
        FSTAGE_WEAPON_MANAGER_ACTIVE | FSTAGE_WEAPON_MANAGER_UPDATE);
    if(primary.clump != NULL)
    {
        if(scene->world != NULL)
            scene->world->removeClump(primary.clump);
        primary.clump->destroy();
        primary.clump = NULL;
    }

    FstageWeaponManagerRecord& replacement =
        scene->stageWeapons.records[linked];
    rw::Frame* replacementFrame =
        replacement.clump == NULL ? NULL : replacement.clump->getFrame();
    if(replacementFrame == NULL)
        return false;
    setFstageWeaponRendered(replacement.clump, true);
    if(parent != NULL)
    {
        parent->addChild(replacementFrame);
        applyFstageWeaponAttachment(
            replacementFrame, descriptor->position, descriptor->rotation,
            weapon.attachmentSlot < 0 ? 0 :
                static_cast<uint8_t>(weapon.attachmentSlot));
    }
    else
        replacementFrame->transform(&world, rw::COMBINEREPLACE);
    replacement.flags |=
        FSTAGE_WEAPON_MANAGER_ACTIVE | FSTAGE_WEAPON_MANAGER_UPDATE;
    weapon.managerId = linked;
    weapon.broken = true;

    if(scene->objectHooks.effect != NULL &&
       actor->attachmentBones[0] != NULL)
    {
        actor->attachmentBones[0]->updateObjects();
        scene->objectHooks.effect(
            6, *actor->attachmentBones[0]->getLTM(), 200, 0x14,
            scene->objectHooks.userData);
    }
    if(scene->objectHooks.weaponAction != NULL)
    {
        scene->objectHooks.weaponAction(
            2, 0x4e, scene->objectHooks.userData);
        scene->objectHooks.weaponAction(
            2, 0x5b, scene->objectHooks.userData);
        scene->objectHooks.weaponAction(
            1, 0x11, scene->objectHooks.userData);
    }

    detachFstageWeaponFrame(replacementFrame);
    weapon.velocity.x = -0.2f * actor->forward.x;
    weapon.velocity.y = 0.0f;
    weapon.velocity.z = -0.2f * actor->forward.z;
    weapon.angularVelocity.x = weapon.angularVelocity.y =
        weapon.angularVelocity.z = 0.0f;
    weapon.flags = (weapon.flags & 0x60) | FSTAGE_WEAPON_POST_BREAK;
    weapon.ownerSlot = -1;
    weapon.attachmentSlot = -1;
    weapon.motionState = FSTAGE_WEAPON_MOTION_BREAK_DROP;
    actor->heldWeapon = FSTAGE_WEAPON_INVALID_ID;
    actor->actionFlags &= ~0x4000u;
    if((actor->slotIndex == 0 && (weapon.flags & 0x20) != 0) ||
       (actor->slotIndex == 1 && (weapon.flags & 0x40) != 0))
        actor->linkedHeldWeapon = FSTAGE_WEAPON_INVALID_ID;
    replacement.flags &= ~static_cast<uint16_t>(
        FSTAGE_WEAPON_MANAGER_ACTIVE | FSTAGE_WEAPON_MANAGER_UPDATE);
    return true;
}

bool setFstageWeaponVisible(FstageScene* scene, int16_t weaponId, bool visible)
{
    if(scene == NULL || weaponId < 0 || weaponId >= FSTAGE_WEAPON_CAPACITY)
        return false;
    const FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    if(!weapon.active || !validFstageManagerId(scene, weapon.managerId))
        return false;
    FstageWeaponManagerRecord& manager = scene->stageWeapons.records[weapon.managerId];
    if(visible)
        manager.flags |= FSTAGE_WEAPON_MANAGER_DRAW;
    else
        manager.flags &= ~FSTAGE_WEAPON_MANAGER_DRAW;
    setFstageWeaponRendered(manager.clump,
        visible && (manager.flags & FSTAGE_WEAPON_MANAGER_ACTIVE) != 0);
    return true;
}

bool releaseFstageWeapon(FstageScene* scene, int16_t weaponId)
{
    if(scene == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY)
        return false;
    FstageWeaponInstance& weapon =
        scene->stageWeapons.instances[weaponId];
    if(!weapon.active)
        return false;
    int16_t linked = FSTAGE_WEAPON_INVALID_ID;
    if(validFstageManagerId(scene, weapon.primaryManagerId))
        linked = scene->stageWeapons.records[
            weapon.primaryManagerId].linkedManagerId;
    if(linked >= 0)
        releaseFstageManagerRecord(scene, linked);
    releaseFstageManagerRecord(scene, weapon.primaryManagerId);
    if(weapon.managerId != weapon.primaryManagerId &&
       weapon.managerId != linked)
        releaseFstageManagerRecord(scene, weapon.managerId);
    weapon = FstageWeaponInstance();
    if(scene->stageWeapons.weaponCount > 0)
        --scene->stageWeapons.weaponCount;
    return true;
}

void updateFstageWeapons(FstageScene* scene, float deltaSeconds)
{
    if(scene == NULL || deltaSeconds <= 0.0f)
        return;
    for(int16_t managerId = 0;
        managerId < FSTAGE_WEAPON_CAPACITY; ++managerId)
    {
        FstageWeaponManagerRecord& manager =
            scene->stageWeapons.records[managerId];
        if((manager.flags &
            (FSTAGE_WEAPON_MANAGER_ACTIVE |
             FSTAGE_WEAPON_MANAGER_UPDATE)) ==
            (FSTAGE_WEAPON_MANAGER_ACTIVE |
             FSTAGE_WEAPON_MANAGER_UPDATE) &&
           (manager.flags & FSTAGE_WEAPON_MANAGER_SUPPRESS_ONCE) != 0)
            manager.flags &= ~FSTAGE_WEAPON_MANAGER_SUPPRESS_ONCE;
    }
    rw::V3d axisX = { 1.0f, 0.0f, 0.0f };
    rw::V3d axisY = { 0.0f, 1.0f, 0.0f };
    rw::V3d axisZ = { 0.0f, 0.0f, 1.0f };
    for(int16_t i = 0; i < FSTAGE_WEAPON_CAPACITY; ++i)
    {
        FstageWeaponInstance& weapon =
            scene->stageWeapons.instances[i];
        if(!weapon.active)
            continue;
        rw::Frame* frame = fstageWeaponFrame(scene, weapon);
        if(frame == NULL)
            continue;
        if(weapon.motionState == FSTAGE_WEAPON_MOTION_IDLE)
        {
            frame->updateObjects();
            weapon.previousWorld = *frame->getLTM();
            continue;
        }
        if((weapon.flags & FSTAGE_WEAPON_NORMAL_TOSS) != 0)
        {
            weapon.motionState = FSTAGE_WEAPON_MOTION_TOSS;
            weapon.flags &= ~FSTAGE_WEAPON_NORMAL_TOSS;
        }
        else if((weapon.flags & FSTAGE_WEAPON_POST_BREAK) != 0)
        {
            weapon.motionState = FSTAGE_WEAPON_MOTION_BREAK_DROP;
            weapon.flags &= ~FSTAGE_WEAPON_POST_BREAK;
        }
        else if((weapon.flags & FSTAGE_WEAPON_TARGET_THROW) != 0)
            weapon.motionState = FSTAGE_WEAPON_MOTION_THROW;

        frame->rotate(
            &axisX, weapon.angularVelocity.x, rw::COMBINEPOSTCONCAT);
        frame->rotate(
            &axisY, weapon.angularVelocity.y, rw::COMBINEPOSTCONCAT);
        frame->rotate(
            &axisZ, weapon.angularVelocity.z, rw::COMBINEPOSTCONCAT);
        frame->translate(&weapon.velocity, rw::COMBINEPOSTCONCAT);
        weapon.velocity.y -=
            weapon.motionState == FSTAGE_WEAPON_MOTION_BREAK_DROP ?
                0.05f : 0.1f;
        frame->updateObjects();
        const rw::V3d position = frame->getLTM()->pos;
        float floor = position.y;
        const FstageWeaponDescriptor* descriptor =
            fstageWeaponDescriptor(weapon.selector);
        const float offset = descriptor == NULL ? 0.0f :
            floatFromBits(descriptor->groundOffset);
        if(!rwWorldFloorNearHeight(
               scene->worldGeometry, position.x, position.z,
               position.y, &floor) ||
           position.y > floor + offset)
            continue;
        if(weapon.motionState == FSTAGE_WEAPON_MOTION_THROW)
        {
            weapon.velocity.x *= 0.25f;
            weapon.velocity.y = 0.4f;
            weapon.velocity.z *= 0.25f;
            weapon.flags &= ~FSTAGE_WEAPON_TARGET_THROW;
            weapon.ownerSlot = -1;
            weapon.motionState = FSTAGE_WEAPON_MOTION_THROW_SETTLE;
            continue;
        }
        flattenFstageWeapon(frame, floor + offset);
        weapon.velocity.x = weapon.velocity.y = weapon.velocity.z = 0.0f;
        weapon.angularVelocity.x = weapon.angularVelocity.y =
            weapon.angularVelocity.z = 0.0f;
        weapon.motionState = FSTAGE_WEAPON_MOTION_IDLE;
    }
}

void buildFstageInteractionMarkers(FstageScene* scene)
{
    if(scene == NULL)
        return;
    scene->interactionMarkers.clear();
    rw::Clump* clump = scene->sectionClumps[3];
    if(clump == NULL || clump->getFrame() == NULL)
        return;
    rw::Frame* rootChild = clump->getFrame()->child;
    FstageInteractionMarker marker;
    FORLIST(link, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(link);
        rw::Frame* frame = atomic == NULL ? NULL : atomic->getFrame();
        if(frame == NULL)
            continue;
        if(frame->getParent() == rootChild)
        {
            marker.broadphaseAtomic = atomic;
            rw::HAnimData* data = rw::HAnimData::get(frame);
            int32_t id = data == NULL ? 0 : data->id;
            marker.markerKey = static_cast<int16_t>(id);
            marker.textId = static_cast<int16_t>(id);
            marker.commandParameter = -1;
            if(marker.textId < 0)
            {
                const int32_t encoded = -static_cast<int32_t>(marker.textId);
                marker.commandParameter =
                    static_cast<int16_t>(encoded % 10);
                marker.textId = static_cast<int16_t>(encoded / 10);
            }
        }
        else
            marker.collisionAtomic = atomic;

        if(marker.broadphaseAtomic != NULL &&
           marker.collisionAtomic != NULL)
        {
            appendAtomicToWorldGeometry(&marker.broadphaseGeometry,
                                        marker.broadphaseAtomic);
            appendAtomicToWorldGeometry(&marker.collisionGeometry,
                                        marker.collisionAtomic);
            scene->interactionMarkers.push_back(marker);
            marker = FstageInteractionMarker();
            if(scene->interactionMarkers.size() ==
               FSTAGE_INLINE_COLLIDER_CAPACITY)
                break;
        }
    }
}

size_t gatherFstageCameraCollisionGeometry(
    const FstageScene& scene, const RwWorldGeometry** geometry,
    size_t capacity)
{
    if(geometry == NULL || capacity == 0)
        return 0;

    size_t count = 0;
    if(!scene.cameraStaticCollisionGeometry.triangles.empty())
        geometry[count++] = &scene.cameraStaticCollisionGeometry;
    for(size_t i = 0;
        i < scene.interactionMarkers.size() &&
        i < FSTAGE_INLINE_COLLIDER_CAPACITY && count < capacity; ++i)
    {
        const FstageInteractionMarker& marker = scene.interactionMarkers[i];
        if(marker.broadphaseAtomic != NULL &&
           marker.collisionAtomic != NULL &&
           !marker.collisionGeometry.triangles.empty())
        {
            geometry[count++] = &marker.collisionGeometry;
        }
    }
    return count;
}

FstageMarkerResolution resolveFstageInteractionMarker(
    const FstageScene& scene, const rw::V3d& playerPosition,
    const FstageMarkerQueryState& state, FstageMarkerPrompt* prompt)
{
    if(prompt == NULL)
        return FSTAGE_MARKER_NONE;
    *prompt = FstageMarkerPrompt();
    float floor = 0.0f;
    if(!rwWorldGroundHeight(scene.worldGeometry, playerPosition.x,
                            playerPosition.z, playerPosition.y, &floor))
        return FSTAGE_MARKER_NONE;
    RwWorldVertex top;
    top.x = playerPosition.x;
    top.y = floor + 10.0f;
    top.z = playerPosition.z;
    RwWorldVertex bottom = top;
    bottom.y = floor - 25.0f;

    for(size_t i = 0; i < scene.interactionMarkers.size(); ++i)
    {
        const FstageInteractionMarker& marker =
            scene.interactionMarkers[i];
        if(!rwWorldCapsuleIntersects(marker.broadphaseGeometry,
                                     top, bottom, 5.0f))
            continue;
        const int16_t side = rwWorldCapsuleIntersects(
            marker.collisionGeometry, top, bottom, 5.0f) ? 1 : 0;
        if(state.stageId == 0x30 && marker.textId == 0x33)
            return FSTAGE_MARKER_NONE;

        prompt->textId = marker.textId;
        prompt->commandParameter = marker.commandParameter;
        prompt->side = side;
        prompt->contextValue = marker.contextValue;
        const int eventFlag =
            markerEventFlag(state.stageId, marker.markerKey);
        if(eventFlag >= 0 && state.eventFlag != NULL &&
           state.eventFlag(static_cast<uint16_t>(eventFlag),
                           state.userData))
        {
            prompt->displayTextId = consumedMarkerText(eventFlag);
            prompt->resolution = FSTAGE_MARKER_CONSUMED_TEXT;
            return prompt->resolution;
        }
        if(marker.textId < 0 || marker.textId >= 56 ||
           (state.enabledTextMask &
            (UINT64_C(1) << static_cast<uint16_t>(marker.textId))) == 0)
            return FSTAGE_MARKER_NONE;
        prompt->displayTextId = marker.textId;
        prompt->resolution = side > 0 ?
            FSTAGE_MARKER_TRANSITION : FSTAGE_MARKER_PROMPT;
        return prompt->resolution;
    }
    return FSTAGE_MARKER_NONE;
}

namespace
{

bool decodeFstageScene(const char* entryName, rw::World* world,
                       uint16_t profileStageId, FstageScene* scene)
{
    scene->world = world;
    scene->profileStageId = profileStageId;
    std::vector<SectionSpan> sections;
    if(!readDirectory(scene->data, sections))
    {
        SectionSpan streamSection;
        streamSection.offset = 0;
        streamSection.length =
            static_cast<uint32_t>(scene->data.size());
        rw::Clump* clump = NULL;
        if(!loadTextureDictionary(scene->data, streamSection,
                                  scene->textureDictionary) ||
           !addStageClump(scene,
                         clump = readClump(scene->data.data(),
                                           streamSection.length)))
        {
            printf("ERROR: Could not decode composite stage '%s'.\n",
                   entryName);
            freeFstageScene(scene);
            return false;
        }
        scene->hierarchy = rw::HAnimHierarchy::get(clump);
        loadAnimations(scene->data, streamSection, scene->animations);
        if(scene->hierarchy != NULL && !scene->animations.empty())
            scene->hierarchy->interpolator->setCurrentAnim(
                scene->animations[0]);
        scene->activeAnimation = 0;
        calculateBounds(scene);
        scene->authoredWorldVertexCount = scene->worldGeometry.vertices.size();
        scene->authoredWorldTriangleCount =
            scene->worldGeometry.triangles.size();
        return true;
    }
    if(!loadTextureDictionary(scene->data, sections[0],
                              scene->textureDictionary))
    {
        printf("ERROR: Invalid ST4 texture section in '%s'.\n",
               entryName);
        freeFstageScene(scene);
        return false;
    }

    scene->worldClump = readRwWorldClump(
        scene->data.data() + sections[1].offset, sections[1].length,
        &scene->worldGeometry);
    if(scene->worldClump == NULL)
    {
        printf("ERROR: Could not decode ST4 world section in '%s'.\n",
               entryName);
        freeFstageScene(scene);
        return false;
    }
    prepareStageClump(scene, scene->worldClump, true);
    scene->world->addClump(scene->worldClump);
    scene->clumps.push_back(scene->worldClump);
    if(scene->clump == NULL)
        scene->clump = scene->worldClump;
    for(size_t i = 2; i <= 5; ++i)
    {
        rw::Clump* clump = readClump(scene->data.data() + sections[i].offset,
                                     sections[i].length);
        if(!addStageClump(scene, clump))
        {
            printf("ERROR: Could not decode ST4 clump section %u in '%s'.\n",
                   static_cast<unsigned>(i), entryName);
            freeFstageScene(scene);
            return false;
        }
        scene->sectionClumps[i - 2] = clump;
        if(i == 5)
            scene->hierarchy = rw::HAnimHierarchy::get(clump);
    }

    scene->groundGeometry = RwWorldGeometry();
    appendClumpToWorldGeometry(&scene->groundGeometry,
                               scene->sectionClumps[2]);

    loadAnimations(scene->data, sections[6], scene->animations);
    scene->objectAnimationBase = scene->animations.size();
    if(sections.size() > 9)
        loadAnimations(scene->data, sections[9], scene->animations);
    if(scene->hierarchy != NULL && !scene->animations.empty())
        scene->hierarchy->interpolator->setCurrentAnim(scene->animations[0]);
    if(scene->hierarchy != NULL)
    {
        scene->hierarchy->attach();
        scene->hierarchy->updateMatrices();
    }

    if(!loadObjectTemplates(scene, sections[7]) ||
       !instantiatePlacements(scene, sections[8]))
    {
        printf("ERROR: Could not decode ST4 objects in '%s'.\n", entryName);
        freeFstageScene(scene);
        return false;
    }

    scene->activeAnimation = 0;

    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        if(scene->clumps[i] == scene->worldClump)
            continue;
        appendClumpToWorldGeometry(&scene->worldGeometry, scene->clumps[i]);
    }
    scene->authoredWorldVertexCount = scene->worldGeometry.vertices.size();
    scene->authoredWorldTriangleCount =
        scene->worldGeometry.triangles.size();

    scene->cameraStaticCollisionGeometry = RwWorldGeometry();
    appendAtomicToWorldGeometry(&scene->cameraStaticCollisionGeometry,
                                firstClumpAtomic(scene->sectionClumps[1]));
    buildFstageInteractionMarkers(scene);
    buildFstageCollisionGeometry(scene, profileStageId);
    calculateBounds(scene);
    return true;
}

}

uint32_t
st4EntryIndexForStage(uint16_t stageId, uint32_t variantSelector,
                      bool gameStateBit1, bool matchSetupFlag)
{
    int32_t offset = 0;
    switch(stageId)
    {
    case 0x01:

        if(gameStateBit1 && matchSetupFlag)
            offset = 0x17;
        else
            offset = static_cast<int32_t>(variantSelector % 3);
        break;
    case 0x07:
        offset = (variantSelector & 1) != 0 ? 4 : 0;
        break;
    case 0x13:
        switch(variantSelector % 3)
        {
        case 1: offset = -1; break;
        case 2: offset = 8; break;
        default: break;
        }
        break;
    case 0x2f:
        switch(variantSelector % 3)
        {
        case 1: offset = -0x13; break;
        case 2: offset = -0x0b; break;
        default: break;
        }
        break;
    default:
        break;
    }
    const int32_t entry = static_cast<int32_t>(stageId) + offset;
    if(entry < static_cast<int32_t>(FSTAGE_ST4_FIRST_ENTRY))
        return FSTAGE_ST4_FIRST_ENTRY;
    if(entry > static_cast<int32_t>(FSTAGE_ST4_LAST_ENTRY))
        return FSTAGE_ST4_LAST_ENTRY;
    return static_cast<uint32_t>(entry);
}

bool
loadFstageSceneByIndex(const char* archivePath, uint32_t entryIndex,
                       rw::World* world, FstageScene* scene)
{
    const uint16_t stageId = entryIndex < stage_object_data::STAGE_COUNT ?
        static_cast<uint16_t>(entryIndex) : 0xffff;
    return loadFstageSceneByIndexForStage(
        archivePath, entryIndex, stageId, world, scene);
}

bool
loadFstageSceneByIndexForStage(const char* archivePath,
                               uint32_t entryIndex, uint16_t stageId,
                               rw::World* world, FstageScene* scene)
{
    if(scene == NULL || world == NULL)
        return false;

    freeFstageScene(scene);
    if(!readXafEntryByIndex(archivePath, entryIndex, scene->data))
        return false;

    char label[32];
    snprintf(label, sizeof(label), "FSTAGE entry %u",
             static_cast<unsigned>(entryIndex));
    return decodeFstageScene(label, world, stageId, scene);
}

bool
loadFstageScene(const char* archivePath, const char* entryName,
                rw::World* world, FstageScene* scene)
{
    if(scene == NULL || world == NULL)
        return false;

    freeFstageScene(scene);
    if(!readXafEntry(archivePath, entryName, scene->data))
        return false;

    return decodeFstageScene(entryName, world, 0xffff, scene);
}

void
tickFstageScene(FstageScene* scene, float deltaSeconds)
{
    if(scene == NULL || deltaSeconds <= 0.0f)
        return;

    updateFstageWeapons(scene, deltaSeconds);
    for(size_t i = 0; i < scene->stageObjects.size();)
    {
        FstageObjectInstance& instance = scene->stageObjects[i];
        if((instance.state & 0x8000) != 0)
        {
            destroyFstageObject(scene, i);
            continue;
        }
        if(instance.phase == FSTAGE_OBJECT_BREAK_ANIMATION)
        {
            const bool wasInitialized = instance.animationInitialized;
            initializeFstageBreakAnimation(scene, instance);
            if(!wasInitialized)
            {
                ++i;
                continue;
            }
            if(instance.hierarchy != NULL &&
               instance.hierarchy->interpolator != NULL)
            {
                rw::AnimInterpolator* interpolator =
                    instance.hierarchy->interpolator;
                const float duration =
                    interpolator->currentAnim == NULL ? 0.0f :
                    interpolator->currentAnim->duration;
                if(duration <= 0.0f ||
                   interpolator->currentTime + deltaSeconds >= duration)
                {
                    if(duration > interpolator->currentTime)
                        interpolator->addTime(
                            duration - interpolator->currentTime);
                    instance.hierarchy->updateMatrices();
                    instance.state |= 0x10;
                }
                else
                {
                    interpolator->addTime(deltaSeconds);
                    instance.hierarchy->updateMatrices();
                }
            }
            if((instance.state & 0x10) != 0)
                replaceFstageObject(
                    scene, i, static_cast<int16_t>(instance.pool + 1),
                    FSTAGE_OBJECT_FADE);
        }
        else if(instance.phase == FSTAGE_OBJECT_FADE)
        {
            if(instance.fadeCounter < 30)
            {
                ++instance.fadeCounter;
                const uint8_t alpha = static_cast<uint8_t>(
                    ((30 - instance.fadeCounter) * 255) / 30);
                setStageObjectAlpha(instance, alpha);
            }
            else
            {
                setStageObjectAlpha(instance, 0);
                instance.state |= 0x8000;
            }
        }
        ++i;
    }

    if(scene->hierarchy != NULL &&
       scene->activeAnimation < scene->animations.size())
    {
        scene->hierarchy->interpolator->addTime(deltaSeconds);
        scene->hierarchy->updateMatrices();
        buildFstageInteractionMarkers(scene);
    }
}

void
freeFstageScene(FstageScene* scene)
{
    if(scene == NULL)
        return;
    clearFstageStageObjects(scene);
    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        if(scene->world != NULL)
            scene->world->removeClump(scene->clumps[i]);
        scene->clumps[i]->destroy();
    }
    for(size_t i = 0; i < scene->objectTemplates.size(); ++i)
        scene->objectTemplates[i]->destroy();
    for(size_t i = 0; i < scene->animations.size(); ++i)
        scene->animations[i]->destroy();
    if(scene->textureDictionary != NULL)
    {
        if(rw::TexDictionary::getCurrent() == scene->textureDictionary)
            rw::TexDictionary::setCurrent(NULL);
        scene->textureDictionary->destroy();
    }

    scene->data.clear();
    scene->clumps.clear();
    scene->objectTemplates.clear();
    scene->objectBounds.clear();
    scene->placements.clear();
    scene->interactionMarkers.clear();
    scene->animations.clear();
    scene->textureDictionary = NULL;
    scene->clump = NULL;
    scene->worldClump = NULL;
    for(size_t i = 0; i < 4; ++i)
        scene->sectionClumps[i] = NULL;
    scene->groundGeometry = RwWorldGeometry();
    scene->collisionGeometry = RwWorldGeometry();
    scene->combatSurfaceGeometry = RwWorldGeometry();
    scene->cameraStaticCollisionGeometry = RwWorldGeometry();
    scene->worldGeometry = RwWorldGeometry();
    scene->hierarchy = NULL;
    scene->activeAnimation = 0;
    scene->objectAnimationBase = 0;
    scene->world = NULL;
    scene->center.x = scene->center.y = scene->center.z = 0.0f;
    scene->minBounds.x = scene->minBounds.y = scene->minBounds.z = 0.0f;
    scene->maxBounds.x = scene->maxBounds.y = scene->maxBounds.z = 0.0f;
    scene->radius = 0.0f;
    scene->haveBounds = false;
    scene->profileStageId = 0xffff;
    scene->collisionProfileReady = false;
    scene->authoredWorldVertexCount = 0;
    scene->authoredWorldTriangleCount = 0;
}

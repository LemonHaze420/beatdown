#include "character.h"
#include "character_internal.h"
#include "character_records.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include <rw.h>

#include <filesystem>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <utility>
#include <new>

using namespace character_detail;

namespace
{

static_assert(sizeof(CharacterArchiveMetadata) == 0x34,
              "actor archive metadata ABI");

std::vector<uint8_t> g_characterNpcTextureTable;
std::vector<uint8_t> g_characterNpcReplacementTable;
std::vector<XafEntryInfo> g_characterNpcDataEntries;
std::string g_characterNpcDataPath;
std::vector<uint8_t> g_characterArchiveMetadata;
std::vector<uint8_t> g_characterCostumeTables[3];

float decodeCompressedFloat(uint16_t value)
{
    uint32_t bits = static_cast<uint32_t>(value & 0x8000) << 16;
    if((value & 0x7fff) != 0)
    {
        bits |= (static_cast<uint32_t>(value & 0x7800) << 12) +
                0x38000000;
        bits |= static_cast<uint32_t>(value & 0x07ff) << 12;
    }
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

bool convertCompressedAnimation(std::vector<uint8_t>& data)
{
    const uint32_t COMPRESSED_FRAME_SIZE = 22;
    const uint32_t HANIM_FRAME_SIZE = 36;
    const uint32_t CUSTOM_DATA_SIZE = 24;
    if(data.size() < 32 || read32(data.data()) != CHUNK_ANIMATION ||
       read32(data.data() + 12) != 0x100)
        return false;

    const uint32_t type = read32(data.data() + 16);
    if(type == 1)
        return true;
    if(type != 2)
        return false;

    const uint32_t frameCount = read32(data.data() + 20);
    const uint64_t compressedPayloadSize =
        20 + static_cast<uint64_t>(frameCount) * COMPRESSED_FRAME_SIZE +
        CUSTOM_DATA_SIZE;
    if(compressedPayloadSize != read32(data.data() + 4) ||
       compressedPayloadSize + 12 != data.size())
        return false;

    const uint8_t* custom =
        data.data() + 32 + frameCount * COMPRESSED_FRAME_SIZE;
    float translationOffset[3];
    float translationScale[3];
    memcpy(translationOffset, custom, sizeof(translationOffset));
    memcpy(translationScale, custom + sizeof(translationOffset),
           sizeof(translationScale));

    const uint32_t outputPayloadSize = 20 + frameCount * HANIM_FRAME_SIZE;
    std::vector<uint8_t> output(12 + outputPayloadSize);
    memcpy(output.data(), data.data(), 32);
    write32(output.data() + 4, outputPayloadSize);
    write32(output.data() + 16, 1);

    for(uint32_t i = 0; i < frameCount; i++)
    {
        const uint8_t* source =
            data.data() + 32 + i * COMPRESSED_FRAME_SIZE;
        uint8_t* destination = output.data() + 32 + i * HANIM_FRAME_SIZE;
        memcpy(destination, source, 4);
        for(uint32_t component = 0; component < 7; component++)
        {
            float value = decodeCompressedFloat(
                read16(source + 4 + component * 2));
            if(component >= 4)
            {
                const uint32_t axis = component - 4;
                value = value * translationScale[axis] +
                        translationOffset[axis];
            }
            memcpy(destination + 4 + component * 4, &value, sizeof(value));
        }

        int32_t previousOffset;
        memcpy(&previousOffset, source + 18, sizeof(previousOffset));
        if(previousOffset < 0)
        {

            previousOffset = -static_cast<int32_t>(HANIM_FRAME_SIZE);
        }
        else
        {
            if(previousOffset % static_cast<int32_t>(0x18) != 0 ||
               static_cast<uint32_t>(previousOffset / 0x18) >= frameCount)
                return false;
            previousOffset = previousOffset / 0x18 * 0x24;
        }
        memcpy(destination + 32, &previousOffset, sizeof(previousOffset));
    }

    data.swap(output);
    return true;
}

rw::Texture*
readMissingCharacterTexture(const char* name, const char* mask)
{
    rw::Texture* texture = rw::Texture::create(NULL);
    if(texture == NULL)
        return NULL;
    strncpy(texture->name, name == NULL ? "" : name,
            sizeof(texture->name) - 1);
    texture->name[sizeof(texture->name) - 1] = '\0';
    if(mask != NULL)
    {
        strncpy(texture->mask, mask, sizeof(texture->mask) - 1);
        texture->mask[sizeof(texture->mask) - 1] = '\0';
    }
    return texture;
}

rw::Raster* readImageRaster(const uint8_t* chunk, uint32_t chunkLength)
{
    rw::Image* image = decodeCharacterImageChunk(chunk, chunkLength);
    if(image == NULL)
        return NULL;
    rw::Raster* raster = rw::Raster::createFromImage(image);
    image->destroy();
    if(raster == NULL)
        return NULL;
    raster = rw::Raster::convertTexToCurrentPlatform(raster);
    if(raster == NULL)
        return NULL;
    const bool compatibleD3dRaster =
        (rw::platform == rw::PLATFORM_D3D8 ||
         rw::platform == rw::PLATFORM_D3D9) &&
        (raster->platform == rw::PLATFORM_D3D8 ||
         raster->platform == rw::PLATFORM_D3D9);
    if(raster->platform != rw::platform && !compatibleD3dRaster)
    {
        raster->destroy();
        return NULL;
    }
    return raster;
}

bool
buildCharacterBoneTransforms(rw::HAnimHierarchy* hierarchy, rw::Skin* skin,
                             std::vector<CharacterBoneTransform>& transforms)
{
    if(hierarchy == NULL || skin == NULL ||
       skin->numBones != hierarchy->numNodes || skin->numBones <= 0)
        return false;

    transforms.resize(skin->numBones);
    transforms[0].parentIndex = -1;
    transforms[0].translation[0] = 0.0f;
    transforms[0].translation[1] = 0.0f;
    transforms[0].translation[2] = 0.0f;

    rw::Matrix* inverse =
        reinterpret_cast<rw::Matrix*>(skin->inverseMatrices);
    int parent = 0;
    int parentStack[64];
    int parentStackSize = 0;
    for(int bone = 1; bone < skin->numBones; ++bone)
    {
        CharacterBoneTransform& transform = transforms[bone];
        transform.parentIndex = parent;

        inverse[bone].flags = 0;
        inverse[parent].flags = 0;
        rw::Matrix bindMatrix;
        rw::Matrix::invert(&bindMatrix, &inverse[bone]);
        rw::V3d translation;
        rw::V3d::transformPoints(&translation, &bindMatrix.pos, 1,
                                 &inverse[parent]);
        transform.translation[0] = translation.x;
        transform.translation[1] = translation.y;
        transform.translation[2] = translation.z;

        const int flags = hierarchy->nodeInfo[bone].flags;
        if((flags & rw::HAnimHierarchy::PUSH) != 0 &&
           parentStackSize < static_cast<int>(
               sizeof(parentStack) / sizeof(parentStack[0])))
            parentStack[parentStackSize++] = parent;
        parent = bone;
        if((flags & rw::HAnimHierarchy::POP) != 0 && parentStackSize > 0)
            parent = parentStack[--parentStackSize];
    }
    return true;
}

void
restoreCharacterBoneTransforms(
    rw::HAnimHierarchy* hierarchy,
    const std::vector<CharacterBoneTransform>& transforms)
{
    if(hierarchy == NULL ||
       transforms.size() != static_cast<size_t>(hierarchy->numNodes))
        return;

    for(int bone = 1; bone < hierarchy->numNodes; ++bone)
    {

        if(bone == 1 || bone == 8)
            continue;

        const CharacterBoneTransform& transform = transforms[bone];
        if(transform.parentIndex < 0 ||
           transform.parentIndex >= hierarchy->numNodes)
            continue;
        rw::V3d translation =
        {
            transform.translation[0],
            transform.translation[1],
            transform.translation[2]
        };
        rw::V3d::transformPoints(
            &hierarchy->matrices[bone].pos, &translation, 1,
            &hierarchy->matrices[transform.parentIndex]);
    }
}

bool
setCharacterBindPoseMatrices(CharacterScene* scene,
                             rw::HAnimHierarchy* hierarchy)
{
    if(scene == NULL || hierarchy == NULL)
        return false;

    for(size_t clump = 0; clump < scene->clumps.size(); ++clump)
    {
        FORLIST(atomicLink, scene->clumps[clump]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            rw::Skin* skin = rw::Skin::get(atomic->geometry);
            if(skin == NULL || skin->numBones != hierarchy->numNodes ||
               rw::Skin::getHierarchy(atomic) != hierarchy)
                continue;

            rw::Matrix* inverseMatrices =
                reinterpret_cast<rw::Matrix*>(skin->inverseMatrices);
            const bool local =
                (hierarchy->flags &
                 rw::HAnimHierarchy::LOCALSPACEMATRICES) != 0;
            const rw::Matrix* atomicLtm = atomic->getFrame()->getLTM();
            for(int bone = 0; bone < skin->numBones; ++bone)
            {
                inverseMatrices[bone].flags = 0;
                rw::Matrix bindMatrix;
                rw::Matrix::invert(&bindMatrix, &inverseMatrices[bone]);
                if(local)
                    hierarchy->matrices[bone] = bindMatrix;
                else
                    rw::Matrix::mult(&hierarchy->matrices[bone],
                                     &bindMatrix, atomicLtm);
                hierarchy->matrices[bone].flags = 0;
            }
            return true;
        }
    }
    return false;
}

void
updateCharacterArchiveAttachmentTransforms(CharacterScene* scene)
{
    if(scene == NULL || !scene->assembledFromArchive ||
       scene->hierarchies.empty())
        return;
    rw::HAnimHierarchy* primary = scene->hierarchies[0];
    if(primary == NULL || primary->matrices == NULL)
        return;

    for(size_t group = 1; group < scene->attachments.size(); ++group)
    {
        const CharacterAttachment& attachment = scene->attachments[group];
        if(!attachment.active || attachment.metadata.type == 21 ||
           attachment.boneIndex < 0 ||
           attachment.boneIndex >= primary->numNodes ||
           attachment.boneFrame == NULL ||
           primary->nodeInfo == NULL ||
           primary->nodeInfo[attachment.boneIndex].frame !=
               attachment.boneFrame)
            continue;
        for(size_t local = 0; local < attachment.clumpCount; ++local)
        {
            const size_t clumpIndex = attachment.firstClump + local;
            if(clumpIndex >= scene->clumps.size())
                break;
            rw::Frame* frame = scene->clumps[clumpIndex]->getFrame();
            if(frame == NULL)
                continue;
            frame->transform(&primary->matrices[attachment.boneIndex],
                             rw::COMBINEREPLACE);
            frame->matrix.right.x *= attachment.scale[0];
            frame->matrix.right.y *= attachment.scale[0];
            frame->matrix.right.z *= attachment.scale[0];
            frame->matrix.up.x *= attachment.scale[1];
            frame->matrix.up.y *= attachment.scale[1];
            frame->matrix.up.z *= attachment.scale[1];
            frame->matrix.at.x *= attachment.scale[2];
            frame->matrix.at.y *= attachment.scale[2];
            frame->matrix.at.z *= attachment.scale[2];
            frame->matrix.optimize();
            frame->updateObjects();
            rw::HAnimHierarchy* hierarchy =
                rw::HAnimHierarchy::get(scene->clumps[clumpIndex]);
            if(hierarchy == NULL || hierarchy == primary ||
               hierarchy->matrices == NULL || hierarchy->nodeInfo == NULL ||
               (hierarchy->flags &
                rw::HAnimHierarchy::LOCALSPACEMATRICES) != 0)
                continue;

            bool primarySubset = true;
            for(int node = 0; node < hierarchy->numNodes; ++node)
                if(primary->getIndex(hierarchy->nodeInfo[node].id) < 0)
                {
                    primarySubset = false;
                    break;
                }
            if(!primarySubset)
            {

                setCharacterBindPoseMatrices(scene, hierarchy);
                continue;
            }

            for(int node = 0; node < hierarchy->numNodes; ++node)
            {
                const int primaryNode =
                    primary->getIndex(hierarchy->nodeInfo[node].id);
                hierarchy->matrices[node] = primary->matrices[primaryNode];
                hierarchy->matrices[node].flags = 0;
            }
        }
    }
}

void
updateCharacterSecondaryClumpTransform(CharacterScene* scene)
{
    if(scene != NULL && scene->assembledFromArchive)
    {
        updateCharacterArchiveAttachmentTransforms(scene);
        return;
    }
    if(scene == NULL || scene->clumps.size() < 2 ||
       scene->hierarchies.empty())
        return;

    rw::HAnimHierarchy* hierarchy = scene->hierarchies[0];
    rw::Frame* frame = scene->clumps[1]->getFrame();
    if(hierarchy == NULL || hierarchy->matrices == NULL || frame == NULL ||
       hierarchy->numNodes <= CHARACTER_SECONDARY_BONE_INDEX)
        return;

    frame->transform(
        &hierarchy->matrices[CHARACTER_SECONDARY_BONE_INDEX],
        rw::COMBINEREPLACE);
    rw::HAnimHierarchy* secondary =
        rw::HAnimHierarchy::get(scene->clumps[1]);
    if(secondary != NULL && secondary != hierarchy)
        setCharacterBindPoseMatrices(scene, secondary);
}
bool
captureCharacterMorphBase(rw::Geometry* geometry,
                          CharacterMorphGeometryData* data)
{
    if(geometry == NULL || data == NULL || geometry->numVertices <= 0 ||
       geometry->numMorphTargets <= 0 ||
       geometry->morphTargets[0].vertices == NULL)
        return false;
    if(data->baseVertices != NULL &&
       data->baseVertexCount == geometry->numVertices)
        return true;

    delete[] data->baseVertices;
    delete[] data->baseNormals;
    data->baseVertices = new(std::nothrow) rw::V3d[geometry->numVertices];
    data->baseNormals = NULL;
    data->baseVertexCount = 0;
    if(data->baseVertices == NULL)
        return false;
    memcpy(data->baseVertices, geometry->morphTargets[0].vertices,
           sizeof(rw::V3d) * geometry->numVertices);
    if(geometry->morphTargets[0].normals != NULL)
    {
        data->baseNormals =
            new(std::nothrow) rw::V3d[geometry->numVertices];
        if(data->baseNormals != NULL)
            memcpy(data->baseNormals, geometry->morphTargets[0].normals,
                   sizeof(rw::V3d) * geometry->numVertices);
    }
    data->baseVertexCount = geometry->numVertices;
    return true;
}

const rw::V3d*
characterMorphVertices(const rw::Geometry* geometry,
                       const CharacterMorphGeometryData* data, int target)
{
    if(target == 0)
        return data->baseVertices;
    if(target < 0 || target >= geometry->numMorphTargets)
        return NULL;
    return geometry->morphTargets[target].vertices;
}

const rw::V3d*
characterMorphNormals(const rw::Geometry* geometry,
                      const CharacterMorphGeometryData* data, int target)
{
    if(target == 0)
        return data->baseNormals;
    if(target < 0 || target >= geometry->numMorphTargets)
        return NULL;
    return geometry->morphTargets[target].normals;
}

bool
applyCharacterGeometryMorph(rw::Geometry* geometry, int startTarget,
                            int endTarget, float amount)
{
    CharacterMorphGeometryData* data = characterMorphGeometryData(geometry);
    if(!captureCharacterMorphBase(geometry, data))
        return false;
    const rw::V3d* start = characterMorphVertices(
        geometry, data, startTarget);
    const rw::V3d* end = characterMorphVertices(geometry, data, endTarget);
    if(start == NULL || end == NULL)
        return false;
    if(amount < 0.0f)
        amount = 0.0f;
    if(amount > 1.0f)
        amount = 1.0f;

    int32_t lockFlags = rw::Geometry::LOCKVERTICES;
    const rw::V3d* startNormals = characterMorphNormals(
        geometry, data, startTarget);
    const rw::V3d* endNormals = characterMorphNormals(
        geometry, data, endTarget);
    if(startNormals != NULL && endNormals != NULL)
        lockFlags |= rw::Geometry::LOCKNORMALS;
    geometry->lock(lockFlags);
    rw::V3d* vertices = geometry->morphTargets[0].vertices;
    for(int32_t i = 0; i < geometry->numVertices; ++i)
    {
        vertices[i].x = start[i].x + (end[i].x - start[i].x) * amount;
        vertices[i].y = start[i].y + (end[i].y - start[i].y) * amount;
        vertices[i].z = start[i].z + (end[i].z - start[i].z) * amount;
    }
    if(startNormals != NULL && endNormals != NULL)
    {
        rw::V3d* normals = geometry->morphTargets[0].normals;
        for(int32_t i = 0; i < geometry->numVertices; ++i)
        {
            normals[i].x = startNormals[i].x +
                           (endNormals[i].x - startNormals[i].x) * amount;
            normals[i].y = startNormals[i].y +
                           (endNormals[i].y - startNormals[i].y) * amount;
            normals[i].z = startNormals[i].z +
                           (endNormals[i].z - startNormals[i].z) * amount;
        }
    }
    geometry->unlock();
    return true;
}

bool
applyCharacterClumpMorph(rw::Clump* clump, int startTarget, int endTarget,
                         float amount)
{
    if(clump == NULL)
        return false;
    bool applied = false;
    FORLIST(atomicLink, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
        if(applyCharacterGeometryMorph(atomic->geometry, startTarget,
                                       endTarget, amount))
            applied = true;
    }
    return applied;
}

}

void
updateCharacterSceneAttachmentTransforms(CharacterScene* scene)
{
    updateCharacterSecondaryClumpTransform(scene);
}

CharacterSceneModelMode characterSceneModelMode(const char* resourceName)
{
    if(resourceName != NULL && resourceName[0] >= '0' &&
       resourceName[0] <= '9')
    {
        const char* suffix = resourceName;
        while(*suffix >= '0' && *suffix <= '9')
            ++suffix;
        if(*suffix == 'H')
            return CHARACTER_SCENE_MODEL_ARCHIVE;
    }
    return CHARACTER_SCENE_MODEL_DIRECT;
}

bool
applyCharacterSceneMorph(CharacterScene* scene, int attachmentType,
                         int startTarget, int endTarget, float amount)
{
    if(scene == NULL)
        return false;
    bool applied = false;
    bool matchedAttachment = false;
    if(scene->assembledFromArchive)
    {
        for(size_t group = 0; group < scene->attachments.size(); ++group)
        {
            const CharacterAttachment& attachment = scene->attachments[group];
            if(!attachment.active ||
               static_cast<int>(attachment.metadata.type) != attachmentType)
                continue;
            matchedAttachment = true;
            for(size_t local = 0; local < attachment.clumpCount; ++local)
            {
                const size_t clump = attachment.firstClump + local;
                if(clump >= scene->clumps.size())
                    break;
                if(applyCharacterClumpMorph(scene->clumps[clump], startTarget,
                                            endTarget, amount))
                    applied = true;
            }
        }
        if(matchedAttachment)
            return applied;
    }
    for(size_t clump = 0; clump < scene->clumps.size(); ++clump)
        if(applyCharacterClumpMorph(scene->clumps[clump], startTarget,
                                    endTarget, amount))
            applied = true;
    return applied;
}

bool
applyCharacterSceneLip(CharacterScene* scene, int lipInterpolator,
                       float lipRate, float elapsedSeconds)
{
    if(scene == NULL || lipInterpolator < 0 || lipRate <= 0.0f)
        return false;
    bool applied = false;
    for(size_t clump = 0; clump < scene->clumps.size(); ++clump)
    {
        rw::Clump* actorClump = scene->clumps[clump];
        FORLIST(atomicLink, actorClump->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            CharacterMorphGeometryData* data =
                characterMorphGeometryData(atomic->geometry);
            if(data == NULL || data->interpolators == NULL ||
               lipInterpolator >= data->count)
                continue;

            int index = lipInterpolator;
            float time = elapsedSeconds * lipRate * 2.0f;
            for(int step = 0; step < data->count * 2; ++step)
            {
                const CharacterMorphInterpolator& interpolator =
                    data->interpolators[index];
                if(interpolator.duration <= 0.0f ||
                   time < interpolator.duration)
                {
                    const float amount = interpolator.duration > 0.0f ?
                        time * interpolator.reciprocalDuration : 0.0f;
                    if(applyCharacterGeometryMorph(
                           atomic->geometry, interpolator.startTarget,
                           interpolator.endTarget, amount))
                        applied = true;
                    break;
                }
                time -= interpolator.duration;
                if(interpolator.next == index)
                {
                    time = fmodf(time, interpolator.duration);
                    const float amount =
                        time * interpolator.reciprocalDuration;
                    if(applyCharacterGeometryMorph(
                           atomic->geometry, interpolator.startTarget,
                           interpolator.endTarget, amount))
                        applied = true;
                    break;
                }
                index = interpolator.next;
            }
        }
    }
    return applied;
}

static bool
loadCharacterNpcTextureTables(const char* executablePath)
{
    if(executablePath == NULL)
        return false;
    FILE* file = fopen(executablePath, "rb");
    if(file == NULL)
        return false;

    std::vector<uint8_t> textureTable(
        CHARACTER_NPC_RESOURCE_COUNT * CHARACTER_NPC_TEXTURE_COUNT *
        sizeof(int16_t));
    std::vector<uint8_t> replacementTable(CHARACTER_NPC_RESOURCE_COUNT);

    bool ok =
        fseek(file, CHARACTER_NPC_TEXTURE_TABLE_FILE_OFFSET, SEEK_SET) == 0 &&
        fread(textureTable.data(), 1, textureTable.size(), file) ==
            textureTable.size();

    ok = ok &&
        fseek(file, CHARACTER_NPC_REPLACEMENT_TABLE_FILE_OFFSET, SEEK_SET) ==
            0 &&
        fread(replacementTable.data(), 1, replacementTable.size(), file) ==
            replacementTable.size();
    fclose(file);
    if(!ok)
        return false;
    g_characterNpcTextureTable.swap(textureTable);
    g_characterNpcReplacementTable.swap(replacementTable);
    return true;
}

bool
loadCharacterArchiveMetadata(const char* executablePath)
{
    g_characterArchiveMetadata.clear();
    g_characterNpcTextureTable.clear();
    g_characterNpcReplacementTable.clear();
    for(size_t family = 0; family < 3; ++family)
        g_characterCostumeTables[family].clear();
    if(executablePath == NULL)
        return false;
    FILE* file = fopen(executablePath, "rb");
    if(file == NULL)
        return false;
    const size_t byteCount =
        CHARACTER_ARCHIVE_METADATA_COUNT * sizeof(CharacterArchiveMetadata);
    std::vector<uint8_t> bytes(byteCount);
    bool ok =
        fseek(file, CHARACTER_ARCHIVE_METADATA_FILE_OFFSET, SEEK_SET) == 0 &&
        fread(bytes.data(), 1, byteCount, file) == byteCount;
    for(size_t family = 0; ok && family < 3; ++family)
    {
        std::vector<uint8_t>& table =
            g_characterCostumeTables[family];
        table.resize(CHARACTER_COSTUME_VARIANT_COUNT *
                     CHARACTER_COSTUME_VARIANT_SIZE);
        ok =
            fseek(file, CHARACTER_COSTUME_TABLE_FILE_OFFSETS[family],
                  SEEK_SET) == 0 &&
            fread(table.data(), 1, table.size(), file) == table.size();
    }
    fclose(file);
    if(!ok)
        return false;
    g_characterArchiveMetadata.swap(bytes);
    if(!loadCharacterNpcTextureTables(executablePath))
    {
        g_characterArchiveMetadata.clear();
        for(size_t family = 0; family < 3; ++family)
            g_characterCostumeTables[family].clear();
        return false;
    }
    return true;
}

bool
characterArchiveMetadata(uint32_t archiveId,
                         CharacterArchiveMetadata& metadata)
{
    memset(&metadata, 0, sizeof(metadata));
    if(archiveId >= CHARACTER_ARCHIVE_METADATA_COUNT ||
       g_characterArchiveMetadata.size() !=
           CHARACTER_ARCHIVE_METADATA_COUNT *
               sizeof(CharacterArchiveMetadata))
        return false;
    const uint8_t* record =
        g_characterArchiveMetadata.data() +
        archiveId * sizeof(CharacterArchiveMetadata);
    metadata.type = read32(record);
    metadata.objectTag = read32(record + 4);
    metadata.flags = read32(record + 8);
    metadata.callbackAddress = read32(record + 12);
    metadata.resourceOrdinal = read16(record + 16);
    metadata.auxiliaryOrdinal = read16(record + 18);
    for(size_t i = 0; i < CHARACTER_ARCHIVE_TEXTURE_COUNT; ++i)
    {
        metadata.textureDependencies[i] =
            static_cast<int16_t>(read16(record + 20 + i * 2));
    }
    return true;
}

uint64_t
characterCustomizationFingerprint(
    const CharacterCustomization& customization)
{
    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(&customization);
    uint64_t hash = 1469598103934665603ull;
    for(size_t i = 0; i < sizeof(customization); ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}
void
normalizeInventoryEquipRoute(int32_t& menuCategory, int32_t& categorySlot)
{
    if(menuCategory == 3)
    {
        switch(categorySlot)
        {
        case 1:
        case 3:
        case 6:
        case 10:
        case 12:
        case 16:
            --categorySlot;
            break;
        case 18:
            categorySlot = 17;
            break;
        default:
            break;
        }
    }
    else if(menuCategory == 2)
    {
        if(categorySlot == 1 || categorySlot == 3 ||
           categorySlot == 9 || categorySlot == 12)
            --categorySlot;
        else if(categorySlot == 4 || categorySlot == 5 ||
                categorySlot == 6 || categorySlot == 7 ||
                categorySlot == 10 || categorySlot == 13 ||
                categorySlot == 14 || categorySlot == 15)
        {
            menuCategory = 0;
            categorySlot = 0;
        }
    }
    else if(menuCategory == 1)
    {
        if(categorySlot == 2 || categorySlot == 4)
            --categorySlot;
        else if(categorySlot >= 5 && categorySlot <= 8)
            categorySlot = 0;
    }
}

static bool
selectCharacterArchiveResource(
    const InventoryItemDefinition& definition, uint8_t variant,
    uint32_t type, uint32_t salt, uint32_t& archiveId)
{
    CharacterArchiveMetadata metadata;
    if(definition.field_0x14 >= 3 &&
       characterArchiveMetadata(
           static_cast<uint32_t>(definition.field_0x14), metadata) &&
       metadata.type == type)
    {
        archiveId = static_cast<uint32_t>(definition.field_0x14);
        return true;
    }

    uint32_t candidates[CHARACTER_ARCHIVE_METADATA_COUNT];
    uint32_t candidateCount = 0;
    for(uint32_t id = 3; id < CHARACTER_ARCHIVE_METADATA_COUNT; ++id)
    {
        if(characterArchiveMetadata(id, metadata) && metadata.type == type)
            candidates[candidateCount++] = id;
    }
    if(candidateCount == 0)
        return false;
    uint32_t selector = static_cast<uint32_t>(definition.itemId);
    selector = selector * 33u +
        static_cast<uint32_t>(definition.field_0x14);
    selector = selector * 33u +
        static_cast<uint32_t>(definition.field_0x18);
    selector = selector * 33u +
        static_cast<uint32_t>(definition.field_0x1c);
    selector = selector * 33u +
        static_cast<uint32_t>(definition.field_0x20);
    selector = selector * 33u +
        static_cast<uint32_t>(definition.variantSelector);
    selector += static_cast<uint32_t>(variant) + salt;
    archiveId = candidates[selector % candidateCount];
    return true;
}

static bool
regenerateCharacterArchiveGroup(
    const InventoryItemDefinition& definition, uint8_t variant,
    uint32_t type, uint32_t salt, CharacterArchiveGroup& group)
{
    uint32_t archiveId = 0;
    if(!selectCharacterArchiveResource(
           definition, variant, type, salt, archiveId))
        return false;
    CharacterArchiveMetadata metadata;
    if(!characterArchiveMetadata(archiveId, metadata))
        return false;
    group.archiveId = archiveId;
    int slot = 0;
    for(; slot < CHARACTER_ARCHIVE_TEXTURE_COUNT; ++slot)
    {
        const int16_t dependency = metadata.textureDependencies[slot];
        group.textureIds[slot] = dependency;
        if(dependency < 0)
            break;
    }
    if(slot == CHARACTER_ARCHIVE_TEXTURE_COUNT)
        return true;
    for(++slot; slot < CHARACTER_ARCHIVE_TEXTURE_COUNT; ++slot)
        group.textureIds[slot] = 0;
    return true;
}

bool
applyInventoryEquipToCustomization(
    const InventoryItemDefinition& definition, uint8_t variant,
    CharacterCustomization& customization, uint32_t& changedGroupMask)
{
    changedGroupMask = 0;
    const uint32_t itemCategory = definition.itemId / 100;
    if(definition.consumable ||
       (itemCategory >= 0x21 && itemCategory <= 0x24) ||
       definition.modelPart < 0 ||
       definition.modelPart >= CHARACTER_ARCHIVE_ENTRY_COUNT ||
       definition.modelPart == 3 || definition.modelPart == 8)
        return false;

    int32_t menuCategory = definition.menuCategory;
    int32_t categorySlot = definition.categorySlot;
    normalizeInventoryEquipRoute(menuCategory, categorySlot);

    uint32_t types[4];
    size_t typeCount = 0;
    types[typeCount++] = static_cast<uint32_t>(definition.modelPart);
    const uint32_t primaryType =
        static_cast<uint32_t>(definition.modelPart);
    if(primaryType == 5 || primaryType == 6)
    {
        types[0] = 5;
        types[typeCount++] = 6;
    }
    else if(primaryType == 7)
        types[typeCount++] = 8;
    if(itemCategory == INVENTORY_CATEGORY_LEG_PANTS_COUPLING)
    {
        types[0] = 19;
        typeCount = 1;
        types[typeCount++] = 7;
        types[typeCount++] = 8;
    }
    if(definition.menuCategory == 3 && categorySlot == 17)
    {
        types[0] = 18;
        typeCount = 1;
        types[typeCount++] = 19;
    }

    CharacterCustomization working = customization;
    for(size_t i = 0; i < typeCount; ++i)
    {
        const uint32_t type = types[i];
        if(type >= CHARACTER_ARCHIVE_ENTRY_COUNT)
            return false;
        if(!regenerateCharacterArchiveGroup(
               definition, variant, type, static_cast<uint32_t>(i * 17),
               working.archiveEntries[type]))
            return false;
        changedGroupMask |= 1u << type;
    }
    if(menuCategory == 0 && (categorySlot == 0 || categorySlot == 1))
        working.costumeVariant = variant;
    customization = working;
    return changedGroupMask != 0;
}

rw::Texture*
loadStxTexture(const char* archivePath, const char* entryName)
{
    if(archivePath == NULL || entryName == NULL)
        return NULL;

    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, entryName, data))
        return NULL;

    const uint8_t* imageChunk = data.data();
    uint32_t imageLength = static_cast<uint32_t>(data.size());
    if(data.size() >= 12 && read32(data.data()) == CHUNK_IMAGE)
    {
        const uint32_t rootLength = read32(data.data() + 4);
        if(static_cast<uint64_t>(rootLength) + 12 != data.size())
            return NULL;
        imageChunk += 12;
        imageLength = rootLength;
    }
    rw::Raster* raster = readImageRaster(imageChunk, imageLength);
    if(raster == NULL)
        return NULL;

    rw::Texture* texture = rw::Texture::create(raster);
    if(texture == NULL)
    {
        raster->destroy();
        return NULL;
    }
    strncpy(texture->name, entryName, sizeof(texture->name) - 1);
    texture->name[sizeof(texture->name) - 1] = '\0';
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}
static rw::Texture*
loadStxTextureByIndex(const char* archivePath, const XafEntryInfo& entry)
{
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, entry, data))
        return NULL;
    const uint8_t* imageChunk = data.data();
    uint32_t imageLength = static_cast<uint32_t>(data.size());
    if(data.size() >= 12 && read32(data.data()) == CHUNK_IMAGE)
    {
        const uint32_t rootLength = read32(data.data() + 4);
        if(static_cast<uint64_t>(rootLength) + 12 != data.size())
            return NULL;
        imageChunk += 12;
        imageLength = rootLength;
    }
    rw::Raster* raster = readImageRaster(imageChunk, imageLength);
    if(raster == NULL)
        return NULL;
    rw::Texture* texture = rw::Texture::create(raster);
    if(texture == NULL)
    {
        raster->destroy();
        return NULL;
    }
    strncpy(texture->name, entry.name.c_str(), sizeof(texture->name) - 1);
    texture->name[sizeof(texture->name) - 1] = '\0';
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}

static void
textureStem(const char* path, char stem[32])
{
    stem[0] = '\0';
    if(path == NULL)
        return;
    const char* base = path;
    for(const char* cursor = path; *cursor != '\0'; ++cursor)
        if(*cursor == '/' || *cursor == '\\')
            base = cursor + 1;
    size_t length = 0;
    while(base[length] != '\0' && base[length] != '.' &&
          length + 1 < 32)
    {
        stem[length] =
            static_cast<char>(toupper(static_cast<unsigned char>(base[length])));
        ++length;
    }
    stem[length] = '\0';
}

static bool
textureNameEqual(const char* left, const char* right)
{
    char leftStem[32];
    char rightStem[32];
    textureStem(left, leftStem);
    textureStem(right, rightStem);
    return strcmp(leftStem, rightStem) == 0;
}

static bool
prepareCharacterNpcTextureMetadata(
    const char* archivePath, const std::vector<XafEntryInfo>*& dataEntries)
{
    dataEntries = NULL;
    if(archivePath == NULL)
        return false;

    const std::filesystem::path cdataPath(archivePath);
    if(g_characterNpcTextureTable.size() !=
           CHARACTER_NPC_RESOURCE_COUNT * CHARACTER_NPC_TEXTURE_COUNT *
               sizeof(int16_t) ||
       g_characterNpcReplacementTable.size() !=
           CHARACTER_NPC_RESOURCE_COUNT)
    {
        const std::filesystem::path executablePath =
            cdataPath.parent_path().parent_path() / "SLES_535.05";
        const std::string executable = executablePath.string();
        if(!loadCharacterNpcTextureTables(executable.c_str()))
            return false;
    }

    const std::filesystem::path dataPath =
        cdataPath.parent_path() / "DATA.XAF";
    const std::string normalizedDataPath =
        dataPath.lexically_normal().string();
    if(g_characterNpcDataEntries.empty() ||
       g_characterNpcDataPath != normalizedDataPath)
    {
        std::vector<XafEntryInfo> entries;
        if(!listXafEntries(normalizedDataPath.c_str(), entries))
            return false;
        g_characterNpcDataEntries.swap(entries);
        g_characterNpcDataPath = normalizedDataPath;
    }
    dataEntries = &g_characterNpcDataEntries;
    return true;
}

static int16_t
characterNpcTextureDependency(uint16_t resourceId, size_t imageIndex)
{
    if(resourceId >= CHARACTER_NPC_RESOURCE_COUNT ||
       imageIndex >= CHARACTER_NPC_TEXTURE_COUNT ||
       g_characterNpcTextureTable.size() !=
           CHARACTER_NPC_RESOURCE_COUNT * CHARACTER_NPC_TEXTURE_COUNT *
               sizeof(int16_t))
    {
        return -1;
    }
    const size_t offset =
        (static_cast<size_t>(resourceId) * CHARACTER_NPC_TEXTURE_COUNT +
         imageIndex) * sizeof(int16_t);
    return static_cast<int16_t>(
        read16(g_characterNpcTextureTable.data() + offset));
}

static bool
createCharacterNpcTextures(
    uint16_t resourceId, rw::TexDictionary* dictionary,
    const std::vector<XafEntryInfo>& dataEntries,
    std::vector<rw::Raster*>& rasters,
    std::vector<rw::Texture*>& imageTextures)
{
    imageTextures.clear();
    if(dictionary == NULL || rasters.size() > CHARACTER_NPC_TEXTURE_COUNT)
        return false;
    imageTextures.reserve(rasters.size());
    for(size_t imageIndex = 0; imageIndex < rasters.size(); ++imageIndex)
    {
        const int16_t dependency =
            characterNpcTextureDependency(resourceId, imageIndex);
        if(dependency < 0 ||
           static_cast<size_t>(dependency) >= dataEntries.size())
        {
            return false;
        }

        char name[32];
        textureStem(dataEntries[dependency].name.c_str(), name);
        if(name[0] == '\0')
            return false;

        rw::Texture* texture = dictionary->find(name);
        if(texture != NULL)
        {

            rasters[imageIndex]->destroy();
            rasters[imageIndex] = NULL;
            imageTextures.push_back(texture);
            continue;
        }

        texture = rw::Texture::create(rasters[imageIndex]);
        if(texture == NULL)
            return false;
        rasters[imageIndex] = NULL;
        strncpy(texture->name, name, sizeof(texture->name) - 1);
        texture->name[sizeof(texture->name) - 1] = '\0';
        texture->setFilter(rw::Texture::LINEAR);
        dictionary->add(texture);
        imageTextures.push_back(texture);
    }
    return true;
}

static void
replaceCharacterNpcTextures(
    CharacterScene& scene, const std::vector<rw::Texture*>& imageTextures,
    bool replaceTextures)
{
    if(!replaceTextures || imageTextures.size() < 8 ||
       scene.clumps.empty())
    {
        return;
    }

    FORLIST(atomicLink, scene.clumps[0]->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
        rw::Geometry* geometry = atomic->geometry;
        for(int32_t materialIndex = 0;
            materialIndex < geometry->matList.numMaterials;
            ++materialIndex)
        {
            rw::Material* material =
                geometry->matList.materials[materialIndex];
            if(material == NULL || material->texture == NULL)
                continue;
            for(size_t slot = 0; slot < 4; ++slot)
            {
                if(imageTextures[slot] != NULL &&
                   imageTextures[slot + 4] != NULL &&
                   textureNameEqual(material->texture->name,
                                    imageTextures[slot]->name))
                {
                    material->setTexture(imageTextures[slot + 4]);
                    break;
                }
            }
        }
    }

    if(scene.clumps.size() < 2 || imageTextures[7] == NULL)
        return;

    FORLIST(atomicLink, scene.clumps[1]->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
        rw::Geometry* geometry = atomic->geometry;
        for(int32_t materialIndex = 0;
            materialIndex < geometry->matList.numMaterials;
            ++materialIndex)
        {
            rw::Material* material =
                geometry->matList.materials[materialIndex];
            if(material != NULL && material->texture != NULL)
                material->setTexture(imageTextures[7]);
        }
    }
}

static bool
validateCharacterNpcTextures(
    const CharacterScene& scene,
    const std::vector<rw::Texture*>& imageTextures,
    const char* entryName)
{
    bool valid = true;
    for(size_t clumpIndex = 0;
        clumpIndex < scene.clumps.size(); ++clumpIndex)
    {
        size_t atomicIndex = 0;
        FORLIST(atomicLink, scene.clumps[clumpIndex]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            rw::Geometry* geometry = atomic->geometry;
            for(int32_t materialIndex = 0;
                materialIndex < geometry->matList.numMaterials;
                ++materialIndex)
            {
                rw::Material* material =
                    geometry->matList.materials[materialIndex];
                if(material == NULL || material->texture == NULL)
                    continue;
                bool packageTexture = false;
                for(size_t imageIndex = 0;
                    imageIndex < imageTextures.size(); ++imageIndex)
                {
                    if(material->texture == imageTextures[imageIndex])
                    {
                        packageTexture =
                            imageTextures[imageIndex]->raster != NULL;
                        break;
                    }
                }
                if(packageTexture)
                    continue;
                printf(
                    "ERROR: %s clump %u atomic %u material %d requested "
                    "unresolved texture '%s'.\n",
                    entryName,
                    static_cast<unsigned>(clumpIndex),
                    static_cast<unsigned>(atomicIndex), materialIndex,
                    material->texture->name);
                valid = false;
            }
            ++atomicIndex;
        }
    }
    return valid;
}

static void
replaceAttachmentTextures(
    CharacterScene& scene, const CharacterAttachment& attachment,
    const CharacterArchiveGroup& group, const char* dataPath,
    const std::vector<XafEntryInfo>& dataEntries)
{
    rw::Texture* replacements[CHARACTER_ARCHIVE_TEXTURE_COUNT] = {};
    for(int slot = 0; slot < CHARACTER_ARCHIVE_TEXTURE_COUNT; ++slot)
    {
        const int16_t textureId = group.textureIds[slot];
        if(textureId < 0)
            break;
        if(static_cast<size_t>(textureId) >= dataEntries.size())
            continue;
        replacements[slot] = loadStxTextureByIndex(
            dataPath, dataEntries[textureId]);
    }

    for(size_t local = 0; local < attachment.clumpCount; ++local)
    {
        const size_t clumpIndex = attachment.firstClump + local;
        if(clumpIndex >= scene.clumps.size())
            break;
        FORLIST(atomicLink, scene.clumps[clumpIndex]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            rw::Geometry* geometry = atomic->geometry;
            for(int32_t materialIndex = 0;
                materialIndex < geometry->matList.numMaterials;
                ++materialIndex)
            {
                rw::Material* material =
                    geometry->matList.materials[materialIndex];
                if(material == NULL || material->texture == NULL)
                    continue;
                for(int slot = 0;
                    slot < CHARACTER_ARCHIVE_TEXTURE_COUNT; ++slot)
                {
                    const int16_t dependency =
                        attachment.metadata.textureDependencies[slot];
                    if(dependency < 0)
                        break;
                    if(replacements[slot] == NULL ||
                       static_cast<size_t>(dependency) >= dataEntries.size())
                        continue;
                    if(textureNameEqual(
                           material->texture->name,
                           dataEntries[dependency].name.c_str()))
                    {
                        material->setTexture(replacements[slot]);
                        break;
                    }
                }
            }
        }
    }
    for(int slot = 0; slot < CHARACTER_ARCHIVE_TEXTURE_COUNT; ++slot)
        if(replacements[slot] != NULL)
            replacements[slot]->destroy();
}

static bool
decodeCharacterArchiveGroup(
    const char* archivePath, const char* dataPath,
    const std::vector<XafEntryInfo>& dataEntries,
    const CharacterArchiveGroup& group, uint32_t groupIndex,
    CharacterScene& scene)
{
    CharacterAttachment attachment;
    attachment.archiveId = group.archiveId;
    if(!characterArchiveMetadata(group.archiveId, attachment.metadata))
        return false;
    attachment.firstClump = scene.clumps.size();

    std::vector<uint8_t> resource;
    if(!readXafEntryByIndex(
           archivePath,
           group.archiveId + CHARACTER_ARCHIVE_RESOURCE_BASE, resource))
        return false;

    rw::TexDictionary* dictionary = NULL;
    for(size_t offset = 0; offset + 12 <= resource.size();)
    {
        const uint32_t type = read32(resource.data() + offset);
        const uint32_t length = read32(resource.data() + offset + 4);
        const uint64_t end = static_cast<uint64_t>(offset) + 12 + length;
        if(end > resource.size())
            return false;
        if(type == rw::ID_TEXDICTIONARY && dictionary == NULL)
        {
            rw::StreamMemory stream;
            stream.open(resource.data() + offset,
                        static_cast<uint32_t>(12 + length));
            if(!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, NULL, NULL))
                return false;
            dictionary = rw::TexDictionary::streamRead(&stream);
            if(dictionary == NULL)
                return false;
            scene.attachmentTextureDictionaries.push_back(dictionary);
        }
        offset = static_cast<size_t>(end);
    }

    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();
    if(dictionary != NULL)
        rw::TexDictionary::setCurrent(dictionary);
    bool valid = true;
    for(size_t offset = 0; offset + 12 <= resource.size();)
    {
        const uint32_t type = read32(resource.data() + offset);
        const uint32_t length = read32(resource.data() + offset + 4);
        const uint64_t end = static_cast<uint64_t>(offset) + 12 + length;
        if(end > resource.size())
        {
            valid = false;
            break;
        }
        if(type == CHUNK_CLUMP)
        {
            rw::StreamMemory stream;
            stream.open(resource.data() + offset,
                        static_cast<uint32_t>(12 + length));
            if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
            {
                valid = false;
                break;
            }
            rw::Clump* clump = rw::Clump::streamRead(&stream);
            if(clump == NULL)
            {
                valid = false;
                break;
            }
            prepareCharacterRenderClump(clump);
            scene.clumps.push_back(clump);
        }
        offset = static_cast<size_t>(end);
    }
    rw::TexDictionary::setCurrent(previous);
    if(!valid || scene.clumps.size() == attachment.firstClump)
        return false;

    attachment.clumpCount = scene.clumps.size() - attachment.firstClump;
    attachment.active = true;
    scene.attachments[groupIndex] = attachment;
    replaceAttachmentTextures(scene, scene.attachments[groupIndex], group,
                              dataPath, dataEntries);
    return true;
}
static bool
characterHierarchiesCompatible(const rw::HAnimHierarchy* first,
                               const rw::HAnimHierarchy* second)
{
    if(first == NULL || second == NULL ||
       first->numNodes != second->numNodes ||
       first->nodeInfo == NULL || second->nodeInfo == NULL)
        return false;
    for(int node = 0; node < first->numNodes; ++node)
        if(first->nodeInfo[node].id != second->nodeInfo[node].id)
            return false;
    return true;
}

static bool
finalizeCharacterArchiveScene(CharacterScene& scene)
{
    rw::HAnimHierarchy* primary = NULL;
    std::vector<CharacterBoneTransform> boneTransforms;

    for(size_t i = 0; i < scene.clumps.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy =
            rw::HAnimHierarchy::get(scene.clumps[i]);
        if(hierarchy == NULL)
            continue;
        hierarchy->attach();
        hierarchy->flags |= rw::HAnimHierarchy::UPDATEMODELLINGMATRICES;

        setCharacterBindPoseMatrices(&scene, hierarchy);
        if(primary != NULL)
            continue;
        FORLIST(atomicLink, scene.clumps[i]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            rw::Skin* skin = rw::Skin::get(atomic->geometry);
            if(skin == NULL || skin->numBones != hierarchy->numNodes)
                continue;
            primary = hierarchy;
            if(!buildCharacterBoneTransforms(primary, skin, boneTransforms))
                return false;
            break;
        }
    }
    if(primary == NULL)
        return false;

    if(primary == NULL || boneTransforms.empty())
        return false;
    scene.hierarchies.push_back(primary);
    scene.hierarchyBoneTransforms.push_back(boneTransforms);

    for(size_t i = 0; i < scene.attachments.size(); ++i)
    {
        CharacterAttachment& attachment = scene.attachments[i];
        if(!attachment.active)
            continue;
        if(attachment.metadata.objectTag >=
               static_cast<uint32_t>(primary->numNodes) ||
           primary->nodeInfo == NULL)
            return false;
        attachment.boneIndex =
            static_cast<int32_t>(attachment.metadata.objectTag);

        attachment.boneFrame =
            primary->nodeInfo[attachment.boneIndex].frame;
        if(attachment.boneFrame == NULL)
            return false;
    }
    bool haveBounds = false;
    rw::V3d minimum = { 0.0f, 0.0f, 0.0f };
    rw::V3d maximum = { 0.0f, 0.0f, 0.0f };
    const CharacterAttachment& body = scene.attachments[0];
    for(size_t local = 0; local < body.clumpCount; ++local)
    {
        const size_t clumpIndex = body.firstClump + local;
        if(clumpIndex >= scene.clumps.size())
            break;
        scene.clumps[clumpIndex]->getFrame()->updateObjects();
        FORLIST(atomicLink, scene.clumps[clumpIndex]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            const rw::Sphere* sphere = atomic->getWorldBoundingSphere();
            const rw::V3d low =
            {
                sphere->center.x - sphere->radius,
                sphere->center.y - sphere->radius,
                sphere->center.z - sphere->radius
            };
            const rw::V3d high =
            {
                sphere->center.x + sphere->radius,
                sphere->center.y + sphere->radius,
                sphere->center.z + sphere->radius
            };
            if(!haveBounds)
            {
                minimum = low;
                maximum = high;
                haveBounds = true;
            }
            else
            {
                minimum.x = fminf(minimum.x, low.x);
                minimum.y = fminf(minimum.y, low.y);
                minimum.z = fminf(minimum.z, low.z);
                maximum.x = fmaxf(maximum.x, high.x);
                maximum.y = fmaxf(maximum.y, high.y);
                maximum.z = fmaxf(maximum.z, high.z);
            }
        }
    }
    if(haveBounds)
    {
        scene.boundsCenter[0] = (minimum.x + maximum.x) * 0.5f;
        scene.boundsCenter[1] = (minimum.y + maximum.y) * 0.5f;
        scene.boundsCenter[2] = (minimum.z + maximum.z) * 0.5f;
        const float dx = maximum.x - minimum.x;
        const float dy = maximum.y - minimum.y;
        const float dz = maximum.z - minimum.z;
        scene.boundsRadius = sqrtf(dx * dx + dy * dy + dz * dz) * 0.5f;
    }

    updateCharacterArchiveAttachmentTransforms(&scene);
    return true;
}
bool
applyCharacterCostumeVariant(
    const CharacterCustomization& customization, CharacterScene* scene)
{
    if(scene == NULL ||
       scene->attachments.size() != CHARACTER_ARCHIVE_ENTRY_COUNT)
        return false;
    const size_t family = customization.variantFlags != 0 ? 0 :
        (customization.genderSelector == 0 ? 1 : 2);
    const size_t variant = customization.costumeVariant;
    if(family >= 3 ||
       g_characterCostumeTables[family].size() !=
           CHARACTER_COSTUME_VARIANT_COUNT *
               CHARACTER_COSTUME_VARIANT_SIZE)
        return false;
    const uint8_t* record = variant < CHARACTER_COSTUME_VARIANT_COUNT ?
        g_characterCostumeTables[family].data() +
            variant * CHARACTER_COSTUME_VARIANT_SIZE : NULL;
    for(size_t group = 0;
        group < CHARACTER_ARCHIVE_ENTRY_COUNT; ++group)
    {
        CharacterAttachment& attachment = scene->attachments[group];
        const uint32_t type = attachment.metadata.type;
        const bool clothingScale = type == 4 || type == 7 ||
            type == 14 || type == 19 || type == 10;
        for(size_t axis = 0; axis < 3; ++axis)
        {
            attachment.scale[axis] = 1.0f;

            if(record != NULL && clothingScale)
                memcpy(&attachment.scale[axis],
                       record + (group * 3 + axis) * sizeof(float),
                       sizeof(float));
        }
    }
    updateCharacterArchiveAttachmentTransforms(scene);
    return true;
}

bool
loadCharacterArchiveScene(const char* archivePath, const char* dataPath,
                          const CharacterCustomization& customization,
                          rw::World* world, CharacterScene* scene)
{
    if(archivePath == NULL || dataPath == NULL || world == NULL ||
       scene == NULL || g_characterArchiveMetadata.empty())
        return false;

    std::vector<XafEntryInfo> dataEntries;
    if(!listXafEntries(dataPath, dataEntries))
        return false;

    CharacterScene replacement;
    replacement.attachments.resize(CHARACTER_ARCHIVE_ENTRY_COUNT);
    replacement.assembledFromArchive = true;
    replacement.archiveFingerprint =
        characterCustomizationFingerprint(customization);
    for(uint32_t group = 0;
        group < CHARACTER_ARCHIVE_ENTRY_COUNT; ++group)
    {
        const CharacterArchiveGroup& archiveGroup =
            customization.archiveEntries[group];

        if(static_cast<int32_t>(archiveGroup.archiveId) <= 0)
            continue;
        if(!decodeCharacterArchiveGroup(
               archivePath, dataPath, dataEntries, archiveGroup, group,
               replacement))
        {
            freeCharacterScene(&replacement);
            return false;
        }
    }
    if(replacement.clumps.empty() ||
       !replacement.attachments[0].active ||
       !finalizeCharacterArchiveScene(replacement))
    {
        freeCharacterScene(&replacement);
        return false;
    }
    if(!applyCharacterCostumeVariant(customization, &replacement))
    {
        freeCharacterScene(&replacement);
        return false;
    }

    freeCharacterScene(scene);
    *scene = std::move(replacement);
    scene->world = world;
    for(size_t i = 0; i < scene->clumps.size(); ++i)
        world->addClump(scene->clumps[i]);
    return true;
}

rw::Texture*
createPalModeTexture(const PalModePackage& package, int imageId)
{
    if(!package.valid || imageId < 0 || imageId >= PALMODE_IMAGE_COUNT)
        return NULL;
    const PalModeImage& image = package.images[imageId];
    if(image.chunkOffset + image.chunkLength > package.data.size())
        return NULL;

    rw::Raster* raster = readImageRaster(
        package.data.data() + image.chunkOffset,
        static_cast<uint32_t>(image.chunkLength));
    if(raster == NULL)
        return NULL;
    rw::Texture* texture = rw::Texture::create(raster);
    if(texture == NULL)
    {
        raster->destroy();
        return NULL;
    }
    strncpy(texture->name, CHARACTER_PALMODE_TEXTURE_NAMES[imageId], sizeof(texture->name) - 1);
    texture->name[sizeof(texture->name) - 1] = '\0';
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}

CharacterScene::CharacterScene()
    : basicAnimation(SIZE_MAX), activeAnimation(0),
      animationState(0xffffffffu), animationRate(1.0f),
      animationLoop(true), animationStartTime(0.0f),
      animationEndTime(0.0f), animationTransitionDuration(0.0f),
      animationTransitionRemaining(0.0f), textureDictionary(NULL), world(NULL),
      boundsRadius(0.0f)
{
    boundsCenter[0] = boundsCenter[1] = boundsCenter[2] = 0.0f;
}

bool
loadCharacterShadowScene(const char* archivePath, CharacterScene* scene)
{
    const uint32_t SHADOW_DFF_ENTRY = 2648;
    if(archivePath == NULL || scene == NULL)
        return false;

    CharacterScene replacement;
    if(!readXafEntryByIndex(
           archivePath, SHADOW_DFF_ENTRY, replacement.data))
    {
        return false;
    }

    rw::StreamMemory stream;
    stream.open(replacement.data.data(),
                static_cast<uint32_t>(replacement.data.size()));
    if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
        return false;
    rw::Clump* clump = rw::Clump::streamRead(&stream);
    if(clump == NULL)
        return false;
    prepareCharacterRenderClump(clump);
    replacement.clumps.push_back(clump);

    rw::HAnimHierarchy* hierarchy = rw::HAnimHierarchy::get(clump);
    if(hierarchy == NULL || hierarchy->matrices == NULL)
    {
        freeCharacterScene(&replacement);
        return false;
    }
    hierarchy->attach();
    hierarchy->flags |= rw::HAnimHierarchy::UPDATEMODELLINGMATRICES;
    replacement.hierarchies.push_back(hierarchy);

    freeCharacterScene(scene);
    *scene = std::move(replacement);
    return true;
}

bool
copyCharacterPoseToShadowScene(const CharacterScene* source,
                               CharacterScene* shadow)
{
    if(source == NULL || shadow == NULL ||
       source->clumps.empty() || shadow->clumps.empty() ||
       source->hierarchies.empty() || shadow->hierarchies.empty())
    {
        return false;
    }

    rw::HAnimHierarchy* sourceHierarchy = source->hierarchies[0];
    rw::HAnimHierarchy* shadowHierarchy = shadow->hierarchies[0];
    if(sourceHierarchy == NULL || shadowHierarchy == NULL ||
       sourceHierarchy->matrices == NULL || shadowHierarchy->matrices == NULL ||
       sourceHierarchy->numNodes != shadowHierarchy->numNodes)
    {
        return false;
    }

    rw::Frame* sourceFrame = source->clumps[0]->getFrame();
    rw::Frame* shadowFrame = shadow->clumps[0]->getFrame();
    if(sourceFrame == NULL || shadowFrame == NULL)
        return false;
    shadowFrame->matrix = sourceFrame->matrix;
    shadowFrame->updateObjects();
    memcpy(shadowHierarchy->matrices, sourceHierarchy->matrices,
           sizeof(rw::Matrix) *
               static_cast<size_t>(sourceHierarchy->numNodes));
    return true;
}

bool
loadCharacterScene(const char* archivePath, uint16_t resourceId,
                   rw::World* world, CharacterScene* scene)
{
    if(scene == NULL || world == NULL)
        return false;

    freeCharacterScene(scene);

    char entryName[16];
    snprintf(entryName, sizeof(entryName), "NPC%03u.BIN", resourceId);
    if(!readXafEntry(archivePath, entryName, scene->data))
        return false;

    const std::vector<XafEntryInfo>* dataEntries = NULL;
    if(resourceId >= CHARACTER_NPC_RESOURCE_COUNT ||
       !prepareCharacterNpcTextureMetadata(archivePath, dataEntries))
    {
        printf("ERROR: Could not load texture bindings for '%s'.\n",
               entryName);
        freeCharacterScene(scene);
        return false;
    }

    std::vector<rw::Raster*> rasters;
    for(size_t offset = 0; offset + 12 <= scene->data.size();)
    {
        const uint8_t* header = scene->data.data() + offset;
        const uint32_t type = read32(header);
        const uint32_t length = read32(header + 4);
        if(static_cast<uint64_t>(offset) + 12 + length > scene->data.size())
            break;
        if(type == CHUNK_IMAGE)
        {
            rw::Raster* raster = readImageRaster(header + 12, length);
            if(raster == NULL)
            {
                printf("ERROR: Could not decode an image in '%s'.\n", entryName);
                for(size_t i = 0; i < rasters.size(); i++)
                    if(rasters[i] != NULL)
                        rasters[i]->destroy();
                freeCharacterScene(scene);
                return false;
            }
            rasters.push_back(raster);
        }
        offset += 12 + length;
    }

    scene->textureDictionary = rw::TexDictionary::create();
    if(scene->textureDictionary == NULL)
    {
        for(size_t i = 0; i < rasters.size(); i++)
            if(rasters[i] != NULL)
                rasters[i]->destroy();
        freeCharacterScene(scene);
        return false;
    }

    std::vector<rw::Texture*> imageTextures;
    if(!createCharacterNpcTextures(
           resourceId, scene->textureDictionary, *dataEntries, rasters,
           imageTextures))
    {
        printf("ERROR: Could not bind package images for '%s'.\n",
               entryName);
        for(size_t i = 0; i < rasters.size(); ++i)
            if(rasters[i] != NULL)
                rasters[i]->destroy();
        freeCharacterScene(scene);
        return false;
    }

    rw::TexDictionary* previousDictionary =
        rw::TexDictionary::getCurrent();
    rw::TexDictionary::setCurrent(scene->textureDictionary);
    rw::Texture* (*previousReadCallback)(const char*, const char*) =
        rw::Texture::readCB;

    rw::Texture::readCB = readMissingCharacterTexture;

    bool valid = true;
    for(size_t offset = 0; offset + 12 <= scene->data.size();)
    {
        const uint8_t* header = scene->data.data() + offset;
        const uint32_t type = read32(header);
        const uint32_t length = read32(header + 4);
        const uint64_t end = static_cast<uint64_t>(offset) + 12 + length;
        if(end > scene->data.size())
        {
            valid = false;
            break;
        }

        if(type == CHUNK_CLUMP)
        {
            rw::StreamMemory stream;
            stream.open(scene->data.data() + offset,
                        static_cast<uint32_t>(12 + length));
            if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
            {
                valid = false;
                break;
            }
            rw::Clump* clump = rw::Clump::streamRead(&stream);
            if(clump == NULL)
            {
                valid = false;
                break;
            }
            prepareCharacterRenderClump(clump);
            scene->clumps.push_back(clump);
        }
        else if(type != CHUNK_IMAGE && type != CHUNK_ANIMATION)
        {

        }
        offset = static_cast<size_t>(end);
    }

    rw::TexDictionary::setCurrent(previousDictionary);
    rw::Texture::readCB = previousReadCallback;
    for(size_t i = 0; i < rasters.size(); i++)
        if(rasters[i] != NULL)
            rasters[i]->destroy();

    if(!valid || scene->clumps.empty())
    {
        printf("ERROR: Could not decode character stream '%s'.\n", entryName);
        freeCharacterScene(scene);
        return false;
    }

    replaceCharacterNpcTextures(
        *scene, imageTextures,
        g_characterNpcReplacementTable[resourceId] == 1);
    if(!validateCharacterNpcTextures(*scene, imageTextures, entryName))
    {
        freeCharacterScene(scene);
        return false;
    }

    bool haveBounds = false;
    rw::V3d boundsMin = { 0.0f, 0.0f, 0.0f };
    rw::V3d boundsMax = { 0.0f, 0.0f, 0.0f };
    for(size_t i = 0; i < 1; i++)
    {
        scene->clumps[i]->getFrame()->updateObjects();
        FORLIST(atomicLink, scene->clumps[i]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            const rw::Sphere* sphere = atomic->getWorldBoundingSphere();
            rw::V3d minimum =
            {
                sphere->center.x - sphere->radius,
                sphere->center.y - sphere->radius,
                sphere->center.z - sphere->radius
            };
            rw::V3d maximum =
            {
                sphere->center.x + sphere->radius,
                sphere->center.y + sphere->radius,
                sphere->center.z + sphere->radius
            };
            if(!haveBounds)
            {
                boundsMin = minimum;
                boundsMax = maximum;
                haveBounds = true;
            }
            else
            {
                boundsMin.x = fminf(boundsMin.x, minimum.x);
                boundsMin.y = fminf(boundsMin.y, minimum.y);
                boundsMin.z = fminf(boundsMin.z, minimum.z);
                boundsMax.x = fmaxf(boundsMax.x, maximum.x);
                boundsMax.y = fmaxf(boundsMax.y, maximum.y);
                boundsMax.z = fmaxf(boundsMax.z, maximum.z);
            }
        }
    }
    if(haveBounds)
    {
        scene->boundsCenter[0] = (boundsMin.x + boundsMax.x) * 0.5f;
        scene->boundsCenter[1] = (boundsMin.y + boundsMax.y) * 0.5f;
        scene->boundsCenter[2] = (boundsMin.z + boundsMax.z) * 0.5f;
        const float dx = boundsMax.x - boundsMin.x;
        const float dy = boundsMax.y - boundsMin.y;
        const float dz = boundsMax.z - boundsMin.z;
        scene->boundsRadius = sqrtf(dx * dx + dy * dy + dz * dz) * 0.5f;
    }

    bool haveAnimationHierarchy = false;
    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy =
            rw::HAnimHierarchy::get(scene->clumps[i]);
        if(hierarchy == NULL)
            continue;
        hierarchy->attach();
        hierarchy->flags |= rw::HAnimHierarchy::UPDATEMODELLINGMATRICES;
        std::vector<CharacterBoneTransform> boneTransforms;
        FORLIST(atomicLink, scene->clumps[i]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            rw::Skin* skin = rw::Skin::get(atomic->geometry);
            if(skin == NULL || skin->numBones != hierarchy->numNodes)
                continue;
            if(boneTransforms.empty())
                buildCharacterBoneTransforms(hierarchy, skin, boneTransforms);
        }

        setCharacterBindPoseMatrices(scene, hierarchy);

        if(!haveAnimationHierarchy && !boneTransforms.empty())
        {
            scene->hierarchies.push_back(hierarchy);
            scene->hierarchyBoneTransforms.push_back(boneTransforms);
            haveAnimationHierarchy = true;
        }
    }
    updateCharacterSecondaryClumpTransform(scene);

    printf("character %s clumps=%u hierarchies=%u bounds=(%.2f,%.2f,%.2f) radius=%.2f\n",
           entryName, static_cast<unsigned>(scene->clumps.size()),
           static_cast<unsigned>(scene->hierarchies.size()),
           scene->boundsCenter[0], scene->boundsCenter[1],
           scene->boundsCenter[2], scene->boundsRadius);
    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        const rw::Matrix& matrix = scene->clumps[i]->getFrame()->matrix;
        rw::HAnimHierarchy* hierarchy =
            rw::HAnimHierarchy::get(scene->clumps[i]);
        printf("  clump %u root=(%.2f,%.2f,%.2f) nodes=%d flags=%x\n",
               static_cast<unsigned>(i), matrix.pos.x, matrix.pos.y,
               matrix.pos.z, hierarchy != NULL ? hierarchy->numNodes : 0,
               hierarchy != NULL ? hierarchy->flags : 0);
        if(hierarchy != NULL)
        {
            for(int node = 0; node < hierarchy->numNodes; ++node)
            {
                rw::Frame* frame = hierarchy->nodeInfo[node].frame;
                printf("    node %d id=%d flags=%x frame=%s local=(%.2f,%.2f,%.2f)\n",
                       node, hierarchy->nodeInfo[node].id,
                       hierarchy->nodeInfo[node].flags,
                       frame != NULL ? "yes" : "no",
                       frame != NULL ? frame->matrix.pos.x : 0.0f,
                       frame != NULL ? frame->matrix.pos.y : 0.0f,
                       frame != NULL ? frame->matrix.pos.z : 0.0f);
            }
        }
    }
    fflush(stdout);

    scene->world = world;
    for(size_t i = 0; i < scene->clumps.size(); i++)
        world->addClump(scene->clumps[i]);
    return true;
}

static rw::Animation*
decodePackedBundleAnimation(const std::vector<uint8_t>& bundle,
                            uint32_t entryIndex)
{
    std::vector<uint8_t> animationData;
    if(!extractPackedBundleEntry(bundle, entryIndex, animationData) ||
       !convertCompressedAnimation(animationData))
        return NULL;

    rw::StreamMemory stream;
    stream.open(animationData.data(),
                static_cast<uint32_t>(animationData.size()));
    if(!rw::findChunk(&stream, rw::ID_ANIMANIMATION, NULL, NULL))
        return NULL;
    return rw::Animation::streamRead(&stream);
}

static rw::Animation*
decodeStandaloneAnimation(std::vector<uint8_t>& animationData)
{
    if(!convertCompressedAnimation(animationData))
        return NULL;

    rw::StreamMemory stream;
    stream.open(animationData.data(),
                static_cast<uint32_t>(animationData.size()));
    if(!rw::findChunk(&stream, rw::ID_ANIMANIMATION, NULL, NULL))
        return NULL;
    return rw::Animation::streamRead(&stream);
}

static bool
installAnimationBundle(const std::vector<uint8_t>& bundle,
                       CharacterScene* scene, const char* label)
{
    if(scene == NULL || scene->hierarchies.empty() || bundle.size() < 4)
        return false;

    const uint32_t entryCount = read32(bundle.data());
    std::vector<rw::Animation*> animations;
    for(uint32_t i = 0; i < entryCount; i++)
    {
        rw::Animation* animation = decodePackedBundleAnimation(bundle, i);
        if(animation == NULL)
            goto fail;
        animations.push_back(animation);
    }
    if(animations.empty())
        return false;

    for(size_t i = 0; i < scene->animations.size(); i++)
        scene->animations[i]->destroy();
    scene->animations.swap(animations);
    scene->animationKeys.assign(scene->animations.size(), UINT64_MAX);
    scene->basicAnimation = SIZE_MAX;
    scene->activeAnimation = 0;
    scene->animationState = 0xffffffffu;
    scene->animationRate = 1.0f;
    scene->animationLoop = true;
    scene->animationStartTime = 0.0f;
    scene->animationEndTime = scene->animations[0]->duration;
    scene->animationTransitionDuration = 0.0f;
    scene->animationTransitionRemaining = 0.0f;
    scene->animationBlendSource.clear();
    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
        scene->hierarchies[i]->interpolator->setCurrentAnim(
            scene->animations[0]);
    return true;

fail:
    for(size_t i = 0; i < animations.size(); i++)
        animations[i]->destroy();
    printf("ERROR: Could not decode character animation bundle '%s'.\n",
           label);
    return false;
}

bool
loadCharacterAnimationBundle(const char* archivePath,
                             const char* entryName,
                             CharacterScene* scene)
{
    std::vector<uint8_t> bundle;
    if(!readXafEntry(archivePath, entryName, bundle))
        return false;
    return installAnimationBundle(bundle, scene, entryName);
}

bool
loadCharacterAnimationBundleByIndex(const char* archivePath,
                                    uint32_t entryIndex,
                                    CharacterScene* scene)
{
    std::vector<uint8_t> bundle;
    if(!readXafEntryByIndex(archivePath, entryIndex, bundle))
        return false;
    char label[32];
    snprintf(label, sizeof(label), "entry %u", entryIndex);
    return installAnimationBundle(bundle, scene, label);
}

bool
loadCharacterAnimationClipByIndex(const char* archivePath,
                                  uint32_t entryIndex,
                                  size_t positionInBundle,
                                  CharacterScene* scene,
                                  size_t* animationIndex)
{
    if(scene == NULL || animationIndex == NULL)
        return false;
    const uint64_t key =
        (static_cast<uint64_t>(entryIndex) << 32) |
        static_cast<uint64_t>(positionInBundle);
    for(size_t i = 0; i < scene->animationKeys.size(); ++i)
    {
        if(scene->animationKeys[i] != key)
            continue;
        *animationIndex = i;
        return true;
    }

    std::vector<uint8_t> bundle;
    if(!readXafEntryByIndex(archivePath, entryIndex, bundle) ||
       positionInBundle > UINT32_MAX)
        return false;
    rw::Animation* animation = decodePackedBundleAnimation(
        bundle, static_cast<uint32_t>(positionInBundle));
    if(animation == NULL)
        return false;
    scene->animations.push_back(animation);
    scene->animationKeys.push_back(key);
    *animationIndex = scene->animations.size() - 1;
    return true;
}

bool
loadCharacterAnimationResourceById(const char* archivePath,
                                   int resourceId, CharacterScene* scene)
{
    if(scene == nullptr || resourceId < 0 || resourceId >= MOVE_ANIMATION_BUNDLE_COUNT)
        return false;
    const MoveAnimationBundle& bundle = MOVE_ANIMATION_BUNDLES[resourceId];
    if(bundle.xafEntry < 0)
        return true;
    for(int clip = 0; clip < bundle.motionCount; ++clip)
    {
        size_t animation = 0;
        if(!loadCharacterAnimationClipByIndex(archivePath,
               static_cast<uint32_t>(bundle.xafEntry), static_cast<size_t>(clip),
               scene, &animation))
            return false;
    }
    return true;
}

bool
loadCharacterBasicAnimation(const char* archivePath, CharacterScene* scene,
                            size_t* animationIndex)
{
    if(scene == NULL)
        return false;
    if(scene->basicAnimation < scene->animations.size() &&
       scene->animations[scene->basicAnimation] != NULL)
    {
        if(animationIndex != NULL)
            *animationIndex = scene->basicAnimation;
        return true;
    }

    std::vector<uint8_t> animationData;
    if(!readXafEntryByIndex(archivePath, 0, animationData))
        return false;
    rw::Animation* animation = decodeStandaloneAnimation(animationData);
    if(animation == NULL)
        return false;

    scene->animations.push_back(animation);
    scene->animationKeys.push_back(UINT64_MAX);
    scene->basicAnimation = scene->animations.size() - 1;
    if(animationIndex != NULL)
        *animationIndex = scene->basicAnimation;
    return true;
}

bool
loadCharacterScheduledAnimation(const char* archivePath,
                                int motionResourceSet, int motionId,
                                CharacterScene* scene, size_t* animationIndex,
                                bool* usedBasic)
{
    if(scene == NULL || animationIndex == NULL || motionId <= 0)
        return false;

    int position = -1;
    const int bundle = moveMotionBundleForCharacter(
        motionResourceSet, motionId, &position);
    if(bundle < 0 || position < 0)
    {
        if(scene->basicAnimation >= scene->animations.size() ||
           scene->animations[scene->basicAnimation] == NULL)
            return false;
        *animationIndex = scene->basicAnimation;
        if(usedBasic != NULL)
            *usedBasic = true;
        return true;
    }

    const int entry = MOVE_ANIMATION_BUNDLES[bundle].xafEntry;
    if(entry < 0 || !loadCharacterAnimationClipByIndex(
           archivePath, static_cast<uint32_t>(entry),
           static_cast<size_t>(position), scene, animationIndex))
        return false;
    if(usedBasic != NULL)
        *usedBasic = false;
    return true;
}

bool
loadCharacterSceneAnimation(const char* archivePath,
                            int motionResourceSet, int motionId,
                            CharacterScene* scene, size_t* animationIndex)
{
    bool usedBasic = false;
    if(!loadCharacterScheduledAnimation(
           archivePath, motionResourceSet, motionId, scene, animationIndex,
           &usedBasic))
        return false;
    if(!usedBasic)
        return true;

    int position = -1;
    const int bundle = moveMotionBundle(motionId, &position);
    if(bundle < 0 || position < 0)
        return false;
    const int entry = MOVE_ANIMATION_BUNDLES[bundle].xafEntry;
    return entry >= 0 && loadCharacterAnimationClipByIndex(
        archivePath, static_cast<uint32_t>(entry),
        static_cast<size_t>(position), scene, animationIndex);
}

static bool
captureCharacterAnimationBlendSource(CharacterScene* scene)
{
    scene->animationBlendSource.resize(scene->hierarchies.size());
    bool captured = false;
    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        std::vector<CharacterAnimationBlendPose>& poses =
            scene->animationBlendSource[i];
        if(hierarchy == NULL || hierarchy->interpolator == NULL ||
           hierarchy->interpolator->currentAnim == NULL ||
           hierarchy->numNodes <= 0)
        {
            poses.clear();
            continue;
        }

        poses.resize(static_cast<size_t>(hierarchy->numNodes));
        for(int node = 0; node < hierarchy->numNodes; ++node)
        {
            const rw::HAnimInterpFrame* frame =
                reinterpret_cast<const rw::HAnimInterpFrame*>(
                    hierarchy->interpolator->getInterpFrame(node));
            CharacterAnimationBlendPose& pose = poses[node];
            pose.rotation[0] = frame->q.x;
            pose.rotation[1] = frame->q.y;
            pose.rotation[2] = frame->q.z;
            pose.rotation[3] = frame->q.w;
            pose.translation[0] = frame->t.x;
            pose.translation[1] = frame->t.y;
            pose.translation[2] = frame->t.z;
        }
        captured = true;
    }
    return captured;
}

static void
applyCharacterAnimationBlend(CharacterScene* scene, size_t hierarchyIndex,
                             float weight)
{
    if(weight >= 1.0f ||
       hierarchyIndex >= scene->animationBlendSource.size())
        return;
    rw::HAnimHierarchy* hierarchy = scene->hierarchies[hierarchyIndex];
    const std::vector<CharacterAnimationBlendPose>& poses =
        scene->animationBlendSource[hierarchyIndex];
    if(hierarchy == NULL ||
       poses.size() != static_cast<size_t>(hierarchy->numNodes))
        return;

    for(int node = 0; node < hierarchy->numNodes; ++node)
    {
        rw::HAnimInterpFrame* frame =
            reinterpret_cast<rw::HAnimInterpFrame*>(
                hierarchy->interpolator->getInterpFrame(node));
        const CharacterAnimationBlendPose& pose = poses[node];
        const rw::Quat source =
        {
            pose.rotation[0], pose.rotation[1],
            pose.rotation[2], pose.rotation[3]
        };
        frame->q = rw::slerp(source, frame->q, weight);
        frame->t.x = pose.translation[0] +
            (frame->t.x - pose.translation[0]) * weight;
        frame->t.y = pose.translation[1] +
            (frame->t.y - pose.translation[1]) * weight;
        frame->t.z = pose.translation[2] +
            (frame->t.z - pose.translation[2]) * weight;
    }
}

CharacterAnimationPlayback
makeCharacterAnimationPlayback(float clipSeconds, int startFrame,
                               int maxDurationFrames, int transitionFrames,
                               float rate, bool loop,
                               bool suppressTransition)
{
    CharacterAnimationPlayback playback;
    playback.startSeconds = static_cast<float>(startFrame) / 60.0f;
    playback.endSeconds = clipSeconds;
    if(maxDurationFrames != -1)
    {
        const float clamped =
            static_cast<float>(maxDurationFrames) / 60.0f;
        if(clamped <= playback.endSeconds)
            playback.endSeconds = clamped;
    }
    playback.transitionSeconds = suppressTransition ? 0.0f :
        static_cast<float>(transitionFrames) / 60.0f;

    if(playback.endSeconds - playback.startSeconds <
       playback.transitionSeconds)
        playback.transitionSeconds = 0.0f;
    playback.rate = rate;
    playback.loop = loop;
    return playback;
}

float
advanceCharacterAnimationTransition(CharacterScene* scene,
                                    float animationDeltaSeconds)
{
    if(scene == NULL || scene->animationTransitionDuration <= 0.0f)
        return 1.0f;
    if(animationDeltaSeconds > 0.0f &&
       scene->animationTransitionRemaining > 0.0f)
    {
        scene->animationTransitionRemaining -= animationDeltaSeconds;
        if(scene->animationTransitionRemaining < 0.0f)
            scene->animationTransitionRemaining = 0.0f;
    }

    float weight =
        (scene->animationTransitionDuration -
         scene->animationTransitionRemaining) /
        scene->animationTransitionDuration;
    if(weight < 0.0f)
        weight = 0.0f;
    if(weight > 1.0f)
        weight = 1.0f;
    return weight;
}

void
setCharacterSceneAnimation(CharacterScene* scene, size_t index)
{
    if(scene == NULL || scene->hierarchies.empty() ||
       index >= scene->animations.size())
        return;
    const CharacterAnimationPlayback playback =
        makeCharacterAnimationPlayback(
            scene->animations[index]->duration, 0, -1, 0, 1.0f, true);
    bindCharacterSceneAnimation(
        scene, index, static_cast<uint32_t>(index), 0.0f, playback);
}

void
bindCharacterSceneAnimation(CharacterScene* scene, size_t index,
                            uint32_t state, float elapsedSeconds,
                            float stateDurationSeconds, bool loop)
{
    if(scene == NULL || index >= scene->animations.size())
        return;
    rw::Animation* animation = scene->animations[index];
    float rate = stateDurationSeconds > 0.0f ?
        animation->duration / stateDurationSeconds : 1.0f;
    if(rate < 0.05f)
        rate = 0.05f;
    if(rate > 8.0f)
        rate = 8.0f;
    const CharacterAnimationPlayback playback =
        makeCharacterAnimationPlayback(
            animation->duration, 0, -1, 0, rate, loop);
    bindCharacterSceneAnimation(
        scene, index, state, elapsedSeconds, playback);
}

void
bindCharacterSceneAnimation(
    CharacterScene* scene, size_t index, uint32_t state,
    float elapsedSeconds, const CharacterAnimationPlayback& playback)
{
    if(scene == NULL || scene->hierarchies.empty() ||
       index >= scene->animations.size())
        return;

    scene->animationRate = playback.rate;
    scene->animationLoop = playback.loop;
    scene->animationStartTime = playback.startSeconds;
    scene->animationEndTime = playback.endSeconds;
    if(index == scene->activeAnimation && state == scene->animationState)
        return;

    const float elapsedAnimation = elapsedSeconds > 0.0f ?
        elapsedSeconds * playback.rate : 0.0f;
    scene->animationTransitionDuration = playback.transitionSeconds;
    scene->animationTransitionRemaining =
        playback.transitionSeconds - elapsedAnimation;
    if(scene->animationTransitionRemaining < 0.0f)
        scene->animationTransitionRemaining = 0.0f;

    if(scene->animationTransitionRemaining > 0.0f &&
       !captureCharacterAnimationBlendSource(scene))
        scene->animationTransitionRemaining = 0.0f;

    scene->activeAnimation = index;
    scene->animationState = state;
    float animationTime = playback.startSeconds + elapsedAnimation;
    const float span = playback.endSeconds - playback.startSeconds;
    if(playback.loop && span > 0.0f &&
       animationTime > playback.endSeconds)
    {
        animationTime = playback.startSeconds +
            fmodf(animationTime - playback.startSeconds, span);
    }
    else if(animationTime > playback.endSeconds)
    {
        animationTime = playback.endSeconds;
    }

    const float blendWeight = scene->animationTransitionRemaining > 0.0f ?
        advanceCharacterAnimationTransition(scene, 0.0f) : 1.0f;
    rw::Animation* animation = scene->animations[index];
    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        hierarchy->interpolator->setCurrentAnim(animation);
        if(animationTime > 0.0f)
            hierarchy->interpolator->addTime(animationTime);
        applyCharacterAnimationBlend(scene, i, blendWeight);
        hierarchy->updateMatrices();
        if(i < scene->hierarchyBoneTransforms.size())
            restoreCharacterBoneTransforms(
                hierarchy, scene->hierarchyBoneTransforms[i]);
    }
    updateCharacterSecondaryClumpTransform(scene);
}

bool
alignCharacterSceneAnimationRoot(CharacterScene* scene, float worldX,
                                 float worldZ)
{
    if(scene == NULL || scene->clumps.empty() || scene->hierarchies.empty() ||
       scene->activeAnimation >= scene->animations.size())
        return false;
    rw::Frame* const actorFrame = scene->clumps[0]->getFrame();
    rw::HAnimHierarchy* const primary = scene->hierarchies[0];
    if(actorFrame == NULL || primary == NULL || primary->numNodes <= 0 ||
       primary->matrices == NULL || primary->interpolator == NULL)
        return false;

    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        const float time = hierarchy->interpolator->currentTime;
        hierarchy->interpolator->setCurrentAnim(
            scene->animations[scene->activeAnimation]);
        if(time > 0.0f)
            hierarchy->interpolator->addTime(time);
        hierarchy->updateMatrices();
        if(i < scene->hierarchyBoneTransforms.size())
            restoreCharacterBoneTransforms(hierarchy, scene->hierarchyBoneTransforms[i]);
    }
    const int root = primary->numNodes > 1 ? 1 : 0;
    const rw::V3d delta = { worldX - primary->matrices[root].pos.x, 0.0f,
                           worldZ - primary->matrices[root].pos.z };
    rw::Matrix inverse;
    rw::Matrix::invert(&inverse, actorFrame->getLTM());
    rw::V3d localDelta;
    rw::V3d::transformVectors(&localDelta, &delta, 1, &inverse);

    if(scene->animationTransitionRemaining > 0.0f)
        for(size_t i = 0; i < scene->animationBlendSource.size(); ++i)
        {
            std::vector<CharacterAnimationBlendPose>& poses = scene->animationBlendSource[i];
            for(size_t node = 0; node < poses.size(); ++node)
                if((poses.size() == 1 && node == 0) || node == 1 || node == 8 || node == 21)
                {
                    poses[node].translation[0] -= localDelta.x;
                    poses[node].translation[2] -= localDelta.z;
                }
        }
    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        rw::Frame* frame = scene->clumps[i]->getFrame();
        if(frame == NULL)
            continue;
        frame->matrix.pos.x += delta.x;
        frame->matrix.pos.z += delta.z;
        frame->matrix.optimize();
        frame->updateObjects();
    }
    const float weight = advanceCharacterAnimationTransition(scene, 0.0f);
    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        applyCharacterAnimationBlend(scene, i, weight);
        hierarchy->updateMatrices();
        if(i < scene->hierarchyBoneTransforms.size())
            restoreCharacterBoneTransforms(hierarchy, scene->hierarchyBoneTransforms[i]);
    }
    updateCharacterSecondaryClumpTransform(scene);
    return true;
}

void
tickCharacterScene(CharacterScene* scene, float deltaSeconds)
{
    if(scene == NULL || scene->hierarchies.empty() ||
       scene->activeAnimation >= scene->animations.size() ||
       deltaSeconds <= 0.0f)
        return;

    const float animationDelta = deltaSeconds * scene->animationRate;
    const float span =
        scene->animationEndTime - scene->animationStartTime;
    const rw::AnimInterpolator* first =
        scene->hierarchies[0]->interpolator;
    const bool restartLoop =
        scene->animationLoop && span > 0.0f &&
        first->currentTime >= scene->animationEndTime;
    if(restartLoop)
    {

        scene->animationTransitionRemaining =
            scene->animationTransitionDuration;
        if(scene->animationTransitionRemaining > 0.0f &&
           !captureCharacterAnimationBlendSource(scene))
            scene->animationTransitionRemaining = 0.0f;
    }
    const float blendWeight =
        advanceCharacterAnimationTransition(scene, animationDelta);

    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        rw::AnimInterpolator* interpolator = hierarchy->interpolator;
        if(restartLoop)
        {
            interpolator->setCurrentAnim(
                scene->animations[scene->activeAnimation]);
            if(scene->animationStartTime > 0.0f)
                interpolator->addTime(scene->animationStartTime);
        }

        float hierarchyDelta = animationDelta;
        const float remaining =
            scene->animationEndTime - interpolator->currentTime;
        if(remaining <= 0.0f)
            hierarchyDelta = 0.0f;
        else if(hierarchyDelta > remaining)
            hierarchyDelta = remaining;
        if(hierarchyDelta > 0.0f)
            interpolator->addTime(hierarchyDelta);

        applyCharacterAnimationBlend(scene, i, blendWeight);
        hierarchy->updateMatrices();
        if(i < scene->hierarchyBoneTransforms.size())
            restoreCharacterBoneTransforms(
                hierarchy, scene->hierarchyBoneTransforms[i]);
    }
    updateCharacterSecondaryClumpTransform(scene);
}

void
setCharacterSceneTransform(CharacterScene* scene,
                           float x, float y, float z,
                           float headingDegrees, float tiltDegrees,
                           float heightOffset)
{
    if(scene == NULL)
        return;

    const float heading = headingDegrees * 0.01745329251994329577f;
    const float tilt = tiltDegrees * 0.01745329251994329577f;
    const float headingSine = sinf(heading);
    const float headingCosine = cosf(heading);
    const float tiltSine = sinf(tilt);
    const float tiltCosine = cosf(tilt);
    rw::V3d right = { headingCosine, 0.0f, -headingSine };
    rw::V3d up =
    {
        headingSine * tiltSine,
        tiltCosine,
        headingCosine * tiltSine
    };
    rw::V3d at =
    {
        headingSine * tiltCosine,
        -tiltSine,
        headingCosine * tiltCosine
    };
    rw::V3d position = { x, y + heightOffset, z };

    if(scene->clumps.empty())
        return;
    rw::Frame* frame = scene->clumps[0]->getFrame();
    if(frame == NULL)
        return;
    frame->matrix.right = right;
    frame->matrix.up = up;
    frame->matrix.at = at;
    frame->matrix.pos = position;
    frame->matrix.optimize();
    frame->updateObjects();
    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        if(hierarchy->interpolator->currentAnim != NULL)
        {
            hierarchy->updateMatrices();
            if(i < scene->hierarchyBoneTransforms.size())
                restoreCharacterBoneTransforms(
                    hierarchy, scene->hierarchyBoneTransforms[i]);
            continue;
        }

        setCharacterBindPoseMatrices(scene, hierarchy);
    }

    updateCharacterSecondaryClumpTransform(scene);
}

void
setCharacterSceneRootY(CharacterScene* scene, float y)
{
    if(scene == NULL || scene->clumps.empty())
        return;
    rw::Frame* frame = scene->clumps[0]->getFrame();
    if(frame == NULL)
        return;

    frame->matrix.pos.y = y;
    frame->matrix.optimize();
    frame->updateObjects();
    for(size_t i = 0; i < scene->hierarchies.size(); ++i)
    {
        rw::HAnimHierarchy* hierarchy = scene->hierarchies[i];
        if(hierarchy->interpolator->currentAnim != NULL)
        {
            hierarchy->updateMatrices();
            if(i < scene->hierarchyBoneTransforms.size())
                restoreCharacterBoneTransforms(
                    hierarchy, scene->hierarchyBoneTransforms[i]);
            continue;
        }
        setCharacterBindPoseMatrices(scene, hierarchy);
    }
    updateCharacterSecondaryClumpTransform(scene);
}

rw::Frame*
characterSceneBoneFrame(const CharacterScene* scene, int boneIndex)
{
    if(scene == NULL || scene->hierarchies.empty() || boneIndex < 0)
        return NULL;
    const rw::HAnimHierarchy* hierarchy = scene->hierarchies[0];
    if(hierarchy == NULL || hierarchy->nodeInfo == NULL ||
       boneIndex >= hierarchy->numNodes)
    {
        return NULL;
    }
    return hierarchy->nodeInfo[boneIndex].frame;
}

void
freeCharacterScene(CharacterScene* scene)
{
    if(scene == NULL)
        return;

    if(scene->world != NULL)
    {
        FORLIST(link, scene->world->clumps)
        {
            rw::Clump* external = rw::Clump::fromWorld(link);
            bool owned = false;
            for(rw::Clump* clump : scene->clumps)
                owned |= clump == external;
            rw::Frame* frame = external->getFrame();
            if(owned || frame == NULL || frame->getParent() == NULL)
                continue;
            bool attached = false;
            for(rw::Frame* parent = frame->getParent(); parent != NULL;
                parent = parent->getParent())
                for(rw::Clump* clump : scene->clumps)
                    attached |= parent == clump->getFrame();
            if(attached)
            {
                frame->updateObjects();
                const rw::Matrix world = *frame->getLTM();
                frame->removeChild();
                frame->transform(&world, rw::COMBINEREPLACE);
            }
        }
    }

    for(size_t i = 0; i < scene->clumps.size(); i++)
    {
        FORLIST(atomicLink, scene->clumps[i]->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            if(rw::Skin::get(atomic->geometry) != NULL)
                rw::Skin::setHierarchy(atomic, NULL);
        }
    }

    for(size_t i = 0; i < scene->clumps.size(); i++)
    {
        if(scene->world != NULL)
            scene->world->removeClump(scene->clumps[i]);
        scene->clumps[i]->destroy();
    }
    scene->clumps.clear();

    scene->hierarchies.clear();
    scene->hierarchyBoneTransforms.clear();
    for(size_t i = 0; i < scene->animations.size(); i++)
        scene->animations[i]->destroy();
    scene->animations.clear();
    scene->animationKeys.clear();
    scene->basicAnimation = SIZE_MAX;
    scene->activeAnimation = 0;
    scene->animationState = 0xffffffffu;
    scene->animationRate = 1.0f;
    scene->animationLoop = true;
    scene->animationStartTime = 0.0f;
    scene->animationEndTime = 0.0f;
    scene->animationTransitionDuration = 0.0f;
    scene->animationTransitionRemaining = 0.0f;
    scene->animationBlendSource.clear();

    for(size_t i = 0;
        i < scene->attachmentTextureDictionaries.size(); ++i)
    {
        rw::TexDictionary* dictionary =
            scene->attachmentTextureDictionaries[i];
        if(dictionary == NULL)
            continue;
        if(rw::TexDictionary::getCurrent() == dictionary)
            rw::TexDictionary::setCurrent(NULL);
        dictionary->destroy();
    }
    scene->attachmentTextureDictionaries.clear();
    scene->attachments.clear();
    scene->archiveFingerprint = 0;
    scene->assembledFromArchive = false;

    if(scene->textureDictionary != NULL)
    {
        if(rw::TexDictionary::getCurrent() == scene->textureDictionary)
            rw::TexDictionary::setCurrent(NULL);
        scene->textureDictionary->destroy();
    }
    scene->textureDictionary = NULL;
    scene->world = NULL;
    scene->boundsCenter[0] = scene->boundsCenter[1] =
        scene->boundsCenter[2] = 0.0f;
    scene->boundsRadius = 0.0f;
    scene->data.clear();
}

int
selectRenderableStoryActors(const StoryState& story,
                            const uint16_t* excluded, int excludedCount,
                            uint16_t* out, int capacity)
{
    if(out == NULL || capacity <= 0)
        return 0;

    int count = 0;
    for(int i = 0; i < STORY_ACTOR_COUNT && count < capacity; ++i)
    {
        const StoryActorState& actor = story.actors[i];
        if(!actor.active || !actor.visible)
            continue;
        if(actor.actorId == 0 || actor.actorId == 0xffffu)
            continue;

        if(characterRecordResourceId(actor.actorId) <= 0)
            continue;

        bool skip = false;
        for(int j = 0; j < excludedCount; ++j)
        {
            if(excluded != NULL && excluded[j] == actor.actorId)
            {
                skip = true;
                break;
            }
        }

        for(int j = 0; !skip && j < count; ++j)
        {
            if(out[j] == actor.actorId)
                skip = true;
        }
        if(skip)
            continue;
        out[count++] = actor.actorId;
    }
    return count;
}

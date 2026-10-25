#ifndef CHARACTER_INTERNAL_H
#define CHARACTER_INTERNAL_H

#include "character/character.h"
#include <cstring>
namespace rw
{
struct V3d;
}

namespace character_detail
{

inline uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

inline uint16_t read16(const uint8_t* data)
{
    uint16_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

inline void write32(uint8_t* data, uint32_t value)
{
    memcpy(data, &value, sizeof(value));
}

inline uint8_t expandPs2Alpha(uint8_t alpha)
{
    const uint32_t expanded = static_cast<uint32_t>(alpha) * 2;
    return static_cast<uint8_t>(expanded > 255 ? 255 : expanded);
}

inline constexpr uint32_t CHUNK_STRUCT = 0x01;
inline constexpr uint32_t CHUNK_CLUMP = 0x10;
inline constexpr uint32_t CHUNK_IMAGE = 0x18;
inline constexpr uint32_t CHUNK_ANIMATION = 0x1b;
inline constexpr int CHARACTER_SECONDARY_BONE_INDEX = 10;
inline constexpr uint32_t PLUGIN_MORPH = 0x105;

struct CharacterMorphInterpolator
{
    int32_t flags;
    int16_t startTarget;
    int16_t endTarget;
    float duration;
    float reciprocalDuration;
    int32_t next;
};

struct CharacterMorphGeometryData
{
    int32_t count;
    void* callback;
    CharacterMorphInterpolator* interpolators;
    rw::V3d* baseVertices;
    rw::V3d* baseNormals;
    int32_t baseVertexCount;
};

CharacterMorphGeometryData* characterMorphGeometryData(rw::Geometry* geometry);
const CharacterMorphGeometryData* characterMorphGeometryData(const rw::Geometry* geometry);

inline constexpr size_t CHARACTER_NPC_RESOURCE_COUNT = 189;
inline constexpr size_t CHARACTER_NPC_TEXTURE_COUNT = 10;
inline constexpr long CHARACTER_NPC_TEXTURE_TABLE_FILE_OFFSET = 0x331500;
inline constexpr long CHARACTER_NPC_REPLACEMENT_TABLE_FILE_OFFSET = 0x3323d0;
inline constexpr long CHARACTER_ARCHIVE_METADATA_FILE_OFFSET = 0x3f2c00;
inline constexpr long CHARACTER_COSTUME_TABLE_FILE_OFFSETS[3] =
{
    0x3ed260, 0x3edbc0, 0x3eed60
};
inline constexpr size_t CHARACTER_COSTUME_VARIANT_COUNT = 9;
inline constexpr size_t CHARACTER_COSTUME_VARIANT_SIZE = 0x108;
inline constexpr const char* CHARACTER_PALMODE_TEXTURE_NAMES[PALMODE_IMAGE_COUNT] =
{
    "MESS_E", "VSCRN", "TEST"
};

}

#endif

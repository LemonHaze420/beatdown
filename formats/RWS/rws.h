#ifndef RWS_H
#define RWS_H

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t RW_CHUNK_STRUCT = 0x01;
constexpr uint32_t RW_CHUNK_ATOMIC_SECTOR = 0x09;
constexpr uint32_t RW_CHUNK_PLANE_SECTOR = 0x0a;
constexpr uint32_t RW_CHUNK_CLUMP = 0x10;
constexpr uint32_t RW_CHUNK_ATOMIC = 0x14;
constexpr uint32_t RW_CHUNK_TEXDICTIONARY = 0x16;
constexpr uint32_t RW_CHUNK_IMAGE = 0x18;
constexpr uint32_t RW_CHUNK_ANIMATION = 0x1b;
constexpr uint32_t RW_CHUNK_OBJECT_DIRECTORY = 0x79032003;
constexpr uint32_t RW_CHUNK_CUSTOM = 0x78032010;
constexpr uint32_t RW_CHUNK_CUSTOM_PAYLOAD = 0x78032020;
constexpr uint32_t RW_CHUNK_CUSTOM_ANIMATIONS = 0x78032030;
constexpr uint32_t RW_CHUNK_HEADER_SIZE = 0x0c;
constexpr uint32_t RW_WORLD_STRUCT_PREFIX = 40;
constexpr uint32_t RW_PLANE_SECTOR_STRUCT_SIZE = 24;
constexpr uint32_t RW_ATOMIC_SECTOR_STRUCT_PREFIX = 44;

struct RwChunkSpan
{
    uint32_t id;
    uint32_t dataSize;
    uint32_t libraryId;
    size_t offset;
    size_t end;
};

bool readRwChunk(const uint8_t* data, size_t length, size_t offset, RwChunkSpan& chunk);

#endif

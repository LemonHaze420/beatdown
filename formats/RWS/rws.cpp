#include "rws.h"
#include "../common.h"

bool readRwChunk(const uint8_t* data, size_t length, size_t offset, RwChunkSpan& chunk)
{
    if (data == NULL || offset > length || length - offset < RW_CHUNK_HEADER_SIZE)
        return false;
    const uint32_t size = read32(data + offset + 4);
    if (size > length - offset - RW_CHUNK_HEADER_SIZE)
        return false;
    chunk.id = read32(data + offset);
    chunk.dataSize = size;
    chunk.libraryId = read32(data + offset + 8);
    chunk.offset = offset;
    chunk.end = offset + RW_CHUNK_HEADER_SIZE + size;
    return true;
}

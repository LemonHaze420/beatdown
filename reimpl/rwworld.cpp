#include "rwworld.h"
#include "formats/common.h"
#include "formats/RWS/rws.h"

#include <rw.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{

bool readMaterialTextureAlphaMask(const uint8_t* data, uint32_t length,
                                  uint32_t materialOffset,
                                  uint32_t materialLength,
                                  bool& hasAlphaMask)
{
    hasAlphaMask = false;
    const uint64_t materialEnd64 = static_cast<uint64_t>(materialOffset) + RW_CHUNK_HEADER_SIZE + materialLength;
    if(materialEnd64 > length)
        return false;
    const uint32_t materialEnd = static_cast<uint32_t>(materialEnd64);

    bool haveStruct = false;
    bool textured = false;
    uint32_t cursor = materialOffset + RW_CHUNK_HEADER_SIZE;
    while(cursor < materialEnd)
    {
        RwChunkSpan child;
        if (!readRwChunk(data, length, cursor, child) ||
            static_cast<uint64_t>(cursor) + RW_CHUNK_HEADER_SIZE + child.dataSize > materialEnd)
            return false;

        if (child.id == rw::ID_STRUCT && !haveStruct)
        {

            if (child.dataSize < 0x10)
                return false;
            textured = read32(data + cursor + RW_CHUNK_HEADER_SIZE + 0x0c) != 0;
            haveStruct = true;
        }
        else if (child.id == rw::ID_TEXTURE)
        {
            if(!haveStruct || !textured)
                return false;
            const uint32_t textureEnd = cursor + RW_CHUNK_HEADER_SIZE + child.dataSize;
            uint32_t textureCursor = cursor + RW_CHUNK_HEADER_SIZE;
            int stringIndex = 0;
            while(textureCursor < textureEnd)
            {
                RwChunkSpan textureChild;
                if (!readRwChunk(data, length, textureCursor, textureChild) ||
                    static_cast<uint64_t>(textureCursor) + RW_CHUNK_HEADER_SIZE + textureChild.dataSize > textureEnd)
                    return false;
                if (textureChild.id == rw::ID_STRING)
                {

                    if(stringIndex == 1)
                    {
                        hasAlphaMask = textureChild.dataSize != 0 && data[textureCursor + RW_CHUNK_HEADER_SIZE] != 0;
                        return true;
                    }
                    ++stringIndex;
                }
                textureCursor += RW_CHUNK_HEADER_SIZE + textureChild.dataSize;
            }
            return false;
        }
        cursor += RW_CHUNK_HEADER_SIZE + child.dataSize;
    }
    return haveStruct && !textured;
}

bool readMaterialAlphaMasks(const uint8_t* data, uint32_t length,
                            uint32_t materialListOffset,
                            uint32_t materialListLength,
                            std::vector<uint8_t>& alphaMasks)
{
    const uint64_t materialListEnd64 =
        static_cast<uint64_t>(materialListOffset) + materialListLength;
    if(materialListEnd64 > length)
        return false;
    const uint32_t materialListEnd =
        static_cast<uint32_t>(materialListEnd64);

    RwChunkSpan body;
    if (!readRwChunk(data, length, materialListOffset, body) || body.id != rw::ID_STRUCT ||
        static_cast<uint64_t>(materialListOffset) + RW_CHUNK_HEADER_SIZE + body.dataSize > materialListEnd ||
        body.dataSize < 4)
        return false;
    const uint8_t* record = data + materialListOffset + RW_CHUNK_HEADER_SIZE;
    const int32_t materialCount = static_cast<int32_t>(read32(record));
    if (materialCount < 0 || 4u + static_cast<uint64_t>(materialCount) * 4u > body.dataSize)
        return false;

    alphaMasks.assign(static_cast<size_t>(materialCount), 0);
    uint32_t cursor = materialListOffset + RW_CHUNK_HEADER_SIZE + body.dataSize;
    for(int32_t i = 0; i < materialCount; ++i)
    {
        const int32_t previous =
            static_cast<int32_t>(read32(record + 4u + i * 4u));
        if(previous >= 0)
        {
            if(previous >= i)
                return false;
            alphaMasks[i] = alphaMasks[previous];
            continue;
        }

        RwChunkSpan material;
        if (!readRwChunk(data, length, cursor, material) || material.id != rw::ID_MATERIAL ||
            static_cast<uint64_t>(cursor) + RW_CHUNK_HEADER_SIZE + material.dataSize > materialListEnd)
            return false;
        bool hasAlphaMask = false;
        if (!readMaterialTextureAlphaMask(data, length, cursor, material.dataSize, hasAlphaMask))
            return false;
        alphaMasks[i] = hasAlphaMask ? 1 : 0;
        cursor += RW_CHUNK_HEADER_SIZE + material.dataSize;
    }
    return true;
}

struct WorldReader
{
    const uint8_t* data;
    uint32_t length;
    uint32_t format;
    uint32_t vertexStride;
    rw::MaterialList* materials;
    rw::Clump* clump;
    rw::Frame* frame;
    RwWorldGeometry* geometry;
};

void includeBounds(RwWorldGeometry* geometry, const RwWorldVertex& point)
{
    if(!geometry->haveBounds)
    {
        geometry->minBounds = point;
        geometry->maxBounds = point;
        geometry->haveBounds = true;
        return;
    }
    if(point.x < geometry->minBounds.x)
        geometry->minBounds.x = point.x;
    if(point.y < geometry->minBounds.y)
        geometry->minBounds.y = point.y;
    if(point.z < geometry->minBounds.z)
        geometry->minBounds.z = point.z;
    if(point.x > geometry->maxBounds.x)
        geometry->maxBounds.x = point.x;
    if(point.y > geometry->maxBounds.y)
        geometry->maxBounds.y = point.y;
    if(point.z > geometry->maxBounds.z)
        geometry->maxBounds.z = point.z;
}

uint32_t geometryFlags(uint32_t format)
{
    uint32_t flags = rw::Geometry::POSITIONS;
    if(format & RW_WORLD_TEXTURED)
        flags |= rw::Geometry::TEXTURED;
    if(format & RW_WORLD_TEXTURED2)
        flags |= rw::Geometry::TEXTURED2;
    if(format & RW_WORLD_PRELIT)
        flags |= rw::Geometry::PRELIT;
    if(format & RW_WORLD_NORMALS)
        flags |= rw::Geometry::NORMALS;
    if(format & RW_WORLD_LIGHT)
        flags |= rw::Geometry::LIGHT;
    if(format & RW_WORLD_MODULATE)
        flags |= rw::Geometry::MODULATE;
    return flags;
}

uint32_t sectorVertexStride(uint32_t format)
{
    uint32_t stride = sizeof(float) * 3;
    if(format & RW_WORLD_NORMALS)
        stride += 4;
    if(format & RW_WORLD_PRELIT)
        stride += 4;
    if(format & RW_WORLD_TEXTURED)
        stride += sizeof(float) * 2;
    if(format & RW_WORLD_TEXTURED2)
        stride += sizeof(float) * 2;
    return stride;
}

bool readAtomicSector(WorldReader& reader, uint32_t offset)
{
    RwChunkSpan header;
    if (!readRwChunk(reader.data, reader.length, offset, header) || header.id != RW_CHUNK_ATOMIC_SECTOR)
        return false;

    RwChunkSpan body;
    const uint32_t structOffset = offset + RW_CHUNK_HEADER_SIZE;
    if (!readRwChunk(reader.data, reader.length, structOffset, body) || body.id != 0x01 ||
        body.dataSize < RW_ATOMIC_SECTOR_STRUCT_PREFIX)
        return false;

    const uint8_t* record = reader.data + structOffset + RW_CHUNK_HEADER_SIZE;
    const int32_t matListWindowBase = static_cast<int32_t>(read32(record));
    const int32_t numTriangles = static_cast<int32_t>(read32(record + 4));
    const int32_t numVertices = static_cast<int32_t>(read32(record + 8));
    if(numTriangles < 0 || numVertices < 0)
        return false;
    if(numTriangles == 0 || numVertices == 0)
        return true;

    const uint64_t needed = static_cast<uint64_t>(RW_ATOMIC_SECTOR_STRUCT_PREFIX) +
                            static_cast<uint64_t>(numVertices) * reader.vertexStride +
                            static_cast<uint64_t>(numTriangles) * 8;
    if (needed != body.dataSize)
        return false;

    rw::Geometry* geometry =
        rw::Geometry::create(numVertices, numTriangles,
                             geometryFlags(reader.format));
    if(geometry == NULL)
        return false;
    for(int32_t i = 0; i < reader.materials->numMaterials; ++i)
        geometry->matList.appendMaterial(reader.materials->materials[i]);

    const uint32_t vertexBase = static_cast<uint32_t>(
        reader.geometry->vertices.size());
    reader.geometry->vertices.reserve(vertexBase + numVertices);

    const uint8_t* cursor = record + RW_ATOMIC_SECTOR_STRUCT_PREFIX;
    rw::V3d* positions = geometry->morphTargets[0].vertices;
    for(int32_t i = 0; i < numVertices; ++i)
    {
        RwWorldVertex position;
        position.x = readFloat(cursor);
        position.y = readFloat(cursor + 4);
        position.z = readFloat(cursor + 8);
        cursor += 12;
        positions[i].x = position.x;
        positions[i].y = position.y;
        positions[i].z = position.z;
        reader.geometry->vertices.push_back(position);
        includeBounds(reader.geometry, position);
    }

    if(reader.format & RW_WORLD_NORMALS)
    {
        rw::V3d* normals = geometry->morphTargets[0].normals;
        for(int32_t i = 0; i < numVertices; ++i)
        {
            normals[i].x = static_cast<int8_t>(cursor[0]) / 127.0f;
            normals[i].y = static_cast<int8_t>(cursor[1]) / 127.0f;
            normals[i].z = static_cast<int8_t>(cursor[2]) / 127.0f;
            cursor += 4;
        }
    }

    if(reader.format & RW_WORLD_PRELIT)
    {
        for(int32_t i = 0; i < numVertices; ++i)
        {
            geometry->colors[i].red = cursor[0];
            geometry->colors[i].green = cursor[1];
            geometry->colors[i].blue = cursor[2];
            geometry->colors[i].alpha = cursor[3];
            cursor += 4;
        }
    }

    for(int32_t set = 0; set < geometry->numTexCoordSets; ++set)
    {
        for(int32_t i = 0; i < numVertices; ++i)
        {
            geometry->texCoords[set][i].u = readFloat(cursor);
            geometry->texCoords[set][i].v = readFloat(cursor + 4);
            cursor += 8;
        }
    }

    for(int32_t i = 0; i < numTriangles; ++i)
    {
        const uint16_t v1 = read16(cursor);
        const uint16_t v0 = read16(cursor + 2);
        const uint16_t v2 = read16(cursor + 4);
        const uint16_t matId = read16(cursor + 6);
        cursor += 8;
        if(v0 >= numVertices || v1 >= numVertices || v2 >= numVertices)
        {
            geometry->destroy();
            return false;
        }
        int32_t material = matListWindowBase + matId;
        if(material < 0 || material >= geometry->matList.numMaterials)
            material = 0;
        geometry->triangles[i].v[0] = v0;
        geometry->triangles[i].v[1] = v1;
        geometry->triangles[i].v[2] = v2;
        geometry->triangles[i].matId = static_cast<uint16_t>(material);

        RwWorldTriangle triangle;
        triangle.v[0] = vertexBase + v0;
        triangle.v[1] = vertexBase + v1;
        triangle.v[2] = vertexBase + v2;
        triangle.matId = static_cast<uint16_t>(material);
        reader.geometry->triangles.push_back(triangle);
    }

    geometry->calculateBoundingSphere();
    geometry->buildMeshes();

    rw::Atomic* atomic = rw::Atomic::create();
    if(atomic == NULL)
    {
        geometry->destroy();
        return false;
    }
    atomic->setFrame(reader.frame);
    atomic->setGeometry(geometry, 0);
    geometry->destroy();
    reader.clump->addAtomic(atomic);
    reader.geometry->sectorCount++;
    return true;
}

bool isFloorTriangle(const RwWorldGeometry& geometry,
                     const RwWorldTriangle& triangle)
{
    const RwWorldVertex& a = geometry.vertices[triangle.v[0]];
    const RwWorldVertex& b = geometry.vertices[triangle.v[1]];
    const RwWorldVertex& c = geometry.vertices[triangle.v[2]];
    const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    const float nx = uy * vz - uz * vy;
    const float ny = uz * vx - ux * vz;
    const float nz = ux * vy - uy * vx;
    const float length = sqrtf(nx * nx + ny * ny + nz * nz);
    return length > 0.0f && fabsf(ny) / length >= RW_WORLD_FLOOR_NORMAL_Y;
}

bool triangleColumnHeight(const RwWorldGeometry& geometry,
                           const RwWorldTriangle& triangle,
                           float x, float z, float* height)
{
    const RwWorldVertex& a = geometry.vertices[triangle.v[0]];
    const RwWorldVertex& b = geometry.vertices[triangle.v[1]];
    const RwWorldVertex& c = geometry.vertices[triangle.v[2]];
    const float area =
        (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
    if(fabsf(area) < 1e-6f)
        return false;
    const float u =
        ((x - a.x) * (c.z - a.z) - (z - a.z) * (c.x - a.x)) / area;
    if(u < 0.0f || u > 1.0f)
        return false;
    const float v =
        ((b.x - a.x) * (z - a.z) - (b.z - a.z) * (x - a.x)) / area;
    if(v < 0.0f || u + v > 1.0f)
        return false;
    *height = a.y + (b.y - a.y) * u + (c.y - a.y) * v;
    return true;
}

bool readSector(WorldReader& reader, uint32_t offset)
{
    RwChunkSpan header;
    if (!readRwChunk(reader.data, reader.length, offset, header))
        return false;
    if (header.id == RW_CHUNK_ATOMIC_SECTOR)
        return readAtomicSector(reader, offset);
    if (header.id != RW_CHUNK_PLANE_SECTOR)
        return false;

    RwChunkSpan body;
    const uint32_t structOffset = offset + RW_CHUNK_HEADER_SIZE;
    if (!readRwChunk(reader.data, reader.length, structOffset, body) || body.id != 0x01 ||
        body.dataSize < RW_PLANE_SECTOR_STRUCT_SIZE)
        return false;

    uint32_t child = structOffset + RW_CHUNK_HEADER_SIZE + body.dataSize;
    const uint32_t end = offset + RW_CHUNK_HEADER_SIZE + header.dataSize;
    for(int side = 0; side < 2; ++side)
    {
        if(child >= end)
            return false;
        RwChunkSpan childHeader;
        if (!readRwChunk(reader.data, reader.length, child, childHeader))
            return false;
        if(!readSector(reader, child))
            return false;
        child += RW_CHUNK_HEADER_SIZE + childHeader.dataSize;
    }
    return true;
}

RwWorldVertex subtractVertex(const RwWorldVertex& a,
                             const RwWorldVertex& b)
{
    RwWorldVertex result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

RwWorldVertex addScaledVertex(const RwWorldVertex& a,
                              const RwWorldVertex& b, float scale)
{
    RwWorldVertex result;
    result.x = a.x + b.x * scale;
    result.y = a.y + b.y * scale;
    result.z = a.z + b.z * scale;
    return result;
}

float dotVertex(const RwWorldVertex& a, const RwWorldVertex& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float pointTriangleDistanceSquared(const RwWorldVertex& point,
                                   const RwWorldVertex& a,
                                   const RwWorldVertex& b,
                                   const RwWorldVertex& c)
{
    const RwWorldVertex ab = subtractVertex(b, a);
    const RwWorldVertex ac = subtractVertex(c, a);
    const RwWorldVertex ap = subtractVertex(point, a);
    const float d1 = dotVertex(ab, ap);
    const float d2 = dotVertex(ac, ap);
    if(d1 <= 0.0f && d2 <= 0.0f)
        return dotVertex(ap, ap);

    const RwWorldVertex bp = subtractVertex(point, b);
    const float d3 = dotVertex(ab, bp);
    const float d4 = dotVertex(ac, bp);
    if(d3 >= 0.0f && d4 <= d3)
        return dotVertex(bp, bp);

    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        const float amount = d1 / (d1 - d3);
        const RwWorldVertex closest = addScaledVertex(a, ab, amount);
        const RwWorldVertex delta = subtractVertex(point, closest);
        return dotVertex(delta, delta);
    }

    const RwWorldVertex cp = subtractVertex(point, c);
    const float d5 = dotVertex(ab, cp);
    const float d6 = dotVertex(ac, cp);
    if(d6 >= 0.0f && d5 <= d6)
        return dotVertex(cp, cp);

    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        const float amount = d2 / (d2 - d6);
        const RwWorldVertex closest = addScaledVertex(a, ac, amount);
        const RwWorldVertex delta = subtractVertex(point, closest);
        return dotVertex(delta, delta);
    }

    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
    {
        const RwWorldVertex bc = subtractVertex(c, b);
        const float amount = (d4 - d3) /
            ((d4 - d3) + (d5 - d6));
        const RwWorldVertex closest = addScaledVertex(b, bc, amount);
        const RwWorldVertex delta = subtractVertex(point, closest);
        return dotVertex(delta, delta);
    }

    const float denominator = 1.0f / (va + vb + vc);
    const float v = vb * denominator;
    const float w = vc * denominator;
    RwWorldVertex closest = addScaledVertex(a, ab, v);
    closest = addScaledVertex(closest, ac, w);
    const RwWorldVertex delta = subtractVertex(point, closest);
    return dotVertex(delta, delta);
}

float segmentSegmentDistanceSquared(const RwWorldVertex& p1,
                                    const RwWorldVertex& q1,
                                    const RwWorldVertex& p2,
                                    const RwWorldVertex& q2)
{
    const RwWorldVertex d1 = subtractVertex(q1, p1);
    const RwWorldVertex d2 = subtractVertex(q2, p2);
    const RwWorldVertex r = subtractVertex(p1, p2);
    const float a = dotVertex(d1, d1);
    const float e = dotVertex(d2, d2);
    const float f = dotVertex(d2, r);
    float s = 0.0f;
    float t = 0.0f;

    if(a <= 1e-12f && e <= 1e-12f)
        return dotVertex(r, r);
    if(a <= 1e-12f)
    {
        t = f / e;
        if(t < 0.0f)
            t = 0.0f;
        else if(t > 1.0f)
            t = 1.0f;
    }
    else
    {
        const float c = dotVertex(d1, r);
        if(e <= 1e-12f)
        {
            s = -c / a;
            if(s < 0.0f)
                s = 0.0f;
            else if(s > 1.0f)
                s = 1.0f;
        }
        else
        {
            const float b = dotVertex(d1, d2);
            const float denominator = a * e - b * b;
            if(denominator != 0.0f)
                s = (b * f - c * e) / denominator;
            if(s < 0.0f)
                s = 0.0f;
            else if(s > 1.0f)
                s = 1.0f;
            t = (b * s + f) / e;
            if(t < 0.0f)
            {
                t = 0.0f;
                s = -c / a;
                if(s < 0.0f)
                    s = 0.0f;
                else if(s > 1.0f)
                    s = 1.0f;
            }
            else if(t > 1.0f)
            {
                t = 1.0f;
                s = (b - c) / a;
                if(s < 0.0f)
                    s = 0.0f;
                else if(s > 1.0f)
                    s = 1.0f;
            }
        }
    }

    const RwWorldVertex c1 = addScaledVertex(p1, d1, s);
    const RwWorldVertex c2 = addScaledVertex(p2, d2, t);
    const RwWorldVertex delta = subtractVertex(c1, c2);
    return dotVertex(delta, delta);
}

bool segmentIntersectsTriangle(const RwWorldVertex& start,
                               const RwWorldVertex& end,
                               const RwWorldVertex& a,
                               const RwWorldVertex& b,
                               const RwWorldVertex& c)
{
    const RwWorldVertex direction = subtractVertex(end, start);
    const RwWorldVertex edge1 = subtractVertex(b, a);
    const RwWorldVertex edge2 = subtractVertex(c, a);
    RwWorldVertex cross;
    cross.x = direction.y * edge2.z - direction.z * edge2.y;
    cross.y = direction.z * edge2.x - direction.x * edge2.z;
    cross.z = direction.x * edge2.y - direction.y * edge2.x;
    const float determinant = dotVertex(edge1, cross);
    if(fabsf(determinant) < 1e-8f)
        return false;
    const float inverse = 1.0f / determinant;
    const RwWorldVertex fromA = subtractVertex(start, a);
    const float u = dotVertex(fromA, cross) * inverse;
    if(u < 0.0f || u > 1.0f)
        return false;
    RwWorldVertex q;
    q.x = fromA.y * edge1.z - fromA.z * edge1.y;
    q.y = fromA.z * edge1.x - fromA.x * edge1.z;
    q.z = fromA.x * edge1.y - fromA.y * edge1.x;
    const float v = dotVertex(direction, q) * inverse;
    if(v < 0.0f || u + v > 1.0f)
        return false;
    const float amount = dotVertex(edge2, q) * inverse;
    return amount >= 0.0f && amount <= 1.0f;
}

float segmentTriangleDistanceSquared(const RwWorldVertex& start,
                                     const RwWorldVertex& end,
                                     const RwWorldVertex& a,
                                     const RwWorldVertex& b,
                                     const RwWorldVertex& c)
{
    if(segmentIntersectsTriangle(start, end, a, b, c))
        return 0.0f;
    float best = pointTriangleDistanceSquared(start, a, b, c);
    const float endDistance = pointTriangleDistanceSquared(end, a, b, c);
    if(endDistance < best)
        best = endDistance;
    const float ab = segmentSegmentDistanceSquared(start, end, a, b);
    const float bc = segmentSegmentDistanceSquared(start, end, b, c);
    const float ca = segmentSegmentDistanceSquared(start, end, c, a);
    if(ab < best)
        best = ab;
    if(bc < best)
        best = bc;
    if(ca < best)
        best = ca;
    return best;
}

}

RwWorldVertex::RwWorldVertex()
    : x(0.0f), y(0.0f), z(0.0f)
{
}

RwWorldTriangle::RwWorldTriangle()
    : matId(0)
{
    v[0] = v[1] = v[2] = 0;
}

RwWorldGeometry::RwWorldGeometry()
    : format(0), sectorCount(0), haveBounds(false)
{
    minBounds.x = minBounds.y = minBounds.z = 0.0f;
    maxBounds.x = maxBounds.y = maxBounds.z = 0.0f;
}

rw::Clump*
readRwWorldClump(const uint8_t* data, uint32_t length,
                 RwWorldGeometry* geometry)
{
    if(data == NULL || geometry == NULL)
        return NULL;

    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(data), length);
    if(!rw::findChunk(&stream, rw::ID_WORLD, NULL, NULL))
        return NULL;

    uint32_t structLength = 0;
    if (!rw::findChunk(&stream, rw::ID_STRUCT, &structLength, NULL) || structLength < RW_WORLD_STRUCT_PREFIX)
        return NULL;

    const uint32_t structOffset = stream.tell();
    const uint8_t* header = data + structOffset;
    const uint32_t format = read32(header + 36);
    if(format & RW_WORLD_NATIVE)
    {
        printf("ERROR: Native RenderWare world sectors are not supported.\n");
        return NULL;
    }
    stream.seek(static_cast<int32_t>(structLength));

    uint32_t materialListLength = 0;
    if(!rw::findChunk(&stream, rw::ID_MATLIST, &materialListLength, NULL))
        return NULL;
    const uint32_t materialListOffset = stream.tell();
    std::vector<uint8_t> materialAlphaMasks;
    if(!readMaterialAlphaMasks(data, length, materialListOffset,
                               materialListLength, materialAlphaMasks))
        return NULL;

    rw::MaterialList materials;
    materials.init();
    if(rw::MaterialList::streamRead(&stream, &materials) == NULL)
        return NULL;
    if(static_cast<size_t>(materials.numMaterials) !=
       materialAlphaMasks.size())
    {
        materials.deinit();
        return NULL;
    }

    std::vector<rw::Texture*> alphaMaskedTextures;
    for(int32_t i = 0; i < materials.numMaterials; ++i)
    {
        if(materialAlphaMasks[i] == 0 ||
           materials.materials[i]->texture == NULL)
            continue;
        rw::Texture* texture = materials.materials[i]->texture;
        bool found = false;
        for(size_t j = 0; j < alphaMaskedTextures.size(); ++j)
        {
            if(alphaMaskedTextures[j] == texture)
            {
                found = true;
                break;
            }
        }
        if(!found)
            alphaMaskedTextures.push_back(texture);
    }

    rw::Clump* clump = rw::Clump::create();
    rw::Frame* frame = rw::Frame::create();
    if(clump == NULL || frame == NULL)
    {
        if(clump != NULL)
            clump->destroy();
        if(frame != NULL)
            frame->destroy();
        materials.deinit();
        return NULL;
    }
    clump->setFrame(frame);

    WorldReader reader;
    reader.data = data;
    reader.length = length;
    reader.format = format;
    reader.vertexStride = sectorVertexStride(format);
    reader.materials = &materials;
    reader.clump = clump;
    reader.frame = frame;
    reader.geometry = geometry;

    geometry->format = format;
    const bool ok = readSector(reader, stream.tell());
    materials.deinit();
    if(!ok)
    {
        clump->destroy();
        return NULL;
    }
    geometry->alphaMaskedTextures.swap(alphaMaskedTextures);
    return clump;
}

bool
rwWorldGroundHeight(const RwWorldGeometry& geometry, float x, float z,
                    float ceiling, float* height)
{

    bool found = false;
    float best = 0.0f;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        if(!isFloorTriangle(geometry, triangle))
            continue;
        float y;
        if(!triangleColumnHeight(geometry, triangle, x, z, &y))
            continue;
        if(y > ceiling)
            continue;
        if(!found || y > best)
        {
            best = y;
            found = true;
        }
    }
    if(found && height != NULL)
        *height = best;
    return found;
}

bool
rwWorldGroundColumn(const RwWorldGeometry& geometry, float x, float z,
                     float referenceY, float* height, uint16_t* material,
                     RwWorldVertex* normal)
{

    int count = 0;
    bool below = false;
    float best = 0.0f;
    uint16_t bestMaterial = 0;
    const RwWorldTriangle* bestTriangle = NULL;
    for(size_t i = 0; i < geometry.triangles.size() && count < 8; ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        float y;
        if(!triangleColumnHeight(geometry, triangle, x, z, &y))
            continue;
        if(count == 0 || (y <= referenceY && (!below || y > best)))
        {
            best = y;
            bestMaterial = triangle.matId;
            bestTriangle = &triangle;
        }
        if(y <= referenceY)
            below = true;
        ++count;
    }
    if(count == 0)
        return false;
    if(height != NULL)
        *height = best;
    if(material != NULL)
        *material = bestMaterial;
    if(normal != NULL)
    {
        const RwWorldVertex& origin = geometry.vertices[bestTriangle->v[0]];
        const RwWorldVertex a = subtractVertex(
            geometry.vertices[bestTriangle->v[1]], origin);
        const RwWorldVertex b = subtractVertex(
            geometry.vertices[bestTriangle->v[2]], origin);
        normal->x = a.y * b.z - a.z * b.y;
        normal->y = a.z * b.x - a.x * b.z;
        normal->z = a.x * b.y - a.y * b.x;
        const float length = sqrtf(dotVertex(*normal, *normal));
        normal->x /= length;
        normal->y /= length;
        normal->z /= length;
    }
    return true;
}

bool
rwWorldNearestFloor(const RwWorldGeometry& geometry, float x, float z,
                    RwWorldVertex* position)
{
    bool found = false;
    float best = 0.0f;
    RwWorldVertex nearest;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        if(!isFloorTriangle(geometry, triangle))
            continue;
        const RwWorldVertex& a = geometry.vertices[triangle.v[0]];
        const RwWorldVertex& b = geometry.vertices[triangle.v[1]];
        const RwWorldVertex& c = geometry.vertices[triangle.v[2]];
        RwWorldVertex centre;
        centre.x = (a.x + b.x + c.x) / 3.0f;
        centre.y = (a.y + b.y + c.y) / 3.0f;
        centre.z = (a.z + b.z + c.z) / 3.0f;
        const float dx = centre.x - x;
        const float dz = centre.z - z;
        const float distanceSquared = dx * dx + dz * dz;
        if(found && distanceSquared >= best)
            continue;
        best = distanceSquared;
        nearest = centre;
        found = true;
    }
    if(found && position != NULL)
        *position = nearest;
    return found;
}

bool
rwWorldRaycast(const RwWorldGeometry& geometry, const RwWorldVertex& origin,
               const RwWorldVertex& direction, float maxDistance,
               float* distance)
{

    bool found = false;
    float best = maxDistance;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        const RwWorldVertex& a = geometry.vertices[triangle.v[0]];
        const RwWorldVertex& b = geometry.vertices[triangle.v[1]];
        const RwWorldVertex& c = geometry.vertices[triangle.v[2]];
        const float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
        const float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
        const float px = direction.y * e2z - direction.z * e2y;
        const float py = direction.z * e2x - direction.x * e2z;
        const float pz = direction.x * e2y - direction.y * e2x;
        const float det = e1x * px + e1y * py + e1z * pz;
        if(fabsf(det) < 1e-7f)
            continue;
        const float inverseDet = 1.0f / det;
        const float tx = origin.x - a.x;
        const float ty = origin.y - a.y;
        const float tz = origin.z - a.z;
        const float u = (tx * px + ty * py + tz * pz) * inverseDet;
        if(u < 0.0f || u > 1.0f)
            continue;
        const float qx = ty * e1z - tz * e1y;
        const float qy = tz * e1x - tx * e1z;
        const float qz = tx * e1y - ty * e1x;
        const float v = (direction.x * qx + direction.y * qy +
                         direction.z * qz) * inverseDet;
        if(v < 0.0f || u + v > 1.0f)
            continue;
        const float t = (e2x * qx + e2y * qy + e2z * qz) * inverseDet;
        if(t <= 0.01f || t >= best)
            continue;
        best = t;
        found = true;
    }
    if(found && distance != NULL)
        *distance = best;
    return found;
}

void
appendAtomicToWorldGeometry(RwWorldGeometry* geometry, rw::Atomic* atomic)
{
    if(geometry == NULL || atomic == NULL)
        return;
    rw::Geometry* source = atomic->geometry;
    if(source == NULL || atomic->getFrame() == NULL)
        return;
    if((source->flags & rw::Geometry::NATIVE) != 0)
        return;
    const rw::V3d* positions = source->morphTargets[0].vertices;
    if(positions == NULL || source->numVertices <= 0 ||
       source->numTriangles <= 0)
        return;

    atomic->getFrame()->updateObjects();
    const rw::Matrix* ltm = atomic->getFrame()->getLTM();
    const uint32_t base =
        static_cast<uint32_t>(geometry->vertices.size());
    geometry->vertices.reserve(base + source->numVertices);
    for(int32_t i = 0; i < source->numVertices; ++i)
    {
        rw::V3d world = positions[i];
        if(ltm != NULL)
            rw::V3d::transformPoints(&world, &world, 1, ltm);
        RwWorldVertex vertex;
        vertex.x = world.x;
        vertex.y = world.y;
        vertex.z = world.z;
        geometry->vertices.push_back(vertex);
        includeBounds(geometry, vertex);
    }
    for(int32_t i = 0; i < source->numTriangles; ++i)
    {
        RwWorldTriangle triangle;
        triangle.v[0] = base + source->triangles[i].v[0];
        triangle.v[1] = base + source->triangles[i].v[1];
        triangle.v[2] = base + source->triangles[i].v[2];
        triangle.matId = source->triangles[i].matId;
        geometry->triangles.push_back(triangle);
    }
}

void
appendClumpToWorldGeometry(RwWorldGeometry* geometry, rw::Clump* clump)
{
    if(geometry == NULL || clump == NULL)
        return;
    FORLIST(link, clump->atomics)
        appendAtomicToWorldGeometry(geometry, rw::Atomic::fromClump(link));
}

bool
rwWorldProjectedSphereContact(const RwWorldGeometry& geometry,
                               const RwWorldVertex& centre, float radius,
                               RwWorldSurfaceContact* contact)
{
    if(contact == NULL || radius < 0.0f)
        return false;
    bool found = false;
    float nearest = 0.0f;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        const RwWorldVertex* vertices[3] =
        {
            &geometry.vertices[triangle.v[0]],
            &geometry.vertices[triangle.v[1]],
            &geometry.vertices[triangle.v[2]]
        };
        const RwWorldVertex a = subtractVertex(*vertices[1], *vertices[0]);
        const RwWorldVertex b = subtractVertex(*vertices[2], *vertices[0]);
        RwWorldVertex normal;
        normal.x = a.y * b.z - a.z * b.y;
        normal.y = a.z * b.x - a.x * b.z;
        normal.z = a.x * b.y - a.y * b.x;
        const float length = sqrtf(dotVertex(normal, normal));
        if(length == 0.0f)
            continue;
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
        const float distance = dotVertex(*vertices[0], normal) -
            dotVertex(centre, normal);
        const float absolute = fabsf(distance);
        if(absolute > radius || (found && absolute >= nearest))
            continue;
        const RwWorldVertex point = addScaledVertex(centre, normal, distance);

        const float nx = fabsf(normal.x);
        const float ny = fabsf(normal.y);
        const float nz = fabsf(normal.z);
        const int axis = ny < nz ? (nx < nz ? 2 : 0) : (nx < ny ? 1 : 0);
        const float pointU = axis == 0 ? point.z : point.x;
        const float pointV = axis == 1 ? point.z : point.y;
        bool inside = false;
        int previous = 2;
        for(int vertex = 0; vertex < 3; previous = vertex++)
        {
            const RwWorldVertex& first = *vertices[vertex];
            const RwWorldVertex& last = *vertices[previous];
            const float u = axis == 0 ? first.z : first.x;
            const float v = axis == 1 ? first.z : first.y;
            const float lastU = axis == 0 ? last.z : last.x;
            const float lastV = axis == 1 ? last.z : last.y;
            if(((v <= pointV && pointV < lastV) ||
                (lastV <= pointV && pointV < v)) &&
               pointU < u + (pointV - v) * (lastU - u) / (lastV - v))
                inside = !inside;
        }
        if(!inside)
            continue;
        found = true;
        nearest = absolute;
        contact->point = point;
        contact->normal = normal;
        contact->material = triangle.matId;
    }
    return found;
}

bool
rwWorldCapsuleContact(const RwWorldGeometry& geometry,
                      const RwWorldVertex& start,
                      const RwWorldVertex& end, float radius,
                      RwWorldCapsuleContact* contact)
{
    if(contact == NULL || radius < 0.0f)
        return false;

    const float radiusSquared = radius * radius;
    const RwWorldVertex travel = subtractVertex(end, start);
    bool found = false;
    float bestFraction = 1.0f;
    RwWorldVertex bestNormal;
    RwWorldVertex bestPoint;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        const RwWorldVertex& a = geometry.vertices[triangle.v[0]];
        const RwWorldVertex& b = geometry.vertices[triangle.v[1]];
        const RwWorldVertex& c = geometry.vertices[triangle.v[2]];
        if(segmentTriangleDistanceSquared(start, end, a, b, c) >
           radiusSquared)
            continue;

        float lower = 0.0f;
        float upper = 1.0f;
        if(pointTriangleDistanceSquared(start, a, b, c) >
           radiusSquared)
        {
            bool bracketed = false;
            float previous = 0.0f;
            const int sampleCount = 64;
            for(int sample = 1; sample <= sampleCount; ++sample)
            {
                const float current =
                    static_cast<float>(sample) / sampleCount;
                const RwWorldVertex centre =
                    addScaledVertex(start, travel, current);
                if(pointTriangleDistanceSquared(centre, a, b, c) <=
                   radiusSquared)
                {
                    lower = previous;
                    upper = current;
                    bracketed = true;
                    break;
                }
                previous = current;
            }
            if(!bracketed)
                continue;
            for(int iteration = 0; iteration < 24; ++iteration)
            {
                const float middle = (lower + upper) * 0.5f;
                const RwWorldVertex centre =
                    addScaledVertex(start, travel, middle);
                if(pointTriangleDistanceSquared(centre, a, b, c) <=
                   radiusSquared)
                    upper = middle;
                else
                    lower = middle;
            }
        }

        const RwWorldVertex centre =
            addScaledVertex(start, travel, upper);
        const RwWorldVertex edge1 = subtractVertex(b, a);
        const RwWorldVertex edge2 = subtractVertex(c, a);
        RwWorldVertex normal;
        normal.x = edge1.y * edge2.z - edge1.z * edge2.y;
        normal.y = edge1.z * edge2.x - edge1.x * edge2.z;
        normal.z = edge1.x * edge2.y - edge1.y * edge2.x;
        const float normalLength = sqrtf(
            normal.x * normal.x + normal.y * normal.y +
            normal.z * normal.z);
        if(normalLength <= 1e-8f)
            continue;
        normal.x /= normalLength;
        normal.y /= normalLength;
        normal.z /= normalLength;
        const RwWorldVertex fromFace = subtractVertex(centre, a);
        if(dotVertex(fromFace, normal) < 0.0f)
        {
            normal.x = -normal.x;
            normal.y = -normal.y;
            normal.z = -normal.z;
        }
        if(!found || upper < bestFraction)
        {
            found = true;
            bestFraction = upper;
            bestNormal = normal;
            bestPoint = addScaledVertex(centre, normal, -radius);
        }
    }
    if(!found)
        return false;
    contact->fraction = bestFraction;
    contact->normal = bestNormal;
    contact->point = bestPoint;
    return true;
}

bool
rwWorldCapsuleIntersects(const RwWorldGeometry& geometry,
                         const RwWorldVertex& start,
                         const RwWorldVertex& end, float radius)
{
    RwWorldCapsuleContact contact;
    return rwWorldCapsuleContact(
        geometry, start, end, radius, &contact);
}

bool
rwWorldFloorNearHeight(const RwWorldGeometry& geometry, float x, float z,
                       float referenceY, float* height)
{
    bool found = false;
    float best = 0.0f;
    float bestDistance = 0.0f;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        if(!isFloorTriangle(geometry, triangle))
            continue;
        float y;
        if(!triangleColumnHeight(geometry, triangle, x, z, &y))
            continue;
        const float distance = fabsf(y - referenceY);
        if(!found || distance < bestDistance)
        {
            best = y;
            bestDistance = distance;
            found = true;
        }
    }
    if(found && height != NULL)
        *height = best;
    return found;
}

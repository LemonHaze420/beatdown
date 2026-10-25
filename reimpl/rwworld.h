#ifndef RWWORLD_H
#define RWWORLD_H

#include <stdint.h>
#include <vector>

namespace rw { struct Atomic; struct Clump; struct Texture; }

enum RwWorldFormat
{
    RW_WORLD_TRISTRIP = 0x00000001,
    RW_WORLD_POSITIONS = 0x00000002,
    RW_WORLD_TEXTURED = 0x00000004,
    RW_WORLD_PRELIT = 0x00000008,
    RW_WORLD_NORMALS = 0x00000010,
    RW_WORLD_LIGHT = 0x00000020,
    RW_WORLD_MODULATE = 0x00000040,
    RW_WORLD_TEXTURED2 = 0x00000080,
    RW_WORLD_NATIVE = 0x01000000,
    RW_WORLD_SECTORS_OVERLAP = 0x40000000
};

const float RW_WORLD_FLOOR_NORMAL_Y = 0.5f;

struct RwWorldVertex
{
    float x;
    float y;
    float z;

    RwWorldVertex();
};

struct RwWorldTriangle
{
    uint32_t v[3];
    uint16_t matId;

    RwWorldTriangle();
};

struct RwWorldGeometry
{
    std::vector<RwWorldVertex> vertices;
    std::vector<RwWorldTriangle> triangles;
    RwWorldVertex minBounds;
    RwWorldVertex maxBounds;
    uint32_t format;

    std::vector<rw::Texture*> alphaMaskedTextures;
    int32_t sectorCount;
    bool haveBounds;

    RwWorldGeometry();
};

rw::Clump* readRwWorldClump(const uint8_t* data, uint32_t length,
                            RwWorldGeometry* geometry);

bool rwWorldGroundHeight(const RwWorldGeometry& geometry, float x, float z,
                         float ceiling, float* height);

bool rwWorldGroundColumn(const RwWorldGeometry& geometry, float x, float z,
                         float referenceY, float* height,
                         uint16_t* material, RwWorldVertex* normal = NULL);

bool rwWorldNearestFloor(const RwWorldGeometry& geometry, float x, float z,
                         RwWorldVertex* position);

bool rwWorldRaycast(const RwWorldGeometry& geometry,
                    const RwWorldVertex& origin,
                    const RwWorldVertex& direction, float maxDistance,
                    float* distance);

bool rwWorldFloorNearHeight(const RwWorldGeometry& geometry, float x, float z,
                            float referenceY, float* height);

void appendClumpToWorldGeometry(RwWorldGeometry* geometry, rw::Clump* clump);

void appendAtomicToWorldGeometry(RwWorldGeometry* geometry,
                                 rw::Atomic* atomic);

struct RwWorldCapsuleContact
{
    float fraction;
    RwWorldVertex normal;
    RwWorldVertex point;
};

struct RwWorldSurfaceContact
{
    RwWorldVertex point;
    RwWorldVertex normal;
    uint16_t material;
};

bool rwWorldProjectedSphereContact(const RwWorldGeometry& geometry,
                                    const RwWorldVertex& centre, float radius,
                                    RwWorldSurfaceContact* contact);

bool rwWorldCapsuleContact(const RwWorldGeometry& geometry,
                           const RwWorldVertex& start,
                           const RwWorldVertex& end, float radius,
                           RwWorldCapsuleContact* contact);

bool rwWorldCapsuleIntersects(const RwWorldGeometry& geometry,
                              const RwWorldVertex& start,
                              const RwWorldVertex& end, float radius);

#endif

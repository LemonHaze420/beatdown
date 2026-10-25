#ifndef ST4_H
#define ST4_H

#include "rwworld.h"
#include <rw.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

struct St4Placement
{
    int32_t objectIndex;
    float   translation[3];
    float   quaternion[4];
};

struct St4Section
{
    uint32_t offset;
    uint32_t length;
};

const size_t ST4_INLINE_COLLIDER_CAPACITY = 9;
const size_t ST4_CAMERA_COLLISION_SOURCE_CAPACITY =
    1 + ST4_INLINE_COLLIDER_CAPACITY;

struct St4InlineCollider
{
    rw::Atomic* broadphaseAtomic;
    rw::Atomic* collisionAtomic;
    RwWorldGeometry broadphaseGeometry;
    RwWorldGeometry collisionGeometry;

    St4InlineCollider()
        : broadphaseAtomic(nil), collisionAtomic(nil)
    {
    }
};

struct St4Stage
{
    std::vector<uint8_t>      fileData;
    std::vector<St4Section>   sections;

    std::vector<rw::Clump*>   fixedClumps;
    rw::Clump                *cameraCollisionClump;
    RwWorldGeometry           cameraCollisionGeometry;
    rw::Clump                *animatedClump;
    rw::Animation            *animation;
    rw::HAnimHierarchy       *hierarchy;
    St4InlineCollider         inlineColliders[ST4_INLINE_COLLIDER_CAPACITY];
    size_t                    inlineColliderCount;

    std::vector<rw::Clump*>   objects;
    std::vector<St4Placement> placements;

    St4Stage() : cameraCollisionClump(nil), animatedClump(nil), animation(nil),
                 hierarchy(nil), inlineColliderCount(0) {}
};

bool readSt4Stage(const char* path, St4Stage* stage);

void bindSt4Animation(St4Stage* stage);

void tickSt4Stage(St4Stage* stage, float dt);

void st4PlacementMatrix(const St4Placement& p, rw::Matrix* out);

const RwWorldGeometry& st4CameraCollisionGeometry(const St4Stage& stage);

size_t gatherSt4CameraCollisionGeometry(
    const St4Stage& stage, const RwWorldGeometry** geometry, size_t capacity);

void freeSt4Stage(St4Stage* stage);

#endif

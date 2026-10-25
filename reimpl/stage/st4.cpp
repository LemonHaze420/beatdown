#include "stage/st4.h"

#include <stdio.h>
#include <string.h>

using namespace rw;

static uint32_t
readU32(const uint8_t* p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static uint16_t
readU16(const uint8_t* p)
{
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

static float
readF32(const uint8_t* p)
{
    float v;
    memcpy(&v, p, 4);
    return v;
}

static Clump*
readClumpSpan(uint8_t* base, uint32_t len)
{
    if(len < 12)
        return nil;
    StreamMemory s;
    s.open(base, len);
    if(!findChunk(&s, ID_CLUMP, nil, nil))
        return nil;
    return Clump::streamRead(&s);
}

static Atomic*
firstClumpAtomic(Clump* clump)
{
    if(clump == nil)
        return nil;
    FORLIST(link, clump->atomics)
    {
        Atomic* atomic = Atomic::fromClump(link);
        if(atomic != nil && atomic->geometry != nil)
            return atomic;
    }
    return nil;
}

static void
buildInlineColliders(St4Stage* stage)
{
    stage->inlineColliderCount = 0;
    for(size_t i = 0; i < ST4_INLINE_COLLIDER_CAPACITY; ++i)
        stage->inlineColliders[i] = St4InlineCollider();
    if(stage->animatedClump == nil ||
       stage->animatedClump->getFrame() == nil)
        return;

    Frame* rootChild = stage->animatedClump->getFrame()->child;
    Atomic* broadphase = nil;
    Atomic* collision = nil;
    FORLIST(link, stage->animatedClump->atomics)
    {
        Atomic* atomic = Atomic::fromClump(link);
        Frame* frame = atomic == nil ? nil : atomic->getFrame();
        if(frame == nil)
            continue;
        if(frame->getParent() == rootChild)
            broadphase = atomic;
        else
            collision = atomic;
        if(broadphase == nil || collision == nil)
            continue;

        St4InlineCollider& collider =
            stage->inlineColliders[stage->inlineColliderCount++];
        collider.broadphaseAtomic = broadphase;
        collider.collisionAtomic = collision;
        appendAtomicToWorldGeometry(&collider.broadphaseGeometry, broadphase);
        appendAtomicToWorldGeometry(&collider.collisionGeometry, collision);
        if(stage->inlineColliderCount == ST4_INLINE_COLLIDER_CAPACITY)
            break;
        broadphase = nil;
        collision = nil;
    }
}

static bool
loadFile(const char* path, std::vector<uint8_t>& out)
{
    FILE* f = fopen(path, "rb");
    if(f == nil)
        return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size <= 0){
        fclose(f);
        return false;
    }
    out.resize((size_t)size);
    size_t got = fread(out.data(), 1, (size_t)size, f);
    fclose(f);
    return got == (size_t)size;
}

bool
readSt4Stage(const char* path, St4Stage* stage)
{
    if(!loadFile(path, stage->fileData))
        return false;

    uint8_t* data = stage->fileData.data();
    size_t total = stage->fileData.size();

    if(total < 4)
        return false;
    uint32_t sectionCount = readU32(data);
    if(sectionCount == 0 || (size_t)(4 + sectionCount * 8) > total)
        return false;
    stage->sections.reserve(sectionCount);
    for(uint32_t i = 0; i < sectionCount; i++){
        St4Section sec;
        sec.offset = readU32(data + 4 + i * 8);
        sec.length = readU32(data + 4 + i * 8 + 4);
        if((size_t)sec.offset + sec.length > total)
            return false;
        stage->sections.push_back(sec);
    }

    for(uint32_t i = 2; i <= 4 && i < sectionCount; i++){
        Clump* c = readClumpSpan(data + stage->sections[i].offset,
                                 stage->sections[i].length);
        if(c)
        {
            stage->fixedClumps.push_back(c);
            if(i == 3)
            {
                stage->cameraCollisionClump = c;
                appendAtomicToWorldGeometry(
                    &stage->cameraCollisionGeometry, firstClumpAtomic(c));
            }
        }
    }

    if(sectionCount > 5){
        stage->animatedClump = readClumpSpan(data + stage->sections[5].offset,
                                             stage->sections[5].length);
        if(stage->animatedClump == nil)
            return false;
        stage->hierarchy = HAnimHierarchy::get(stage->animatedClump);
    }

    if(sectionCount > 6 && stage->sections[6].length > 0){
        StreamMemory s;
        s.open(data + stage->sections[6].offset, stage->sections[6].length);
        if(findChunk(&s, ID_ANIMANIMATION, nil, nil))
            stage->animation = Animation::streamRead(&s);
    }

    if(sectionCount > 7){
        uint8_t* sec = data + stage->sections[7].offset;
        uint32_t secLen = stage->sections[7].length;
        if(secLen < 4)
            return false;
        uint32_t objectCount = readU32(sec);
        if((size_t)4 + objectCount * 8 > secLen)
            return false;
        stage->objects.reserve(objectCount);
        for(uint32_t i = 0; i < objectCount; i++){
            uint32_t clumpOffset = readU32(sec + 4 + i * 8);
            uint32_t clumpLength = readU32(sec + 4 + i * 8 + 4);
            if((size_t)clumpOffset + clumpLength > secLen)
                return false;
            Clump* c = readClumpSpan(sec + clumpOffset, clumpLength);
            if(c == nil)
                return false;
            stage->objects.push_back(c);
        }
    }

    if(sectionCount > 8 && stage->sections[8].length > 0){
        uint8_t* sec = data + stage->sections[8].offset;
        uint32_t secLen = stage->sections[8].length;
        if(secLen < 4 || sec[0] != 'P' || sec[1] != 'B')
            return false;
        uint16_t count = readU16(sec + 2);
        if((size_t)4 + count * 0x20 > secLen)
            return false;
        stage->placements.reserve(count);
        for(uint16_t i = 0; i < count; i++){
            const uint8_t* rec = sec + 4 + i * 0x20;
            St4Placement p;
            int32_t idx;
            memcpy(&idx, rec, 4);
            p.objectIndex = idx;
            for(int j = 0; j < 3; j++)
                p.translation[j] = readF32(rec + 4 + j * 4);
            for(int j = 0; j < 4; j++)
                p.quaternion[j] = readF32(rec + 16 + j * 4);
            stage->placements.push_back(p);
        }
    }

    return true;
}

void
bindSt4Animation(St4Stage* stage)
{
    if(stage->hierarchy && stage->animation)
        stage->hierarchy->interpolator->setCurrentAnim(stage->animation);
    if(stage->hierarchy)
    {
        stage->hierarchy->attach();
        stage->hierarchy->updateMatrices();
    }
    buildInlineColliders(stage);
}

void
tickSt4Stage(St4Stage* stage, float dt)
{
    if(stage->hierarchy && stage->animation){
        stage->hierarchy->interpolator->addTime(dt);
        stage->hierarchy->updateMatrices();
        buildInlineColliders(stage);
    }
}

void
st4PlacementMatrix(const St4Placement& p, Matrix* out)
{
    float qx = p.quaternion[0];
    float qy = p.quaternion[1];
    float qz = p.quaternion[2];
    float qw = p.quaternion[3];

    float s = 2.0f / (qw * qw + qz * qz + qx * qx + qy * qy);
    float sx = qx * s, sy = qy * s, sz = qz * s;

    out->right.x = 1.0f - (qy * sy + qz * sz);
    out->right.y = qx * sy + sz * qw;
    out->right.z = qz * sx - sy * qw;
    out->up.x    = qx * sy - sz * qw;
    out->up.y    = 1.0f - (qz * sz + qx * sx);
    out->up.z    = qy * sz + sx * qw;
    out->at.x    = qz * sx + sy * qw;
    out->at.y    = qy * sz - sx * qw;
    out->at.z    = 1.0f - (qx * sx + qy * sy);
    out->pos.x   = p.translation[0];
    out->pos.y   = p.translation[1];
    out->pos.z   = p.translation[2];
    out->optimize();
}
const RwWorldGeometry&
st4CameraCollisionGeometry(const St4Stage& stage)
{
    return stage.cameraCollisionGeometry;
}

size_t
gatherSt4CameraCollisionGeometry(
    const St4Stage& stage, const RwWorldGeometry** geometry, size_t capacity)
{
    if(geometry == nil || capacity == 0)
        return 0;
    size_t count = 0;
    if(!stage.cameraCollisionGeometry.triangles.empty())
        geometry[count++] = &stage.cameraCollisionGeometry;
    for(size_t i = 0;
        i < stage.inlineColliderCount && count < capacity; ++i)
    {
        const St4InlineCollider& collider = stage.inlineColliders[i];
        if(collider.broadphaseAtomic != nil &&
           collider.collisionAtomic != nil &&
           !collider.collisionGeometry.triangles.empty())
        {
            geometry[count++] = &collider.collisionGeometry;
        }
    }
    return count;
}

void
freeSt4Stage(St4Stage* stage)
{
    for(size_t i = 0; i < stage->fixedClumps.size(); i++)
        if(stage->fixedClumps[i])
            stage->fixedClumps[i]->destroy();
    for(size_t i = 0; i < stage->objects.size(); i++)
        if(stage->objects[i])
            stage->objects[i]->destroy();
    if(stage->animatedClump)
        stage->animatedClump->destroy();
    if(stage->animation)
        stage->animation->destroy();

    stage->fixedClumps.clear();
    stage->objects.clear();
    stage->placements.clear();
    stage->sections.clear();
    stage->fileData.clear();
    stage->cameraCollisionClump = nil;
    stage->cameraCollisionGeometry = RwWorldGeometry();
    stage->animatedClump = nil;
    stage->animation = nil;
    stage->hierarchy = nil;
    stage->inlineColliderCount = 0;
    for(size_t i = 0; i < ST4_INLINE_COLLIDER_CAPACITY; ++i)
        stage->inlineColliders[i] = St4InlineCollider();
}

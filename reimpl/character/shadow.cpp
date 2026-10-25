#include "character/shadow.h"

#include "character/character.h"
#include "rwworld.h"

#include <rw.h>

#include <cmath>
#include <vector>

namespace
{

const int SHADOW_TARGET_SIZE = 0x80;
const float SHADOW_CAMERA_VIEW_WINDOW = 20.0f;
const float SHADOW_CAMERA_NEAR_PLANE = 0.02f;
const float SHADOW_CAMERA_FAR_PLANE = 40.0f;
const float SHADOW_PROJECTION_DISTANCE = 50.0f;
const float SHADOW_LIGHT_X = -2.0f;
const float SHADOW_LIGHT_Y = 15.0f;
const float SHADOW_LIGHT_Z = -2.0f;
const float SHADOW_SURFACE_NORMAL_OFFSET = 0.05f;
const float SHADOW_SURFACE_Y_OFFSET = 0.04f;
const float SHADOW_TRIANGLE_BELOW_LIMIT = 20.0f;
const float SHADOW_TRIANGLE_MARGIN = 10.0f;
const uint8_t SHADOW_VERTEX_ALPHA = 0x90;
const size_t SHADOW_VERTEX_BATCH = 300;

struct ShadowState
{
    CharacterScene caster;
    rw::Camera* camera;
    rw::Raster* zBuffer;
    rw::World* world;
    std::vector<rw::Raster*> targets;
    std::vector<uint8_t> validTargets;
    rw::RWDEVICE::Im3DVertex vertices[SHADOW_VERTEX_BATCH];
    size_t vertexCount;
    bool initialized;

    ShadowState()
        : camera(NULL), zBuffer(NULL), world(NULL), vertexCount(0),
          initialized(false)
    {
    }
};

ShadowState g_shadow;

rw::V3d subtract(const rw::V3d& a, const rw::V3d& b)
{
    const rw::V3d value = { a.x - b.x, a.y - b.y, a.z - b.z };
    return value;
}

bool normalize(rw::V3d& value)
{
    const float lengthSquared = rw::dot(value, value);
    if(lengthSquared <= 0.000001f)
        return false;
    const float inverseLength = 1.0f / sqrtf(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    return true;
}

void shadowBasis(rw::V3d& right, rw::V3d& up, rw::V3d& at)
{

    at = { -SHADOW_LIGHT_X, -SHADOW_LIGHT_Y, -SHADOW_LIGHT_Z };
    normalize(at);
    right = { at.z, 0.0f, -at.x };
    normalize(right);
    up = rw::cross(at, right);
    normalize(up);
}

void positionShadowCamera(const CharacterShadowActor& actor)
{
    rw::V3d right;
    rw::V3d up;
    rw::V3d at;
    shadowBasis(right, up, at);

    rw::Frame* frame = g_shadow.camera->getFrame();
    frame->matrix.right = right;
    frame->matrix.up = up;
    frame->matrix.at = at;
    frame->matrix.pos =
    {
        actor.x - at.x * (SHADOW_CAMERA_FAR_PLANE * 0.5f),
        actor.y - at.y * (SHADOW_CAMERA_FAR_PLANE * 0.5f),
        actor.z - at.z * (SHADOW_CAMERA_FAR_PLANE * 0.5f)
    };
    frame->matrix.optimize();
    frame->updateObjects();
}

bool ensureTargets(size_t count)
{
    while(g_shadow.targets.size() < count)
    {
        rw::Raster* target = rw::Raster::create(
            SHADOW_TARGET_SIZE, SHADOW_TARGET_SIZE, 32,
            rw::Raster::C8888 | rw::Raster::CAMERATEXTURE);
        if(target == NULL)
            return false;
        g_shadow.targets.push_back(target);
    }
    g_shadow.validTargets.resize(count, 0);
    return true;
}

void renderProjectionBatch(void)
{
    if(g_shadow.vertexCount == 0)
        return;
    rw::im3d::Transform(
        g_shadow.vertices, static_cast<int32_t>(g_shadow.vertexCount), NULL,
        rw::im3d::VERTEXUV | rw::im3d::VERTEXXYZ |
            rw::im3d::VERTEXRGBA);
    rw::im3d::RenderPrimitive(rw::PRIMTYPETRILIST);
    rw::im3d::End();
    g_shadow.vertexCount = 0;
}

bool triangleIntersectsShadowBounds(
    const rw::V3d& a, const rw::V3d& b, const rw::V3d& c,
    const CharacterShadowActor& actor, const rw::V3d& at)
{
    rw::V3d minimum =
    {
        actor.x - SHADOW_TRIANGLE_MARGIN,
        actor.y - SHADOW_TRIANGLE_MARGIN,
        actor.z - SHADOW_TRIANGLE_MARGIN
    };
    rw::V3d maximum =
    {
        actor.x + SHADOW_TRIANGLE_MARGIN,
        actor.y + SHADOW_TRIANGLE_MARGIN,
        actor.z + SHADOW_TRIANGLE_MARGIN
    };
    const float extension[3] =
    {
        at.x * SHADOW_PROJECTION_DISTANCE,
        at.y * SHADOW_PROJECTION_DISTANCE,
        at.z * SHADOW_PROJECTION_DISTANCE
    };
    float* minimumValues = &minimum.x;
    float* maximumValues = &maximum.x;
    for(int axis = 0; axis < 3; ++axis)
    {
        if(extension[axis] < 0.0f)
            minimumValues[axis] += extension[axis];
        else
            maximumValues[axis] += extension[axis];
    }

    const float triangleMinimum[3] =
    {
        fminf(a.x, fminf(b.x, c.x)),
        fminf(a.y, fminf(b.y, c.y)),
        fminf(a.z, fminf(b.z, c.z))
    };
    const float triangleMaximum[3] =
    {
        fmaxf(a.x, fmaxf(b.x, c.x)),
        fmaxf(a.y, fmaxf(b.y, c.y)),
        fmaxf(a.z, fmaxf(b.z, c.z))
    };
    for(int axis = 0; axis < 3; ++axis)
    {
        if(triangleMaximum[axis] < minimumValues[axis] ||
           triangleMinimum[axis] > maximumValues[axis])
        {
            return false;
        }
    }
    return true;
}

void appendProjectedTriangle(
    const rw::V3d source[3], const rw::V3d& normal,
    const rw::V3d& eye, const rw::V3d& right, const rw::V3d& up,
    const rw::V3d& at)
{
    rw::V3d positions[3];
    float projected[3][3];
    const float textureScale = -0.5f /
        (SHADOW_CAMERA_VIEW_WINDOW * 1.01f);
    const float depthScale = 1.0f /
        (SHADOW_PROJECTION_DISTANCE +
         SHADOW_CAMERA_VIEW_WINDOW * 1.01f);
    for(int i = 0; i < 3; ++i)
    {

        positions[i] =
        {
            source[i].x + normal.x * SHADOW_SURFACE_NORMAL_OFFSET,
            source[i].y + normal.y * SHADOW_SURFACE_NORMAL_OFFSET +
                SHADOW_SURFACE_Y_OFFSET,
            source[i].z + normal.z * SHADOW_SURFACE_NORMAL_OFFSET
        };
        const rw::V3d relative = subtract(positions[i], eye);
        projected[i][0] =
            rw::dot(relative, right) * textureScale + 0.499f;
        projected[i][1] =
            rw::dot(relative, up) * textureScale + 0.489f;
        projected[i][2] = rw::dot(relative, at) * depthScale;
    }

    if((projected[0][2] < 0.0f && projected[1][2] < 0.0f &&
        projected[2][2] < 0.0f) ||
       (projected[0][0] < 0.0f && projected[1][0] < 0.0f &&
        projected[2][0] < 0.0f) ||
       (projected[0][0] > 1.0f && projected[1][0] > 1.0f &&
        projected[2][0] > 1.0f) ||
       (projected[0][1] < 0.0f && projected[1][1] < 0.0f &&
        projected[2][1] < 0.0f) ||
       (projected[0][1] > 1.0f && projected[1][1] > 1.0f &&
        projected[2][1] > 1.0f))
    {
        return;
    }

    if(g_shadow.vertexCount + 3 > SHADOW_VERTEX_BATCH)
        renderProjectionBatch();
    for(int i = 0; i < 3; ++i)
    {
        float falloff = 1.0f - projected[i][2] * projected[i][2];
        if(falloff < 0.0f)
            falloff = 0.0f;
        const uint8_t color = static_cast<uint8_t>(
            falloff * static_cast<float>(SHADOW_VERTEX_ALPHA));
        rw::RWDEVICE::Im3DVertex& vertex =
            g_shadow.vertices[g_shadow.vertexCount++];
        vertex.setX(positions[i].x);
        vertex.setY(positions[i].y);
        vertex.setZ(positions[i].z);
        vertex.setColor(color, color, color, color);
        vertex.setU(projected[i][0]);
        vertex.setV(projected[i][1]);
    }
}

void renderProjectedTarget(
    const RwWorldGeometry& geometry, const CharacterShadowActor& actor,
    rw::Raster* target)
{
    rw::V3d right;
    rw::V3d up;
    rw::V3d at;
    shadowBasis(right, up, at);
    const rw::V3d eye =
    {
        actor.x - at.x * (SHADOW_CAMERA_FAR_PLANE * 0.5f),
        actor.y - at.y * (SHADOW_CAMERA_FAR_PLANE * 0.5f),
        actor.z - at.z * (SHADOW_CAMERA_FAR_PLANE * 0.5f)
    };

    rw::SetRenderStatePtr(rw::TEXTURERASTER, target);
    g_shadow.vertexCount = 0;
    for(size_t i = 0; i < geometry.triangles.size(); ++i)
    {
        const RwWorldTriangle& triangle = geometry.triangles[i];
        if(triangle.v[0] >= geometry.vertices.size() ||
           triangle.v[1] >= geometry.vertices.size() ||
           triangle.v[2] >= geometry.vertices.size())
        {
            continue;
        }
        const RwWorldVertex& sourceA = geometry.vertices[triangle.v[0]];
        const RwWorldVertex& sourceB = geometry.vertices[triangle.v[1]];
        const RwWorldVertex& sourceC = geometry.vertices[triangle.v[2]];
        const rw::V3d a = { sourceA.x, sourceA.y, sourceA.z };
        const rw::V3d b = { sourceB.x, sourceB.y, sourceB.z };
        const rw::V3d c = { sourceC.x, sourceC.y, sourceC.z };
        if(actor.y - b.y > SHADOW_TRIANGLE_BELOW_LIMIT ||
           !triangleIntersectsShadowBounds(a, b, c, actor, at))
        {
            continue;
        }

        rw::V3d normal =
            rw::cross(subtract(b, a), subtract(c, a));
        if(!normalize(normal) || rw::dot(normal, eye) > -0.2f)
            continue;
        const rw::V3d vertices[3] = { a, b, c };
        appendProjectedTriangle(
            vertices, normal, eye, right, up, at);
    }
    renderProjectionBatch();
}

void setShadowCasterVisible(bool visible)
{
    for(size_t i = 0; i < g_shadow.caster.clumps.size(); ++i)
    {
        rw::Clump* clump = g_shadow.caster.clumps[i];
        FORLIST(link, clump->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(link);
            uint32_t flags = atomic->getFlags();
            if(visible)
                flags |= rw::Atomic::RENDER;
            else
                flags &= ~rw::Atomic::RENDER;
            atomic->setFlags(flags);
        }
    }
}

}

bool
initializeCharacterShadows(const char* dataArchivePath, rw::World* world)
{
    terminateCharacterShadows();
    if(world == NULL ||
       !loadCharacterShadowScene(dataArchivePath, &g_shadow.caster))
        return false;

    g_shadow.camera = rw::Camera::create();
    rw::Frame* frame = rw::Frame::create();
    g_shadow.zBuffer = rw::Raster::create(
        SHADOW_TARGET_SIZE, SHADOW_TARGET_SIZE, 0, rw::Raster::ZBUFFER);
    if(g_shadow.camera == NULL || frame == NULL || g_shadow.zBuffer == NULL)
    {
        if(frame != NULL)
            frame->destroy();
        terminateCharacterShadows();
        return false;
    }

    g_shadow.camera->setFrame(frame);
    g_shadow.camera->zBuffer = g_shadow.zBuffer;
    g_shadow.camera->setProjection(rw::Camera::PARALLEL);
    g_shadow.camera->setNearPlane(SHADOW_CAMERA_NEAR_PLANE);
    g_shadow.camera->setFarPlane(SHADOW_CAMERA_FAR_PLANE);
    const rw::V2d viewWindow =
    {
        SHADOW_CAMERA_VIEW_WINDOW,
        SHADOW_CAMERA_VIEW_WINDOW
    };
    g_shadow.camera->setViewWindow(&viewWindow);
    g_shadow.world = world;
    g_shadow.world->addCamera(g_shadow.camera);
    for(size_t i = 0; i < g_shadow.caster.clumps.size(); ++i)
        g_shadow.world->addClump(g_shadow.caster.clumps[i]);
    setShadowCasterVisible(false);
    g_shadow.initialized = true;
    return true;
}

void
terminateCharacterShadows(void)
{
    if(g_shadow.world != NULL)
    {
        if(g_shadow.camera != NULL)
            g_shadow.world->removeCamera(g_shadow.camera);
        for(size_t i = 0; i < g_shadow.caster.clumps.size(); ++i)
            g_shadow.world->removeClump(g_shadow.caster.clumps[i]);
        g_shadow.world = NULL;
    }
    if(g_shadow.camera != NULL)
    {
        g_shadow.camera->frameBuffer = NULL;
        g_shadow.camera->zBuffer = NULL;
        rw::Frame* frame = g_shadow.camera->getFrame();
        g_shadow.camera->setFrame(NULL);
        if(frame != NULL)
            frame->destroy();
        g_shadow.camera->destroy();
        g_shadow.camera = NULL;
    }
    if(g_shadow.zBuffer != NULL)
    {
        g_shadow.zBuffer->destroy();
        g_shadow.zBuffer = NULL;
    }
    for(size_t i = 0; i < g_shadow.targets.size(); ++i)
        g_shadow.targets[i]->destroy();
    g_shadow.targets.clear();
    g_shadow.validTargets.clear();
    freeCharacterScene(&g_shadow.caster);
    g_shadow.vertexCount = 0;
    g_shadow.initialized = false;
}

void
captureCharacterShadows(const CharacterShadowActor* actors, size_t actorCount)
{
    if(!g_shadow.initialized || actors == NULL ||
       !ensureTargets(actorCount))
    {
        return;
    }

    const uint32_t oldZTest = rw::GetRenderState(rw::ZTESTENABLE);
    const uint32_t oldZWrite = rw::GetRenderState(rw::ZWRITEENABLE);
    const uint32_t oldCull = rw::GetRenderState(rw::CULLMODE);
    const uint32_t oldVertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
    rw::SetRenderState(rw::ZTESTENABLE, 1);
    rw::SetRenderState(rw::ZWRITEENABLE, 1);
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);
    rw::SetRenderState(rw::VERTEXALPHA, 0);

    for(size_t i = 0; i < actorCount; ++i)
    {
        g_shadow.validTargets[i] = 0;
        if(actors[i].scene == NULL ||
           !copyCharacterPoseToShadowScene(
               actors[i].scene, &g_shadow.caster))
        {
            continue;
        }
        positionShadowCamera(actors[i]);
        g_shadow.camera->frameBuffer = g_shadow.targets[i];
        const rw::RGBA clear = rw::makeRGBA(255, 255, 255, 0);
        g_shadow.camera->clear(
            const_cast<rw::RGBA*>(&clear),
            rw::Camera::CLEARIMAGE | rw::Camera::CLEARZ);
        setShadowCasterVisible(true);
        g_shadow.camera->beginUpdate();
        for(size_t clump = 0; clump < g_shadow.caster.clumps.size(); ++clump)
            g_shadow.caster.clumps[clump]->render();
        g_shadow.camera->endUpdate();
        setShadowCasterVisible(false);
        g_shadow.validTargets[i] = 1;
    }
    g_shadow.camera->frameBuffer = NULL;

    rw::SetRenderState(rw::ZTESTENABLE, oldZTest);
    rw::SetRenderState(rw::ZWRITEENABLE, oldZWrite);
    rw::SetRenderState(rw::CULLMODE, oldCull);
    rw::SetRenderState(rw::VERTEXALPHA, oldVertexAlpha);
}

void
renderCharacterShadows(const RwWorldGeometry& geometry,
                       const CharacterShadowActor* actors, size_t actorCount)
{
    if(!g_shadow.initialized || actors == NULL)
        return;
    if(actorCount > g_shadow.targets.size())
        actorCount = g_shadow.targets.size();

    void* oldTexture = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    const uint32_t oldAddressU = rw::GetRenderState(rw::TEXTUREADDRESSU);
    const uint32_t oldAddressV = rw::GetRenderState(rw::TEXTUREADDRESSV);
    const uint32_t oldFilter = rw::GetRenderState(rw::TEXTUREFILTER);
    const uint32_t oldVertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
    const uint32_t oldSourceBlend = rw::GetRenderState(rw::SRCBLEND);
    const uint32_t oldDestinationBlend = rw::GetRenderState(rw::DESTBLEND);
    const uint32_t oldZTest = rw::GetRenderState(rw::ZTESTENABLE);
    const uint32_t oldZWrite = rw::GetRenderState(rw::ZWRITEENABLE);
    const uint32_t oldFog = rw::GetRenderState(rw::FOGENABLE);
    const uint32_t oldCull = rw::GetRenderState(rw::CULLMODE);

    rw::SetRenderState(rw::TEXTUREADDRESSU, rw::Texture::CLAMP);
    rw::SetRenderState(rw::TEXTUREADDRESSV, rw::Texture::CLAMP);
    rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::LINEAR);
    rw::SetRenderState(rw::VERTEXALPHA, 1);

    rw::SetRenderState(rw::SRCBLEND, rw::BLENDZERO);
    rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
    rw::SetRenderState(rw::ZTESTENABLE, 1);
    rw::SetRenderState(rw::ZWRITEENABLE, 1);
    rw::SetRenderState(rw::FOGENABLE, 0);
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);

    for(size_t i = 0; i < actorCount; ++i)
    {
        if(i < g_shadow.validTargets.size() &&
           g_shadow.validTargets[i] != 0)
        {
            renderProjectedTarget(
                geometry, actors[i], g_shadow.targets[i]);
        }
    }

    rw::SetRenderStatePtr(rw::TEXTURERASTER, oldTexture);
    rw::SetRenderState(rw::TEXTUREADDRESSU, oldAddressU);
    rw::SetRenderState(rw::TEXTUREADDRESSV, oldAddressV);
    rw::SetRenderState(rw::TEXTUREFILTER, oldFilter);
    rw::SetRenderState(rw::VERTEXALPHA, oldVertexAlpha);
    rw::SetRenderState(rw::SRCBLEND, oldSourceBlend);
    rw::SetRenderState(rw::DESTBLEND, oldDestinationBlend);
    rw::SetRenderState(rw::ZTESTENABLE, oldZTest);
    rw::SetRenderState(rw::ZWRITEENABLE, oldZWrite);
    rw::SetRenderState(rw::FOGENABLE, oldFog);
    rw::SetRenderState(rw::CULLMODE, oldCull);
}

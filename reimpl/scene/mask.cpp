#include "scene/mask.h"

#include "game/game.h"

#include <rw.h>
#include <skeleton.h>

#include <stdint.h>

namespace
{

const float MASK_VIRTUAL_WIDTH = 640.0f;
const float MASK_VIRTUAL_HEIGHT = 256.0f;
const float MASK_MODE_ONE_CROP = 42.0f;

const int MASK_BLEND_ALPHA = 191;

struct MaskPoint
{
    float x;
    float y;
};

struct SceneMaskRuntime
{
    rw::Texture* captureTexture;
    bool offsetMode;
    uint8_t frameCycle;
    bool effectActive;
    int effectMode;
};

SceneMaskRuntime g_sceneMask =
{
    NULL, false, 0, false, SCENE_MASK_NONE
};

const MaskPoint CYCLING_QUADS[4][4] =
{
    {
        { -2.0f,  0.0f }, { -2.0f,   0.0f },
        { 638.0f, 0.0f }, { 638.0f,  0.0f }
    },
    {
        { 0.0f,  -1.0f }, { 0.0f,   223.0f },
        { 640.0f, 223.0f }, { 640.0f, -1.0f }
    },
    {
        { 2.0f,  0.0f }, { 2.0f,   0.0f },
        { 642.0f, 0.0f }, { 642.0f, 0.0f }
    },
    {
        { 0.0f,  1.0f }, { 0.0f,   225.0f },
        { 640.0f, 225.0f }, { 640.0f, 1.0f }
    }
};

void releaseCaptureTexture(void)
{
    if(g_sceneMask.captureTexture == NULL)
        return;
    g_sceneMask.captureTexture->destroy();
    g_sceneMask.captureTexture = NULL;
}

bool ensureCaptureTexture(const rw::Raster& frameBuffer)
{
    if(g_sceneMask.captureTexture != NULL)
    {
        const rw::Raster* capture = g_sceneMask.captureTexture->raster;
        if(capture != NULL && capture->width == frameBuffer.width &&
           capture->height == frameBuffer.height &&
           capture->depth == frameBuffer.depth)
        {
            return true;
        }
        releaseCaptureTexture();
    }

    rw::Raster* capture = rw::Raster::create(
        frameBuffer.width, frameBuffer.height, frameBuffer.depth,
        rw::Raster::CAMERATEXTURE);
    if(capture == NULL)
        return false;

    g_sceneMask.captureTexture = rw::Texture::create(capture);
    if(g_sceneMask.captureTexture == NULL)
    {
        capture->destroy();
        return false;
    }
    g_sceneMask.captureTexture->setFilter(rw::Texture::LINEAR);
    g_sceneMask.captureTexture->setAddressU(rw::Texture::CLAMP);
    g_sceneMask.captureTexture->setAddressV(rw::Texture::CLAMP);
    return true;
}

bool captureCurrentFrame(void)
{
    if(rw::engine == NULL || rw::engine->currentCamera == NULL)
        return false;
    rw::Raster* frameBuffer = rw::engine->currentCamera->frameBuffer;
    if(frameBuffer == NULL || !ensureCaptureTexture(*frameBuffer))
        return false;

    rw::Raster* capture = g_sceneMask.captureTexture->raster;
    if(rw::Raster::pushContext(capture) == NULL)
        return false;
    const bool copied = frameBuffer->renderFast(0, 0) != 0;
    rw::Raster::popContext();
    return copied;
}

bool selectQuad(int mode, MaskPoint* points, float* bottomV)
{
    *bottomV = 1.0f;
    switch(mode)
    {
    case SCENE_MASK_FULL_FRAME:
    {
        const float bottom = g_sceneMask.offsetMode ?
            MASK_VIRTUAL_HEIGHT - MASK_MODE_ONE_CROP : MASK_VIRTUAL_HEIGHT;
        points[0] = MaskPoint{ 0.0f, 0.0f };
        points[1] = MaskPoint{ 0.0f, bottom };
        points[2] = MaskPoint{ MASK_VIRTUAL_WIDTH, bottom };
        points[3] = MaskPoint{ MASK_VIRTUAL_WIDTH, 0.0f };
        if(g_sceneMask.offsetMode)
            *bottomV = bottom / MASK_VIRTUAL_HEIGHT;
        return true;
    }
    case SCENE_MASK_INSET:
        points[0] = MaskPoint{ 4.0f, 2.0f };
        points[1] = MaskPoint{ 4.0f, MASK_VIRTUAL_HEIGHT - 2.0f };
        points[2] = MaskPoint{ 636.0f, MASK_VIRTUAL_HEIGHT - 2.0f };
        points[3] = MaskPoint{ 636.0f, 2.0f };
        return true;
    case SCENE_MASK_HORIZONTAL_INSET:
        points[0] = MaskPoint{ 4.0f, 0.0f };
        points[1] = MaskPoint{ 4.0f, MASK_VIRTUAL_HEIGHT };
        points[2] = MaskPoint{ 636.0f, MASK_VIRTUAL_HEIGHT };
        points[3] = MaskPoint{ 636.0f, 0.0f };
        return true;
    case SCENE_MASK_CYCLING:
    {
        g_sceneMask.frameCycle =
            static_cast<uint8_t>((g_sceneMask.frameCycle + 1) & 0x7);
        const int frame = g_sceneMask.frameCycle >> 1;
        for(int i = 0; i < 4; ++i)
            points[i] = CYCLING_QUADS[frame][i];
        return true;
    }
    default:
        return false;
    }
}

ImTextureRef captureTextureRef(void)
{
    return ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(g_sceneMask.captureTexture)));
}

ImVec2 screenPoint(const MaskPoint& point, const ImVec2& origin,
                   float scaleX, float scaleY)
{
    return ImVec2(origin.x + point.x * scaleX,
                  origin.y + point.y * scaleY);
}

}

int
sceneMaskMode(void)
{
    if(g_sceneMask.effectActive)
        return g_sceneMask.effectMode;
    const int battleMode = g_game.battleFeedbackMode();
    return battleMode != SCENE_MASK_NONE ?
        battleMode : g_game.sceneSequence().slowMotion();
}

void
setSceneMaskOffsetMode(bool enabled)
{
    g_sceneMask.offsetMode = enabled;
}
void
setSceneMaskEffectMode(int mode, bool active)
{
    g_sceneMask.effectMode = mode;
    g_sceneMask.effectActive = active;
}

void
resetSceneMask(void)
{
    releaseCaptureTexture();
    g_sceneMask.offsetMode = false;
    g_sceneMask.frameCycle = 0;
    g_sceneMask.effectActive = false;
    g_sceneMask.effectMode = SCENE_MASK_NONE;
}

void
drawSceneMask(void)
{
    MaskPoint points[4];
    float bottomV;
    const int mode = sceneMaskMode();
    if(!selectQuad(mode, points, &bottomV))
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if(drawList == NULL || !captureCurrentFrame())
        return;

    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float scaleX = size.x / MASK_VIRTUAL_WIDTH;
    const float scaleY = size.y / MASK_VIRTUAL_HEIGHT;
    const ImU32 tint = IM_COL32(255, 255, 255, MASK_BLEND_ALPHA);

    drawList->AddImageQuad(
        captureTextureRef(),
        screenPoint(points[0], origin, scaleX, scaleY),
        screenPoint(points[3], origin, scaleX, scaleY),
        screenPoint(points[2], origin, scaleX, scaleY),
        screenPoint(points[1], origin, scaleX, scaleY),
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 0.0f),
        ImVec2(1.0f, bottomV), ImVec2(0.0f, bottomV), tint);
}

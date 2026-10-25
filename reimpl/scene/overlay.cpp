#include "scene/overlay.h"

#include "game/game.h"

#include <rw.h>
#include <skeleton.h>

namespace
{

const float OVERLAY_SCREEN_WIDTH = 640.0f;
const float OVERLAY_SCREEN_HEIGHT = 480.0f;
const int OVERLAY_ALPHA_MAX = 0xff;
const int OVERLAY_ALPHA_STEP = 8;
bool g_sceneLetterboxActive = false;

struct OverlayRuntime
{
    bool wasActive;
    int kind;
    int alpha;
    int lastDrawFrame;
};

OverlayRuntime g_overlayRuntime =
{
    false, SCENE_OVERLAY_NONE, 0, -1
};

struct VirtualRect
{
    float left;
    float top;
    float right;
    float bottom;
};

const VirtualRect LETTERBOX_RECTS[2] =
{
    { 0.0f, -32.0f, 640.0f,  68.0f },
    { 0.0f, 400.0f, 640.0f, 512.0f }
};

const VirtualRect FADE_RECT =
{
    0.0f, -16.0f, 640.0f, 496.0f
};

int overlayOperand(const SceneSequencer& sequencer, int kind)
{
    if(kind == SCENE_OVERLAY_LETTERBOX)
        return sequencer.letterboxState();
    if(kind == SCENE_OVERLAY_FADE)
        return sequencer.overlayFadeState();
    return 0;
}

int advanceOverlayAlpha(const SceneSequencer& sequencer, int kind)
{
    if(kind != SCENE_OVERLAY_LETTERBOX && kind != SCENE_OVERLAY_FADE)
    {
        g_overlayRuntime.wasActive = false;
        return 0;
    }

    const int operand = overlayOperand(sequencer, kind);
    if(!g_overlayRuntime.wasActive || g_overlayRuntime.kind != kind)
    {

        g_overlayRuntime.alpha = operand == 0 ? 0 : OVERLAY_ALPHA_MAX;
    }
    else if(operand != 0)
    {
        g_overlayRuntime.alpha = OVERLAY_ALPHA_MAX;
    }

    g_overlayRuntime.wasActive = true;
    g_overlayRuntime.kind = kind;

    const int drawFrame = ImGui::GetFrameCount();
    if(drawFrame != g_overlayRuntime.lastDrawFrame)
    {
        g_overlayRuntime.lastDrawFrame = drawFrame;
        if(sequencer.overlayFadeStopping())
        {

            g_overlayRuntime.alpha -= OVERLAY_ALPHA_STEP;
            if(g_overlayRuntime.alpha < 0)
                g_overlayRuntime.alpha = 0;
        }
        else if(operand == 0 && g_overlayRuntime.alpha < OVERLAY_ALPHA_MAX)
        {

            g_overlayRuntime.alpha += OVERLAY_ALPHA_STEP;
            if(g_overlayRuntime.alpha > OVERLAY_ALPHA_MAX)
                g_overlayRuntime.alpha = OVERLAY_ALPHA_MAX;
        }
    }
    return g_overlayRuntime.alpha;
}

void drawVirtualRect(ImDrawList* drawList, const ImVec2& origin,
                     float scaleX, float scaleY, const VirtualRect& rectangle,
                     ImU32 colour)
{
    const ImVec2 minimum(origin.x + rectangle.left * scaleX,
                         origin.y + rectangle.top * scaleY);
    const ImVec2 maximum(origin.x + rectangle.right * scaleX,
                         origin.y + rectangle.bottom * scaleY);
    drawList->AddRectFilled(minimum, maximum, colour);
}

}
void
setSceneLetterboxActive(bool active)
{
    g_sceneLetterboxActive = active;
}

void
drawSceneOverlay(void)
{
    const SceneSequencer& sequencer = g_game.sceneSequence();
    if(!g_sceneLetterboxActive && !sequencer.overlayActive())
    {
        g_overlayRuntime.wasActive = false;
        return;
    }

    if(g_sceneLetterboxActive)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if(drawList == NULL)
            return;
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const float scaleX = size.x / OVERLAY_SCREEN_WIDTH;
        const float scaleY = size.y / OVERLAY_SCREEN_HEIGHT;
        const ImU32 colour = IM_COL32(0, 0, 0, OVERLAY_ALPHA_MAX);
        drawVirtualRect(drawList, origin, scaleX, scaleY,
                        LETTERBOX_RECTS[0], colour);
        drawVirtualRect(drawList, origin, scaleX, scaleY,
                        LETTERBOX_RECTS[1], colour);
        return;
    }

    const int kind = sequencer.overlayKind();
    const int alpha = advanceOverlayAlpha(sequencer, kind);
    if(alpha <= 0)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if(drawList == NULL)
        return;
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float scaleX = size.x / OVERLAY_SCREEN_WIDTH;
    const float scaleY = size.y / OVERLAY_SCREEN_HEIGHT;
    const ImU32 colour = IM_COL32(0, 0, 0, alpha);

    if(kind == SCENE_OVERLAY_LETTERBOX)
    {
        drawVirtualRect(drawList, origin, scaleX, scaleY,
                        LETTERBOX_RECTS[0], colour);
        drawVirtualRect(drawList, origin, scaleX, scaleY,
                        LETTERBOX_RECTS[1], colour);
    }
    else if(kind == SCENE_OVERLAY_FADE)
    {
        drawVirtualRect(drawList, origin, scaleX, scaleY, FADE_RECT, colour);
    }
}

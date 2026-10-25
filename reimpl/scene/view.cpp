#include "scene/view.h"

#include "scene/task.h"

#include <rw.h>
#include <skeleton.h>

#include <stdint.h>
#include "game/game.h"

namespace
{

ManualCameraControl g_sceneManualCamera =
{
    0.0f,
    MANUAL_CAMERA_DEFAULT_POLAR_DEGREES,
    false,
    false
};

rw::Texture* g_characterSelectCapture;

void
releaseCharacterSelectCapture(void)
{
    if(g_characterSelectCapture == NULL)
        return;
    g_characterSelectCapture->destroy();
    g_characterSelectCapture = NULL;
}

bool
ensureCharacterSelectCapture(const rw::Raster& frameBuffer)
{
    if(g_characterSelectCapture != NULL)
    {
        const rw::Raster* capture = g_characterSelectCapture->raster;
        if(capture != NULL && capture->width == frameBuffer.width &&
           capture->height == frameBuffer.height &&
           capture->depth == frameBuffer.depth)
            return true;
        releaseCharacterSelectCapture();
    }

    rw::Raster* capture = rw::Raster::create(
        frameBuffer.width, frameBuffer.height, frameBuffer.depth,
        rw::Raster::CAMERATEXTURE);
    if(capture == NULL)
        return false;
    g_characterSelectCapture = rw::Texture::create(capture);
    if(g_characterSelectCapture == NULL)
    {
        capture->destroy();
        return false;
    }
    g_characterSelectCapture->setFilter(rw::Texture::LINEAR);
    g_characterSelectCapture->setAddressU(rw::Texture::CLAMP);
    g_characterSelectCapture->setAddressV(rw::Texture::CLAMP);
    return true;
}

bool
captureCharacterSelectFrame(void)
{
    if(rw::engine == NULL || rw::engine->currentCamera == NULL)
        return false;
    rw::Raster* frameBuffer = rw::engine->currentCamera->frameBuffer;
    if(frameBuffer == NULL || !ensureCharacterSelectCapture(*frameBuffer))
        return false;

    rw::Raster* capture = g_characterSelectCapture->raster;
    if(rw::Raster::pushContext(capture) == NULL)
        return false;
    const bool copied = frameBuffer->renderFast(0, 0) != 0;
    rw::Raster::popContext();
    return copied;
}

ImTextureRef
characterSelectCaptureRef(void)
{
    return ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(g_characterSelectCapture)));
}

const SceneSequencer& sceneSequence(void)
{
    return g_game.sceneSequence();
}

}

int
sceneHudMode(void)
{
    return sceneSequence().hudMode();
}

bool
sceneHudVisible(void)
{
    return sceneSequence().hudVisible();
}

int
sceneHudPrideSide(void)
{
    return sceneSequence().hudPrideSide();
}

bool
sceneManualCamera(void)
{
    return sceneSequence().manualCamera();
}

bool
sceneHudOverrideActive(void)
{
    return sceneHudMode() >= 0;
}

bool
sceneHudSideVisible(int screenSide)
{
    if(screenSide != SCENE_HUD_LEFT && screenSide != SCENE_HUD_RIGHT)
        return false;

    if(!sceneHudOverrideActive())
        return true;
    if(sceneHudMode() != 1 || !sceneHudVisible())
        return false;

    return screenSide == SCENE_HUD_LEFT && sceneHudPrideSide() == 1;
}

void
updateSceneManualCamera(float horizontalAxis, float verticalAxis,
                        float frameStep)
{
    const GameOptions& options = g_game.options();
    if(options.cameraXType != 0)
        horizontalAxis = -horizontalAxis;
    if(options.cameraYType != 0)
        verticalAxis = -verticalAxis;

    const SceneSequencer& sequence = sceneSequence();

    const bool enabled = !sequence.available() || sceneManualCamera();
    updateManualCameraControl(g_sceneManualCamera, enabled,
                              horizontalAxis, verticalAxis, frameStep);
}

PlayerFollowFraming
applySceneManualCamera(const PlayerFollowFraming& automaticFraming)
{
    return applyManualCameraControl(g_sceneManualCamera, automaticFraming);
}

void
drawSceneCharacterSelectOverlay(void)
{
    const bool captureRequested = takeSceneCharacterCaptureRequest();
    if(captureRequested)
        captureCharacterSelectFrame();

    const SceneCharacterSelectState previousState =
        sceneCharacterSelectState();
    if(previousState == SCENE_CHARACTER_SELECT_CLEAR && !captureRequested)
        releaseCharacterSelectCapture();

    if(!sceneCharacterSelectActive() ||
       sceneCharacterSelectAlternateGeometry() ||
       sceneCharacterSelectAlpha() <= 0)
        return;
    uint8_t red = 0, green = 0, blue = 0;
    if(sceneCharacterSelectOverlayColor(&red, &green, &blue))
    {
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const int alpha = sceneCharacterSelectAlpha() * 255 / 127;
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            origin, ImVec2(origin.x + size.x, origin.y + size.y),
            IM_COL32(red, green, blue, alpha));
        return;
    }
    if(g_characterSelectCapture == NULL && !captureCharacterSelectFrame())
        return;

    const int alpha =
        sceneCharacterSelectAlpha() * 0xbf / 0x7f;
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    ImGui::GetBackgroundDrawList()->AddImage(
        characterSelectCaptureRef(),
        origin, ImVec2(origin.x + size.x, origin.y + size.y),
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
        IM_COL32(0xff, 0xff, 0xff, alpha));
}

void
resetSceneView(void)
{
    releaseCharacterSelectCapture();
    resetManualCameraControl(g_sceneManualCamera);
}

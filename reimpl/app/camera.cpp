#include "app/internal.h"

namespace app_detail
{

float stageCameraFarPlane(uint16_t stageId)
{
    switch(stageId)
    {
    case 0x2f: return 2500.0f;
    case 0x2e:
    case 0x27:
    case 0x13:
    case 0x01: return 1800.0f;
    case 0x08: return 350.0f;
    default:   return 1500.0f;
    }
}

float stageFieldCameraDistance(uint16_t stageId)
{

    switch(stageId)
    {
    case 0x14:
    case 0x17:
    case 0x19:
    case 0x1f:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x2b:
    case 0x2d:
    case 0x32:
    case 0x33:
    case 0x35:
        return 40.0f;
    default:
        return 50.0f;
    }
}

void setCameraForStage(void)
{
    const float radius = g_scene.stage.radius > 1.0f ? g_scene.stage.radius : 1.0f;
    rw::V3d eye =
    {
        g_scene.stage.center.x + radius * 1.1f,
        g_scene.stage.center.y + radius * 0.65f,
        g_scene.stage.center.z - radius * 1.1f
    };
    rw::V3d at =
    {
        g_scene.stage.center.x - eye.x,
        g_scene.stage.center.y - eye.y,
        g_scene.stage.center.z - eye.z
    };
    const float atLength = sqrtf(at.x * at.x + at.y * at.y + at.z * at.z);
    at.x /= atLength;
    at.y /= atLength;
    at.z /= atLength;

    rw::V3d right = { at.z, 0.0f, -at.x };
    const float rightLength = sqrtf(right.x * right.x + right.z * right.z);
    right.x /= rightLength;
    right.z /= rightLength;
    rw::V3d up =
    {
        at.y * right.z - at.z * right.y,
        at.z * right.x - at.x * right.z,
        at.x * right.y - at.y * right.x
    };

    rw::Frame* frame = g_scene.camera->getFrame();
    frame->matrix.right = right;
    frame->matrix.up = up;
    frame->matrix.at = at;
    frame->matrix.pos = eye;
    frame->matrix.optimize();
    frame->updateObjects();
    g_scene.camera->setNearPlane(radius * 0.001f);
    g_scene.camera->setFarPlane(radius * 4.0f);
    g_scene.camera->setFOV(60.0f,
        static_cast<float>(sk::globals.width) / static_cast<float>(sk::globals.height));
}

void setCameraLookAt(const rw::V3d& eye, const rw::V3d& target)
{
    rw::V3d at =
    {
        target.x - eye.x,
        target.y - eye.y,
        target.z - eye.z
    };
    const float atLength = sqrtf(at.x * at.x + at.y * at.y + at.z * at.z);
    if(atLength == 0.0f)
        return;
    at.x /= atLength;
    at.y /= atLength;
    at.z /= atLength;

    rw::V3d right = { at.z, 0.0f, -at.x };
    const float rightLength = sqrtf(right.x * right.x + right.z * right.z);
    if(rightLength == 0.0f)
        return;
    right.x /= rightLength;
    right.z /= rightLength;
    rw::V3d up =
    {
        at.y * right.z - at.z * right.y,
        at.z * right.x - at.x * right.z,
        at.x * right.y - at.y * right.x
    };

    rw::Frame* frame = g_scene.camera->getFrame();
    frame->matrix.right = right;
    frame->matrix.up = up;
    frame->matrix.at = at;
    frame->matrix.pos = eye;
    frame->matrix.optimize();
    frame->updateObjects();
}

float clampPresentation(float value, float minimum, float maximum)
{
    if(value < minimum)
        return minimum;
    if(value > maximum)
        return maximum;
    return value;
}

float wrapPresentationDegrees(float degrees)
{
    while(degrees > 180.0f)
        degrees -= 360.0f;
    while(degrees <= -180.0f)
        degrees += 360.0f;
    return degrees;
}

float advancePresentationAngle(float current, float target, float maximumStep)
{
    const float delta = wrapPresentationDegrees(target - current);
    return wrapPresentationDegrees(
        current + clampPresentation(delta, -maximumStep, maximumStep));
}

rw::V3d presentationVector(float x, float y, float z)
{
    rw::V3d value = { x, y, z };
    return value;
}

rw::V3d mixPresentationVector(const rw::V3d& from, const rw::V3d& to,
                              float amount)
{
    return presentationVector(
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
        from.z + (to.z - from.z) * amount);
}

rw::V3d smoothPresentationVector(const rw::V3d& from, const rw::V3d& to,
                                 float amount, float deadband)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dz = to.z - from.z;
    if(dx * dx + dy * dy + dz * dz <= deadband * deadband)
        return from;
    return mixPresentationVector(from, to, amount);
}

uint32_t cameraTransitionSignature(const StoryCameraTransition& transition)
{
    uint32_t signature = 2166136261u;
    for(int i = 0; i < 6; i++)
    {
        uint32_t bits;
        const float value = i < 3 ? transition.start[i] :
                                    transition.end[i - 3];
        memcpy(&bits, &value, sizeof(bits));
        signature = (signature ^ bits) * 16777619u;
    }
    signature = (signature ^
                 static_cast<uint32_t>(transition.durationFrames)) * 16777619u;
    signature = (signature ^ transition.interpolate) * 16777619u;
    return (signature ^ transition.active) * 16777619u;
}

rw::V3d sampleCameraTransition(const StoryCameraTransition& transition,
                               float elapsedFrames)
{
    if(!transition.interpolate || transition.durationFrames <= 0)
    {
        return presentationVector(transition.end[0], transition.end[1],
                                  transition.end[2]);
    }
    const float amount = clampPresentation(
        elapsedFrames / static_cast<float>(transition.durationFrames),
        0.0f, 1.0f);
    return presentationVector(
        transition.start[0] +
            (transition.end[0] - transition.start[0]) * amount,
        transition.start[1] +
            (transition.end[1] - transition.start[1]) * amount,
        transition.start[2] +
            (transition.end[2] - transition.start[2]) * amount);
}

CameraPose previewCameraPose(void)
{
    const float radius = clampPresentation(g_scene.stage.radius, 12.0f, 95.0f);
    CameraPose pose;
    pose.target = presentationVector(g_scene.stage.center.x,
                                     g_scene.stage.center.y + radius * 0.06f,
                                     g_scene.stage.center.z);
    pose.eye = presentationVector(g_scene.stage.center.x + radius * 0.95f,
                                  g_scene.stage.center.y + radius * 0.55f,
                                  g_scene.stage.center.z - radius * 0.95f);
    pose.fieldOfView = 48.0f;
    pose.nearPlane = STAGE_CAMERA_NEAR_PLANE;
    pose.farPlane = stageCameraFarPlane(g_scene.stageOriginalId);
    return pose;
}

uint8_t fieldCameraGroundSurface(uint16_t stageId, float playerZ)
{

    switch(stageId)
    {
    case 0x0d:
    case 0x1d:
        return 4;
    case 0x2f:
        return playerZ > 100.0f ? 5 : 4;
    default:
        return 0;
    }
}

CameraPose combatCameraPose(float deltaSeconds)
{
    const GameSlotRecord& player = g_game.runtimeState().player;
    CameraPose pose;

    if(updateCamdatCamera(deltaSeconds, pose))
        return pose;

    const GameSlotRecord& target = g_game.opponent();
    const float deltaX = target.position.x - player.position.x;
    const float deltaZ = target.position.z - player.position.z;
    const float combatantDistance = sqrtf(deltaX * deltaX + deltaZ * deltaZ);

    const bool fieldEncounter = g_game.mode() == GAME_MODE_STORY;
    if(fieldEncounter)
        g_cameraPresentation.followPlayer = true;
    else if(!g_cameraPresentation.initialized)
        g_cameraPresentation.followPlayer =
            combatantDistance > COMBAT_CAMERA_FOLLOW_DISTANCE;
    else if(g_cameraPresentation.followPlayer)
        g_cameraPresentation.followPlayer =
            combatantDistance >= COMBAT_CAMERA_ORBIT_DISTANCE;
    else
        g_cameraPresentation.followPlayer =
            combatantDistance > COMBAT_CAMERA_FOLLOW_DISTANCE;

    if(g_cameraPresentation.followPlayer)
    {
        g_cameraPresentation.battleSubjectsInitialized = false;
        g_cameraPresentation.battleFramingAngleDegrees = 0.0f;

        EffectMatrix targetBone;
        Vec3 targetPosition = player.position;
        if(combatBoneMatrix(player.slotIndex, 10, &targetBone))
        {
            targetPosition.x = targetBone.pos.x;
            targetPosition.y = targetBone.pos.y;
            targetPosition.z = targetBone.pos.z;
        }
        const float desiredAzimuth =
            wrapPresentationDegrees(90.0f - player.headingDegrees);
        if(!g_cameraPresentation.fieldAzimuthInitialized)
        {
            g_cameraPresentation.fieldAzimuthDegrees = desiredAzimuth;
            g_cameraPresentation.fieldAzimuthInitialized = true;
        }
        else
        {

            const float frameStep = clampPresentation(
                deltaSeconds * g_game.nominalFrameRateHz(), 0.0f, 2.0f);
            g_cameraPresentation.fieldAzimuthDegrees =
                advancePresentationAngle(
                    g_cameraPresentation.fieldAzimuthDegrees, desiredAzimuth,
                    5.0f * frameStep);
        }
        const float cameraHeading =
            90.0f - g_cameraPresentation.fieldAzimuthDegrees;
        PlayerFollowFraming framing = computePlayerFollowFraming(
            player.position.x, player.position.y, player.position.z,
            cameraHeading,
            stageFieldCameraDistance(g_scene.stageOriginalId),
            targetPosition.x, targetPosition.y, targetPosition.z);
        FieldCameraRegionInputs regionInputs = {};
        regionInputs.stageId = g_scene.stageOriginalId;
        regionInputs.player.x = player.position.x;
        regionInputs.player.y = player.position.y;
        regionInputs.player.z = player.position.z;
        regionInputs.currentEye.x = g_cameraPresentation.initialized ?
            g_cameraPresentation.current.eye.x : framing.eyeX;
        regionInputs.currentEye.y = g_cameraPresentation.initialized ?
            g_cameraPresentation.current.eye.y : framing.eyeY;
        regionInputs.currentEye.z = g_cameraPresentation.initialized ?
            g_cameraPresentation.current.eye.z : framing.eyeZ;
        regionInputs.groundSurface = fieldCameraGroundSurface(
            regionInputs.stageId, regionInputs.player.z);
        regionInputs.storyPhase = static_cast<int>(
            g_game.runtimeState().story.progression.phase);
        regionInputs.eventFlag031e = g_game.eventFlag(0x031e);
        regionInputs.sceneControlFlags =
            g_game.sceneSequence().controlFlagBits();
        const bool fixedRegion = resolveFieldCameraRegion(
            regionInputs, g_fieldCameraRegion);
        if(fixedRegion)
        {
            const FieldCameraRegionRecord& record =
                g_fieldCameraRegion.table[
                    g_fieldCameraRegion.recordIndex];
            framing.eyeX = record.eyeX;
            framing.eyeZ = record.eyeZ;
            if(!g_fieldCameraRegion.preserveEyeY)
                framing.eyeY = record.eyeY;
        }
        else
        {
            framing = applySceneManualCamera(framing);
        }
        pose.target = presentationVector(
            framing.targetX, framing.targetY, framing.targetZ);
        pose.eye = presentationVector(
            framing.eyeX, framing.eyeY, framing.eyeZ);
        pose.fieldOfView = COMBAT_CAMERA_FIELD_OF_VIEW;
    }
    else
    {
        g_fieldCameraRegion.active = false;
        g_fieldCameraRegion.recordIndex = -1;
        g_fieldCameraRegion.table = NULL;
        g_fieldCameraRegion.tableRecordCount = 0;

        Vec3 playerSubject = player.position;
        Vec3 targetSubject = target.position;
        Vec3 renderedSubject;
        if(characterSceneCameraSubject(&g_scene.player, renderedSubject))
            playerSubject = renderedSubject;
        if(characterSceneCameraSubject(
               combatCharacterScene(target), renderedSubject))
            targetSubject = renderedSubject;
        if(!g_cameraPresentation.battleSubjectsInitialized)
        {
            g_cameraPresentation.battleSubjects[0] =
                { playerSubject.x, playerSubject.y, playerSubject.z };
            g_cameraPresentation.battleSubjects[1] =
                { targetSubject.x, targetSubject.y, targetSubject.z };
            g_cameraPresentation.battleSubjectsInitialized = true;
        }
        else
        {
            updateBattleCameraSubject(
                g_cameraPresentation.battleSubjects[0],
                playerSubject.x, playerSubject.y, playerSubject.z);
            updateBattleCameraSubject(
                g_cameraPresentation.battleSubjects[1],
                targetSubject.x, targetSubject.y, targetSubject.z);
        }
        playerSubject =
        {
            g_cameraPresentation.battleSubjects[0].x,
            g_cameraPresentation.battleSubjects[0].y,
            g_cameraPresentation.battleSubjects[0].z
        };
        targetSubject =
        {
            g_cameraPresentation.battleSubjects[1].x,
            g_cameraPresentation.battleSubjects[1].y,
            g_cameraPresentation.battleSubjects[1].z
        };
        const float midpointX = (playerSubject.x + targetSubject.x) * 0.5f;
        const float midpointZ = (playerSubject.z + targetSubject.z) * 0.5f;
        const float previousEyeX = g_cameraPresentation.initialized ?
            g_cameraPresentation.current.eye.x : midpointX;
        const float previousEyeZ = g_cameraPresentation.initialized ?
            g_cameraPresentation.current.eye.z : midpointZ;
        const uint16_t stageId = g_scene.stageOriginalId;
        const float maxDistance =
            stageId == 0x13 || stageId == 0x33 || stageId == 0x36 ?
                50.0f : BATTLE_CAMERA_MAX_DISTANCE;
        const CombatFraming framing = computeBattleCameraFraming(
            playerSubject.x, playerSubject.y, playerSubject.z,
            targetSubject.x, targetSubject.y, targetSubject.z,
            previousEyeX, previousEyeZ, maxDistance);
        pose.target = presentationVector(
            framing.targetX, framing.targetY, framing.targetZ);
        pose.eye = presentationVector(
            framing.eyeX, framing.eyeY, framing.eyeZ);
        const float subjectDeltaX =
            playerSubject.x - targetSubject.x;
        const float subjectDeltaY =
            playerSubject.y - targetSubject.y;
        const float subjectDeltaZ =
            playerSubject.z - targetSubject.z;
        const float subjectSeparation = sqrtf(
            subjectDeltaX * subjectDeltaX +
            subjectDeltaY * subjectDeltaY +
            subjectDeltaZ * subjectDeltaZ);
        const float totalSpan =
            subjectSeparation +
            BATTLE_CAMERA_SUBJECT_CLEARANCE * 2.0f;
        const float baseAngleRadians =
            BATTLE_CAMERA_FRAMING_ANGLE_DEGREES *
            0.01745329251994329577f;
        const float unclampedDistance =
            totalSpan * 0.5f / sinf(baseAngleRadians);
        float desiredFramingAngle = 0.0f;
        if(unclampedDistance > maxDistance && totalSpan > 0.0f)
        {
            const float sine =
                clampPresentation(
                    maxDistance * sinf(baseAngleRadians) * 2.0f /
                        totalSpan,
                    0.0f, 1.0f);
            desiredFramingAngle =
                asinf(sine) * 57.29577951308232f;
        }
        const float angleDifference =
            desiredFramingAngle -
            g_cameraPresentation.battleFramingAngleDegrees;
        if(fabsf(angleDifference) <= 1.0f)
            g_cameraPresentation.battleFramingAngleDegrees =
                desiredFramingAngle;
        else
            g_cameraPresentation.battleFramingAngleDegrees +=
                angleDifference * 0.05f;
        pose.fieldOfView = COMBAT_CAMERA_FIELD_OF_VIEW +
            5.0f * clampPresentation(
                g_cameraPresentation.battleFramingAngleDegrees / 60.0f,
                0.0f, 1.0f);
        g_combatCameraActive = false;
    }
    pose.nearPlane = STAGE_CAMERA_NEAR_PLANE;
    pose.farPlane = stageCameraFarPlane(g_scene.stageOriginalId);
    return pose;
}

CameraPose desiredCameraPose(float deltaSeconds)
{
    CameraPose pose;
    if(titleBackdropScreen(g_game.screen()))
    {

        float ex, ey, ez, tx, ty, tz;
        if(!g_titleCameraPathStarted)
            cameraPathStart(g_titleCameraPath, TITLE_CAMERA_PATH);
        g_titleCameraPathStarted = true;

        cameraPathSample(g_titleCameraPath, 0.0f, &ex, &ey, &ez, &tx, &ty, &tz);
        pose.eye = presentationVector(ex, ey, ez);
        pose.target = presentationVector(tx, ty, tz);
        pose.fieldOfView = COMBAT_CAMERA_FIELD_OF_VIEW;
        pose.nearPlane = STAGE_CAMERA_NEAR_PLANE;
        pose.farPlane = stageCameraFarPlane(g_scene.stageOriginalId);
    }
    else
    {
        pose = g_game.screen() == GAME_SCREEN_GAMEPLAY ?
            combatCameraPose(deltaSeconds) : previewCameraPose();
        if(g_game.screen() != GAME_SCREEN_GAMEPLAY)
        {
            g_combatCameraActive = false;
            g_camdatRuntime.stop();
            g_game.setCamdatTimeScale(1.0f);
        }
    }
    const EventVm& eventVm = g_game.eventScript();
    const StoryState& story = g_game.runtimeState().story;
    const bool scripted = eventVm.running() &&
        (story.cameraPosition.active || story.cameraTarget.active);
    g_cameraPresentation.scripted = scripted;
    if(!scripted)
        return pose;

    const float frameDelta = deltaSeconds * 60.0f;
    if(story.cameraPosition.active)
    {
        const uint32_t signature =
            cameraTransitionSignature(story.cameraPosition);
        if(signature != g_cameraPresentation.positionSignature)
        {
            g_cameraPresentation.positionSignature = signature;
            g_cameraPresentation.positionTransitionFrames = 0.0f;
        }
        pose.eye = sampleCameraTransition(
            story.cameraPosition,
            g_cameraPresentation.positionTransitionFrames);
        g_cameraPresentation.positionTransitionFrames += frameDelta;
    }
    if(story.cameraTarget.active)
    {
        const uint32_t signature =
            cameraTransitionSignature(story.cameraTarget);
        if(signature != g_cameraPresentation.targetSignature)
        {
            g_cameraPresentation.targetSignature = signature;
            g_cameraPresentation.targetTransitionFrames = 0.0f;
        }
        pose.target = sampleCameraTransition(
            story.cameraTarget,
            g_cameraPresentation.targetTransitionFrames);
        g_cameraPresentation.targetTransitionFrames += frameDelta;
    }
    return pose;
}

void applyPresentationCamera(const CameraPose& pose)
{
    if(g_scene.camera == NULL)
        return;
    setCameraLookAt(pose.eye, pose.target);
    g_scene.camera->setNearPlane(pose.nearPlane);
    g_scene.camera->setFarPlane(pose.farPlane);
    const float aspect = sk::globals.height > 0 ?
        static_cast<float>(sk::globals.width) /
            static_cast<float>(sk::globals.height) : 1.0f;
    g_scene.camera->setFOV(pose.fieldOfView, aspect);
}

void applySceneCamera(void*, const SceneCameraState& state)
{
    CameraPose pose;
    pose.eye = presentationVector(state.eye.x, state.eye.y, state.eye.z);
    pose.target = presentationVector(
        state.target.x, state.target.y, state.target.z);
    pose.fieldOfView = state.fieldOfView > 0.0f ?
        state.fieldOfView : 60.0f;
    pose.nearPlane = state.nearClip > 0.0f ? state.nearClip : 0.05f;
    pose.farPlane = state.farClip > pose.nearPlane ?
        state.farClip : 1000.0f;
    applyPresentationCamera(pose);
    g_cameraPresentation.current = pose;
    g_cameraPresentation.initialized = true;
}

}

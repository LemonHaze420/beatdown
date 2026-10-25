#include "game/combat/animation.h"
#include "app/internal.h"

namespace app_detail
{

CharacterScene* combatCharacterSceneForSlot(uint8_t slotIndex)
{
    if(slotIndex == 0)
        return &g_scene.player;
    if(g_game.hasAlly() && slotIndex == g_game.allySlot().slotIndex)
        return &g_scene.ally;

    for(int i = 0; i < g_game.opponentCount(); ++i)
    {
        if(g_game.opponent(i).slotIndex != slotIndex)
            continue;
        return i == 0 ? &g_scene.opponent :
            &g_scene.additionalOpponents[i - 1];
    }
    return NULL;
}

CharacterScene* combatCharacterScene(const GameSlotRecord& slot)
{
    return combatCharacterSceneForSlot(slot.slotIndex);
}

bool combatWeaponFrames(void*, uint8_t slotIndex, rw::Frame** frames)
{
    CharacterScene* scene = combatCharacterSceneForSlot(slotIndex);
    frames[0] = characterSceneBoneFrame(scene, 14);
    frames[1] = characterSceneBoneFrame(scene, 19);
    return frames[0] != NULL || frames[1] != NULL;
}

bool characterSceneBoneMatrix(const CharacterScene* scene, uint8_t boneIndex,
                              rw::Matrix& matrix)
{
    if(scene == NULL || scene->hierarchies.empty())
        return false;
    rw::HAnimHierarchy* hierarchy = scene->hierarchies[0];
    if(hierarchy == NULL || hierarchy->matrices == NULL ||
       boneIndex >= hierarchy->numNodes)
    {
        return false;
    }

    const rw::Matrix& source = hierarchy->matrices[boneIndex];
    if((hierarchy->flags &
        rw::HAnimHierarchy::LOCALSPACEMATRICES) == 0)
    {
        matrix = source;
        return true;
    }

    for(size_t clumpIndex = 0;
        clumpIndex < scene->clumps.size(); ++clumpIndex)
    {
        rw::Clump* clump = scene->clumps[clumpIndex];
        FORLIST(atomicLink, clump->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(atomicLink);
            rw::Skin* skin = rw::Skin::get(atomic->geometry);
            if(skin == NULL || rw::Skin::getHierarchy(atomic) != hierarchy)
                continue;
            rw::Matrix::mult(
                &matrix, &source, atomic->getFrame()->getLTM());
            return true;
        }
    }
    return false;
}

bool combatBoneMatrix(uint8_t slotIndex, uint8_t boneIndex,
                      EffectMatrix* matrix)
{
    if(matrix == NULL)
        return false;
    const CharacterScene* scene = combatCharacterSceneForSlot(slotIndex);
    rw::Matrix source;
    if(!characterSceneBoneMatrix(scene, boneIndex, source))
        return false;
    matrix->right = { source.right.x, source.right.y, source.right.z };
    matrix->up = { source.up.x, source.up.y, source.up.z };
    matrix->at = { source.at.x, source.at.y, source.at.z };
    matrix->pos = { source.pos.x, source.pos.y, source.pos.z };
    return effectMatrixIsFinite(*matrix);
}

const GameSlotRecord* combatSlotForIndex(uint8_t slotIndex)
{
    const GameSlotRecord& player = g_game.runtimeState().player;
    if(player.slotIndex == slotIndex)
        return &player;
    if(g_game.hasAlly() && g_game.allySlot().slotIndex == slotIndex)
        return &g_game.allySlot();
    for(int i = 0; i < g_game.opponentCount(); ++i)
    {
        if(g_game.opponent(i).slotIndex == slotIndex)
            return &g_game.opponent(i);
    }
    return NULL;
}

bool provideCamdatBasis(void* userData, bool linkedActor,
                        uint16_t boneIndex, CamdatFramingBasis* basis)
{
    if(userData == NULL || basis == NULL || boneIndex > 0xff)
        return false;
    const uint8_t actorSlot = *static_cast<const uint8_t*>(userData);
    const GameSlotRecord* actor = combatSlotForIndex(actorSlot);
    if(actor == NULL)
        return false;
    if(linkedActor)
        actor = &g_game.boundTargetOf(*actor);

    EffectMatrix matrix;
    if(!combatBoneMatrix(actor->slotIndex,
                         static_cast<uint8_t>(boneIndex), &matrix))
    {
        return false;
    }
    basis->right = { matrix.right.x, matrix.right.y, matrix.right.z };
    basis->up = { matrix.up.x, matrix.up.y, matrix.up.z };
    basis->forward = { matrix.at.x, matrix.at.y, matrix.at.z };
    basis->target = { matrix.pos.x, matrix.pos.y, matrix.pos.z };
    return true;
}

bool updateCamdatCamera(float deltaSeconds, CameraPose& pose)
{
    int recordIndex;
    bool exclusive;
    uint8_t actorSlot;
    if(g_game.consumeCamdatCameraRequest(
           &recordIndex, &exclusive, &actorSlot))
    {
        CamdatFraming initial;
        if(g_cameraPresentation.initialized)
        {
            initial.eye = {
                g_cameraPresentation.current.eye.x,
                g_cameraPresentation.current.eye.y,
                g_cameraPresentation.current.eye.z
            };
            initial.target = {
                g_cameraPresentation.current.target.x,
                g_cameraPresentation.current.target.y,
                g_cameraPresentation.current.target.z
            };
        }
        else
        {
            initial.eye = { 0.0f, 0.0f, 0.0f };
            initial.target = { 0.0f, 0.0f, 1.0f };
        }
        g_camdatActorSlot = actorSlot;
        g_camdatRuntime.start(&g_game.camdat(), recordIndex, initial);
        g_game.setCamdatTimeScale(g_camdatRuntime.timeScale());
        if(exclusive)
            g_combatCameraActive = false;
    }
    if(!g_camdatRuntime.active())
    {
        g_game.setCamdatTimeScale(1.0f);
        return false;
    }

    CamdatFraming framing;
    if(!g_camdatRuntime.update(
           g_game.scaleFrameDelta(deltaSeconds),
           provideCamdatBasis, &g_camdatActorSlot, &framing))
    {
        g_game.setCamdatTimeScale(g_camdatRuntime.timeScale());
        return false;
    }
    g_game.setCamdatTimeScale(g_camdatRuntime.timeScale());
    bool actorActionFlag;
    if(g_camdatRuntime.consumeActorActionFlag(&actorActionFlag))
    {
        g_game.applyCamdatActorActionFlag(
            g_camdatActorSlot, actorActionFlag);
    }
    pose.eye = presentationVector(
        framing.eye.x, framing.eye.y, framing.eye.z);
    pose.target = presentationVector(
        framing.target.x, framing.target.y, framing.target.z);
    pose.fieldOfView = g_camdatRuntime.fieldOfView();
    pose.nearPlane = STAGE_CAMERA_NEAR_PLANE;
    pose.farPlane = stageCameraFarPlane(g_scene.stageOriginalId);
    return true;
}

bool combatAnimationFrame(uint8_t slotIndex, float* frame)
{
    if(frame == NULL)
        return false;
    const CharacterScene* scene = combatCharacterSceneForSlot(slotIndex);
    if(scene == NULL || scene->hierarchies.empty())
        return false;
    const rw::HAnimHierarchy* hierarchy = scene->hierarchies[0];
    if(hierarchy == NULL || hierarchy->interpolator == NULL ||
       hierarchy->interpolator->currentAnim == NULL)
        return false;
    *frame = hierarchy->interpolator->currentTime * 60.0f;
    return true;
}

bool loadBattleAnimationResource(void*, int resourceId)
{
    if(resourceId < 0 || resourceId >= MOVE_ANIMATION_BUNDLE_COUNT)
        return false;
    const MoveAnimationBundle& bundle = MOVE_ANIMATION_BUNDLES[resourceId];
    if(bundle.xafEntry < 0)
        return true;
    for(int slotIndex = 0; slotIndex < GAME_COMBAT_SLOT_COUNT; ++slotIndex)
    {
        const GameSlotRecord* slot = nullptr;
        if(g_game.runtimeState().player.slotIndex == slotIndex)
            slot = &g_game.runtimeState().player;
        else if(g_game.hasAlly() && g_game.allySlot().slotIndex == slotIndex)
            slot = &g_game.allySlot();
        else
            for(int index = 0; index < g_game.opponentCount(); ++index)
                if(g_game.opponent(index).slotIndex == slotIndex)
                    slot = &g_game.opponent(index);
        if(slot == nullptr || (slot->flags & 1u) == 0)
            continue;
        CharacterScene* scene = combatCharacterScene(*slot);
        if(scene == nullptr || scene->clumps.empty())
            return false;
        if(!loadCharacterAnimationResourceById(g_stagePath, resourceId, scene))
            return false;
    }
    return true;
}

void publishCombatantPose(const GameSlotRecord& slot)
{
    CharacterScene* const scene = combatCharacterScene(slot);
    g_game.setCombatModelState(slot.slotIndex,
        scene != nullptr && !scene->clumps.empty(), slot.actorLoadStateFlags);
    if(scene == NULL || scene->hierarchies.empty())
        return;
    const rw::HAnimHierarchy* const hierarchy = scene->hierarchies[0];
    if(hierarchy == NULL || hierarchy->interpolator == NULL ||
       hierarchy->interpolator->currentAnim == NULL)
        return;

    MoveMotion scheduled = {};
    const MoveMotion* motion = g_game.combatMotion(slot.slotIndex, &scheduled) ?
        &scheduled : nullptr;
    const CombatAnimationRequest request = combatAnimationRequest(slot, motion);
    if(request.scheduled && scene->animationState != request.state)
        return;
    const float currentTime = hierarchy->interpolator->currentTime;
    const bool complete =
        currentTime >= scene->animationEndTime - 0.00001f;
    CombatBoneTransform bones[COMBAT_BONE_COUNT];
    int boneCount = 0;
    if(hierarchy->matrices != NULL)
    {
        boneCount = hierarchy->numNodes < COMBAT_BONE_COUNT ?
            hierarchy->numNodes : COMBAT_BONE_COUNT;
        for(int i = 0; i < boneCount; ++i)
        {
            rw::Matrix source;
            if(!characterSceneBoneMatrix(scene, static_cast<uint8_t>(i), source))
            {
                boneCount = 0;
                break;
            }
            bones[i].right = { source.right.x, source.right.y,
                               source.right.z };
            bones[i].up = { source.up.x, source.up.y, source.up.z };
            bones[i].at = { source.at.x, source.at.y, source.at.z };
            bones[i].position = { source.pos.x, source.pos.y, source.pos.z };
        }
    }
    g_game.setCombatPose(slot.slotIndex, currentTime * 60.0f, complete,
                         bones, boneCount);
}

void publishCombatPoses(void)
{
    if(g_game.screen() != GAME_SCREEN_GAMEPLAY)
        return;
    publishCombatantPose(g_game.runtimeState().player);
    if(g_game.hasAlly())
        publishCombatantPose(g_game.allySlot());
    for(int i = 0; i < g_game.opponentCount(); ++i)
        publishCombatantPose(g_game.opponent(i));
}

bool directionEffectMatrix(const Vec3& direction,
                           const EffectVector3& position,
                           EffectMatrix* matrix)
{
    if(matrix == NULL)
        return false;
    const float atLengthSquared =
        direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z;
    if(atLengthSquared <= 0.000001f)
        return false;
    const float inverseAt = 1.0f / sqrtf(atLengthSquared);
    const EffectVector3 at = {
        direction.x * inverseAt,
        direction.y * inverseAt,
        direction.z * inverseAt
    };
    const float rightLengthSquared = at.x * at.x + at.z * at.z;
    if(rightLengthSquared <= 0.000001f)
        return false;
    const float inverseRight = 1.0f / sqrtf(rightLengthSquared);
    const EffectVector3 right = {
        at.z * inverseRight, 0.0f, -at.x * inverseRight
    };
    const EffectVector3 up = {
        at.y * right.z,
        at.z * right.x - at.x * right.z,
        -at.y * right.x
    };
    matrix->right = right;
    matrix->up = up;
    matrix->at = at;
    matrix->pos = position;
    return true;
}

bool provideLiveBoneEffectMatrix(void* userData, EffectMatrix* matrix)
{
    const LiveBoneEffectSource* source =
        static_cast<const LiveBoneEffectSource*>(userData);
    return source != NULL &&
           combatBoneMatrix(source->actorSlot, source->boneIndex, matrix);
}

LiveBoneEffectSource* acquireLiveBoneEffectSource()
{
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
    {
        EffectEmitterState state;
        if(g_liveBoneEffectSources[i].emitterHandle == 0 ||
           !g_effects.getEmitter(
               g_liveBoneEffectSources[i].emitterHandle, &state))
            return &g_liveBoneEffectSources[i];
    }
    return NULL;
}

bool loadedEstRecord(uint8_t recordIndex)
{
    const EffectAsset& asset = g_effects.asset();
    return recordIndex != EFFECT_RECORD_SENTINEL &&
           static_cast<size_t>(recordIndex) < asset.records.size() &&
           asset.records[recordIndex].active;
}

void dispatchPendingEstEvents()
{
    GameEstEvent event;
    while(g_game.pollEstEvent(&event))
    {
        if(!loadedEstRecord(event.recordIndex))
            continue;
        if(event.placement == GAME_EST_ACTOR_POSITION)
        {
            const EffectVector3 position =
                { event.position.x, event.position.y, event.position.z };
            const EffectMatrix world = effectYRotationMatrix(0.0f, position);
            g_effects.dispatchEst(event.recordIndex, world,
                                  event.tuningA, event.tuningB);
            continue;
        }
        EffectMatrix bone;
        if(!combatBoneMatrix(event.actorSlot, event.boneIndex, &bone))
            continue;
        if(event.periodic)
        {
            LiveBoneEffectSource* source = acquireLiveBoneEffectSource();
            if(source == NULL)
                continue;
            source->actorSlot = event.actorSlot;
            source->boneIndex = event.boneIndex;
            source->emitterHandle = g_effects.startEmitter(
                event.recordIndex, event.intervalSeconds,
                event.emitterLifetimeSeconds, event.emitterMode,
                provideLiveBoneEffectMatrix, source);
            if(source->emitterHandle == EFFECT_INVALID_HANDLE)
                source->emitterHandle = 0;
            continue;
        }
        EffectMatrix world;
        if(event.placement == GAME_EST_DIRECTION_BASIS)
        {
            if(!directionEffectMatrix(event.direction, bone.pos, &world))
                continue;
        }
        else
        {
            float angle = static_cast<float>(event.angleBase);
            if(event.addAnimationFrame)
            {
                float animationFrame;
                if(!combatAnimationFrame(event.actorSlot, &animationFrame))
                    continue;
                angle += static_cast<float>(static_cast<int>(animationFrame));
            }
            world = effectYRotationMatrix(angle, bone.pos);
        }
        g_effects.dispatchEst(event.recordIndex, world,
                              event.tuningA, event.tuningB);
    }
}

bool characterSceneCameraSubject(const CharacterScene* scene, Vec3& position)
{
    rw::Matrix subject;
    if(!characterSceneBoneMatrix(scene, 1, subject))
        return false;

    position.x = subject.pos.x;
    position.y = subject.pos.y;
    position.z = subject.pos.z;
    return true;
}

int gatherPresentationIndices(int* indices)
{
    int count = 0;
    indices[count++] = 0;
    for(int i = 0; i < g_game.opponentCount(); ++i)
        indices[count++] = i + 1;
    if(g_game.hasAlly())
        indices[count++] = ALLY_PRESENTATION_INDEX;
    return count;
}

const GameSlotRecord& presentationSlot(int index)
{
    if(index == 0)
        return g_game.runtimeState().player;
    if(index == ALLY_PRESENTATION_INDEX)
        return g_game.allySlot();
    return g_game.opponent(index - 1);
}

void updateCombatPresentation(float deltaSeconds)
{
    const GameScreen screen = g_game.screen();
    if(screen != GAME_SCREEN_GAMEPLAY)
    {
        if(screen != GAME_SCREEN_PAUSE &&
           screen != GAME_SCREEN_INVENTORY &&
           screen != GAME_SCREEN_OPTIONS &&
           screen != GAME_SCREEN_RESULTS)
        {
            g_combatPresentation.valid = false;
            g_effects.stopAll();
        }
        g_combatPresentation.previousScreen = screen;
        return;
    }

    int presentationIndices[MAX_GAME_OPPONENTS + 2];
    const int slotCount = gatherPresentationIndices(presentationIndices);
    int targetIndex = 0;
    const GameSlotRecord* focused = &g_game.opponent();
    for(int i = 1; i <= g_game.opponentCount(); i++)
    {
        if(&g_game.opponent(i - 1) == focused)
        {
            targetIndex = i;
            break;
        }
    }

    if(!g_combatPresentation.valid)
    {
        for(int n = 0; n < slotCount; n++)
        {
            const int i = presentationIndices[n];
            const GameSlotRecord& slot = presentationSlot(i);
            g_combatPresentation.slots[i].health = slot.currentHealth;
            g_combatPresentation.slots[i].action = slot.action;
            g_combatPresentation.slots[i].reaction = slot.reaction;
            g_combatPresentation.slots[i].comboStep = slot.comboStep;
            g_combatPresentation.damageFlash[i] = 0.0f;
            g_combatPresentation.displayedHealth[i] =
                static_cast<float>(slot.currentHealth);
        }
        g_combatPresentation.targetSlot =
            static_cast<uint8_t>(targetIndex);
        g_combatPresentation.valid = true;
    }

    if(targetIndex != g_combatPresentation.targetSlot)
        g_combatPresentation.targetSlot = static_cast<uint8_t>(targetIndex);

    for(int n = 0; n < slotCount; n++)
    {
        const int i = presentationIndices[n];
        const GameSlotRecord& slot = presentationSlot(i);
        SlotPresentationSnapshot& previous =
            g_combatPresentation.slots[i];
        if(slot.currentHealth < previous.health)
        {
            const int damage = previous.health - slot.currentHealth;
            const bool knockedOut =
                slot.action == COMBAT_ACTION_DEFEATED ||
                slot.reaction == COMBAT_REACTION_DEFEAT;
            g_combatPresentation.damageFlash[i] = 1.0f;
            g_cameraPresentation.shakeSeconds =
                knockedOut ? 0.32f : 0.18f;
            g_cameraPresentation.shakeMagnitude =
                clampPresentation(0.14f + damage * 0.018f, 0.14f, 1.2f);
        }

        if(g_combatPresentation.damageFlash[i] > 0.0f)
        {
            g_combatPresentation.damageFlash[i] -= deltaSeconds * 4.5f;
            if(g_combatPresentation.damageFlash[i] < 0.0f)
                g_combatPresentation.damageFlash[i] = 0.0f;
        }
        const float healthResponse =
            1.0f - expf(-3.2f * clampPresentation(
                deltaSeconds, 0.0f, 0.1f));
        g_combatPresentation.displayedHealth[i] +=
            (slot.currentHealth -
             g_combatPresentation.displayedHealth[i]) * healthResponse;

        previous.health = slot.currentHealth;
        previous.action = slot.action;
        previous.reaction = slot.reaction;
        previous.comboStep = slot.comboStep;
    }
    g_combatPresentation.previousScreen = screen;
}

}

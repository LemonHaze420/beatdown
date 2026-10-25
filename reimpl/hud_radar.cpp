#include "hud_radar.h"

#include "../formats/BATTLE_HUD/battle_hud_data.h"

#include <math.h>

namespace
{

const float HUD_WIDTH = 640.0f;
const float HUD_HEIGHT = 480.0f;
const float RADAR_CENTER_X = 90.0f;
const float RADAR_CENTER_Y = 402.0f;
const float RADAR_MAP_SCALE = 1.17f;
const float RADAR_DESTINATION_RADIUS = 48.0f;
const float PI = 3.14159265358979323846f;

const BattleHudDataPoint RADAR_TARGET_STAGE_POINTS[
    BATTLE_HUD_RADAR_STAGE_COUNT] =
{
    { 299.0f, 303.0f }, { 299.0f, 303.0f }, { 299.0f, 303.0f },
    { 299.0f, 303.0f }, { 424.0f, 334.0f }, { 294.0f, 296.0f },
    { 326.0f, 187.0f }, { 226.0f, 302.0f }, { 137.0f, 302.0f },
    { 282.0f, 309.0f }, { 124.0f, 320.0f }, { 226.0f, 302.0f },
    { 123.0f, 257.0f }, { 126.0f, 310.0f }, {  97.0f, 263.0f },
    { 146.0f, 263.0f }, { 115.0f, 263.0f }, { 115.0f, 263.0f },
    { 311.0f, 198.0f }, { 311.0f, 198.0f }, { 352.0f, 194.0f },
    { 358.0f, 200.0f }, { 350.0f, 200.0f }, { 357.0f, 186.0f },
    { 306.0f, 191.0f }, { 308.0f, 330.0f }, { 281.0f, 197.0f },
    { 311.0f, 198.0f }, { 389.0f, 298.0f }, { 491.0f, 220.0f },
    { 500.0f, 257.0f }, { 487.0f, 254.0f }, { 487.0f, 254.0f },
    { 487.0f, 254.0f }, { 487.0f, 254.0f }, { 525.0f, 286.0f },
    { 389.0f, 298.0f }, { 493.0f, 296.0f }, { 414.0f, 162.0f },
    { 410.0f, 116.0f }, { 448.0f, 130.0f }, { 392.0f, 285.0f },
    { 470.0f, 108.0f }, { 420.0f, 120.0f }, { 420.0f, 108.0f },
    { 454.0f, 147.0f }, { 309.0f, 162.0f }, { 391.0f, 293.0f },
    { 386.0f, 268.0f }, { 392.0f, 285.0f }, { 392.0f, 285.0f },
    { 392.0f, 285.0f }, { 392.0f, 285.0f }, { 392.0f, 285.0f }
};

float normalizeDegrees(float degrees)
{
    while(degrees < 0.0f)
        degrees += 360.0f;
    while(degrees >= 360.0f)
        degrees -= 360.0f;
    return degrees;
}

void radarPlayerMapPosition(const RadarMapTransform& transform,
                            float playerX, float playerZ,
                            float* mapX, float* mapY)
{
    const float xTerm =
        playerX / transform.worldXDivisor +
        transform.pivotX / (transform.worldXDivisor * 4.0f);
    const float zTerm =
        playerZ / transform.worldZDivisor +
        transform.pivotY / (transform.worldZDivisor * 8.0f);

    if(transform.stageId == 0x2a || transform.stageId == 0x0c)
    {
        *mapX = -xTerm;
        *mapY = -zTerm;
    }
    else if(transform.stageId == 4 || transform.stageId == 1)
    {
        *mapX = -(
            playerZ / transform.worldXDivisor +
            transform.pivotX / (transform.worldXDivisor * 4.0f));
        *mapY =
            playerX / transform.worldZDivisor +
            transform.pivotY / (transform.worldZDivisor * 8.0f);
    }
    else
    {
        *mapX = xTerm;
        *mapY = zTerm;
    }
}

void radarWorldMapPosition(const RadarMapTransform& transform,
                           float worldX, float worldZ,
                           float* mapX, float* mapY)
{
    if(transform.stageId == 0x2a || transform.stageId == 0x0c)
    {
        *mapX = -worldX / transform.worldXDivisor;
        *mapY = -worldZ / transform.worldZDivisor;
    }
    else if(transform.stageId == 4 || transform.stageId == 1)
    {
        *mapX = -worldZ / transform.worldXDivisor;
        *mapY = worldX / transform.worldZDivisor;
    }
    else
    {
        *mapX = worldX / transform.worldXDivisor;
        *mapY = worldZ / transform.worldZDivisor;
    }
}

bool objectiveAuthoredPoint(int16_t objectiveId, float* x, float* y)
{
    if(objectiveId == 0x16a)
    {
        *x = 479.0f;
        *y = 263.0f;
        return true;
    }
    if(objectiveId == 0x4c)
    {
        *x = 513.0f;
        *y = 290.0f;
        return true;
    }
    if(objectiveId == 0x6e)
    {
        *x = 290.0f;
        *y = 171.0f;
        return true;
    }
    return false;
}

bool radarTargetOffset(const RadarMapTransform& transform,
                       uint16_t currentStageId, int16_t targetStageId,
                       int16_t objectiveId, float playerX, float playerZ,
                       bool liveActorAvailable, float actorX, float actorZ,
                       float* offsetX, float* offsetY)
{

    if(currentStageId >= BATTLE_HUD_RADAR_STAGE_COUNT)
        return false;
    if(liveActorAvailable &&
       targetStageId == static_cast<int16_t>(currentStageId))
    {
        float playerMapX;
        float playerMapY;
        float actorMapX;
        float actorMapY;
        radarWorldMapPosition(transform, playerX, playerZ,
                              &playerMapX, &playerMapY);
        radarWorldMapPosition(transform, actorX, actorZ,
                              &actorMapX, &actorMapY);
        *offsetX = actorMapX - playerMapX;
        *offsetY = actorMapY - playerMapY;
        return true;
    }

    float targetX;
    float targetY;
    if(!objectiveAuthoredPoint(objectiveId, &targetX, &targetY))
    {
        if(targetStageId < 0 ||
           targetStageId >= BATTLE_HUD_RADAR_STAGE_COUNT)
            return false;
        targetX = RADAR_TARGET_STAGE_POINTS[targetStageId].x;
        targetY = RADAR_TARGET_STAGE_POINTS[targetStageId].y;
    }
    const BattleHudDataPoint origin =
        BATTLE_HUD_RADAR_STAGE_ORIGINS[currentStageId];
    float playerMapX;
    float playerMapY;
    radarPlayerMapPosition(transform, playerX, playerZ,
                           &playerMapX, &playerMapY);
    *offsetX = (targetX - origin.x) + 11.0f - playerMapX;
    *offsetY = (targetY - origin.y) + 5.0f - playerMapY;
    return true;
}

}

float
battleHudLogicalToNativeY(float logicalY, bool ntsc)
{

    const float nativeLines = ntsc ? 224.0f : 256.0f;
    const float palCorrection = ntsc ? 0.0f : 6.0f;
    return ((logicalY - 16.0f) + palCorrection) *
           (nativeLines / HUD_HEIGHT);
}

float
battleHudLogicalToHostY(float logicalY, bool ntsc)
{
    const float nativeLines = ntsc ? 224.0f : 256.0f;
    return battleHudLogicalToNativeY(logicalY, ntsc) *
           (HUD_HEIGHT / nativeLines);
}

bool
battleHudRadarVisible(const BattleHudRadarState& state)
{
    return state.globalSuppress == 0 &&
           (state.interactionStateFlags & 0x8000u) == 0 &&
           (state.battleHudMask & 4) != 0;
}

bool
battleHudRadarDynamicCellsVisible(const BattleHudRadarState& state)
{
    return state.dynamicCellsEnabled != 0 &&
           state.dynamicCellStateReady != 0 &&
           (state.overview == 0 ||
            state.overviewDynamicCellsEnabled != 0);
}

void
battleHudAdvanceRadarAnimation(BattleHudRadarState& state)
{
    if(state.overview != 0)
    {
        state.animationActive = 0;
        state.animationScale = 1.0f;
        state.animationReverse = 0;
        return;
    }
    if(state.animationActive == 0)
        return;
    if(state.animationReverse == 0)
    {
        state.animationScale += 0.05f;
        if(state.animationScale >= 1.8f)
        {
            state.animationActive = 0;
            state.animationScale = 1.8f;
            state.animationReverse = 1;
        }
    }
    else
    {
        state.animationScale -= 0.05f;
        if(state.animationScale <= 1.0f)
        {
            state.animationActive = 0;
            state.animationScale = 1.0f;
            state.animationReverse = 0;
        }
    }
}

float
battleHudRadarCompassRotation(uint16_t stageId, float cameraYawDegrees)
{
    float rotation = cameraYawDegrees + 180.0f;
    if(stageId == 1 || stageId == 4)
        rotation -= 90.0f;
    else if(stageId == 0x0c)
        rotation += 180.0f;
    else if(stageId == 0x2a)
        rotation -= 180.0f;
    return normalizeDegrees(rotation);
}

float
battleHudRadarCursorRotation(uint16_t stageId,
                             float playerHeadingDegrees,
                             float cameraYawDegrees,
                             bool northUp)
{
    float heading = playerHeadingDegrees + 180.0f;
    if(stageId == 1 || stageId == 4)
        heading -= 90.0f;
    else if(stageId == 0x0c || stageId == 0x2a)
        heading -= 180.0f;

    heading = normalizeDegrees(360.0f - normalizeDegrees(heading));
    if(!northUp)
        heading += battleHudRadarCompassRotation(stageId, cameraYawDegrees);
    return normalizeDegrees(heading);
}

RadarMapTransform
battleHudRadarMapTransform(uint16_t stageId,
                           float playerX, float playerZ,
                           float cameraYawDegrees)
{
    RadarMapTransform transform = {};
    transform.stageId = stageId;
    transform.worldXDivisor = 16.0f;
    transform.worldZDivisor = 16.0f;
    transform.compassDegrees =
        battleHudRadarCompassRotation(stageId, cameraYawDegrees);
    if(stageId >= BATTLE_HUD_RADAR_STAGE_COUNT)
        return transform;

    const BattleHudDataPoint offset = BATTLE_HUD_RADAR_MAP_OFFSETS[stageId];
    transform.available = offset.x != 0.0f || offset.y != 0.0f;
    if(!transform.available)
    {
        transform.compassDegrees = 0.0f;
        return transform;
    }

    if(stageId == 1)
    {
        transform.pivotX = (offset.x - 12.0f) +
                           (80.0f - playerZ / 16.0f);
        transform.pivotY = (offset.y + 132.0f) -
                           (80.0f - playerX / 16.0f);
    }
    else if(stageId == 0x0c)
    {
        transform.worldXDivisor = 18.0f;
        transform.pivotX = (offset.x + 154.0f) -
                           (playerX / 18.0f + 80.0f);
        transform.pivotY = (offset.y + 86.0f) -
                           (playerZ / 16.0f + 80.0f);
    }
    else if(stageId == 0x13 || stageId == 0x14 ||
            stageId == 6 || stageId == 0x2e)
    {
        transform.worldZDivisor = 20.0f;
        transform.pivotX = (offset.x + 158.0f) -
                           (80.0f - playerX / 16.0f);
        transform.pivotY = (offset.y + 110.0f) -
                           (80.0f - playerZ / 20.0f);
    }
    else if(stageId == 0x2a)
    {
        transform.worldXDivisor = 20.0f;
        transform.pivotX = offset.x + (80.0f - playerX / 20.0f);
        transform.pivotY = (offset.y - 42.0f) +
                           (80.0f - playerZ / 16.0f);
    }
    else if(stageId == 4)
    {
        transform.pivotX = (offset.x + 2.0f) +
                           (80.0f - playerZ / 16.0f);
        transform.pivotY = (offset.y + 116.0f) -
                           (80.0f - playerX / 16.0f);
    }
    else
    {
        transform.pivotX = (offset.x + 158.0f) -
                           (80.0f - playerX / 16.0f);
        transform.pivotY = (offset.y + 110.0f) -
                           (80.0f - playerZ / 16.0f);
    }
    return transform;
}

RadarProjection
battleHudProjectRadarOffset(float offsetX, float offsetY,
                            float compassDegrees, float insideRadius,
                            bool clampOutside)
{
    RadarProjection projection = {};
    projection.distance = sqrtf(offsetX * offsetX + offsetY * offsetY);
    projection.inside = projection.distance < insideRadius;
    projection.visible = projection.inside || clampOutside;
    if(!projection.visible)
        return projection;

    float scale = RADAR_MAP_SCALE;
    if(!projection.inside && projection.distance > 0.0f)
        scale = 54.0f / projection.distance;
    const float radians = compassDegrees * PI / 180.0f;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    projection.x = RADAR_CENTER_X +
        (offsetX * cosine - offsetY * sine) * scale;
    projection.y = RADAR_CENTER_Y +
        (offsetX * sine + offsetY * cosine) * scale;
    projection.rotationDegrees = normalizeDegrees(
        atan2f(offsetX, offsetY) * 180.0f / PI +
        compassDegrees + 180.0f);
    return projection;
}

RadarProjection
battleHudProjectRadarDestination(const RadarMapTransform& transform,
                                 int destinationIndex,
                                 float playerX, float playerZ)
{
    RadarProjection projection = {};
    if(!transform.available || destinationIndex < 0 ||
       destinationIndex >= BATTLE_HUD_RADAR_DESTINATION_COUNT ||
       transform.stageId >= BATTLE_HUD_RADAR_STAGE_COUNT)
        return projection;

    float playerMapX = 0.0f;
    float playerMapY = 0.0f;
    radarPlayerMapPosition(transform, playerX, playerZ,
                           &playerMapX, &playerMapY);
    const BattleHudDataPoint origin =
        BATTLE_HUD_RADAR_STAGE_ORIGINS[transform.stageId];
    const BattleHudDataPoint destination =
        BATTLE_HUD_RADAR_DESTINATIONS[destinationIndex];
    const float markerX = destination.x - origin.x;
    const float markerY = destination.y - origin.y;
    return battleHudProjectRadarOffset(
        markerX - playerMapX, markerY - playerMapY,
        transform.compassDegrees, RADAR_DESTINATION_RADIUS, false);
}

RadarCellGeometry
battleHudRelationshipCell(const RadarMapTransform& transform,
                          uint16_t currentStageId,
                          int16_t targetStageId,
                          float playerX, float playerZ,
                          bool liveActorAvailable,
                          float actorX, float actorZ,
                          bool overview)
{
    RadarCellGeometry cell = {};
    cell.sourceWidth = 16.0f;
    cell.sourceHeight = 16.0f;
    cell.width = 16.0f;
    cell.height = 16.0f;
    cell.scale = 1.5f;
    cell.renderKey = overview ? 0x125598 : 0x83;
    if(targetStageId < 0 ||
       targetStageId >= BATTLE_HUD_RADAR_STAGE_COUNT)
        return cell;
    if(overview)
    {
        const BattleHudDataPoint point =
            RADAR_TARGET_STAGE_POINTS[targetStageId];
        cell.visible = true;
        cell.inside = true;
        cell.x = point.x * 1.17f - 65.27997f + 9.36f;
        cell.y = point.y * 1.17f - 43.51999f + 2.039999f;
        return cell;
    }

    float offsetX;
    float offsetY;
    if(!radarTargetOffset(transform, currentStageId, targetStageId, -1,
                          playerX, playerZ, liveActorAvailable,
                          actorX, actorZ, &offsetX, &offsetY))
        return cell;
    const RadarProjection projection = battleHudProjectRadarOffset(
        offsetX, offsetY, transform.compassDegrees, 50.0f, true);
    cell.visible = projection.visible;
    cell.inside = projection.inside;
    cell.x = projection.x;
    cell.y = projection.y;
    cell.pivotX = 8.0f;
    cell.pivotY = 8.0f;
    if(!cell.inside)
    {

        cell.x -= 2.0f;
        cell.y -= 1.0f;
    }
    return cell;
}

RadarCellGeometry
battleHudObjectiveCell(const RadarMapTransform& transform,
                       uint16_t currentStageId,
                       int16_t objectiveId,
                       int16_t objectiveStageId,
                       bool alternate,
                       float playerX, float playerZ,
                       bool liveActorAvailable,
                       float actorX, float actorZ,
                       bool overview)
{
    RadarCellGeometry cell = {};
    cell.renderKey = overview ? 0x12558e : 0x82;
    if(objectiveId <= 0 || (alternate && objectiveStageId == 100))
        return cell;
    if(overview)
    {
        cell.visible = true;
        cell.inside = true;
        if(objectiveId == 0x16a)
        {
            cell.x = 495.15002f;
            cell.y = 264.19f;
        }
        else if(objectiveId == 0x4c)
        {
            cell.x = 534.93f;
            cell.y = 295.78f;
        }
        else if(objectiveId == 0x6e)
        {
            cell.x = 274.02002f;
            cell.y = 156.55f;
        }
        else
        {
            if(objectiveStageId < 0 ||
               objectiveStageId >= BATTLE_HUD_RADAR_STAGE_COUNT)
            {
                cell.visible = false;
                return cell;
            }
            const BattleHudDataPoint point =
                RADAR_TARGET_STAGE_POINTS[objectiveStageId];
            cell.x = point.x * 1.17f - 65.27997f + 9.36f;
            cell.y = point.y * 1.17f - 43.51999f + 2.039999f;
        }
        cell.sourceX = 48.0f;
        cell.sourceY = 16.0f;
        cell.sourceWidth = 16.0f;
        cell.sourceHeight = 24.0f;
        cell.width = 16.0f;
        cell.height = 24.0f;
        cell.scale = 1.5f;
        return cell;
    }

    float offsetX;
    float offsetY;
    if(!radarTargetOffset(transform, currentStageId, objectiveStageId,
                          objectiveId, playerX, playerZ,
                          liveActorAvailable, actorX, actorZ,
                          &offsetX, &offsetY))
        return cell;
    const RadarProjection projection = battleHudProjectRadarOffset(
        offsetX, offsetY, transform.compassDegrees, 50.0f, true);

    cell.visible = projection.visible;
    cell.inside = projection.inside;
    cell.x = projection.x;
    cell.y = projection.y;
    if(cell.inside)
    {
        cell.sourceX = 48.0f;
        cell.sourceY = 16.0f;
        cell.sourceWidth = 16.0f;
        cell.sourceHeight = 24.0f;
        cell.width = 16.0f;
        cell.height = 24.0f;
        cell.pivotX = 8.0f;
        cell.pivotY = 12.0f;
        cell.scale = 1.5f;
    }
    else
    {
        cell.sourceY = 16.0f;
        cell.sourceWidth = 24.0f;
        cell.sourceHeight = 24.0f;
        cell.width = 24.0f;
        cell.height = 24.0f;
        cell.pivotX = 12.0f;
        cell.pivotY = 12.0f;
        cell.scale = 1.0f;
        cell.x += 1.0f;
        cell.y += 1.0f;
        cell.rotationDegrees = projection.rotationDegrees;
    }
    return cell;
}

#ifndef BATTLE_HUD_TEST

#include "game.h"

#include "../formats/XAF/xaf_runtime.h"

#include <rw.h>

#include <string.h>
#include <vector>

namespace
{

const char* GRAPH_ARCHIVE_PATH = "raw/XAF/GRAPH.XAF";

struct TextureDescriptor
{
    uint32_t index;
    const char* name;
    int width;
    int height;
};

struct TextureTableState
{
    rw::Texture* textures[BATTLE_HUD_TEXTURE_COUNT];
    uint8_t language;
    bool attempted;
    bool ntsc;
};

TextureTableState g_textureTable = {};

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

rw::Raster* readImageRaster(const uint8_t* chunk, uint32_t length)
{
    if(length < 28 || read32(chunk) != rw::ID_STRUCT ||
       read32(chunk + 4) != 16)
        return NULL;

    const uint32_t width = read32(chunk + 12);
    const uint32_t height = read32(chunk + 16);
    const uint32_t depth = read32(chunk + 20);
    const uint32_t pitch = read32(chunk + 24);
    if(width == 0 || height == 0 || width > 4096 || height > 4096 ||
       (depth != 4 && depth != 8 && depth != 16 &&
        depth != 24 && depth != 32))
        return NULL;

    const uint64_t pixelSize = static_cast<uint64_t>(pitch) * height;
    const uint32_t paletteSize = depth == 4 ? 64 : depth == 8 ? 1024 : 0;
    if(28 + pixelSize + paletteSize != length)
        return NULL;

    rw::Image* image = rw::Image::create(
        static_cast<int32_t>(width), static_cast<int32_t>(height),
        static_cast<int32_t>(depth));
    if(image == NULL)
        return NULL;
    image->allocate();
    if(image->pixels == NULL || (paletteSize != 0 && image->palette == NULL) ||
       static_cast<uint32_t>(image->stride) != pitch)
    {
        image->destroy();
        return NULL;
    }

    memcpy(image->pixels, chunk + 28, static_cast<size_t>(pixelSize));
    if(paletteSize != 0)
    {
        memcpy(image->palette, chunk + 28 + pixelSize, paletteSize);
        const uint32_t colourCount = depth == 4 ? 16 : 256;
        for(uint32_t index = 0; index < colourCount; ++index)
        {
            const uint32_t alpha =
                static_cast<uint32_t>(image->palette[index * 4 + 3]) * 2;
            image->palette[index * 4 + 3] =
                static_cast<uint8_t>(alpha > 255 ? 255 : alpha);
        }
    }
    image->convertTo32();

    rw::Raster* raster = rw::Raster::createFromImage(image);
    image->destroy();
    return raster;
}

rw::Texture* loadTexture(const TextureDescriptor& descriptor)
{
    std::vector<uint8_t> data;
    if(!readXafEntryByIndex(GRAPH_ARCHIVE_PATH, descriptor.index, data) ||
       data.size() < 12 || data.size() > UINT32_MAX ||
       read32(data.data()) != rw::ID_IMAGE)
        return NULL;

    const uint32_t imageLength = read32(data.data() + 4);
    if(static_cast<uint64_t>(imageLength) + 12 != data.size())
        return NULL;
    rw::Raster* raster = readImageRaster(data.data() + 12, imageLength);
    if(raster == NULL || raster->width != descriptor.width ||
       raster->height != descriptor.height)
    {
        if(raster != NULL)
            raster->destroy();
        return NULL;
    }

    rw::Texture* texture = rw::Texture::create(raster);
    if(texture == NULL)
    {
        raster->destroy();
        return NULL;
    }
    strncpy(texture->name, descriptor.name, sizeof(texture->name) - 1);
    texture->name[sizeof(texture->name) - 1] = '\0';
    texture->setFilter(rw::Texture::LINEAR);
    return texture;
}

void releaseTextureTable(void)
{
    for(int slot = 0; slot < BATTLE_HUD_TEXTURE_COUNT; ++slot)
    {
        if(g_textureTable.textures[slot] != NULL)
            g_textureTable.textures[slot]->destroy();
        g_textureTable.textures[slot] = NULL;
    }
}

uint8_t normalizedLanguage(uint8_t language)
{
    return language >= 1 && language <= 5 ? language : 1;
}

TextureDescriptor descriptorForSlot(int slot, uint8_t language)
{

    static const TextureDescriptor fixed[BATTLE_HUD_TEXTURE_COUNT] =
    {
        { 0x0ad, "GD_HPG_01",   128,  32 },
        { 0x0ae, "GD_HPG_02",    64,  32 },
        { 0x111, "MAP_BG",       512, 256 },
        { 0x116, "MAP_MASK",     512, 256 },
        { 0x117, "MAP_PARTS01",  128, 128 },
        { 0x118, "MAP_PARTS02",   64,  32 },
        { 0x166, "SYSFONT02",     256, 256 },
        { 0x0b0, "GD_NGI_00",      64,  64 },
        { 0x054, "CURSOR",        256,  32 },
        { 0x210, "VS_PARTS_02",    32, 128 }
    };
    TextureDescriptor descriptor = fixed[slot];
    if(slot == BATTLE_HUD_TEXTURE_MAP_BACKGROUND ||
       slot == BATTLE_HUD_TEXTURE_VERSUS_PARTS)
    {
        static const uint32_t mapEntries[6] =
        {
            0x111, 0x111, 0x113, 0x114, 0x112, 0x115
        };
        static const uint32_t versusEntries[6] =
        {
            0x210, 0x210, 0x212, 0x213, 0x211, 0x214
        };
        descriptor.index = slot == BATTLE_HUD_TEXTURE_MAP_BACKGROUND ?
            mapEntries[language] : versusEntries[language];
    }
    return descriptor;
}

bool drawHudSpriteBlended(const HudSprite& sprite,
                          uint32_t sourceBlend,
                          uint32_t destinationBlend)
{
    if(sprite.texture == NULL || sprite.texture->raster == NULL ||
       sprite.atlasWidth <= 0.0f || sprite.atlasHeight <= 0.0f ||
       sprite.width <= 0.0f || sprite.height <= 0.0f ||
       rw::engine == NULL || rw::engine->currentCamera == NULL)
        return false;

    rw::Camera* camera = static_cast<rw::Camera*>(rw::engine->currentCamera);
    if(camera->frameBuffer == NULL || camera->nearPlane <= 0.0f)
        return false;
    const float frameWidth =
        static_cast<float>(camera->frameBuffer->width);
    const float frameHeight =
        static_cast<float>(camera->frameBuffer->height);
    const float horizontalScale = frameWidth / HUD_WIDTH;
    const float verticalScale = frameHeight / HUD_HEIGHT;
    const float viewportScale =
        horizontalScale < verticalScale ? horizontalScale : verticalScale;
    const float viewportX =
        (frameWidth - HUD_WIDTH * viewportScale) * 0.5f;
    const float viewportY =
        (frameHeight - HUD_HEIGHT * viewportScale) * 0.5f;
    const float radians = sprite.rotationDegrees * PI / 180.0f;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    const float localX[4] =
    {
        -sprite.pivotX,
        sprite.width - sprite.pivotX,
        sprite.width - sprite.pivotX,
        -sprite.pivotX
    };
    const float localY[4] =
    {
        -sprite.pivotY,
        -sprite.pivotY,
        sprite.height - sprite.pivotY,
        sprite.height - sprite.pivotY
    };
    float leftU = sprite.sourceX / sprite.atlasWidth;
    float rightU =
        (sprite.sourceX + sprite.sourceWidth) / sprite.atlasWidth;
    if(sprite.flipX)
    {
        const float swap = leftU;
        leftU = rightU;
        rightU = swap;
    }
    const float topV = sprite.sourceY / sprite.atlasHeight;
    const float bottomV =
        (sprite.sourceY + sprite.sourceHeight) / sprite.atlasHeight;
    const float u[4] = { leftU, rightU, rightU, leftU };
    const float v[4] = { topV, topV, bottomV, bottomV };

    using namespace rw::RWDEVICE;
    Im2DVertex vertices[4];
    float xOffset = 0.0f;
    float yOffset = 0.0f;
#ifdef RWHALFPIXEL
    xOffset = -0.5f;
    yOffset = 0.5f;
#endif
    const float reciprocalZ = 1.0f / camera->nearPlane;
    const float z = rw::im2d::GetNearZ();
    for(int index = 0; index < 4; ++index)
    {
        const float rotatedX =
            localX[index] * cosine - localY[index] * sine;
        const float rotatedY =
            localX[index] * sine + localY[index] * cosine;
        const float screenX = viewportX +
            (sprite.x + rotatedX) * viewportScale;
        const float logicalY = battleHudLogicalToHostY(
            sprite.y + rotatedY, g_textureTable.ntsc);
        const float screenY = viewportY + logicalY * viewportScale;
        vertices[index].setScreenX(screenX + xOffset);
        vertices[index].setScreenY(screenY + yOffset);
        vertices[index].setScreenZ(z);
        vertices[index].setCameraZ(camera->nearPlane);
        vertices[index].setRecipCameraZ(reciprocalZ);
        vertices[index].setColor(sprite.colour.r, sprite.colour.g,
                                 sprite.colour.b, sprite.colour.a);
        vertices[index].setU(u[index], reciprocalZ);
        vertices[index].setV(v[index], reciprocalZ);
    }
    static rw::uint16 indices[6] = { 0, 1, 2, 0, 2, 3 };

    const void* oldTexture = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    const uint32_t oldFilter = rw::GetRenderState(rw::TEXTUREFILTER);
    const uint32_t oldAddressU = rw::GetRenderState(rw::TEXTUREADDRESSU);
    const uint32_t oldAddressV = rw::GetRenderState(rw::TEXTUREADDRESSV);
    const uint32_t oldVertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
    const uint32_t oldZTest = rw::GetRenderState(rw::ZTESTENABLE);
    const uint32_t oldZWrite = rw::GetRenderState(rw::ZWRITEENABLE);
    const uint32_t oldCull = rw::GetRenderState(rw::CULLMODE);
    const uint32_t oldSourceBlend = rw::GetRenderState(rw::SRCBLEND);
    const uint32_t oldDestinationBlend = rw::GetRenderState(rw::DESTBLEND);

    rw::SetRenderStatePtr(rw::TEXTURERASTER, sprite.texture->raster);
    rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::LINEAR);
    rw::SetRenderState(rw::TEXTUREADDRESSU, rw::Texture::CLAMP);
    rw::SetRenderState(rw::TEXTUREADDRESSV, rw::Texture::CLAMP);
    rw::SetRenderState(rw::VERTEXALPHA, 1);
    rw::SetRenderState(rw::ZTESTENABLE, 0);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);
    rw::SetRenderState(rw::SRCBLEND, sourceBlend);
    rw::SetRenderState(rw::DESTBLEND, destinationBlend);
    rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, vertices, 4,
                                     indices, 6);

    rw::SetRenderStatePtr(rw::TEXTURERASTER, const_cast<void*>(oldTexture));
    rw::SetRenderState(rw::TEXTUREFILTER, oldFilter);
    rw::SetRenderState(rw::TEXTUREADDRESSU, oldAddressU);
    rw::SetRenderState(rw::TEXTUREADDRESSV, oldAddressV);
    rw::SetRenderState(rw::VERTEXALPHA, oldVertexAlpha);
    rw::SetRenderState(rw::ZTESTENABLE, oldZTest);
    rw::SetRenderState(rw::ZWRITEENABLE, oldZWrite);
    rw::SetRenderState(rw::CULLMODE, oldCull);
    rw::SetRenderState(rw::SRCBLEND, oldSourceBlend);
    rw::SetRenderState(rw::DESTBLEND, oldDestinationBlend);
    return true;
}

bool drawHudMapClipped(rw::Texture* texture,
                       const RadarMapTransform& transform)
{
    if(texture == NULL || texture->raster == NULL ||
       rw::engine == NULL || rw::engine->currentCamera == NULL)
        return false;

    rw::Camera* camera = static_cast<rw::Camera*>(rw::engine->currentCamera);
    if(camera->frameBuffer == NULL || camera->nearPlane <= 0.0f)
        return false;

    const float frameWidth =
        static_cast<float>(camera->frameBuffer->width);
    const float frameHeight =
        static_cast<float>(camera->frameBuffer->height);
    const float horizontalScale = frameWidth / HUD_WIDTH;
    const float verticalScale = frameHeight / HUD_HEIGHT;
    const float viewportScale =
        horizontalScale < verticalScale ? horizontalScale : verticalScale;
    const float viewportX =
        (frameWidth - HUD_WIDTH * viewportScale) * 0.5f;
    const float viewportY =
        (frameHeight - HUD_HEIGHT * viewportScale) * 0.5f;

    const float radians = transform.compassDegrees * PI / 180.0f;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    const float mapWidth = 768.0f * RADAR_MAP_SCALE;
    const float mapHeight = 512.0f * RADAR_MAP_SCALE;
    const float pivotX = transform.pivotX * RADAR_MAP_SCALE;
    const float pivotY = transform.pivotY * RADAR_MAP_SCALE;
    const int segmentCount = 64;
    const int innerFirst = 1;
    const int outerFirst = innerFirst + segmentCount;
    const int vertexCount = 1 + segmentCount * 2;
    const int indexCount = segmentCount * 9;
    const float innerRadius = 54.0f;
    const float outerRadius = 58.0f;

    using namespace rw::RWDEVICE;
    Im2DVertex vertices[vertexCount];
    rw::uint16 indices[indexCount];
    float xOffset = 0.0f;
    float yOffset = 0.0f;
#ifdef RWHALFPIXEL
    xOffset = -0.5f;
    yOffset = 0.5f;
#endif
    const float reciprocalZ = 1.0f / camera->nearPlane;
    const float z = rw::im2d::GetNearZ();

    const auto setVertex =
        [&](Im2DVertex& vertex, float x, float y, uint8_t alpha)
        {
            const float localX = x * cosine + y * sine;
            const float localY = -x * sine + y * cosine;
            const float u = (pivotX + localX) / mapWidth;
            const float v = (pivotY + localY) / mapHeight;
            const float screenX = viewportX +
                (RADAR_CENTER_X + x) * viewportScale;
            const float logicalY = battleHudLogicalToHostY(
                RADAR_CENTER_Y + y, g_textureTable.ntsc);
            const float screenY = viewportY + logicalY * viewportScale;
            vertex.setScreenX(screenX + xOffset);
            vertex.setScreenY(screenY + yOffset);
            vertex.setScreenZ(z);
            vertex.setCameraZ(camera->nearPlane);
            vertex.setRecipCameraZ(reciprocalZ);
            vertex.setColor(255, 255, 255, alpha);
            vertex.setU(u, reciprocalZ);
            vertex.setV(v, reciprocalZ);
        };

    setVertex(vertices[0], 0.0f, 0.0f, 255);
    for(int segment = 0; segment < segmentCount; ++segment)
    {
        const float angle =
            static_cast<float>(segment) * 2.0f * PI /
            static_cast<float>(segmentCount);
        const float x = cosf(angle);
        const float y = sinf(angle);
        setVertex(vertices[innerFirst + segment],
                  x * innerRadius, y * innerRadius, 255);
        setVertex(vertices[outerFirst + segment],
                  x * outerRadius, y * outerRadius, 0);

        const int next = (segment + 1) % segmentCount;
        const int index = segment * 9;
        indices[index + 0] = 0;
        indices[index + 1] =
            static_cast<rw::uint16>(innerFirst + segment);
        indices[index + 2] =
            static_cast<rw::uint16>(innerFirst + next);
        indices[index + 3] =
            static_cast<rw::uint16>(innerFirst + segment);
        indices[index + 4] =
            static_cast<rw::uint16>(outerFirst + segment);
        indices[index + 5] =
            static_cast<rw::uint16>(outerFirst + next);
        indices[index + 6] =
            static_cast<rw::uint16>(innerFirst + segment);
        indices[index + 7] =
            static_cast<rw::uint16>(outerFirst + next);
        indices[index + 8] =
            static_cast<rw::uint16>(innerFirst + next);
    }

    const void* oldTexture = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    const uint32_t oldFilter = rw::GetRenderState(rw::TEXTUREFILTER);
    const uint32_t oldAddressU = rw::GetRenderState(rw::TEXTUREADDRESSU);
    const uint32_t oldAddressV = rw::GetRenderState(rw::TEXTUREADDRESSV);
    const uint32_t oldVertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
    const uint32_t oldZTest = rw::GetRenderState(rw::ZTESTENABLE);
    const uint32_t oldZWrite = rw::GetRenderState(rw::ZWRITEENABLE);
    const uint32_t oldCull = rw::GetRenderState(rw::CULLMODE);
    const uint32_t oldSourceBlend = rw::GetRenderState(rw::SRCBLEND);
    const uint32_t oldDestinationBlend = rw::GetRenderState(rw::DESTBLEND);

    rw::SetRenderStatePtr(rw::TEXTURERASTER, texture->raster);
    rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::LINEAR);
    rw::SetRenderState(rw::TEXTUREADDRESSU, rw::Texture::CLAMP);
    rw::SetRenderState(rw::TEXTUREADDRESSV, rw::Texture::CLAMP);
    rw::SetRenderState(rw::VERTEXALPHA, 1);
    rw::SetRenderState(rw::ZTESTENABLE, 0);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);
    rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
    rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
    rw::im2d::RenderIndexedPrimitive(
        rw::PRIMTYPETRILIST, vertices, vertexCount, indices, indexCount);

    rw::SetRenderStatePtr(rw::TEXTURERASTER, const_cast<void*>(oldTexture));
    rw::SetRenderState(rw::TEXTUREFILTER, oldFilter);
    rw::SetRenderState(rw::TEXTUREADDRESSU, oldAddressU);
    rw::SetRenderState(rw::TEXTUREADDRESSV, oldAddressV);
    rw::SetRenderState(rw::VERTEXALPHA, oldVertexAlpha);
    rw::SetRenderState(rw::ZTESTENABLE, oldZTest);
    rw::SetRenderState(rw::ZWRITEENABLE, oldZWrite);
    rw::SetRenderState(rw::CULLMODE, oldCull);
    rw::SetRenderState(rw::SRCBLEND, oldSourceBlend);
    rw::SetRenderState(rw::DESTBLEND, oldDestinationBlend);
    return true;
}

HudSprite sprite(rw::Texture* texture,
                 float atlasWidth, float atlasHeight,
                 float sourceX, float sourceY,
                 float sourceWidth, float sourceHeight,
                 float x, float y, float width, float height,
                 float pivotX, float pivotY, float rotationDegrees)
{
    HudSprite result = {};
    result.texture = texture;
    result.atlasWidth = atlasWidth;
    result.atlasHeight = atlasHeight;
    result.sourceX = sourceX;
    result.sourceY = sourceY;
    result.sourceWidth = sourceWidth;
    result.sourceHeight = sourceHeight;
    result.x = x;
    result.y = y;
    result.width = width;
    result.height = height;
    result.pivotX = pivotX;
    result.pivotY = pivotY;
    result.rotationDegrees = rotationDegrees;
    result.colour.r = 255;
    result.colour.g = 255;
    result.colour.b = 255;
    result.colour.a = 255;
    return result;
}

float currentCameraYaw(void)
{
    if(rw::engine == NULL || rw::engine->currentCamera == NULL)
        return 0.0f;
    rw::Camera* camera = static_cast<rw::Camera*>(rw::engine->currentCamera);
    rw::Frame* frame = camera->getFrame();
    if(frame == NULL)
        return 0.0f;
    return atan2f(frame->matrix.at.x, frame->matrix.at.z) * 180.0f / PI;
}

}

bool
loadBattleHudTextureTable(uint8_t language)
{
    language = normalizedLanguage(language);
    if(g_textureTable.attempted && g_textureTable.language == language)
    {
        for(int slot = 0; slot < BATTLE_HUD_TEXTURE_COUNT; ++slot)
            if(g_textureTable.textures[slot] == NULL)
                return false;
        return true;
    }

    releaseTextureTable();
    g_textureTable.language = language;
    g_textureTable.attempted = true;
    for(int slot = 0; slot < BATTLE_HUD_TEXTURE_COUNT; ++slot)
    {
        const TextureDescriptor descriptor =
            descriptorForSlot(slot, language);
        g_textureTable.textures[slot] = loadTexture(descriptor);
        if(g_textureTable.textures[slot] == NULL)
        {
            releaseTextureTable();
            return false;
        }
    }
    return true;
}

rw::Texture*
battleHudTexture(BattleHudTextureSlot slot)
{
    if(slot < 0 || slot >= BATTLE_HUD_TEXTURE_COUNT)
        return NULL;
    return g_textureTable.textures[slot];
}

void
setBattleHudVideoTiming(bool ntsc)
{
    g_textureTable.ntsc = ntsc;
}

bool
drawHudSprite(const HudSprite& sprite)
{
    return drawHudSpriteBlended(
        sprite, rw::BLENDSRCALPHA, rw::BLENDINVSRCALPHA);
}

void
drawHudRadar(const Game& game, uint16_t stageId)
{
    const BattleHudRadarState& radarState = game.runtimeState().hudRadar;
    if(!battleHudRadarVisible(radarState) ||
       !loadBattleHudTextureTable(game.options().language))
        return;

    setBattleHudVideoTiming(game.videoTimingMode() != 0);
    const GameSlotRecord& player = game.runtimeState().player;
    const float cameraYaw = currentCameraYaw();
    const RadarMapTransform transform = battleHudRadarMapTransform(
        stageId, player.position.x, player.position.z, cameraYaw);
    if(!transform.available)
        return;

    drawHudMapClipped(
        battleHudTexture(BATTLE_HUD_TEXTURE_MAP_BACKGROUND), transform);

    if(battleHudRadarDynamicCellsVisible(radarState))
    {
        const StoryHudRadarState& storyRadar =
            game.runtimeState().story.progression.hudRadar;
        const StoryHudTarget& objective =
            storyRadar.objective.useAlternate ?
                storyRadar.objective.alternate :
                storyRadar.objective.primary;
        const bool objectiveActive = storyRadar.objective.useAlternate ?
            storyRadar.objective.alternateActive != 0 :
            storyRadar.objective.primaryActive != 0;
        const StoryActorState* objectiveActor =
            objectiveActive && objective.characterId > 0 ?
                findStoryActor(game.runtimeState().story,
                               static_cast<uint16_t>(
                                   objective.characterId)) : NULL;
        const bool objectiveActorLive =
            objectiveActor != NULL && objectiveActor->active != 0;
        const RadarCellGeometry objectiveCell = battleHudObjectiveCell(
            transform, stageId,
            objectiveActive ? objective.characterId : -1, objective.stageId,
            storyRadar.objective.useAlternate != 0,
            player.position.x, player.position.z, objectiveActorLive,
            objectiveActorLive ? objectiveActor->position[0] : 0.0f,
            objectiveActorLive ? objectiveActor->position[2] : 0.0f,
            radarState.overview != 0);
        const auto drawCell = [](const RadarCellGeometry& cell)
        {
            if(!cell.visible)
                return;
            HudSprite marker = sprite(
                battleHudTexture(BATTLE_HUD_TEXTURE_MAP_PARTS),
                64.0f, 40.0f, cell.sourceX, cell.sourceY,
                cell.sourceWidth, cell.sourceHeight, cell.x, cell.y,
                cell.width * cell.scale, cell.height * cell.scale,
                cell.pivotX * cell.scale, cell.pivotY * cell.scale,
                cell.rotationDegrees);
            drawHudSprite(marker);
        };
        drawCell(objectiveCell);

        if(storyRadar.suppressRelationshipCellsOnce == 0)
        {
            const StoryHudTarget* targets =
                storyRelationshipTargets(game.runtimeState().story);
            for(int i = 0; i < 2; ++i)
            {
                if(targets[i].characterId <= 0)
                    continue;
                const StoryActorState* actor = findStoryActor(
                    game.runtimeState().story,
                    static_cast<uint16_t>(targets[i].characterId));
                const bool actorLive = actor != NULL && actor->active != 0;
                drawCell(battleHudRelationshipCell(
                    transform, stageId, targets[i].stageId,
                    player.position.x, player.position.z, actorLive,
                    actorLive ? actor->position[0] : 0.0f,
                    actorLive ? actor->position[2] : 0.0f,
                    radarState.overview != 0));
            }
        }
        storyRadar.suppressRelationshipCellsOnce = 0;
    }

    for(int index = 0; index < BATTLE_HUD_RADAR_DESTINATION_COUNT; ++index)
    {
        const RadarProjection projection =
            battleHudProjectRadarDestination(
                transform, index, player.position.x, player.position.z);
        if(!projection.visible)
            continue;
        HudSprite marker = sprite(
            battleHudTexture(BATTLE_HUD_TEXTURE_MAP_PARTS),
            64.0f, 40.0f, 32.0f, 0.0f, 16.0f, 16.0f,
            projection.x, projection.y, 16.0f, 16.0f,
            8.0f, 8.0f, 0.0f);
        drawHudSprite(marker);
    }

    HudSprite ring = sprite(
        battleHudTexture(BATTLE_HUD_TEXTURE_MAP_RING),
        224.0f, 224.0f, 0.0f, 0.0f, 224.0f, 224.0f,
        RADAR_CENTER_X, RADAR_CENTER_Y, 224.0f, 224.0f,
        112.0f, 112.0f, transform.compassDegrees);
    drawHudSprite(ring);

    const float compassRadians = transform.compassDegrees * PI / 180.0f;
    HudSprite north = sprite(
        battleHudTexture(BATTLE_HUD_TEXTURE_MAP_PARTS),
        64.0f, 40.0f, 48.0f, 0.0f, 16.0f, 16.0f,
        82.0f + sinf(compassRadians) * 50.0f,
        394.0f - cosf(compassRadians) * 50.0f,
        16.0f, 16.0f, 0.0f, 0.0f, 0.0f);
    drawHudSprite(north);

    HudSprite cursor = sprite(
        battleHudTexture(BATTLE_HUD_TEXTURE_MAP_PARTS),
        64.0f, 40.0f, 24.0f, 16.0f, 24.0f, 24.0f,
        RADAR_CENTER_X, RADAR_CENTER_Y, 24.0f, 24.0f,
        12.0f, 12.0f,
        battleHudRadarCursorRotation(
            stageId, player.headingDegrees, cameraYaw));
    drawHudSprite(cursor);
}

#endif

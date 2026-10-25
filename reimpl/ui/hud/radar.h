#ifndef HUD_RADAR_H
#define HUD_RADAR_H

#include <stdint.h>

class Game;

namespace rw
{
struct Texture;
}

struct HudColour
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct HudSprite
{
    rw::Texture* texture;
    float atlasWidth;
    float atlasHeight;
    float sourceX;
    float sourceY;
    float sourceWidth;
    float sourceHeight;
    float x;
    float y;
    float width;
    float height;
    float pivotX;
    float pivotY;
    float rotationDegrees;
    HudColour colour;
    bool flipX;
};

enum BattleHudTextureSlot
{
    BATTLE_HUD_TEXTURE_HEALTH_FRAME = 0,
    BATTLE_HUD_TEXTURE_HEALTH_FILL = 1,
    BATTLE_HUD_TEXTURE_MAP_BACKGROUND = 2,
    BATTLE_HUD_TEXTURE_MAP_MASK = 3,
    BATTLE_HUD_TEXTURE_MAP_RING = 4,
    BATTLE_HUD_TEXTURE_MAP_PARTS = 5,
    BATTLE_HUD_TEXTURE_NAME_FONT = 6,
    BATTLE_HUD_TEXTURE_NGI = 7,
    BATTLE_HUD_TEXTURE_CURSOR = 8,
    BATTLE_HUD_TEXTURE_VERSUS_PARTS = 9,
    BATTLE_HUD_TEXTURE_COUNT = 10
};

struct RadarMapTransform
{
    bool available;
    uint16_t stageId;
    float pivotX;
    float pivotY;
    float worldXDivisor;
    float worldZDivisor;
    float compassDegrees;
};

struct RadarProjection
{
    bool visible;
    bool inside;
    float x;
    float y;
    float distance;
    float rotationDegrees;
};

struct BattleHudRadarState
{
    uint32_t interactionStateFlags;
    int32_t battleHudMask;
    float animationScale;
    uint8_t globalSuppress;
    uint8_t overview;
    uint8_t animationActive;
    uint8_t animationReverse;
    uint8_t dynamicCellsEnabled;
    uint8_t dynamicCellStateReady;
    uint8_t overviewDynamicCellsEnabled;
};

struct RadarCellGeometry
{
    bool visible;
    bool inside;
    float x;
    float y;
    float sourceX;
    float sourceY;
    float sourceWidth;
    float sourceHeight;
    float width;
    float height;
    float pivotX;
    float pivotY;
    float scale;
    float rotationDegrees;
    int32_t renderKey;
};

inline BattleHudRadarState battleHudDefaultRadarState(void)
{
    BattleHudRadarState state = {};
    state.battleHudMask = -1;
    state.animationScale = 1.17f;
    state.dynamicCellsEnabled = 1;
    state.dynamicCellStateReady = 1;
    return state;
}
bool battleHudRadarVisible(const BattleHudRadarState& state);
bool battleHudRadarDynamicCellsVisible(const BattleHudRadarState& state);
void battleHudAdvanceRadarAnimation(BattleHudRadarState& state);

RadarCellGeometry battleHudRelationshipCell(
    const RadarMapTransform& transform, uint16_t currentStageId,
    int16_t targetStageId, float playerX, float playerZ,
    bool liveActorAvailable, float actorX, float actorZ, bool overview);
RadarCellGeometry battleHudObjectiveCell(
    const RadarMapTransform& transform, uint16_t currentStageId,
    int16_t objectiveId, int16_t objectiveStageId, bool alternate,
    float playerX, float playerZ, bool liveActorAvailable,
    float actorX, float actorZ, bool overview);

float battleHudLogicalToNativeY(float logicalY, bool ntsc);
float battleHudLogicalToHostY(float logicalY, bool ntsc);

float battleHudRadarCompassRotation(uint16_t stageId,
                                    float cameraYawDegrees);
float battleHudRadarCursorRotation(uint16_t stageId,
                                   float playerHeadingDegrees,
                                   float cameraYawDegrees,
                                   bool northUp = false);
RadarMapTransform battleHudRadarMapTransform(uint16_t stageId,
                                             float playerX, float playerZ,
                                             float cameraYawDegrees);
RadarProjection battleHudProjectRadarDestination(
    const RadarMapTransform& transform, int destinationIndex,
    float playerX, float playerZ);
RadarProjection battleHudProjectRadarOffset(float offsetX, float offsetY,
                                            float compassDegrees,
                                            float insideRadius,
                                            bool clampOutside);

#ifndef BATTLE_HUD_TEST

bool loadBattleHudTextureTable(uint8_t language);
rw::Texture* battleHudTexture(BattleHudTextureSlot slot);

void setBattleHudVideoTiming(bool ntsc);

bool drawHudSprite(const HudSprite& sprite);

void drawHudRadar(const Game& game, uint16_t stageId);
#endif

#endif

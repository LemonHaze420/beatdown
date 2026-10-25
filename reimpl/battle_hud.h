#ifndef BATTLE_HUD_H
#define BATTLE_HUD_H

#include "hud_radar.h"

#include <stdint.h>

class Game;

enum BattleHudHealthLayer
{
    BATTLE_HUD_HEALTH_BACKGROUND,
    BATTLE_HUD_HEALTH_DELAYED,
    BATTLE_HUD_HEALTH_CURRENT,
    BATTLE_HUD_HEALTH_FRAME
};

struct BattleHudHealthQuad
{
    BattleHudTextureSlot texture;
    BattleHudHealthLayer layer;
    float sourceX;
    float sourceY;
    float sourceWidth;
    float sourceHeight;
    float atlasWidth;
    float atlasHeight;
    float x;
    float y;
    float width;
    float height;
    HudColour colour;
    bool flipX;
};

struct BattleHudPlateGeometry
{
    float x;
    float y;
    float cellWidth;
    float cellHeight;
    float atlasWidth;
    float atlasHeight;
    HudColour colour;
    bool rightAligned;
};

int battleHudBuildHealthAssembly(
    int side, int member, int maximum, int delayed, int current,
    bool goldOpponent, int lowHealthPhase,
    BattleHudHealthQuad* quads, int capacity);
BattleHudPlateGeometry battleHudPlateGeometry(
    int side, int member, bool alternateOpponentColour);
float battleHudTextWidth(const char* text);
uint16_t battleHudDisplayNameTextId(int16_t characterId);

#ifndef BATTLE_HUD_TEST

void drawBattleHud(const Game& game, uint16_t stageId);
#endif

#endif

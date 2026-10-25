#include "battle_hud.h"

#include "../formats/BATTLE_HUD/battle_hud_data.h"
#include "../formats/CHARACTER_DB/character_records.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

namespace
{

const int BATTLE_HUD_SIDE_COUNT = 2;
const int BATTLE_HUD_TEAM_MEMBER_COUNT = 3;
const int HEALTH_LAYER_COUNT = 4;
const int HEALTH_SEGMENT_COUNT = 3;
const float HEALTH_VALUE_TO_SOURCE_SCALE = 0.018f;
const float SECONDARY_MEMBER_SCALE = 0.7f;
const float HEALTH_LOGICAL_HEIGHT = 32.0f;
const float HEALTH_FRAME_LOGICAL_WIDTH = 128.0f;
const float HEALTH_FILL_LOGICAL_WIDTH = 111.0f;

struct HealthSegment
{
    float sourceX;
    float sourceWidth;
};

const HealthSegment HEALTH_FRAME_SEGMENTS[HEALTH_SEGMENT_COUNT] =
{
    {   0.0f, 10.0f },
    {  20.0f, 70.0f },
    { 117.0f, 11.0f }
};

const HealthSegment HEALTH_FILL_SEGMENTS[HEALTH_SEGMENT_COUNT] =
{
    {   0.0f,  2.0f },
    {  20.0f, 70.0f },
    { 109.0f,  2.0f }
};

HudColour colour(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    HudColour result = { r, g, b, a };
    return result;
}

HudColour healthColour(int side, int member, BattleHudHealthLayer layer,
                       int maximum, int current, bool goldOpponent,
                       int lowHealthPhase)
{
    if(layer == BATTLE_HUD_HEALTH_FRAME)
        return colour(255, 255, 255, 255);
    if(layer == BATTLE_HUD_HEALTH_BACKGROUND)
        return colour(0, 0, 0, 255);
    if(layer == BATTLE_HUD_HEALTH_DELAYED)
    {
        if(side == 0)
            return colour(255, 32, 32, member == 0 ? 255 : 0);
        return colour(160, 190, 255, 255);
    }

    if(maximum > 0 && current <= maximum * 0.2f)
    {
        if(lowHealthPhase < 0)
            lowHealthPhase = 0;
        if(lowHealthPhase > 7)
            lowHealthPhase = 7;
        const int green = 180 - static_cast<int>(lowHealthPhase * 22.5f);
        const int blue = 50 - static_cast<int>(lowHealthPhase * 6.25f);
        return colour(255, static_cast<uint8_t>(green),
                      static_cast<uint8_t>(blue), 255);
    }
    if(side == 0)
        return colour(0, 255, 255, 255);
    return goldOpponent ? colour(255, 192, 0, 255) :
                          colour(255, 0, 190, 255);
}

void appendHealthQuad(BattleHudHealthQuad* quads, int capacity, int* count,
                      const BattleHudHealthQuad& quad)
{
    if(quad.width <= 0.0f || quad.height <= 0.0f ||
       quad.sourceWidth <= 0.0f || quad.sourceHeight <= 0.0f)
        return;
    if(quads != NULL && *count < capacity)
        quads[*count] = quad;
    ++*count;
}

}

int
battleHudBuildHealthAssembly(
    int side, int member, int maximum, int delayed, int current,
    bool goldOpponent, int lowHealthPhase,
    BattleHudHealthQuad* quads, int capacity)
{
    if(side < 0 || side >= BATTLE_HUD_SIDE_COUNT || member < 0 ||
       member >= BATTLE_HUD_TEAM_MEMBER_COUNT || capacity < 0)
        return 0;

    if(current > maximum)
    {
        maximum = current;
        delayed = current;
    }
    if(maximum < 0)
        maximum = 0;
    if(current < 0)
        current = 0;
    if(delayed < 0)
        delayed = 0;

    const float memberScale = member == 0 ? 1.0f : SECONDARY_MEMBER_SCALE;
    const float sideBias = side == 0 ? -2.0f : 1.0f;
    const float maximumScale =
        (maximum * HEALTH_VALUE_TO_SOURCE_SCALE * memberScale + sideBias) /
        100.0f;
    const float delayedScale =
        delayed * HEALTH_VALUE_TO_SOURCE_SCALE * memberScale / 100.0f;
    const float currentScale =
        current * HEALTH_VALUE_TO_SOURCE_SCALE * memberScale / 100.0f;
    const float maximumSpan = maximumScale * 140.0f;
    const float maximumSpanWithInset = maximumSpan + 4.0f;
    const float memberY[BATTLE_HUD_TEAM_MEMBER_COUNT] =
    {
        47.0f, 81.0f, 109.0f
    };
    const BattleHudHealthLayer order[HEALTH_LAYER_COUNT] =
    {
        BATTLE_HUD_HEALTH_BACKGROUND,
        BATTLE_HUD_HEALTH_DELAYED,
        BATTLE_HUD_HEALTH_CURRENT,
        BATTLE_HUD_HEALTH_FRAME
    };

    int count = 0;
    for(int layerIndex = 0; layerIndex < HEALTH_LAYER_COUNT; ++layerIndex)
    {
        const BattleHudHealthLayer layer = order[layerIndex];
        const bool frameLayer = layer == BATTLE_HUD_HEALTH_FRAME;
        const HealthSegment* segments =
            frameLayer ? HEALTH_FRAME_SEGMENTS : HEALTH_FILL_SEGMENTS;
        const float atlasWidth = frameLayer ?
            HEALTH_FRAME_LOGICAL_WIDTH : HEALTH_FILL_LOGICAL_WIDTH;
        const BattleHudTextureSlot texture = frameLayer ?
            BATTLE_HUD_TEXTURE_HEALTH_FRAME :
            BATTLE_HUD_TEXTURE_HEALTH_FILL;
        const HudColour tint = healthColour(
            side, member, layer, maximum, current,
            goldOpponent, lowHealthPhase);

        for(int segment = 0; segment < HEALTH_SEGMENT_COUNT; ++segment)
        {
            float sourceWidth = segments[segment].sourceWidth;
            float destinationScale = sourceWidth / atlasWidth;
            float x = 0.0f;

            if(side == 0)
            {
                if(layer == BATTLE_HUD_HEALTH_DELAYED ||
                   layer == BATTLE_HUD_HEALTH_CURRENT)
                {
                    const int health =
                        layer == BATTLE_HUD_HEALTH_DELAYED ? delayed : current;
                    const float valueScale =
                        layer == BATTLE_HUD_HEALTH_DELAYED ?
                            delayedScale : currentScale;
                    if(health > 60)
                    {
                        if(segment == 2)
                            x = valueScale * 140.0f + 45.0f;
                        else if(segment == 1)
                        {
                            destinationScale *= 2.0f * valueScale;
                            x = 45.0f;
                        }
                        else
                            x = 43.0f;
                    }
                    else if(segment == 2)
                    {
                        x = 45.0f;
                        if(health > 30 && health < 60)
                            sourceWidth *= 0.5f;
                        else if(health <= 20)
                            destinationScale = 0.0f;
                    }
                    else if(segment == 1)
                        destinationScale = 0.0f;
                    else
                    {
                        x = 43.0f;
                        if(health <= 0)
                            destinationScale = 0.0f;
                        else if(health < 20)
                            sourceWidth *= 0.5f;
                    }
                }
                else if(frameLayer)
                {
                    if(segment == 2)
                        x = maximumSpan + 46.0f;
                    else if(segment == 1)
                    {
                        destinationScale *= 2.0f * maximumScale;
                        x = 46.0f;
                    }
                    else
                        x = 36.0f;
                }
                else
                {
                    if(segment == 2)
                        x = maximumSpan + 45.0f;
                    else if(segment == 1)
                    {
                        destinationScale *= 2.0f * maximumScale;
                        x = 45.0f;
                    }
                    else
                        x = 43.0f;
                }
                x -= 6.0f;
            }
            else
            {
                if(layer == BATTLE_HUD_HEALTH_DELAYED ||
                   layer == BATTLE_HUD_HEALTH_CURRENT)
                {
                    const int health =
                        layer == BATTLE_HUD_HEALTH_DELAYED ? delayed : current;
                    const float valueScale =
                        layer == BATTLE_HUD_HEALTH_DELAYED ?
                            delayedScale : currentScale;
                    const float valueSpan = valueScale * 140.0f;
                    const float valueSpanWithInset = valueSpan + 4.0f;
                    if(health > 60)
                    {
                        if(segment == 2)
                            x = 576.0f - valueSpanWithInset + 19.0f;
                        else if(segment == 1)
                        {
                            destinationScale *= 2.0f * valueScale;
                            x = 576.0f - valueSpanWithInset + 21.0f;
                        }
                        else
                            x = valueSpan + 21.0f +
                                (576.0f - valueSpanWithInset);
                    }
                    else if(segment == 2)
                    {
                        float capPosition = 19.0f;
                        if(health <= 0)
                            destinationScale = 0.0f;
                        else if(health < 15)
                        {
                            sourceWidth = 1.0f;
                            capPosition = 20.0f;
                        }
                        else
                            sourceWidth = 2.0f;
                        x = capPosition +
                            (576.0f - valueSpanWithInset) + 2.0f;
                    }
                    else if(segment == 1)
                        destinationScale = 0.0f;
                    else
                    {
                        if(health <= 45)
                            destinationScale = 0.0f;
                        else
                            sourceWidth = 2.0f;
                        x = 576.0f - valueSpanWithInset + 23.0f;
                    }
                }
                else if(frameLayer)
                {
                    if(segment == 2)
                        x = 576.0f - maximumSpanWithInset + 12.0f;
                    else if(segment == 1)
                    {
                        destinationScale *= 2.0f * maximumScale;
                        x = 576.0f - maximumSpanWithInset + 22.0f;
                    }
                    else
                        x = maximumSpan + 22.0f +
                            (576.0f - maximumSpanWithInset);
                }
                else
                {
                    if(segment == 2)
                        x = 576.0f - maximumSpanWithInset + 19.0f;
                    else if(segment == 1)
                    {
                        destinationScale *= 2.0f * maximumScale;
                        x = 576.0f - maximumSpanWithInset + 21.0f;
                    }
                    else
                        x = maximumSpan + 21.0f +
                            (576.0f - maximumSpanWithInset);
                }
                x += 6.0f;
            }

            BattleHudHealthQuad quad = {};
            quad.texture = texture;
            quad.layer = layer;
            quad.sourceX = segments[segment].sourceX;
            quad.sourceY = 0.0f;
            quad.sourceWidth = sourceWidth;
            quad.sourceHeight = HEALTH_LOGICAL_HEIGHT;
            quad.atlasWidth = atlasWidth;
            quad.atlasHeight = HEALTH_LOGICAL_HEIGHT;
            quad.x = x;
            quad.y = memberY[member] + 40.0f;
            quad.width = atlasWidth * destinationScale;
            quad.height = HEALTH_LOGICAL_HEIGHT * memberScale;
            quad.colour = tint;
            quad.flipX = side == 1;
            appendHealthQuad(quads, capacity, &count, quad);
        }
    }
    return count;
}

BattleHudPlateGeometry
battleHudPlateGeometry(int side, int member,
                       bool alternateOpponentColour)
{
    BattleHudPlateGeometry geometry = {};
    if(side < 0 || side >= BATTLE_HUD_SIDE_COUNT || member < 0 ||
       member >= BATTLE_HUD_TEAM_MEMBER_COUNT)
        return geometry;

    const BattleHudDataPoint anchor =
        BATTLE_HUD_PLATE_ANCHORS[side * BATTLE_HUD_TEAM_MEMBER_COUNT + member];
    geometry.x = anchor.x + (side == 1 ? 104.0f : 0.0f);
    geometry.y = anchor.y;
    geometry.cellWidth = BATTLE_HUD_GLYPH_CELL_WIDTH;
    geometry.cellHeight = BATTLE_HUD_GLYPH_CELL_HEIGHT;
    geometry.atlasWidth = BATTLE_HUD_FONT_ATLAS_WIDTH;
    geometry.atlasHeight = BATTLE_HUD_FONT_ATLAS_HEIGHT;
    geometry.colour = side == 1 && alternateOpponentColour ?
        colour(0xaa, 0xbf, 0xc9, 0xff) :
        colour(0xff, 0xff, 0xff, 0xff);
    geometry.rightAligned = side == 1;
    return geometry;
}

float
battleHudTextWidth(const char* text)
{
    if(text == NULL)
        return 0.0f;
    float lineWidth = 0.0f;
    float widest = 0.0f;
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        const unsigned int character = *cursor++;
        if(character == '\n')
        {
            if(lineWidth > widest)
                widest = lineWidth;
            lineWidth = 0.0f;
        }
        else if(character == '\t')
            lineWidth += BATTLE_HUD_GLYPH_CELL_WIDTH * 4.0f;
        else if(character == 0x20)
            lineWidth += BATTLE_HUD_GLYPH_METRICS[0].advance;
        else if(character > 0x20)
            lineWidth += BATTLE_HUD_GLYPH_METRICS[
                character - BATTLE_HUD_GLYPH_FIRST].advance;
    }
    return lineWidth > widest ? lineWidth : widest;
}

uint16_t
battleHudDisplayNameTextId(int16_t characterId)
{
    const int resourceId = characterRecordResourceId(
        static_cast<uint16_t>(characterId));
    int nameId = characterId;
    switch(resourceId)
    {
    case 0x54: nameId = 0x4a; break;
    case 0x74: nameId = 0x4d; break;
    case 0x23: nameId = 0x4b; break;
    case 0x71:
    case 0x9a: nameId = 0x6c; break;
    case 0x73:
    case 0x9b: nameId = 0x6b; break;
    case 0x4e:
    case 0x99: nameId = 0x44; break;
    case 0x3f:
    case 0x9c: nameId = 0x35; break;
    case 0x43: nameId = 0x39; break;
    case 0x49: nameId = 0x3f; break;
    case 0x9d:
    case 0x18: nameId = 0x16; break;
    case 2: nameId = 0; break;
    default: break;
    }
    return static_cast<uint16_t>(0x2af9 + nameId);
}

#ifndef BATTLE_HUD_TEST

#include "game.h"
#include "scene_view.h"

#include "../formats/XAF/xaf_runtime.h"

#include <rw.h>

#include <stdint.h>

namespace
{

struct HealthTransition
{
    int maximum;
    int current;
    int delayed;
    int pendingDamage;
    int16_t characterId;
    float frameAccumulator;
    bool initialized;
};

struct BattleHudState
{
    HealthTransition health[BATTLE_HUD_SIDE_COUNT]
                           [BATTLE_HUD_TEAM_MEMBER_COUNT];
    float healthLastTime;
    bool healthTimeInitialized;
    float lowHealthFrameAccumulator;
    int lowHealthPhase;
    int lowHealthSidePhase[BATTLE_HUD_SIDE_COUNT];
    bool lowHealthDescending;
};

BattleHudState g_battleHud = {};

HudSprite makeSprite(rw::Texture* texture,
                     float atlasWidth, float atlasHeight,
                     float sourceX, float sourceY,
                     float sourceWidth, float sourceHeight,
                     float x, float y, float width, float height,
                     const HudColour& tint)
{
    HudSprite sprite = {};
    sprite.texture = texture;
    sprite.atlasWidth = atlasWidth;
    sprite.atlasHeight = atlasHeight;
    sprite.sourceX = sourceX;
    sprite.sourceY = sourceY;
    sprite.sourceWidth = sourceWidth;
    sprite.sourceHeight = sourceHeight;
    sprite.x = x;
    sprite.y = y;
    sprite.width = width;
    sprite.height = height;
    sprite.colour = tint;
    return sprite;
}

void resetHealthTransitions(void)
{
    for(int side = 0; side < BATTLE_HUD_SIDE_COUNT; ++side)
        for(int member = 0; member < BATTLE_HUD_TEAM_MEMBER_COUNT; ++member)
            g_battleHud.health[side][member] = HealthTransition();
}

float timeSteps(float battleTime, int nominalRate,
                float* previous, bool* initialized,
                bool* discontinuity)
{
    *discontinuity = false;
    if(!*initialized)
    {
        *initialized = true;
        *previous = battleTime;
        return 1.0f;
    }
    const float delta = battleTime - *previous;
    *previous = battleTime;
    if(delta == 0.0f)
        return 0.0f;
    if(delta < 0.0f)
    {
        *discontinuity = true;
        return 1.0f;
    }
    float steps = delta * static_cast<float>(
        nominalRate > 0 ? nominalRate : 50);
    if(steps > 4.0f)
    {
        *discontinuity = true;
        steps = 4.0f;
    }
    return steps;
}

void updateHealthTransition(HealthTransition& transition,
                            const GameSlotRecord& slot, float steps)
{
    int current = slot.currentHealth;
    if(current < 0)
        current = 0;
    if(!transition.initialized || transition.maximum != slot.maxHealth ||
       transition.characterId != slot.characterId)
    {
        transition.maximum = slot.maxHealth;
        transition.current = current;
        transition.delayed = current;
        transition.pendingDamage = 0;
        transition.characterId = slot.characterId;
        transition.frameAccumulator = 0.0f;
        transition.initialized = true;
        return;
    }
    if(current != transition.current)
    {
        transition.pendingDamage += transition.current - current;
        transition.current = current;
    }
    transition.frameAccumulator += steps;
    int updates = static_cast<int>(transition.frameAccumulator);
    transition.frameAccumulator -= static_cast<float>(updates);
    while(updates-- > 0)
    {

        if(transition.pendingDamage < 1)
        {
            transition.delayed = transition.current;
            continue;
        }
        int step = (transition.delayed - transition.current) / 0x3c;
        if(step == 0)
            step = 1;
        transition.pendingDamage -= step;
        transition.delayed -= step;
    }
}

void advanceLowHealthPulse(float steps)
{
    g_battleHud.lowHealthFrameAccumulator += steps;
    int updates = static_cast<int>(g_battleHud.lowHealthFrameAccumulator);
    g_battleHud.lowHealthFrameAccumulator -= static_cast<float>(updates);
    while(updates-- > 0)
    {
        for(int side = 0; side < BATTLE_HUD_SIDE_COUNT; ++side)
        {
            if(!g_battleHud.lowHealthDescending)
            {
                ++g_battleHud.lowHealthPhase;
                if(g_battleHud.lowHealthPhase >= 7)
                {
                    g_battleHud.lowHealthPhase = 7;
                    g_battleHud.lowHealthDescending = true;
                }
            }
            else
            {
                --g_battleHud.lowHealthPhase;
                if(g_battleHud.lowHealthPhase < 0)
                {
                    g_battleHud.lowHealthPhase = 0;
                    g_battleHud.lowHealthDescending = false;
                }
            }
            g_battleHud.lowHealthSidePhase[side] =
                g_battleHud.lowHealthPhase;
        }
    }
}

bool alternateOpponentColour(const GameSlotRecord& slot)
{
    const CharacterRecord* record = characterRecordForId(
        static_cast<uint16_t>(slot.characterId));
    return record != NULL && record->encounterMode != 0;
}

void drawHealthMember(int side, int member,
                      const GameSlotRecord& slot,
                      const HealthTransition& transition)
{
    BattleHudHealthQuad quads[HEALTH_LAYER_COUNT * HEALTH_SEGMENT_COUNT];
    const int count = battleHudBuildHealthAssembly(
        side, member, transition.maximum, transition.delayed,
        transition.current, alternateOpponentColour(slot),
        g_battleHud.lowHealthSidePhase[side],
        quads, static_cast<int>(sizeof(quads) / sizeof(quads[0])));
    for(int index = 0; index < count; ++index)
    {
        const BattleHudHealthQuad& quad = quads[index];
        HudSprite sprite = makeSprite(
            battleHudTexture(quad.texture),
            quad.atlasWidth, quad.atlasHeight,
            quad.sourceX, quad.sourceY,
            quad.sourceWidth, quad.sourceHeight,
            quad.x, quad.y, quad.width, quad.height, quad.colour);
        sprite.flipX = quad.flipX;
        drawHudSprite(sprite);
    }
}

const char* fallbackCharacterName(int16_t characterId)
{
    switch(characterId)
    {
    case CHARACTER_RAVEN: return "Raven";
    case CHARACTER_AARON: return "Aaron";
    case CHARACTER_LOLA: return "Lola";
    case CHARACTER_GINA: return "Gina";
    case CHARACTER_JASON_G: return "Jason G";
    case CHARACTER_MOBSTER: return "Mobster";
    default: return "";
    }
}

void drawNamePlate(const Game& game, int side, int member,
                   const GameSlotRecord& slot)
{
    const bool alternate = alternateOpponentColour(slot);
    const BattleHudPlateGeometry geometry =
        battleHudPlateGeometry(side, member, alternate);
    const char* text = game.localizedText(
        battleHudDisplayNameTextId(slot.characterId));
    if(text == NULL)
        text = fallbackCharacterName(slot.characterId);
    if(text == NULL || *text == '\0')
        return;

    float x = geometry.x;
    if(geometry.rightAligned)
        x -= battleHudTextWidth(text);
    const unsigned char* cursor =
        reinterpret_cast<const unsigned char*>(text);
    while(*cursor != 0)
    {
        const unsigned int character = *cursor++;
        if(character == '\n')
            break;
        if(character == '\t')
        {
            x += geometry.cellWidth * 4.0f;
            continue;
        }
        if(character == 0x20)
        {
            x += BATTLE_HUD_GLYPH_METRICS[0].advance;
            continue;
        }
        if(character <= 0x20)
            continue;

        const int glyphIndex =
            static_cast<int>(character) - BATTLE_HUD_GLYPH_FIRST;
        const BattleHudGlyphMetrics& metrics =
            BATTLE_HUD_GLYPH_METRICS[glyphIndex];
        const float cellX = static_cast<float>(
            (glyphIndex % BATTLE_HUD_GLYPH_COLUMNS)) * geometry.cellWidth;
        const float cellY = static_cast<float>(
            (glyphIndex / BATTLE_HUD_GLYPH_COLUMNS)) * geometry.cellHeight;
        HudSprite glyph = makeSprite(
            battleHudTexture(BATTLE_HUD_TEXTURE_NAME_FONT),
            geometry.atlasWidth, geometry.atlasHeight,
            cellX + metrics.bearing, cellY,
            static_cast<float>(metrics.advance), geometry.cellHeight,
            x, geometry.y,
            static_cast<float>(metrics.advance), geometry.cellHeight,
            geometry.colour);
        drawHudSprite(glyph);
        x += metrics.advance;
    }
}

}

void
drawBattleHud(const Game& game, uint16_t stageId)
{
    if(game.screen() != GAME_SCREEN_GAMEPLAY)
        return;

    setBattleHudVideoTiming(game.videoTimingMode() != 0);

    const bool battleTexturesLoaded =
        loadBattleHudTextureTable(game.options().language);
    drawHudRadar(game, stageId);

    if(game.options().gaugeDisplay == 0 || !battleTexturesLoaded)
        return;

    const GameSlotRecord* members[BATTLE_HUD_SIDE_COUNT]
                                 [BATTLE_HUD_TEAM_MEMBER_COUNT] = {};
    int memberCounts[BATTLE_HUD_SIDE_COUNT] =
    {
        game.hasAlly() ? 2 : 1,
        game.opponentCount()
    };
    members[0][0] = &game.runtimeState().player;
    if(game.hasAlly())
        members[0][1] = &game.allySlot();
    if(memberCounts[1] > BATTLE_HUD_TEAM_MEMBER_COUNT)
        memberCounts[1] = BATTLE_HUD_TEAM_MEMBER_COUNT;
    for(int member = 0; member < memberCounts[1]; ++member)
        members[1][member] = &game.opponent(member);

    bool healthDiscontinuity = false;
    const float healthSteps = timeSteps(
        game.battleTime(), game.nominalFrameRateHz(),
        &g_battleHud.healthLastTime, &g_battleHud.healthTimeInitialized,
        &healthDiscontinuity);
    if(healthDiscontinuity)
        resetHealthTransitions();
    advanceLowHealthPulse(healthSteps);
    for(int side = 0; side < BATTLE_HUD_SIDE_COUNT; ++side)
    {
        for(int member = 0; member < memberCounts[side]; ++member)
            updateHealthTransition(
                g_battleHud.health[side][member],
                *members[side][member], healthSteps);
    }

    for(int side = 0; side < BATTLE_HUD_SIDE_COUNT; ++side)
    {
        if(!sceneHudSideVisible(side))
            continue;
        for(int member = 0; member < memberCounts[side]; ++member)
            drawHealthMember(side, member, *members[side][member],
                             g_battleHud.health[side][member]);

        for(int member = 0; member < memberCounts[side]; ++member)
            drawNamePlate(game, side, member, *members[side][member]);
    }
}

#endif

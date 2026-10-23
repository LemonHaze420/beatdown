#ifndef BATTLE_HUD_DATA_H
#define BATTLE_HUD_DATA_H
#include <stdint.h>
const int BATTLE_HUD_GLYPH_FIRST = 0x20;
const int BATTLE_HUD_GLYPH_COUNT = 224;
const int BATTLE_HUD_GLYPH_COLUMNS = 16;
const float BATTLE_HUD_GLYPH_CELL_WIDTH = 22.0f;
const float BATTLE_HUD_GLYPH_CELL_HEIGHT = 30.0f;
const float BATTLE_HUD_FONT_ATLAS_WIDTH = 352.0f;
const float BATTLE_HUD_FONT_ATLAS_HEIGHT = 420.0f;
const uint32_t BATTLE_HUD_FONT_ATLAS_ENTRY = 0x166;
const int BATTLE_HUD_PLATE_COUNT = 6;
const int BATTLE_HUD_RADAR_STAGE_COUNT = 54;
const int BATTLE_HUD_RADAR_DESTINATION_COUNT = 23;
struct BattleHudGlyphMetrics
{
    int16_t bearing;
    int16_t advance;
};
struct BattleHudDataPoint
{
    float x;
    float y;
};
extern const BattleHudGlyphMetrics
    BATTLE_HUD_GLYPH_METRICS[BATTLE_HUD_GLYPH_COUNT];
extern const BattleHudDataPoint
    BATTLE_HUD_PLATE_ANCHORS[BATTLE_HUD_PLATE_COUNT];
extern const BattleHudDataPoint
    BATTLE_HUD_RADAR_MAP_OFFSETS[BATTLE_HUD_RADAR_STAGE_COUNT];
extern const BattleHudDataPoint
    BATTLE_HUD_RADAR_STAGE_ORIGINS[BATTLE_HUD_RADAR_STAGE_COUNT];
extern const BattleHudDataPoint
    BATTLE_HUD_RADAR_DESTINATIONS[BATTLE_HUD_RADAR_DESTINATION_COUNT];
#endif

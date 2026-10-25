#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H

#include "game.h"
#include "game_equipped_moves.h"
#include "character_records.h"
#include "move_tables.h"
#include <math.h>

namespace game_detail
{

inline constexpr uint8_t NORMAL_VERSUS_TRAINING_LEVELS[GAME_SAVE_TRAINING_LEVEL_COUNT] =
    { 10, 10, 10, 10, 10, 10, 10, 10 };

inline constexpr int16_t COMBAT_ARMED_STANCE_MOTIONS[24] =
{
    329, 360, 360, 361, 363, 364, 366, 0, 0, 0, 356, 329,
    368, 329, 329, 367, 0, 0, 340, 340, 340, 97, 342, 343
};

inline const CharacterId PLAYABLE_CHARACTER_IDS[5] =
{
    CHARACTER_RAVEN,
    CHARACTER_LOLA,
    CHARACTER_JASON_G,
    CHARACTER_GINA,
    CHARACTER_AARON
};

inline const uint16_t INTRO_CARD_TEXT_IDS[] =
{
    10201,
    GAME_TEXT_TITLE_LAS_SOMBRAS,
    GAME_TEXT_TITLE_WAR_ZONE
};

inline const uint8_t INTRO_CARD_RECORD_COUNT =
    sizeof(INTRO_CARD_TEXT_IDS) / sizeof(INTRO_CARD_TEXT_IDS[0]);

inline const uint8_t INTRO_CARD_TEXT_FADE_STEP = 0x10;

inline const uint8_t INTRO_CARD_TEXT_FADE_THRESHOLD = 0xc0;

inline const uint8_t INTRO_CARD_MASK_FADE_STEP = 6;

inline const float INTRO_CARD_DESCRIPTOR_SECONDS = 3.0f;

inline const float INTRO_CARD_FADE_ALLOWANCE_SECONDS = 0.4f;

inline const uint8_t DAMAGE_CHANNEL_WEIGHTS[8][4] =
{
    { 0, 1, 3, 6 },
    { 0, 1, 3, 6 },
    { 0, 1, 3, 6 },
    { 0, 1, 2, 3 },
    { 0, 1, 2, 3 },
    { 0, 1, 2, 3 },
    { 0, 1, 2, 3 },
    { 0, 1, 2, 3 }
};

inline const uint8_t COMBAT_PRIMARY_EFFECT_CLASS[48] =
{
    0, 0, 0, 1, 1, 1, 1, 1, 1, 7, 2, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 4, 7, 5, 5,
    5, 7, 5, 3, 7, 1, 2, 7, 6, 6, 6, 6,
    6, 7, 6, 6, 6, 7, 0, 0, 1, 6, 0, 0
};

inline const uint8_t COMBAT_SECONDARY_EFFECT_CLASS[48] =
{
    1, 1, 1, 4, 4, 4, 4, 4, 4, 1, 4, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
    1, 1, 1, 3, 1, 4, 4, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 4, 1, 0, 0
};

inline const uint8_t COMBAT_PRIMARY_EFFECT_RECORDS[8][2] =
{
    { 0xff, 0xff }, { 0x4d, 0x4e }, { 0x4c, 0x4e }, { 0x0d, 0xff },
    { 0xff, 0xff }, { 0xff, 0xff }, { 0xff, 0x4e }, { 0xff, 0xff }
};

inline const uint8_t COMBAT_SECONDARY_EFFECT_RECORDS[5][4] =
{
    { 0xff, 0xff, 0xff, 0xff },
    { 0x2c, 0x4f, 0x66, 0xff },
    { 0x2c, 0x4f, 0x66, 0xff },
    { 0x2c, 0x0d, 0x0e, 0xff },
    { 0x2c, 0x4f, 0x66, 0x4d }
};

inline const float PLAYER_AXIS_OUTER_RADIUS =
    (127.5f - sqrtf(0.5f * 0.5f + 0.5f * 0.5f)) / 127.5f;

inline const float PLAYER_AXIS_DEAD_ZONE = PLAYER_AXIS_OUTER_RADIUS * 0.5f;

inline const float PLAYER_RUN_THRESHOLD = 0.998f;

inline const float PLAYER_RUN_SPEED = 54.0f;

inline const float LOCOMOTION_FALLBACK_SPEED = 11.5f;

inline const float LOCOMOTION_FRAMES_PER_SECOND = 60.0f;

struct VersusBattleAnchorRow
{
    int16_t xTenths;
    int16_t yTenths;
    int16_t zTenths;
    int16_t headingTenths;
};

inline const int VERSUS_BATTLE_ANCHOR_COUNT = 56;

inline const VersusBattleAnchorRow VERSUS_BATTLE_ANCHORS[
    VERSUS_BATTLE_ANCHOR_COUNT] =
{
    {     0,  0,     0,      0},
    { -3050,  0,   960,  -1800},
    { -3050,  0,   960,  -1800},
    { -3050,  0,   960,  -1800},
    {  -125,  0,     0,    900},
    {   -12,  0,   650,    900},
    {     0,  0,     0,      0},
    {  1300,  0,  -600,   -900},
    {  -153,  0,    12,    950},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {  -125,  0,     0,    900},
    {   800,  0,   -95,   -275},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {   200,  0,   875,   1800},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {  -910,  0, -1790,    900},
    {     0,  0,     0,      0},
    {  -218,  0,   400,    900},
    {     0,  0,     0,      0},
    {  -460,  0,   250,  -1450},
    {     0,  0,     0,      0},
    {  2635,  0,    60,      0},
    {     0,  0,     0,      0},
    {  2635,  0,   250,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {  -130,  0,   290,   1400},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {  -525,  0,  -675,   1200},
    {     0,  0,     0,      0},
    {     0,  0,     0,      0},
    {    45,  0,   100,  -1450},
    {     0,  0,     0,      0}
};

inline const int MOVE_RECORD_DEFAULT_ACTION = 1;

struct CombatSphereDescriptor
{
    int8_t bone;
    float x;
    float y;
    float z;
    float radius;
};

inline const CombatSphereDescriptor COMBAT_ATTACK_SPHERES[12] =
{
    { 14, 0.7f, 0.0f, 0.0f, 0.8f }, { 19, 0.7f, 0.0f, 0.0f, 0.8f },
    { 4, 0.0f, 0.0f, 0.0f, 2.0f }, { 7, 0.0f, 0.0f, 0.0f, 2.0f },
    { 13, 0.4f, 0.2f, 0.1f, 0.9f }, { 18, 0.4f, 0.2f, 0.1f, 0.9f },
    { 3, 0.5f, 0.0f, 0.0f, 2.0f }, { 6, 0.5f, 0.0f, 0.0f, 2.0f },
    { 11, 1.8f, 0.1f, 0.2f, 1.1f }, { 16, 1.8f, 0.1f, 0.2f, 1.1f },
    { 10, 1.0f, -0.3f, 0.0f, 1.2f }, { 1, 0.5f, -0.3f, 0.0f, 2.2f }
};

inline const CombatSphereDescriptor COMBAT_BODY_SPHERES[18] =
{
    { 12, 1.2f, 0.2f, 0.0f, 0.9f }, { 13, 0.4f, 0.2f, 0.1f, 0.9f },
    { 14, 0.7f, 0.0f, 0.0f, 0.7f }, { 15, -0.6f, 0.1f, 0.0f, 0.7f },
    { 17, 1.2f, 0.2f, 0.0f, 0.9f }, { 18, 0.4f, 0.2f, 0.1f, 0.9f },
    { 19, 0.7f, 0.0f, 0.0f, 0.7f }, { 20, -0.6f, 0.1f, 0.0f, 0.7f },
    { 2, 1.5f, 0.0f, 0.0f, 2.0f }, { 3, 0.5f, 0.0f, 0.0f, 2.0f },
    { 4, 0.0f, 0.0f, 0.0f, 2.0f }, { 5, 1.5f, 0.0f, 0.0f, 2.0f },
    { 6, 0.5f, 0.0f, 0.0f, 2.0f }, { 7, 0.0f, 0.0f, 0.0f, 2.0f },
    { 10, 1.0f, -0.3f, 0.0f, 1.2f }, { 9, 1.7f, -0.1f, 0.0f, 2.2f },
    { 1, 0.5f, -0.3f, 0.0f, 2.2f }, { 8, 1.7f, 0.3f, 0.0f, 1.8f }
};

inline const uint8_t PRIDE_DAMAGE_PERCENT[37] =
{
    0, 120, 120, 100, 100, 100, 90, 90, 150, 100,
    100, 100, 100, 100, 100, 100, 100, 120, 100, 100,
    120, 100, 100, 100, 200, 120, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100
};

inline const int GAME_BATTLE_RESULT_RELATIONMOVES[5] =
    {
        1717, 1921, 1923, 1925, 1919
    };

inline const int GAME_BATTLE_RESULT_MOVES[16] =
    {
        0, 0, 1455, 1457, 1459, 1526, 1685, 1714,
        1711, 1582, 1700, 1703, 1584, 1707, 1717, 1722
    };

inline const uint8_t GAME_BATTLE_RESULT_TERMINAL_CODES[16] =
    {
        0, 0, 0, 0, 0, 0, 0, 3,
        1, 2, 0, 0, 0, 0, 0, 0
    };

inline const uint8_t GAME_COMBAT_CUES[6] = { 0, 0xff, 9, 10, 11, 12 };

inline const uint8_t GAME_COMBAT_DAMAGE_REACTION_SETS[6] = { 0, 1, 1, 1, 0, 1 };

inline const uint8_t GAME_COMBAT_DAMAGE_BASE_RECORDS[3] = { 0x06, 0x48, 0x13 };

inline const int16_t GAME_COMBAT_STATE_HEALTH_TIER_MOTIONS[2][4] =
        {
            { 22, 22, 22, 670 },
            { 21, 21, 21, 21 },
        };

inline const uint16_t GAME_FRONTEND_TEXT_IDS[5] =
    {
        GAME_TEXT_CHARACTER_RAVEN,
        GAME_TEXT_CHARACTER_LOLA,
        GAME_TEXT_CHARACTER_JASON_G,
        GAME_TEXT_CHARACTER_GINA,
        GAME_TEXT_CHARACTER_AARON
    };

inline const uint8_t GAME_FRONTEND_NEXT[5] = { 2, 3, 4, 0, 1 };

inline const uint8_t GAME_FRONTEND_PREVIOUS[5] = { 3, 4, 0, 1, 2 };

inline const uint8_t GAME_FRONTEND_MASKS[5] =
    {
        0x05,
        0x06,
        0x07,
        0x04,
        0x01
    };

float clampNormalizedAxis(float value);
bool normalizeStick(float rawX, float rawZ, float& x, float& z,
                    float& magnitude);

void setLocomotionMove(GameSlotRecord& slot, int moveId);
int16_t defaultCharacterHealth(CharacterId characterId);
uint16_t characterDatabaseIndex(CharacterId characterId);
bool validVersusCharacter(uint16_t characterId);
bool validPlayableCharacter(uint16_t characterId);
int versusStageIndexForId(uint16_t stageId);
float versusOpponentHeading(float playerHeading);
void applyVersusBattleFormation(uint16_t stageId,
                                GameSlotRecord& player,
                                GameSlotRecord& opponent);
Game& gameFromTask(UpdateTask* task);
int wrapIndex(int value, int count);
bool actionRowEnabled(uint8_t actionClass, int row);
float horizontalDistanceSquared(const GameSlotRecord& first,
                                const GameSlotRecord& second);
void faceSlot(GameSlotRecord& slot, const GameSlotRecord& target);
float signedHeadingDelta(const GameSlotRecord& actor,
                         const GameSlotRecord& target);
std::int16_t combatSelfDirectionClass(const GameSlotRecord& actor,
                                      const GameSlotRecord& target);
std::int16_t combatTargetAttackClass(const GameSlotRecord& target);
int actorMotionIdFromAction(const GameSlotRecord& actor, int actionIndex);
int resolveQueuedMotion(const GameSlotRecord& actor, int rawMotionId);
void initializeGameSlot(GameSlotRecord& slot, CharacterId characterId, uint8_t slotIndex);
std::uint32_t drawCombatAiRandom(void* context);
std::int16_t firstSetCombatState(std::uint32_t flags);
std::int16_t firstSetTargetDirection(std::uint32_t flags);
Vec3 transformCombatPoint(const CombatBoneTransform& transform,
                          float x, float y, float z);
float normalizeCombatDegrees(float value);

inline float normalizeActorDegrees(float value)
{
    while(value > 180.0f) value -= 360.0f;
    while(value < -180.0f) value += 360.0f;
    return value;
}
int optionValueFrom(const GameOptions& options, OptionStorage storage);
void setOptionValueIn(GameOptions& options, OptionStorage storage, int value);
OptionAction openerAction(GameOptionsPage page);
GameOptionsPage actionPage(OptionAction action);

}

#endif

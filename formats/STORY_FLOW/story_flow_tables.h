#ifndef STORY_FLOW_TABLES_H
#define STORY_FLOW_TABLES_H
#include <stdint.h>
struct StoryFlowMessageRow
{
    uint16_t stageId;
    uint16_t messageId;
};
struct StoryFlowPostBattleRow
{
    uint16_t characterId;
    uint16_t targetListIndex;
    uint16_t primaryFlag;
    uint16_t resultFlag;
};
struct StoryFlowArrivalVector
{
    float x;
    float y;
    float z;
};

static const StoryFlowMessageRow STORY_FLOW_MESSAGES[] = {
    { 0, 0 },
    { 13, 73 },
    { 25, 106 },
    { 49, 77 },
    { 26, 63 },
    { 4, 0 },
    { 5, 74 },
    { 14, 75 },
    { 23, 13 },
    { 45, 57 },
    { 37, 76 },
    { 33, 60 },
    { 48, 78 },
    { 19, 110 },
    { 1, 288 },
    { 0, 0 },
    { 0, 0 },
};
static const int STORY_FLOW_MESSAGE_COUNT = 17;

static const StoryFlowPostBattleRow STORY_FLOW_CHAPTER_3_ROWS[] = {
    { 1, 0, 0x0415, 0x06f0 },
    { 7, 0, 0x0433, 0x06f1 },
    { 8, 0, 0x0410, 0x06f2 },
    { 27, 1, 0x0435, 0x06f3 },
    { 38, 1, 0x042e, 0x06f4 },
    { 44, 2, 0x041a, 0x06f5 },
    { 54, 3, 0x041f, 0x06f6 },
    { 62, 0, 0x0429, 0x06f7 },
    { 64, 1, 0x0438, 0x06f8 },
    { 69, 1, 0x0439, 0x06f9 },
    { 79, 2, 0x043a, 0x06fa },
    { 80, 4, 0x042a, 0x06fb },
    { 141, 5, 0x0424, 0x06fc },
};
static const StoryFlowPostBattleRow STORY_FLOW_CHAPTER_5_ROWS[] = {
    { 4, 0, 0x0708, 0x0745 },
    { 16, 0, 0x0709, 0x0746 },
    { 23, 0, 0x070a, 0x0747 },
    { 34, 0, 0x070b, 0x0748 },
    { 35, 0, 0x070c, 0x0749 },
    { 43, 1, 0x070d, 0x074a },
    { 66, 1, 0x070e, 0x074b },
    { 70, 1, 0x070f, 0x074c },
    { 71, 1, 0x0710, 0x074d },
    { 147, 0, 0x0711, 0x074e },
    { 182, 0, 0x0712, 0x074f },
    { 196, 0, 0x0713, 0x0750 },
    { 198, 0, 0x0714, 0x0751 },
};
static const StoryFlowPostBattleRow STORY_FLOW_CHAPTER_6_ROWS[] = {
    { 2, 0, 0x071f, 0x0752 },
    { 6, 0, 0x0720, 0x0753 },
    { 10, 0, 0x0721, 0x0754 },
    { 14, 0, 0x0722, 0x0755 },
    { 31, 0, 0x0723, 0x0756 },
    { 40, 0, 0x0724, 0x0757 },
    { 48, 0, 0x0725, 0x0758 },
    { 65, 0, 0x0726, 0x0759 },
    { 139, 0, 0x0727, 0x075a },
    { 148, 0, 0x0728, 0x075b },
    { 181, 0, 0x0729, 0x075c },
    { 184, 0, 0x072a, 0x075d },
    { 188, 0, 0x072b, 0x075e },
};
static const int STORY_FLOW_POST_BATTLE_ROW_COUNT = 13;

static const uint16_t STORY_FLOW_DEFERRED_SCENE_IDS[] = { 1, 2, 3, 5, 6, 8, 9, 10, 13, 18, 20, 21, 22, 23, 24, 25, 27, 29, 31, 34, 35, 38, 39, 40, 41, 49, 50, 51, 54, 55, 56, 59, 62, 66, 99, 69, 70, 71, 72, 73, 74, 75, 77, 78, 79, 84, 80, 81, 82, 83 };
static const int STORY_FLOW_DEFERRED_SCENE_COUNT = 50;

static const uint16_t STORY_FLOW_REGISTER_OVERRIDE_SCRIPT_IDS[] = { 0x0055, 0x0027, 0x0028, 0x0029, 0x0048, 0x0054, 0x0057, 0x0058, 0x0085, 0x006e, 0x006a, 0x0075, 0x006f };
static const int STORY_FLOW_REGISTER_OVERRIDE_SCRIPT_COUNT = 13;

static const StoryFlowArrivalVector STORY_FLOW_ARRIVAL_VECTORS[] = {
    { -15.8f, 10.5f, 11.7f },
    { -14.7f, 10.5f, -26.8f },
    { 40.8f, 10.5f, -13.8f },
    { -83.9f, 10.5f, 75.8f },
    { -40.0f, 10.0f, -549.0f },
    { -20.1f, 10.5f, -5.2f },
    { -10.6f, 10.5f, -29.4f },
    { 170.1f, 10.5f, -50.4f },
    { -75.1f, 10.5f, -188.0f },
    { -243.1f, 10.5f, 87.0f },
    { 60.0f, 19.7f, 1.7f },
    { 41.0f, 10.5f, -28.0f },
    { 45.2f, 10.5f, 79.8f },
};
static const int STORY_FLOW_ARRIVAL_VECTOR_COUNT = 13;
#endif

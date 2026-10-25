#ifndef GAME_BATTLE_END_H
#define GAME_BATTLE_END_H

enum BattleEndState
{
    BATTLE_END_START,
    BATTLE_END_DELAY,
    BATTLE_END_WAIT_NOTICE,
    BATTLE_END_BEFORE_FADE,
    BATTLE_END_WAIT_FADE,
    BATTLE_END_COMPLETE
};
struct BattleEndRuntime
{
    BattleEndState state = BATTLE_END_START;
    float elapsedSeconds = 0;
};
struct BattleEndInput
{
    float scaledDeltaSeconds = 0;
    bool noticeTaskActive = false;
    bool fadeReady = false;
};
struct BattleEndEffects
{
    bool initialize = false;
    bool startNotice = false;
    bool startFade = false;
    bool restore = false;
    bool complete = false;
    bool lockOccupiedActorActions = false;
};

enum BattleEndNoticeState
{
    BATTLE_END_NOTICE_INACTIVE = 0,
    BATTLE_END_NOTICE_INITIALIZE = 1,
    BATTLE_END_NOTICE_FADE_IN = 14,
    BATTLE_END_NOTICE_HOLD = 15,
    BATTLE_END_NOTICE_FADE_OUT = 17
};
struct BattleEndNoticeRuntime
{
    BattleEndNoticeState state = BATTLE_END_NOTICE_INACTIVE;
    float progress = 0;
    float opacity = 0;
};
struct BattleEndNoticeLayout
{
    float centerX;
    float centerY;
    float width;
    float height;
    float u0;
    float v0;
    float u1;
    float v1;
};

const BattleEndNoticeLayout BATTLE_END_NOTICE_LAYOUT =
{
    340.0f, 324.0f, 384.0f, 96.0f, 0.0f, 0.5f, 1.0f, 0.75f
};

BattleEndEffects updateBattleEnd(BattleEndRuntime& runtime,
                                 const BattleEndInput& input);
void startBattleEndNotice(BattleEndNoticeRuntime& runtime);
void updateBattleEndNotice(BattleEndNoticeRuntime& runtime, float scaledSeconds);

#endif

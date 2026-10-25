#ifndef STAGE_AI_H
#define STAGE_AI_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct LgpHeader
{
    std::uint32_t primaryChainWord;
    std::uint32_t alternateChainWord;
    std::uint32_t sectionACount;
    std::uint32_t sectionBCount;
    std::uint32_t sectionDCount;
    std::uint32_t sectionCCount;
    std::int16_t primaryChainIndex;
    std::int16_t alternateChainIndex;
};

struct LgpACommand
{
    std::int16_t control;
    std::int16_t operand;
    std::int16_t kind;
    std::int16_t kindArgument;
    std::array<std::uint8_t, 4> output;
};

struct LgpARecord
{
    std::uint32_t commandCount;
    std::array<LgpACommand, 12> commands;
};

struct LgpBChoice
{
    std::int16_t value;
    std::int16_t weight;
    std::array<std::int16_t, 6> reserved;
};

struct LgpBRecord
{
    std::int16_t selector;
    std::array<std::int16_t, 7> threshold;
    std::array<LgpBChoice, 16> choices;
};

struct LgpCLink
{
    std::uint16_t conditionFlags;
    std::int16_t rhs;
    std::int16_t nextKind;
    std::int16_t nextIndex;
};

struct LgpCRecord
{
    std::int32_t sourceSelector;
    std::array<LgpCLink, 8> links;
};

struct LgpDThresholdLane
{
    std::array<std::int16_t, 8> selectorAndThreshold;
};

struct LgpDRecord
{
    std::int16_t sourceSelector;
    std::int16_t reserved0;
    std::int16_t nextKind;
    std::int16_t nextIndex;
    std::array<LgpDThresholdLane, 2> threshold;

    std::array<std::array<std::int16_t, 7>, 7> transition;
    std::int16_t reserved1;
    std::array<std::int16_t, 7> matchKey;
    std::array<std::int16_t, 7> matchValue;
};

struct LgpStageAiProgram
{
    LgpHeader header;
    std::vector<LgpARecord> sectionA;
    std::vector<LgpBRecord> sectionB;
    std::vector<LgpCRecord> sectionC;
    std::vector<LgpDRecord> sectionD;
};

bool parseLgpStageAi(const std::uint8_t* data, std::size_t size,
                     LgpStageAiProgram* program,
                     std::string* error = nullptr);
bool parseLgpStageAi(const std::vector<std::uint8_t>& data,
                     LgpStageAiProgram* program,
                     std::string* error = nullptr);
bool loadLgpStageAi(const char* archivePath, const char* entryName,
                    LgpStageAiProgram* program,
                    std::string* error = nullptr);
bool loadStageAiProgram(const char* archiveDirectory, std::uint16_t stageId,
                        LgpStageAiProgram* program,
                        std::string* error = nullptr);

struct NpcPositionRow
{
    std::array<std::int16_t, 32> value;
};

struct NpcPositionRecord
{
    std::uint32_t sourceOffset;
    std::array<NpcPositionRow, 7> rows;
    std::array<std::uint8_t, 0x40> reservedTrailer;
};

struct NpcStagePositionTable
{
    std::uint32_t sourceOffset;
    std::array<std::uint32_t, 56> recordOffsets;
    std::array<std::int32_t, 56> recordIndices;
};

struct NpcPositionDat
{
    std::array<std::uint32_t, 128> npcTableOffsets;
    std::array<std::int32_t, 128> tableIndices;
    std::vector<NpcStagePositionTable> tables;
    std::vector<NpcPositionRecord> records;
};

enum NpcPositionRowKind : std::int16_t
{
    NPC_POSITION_BASE = 0,
    NPC_POSITION_CHARACTER = 1,
    NPC_POSITION_CONDITIONAL = 2,
    NPC_POSITION_CONDITIONAL_ALTERNATE = 3,
    NPC_POSITION_CLEAR = 4,
    NPC_POSITION_CLEAR_ALTERNATE = 5
};

struct NpcSpawnTransform
{
    float x;
    float y;
    float z;
    float headingDegrees;
};

bool parseNpcPositionDat(const std::uint8_t* data, std::size_t size,
                         NpcPositionDat* database,
                         std::string* error = nullptr);
bool parseNpcPositionDat(const std::vector<std::uint8_t>& data,
                         NpcPositionDat* database,
                         std::string* error = nullptr);
bool loadNpcPositionDat(const char* archivePath, const char* entryName,
                        NpcPositionDat* database,
                        std::string* error = nullptr);
bool loadStageNpcPositionDat(const char* archiveDirectory,
                             NpcPositionDat* database,
                             std::string* error = nullptr);
const NpcPositionRecord* findNpcPositionRecord(const NpcPositionDat& database,
                                               std::uint16_t npcIndex,
                                               std::uint16_t stageIndex);
const NpcPositionRow* findNpcPositionRow(const NpcPositionDat& database,
                                         std::uint16_t npcIndex,
                                         std::uint16_t stageIndex,
                                         std::uint8_t rowIndex);
bool getNpcSpawnTransform(const NpcPositionRow& row,
                          NpcSpawnTransform* transform);

constexpr std::size_t STAGE_AI_SOURCE_PARAM_COUNT = 22;
constexpr std::size_t STAGE_AI_BIT_COUNT = 64;

using LgpAiRandomDraw = std::uint32_t (*)(void* context);

struct LgpAiRandomSource
{
    LgpAiRandomDraw draw;
    void* context;
};

struct LgpAiVmState
{
    std::array<std::int16_t, STAGE_AI_SOURCE_PARAM_COUNT> sourceParam;
    std::uint64_t bitState;
    LgpAiRandomSource random;
    std::array<float, 4> channels;
    std::int16_t actionId;
    bool failed;
};

void resetLgpAiVmState(LgpAiVmState* state, LgpAiRandomSource random);
std::uint32_t nextLgpAiRandom(LgpAiVmState* state);
bool evaluateLgpAiCondition(LgpAiVmState* state,
                            std::int32_t sourceSelector,
                            std::uint16_t conditionFlags,
                            std::int16_t rhs);
bool selectLgpWeightedAction(const LgpStageAiProgram& program,
                             LgpAiVmState* state,
                             std::int32_t recordIndex,
                             std::int16_t* actionId);
bool resolveLgpAiBranch(const LgpStageAiProgram& program,
                        LgpAiVmState* state,
                        std::int32_t recordIndex,
                        std::int16_t* nextKind,
                        std::int16_t* nextIndex);
bool resolveLgpAiChain(const LgpStageAiProgram& program,
                       LgpAiVmState* state,
                       std::int32_t recordIndex,
                       std::int16_t* actionId);

struct LgpAiApplyResult
{
    bool advanced;
    bool emitted;
    std::int16_t actionId;
    std::array<float, 4> channels;
};

bool applyLgpAiRecord(const LgpARecord& record,
                      LgpAiVmState* state,
                      std::uint16_t* commandIndex,
                      std::uint16_t* waitTicks,
                      LgpAiApplyResult* result);

struct StageAiVec3
{
    float x;
    float y;
    float z;
};

constexpr std::size_t STAGE_ACTOR_ROW_COUNT = 7;

struct StageActorDescriptor
{
    std::uint8_t rowIndex;
    NpcPositionRowKind kind;
    std::int16_t selectorId;
    std::int16_t defaultMotionId;
    std::int16_t alternateMotionId;
    std::int16_t interactionSelector;
    NpcSpawnTransform transform;
    bool actorPreload;
    bool resourcePreload;
};

struct StageActorPreloadPlan
{
    std::array<StageActorDescriptor, STAGE_ACTOR_ROW_COUNT> rows;
    std::uint8_t actorCount;
    std::uint8_t resourceCount;
};

enum NpcPositionOverlayDecision : std::uint8_t
{
    NPC_POSITION_OVERLAY_KEEP,
    NPC_POSITION_OVERLAY_COPY,
    NPC_POSITION_OVERLAY_CLEAR
};

using NpcPositionOverlayFilter = NpcPositionOverlayDecision (*)(
    const NpcPositionRow& row, std::uint8_t rowIndex,
    bool battleLayer, void* context);

void clearNpcPositionRows(
    std::array<NpcPositionRow, STAGE_ACTOR_ROW_COUNT>* rows);
bool applyNpcPositionOverlay(
    const NpcPositionRecord& source, bool baseLayer, bool battleLayer,
    NpcPositionOverlayFilter filter, void* context,
    std::array<NpcPositionRow, STAGE_ACTOR_ROW_COUNT>* rows,
    std::string* error = nullptr);
bool buildStageActorPreload(
    const NpcPositionRecord& finalRecord,
    StageActorPreloadPlan* plan,
    std::string* error = nullptr);

enum CombatAiRole : std::uint8_t
{
    COMBAT_AI_PRIMARY_OPPONENT,
    COMBAT_AI_AMBIENT_OPPONENT,
    COMBAT_AI_PARTY_PARTNER
};

enum CombatAiAction : std::uint8_t
{
    COMBAT_AI_ACTION_NONE,
    COMBAT_AI_ACTION_ATTACK_0,
    COMBAT_AI_ACTION_ATTACK_1,
    COMBAT_AI_ACTION_GRAB,
    COMBAT_AI_ACTION_GUARD,
    COMBAT_AI_ACTION_EVADE,
    COMBAT_AI_ACTION_UNMAPPED
};

constexpr std::uint16_t AI_DIRECTION_AWAY = 0x0001;
constexpr std::uint16_t AI_DIRECTION_TOWARD = 0x0002;
constexpr std::uint16_t AI_DIRECTION_LEFT = 0x0004;
constexpr std::uint16_t AI_DIRECTION_RIGHT = 0x0008;
constexpr std::uint16_t AI_INPUT_GRAB = 0x0040;
constexpr std::uint16_t AI_INPUT_ATTACK_0 = 0x0080;
constexpr std::uint16_t AI_INPUT_GUARD = 0x0100;
constexpr std::uint16_t AI_INPUT_ATTACK_1 = 0x0200;
constexpr std::uint16_t AI_INPUT_UNMAPPED_0 = 0x0800;
constexpr std::uint16_t AI_INPUT_EVADE_0 = 0x1000;
constexpr std::uint16_t AI_INPUT_UNMAPPED_1 = 0x2000;
constexpr std::uint16_t AI_INPUT_RUN_TO_TARGET = 0x4000;
constexpr std::uint16_t AI_INPUT_EVADE_1 = 0x8000;

struct CombatAiUpdateInput
{
    std::uint8_t slotIndex;
    std::uint8_t difficulty;
    std::int16_t selfState;
    std::int16_t targetState;
    std::int16_t selfHealth;
    std::int16_t selfMaxHealth;
    std::int16_t selfDamageGauge;
    std::int16_t selfDamageGaugeMaximum;
    std::int16_t targetHealth;
    std::int16_t targetMaxHealth;
    std::int16_t targetAttackClass;
    std::int16_t targetDirection;
    std::int16_t selfDirection;
    std::uint64_t conditionBits;
    StageAiVec3 selfPosition;
    StageAiVec3 targetPosition;
};

struct CombatAiDecision
{
    CombatAiAction action;
    std::int16_t selectedRecord;
    std::uint16_t logicalInput;
    std::array<float, 4> channels;
    StageAiVec3 movementDirection;
    float movementAngleDegrees;
    bool moving;
    bool running;
    bool verticalSeparation;
    bool exactMoveUnavailable;
};

struct CombatAiRuntime
{
    LgpStageAiProgram program;
    LgpAiVmState vm;
    std::uint8_t profileId;
    bool initialized;
};

constexpr std::size_t STAGE_AI_CONTEXT_COUNT = 16;
constexpr std::uint16_t STAGE_AI_FSTAGE_RESOURCE_BASE = 0x39;

enum StageAiContextState : std::uint8_t
{
    STAGE_AI_CONTEXT_EMPTY,
    STAGE_AI_CONTEXT_REQUESTED,
    STAGE_AI_CONTEXT_READY
};

struct StageAiAsyncContext
{
    CombatAiRuntime runtime;
    StageAiContextState state;
    std::uint8_t playerIndex;
    std::uint8_t sourceSlotIndex;
    std::uint8_t profileId;
    std::uint16_t resourceId;
    std::int16_t decisionValue;
    std::uint32_t lifetime;
    bool parameterZero;
};

struct StageAiTickGate
{
    bool occupied;
    bool battleActive;
    bool scaledDeltaNonzero;
    bool cpuControlled;
    bool specialInputOverride;
};

struct CombatAiProfileSelection
{
    std::uint16_t characterId;
    std::uint16_t persistentProfileId;
    CombatAiRole role;
    std::uint8_t relationshipState;
    std::uint8_t profileCategory;
    std::uint8_t battleMode;
    bool activeRoster;
    bool forceProfile44Matchup;
};

std::uint8_t combatAiProfileForCharacter(std::uint16_t characterId,
                                         CombatAiRole role,
                                         std::uint8_t relationshipState = 0);
std::uint8_t combatAiProfileCategory(std::uint16_t characterId);
bool combatAiCategoryExcluded(std::uint8_t profileCategory);
std::uint8_t resolveCombatAiProfile(
    const CombatAiProfileSelection& selection);
std::int16_t combatAiDifficultyDecisionValue(std::uint8_t difficulty);
float combatAiDifficultyAttackMultiplier(std::uint8_t difficulty);

bool loadCombatAiProgram(const char* archiveDirectory,
                         std::uint8_t profileId,
                         LgpStageAiProgram* program,
                         std::string* error = nullptr);
bool initializeCombatAiRuntime(CombatAiRuntime* runtime,
                               const char* archiveDirectory,
                               std::uint8_t profileId,
                               LgpAiRandomSource random,
                               std::string* error = nullptr);
void resetCombatAiRuntime(CombatAiRuntime* runtime,
                          LgpAiRandomSource random);
bool updateCombatAiRuntime(CombatAiRuntime* runtime,
                           const CombatAiUpdateInput& input,
                           CombatAiDecision* decision,
                           std::string* error = nullptr);
void releaseStageAiContext(StageAiAsyncContext* context);
bool requestStageAiContext(StageAiAsyncContext* context,
                           std::uint16_t profileId,
                           std::uint8_t playerIndex,
                           std::uint8_t sourceSlotIndex,
                           bool parameterZero,
                           std::uint8_t difficulty,
                           LgpAiRandomSource random);
bool completeStageAiContext(StageAiAsyncContext* context,
                            const LgpStageAiProgram& program,
                            LgpAiRandomSource random,
                            std::string* error = nullptr);
bool completeStageAiContextLoad(StageAiAsyncContext* context,
                                const char* archiveDirectory,
                                LgpAiRandomSource random,
                                std::string* error = nullptr);
bool updateStageAiContext(StageAiAsyncContext* context,
                          const StageAiTickGate& gate,
                          const CombatAiUpdateInput& input,
                          CombatAiDecision* decision,
                          std::string* error = nullptr);
bool stageAiDecisionIsNeutral(const CombatAiDecision& decision);

#endif

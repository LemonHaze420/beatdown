#include "stage/ai.h"

#include "xaf_runtime.h"
#include "formats/COMBAT_AI/combat_ai_tables.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <limits>
#include <utility>

namespace
{

constexpr std::size_t LGP_HEADER_SIZE = 0x18;
constexpr std::size_t LGP_A_SIZE = 0x94;
constexpr std::size_t LGP_B_SIZE = 0x110;
constexpr std::size_t LGP_C_SIZE = 0x44;
constexpr std::size_t LGP_D_SIZE = 0xA8;
constexpr std::size_t NPC_ROOT_SIZE = 128 * sizeof(std::uint32_t);
constexpr std::size_t NPC_TABLE_SIZE = 56 * sizeof(std::uint32_t);
constexpr std::size_t NPC_RECORD_SIZE = 0x200;
constexpr std::size_t NPC_ROW_COUNT = 7;
constexpr std::size_t NPC_ROW_WIDTH = 32;
constexpr std::size_t MAX_CHAIN_STEPS = 4096;

struct Span
{
    std::uint32_t begin;
    std::uint32_t end;
};

void setError(std::string* error, const char* message)
{
    if(error != NULL)
        *error = message != NULL ? message : "stage AI parse failure";
}

bool fail(std::string* error, const char* message)
{
    setError(error, message);
    return false;
}

bool failFormatted(std::string* error, const char* prefix,
                   std::uint32_t value)
{
    if(error != NULL)
    {
        char message[128];
        std::snprintf(message, sizeof(message), "%s 0x%08x", prefix, value);
        *error = message;
    }
    return false;
}

std::uint16_t readU16(const std::uint8_t* data, std::size_t offset)
{
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(data[offset + 1]) << 8;
}

std::int16_t readS16(const std::uint8_t* data, std::size_t offset)
{
    return static_cast<std::int16_t>(readU16(data, offset));
}

std::uint32_t readU32(const std::uint8_t* data, std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
           static_cast<std::uint32_t>(data[offset + 1]) << 8 |
           static_cast<std::uint32_t>(data[offset + 2]) << 16 |
           static_cast<std::uint32_t>(data[offset + 3]) << 24;
}

std::int32_t readS32(const std::uint8_t* data, std::size_t offset)
{
    return static_cast<std::int32_t>(readU32(data, offset));
}

bool hasBytes(std::size_t size, std::size_t offset, std::size_t count)
{
    return offset <= size && count <= size - offset;
}

bool validSourceSelector(std::int32_t selector)
{
    return selector >= 0 &&
           static_cast<std::uint32_t>(selector) < STAGE_AI_SOURCE_PARAM_COUNT;
}

bool validConditionFlags(std::uint16_t flags)
{
    const std::uint16_t comparator = flags & 0x0f;
    if(comparator > 9 || (flags & 0xffc0) != 0)
        return false;
    if((flags & 0x30) == 0x30)
        return false;
    return true;
}

bool validConditionOperand(std::uint16_t flags, std::int16_t rhs)
{
    if(!validConditionFlags(flags))
        return false;
    if((flags & 0x20) != 0)
        return rhs >= 0 && static_cast<std::uint16_t>(rhs) < STAGE_AI_BIT_COUNT;
    if((flags & 0x10) != 0)
    {
        return rhs == -1 || validSourceSelector(rhs);
    }
    return true;
}

bool validChainReference(std::int16_t kind, std::int16_t index,
                         std::size_t sectionBCount,
                         std::size_t sectionCCount,
                         std::size_t sectionDCount)
{
    switch(kind)
    {
    case 0:
        return index >= 0 && static_cast<std::size_t>(index) < sectionCCount;
    case 1:
        return index >= 0 && static_cast<std::size_t>(index) < sectionBCount;
    case 2:
        return index >= -1;
    case 3:
        return index >= 0 && static_cast<std::size_t>(index) < sectionDCount;
    default:
        return false;
    }
}

bool validateLgpACommand(const LgpACommand& command, std::string* error)
{
    if(command.kind < 0 || command.kind > 4)
        return fail(error, "LGP A command has an unknown kind");

    switch(command.kind)
    {
    case 0:
        if(command.control < 0)
            return fail(error, "LGP A timing control is negative");
        break;
    case 1:
    {
        const std::uint16_t flags = static_cast<std::uint16_t>(command.control);
        if(!validConditionOperand(flags, command.operand) ||
           !validSourceSelector(command.kindArgument))
            return fail(error, "LGP A condition has an invalid selector or operand");
        break;
    }
    case 2:
        if(command.control < 0 ||
           static_cast<std::uint16_t>(command.control) >= STAGE_AI_BIT_COUNT ||
           (command.operand != 0 && command.operand != 1))
            return fail(error, "LGP A bit test is outside the runtime bit set");
        break;
    case 3:
        if(!validSourceSelector(command.control))
            return fail(error, "LGP A assignment selector is outside sourceParam");
        break;
    case 4:
        if(command.control < 0 ||
           static_cast<std::uint16_t>(command.control) >= STAGE_AI_BIT_COUNT)
            return fail(error, "LGP A bit assignment is outside the runtime bit set");
        break;
    default:
        break;
    }
    return true;
}

bool validateSpans(std::vector<Span>& spans, std::string* error)
{
    std::sort(spans.begin(), spans.end(), [](const Span& left, const Span& right)
    {
        if(left.begin != right.begin)
            return left.begin < right.begin;
        return left.end < right.end;
    });

    for(std::size_t i = 1; i < spans.size(); ++i)
    {
        const Span& previous = spans[i - 1];
        const Span& current = spans[i];
        if(current.begin < previous.end &&
           (current.begin != previous.begin || current.end != previous.end))
            return fail(error, "NPC position tables and records partially overlap");
    }
    return true;
}

bool validTrailer(const std::array<std::uint8_t, 0x40>& trailer)
{
    bool allZero = true;
    for(std::uint8_t value : trailer)
        allZero = allZero && value == 0;
    if(allZero)
        return true;
    if(trailer[0] != 4)
        return false;
    for(std::size_t i = 1; i < trailer.size(); ++i)
    {
        if(trailer[i] != 0)
            return false;
    }
    return true;
}

constexpr std::uint8_t COMBAT_AI_MAX_PROFILE_ID = 70;

constexpr std::size_t AI_SOURCE_SLOT = 0;
constexpr std::size_t AI_SOURCE_SELF_STATE = 2;
constexpr std::size_t AI_SOURCE_SELF_HEALTH = 3;
constexpr std::size_t AI_SOURCE_SELF_DAMAGE = 4;
constexpr std::size_t AI_SOURCE_TARGET_STATE = 5;
constexpr std::size_t AI_SOURCE_TARGET_HEALTH = 6;
constexpr std::size_t AI_SOURCE_DISTANCE = 7;
constexpr std::size_t AI_SOURCE_DIFFICULTY = 8;
constexpr std::size_t AI_SOURCE_RECORD = 9;
constexpr std::size_t AI_SOURCE_LOGICAL_INPUT = 10;
constexpr std::size_t AI_SOURCE_COMMAND = 12;
constexpr std::size_t AI_SOURCE_WAIT = 13;
constexpr std::size_t AI_SOURCE_COMPARISON = 16;
constexpr std::size_t AI_SOURCE_WATCHDOG = 17;
constexpr std::size_t AI_SOURCE_TARGET_ATTACK = 18;
constexpr std::size_t AI_SOURCE_TARGET_DIRECTION = 19;
constexpr std::size_t AI_SOURCE_SELF_DIRECTION = 20;
constexpr std::size_t AI_SOURCE_RESERVED = 21;

constexpr std::uint64_t AI_RECORD_ACTIVE = std::uint64_t(1) << 5;
constexpr std::uint64_t AI_RECORD_INTERRUPTED = std::uint64_t(1) << 6;
constexpr std::uint64_t AI_SENSOR_BIT_MASK =
    (std::uint64_t(1) << 0) | (std::uint64_t(1) << 1) |
    (std::uint64_t(1) << 2) | (std::uint64_t(1) << 3) |
    (std::uint64_t(1) << 8) | (std::uint64_t(1) << 9) |
    (std::uint64_t(1) << 10) | (std::uint64_t(1) << 11) |
    (std::uint64_t(1) << 12) | (std::uint64_t(1) << 13) |
    (std::uint64_t(1) << 14) | (std::uint64_t(1) << 15);

constexpr float AI_APPROACH_VERTICAL_LIMIT = 35.0f;
constexpr float AI_APPROACH_NARROW_ANGLE = 50.0f;
constexpr float AI_APPROACH_WIDE_ANGLE = 70.0f;
constexpr float AI_CHANNEL_MOVEMENT_THRESHOLD = 0.3f;

constexpr std::int16_t AI_RECORD_WATCHDOG_FRAMES = 185;

std::int16_t percentage(std::int16_t value, std::int16_t maximum)
{
    if(maximum <= 0)
        return 0;
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(value) * 100 / maximum);
}

std::int16_t clampPercent(std::int32_t value)
{
    return static_cast<std::int16_t>(
        std::max<std::int32_t>(0, std::min<std::int32_t>(100, value)));
}

bool finitePosition(const StageAiVec3& position)
{
    return std::isfinite(position.x) && std::isfinite(position.y) &&
           std::isfinite(position.z);
}

bool readConditionLhs(LgpAiVmState* state, std::int32_t selector,
                      std::int16_t* value)
{
    if(state == NULL || value == NULL)
        return false;
    if(selector == -1)
    {
        *value = static_cast<std::int16_t>(nextLgpAiRandom(state) % 100u);
        return true;
    }
    if(!validSourceSelector(selector))
    {
        state->failed = true;
        return false;
    }
    *value = state->sourceParam[static_cast<std::size_t>(selector)];
    return true;
}

bool readConditionRhs(LgpAiVmState* state, std::uint16_t flags,
                      std::int16_t rhs, std::int16_t* value)
{
    if(state == NULL || value == NULL || !validConditionOperand(flags, rhs))
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }
    if((flags & 0x20) != 0)
    {
        const std::size_t bit = static_cast<std::size_t>(rhs);
        *value = ((state->bitState >> bit) & 1u) != 0 ? 1 : 0;
        return true;
    }
    if((flags & 0x10) != 0)
    {
        if(rhs == -1)
        {
            *value = static_cast<std::int16_t>(nextLgpAiRandom(state) % 100u);
            return true;
        }
        *value = state->sourceParam[static_cast<std::size_t>(rhs)];
        return true;
    }
    *value = rhs;
    return true;
}

bool resolveLgpAiChainInternal(const LgpStageAiProgram& program,
                               LgpAiVmState* state,
                               std::int32_t recordIndex,
                               std::int16_t* actionId,
                               std::size_t* steps);
bool dispatchChainLink(const LgpStageAiProgram& program,
                       LgpAiVmState* state,
                       std::int16_t nextKind,
                       std::int16_t nextIndex,
                       std::int16_t* actionId,
                       std::size_t* steps)
{
    if(actionId == NULL || steps == NULL || *steps >= MAX_CHAIN_STEPS)
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }

    switch(nextKind)
    {
    case 0:
        ++*steps;
        return resolveLgpAiChainInternal(program, state, nextIndex,
                                         actionId, steps);
    case 1:
        ++*steps;
        return selectLgpWeightedAction(program, state, nextIndex, actionId);
    case 2:
        *actionId = nextIndex;
        return true;
    case 3:
    {
        std::int16_t branchKind = 0;
        std::int16_t branchIndex = 0;
        ++*steps;
        if(!resolveLgpAiBranch(program, state, nextIndex,
                               &branchKind, &branchIndex))
            return false;
        return dispatchChainLink(program, state, branchKind, branchIndex,
                                 actionId, steps);
    }
    default:
        if(state != NULL)
            state->failed = true;
        return false;
    }
}

}

bool
parseLgpStageAi(const std::uint8_t* data, std::size_t size,
                LgpStageAiProgram* program, std::string* error)
{
    if(program == NULL || data == NULL)
        return fail(error, "LGP parser received a null argument");
    if(size < LGP_HEADER_SIZE)
        return fail(error, "LGP input is shorter than its 0x18-byte header");

    LgpStageAiProgram parsed = {};
    parsed.header.primaryChainWord = readU32(data, 0);
    parsed.header.alternateChainWord = readU32(data, 4);
    parsed.header.sectionACount = readU32(data, 8);
    parsed.header.sectionBCount = readU32(data, 12);
    parsed.header.sectionDCount = readU32(data, 16);
    parsed.header.sectionCCount = readU32(data, 20);
    parsed.header.primaryChainIndex = static_cast<std::int16_t>(
        parsed.header.primaryChainWord & 0xffffu);
    parsed.header.alternateChainIndex = static_cast<std::int16_t>(
        parsed.header.alternateChainWord & 0xffffu);

    const std::size_t aCount = parsed.header.sectionACount;
    const std::size_t bCount = parsed.header.sectionBCount;
    const std::size_t cCount = parsed.header.sectionCCount;
    const std::size_t dCount = parsed.header.sectionDCount;
    std::size_t cursor = LGP_HEADER_SIZE;

    if(aCount > (size - cursor) / LGP_A_SIZE)
        return fail(error, "LGP A section is outside the input");
    parsed.sectionA.resize(aCount);
    for(std::size_t recordIndex = 0; recordIndex < aCount; ++recordIndex)
    {
        const std::size_t recordOffset = cursor + recordIndex * LGP_A_SIZE;
        LgpARecord& record = parsed.sectionA[recordIndex];
        record.commandCount = readU32(data, recordOffset);
        if(record.commandCount == 0 || record.commandCount > record.commands.size())
            return failFormatted(error, "LGP A command count exceeds twelve at", recordOffset);
        for(std::size_t commandIndex = 0; commandIndex < record.commands.size();
            ++commandIndex)
        {
            const std::size_t offset = recordOffset + 4 + commandIndex * 12;
            LgpACommand& command = record.commands[commandIndex];
            command.control = readS16(data, offset);
            command.operand = readS16(data, offset + 2);
            command.kind = readS16(data, offset + 4);
            command.kindArgument = readS16(data, offset + 6);
            for(std::size_t channel = 0; channel < command.output.size(); ++channel)
                command.output[channel] = data[offset + 8 + channel];
            if(commandIndex < record.commandCount &&
               !validateLgpACommand(command, error))
                return false;
        }
    }
    cursor += aCount * LGP_A_SIZE;

    if(bCount > (size - cursor) / LGP_B_SIZE)
        return fail(error, "LGP B section is outside the input");
    parsed.sectionB.resize(bCount);
    for(std::size_t recordIndex = 0; recordIndex < bCount; ++recordIndex)
    {
        const std::size_t recordOffset = cursor + recordIndex * LGP_B_SIZE;
        LgpBRecord& record = parsed.sectionB[recordIndex];
        record.selector = readS16(data, recordOffset);
        if(record.selector < -2 && !validSourceSelector(record.selector))
            return fail(error, "LGP B selector is outside sourceParam");
        for(std::size_t i = 0; i < record.threshold.size(); ++i)
            record.threshold[i] = readS16(data, recordOffset + 2 + i * 2);
        for(std::size_t choiceIndex = 0; choiceIndex < record.choices.size();
            ++choiceIndex)
        {
            const std::size_t offset = recordOffset + 16 + choiceIndex * 16;
            LgpBChoice& choice = record.choices[choiceIndex];
            choice.value = readS16(data, offset);
            choice.weight = readS16(data, offset + 2);
            if(choice.weight < 0)
                return fail(error, "LGP B tier-zero weight is negative");
            for(std::size_t lane = 0; lane < choice.reserved.size(); ++lane)
            {
                choice.reserved[lane] = readS16(data, offset + 4 + lane * 2);
                if(choice.reserved[lane] < 0)
                    return fail(error, "LGP B tier weight is negative");
            }
        }
    }
    cursor += bCount * LGP_B_SIZE;

    if(cCount > (size - cursor) / LGP_C_SIZE)
        return fail(error, "LGP C section is outside the input");
    parsed.sectionC.resize(cCount);
    for(std::size_t recordIndex = 0; recordIndex < cCount; ++recordIndex)
    {
        const std::size_t recordOffset = cursor + recordIndex * LGP_C_SIZE;
        LgpCRecord& record = parsed.sectionC[recordIndex];
        record.sourceSelector = readS32(data, recordOffset);
        if(record.sourceSelector != -1 &&
           !validSourceSelector(record.sourceSelector))
            return fail(error, "LGP C source selector is outside sourceParam");
        for(std::size_t linkIndex = 0; linkIndex < record.links.size(); ++linkIndex)
        {
            const std::size_t offset = recordOffset + 4 + linkIndex * 8;
            LgpCLink& link = record.links[linkIndex];
            link.conditionFlags = readU16(data, offset);
            link.rhs = readS16(data, offset + 2);
            link.nextKind = readS16(data, offset + 4);
            link.nextIndex = readS16(data, offset + 6);
            if(!validConditionOperand(link.conditionFlags, link.rhs))
                return fail(error, "LGP C condition has an invalid selector or operand");
            if(!validChainReference(link.nextKind, link.nextIndex,
                                    bCount, cCount, dCount))
                return fail(error, "LGP C link targets an invalid chain record");
        }
    }
    cursor += cCount * LGP_C_SIZE;

    if(dCount > (size - cursor) / LGP_D_SIZE)
        return fail(error, "LGP D section is outside the input");
    parsed.sectionD.resize(dCount);
    for(std::size_t recordIndex = 0; recordIndex < dCount; ++recordIndex)
    {
        const std::size_t recordOffset = cursor + recordIndex * LGP_D_SIZE;
        LgpDRecord& record = parsed.sectionD[recordIndex];
        record.sourceSelector = readS16(data, recordOffset);
        record.reserved0 = readS16(data, recordOffset + 2);
        record.nextKind = readS16(data, recordOffset + 4);
        record.nextIndex = readS16(data, recordOffset + 6);
        if(!validSourceSelector(record.sourceSelector) ||
           !validChainReference(record.nextKind, record.nextIndex,
                                bCount, cCount, dCount))
            return fail(error, "LGP D record has an invalid source or chain target");
        for(std::size_t lane = 0; lane < record.threshold.size(); ++lane)
        {
            for(std::size_t word = 0; word < record.threshold[lane].selectorAndThreshold.size();
                ++word)
            {
                record.threshold[lane].selectorAndThreshold[word] = readS16(
                    data, recordOffset + 8 + lane * 16 + word * 2);
            }
        }
        for(std::size_t lane1 = 0; lane1 < record.transition.size(); ++lane1)
        {
            for(std::size_t lane0 = 0; lane0 < record.transition[lane1].size(); ++lane0)
            {
                const std::size_t transitionIndex = lane1 * 7 + lane0;
                record.transition[lane1][lane0] = readS16(
                    data, recordOffset + 40 + transitionIndex * 2);
            }
        }
        record.reserved1 = readS16(data, recordOffset + 138);
        for(std::size_t i = 0; i < record.matchKey.size(); ++i)
        {
            record.matchKey[i] = readS16(data, recordOffset + 140 + i * 2);
            record.matchValue[i] = readS16(data, recordOffset + 154 + i * 2);
        }
    }
    cursor += dCount * LGP_D_SIZE;

    if(cursor != size)
        return fail(error, "LGP input has trailing bytes after section D");
    if(parsed.header.primaryChainIndex < 0 ||
       static_cast<std::size_t>(parsed.header.primaryChainIndex) >= cCount ||
       parsed.header.alternateChainIndex < 0 ||
       static_cast<std::size_t>(parsed.header.alternateChainIndex) >= cCount)
        return fail(error, "LGP header chain index is outside section C");

    *program = parsed;
    if(error != NULL)
        error->clear();
    return true;
}

bool
parseLgpStageAi(const std::vector<std::uint8_t>& data,
                LgpStageAiProgram* program, std::string* error)
{
    return parseLgpStageAi(data.data(), data.size(), program, error);
}

bool
loadLgpStageAi(const char* archivePath, const char* entryName,
               LgpStageAiProgram* program, std::string* error)
{
    if(archivePath == NULL || entryName == NULL || program == NULL)
        return fail(error, "LGP archive loader received a null argument");
    std::vector<std::uint8_t> data;
    if(!readXafEntry(archivePath, entryName, data))
        return fail(error, "LGP archive entry could not be read");
    return parseLgpStageAi(data, program, error);
}

bool
loadStageAiProgram(const char* archiveDirectory, std::uint16_t stageId,
                   LgpStageAiProgram* program, std::string* error)
{
    if(archiveDirectory == NULL || program == NULL)
        return fail(error, "stage AI loader received a null argument");
    char archivePath[512];
    char entryName[32];
    const int archiveLength = std::snprintf(archivePath, sizeof(archivePath),
                                            "%s/FSTAGE.XAF", archiveDirectory);
    const int entryLength = std::snprintf(entryName, sizeof(entryName),
                                          "AI%03u.LGP", stageId);
    if(archiveLength < 0 || static_cast<std::size_t>(archiveLength) >= sizeof(archivePath) ||
       entryLength < 0 || static_cast<std::size_t>(entryLength) >= sizeof(entryName))
        return fail(error, "stage AI archive path is too long");
    return loadLgpStageAi(archivePath, entryName, program, error);
}

bool
parseNpcPositionDat(const std::uint8_t* data, std::size_t size,
                    NpcPositionDat* database, std::string* error)
{
    if(database == NULL || data == NULL)
        return fail(error, "NPC position parser received a null argument");
    if(size < NPC_ROOT_SIZE)
        return fail(error, "NPC position input is shorter than its root table");

    NpcPositionDat parsed = {};
    parsed.tableIndices.fill(-1);
    for(NpcStagePositionTable& table : parsed.tables)
        table.recordIndices.fill(-1);

    std::map<std::uint32_t, std::int32_t> tableMap;
    std::map<std::uint32_t, std::int32_t> recordMap;
    std::vector<Span> spans;
    spans.push_back({0, static_cast<std::uint32_t>(NPC_ROOT_SIZE)});

    for(std::size_t npcIndex = 0; npcIndex < parsed.npcTableOffsets.size(); ++npcIndex)
    {
        const std::uint32_t tableOffset = readU32(data, npcIndex * 4);
        parsed.npcTableOffsets[npcIndex] = tableOffset;
        if(tableOffset == 0)
            continue;
        if(tableOffset > std::numeric_limits<std::uint32_t>::max() - NPC_TABLE_SIZE ||
           (tableOffset & 3u) != 0 ||
           !hasBytes(size, tableOffset, NPC_TABLE_SIZE))
            return failFormatted(error, "NPC stage table is misaligned or out of range at", tableOffset);
        auto tableIt = tableMap.find(tableOffset);
        if(tableIt == tableMap.end())
        {
            const std::int32_t index = static_cast<std::int32_t>(parsed.tables.size());
            tableMap.insert({tableOffset, index});
            NpcStagePositionTable table = {};
            table.sourceOffset = tableOffset;
            table.recordIndices.fill(-1);
            parsed.tables.push_back(table);
            spans.push_back({tableOffset,
                             static_cast<std::uint32_t>(tableOffset + NPC_TABLE_SIZE)});
        }
        parsed.tableIndices[npcIndex] = tableMap.find(tableOffset)->second;
    }

    for(NpcStagePositionTable& table : parsed.tables)
    {
        for(std::size_t stageIndex = 0; stageIndex < table.recordOffsets.size(); ++stageIndex)
        {
            const std::uint32_t recordOffset = readU32(
                data, table.sourceOffset + stageIndex * 4);
            table.recordOffsets[stageIndex] = recordOffset;
            if(recordOffset == 0)
                continue;
            if(recordOffset > std::numeric_limits<std::uint32_t>::max() - NPC_RECORD_SIZE ||
               (recordOffset & 3u) != 0 ||
               !hasBytes(size, recordOffset, NPC_RECORD_SIZE))
                return failFormatted(error, "NPC position record is misaligned or out of range at", recordOffset);
            auto recordIt = recordMap.find(recordOffset);
            if(recordIt == recordMap.end())
            {
                const std::int32_t index = static_cast<std::int32_t>(parsed.records.size());
                recordMap.insert({recordOffset, index});
                NpcPositionRecord record = {};
                record.sourceOffset = recordOffset;
                parsed.records.push_back(record);
                spans.push_back({recordOffset,
                                 static_cast<std::uint32_t>(recordOffset + NPC_RECORD_SIZE)});
            }
            table.recordIndices[stageIndex] = recordMap.find(recordOffset)->second;
        }
    }

    if(!validateSpans(spans, error))
        return false;

    for(NpcPositionRecord& record : parsed.records)
    {
        for(std::size_t row = 0; row < record.rows.size(); ++row)
        {
            for(std::size_t column = 0; column < record.rows[row].value.size(); ++column)
            {
                const std::size_t offset = record.sourceOffset +
                    (row * NPC_ROW_WIDTH + column) * sizeof(std::int16_t);
                record.rows[row].value[column] = readS16(data, offset);
            }
        }
        for(std::size_t i = 0; i < record.reservedTrailer.size(); ++i)
            record.reservedTrailer[i] = data[record.sourceOffset + 0x1c0 + i];
        if(!validTrailer(record.reservedTrailer))
            return failFormatted(error, "NPC position record has an unknown trailer at", record.sourceOffset);
    }

    *database = parsed;
    if(error != NULL)
        error->clear();
    return true;
}

bool
parseNpcPositionDat(const std::vector<std::uint8_t>& data,
                    NpcPositionDat* database, std::string* error)
{
    return parseNpcPositionDat(data.data(), data.size(), database, error);
}

bool
loadNpcPositionDat(const char* archivePath, const char* entryName,
                   NpcPositionDat* database, std::string* error)
{
    if(archivePath == NULL || entryName == NULL || database == NULL)
        return fail(error, "NPC position archive loader received a null argument");
    std::vector<std::uint8_t> data;
    if(!readXafEntry(archivePath, entryName, data))
        return fail(error, "NPC position archive entry could not be read");
    return parseNpcPositionDat(data, database, error);
}

bool
loadStageNpcPositionDat(const char* archiveDirectory,
                        NpcPositionDat* database, std::string* error)
{
    if(archiveDirectory == NULL || database == NULL)
        return fail(error, "stage NPC position loader received a null argument");
    char archivePath[512];
    const int length = std::snprintf(archivePath, sizeof(archivePath),
                                     "%s/CDATA.XAF", archiveDirectory);
    if(length < 0 || static_cast<std::size_t>(length) >= sizeof(archivePath))
        return fail(error, "NPC position archive path is too long");
    return loadNpcPositionDat(archivePath, "NPOS_DATA.DAT", database, error);
}

const NpcPositionRecord*
findNpcPositionRecord(const NpcPositionDat& database,
                   std::uint16_t npcIndex, std::uint16_t stageIndex)
{
    if(npcIndex >= database.tableIndices.size() || stageIndex >= 56)
        return NULL;
    const std::int32_t tableIndex = database.tableIndices[npcIndex];
    if(tableIndex < 0 || static_cast<std::size_t>(tableIndex) >= database.tables.size())
        return NULL;
    const NpcStagePositionTable& table = database.tables[tableIndex];
    const std::int32_t recordIndex = table.recordIndices[stageIndex];
    if(recordIndex < 0 || static_cast<std::size_t>(recordIndex) >= database.records.size())
        return NULL;
    return &database.records[recordIndex];
}

const NpcPositionRow*
findNpcPositionRow(const NpcPositionDat& database,
                 std::uint16_t npcIndex, std::uint16_t stageIndex,
                 std::uint8_t rowIndex)
{
    if(rowIndex >= NPC_ROW_COUNT)
        return NULL;
    const NpcPositionRecord* record = findNpcPositionRecord(database, npcIndex, stageIndex);
    return record != NULL ? &record->rows[rowIndex] : NULL;
}

bool
getNpcSpawnTransform(const NpcPositionRow& row, NpcSpawnTransform* transform)
{
    if(transform == NULL)
        return false;
    if(row.value[0] == NPC_POSITION_CONDITIONAL && row.value[1] == 0)
    {
        transform->x = 2152.42f;
        transform->y = 76.64f;
        transform->z = -461.59f;
        transform->headingDegrees = 0.0f;
        return true;
    }

    transform->x = static_cast<float>(row.value[4]) * 0.1f;
    transform->y = static_cast<float>(row.value[5]) * 0.1f;
    transform->z = static_cast<float>(row.value[6]) * 0.1f;
    transform->headingDegrees = static_cast<float>(row.value[7]) * 0.1f;
    return true;
}

void
resetLgpAiVmState(LgpAiVmState* state, LgpAiRandomSource random)
{
    if(state == NULL)
        return;
    state->sourceParam.fill(0);
    state->bitState = 0;
    state->random = random;
    state->channels.fill(0.0f);
    state->actionId = 0;
    state->failed = random.draw == NULL;
}

std::uint32_t
nextLgpAiRandom(LgpAiVmState* state)
{
    if(state == NULL || state->random.draw == NULL)
    {
        if(state != NULL)
            state->failed = true;
        return 0;
    }

    return state->random.draw(state->random.context);
}

bool
evaluateLgpAiCondition(LgpAiVmState* state,
                         std::int32_t sourceSelector,
                         std::uint16_t conditionFlags,
                         std::int16_t rhs)
{
    if(state == NULL || !validConditionFlags(conditionFlags))
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }
    std::int16_t lhs = 0;
    std::int16_t right = 0;
    if(!readConditionLhs(state, sourceSelector, &lhs) ||
       !readConditionRhs(state, conditionFlags, rhs, &right))
        return false;
    switch(conditionFlags & 0x0f)
    {
    case 0:
        return lhs == right;
    case 1:
        return lhs != right;
    case 2:
        return lhs < right;
    case 3:
        return lhs <= right;
    case 4:
        return lhs > right;
    case 5:
        return lhs >= right;
    case 6:
        return true;
    case 7:
        return false;
    case 8:
        return right != 0;
    case 9:
        return right == 0;
    default:
        state->failed = true;
        return false;
    }
}

bool
selectLgpWeightedAction(const LgpStageAiProgram& program,
                        LgpAiVmState* state, std::int32_t recordIndex,
                        std::int16_t* actionId)
{
    if(state == NULL || actionId == NULL || recordIndex < 0 ||
       static_cast<std::size_t>(recordIndex) >= program.sectionB.size())
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }
    const LgpBRecord& record = program.sectionB[recordIndex];
    std::int32_t selector = record.selector;
    std::size_t tier = 0;
    if(selector >= -1)
    {
        if(selector == -1)
            selector = static_cast<std::int32_t>(nextLgpAiRandom(state) % 100u);
        else if(!validSourceSelector(selector))
        {
            state->failed = true;
            return false;
        }
        if(record.selector >= 0)
            selector = state->sourceParam[static_cast<std::size_t>(record.selector)];
        tier = 0;
        for(std::size_t i = 0; i < record.threshold.size(); ++i)
        {
            if(selector <= record.threshold[i])
            {
                tier = i;
                break;
            }
        }
    }

    const std::uint32_t roll = nextLgpAiRandom(state) % 100u;
    std::int32_t cumulative = 0;
    for(const LgpBChoice& choice : record.choices)
    {
        const std::int16_t weight = tier == 0 ? choice.weight :
            choice.reserved[tier - 1];
        cumulative += weight;
        if(static_cast<std::int32_t>(roll) < cumulative)
        {
            *actionId = choice.value;
            return true;
        }
    }
    *actionId = -1;
    return true;
}

bool
resolveLgpAiBranch(const LgpStageAiProgram& program,
                   LgpAiVmState* state, std::int32_t recordIndex,
                   std::int16_t* nextKind, std::int16_t* nextIndex)
{
    if(state == NULL || nextKind == NULL || nextIndex == NULL || recordIndex < 0 ||
       static_cast<std::size_t>(recordIndex) >= program.sectionD.size())
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }
    const LgpDRecord& record = program.sectionD[recordIndex];
    if(!validSourceSelector(record.sourceSelector))
    {
        state->failed = true;
        return false;
    }
    std::array<std::size_t, 2> laneIndex = {0, 0};
    for(std::size_t lane = 0; lane < 2; ++lane)
    {
        const std::int16_t selector = record.threshold[lane].selectorAndThreshold[0];
        if(selector < 0)
            continue;
        if(!validSourceSelector(selector))
        {
            state->failed = true;
            return false;
        }
        const std::int16_t value = state->sourceParam[selector];
        for(std::size_t index = 0; index < 7; ++index)
        {
            const std::int16_t threshold =
                record.threshold[lane].selectorAndThreshold[index + 1];
            if(value <= threshold)
            {
                laneIndex[lane] = index;
                break;
            }
        }
    }

    const std::int16_t stateValue = record.transition[laneIndex[1]][laneIndex[0]];
    std::int16_t& targetState = state->sourceParam[record.sourceSelector];
    if(targetState == stateValue)
    {
        targetState = stateValue;
        for(std::size_t i = 0; i < record.matchKey.size(); ++i)
        {
            if(record.matchKey[i] == -1)
                break;
            if(record.matchKey[i] == stateValue)
            {
                targetState = record.matchValue[i];
                break;
            }
        }
    }
    else
        targetState = stateValue;

    *nextKind = record.nextKind;
    *nextIndex = record.nextIndex;
    return true;
}

namespace
{
bool
resolveLgpAiChainInternal(const LgpStageAiProgram& program,
                          LgpAiVmState* state, std::int32_t recordIndex,
                          std::int16_t* actionId, std::size_t* steps)
{
    if(state == NULL || actionId == NULL || steps == NULL || recordIndex < 0 ||
       static_cast<std::size_t>(recordIndex) >= program.sectionC.size())
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }
    std::int32_t current = recordIndex;
    while(*steps < MAX_CHAIN_STEPS)
    {
        ++*steps;
        if(current < 0 || static_cast<std::size_t>(current) >= program.sectionC.size())
        {
            state->failed = true;
            return false;
        }
        const LgpCRecord& record = program.sectionC[current];
        std::size_t selectedLink = record.links.size() - 1;
        for(std::size_t linkIndex = 0; linkIndex + 1 < record.links.size(); ++linkIndex)
        {
            const LgpCLink& link = record.links[linkIndex];
            if(evaluateLgpAiCondition(state, record.sourceSelector,
                                      link.conditionFlags, link.rhs))
            {
                selectedLink = linkIndex;
                break;
            }
            if(state->failed)
                return false;
        }
        const LgpCLink& link = record.links[selectedLink];
        switch(link.nextKind)
        {
        case 0:
            current = link.nextIndex;
            continue;
        case 1:
            return selectLgpWeightedAction(program, state, link.nextIndex,
                                           actionId);
        case 2:
            *actionId = link.nextIndex;
            return true;
        case 3:
        {
            std::int16_t branchKind = 0;
            std::int16_t branchIndex = 0;
            if(!resolveLgpAiBranch(program, state, link.nextIndex,
                                   &branchKind, &branchIndex))
                return false;
            return dispatchChainLink(program, state, branchKind, branchIndex,
                                     actionId, steps);
        }
        default:
            state->failed = true;
            return false;
        }
    }
    state->failed = true;
    return false;
}

}
bool
resolveLgpAiChain(const LgpStageAiProgram& program,
                  LgpAiVmState* state, std::int32_t recordIndex,
                  std::int16_t* actionId)
{
    std::size_t steps = 0;
    return resolveLgpAiChainInternal(program, state, recordIndex,
                                     actionId, &steps);
}

bool
applyLgpAiRecord(const LgpARecord& record, LgpAiVmState* state,
                 std::uint16_t* commandIndex, std::uint16_t* waitTicks,
                 LgpAiApplyResult* result)
{
    if(state == NULL || commandIndex == NULL || waitTicks == NULL || result == NULL ||
       record.commandCount == 0 || record.commandCount > record.commands.size())
    {
        if(state != NULL)
            state->failed = true;
        return false;
    }
    if(*commandIndex >= record.commandCount)
        *commandIndex = 0;
    const LgpACommand& command = record.commands[*commandIndex];
    result->advanced = false;
    result->emitted = true;
    result->actionId = state->actionId;
    result->channels = state->channels;

    switch(command.kind)
    {
    case 0:
        state->actionId = command.operand;
        for(std::size_t i = 0; i < state->channels.size(); ++i)
            state->channels[i] = static_cast<float>(command.output[i]) / 127.5f - 1.0f;
        result->actionId = state->actionId;
        result->channels = state->channels;
        if(*waitTicks < std::numeric_limits<std::uint16_t>::max())
            ++*waitTicks;
        if(command.control <= static_cast<std::int16_t>(*waitTicks))
        {
            result->advanced = true;
            *waitTicks = 0;
        }
        break;
    case 1:
        if(evaluateLgpAiCondition(state, command.kindArgument,
                                  static_cast<std::uint16_t>(command.control),
                                  command.operand))
            result->advanced = true;
        break;
    case 2:
    {
        const std::uint64_t mask = std::uint64_t(1) << command.control;
        const std::int16_t bit = (state->bitState & mask) != 0 ? 1 : 0;
        if(bit == command.operand)
            result->advanced = true;
        break;
    }
    case 3:
        if(!validSourceSelector(command.control))
        {
            state->failed = true;
            return false;
        }
        state->sourceParam[command.control] = command.kindArgument;
        result->advanced = true;
        break;
    case 4:
    {
        const std::uint64_t mask = std::uint64_t(1) << command.control;
        if(command.operand == -1)
            state->bitState ^= mask;
        else if(command.operand == 0)
            state->bitState &= ~mask;
        else
            state->bitState |= mask;
        result->advanced = true;
        break;
    }
    default:
        state->failed = true;
        return false;
    }

    if(state->failed)
        return false;
    if(result->advanced)
    {
        ++*commandIndex;
        if(*commandIndex >= record.commandCount)
        {
            *commandIndex = 0;

            state->bitState &= ~(AI_RECORD_ACTIVE | AI_RECORD_INTERRUPTED);
        }
    }
    return true;
}

void
clearNpcPositionRows(
    std::array<NpcPositionRow, STAGE_ACTOR_ROW_COUNT>* rows)
{
    if(rows == NULL)
        return;
    for(NpcPositionRow& row : *rows)
        row = {};
}

bool
applyNpcPositionOverlay(
    const NpcPositionRecord& source, bool baseLayer, bool battleLayer,
    NpcPositionOverlayFilter filter, void* context,
    std::array<NpcPositionRow, STAGE_ACTOR_ROW_COUNT>* rows,
    std::string* error)
{
    if(rows == NULL)
        return fail(error, "NPC overlay destination is null");
    for(std::size_t index = 0; index < source.rows.size(); ++index)
    {
        const NpcPositionRow& sourceRow = source.rows[index];
        const std::int16_t rawKind = sourceRow.value[0];
        if(rawKind < NPC_POSITION_BASE ||
           rawKind > NPC_POSITION_CLEAR_ALTERNATE)
            return fail(error, "NPC position row has an unknown overlay kind");

        if(rawKind == NPC_POSITION_CLEAR ||
           rawKind == NPC_POSITION_CLEAR_ALTERNATE)
        {
            (*rows)[index] = {};
            continue;
        }
        if(rawKind == NPC_POSITION_BASE)
        {

            if(baseLayer)
                (*rows)[index] = sourceRow;
            continue;
        }
        if(rawKind == NPC_POSITION_CONDITIONAL ||
           rawKind == NPC_POSITION_CONDITIONAL_ALTERNATE)
        {
            (*rows)[index] = sourceRow;
            continue;
        }

        NpcPositionOverlayDecision decision = NPC_POSITION_OVERLAY_COPY;
        if(filter != NULL)
        {
            decision = filter(sourceRow, static_cast<std::uint8_t>(index),
                              battleLayer, context);
        }
        if(decision == NPC_POSITION_OVERLAY_COPY)
            (*rows)[index] = sourceRow;
        else if(decision == NPC_POSITION_OVERLAY_CLEAR)
            (*rows)[index] = {};
    }
    if(error != NULL)
        error->clear();
    return true;
}

bool
buildStageActorPreload(const NpcPositionRecord& finalRecord,
                       StageActorPreloadPlan* plan, std::string* error)
{
    if(plan == NULL)
        return fail(error, "stage actor preload output is null");
    *plan = {};
    for(std::size_t index = 0; index < finalRecord.rows.size(); ++index)
    {
        const NpcPositionRow& row = finalRecord.rows[index];
        const std::int16_t rawKind = row.value[0];
        if(rawKind < NPC_POSITION_BASE ||
           rawKind > NPC_POSITION_CLEAR_ALTERNATE)
            return fail(error, "NPC position row has an unknown preload kind");

        StageActorDescriptor& descriptor = plan->rows[index];
        descriptor.rowIndex = static_cast<std::uint8_t>(index);
        descriptor.kind = static_cast<NpcPositionRowKind>(rawKind);
        descriptor.selectorId = row.value[1];
        descriptor.alternateMotionId = row.value[2];
        descriptor.interactionSelector = row.value[3];
        descriptor.defaultMotionId = row.value[8];
        if(rawKind <= NPC_POSITION_CONDITIONAL_ALTERNATE &&
           !getNpcSpawnTransform(row, &descriptor.transform))
            return fail(error, "NPC row transform could not be decoded");
        descriptor.actorPreload = rawKind == NPC_POSITION_CHARACTER;
        descriptor.resourcePreload = rawKind == NPC_POSITION_CONDITIONAL;
        if(descriptor.actorPreload)
            ++plan->actorCount;
        if(descriptor.resourcePreload)
            ++plan->resourceCount;
    }
    if(error != NULL)
        error->clear();
    return true;
}

std::uint8_t
combatAiProfileForCharacter(std::uint16_t characterId, CombatAiRole role,
                            std::uint8_t relationshipState)
{
    const std::uint8_t battleProfile =
        characterId < COMBAT_AI_CHARACTER_PROFILE_COUNT ?
            COMBAT_AI_CHARACTER_PROFILES[characterId].battleProfileId : 0;
    CombatAiProfileSelection selection = {};
    selection.characterId = characterId;
    selection.persistentProfileId = battleProfile;
    selection.role = role;
    selection.relationshipState = relationshipState;
    selection.profileCategory = combatAiProfileCategory(characterId);
    selection.activeRoster = role == COMBAT_AI_PARTY_PARTNER;
    return resolveCombatAiProfile(selection);
}

std::uint8_t
combatAiProfileCategory(std::uint16_t characterId)
{
    return characterId < COMBAT_AI_CHARACTER_PROFILE_COUNT ?
        COMBAT_AI_CHARACTER_PROFILES[characterId].profileCategory : 0;
}

bool
combatAiCategoryExcluded(std::uint8_t profileCategory)
{
    return profileCategory == 3 || profileCategory == 5;
}

std::uint8_t
resolveCombatAiProfile(const CombatAiProfileSelection& selection)
{
    if(selection.role == COMBAT_AI_PRIMARY_OPPONENT)
    {
        std::uint16_t profile = selection.persistentProfileId;
        if(selection.battleMode == 3)
            profile = 0x19;
        if(selection.forceProfile44Matchup)
            profile = 0x3e;
        return profile <= COMBAT_AI_MAX_PROFILE_ID ?
            static_cast<std::uint8_t>(profile) : 0;
    }

    if(selection.activeRoster ||
       selection.role == COMBAT_AI_PARTY_PARTNER)
        return selection.relationshipState == 2 ? 0x1e : 0x18;

    const std::uint8_t category = selection.profileCategory;
    if(category != 1 && category != 2 && category != 6 && category != 8)
        return 0x2d;
    if(selection.characterId >= COMBAT_AI_CHARACTER_PROFILE_COUNT)
        return 0;
    const std::uint8_t profile =
        COMBAT_AI_CHARACTER_PROFILES[selection.characterId].battleProfileId;
    return profile <= COMBAT_AI_MAX_PROFILE_ID ? profile : 0;
}

std::int16_t
combatAiDifficultyDecisionValue(std::uint8_t difficulty)
{

    const std::size_t index = difficulty < COMBAT_AI_DIFFICULTY_COUNT ?
        difficulty : 3;
    return COMBAT_AI_DIFFICULTY[index].decisionValue;
}

float
combatAiDifficultyAttackMultiplier(std::uint8_t difficulty)
{
    const std::size_t index = difficulty < COMBAT_AI_DIFFICULTY_COUNT ?
        difficulty : 3;
    return COMBAT_AI_DIFFICULTY[index].attackPowerMultiplier;
}

bool
loadCombatAiProgram(const char* archiveDirectory, std::uint8_t profileId,
                    LgpStageAiProgram* program, std::string* error)
{
    if(profileId > COMBAT_AI_MAX_PROFILE_ID)
        profileId = 0;

    return loadStageAiProgram(archiveDirectory, profileId, program, error);
}

bool
initializeCombatAiRuntime(CombatAiRuntime* runtime,
                          const char* archiveDirectory,
                          std::uint8_t profileId,
                          LgpAiRandomSource random,
                          std::string* error)
{
    if(runtime == NULL || archiveDirectory == NULL || random.draw == NULL)
        return fail(error, "combat AI initialization received a null argument");
    CombatAiRuntime initialized = {};
    initialized.profileId = profileId <= COMBAT_AI_MAX_PROFILE_ID ?
        profileId : 0;
    if(!loadCombatAiProgram(archiveDirectory, initialized.profileId,
                            &initialized.program, error))
        return false;
    resetLgpAiVmState(&initialized.vm, random);
    initialized.initialized = !initialized.vm.failed;
    *runtime = std::move(initialized);
    if(error != NULL)
        error->clear();
    return runtime->initialized;
}

void
resetCombatAiRuntime(CombatAiRuntime* runtime, LgpAiRandomSource random)
{
    if(runtime == NULL)
        return;
    resetLgpAiVmState(&runtime->vm, random);
    runtime->initialized = random.draw != NULL;
}

bool
updateCombatAiRuntime(CombatAiRuntime* runtime,
                      const CombatAiUpdateInput& input,
                      CombatAiDecision* decision,
                      std::string* error)
{
    if(decision == NULL)
        return fail(error, "combat AI decision output is null");
    *decision = {};
    decision->selectedRecord = -1;
    if(runtime == NULL || !runtime->initialized ||
       runtime->vm.random.draw == NULL ||
       !finitePosition(input.selfPosition) ||
       !finitePosition(input.targetPosition))
        return fail(error, "combat AI update input is invalid");

    LgpAiVmState& vm = runtime->vm;
    vm.failed = false;

    vm.bitState =
        (vm.bitState & ~AI_SENSOR_BIT_MASK) |
        (input.conditionBits & AI_SENSOR_BIT_MASK);

    vm.sourceParam[AI_SOURCE_SLOT] = input.slotIndex;
    vm.sourceParam[AI_SOURCE_SELF_STATE] = input.selfState;
    vm.sourceParam[AI_SOURCE_SELF_HEALTH] =
        percentage(input.selfHealth, input.selfMaxHealth);
    vm.sourceParam[AI_SOURCE_SELF_DAMAGE] =
        percentage(input.selfDamageGauge, input.selfDamageGaugeMaximum);
    vm.sourceParam[AI_SOURCE_TARGET_STATE] = input.targetState;
    vm.sourceParam[AI_SOURCE_TARGET_HEALTH] =
        percentage(input.targetHealth, input.targetMaxHealth);
    if(input.targetState != 4 &&
       vm.sourceParam[AI_SOURCE_TARGET_HEALTH] < 1)
        vm.sourceParam[AI_SOURCE_TARGET_HEALTH] = 1;

    const float deltaX = input.targetPosition.x - input.selfPosition.x;
    const float deltaY = input.targetPosition.y - input.selfPosition.y;
    const float deltaZ = input.targetPosition.z - input.selfPosition.z;
    const float distance = std::sqrt(
        deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    vm.sourceParam[AI_SOURCE_DISTANCE] =
        input.targetState == 4 &&
        vm.sourceParam[AI_SOURCE_TARGET_HEALTH] == 0 ?
        0 : static_cast<std::int16_t>(distance);
    vm.sourceParam[AI_SOURCE_DIFFICULTY] =
        combatAiDifficultyDecisionValue(input.difficulty);

    std::int32_t comparison =
        vm.sourceParam[AI_SOURCE_SELF_HEALTH] +
        vm.sourceParam[AI_SOURCE_DIFFICULTY] / 2;
    const std::int32_t healthDifference =
        vm.sourceParam[AI_SOURCE_TARGET_HEALTH] -
        vm.sourceParam[AI_SOURCE_SELF_HEALTH];
    if(healthDifference >= 50)
        comparison -= 50;
    else if(healthDifference <= -50)
        comparison += 50;
    vm.sourceParam[AI_SOURCE_COMPARISON] = clampPercent(comparison);
    vm.sourceParam[AI_SOURCE_TARGET_ATTACK] = input.targetAttackClass;
    vm.sourceParam[AI_SOURCE_TARGET_DIRECTION] = input.targetDirection;
    vm.sourceParam[AI_SOURCE_SELF_DIRECTION] = input.selfDirection;
    vm.sourceParam[AI_SOURCE_RESERVED] = 0;

    std::int16_t selectedRecord = -1;
    if((vm.bitState & AI_RECORD_ACTIVE) == 0)
    {
        if(!resolveLgpAiChain(runtime->program, &vm,
                             runtime->program.header.primaryChainIndex,
                             &selectedRecord))
            return fail(error, "combat AI primary chain resolution failed");
    }
    else
    {
        selectedRecord = vm.sourceParam[AI_SOURCE_RECORD];
        if((vm.bitState & AI_RECORD_INTERRUPTED) == 0)
        {
            std::int16_t interruptRecord = -1;
            if(!resolveLgpAiChain(runtime->program, &vm,
                                 runtime->program.header.alternateChainIndex,
                                 &interruptRecord))
                return fail(error, "combat AI interrupt chain resolution failed");
            if(interruptRecord != -1)
            {
                selectedRecord = interruptRecord;
                vm.bitState &= ~AI_RECORD_ACTIVE;
                vm.bitState |= AI_RECORD_INTERRUPTED;
            }
        }
    }
    if(vm.failed)
        return fail(error, "combat AI chain consumed an invalid random source");

    std::uint16_t frameInput = 0;
    std::array<float, 4> frameChannels = {};
    if(selectedRecord >= 0)
    {
        if(static_cast<std::size_t>(selectedRecord) >=
           runtime->program.sectionA.size())
            return fail(error, "combat AI selected an invalid A record");

        if((vm.bitState & AI_RECORD_ACTIVE) == 0)
        {
            vm.bitState |= AI_RECORD_ACTIVE;
            vm.sourceParam[AI_SOURCE_COMMAND] = 0;
            vm.sourceParam[AI_SOURCE_WAIT] = 0;
        }
        std::uint16_t commandIndex = static_cast<std::uint16_t>(
            vm.sourceParam[AI_SOURCE_COMMAND]);
        std::uint16_t waitTicks = static_cast<std::uint16_t>(
            vm.sourceParam[AI_SOURCE_WAIT]);
        vm.actionId = vm.sourceParam[AI_SOURCE_LOGICAL_INPUT];
        LgpAiApplyResult applied = {};
        applied.actionId = vm.actionId;
        applied.channels = vm.channels;
        if(!applyLgpAiRecord(runtime->program.sectionA[selectedRecord], &vm,
                             &commandIndex, &waitTicks, &applied))
            return fail(error, "combat AI A record application failed");
        vm.sourceParam[AI_SOURCE_COMMAND] =
            static_cast<std::int16_t>(commandIndex);
        vm.sourceParam[AI_SOURCE_WAIT] =
            static_cast<std::int16_t>(waitTicks);
        vm.sourceParam[AI_SOURCE_LOGICAL_INPUT] = applied.actionId;
        vm.channels = applied.channels;
        frameInput = static_cast<std::uint16_t>(applied.actionId);
        frameChannels = applied.channels;

        if(vm.sourceParam[AI_SOURCE_RECORD] != selectedRecord)
            vm.sourceParam[AI_SOURCE_WATCHDOG] = 0;
        vm.sourceParam[AI_SOURCE_WATCHDOG] =
            static_cast<std::int16_t>(
                vm.sourceParam[AI_SOURCE_WATCHDOG] + 1);
        if(vm.sourceParam[AI_SOURCE_WATCHDOG] >
           AI_RECORD_WATCHDOG_FRAMES)
        {
            vm.bitState &= ~(AI_RECORD_ACTIVE | AI_RECORD_INTERRUPTED);
            vm.sourceParam[AI_SOURCE_WATCHDOG] = 0;
        }
        vm.sourceParam[AI_SOURCE_RECORD] = selectedRecord;
    }

    decision->selectedRecord = selectedRecord;
    decision->logicalInput = frameInput;
    decision->channels = frameChannels;
    if((frameInput & AI_INPUT_GUARD) != 0)
        decision->action = COMBAT_AI_ACTION_GUARD;
    else if((frameInput & AI_INPUT_GRAB) != 0)
        decision->action = COMBAT_AI_ACTION_GRAB;
    else if((frameInput & AI_INPUT_ATTACK_1) != 0)
        decision->action = COMBAT_AI_ACTION_ATTACK_1;
    else if((frameInput & AI_INPUT_ATTACK_0) != 0)
        decision->action = COMBAT_AI_ACTION_ATTACK_0;
    else if((frameInput & (AI_INPUT_EVADE_0 | AI_INPUT_EVADE_1)) != 0)
        decision->action = COMBAT_AI_ACTION_EVADE;
    else if((frameInput & (AI_INPUT_UNMAPPED_0 | AI_INPUT_UNMAPPED_1)) != 0)
        decision->action = COMBAT_AI_ACTION_UNMAPPED;

    decision->exactMoveUnavailable =
        decision->action != COMBAT_AI_ACTION_NONE;

    const std::uint16_t directions =
        frameInput & (AI_DIRECTION_AWAY | AI_DIRECTION_TOWARD |
                      AI_DIRECTION_LEFT | AI_DIRECTION_RIGHT);
    const float horizontalLength =
        std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    bool transformedChannels = false;
    if((frameInput & AI_INPUT_RUN_TO_TARGET) != 0)
    {
        decision->running = true;
        decision->moving = true;
        if(std::fabs(deltaY) >= AI_APPROACH_VERTICAL_LIMIT)
        {
            decision->moving = false;
            decision->verticalSeparation = true;
        }
        else if((directions & AI_DIRECTION_TOWARD) != 0)
            decision->movementAngleDegrees = AI_APPROACH_NARROW_ANGLE;
        else if((directions & AI_DIRECTION_AWAY) != 0)
            decision->movementAngleDegrees = -AI_APPROACH_NARROW_ANGLE;
        else if((directions & AI_DIRECTION_RIGHT) != 0)
            decision->movementAngleDegrees = AI_APPROACH_WIDE_ANGLE;
        else if((directions & AI_DIRECTION_LEFT) != 0)
            decision->movementAngleDegrees = -AI_APPROACH_WIDE_ANGLE;
    }
    else if(std::max(std::fabs(frameChannels[0]),
                     std::fabs(frameChannels[1])) >
            AI_CHANNEL_MOVEMENT_THRESHOLD)
    {

        if(horizontalLength > 0.0001f)
        {
            const float forwardX = deltaX / horizontalLength;
            const float forwardZ = deltaZ / horizontalLength;
            decision->movementDirection.x =
                -frameChannels[0] * forwardX -
                frameChannels[1] * forwardZ;
            decision->movementDirection.y = 0.0f;
            decision->movementDirection.z =
                -frameChannels[0] * forwardZ +
                frameChannels[1] * forwardX;
            decision->moving = true;
            transformedChannels = true;
        }
    }
    else if(directions != 0 && decision->action == COMBAT_AI_ACTION_NONE)
    {
        decision->moving = true;
        if((directions & AI_DIRECTION_TOWARD) != 0)
            decision->movementAngleDegrees = 0.0f;
        else if((directions & AI_DIRECTION_AWAY) != 0)
            decision->movementAngleDegrees = 180.0f;
        else if((directions & AI_DIRECTION_LEFT) != 0)
            decision->movementAngleDegrees = -90.0f;
        else
            decision->movementAngleDegrees = 90.0f;
    }

    if(decision->moving && !transformedChannels)
    {
        if(horizontalLength <= 0.0001f)
            decision->moving = false;
        else
        {
            const float radians =
                decision->movementAngleDegrees * 0.01745329251994329577f;
            const float forwardX = deltaX / horizontalLength;
            const float forwardZ = deltaZ / horizontalLength;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            decision->movementDirection.x =
                forwardX * cosine + forwardZ * sine;
            decision->movementDirection.y = 0.0f;
            decision->movementDirection.z =
                forwardZ * cosine - forwardX * sine;
        }
    }
    if(error != NULL)
        error->clear();
    return true;
}

void
releaseStageAiContext(StageAiAsyncContext* context)
{
    if(context == NULL)
        return;
    const std::uint32_t nextLifetime = context->lifetime + 1;
    *context = {};
    context->state = STAGE_AI_CONTEXT_EMPTY;
    context->lifetime = nextLifetime;
}

bool
requestStageAiContext(StageAiAsyncContext* context,
                      std::uint16_t profileId,
                      std::uint8_t playerIndex,
                      std::uint8_t sourceSlotIndex,
                      bool parameterZero,
                      std::uint8_t difficulty,
                      LgpAiRandomSource random)
{
    if(context == NULL || playerIndex >= STAGE_AI_CONTEXT_COUNT ||
       random.draw == NULL)
        return false;
    releaseStageAiContext(context);
    context->state = STAGE_AI_CONTEXT_REQUESTED;
    context->playerIndex = playerIndex;
    context->sourceSlotIndex = sourceSlotIndex;
    context->profileId = profileId <= COMBAT_AI_MAX_PROFILE_ID ?
        static_cast<std::uint8_t>(profileId) : 0;
    context->resourceId = static_cast<std::uint16_t>(
        context->profileId + STAGE_AI_FSTAGE_RESOURCE_BASE);
    context->parameterZero = parameterZero;
    context->decisionValue = combatAiDifficultyDecisionValue(difficulty);
    context->runtime.profileId = context->profileId;
    resetLgpAiVmState(&context->runtime.vm, random);
    context->runtime.initialized = false;
    return true;
}

bool
completeStageAiContext(StageAiAsyncContext* context,
                       const LgpStageAiProgram& program,
                       LgpAiRandomSource random,
                       std::string* error)
{
    if(context == NULL || context->state != STAGE_AI_CONTEXT_REQUESTED ||
       random.draw == NULL)
        return fail(error, "combat AI completion has no pending request");
    context->runtime.program = program;
    context->runtime.profileId = context->profileId;
    resetLgpAiVmState(&context->runtime.vm, random);
    if(context->runtime.vm.failed)
        return fail(error, "combat AI completion has no random source");
    context->runtime.initialized = true;
    context->state = STAGE_AI_CONTEXT_READY;
    if(error != NULL)
        error->clear();
    return true;
}

bool
completeStageAiContextLoad(StageAiAsyncContext* context,
                           const char* archiveDirectory,
                           LgpAiRandomSource random,
                           std::string* error)
{
    if(context == NULL || context->state != STAGE_AI_CONTEXT_REQUESTED ||
       archiveDirectory == NULL)
        return fail(error, "combat AI load has no pending request");
    LgpStageAiProgram program;
    if(!loadCombatAiProgram(archiveDirectory, context->profileId,
                            &program, error))
        return false;
    return completeStageAiContext(context, program, random, error);
}

bool
updateStageAiContext(StageAiAsyncContext* context,
                     const StageAiTickGate& gate,
                     const CombatAiUpdateInput& input,
                     CombatAiDecision* decision,
                     std::string* error)
{
    if(decision == NULL)
        return fail(error, "combat AI decision output is null");
    *decision = {};
    decision->selectedRecord = -1;
    if(context == NULL)
        return fail(error, "combat AI context is null");
    if(!gate.occupied || !gate.battleActive ||
       !gate.scaledDeltaNonzero || !gate.cpuControlled ||
       gate.specialInputOverride ||
       context->state != STAGE_AI_CONTEXT_READY)
    {
        if(error != NULL)
            error->clear();
        return true;
    }
    return updateCombatAiRuntime(
        &context->runtime, input, decision, error);
}

bool
stageAiDecisionIsNeutral(const CombatAiDecision& decision)
{
    if(decision.action != COMBAT_AI_ACTION_NONE ||
       decision.selectedRecord != -1 || decision.logicalInput != 0 ||
       decision.moving || decision.running || decision.verticalSeparation ||
       decision.exactMoveUnavailable)
        return false;
    for(float channel : decision.channels)
        if(channel != 0.0f)
            return false;
    return decision.movementDirection.x == 0.0f &&
           decision.movementDirection.y == 0.0f &&
           decision.movementDirection.z == 0.0f &&
           decision.movementAngleDegrees == 0.0f;
}

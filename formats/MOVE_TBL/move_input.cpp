#include "move_input.h"
#include <string.h>
MoveInputHistory::MoveInputHistory()
: count_(0)
{
    memset(codes_, 0, sizeof(codes_));
    memset(ages_, 0, sizeof(ages_));
}
void
MoveInputHistory::reset(void)
{
    count_ = 0;
    memset(codes_, 0, sizeof(codes_));
    memset(ages_, 0, sizeof(ages_));
}
void
MoveInputHistory::update(uint32_t code, float deltaSeconds)
{
    if(count_ > 0)
        ages_[0] += deltaSeconds;
    if(count_ > 0 && codes_[0] == code)
        return;

    int last = count_;
    if(last >= MOVE_INPUT_HISTORY_CAPACITY)
        last = MOVE_INPUT_HISTORY_CAPACITY - 1;
    for(int i = last; i > 0; --i)
    {
        codes_[i] = codes_[i - 1];
        ages_[i] = ages_[i - 1];
    }
    codes_[0] = code;
    ages_[0] = 0.0f;
    if(count_ < MOVE_INPUT_HISTORY_CAPACITY)
        ++count_;
}
int
MoveInputHistory::count(void) const
{
    return count_;
}
uint32_t
MoveInputHistory::code(int index) const
{
    if(index < 0 || index >= count_)
        return 0;
    return codes_[index];
}
float
MoveInputHistory::age(int index) const
{
    if(index < 0 || index >= count_)
        return 0.0f;
    return ages_[index];
}

const uint32_t MOVE_INPUT_SLOT_MASKS[MOVE_INPUT_SLOT_COUNT] =
{
    0x0100u, 0x0040u, 0x1000u, 0x0080u, 0x8000u,
    0x1000u, 0x0200u, 0x0800u, 0x2000u
};
uint32_t
moveInputEdge(uint32_t step, uint32_t current, uint32_t previous)
{
    if((step & MOVE_INPUT_ON_RELEASE) != 0)
        return ~current & previous;
    if((step & MOVE_INPUT_WHILE_HELD) != 0)
        return current;
    return current & ~previous;
}
uint32_t
moveInputStepMask(uint32_t step, uint32_t relation, uint32_t mirrored)
{
    uint32_t required = 0;

    if((step & MOVE_INPUT_DIR_CURRENT) != 0)
        required = relation;
    else if((step & MOVE_INPUT_DIR_MIRRORED) != 0)
    {
        if((relation & MOVE_INPUT_RELATION_AWAY) != 0)
            required |= MOVE_INPUT_RELATION_TOWARD;
        if((relation & MOVE_INPUT_RELATION_TOWARD) != 0)
            required |= MOVE_INPUT_RELATION_AWAY;
        if((relation & 0x4u) != 0)
            required |= 0x8u;
        if((relation & 0x8u) != 0)
            required |= 0x4u;
    }
    else if((step & MOVE_INPUT_DIR_LEFT) != 0)
        required = 0x4;
    else if((step & MOVE_INPUT_DIR_RIGHT) != 0)
        required = 0x8;
    else if((step & MOVE_INPUT_DIR_SWAP_LR) != 0)
    {
        if((mirrored & 0x1) != 0)
            required |= 0x4;
        if((mirrored & 0x2) != 0)
            required |= 0x8;
        if((mirrored & 0x4) != 0)
            required |= 0x2;
        if((mirrored & 0x8) != 0)
            required |= 0x1;
    }
    else if((step & MOVE_INPUT_DIR_SWAP_UD) != 0)
    {
        if((mirrored & 0x1) != 0)
            required |= 0x8;
        if((mirrored & 0x2) != 0)
            required |= 0x4;
        if((mirrored & 0x4) != 0)
            required |= 0x1;
        if((mirrored & 0x8) != 0)
            required |= 0x2;
    }

    for(int slot = 0; slot < MOVE_INPUT_SLOT_COUNT; ++slot)
    {
        if((step & (1u << slot)) != 0)
            required |= MOVE_INPUT_SLOT_MASKS[slot];
    }
    if((step & MOVE_INPUT_ANY_DIRECTION) != 0)
        required |= 0xf;
    if((step & MOVE_INPUT_RIGHT_STICK_UP) != 0) required |= 0x100000u;
    else if((step & MOVE_INPUT_RIGHT_STICK_DOWN) != 0) required |= 0x200000u;
    else if((step & MOVE_INPUT_RIGHT_STICK_RIGHT) != 0) required |= 0x400000u;
    else if((step & MOVE_INPUT_RIGHT_STICK_LEFT) != 0) required |= 0x800000u;
    else if((step & MOVE_INPUT_RIGHT_STICK_ANY) != 0) required |= 0xf00000u;
    return required;
}
int
moveInputRecognise(const MoveInputHistory& history, uint32_t facing,
                   uint32_t mirrored, uint32_t actorBehaviour,
                   uint32_t linkedBehaviour, uint32_t actorField0x4390,
                   uint32_t linkedField0x4390, uint32_t actorTargeting,
                   uint32_t linkedTargeting, int edgeMode)
{

    uint32_t collectedDirections = 0;
    for(int index = 1; index < MOVE_INPUT_COMMAND_COUNT; ++index)
    {
        if(index == 0x0f || index == 0x1d)
            continue;
        const MoveInputCommand& command = MOVE_INPUT_COMMANDS[index];
        const uint32_t firstStep = MOVE_INPUT_STEPS[command.firstStep];
        const bool release = (firstStep & MOVE_INPUT_ON_RELEASE) != 0;
        if((edgeMode >= 0 && edgeMode != 1 && release) ||
           (edgeMode == 1 && !release) ||
           (edgeMode == 0 && firstStep == 0) ||
           (edgeMode == 2 && (release ||
                ((firstStep & (1u << 3)) == 0 && index != 1))))
            continue;
        if(!moveInputCommandEligible(command, actorBehaviour, linkedBehaviour,
                                     actorField0x4390, linkedField0x4390,
                                     actorTargeting, linkedTargeting))
            continue;
        const float window = command.windowMilliseconds * 0.001f;
        float elapsed = edgeMode == 2 ? window - 0.001f : history.age(0);
        int step = 0;
        int entry = 0;
        while(step < command.stepCount && entry < history.count())
        {
            if(elapsed >= window)
                break;
            const uint32_t current = history.code(entry);
            const uint32_t previous = history.code(entry + 1);
            uint32_t seen = moveInputEdge(
                MOVE_INPUT_STEPS[command.firstStep + step], current, previous);

            int available = 0;
            for(uint32_t mask : MOVE_INPUT_DIRECTION_MASKS)
                available += (seen & mask) != 0;
            for(uint32_t mask : MOVE_INPUT_SLOT_MASKS)
                available += (seen & mask) != 0;
            for(uint32_t mask = 0x100000u; mask <= 0x800000u; mask <<= 1)
                available += (seen & mask) != 0;
            bool abandoned = false;
            for(int token = 0; token < available && step < command.stepCount; ++token)
            {
                const uint32_t word = MOVE_INPUT_STEPS[command.firstStep + step];
                const uint32_t required = moveInputStepMask(word, facing, mirrored);
                if(required == 0xfu)
                {

                    for(uint32_t mask : MOVE_INPUT_DIRECTION_MASKS)
                    {
                        if((seen & mask) != 0 && (collectedDirections & mask) == 0)
                        {
                            collectedDirections |= mask;
                            break;
                        }
                    }
                    if(collectedDirections == 0xfu)
                    {
                        collectedDirections = 0;
                        ++step;
                    }
                    break;
                }
                if((seen & required) == required)
                {
                    ++step;
                    seen &= ~required;
                }
                else if((seen & MOVE_INPUT_ABANDON_MASK) != 0)
                {
                    abandoned = true;
                    break;
                }
            }

            if(abandoned || (entry == 0 && step == 0))
                break;
            ++entry;
            elapsed += history.age(entry);
        }
        if(step == command.stepCount)
            return index;
    }
    return -1;
}

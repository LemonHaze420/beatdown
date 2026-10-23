#include "move_chains.h"

int moveActionChain(const MoveActionChainFamily& family,
                     const int16_t* actions, int count,
                     const MovePersonalActionContext& context)
{
    if(actions == nullptr || count <= 0 || count > MOVE_ACTION_CHAIN_CAPACITY)
        return 0;
    for(int i = 0; i < family.rowCount; ++i)
    {
        const MoveActionChainRow& row =
            MOVE_ACTION_CHAIN_ROWS[family.firstRow + i];
        if(row.stepCount != count)
            continue;
        const MoveActionChainStep* steps = &MOVE_ACTION_CHAIN_STEPS[row.firstStep];
        bool matched = true;
        for(int j = 0; j < count; ++j)
            if(steps[j].action != actions[j])
            {
                matched = false;
                break;
            }
        if(matched && movePersonalActionEligible(
               row.requirements,
               &MOVE_ACTION_CHAIN_CONDITIONS[row.firstCondition],
               row.conditionCount, context))
            return steps[count - 1].move;
    }
    return 0;
}

int moveInitialActionChain(const MoveActionChainFamily& family, int action,
                           const MovePersonalActionContext& context,
                           int* matchedAction)
{
    if(matchedAction != nullptr)
        *matchedAction = -1;
    for(int depth = 0; action >= 0 && depth < MOVE_ACTION_COUNT;
        action = moveActionParent(action), ++depth)
    {
        const int16_t input = static_cast<int16_t>(action);
        const int move = moveActionChain(family, &input, 1, context);
        if(move != 0)
        {
            if(matchedAction != nullptr)
                *matchedAction = action;
            return move;
        }
    }
    return 0;
}

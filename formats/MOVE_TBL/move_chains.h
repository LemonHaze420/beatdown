#ifndef MOVE_CHAINS_H
#define MOVE_CHAINS_H

#include "move_tables.h"

const int MOVE_ACTION_CHAIN_CAPACITY = 64;
const int MOVE_WEAPON_CHAIN_FAMILY_COUNT = 28;

struct MoveActionChainStep
{
    int16_t action;
    int16_t move;
};

struct MoveActionChainRow
{
    uint16_t firstStep;
    uint8_t stepCount;
    uint16_t firstCondition;
    uint8_t conditionCount;
    uint8_t requirements[MOVE_LIST_REQUIREMENT_COUNT];
};

struct MoveActionChainFamily
{
    uint16_t firstRow;
    uint16_t rowCount;
};

extern const MoveActionChainStep MOVE_ACTION_CHAIN_STEPS[];
extern const MovePersonalCondition MOVE_ACTION_CHAIN_CONDITIONS[];
extern const MoveActionChainRow MOVE_ACTION_CHAIN_ROWS[];
extern const MoveActionChainFamily MOVE_PERSONAL_CHAIN_FAMILIES[37];
extern const MoveActionChainFamily MOVE_WEAPON_CHAIN_FAMILIES[28];

int moveActionChain(const MoveActionChainFamily& family,
                     const int16_t* actions, int count,
                     const MovePersonalActionContext& context);

int moveInitialActionChain(const MoveActionChainFamily& family, int action,
                           const MovePersonalActionContext& context,
                           int* matchedAction = nullptr);

#endif

#ifndef MOVE_INPUT_H
#define MOVE_INPUT_H
#include "move_tables.h"

const uint32_t MOVE_INPUT_ABANDON_MASK = 0xfbc0u;

const uint32_t MOVE_INPUT_RELATION_AWAY = 0x1;
const uint32_t MOVE_INPUT_RELATION_TOWARD = 0x2;
const uint32_t GAMEPAD_CODE_DPAD_UP = 0x0040;
const uint32_t GAMEPAD_CODE_DPAD_DOWN = 0x0080;
const uint32_t GAMEPAD_CODE_DPAD_LEFT = 0x0100;
const uint32_t GAMEPAD_CODE_DPAD_RIGHT = 0x0200;
const uint32_t GAMEPAD_CODE_SELECT = 0x0800;
const uint32_t GAMEPAD_CODE_START = 0x1000;
const uint32_t GAMEPAD_CODE_L3 = 0x2000;
const uint32_t GAMEPAD_CODE_R2 = 0x4000;
const uint32_t GAMEPAD_CODE_L1 = 0x8000;
const int MOVE_INPUT_SLOT_COUNT = 9;
extern const uint32_t MOVE_INPUT_SLOT_MASKS[MOVE_INPUT_SLOT_COUNT];

const int MOVE_INPUT_HISTORY_CAPACITY = 128;
class MoveInputHistory
{
public:
    MoveInputHistory();
    void reset();

    void update(uint32_t code, float deltaSeconds);
    int count(void) const;

    uint32_t code(int index) const;
    float age(int index) const;
private:
    uint32_t codes_[MOVE_INPUT_HISTORY_CAPACITY];
    float ages_[MOVE_INPUT_HISTORY_CAPACITY];
    int count_;
};

inline constexpr uint32_t MOVE_INPUT_DIRECTION_MASKS[4] = { 4u, 8u, 1u, 2u };

uint32_t moveInputEdge(uint32_t step, uint32_t current, uint32_t previous);

uint32_t moveInputStepMask(uint32_t step, uint32_t relation,
                           uint32_t mirrored);

int moveInputRecognise(const MoveInputHistory& history, uint32_t facing,
                       uint32_t mirrored, uint32_t actorBehaviour,
                       uint32_t linkedBehaviour, uint32_t actorField0x4390,
                       uint32_t linkedField0x4390, uint32_t actorTargeting,
                       uint32_t linkedTargeting, int edgeMode = -1);
#endif

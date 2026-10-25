#include "game_random.h"

namespace
{

const int32_t SEED_MULTIPLIER = 1103515245;
const int32_t SEED_INCREMENT = 12345;
const int DISCARDED_OUTPUTS = 310;
const int FRONT_WORD = 3;
const int REAR_WORD = 0;

}

void
seedGameRandom(GameRandom& random, uint32_t seed)
{
    random.state[0] = static_cast<int32_t>(seed);
    for(int i = 1; i < GAME_RANDOM_STATE_WORDS; ++i)
    {

        const uint32_t previous = static_cast<uint32_t>(random.state[i - 1]);
        const uint32_t next =
            previous * static_cast<uint32_t>(SEED_MULTIPLIER) +
            static_cast<uint32_t>(SEED_INCREMENT);
        random.state[i] = static_cast<int32_t>(next);
    }
    random.front = FRONT_WORD;
    random.rear = REAR_WORD;
    for(int i = 0; i < DISCARDED_OUTPUTS; ++i)
        nextGameRandom(random);
}

uint32_t
nextGameRandom(GameRandom& random)
{
    const uint32_t sum =
        static_cast<uint32_t>(random.state[random.front]) +
        static_cast<uint32_t>(random.state[random.rear]);
    random.state[random.front] = static_cast<int32_t>(sum);
    const uint32_t result = sum >> 1;

    if(++random.front >= GAME_RANDOM_STATE_WORDS)
        random.front = 0;
    if(++random.rear >= GAME_RANDOM_STATE_WORDS)
        random.rear = 0;
    return result;
}

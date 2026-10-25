#ifndef GAME_RANDOM_H
#define GAME_RANDOM_H

#include <stdint.h>

const int GAME_RANDOM_STATE_WORDS = 31;

struct GameRandom
{
    int32_t state[GAME_RANDOM_STATE_WORDS];
    int front;
    int rear;
};

void seedGameRandom(GameRandom& random, uint32_t seed);

uint32_t nextGameRandom(GameRandom& random);

#endif

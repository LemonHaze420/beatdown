#include "game.h"

void Game::setBattleResourceCallbacks(const BattleResourceCallbacks& callbacks)
{
    battleResourceCallbacks = callbacks;
}

int Game::battleMotionResource(int motionId, int* positionInBundle) const
{
    return battleResourceForMotion(battleResources, motionId, positionInBundle);
}

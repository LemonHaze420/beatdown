#ifndef RWFIGHTTRIBE_STAGE_INTERACTIONS_H
#define RWFIGHTTRIBE_STAGE_INTERACTIONS_H

#include "stage.h"

class Game;

bool installStageInteractions(
    const char* archivePath, Game& game, FstageNpcPositionData& positions,
    uint16_t stageId, FstageScene* scene,
    const FstageObjectSpawnState* objectState = NULL,
    FstageObjectInstallResult* objectResult = NULL,
    size_t* selectedRowCount = NULL);

#endif

#ifndef COMBAT_STYLES_H
#define COMBAT_STYLES_H
#include <stdint.h>

const int COMBAT_STYLE_COUNT = 53;

const int COMBAT_STYLE_UNKNOWN = 0;

const int COMBAT_STYLE_ID_LIMIT = 0x33;
extern const char* const COMBAT_STYLE_NAMES[COMBAT_STYLE_COUNT];

const char* combatStyleName(int combatStyleId);
#endif

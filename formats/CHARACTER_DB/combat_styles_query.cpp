#include "combat_styles.h"
const char*
combatStyleName(int combatStyleId)
{
    if(combatStyleId < 0 || combatStyleId >= COMBAT_STYLE_ID_LIMIT)
        return COMBAT_STYLE_NAMES[COMBAT_STYLE_UNKNOWN];
    const int index = combatStyleId + 1;
    if(index >= COMBAT_STYLE_COUNT)
        return COMBAT_STYLE_NAMES[COMBAT_STYLE_UNKNOWN];
    return COMBAT_STYLE_NAMES[index];
}

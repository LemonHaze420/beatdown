#ifndef REIMPL_MENU_SYSTEM_H
#define REIMPL_MENU_SYSTEM_H

#include <stdint.h>

#include "formats/MENU_LAYOUT/title_select_artwork.h"

class Game;
struct CreditsSequenceState;

void drawTitleArtwork(float scale, uint8_t alpha);
void updateMenuSystemPresentation(float deltaSeconds);
void drawTitleMenuScreen(void);
void drawCharacterSelectScreen(void);
void releaseMenuSystemArtwork(void);
bool prepareLocalizedInfoFamily(uint8_t language, int family);
void drawLocalizedInfoPresentation(const Game& game);
void drawBattleEndNotice(const Game& game);
bool drawMemoryCardArtwork(uint8_t language);
void drawCreditsSequenceScreen(const CreditsSequenceState& state);
void releaseCreditsArtwork(void);

TitleSelectColour titleMenuSelectedColour(int pulse);
uint8_t selectModeHighlightLevel(int frame);
void drawLoadingScreen(void);
void drawVideoModeScreen(void);
void drawNoSaveDataScreen(void);
void drawVersusTypeScreen(void);
void drawResultsScreen(void);

#endif

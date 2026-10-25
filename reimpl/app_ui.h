#ifndef APP_UI_H
#define APP_UI_H
#include <stdint.h>

void uiCenterText(const char* text);

void uiDrawMenuItem(const char* label, bool selected, bool enabled);

void uiDrawCommandPrompt(bool backAvailable);

bool uiCharacterCustomizationReady(uint64_t fingerprint);

void uiDrawPauseFallback(void);

void uiDrawOptionsFallback(void);

#endif

#ifndef REIMPL_HUD_PROMPT_H
#define REIMPL_HUD_PROMPT_H

#include <stdint.h>

#include "../formats/HUD_PROMPT/hud_prompt_layout.h"

bool hudPromptLoad(void);
void hudPromptRelease(void);

void hudDrawPanel(const HudPanelRecord& panel);

void hudDrawBottomBand(void);

void hudPromptClear(void);

bool hudPromptAdd(float x, float y, HudPromptKind kind, const char* text);

bool hudPromptAddTextId(float x, float y, HudPromptKind kind, uint16_t textId);

void hudPromptSetPlainText(float x, float y, const char* text, float scaleX,
                           float scaleY, uint8_t r, uint8_t g, uint8_t b,
                           uint8_t a);

void hudPromptSelectContext(int context);

void hudPromptSetGate(uint8_t suppress, uint8_t enable);

void hudPromptSetSlotBlink(HudPromptKind kind, uint8_t blink);
void hudPromptUpdate(float deltaSeconds);

void hudPromptDraw(void);

void hudPromptFlush(void);

void hudDrawBattlePromptLine(void);

void hudDrawBottomLine(int bodyRow);

void hudDrawPromptText(float x, float y, const char* folded, float scaleX,
                       float scaleY, uint8_t r, uint8_t g, uint8_t b,
                       uint8_t a, bool rightAlign);

float hudMeasurePromptText(const char* folded, float scaleX, float scaleY);

float hudPromptLineHeight(float scaleY);

#endif

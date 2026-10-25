#ifndef REIMPL_MENU_FONT_H
#define REIMPL_MENU_FONT_H

#include <stdint.h>

#include "formats/MENU_FONT/menu_font.h"

bool loadMenuFont(void);

void drawMenuText(const char* text, float x, float y,
                  float scaleX, float scaleY,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool shadow);
void drawMenuText(const char* text, float x, float y, float scale,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool shadow);

bool loadRetainedMenuFont(void);
void drawRetainedMenuText(const char* text, float x, float y,
                          float scaleX, float scaleY,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                          bool shadow);
void drawRetainedMenuText(const char* text, float x, float y, float scale,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                          bool shadow);

struct MenuTextExtent
{
    float width;
    float height;
};

MenuTextExtent measureMenuText(const char* text, float scaleX, float scaleY);
float measureMenuText(const char* text, float scale);

MenuTextExtent measureRetainedMenuText(const char* text,
                                       float scaleX, float scaleY);
float measureRetainedMenuText(const char* text, float scale);

#endif

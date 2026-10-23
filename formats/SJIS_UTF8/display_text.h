#ifndef DISPLAY_TEXT_H
#define DISPLAY_TEXT_H
#include <stdint.h>

const int DISPLAY_TEXT_FOLD_COUNT = 95;
struct DisplayTextFold
{

    uint16_t sjis;

    uint16_t unicode;

    char ascii;
};
extern const DisplayTextFold DISPLAY_TEXT_FOLD[DISPLAY_TEXT_FOLD_COUNT];
#endif

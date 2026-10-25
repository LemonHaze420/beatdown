#include "scene/caption_view.h"

#include "game/game.h"
#include "ui/menu/font.h"

#include <stdint.h>

namespace
{

const float RETAINED_TEXT_SCALE = 1.0f;
const int CAPTION_ALPHA_MAX = 0xff;

void
drawCaptionLine(const SceneSequenceCaptionLine& line, uint8_t alpha,
                bool shadow)
{
    if(line.text == NULL || line.text[0] == 0)
        return;

    const MenuTextExtent extent = measureRetainedMenuText(
        line.text, RETAINED_TEXT_SCALE, RETAINED_TEXT_SCALE);
    const float x = line.x - extent.width * 0.5f;
    const float y = line.y - extent.height * 0.5f;

    const uint8_t red = static_cast<uint8_t>((line.color >> 24) & 0xffu);
    const uint8_t green = static_cast<uint8_t>((line.color >> 16) & 0xffu);
    const uint8_t blue = static_cast<uint8_t>((line.color >> 8) & 0xffu);

    drawRetainedMenuText(line.text, x, y, RETAINED_TEXT_SCALE,
                         RETAINED_TEXT_SCALE, red, green, blue, alpha, shadow);
}

}

void
drawSequencerCaption(void)
{
    const SceneSequencer& sequencer = g_game.sceneSequence();
    const SceneSequenceCaption& caption = sequencer.caption();
    if(caption.phase == SCENE_SEQUENCE_CAPTION_OFF ||
       caption.phase == SCENE_SEQUENCE_CAPTION_CLEANUP ||
       caption.lineCount <= 0 || caption.alpha <= 0)
        return;

    const SceneSequenceCaptionConfig& config = sequencer.captionConfig();
    const int count = caption.lineCount < SCENE_SEQUENCE_CAPTION_LINE_COUNT ?
        caption.lineCount : SCENE_SEQUENCE_CAPTION_LINE_COUNT;

    const uint8_t alpha = static_cast<uint8_t>(
        caption.alpha < CAPTION_ALPHA_MAX ? caption.alpha : CAPTION_ALPHA_MAX);
    for(int i = 0; i < count; ++i)
        drawCaptionLine(config.lines[i], alpha, caption.shadow);
}

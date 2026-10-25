#include "app_internal.h"

namespace app_detail
{

void centerText(const char* text)
{
    float width = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - width) * 0.5f);
    ImGui::TextUnformatted(text);
}

void drawMenuItem(const char* label, bool selected, bool enabled)
{
    char rendered[512];
    if(!enabled && selected)
        snprintf(rendered, sizeof(rendered), "[X] > %s <", label);
    else if(!enabled)
        snprintf(rendered, sizeof(rendered), "[X] %s", label);
    else if(selected)
        snprintf(rendered, sizeof(rendered), "> %s <", label);
    else
        snprintf(rendered, sizeof(rendered), "%s", label);

    if(!enabled)
        ImGui::PushStyleColor(ImGuiCol_Text, UI_COLOR_DISABLED);
    else if(selected)
        ImGui::PushStyleColor(ImGuiCol_Text, UI_COLOR_SELECTED);
    centerText(rendered);
    if(!enabled || selected)
        ImGui::PopStyleColor();
}

void drawMenuItem(const char* label, bool selected)
{
    drawMenuItem(label, selected, true);
}

void drawMenuValue(const char* label, const char* value,
                   bool selected, bool enabled)
{
    char text[512];
    snprintf(text, sizeof(text), "%s : %s", label, value);
    drawMenuItem(text, selected, enabled);
}

void drawCommandPrompt(bool backAvailable)
{
    const char* select = presentationText(GAME_TEXT_SELECT, "SELECT");
    const char* back = presentationText(GAME_TEXT_BACK, "BACK");
    char prompt[512];
    if(backAvailable)
    {
        snprintf(prompt, sizeof(prompt),
                 "ENTER / A: %s    ESC / B: %s", select, back);
    }
    else
        snprintf(prompt, sizeof(prompt), "ENTER / A: %s", select);
    ImGui::PushStyleColor(ImGuiCol_Text, UI_COLOR_PROMPT);
    centerText(prompt);
    ImGui::PopStyleColor();
}

const char* optionsRowValue(const OptionEntry& entry, char* buffer, size_t size)
{
    if(!optionEntryHasValue(entry))
        return NULL;
    const int value = g_game.optionValue(entry);
    if(entry.valueFormat == OPTION_VALUE_SIGNED_DECIMAL)
    {
        snprintf(buffer, size, "%d", value);
        return buffer;
    }
    const char* literal = optionValueLiteral(entry, value);
    const uint16_t textId = optionValueTextId(entry, value);
    return textId != 0 ? presentationText(textId, literal) : literal;
}

void drawOptionCommandPrompt(OptionPromptStyle style)
{
    hudPromptClear();
    if(style == OPTION_PROMPT_SELECT_OR_BACK)
    {
        hudPromptAddTextId(63.0f, 452.0f, HUD_PROMPT_DPAD, 0x28a2);
        hudPromptAddTextId(195.0f, 452.0f, HUD_PROMPT_CONFIRM, 0x28d4);
    }
    else
    {
        hudPromptAddTextId(63.0f, 452.0f, HUD_PROMPT_DPAD_LEFT, 0x28a2);
        if(style == OPTION_PROMPT_MOVIE_OR_BACK)
            hudPromptAddTextId(
                195.0f, 452.0f, HUD_PROMPT_CONFIRM, 0x28d4);
    }
    hudPromptAddTextId(459.0f, 452.0f, HUD_PROMPT_CANCEL, 0x28a6);
    hudPromptSetGate(0, 1);
    hudPromptDraw();
}

void drawPauseMenuImGui(void)
{
    const bool battlePause =
        g_game.mode() != GAME_MODE_STORY;
    const int rowCount = battlePause ? BATTLE_PAUSE_ROW_COUNT :
        PAUSE_ROW_COUNT;

    centerText(presentationText(GAME_TEXT_PAUSE, "PAUSE"));
    ImGui::Dummy(ImVec2(0.0f, UI_SPACE_4 * 3.0f));
    for(int i = 0; i < rowCount; i++)
    {
        const uint16_t textId = battlePause ?
            APP_UI_BATTLETEXTIDS[i] : APP_UI_STORYTEXTIDS[i];
        const char* fallback = battlePause ?
            APP_UI_BATTLEFALLBACKS[i] : APP_UI_STORYFALLBACKS[i];
        drawMenuItem(presentationText(textId, fallback),
                     i == g_game.pauseSelection(),
                     g_game.pauseMenuItemEnabled(i));
        ImGui::Dummy(ImVec2(0.0f, UI_SPACE_1));
    }
    ImGui::Dummy(ImVec2(0.0f, UI_SPACE_2));
    drawCommandPrompt(true);
}

void drawOptionsMenu(void)
{
    const int page = static_cast<int>(g_game.optionsPage());
    const int selected = g_game.optionsSelection();
    if(page < 0 || page >= OPTION_PAGE_COUNT ||
       g_ui.systemFont.texture == NULL)
    {
        centerText(presentationText(GAME_TEXT_OPTIONS, "OPTIONS"));
        drawCommandPrompt(true);
        return;
    }

    const OptionPage& current = OPTION_PAGES[page];
    const float scaleX = sk::globals.width / MENU_SCREEN_WIDTH;
    const float scaleY = sk::globals.height / MENU_SCREEN_HEIGHT;
    const bool inGame = g_game.optionsOpenedFromPause();

    if(page == GAME_OPTIONS_PAGE_BRIGHTNESS)
    {
        ImDrawList* list = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();

        for(int i = 0; i < 5; ++i)
        {
            const float left = (80.0f + 96.0f * i) * scaleX;
            const float right = (176.0f + 96.0f * i) * scaleX;
            list->AddRectFilled(
                ImVec2(origin.x + left, origin.y + 100.0f * scaleY),
                ImVec2(origin.x + right, origin.y + 360.0f * scaleY),
                APP_UI_SHADES[i]);
        }
        drawOptionCommandPrompt(OPTION_PROMPT_EDIT_OR_BACK);
        return;
    }

    const uint16_t headingId = inGame ? current.headingInGame :
                                        current.headingTitle;
    const MenuItemColour& heading = STAGE_SELECT_TITLE_COLOUR;
    drawSystemFontText(
        OPTIONS_HEADING_X * scaleX, OPTIONS_HEADING_Y * scaleY,
        presentationText(headingId, "OPTIONS"), true,
        ImVec4(heading.r / 255.0f, heading.g / 255.0f, heading.b / 255.0f,
               heading.a / 255.0f),
        OPTIONS_HEADING_SCALE * scaleX);

    const uint8_t highlight = menuPulseHighlight(g_menuPulse.value);
    char value[32];
    int visibleIndex = 0;
    for(int i = 0; i < current.entryCount; ++i)
    {
        const OptionEntry& entry = current.entries[i];
        if(!g_game.optionsItemVisible(i))
            continue;
        const char* label = entry.textId != 0 ?
            presentationText(entry.textId, entry.literal) : entry.literal;
        const bool selectedRow = i == selected;
        const ImVec4 tint = selectedRow ?
            ImVec4(highlight / 255.0f, highlight / 255.0f,
                   highlight / 255.0f, 1.0f) :
            ImVec4(STAGE_SELECT_ROW_COLOUR.r / 255.0f,
                   STAGE_SELECT_ROW_COLOUR.g / 255.0f,
                   STAGE_SELECT_ROW_COLOUR.b / 255.0f,
                   STAGE_SELECT_ROW_COLOUR.a / 255.0f);

        const float y = optionsRowY(visibleIndex++) * scaleY;
        const MenuTextLayout layout = menuTextLayoutDefaults();
        const float labelWidth =
            menuTextWidth(label, layout) * OPTIONS_ROW_SCALE;
        drawSystemFontText(
            (OPTIONS_LABEL_RIGHT_X - labelWidth * 0.5f) * scaleX, y, label,
            true, tint, OPTIONS_ROW_SCALE * scaleX);

        const char* shown = optionsRowValue(entry, value, sizeof(value));
        if(shown != NULL)
        {
            const float valueWidth =
                menuTextWidth(shown, layout) * OPTIONS_ROW_SCALE;
            drawSystemFontText(
                (OPTIONS_LABEL_RIGHT_X + 20.0f + valueWidth * 0.5f) * scaleX,
                y, shown, true, tint, OPTIONS_ROW_SCALE * scaleX);
        }
    }

    const OptionEntry& selectedEntry = current.entries[selected];
    const uint16_t helpId = optionHelpTextId(selectedEntry, inGame);
    const char* help = presentationText(helpId, NULL);
    if(help != NULL)
        drawSystemFontText(OPTIONS_HEADING_X * scaleX, 410.0f * scaleY, help,
                           true, UI_COLOR_PROMPT, 0.55f * scaleX);
    if(g_game.optionsDefaultConfirmationActive())
    {
        char confirmation[64];
        snprintf(confirmation, sizeof(confirmation), "DEFAULT?   %s",
                 g_game.optionsDefaultConfirmationSelection() != 0 ?
                     "YES" : "NO");
        drawSystemFontText(OPTIONS_HEADING_X * scaleX, 380.0f * scaleY,
                           confirmation, true, UI_COLOR_SELECTED,
                           0.7f * scaleX);
    }
    drawOptionCommandPrompt(selectedEntry.promptStyle);
}

ImU32 eventColor(uint32_t color)
{
    return IM_COL32((color >> 16) & 0xff, (color >> 8) & 0xff,
                    color & 0xff, color >> 24);
}

ImU32 eventColorWithAlpha(uint32_t color, float alpha)
{
    const uint32_t sourceAlpha = color >> 24;
    const uint32_t adjustedAlpha = static_cast<uint32_t>(
        clampPresentation(sourceAlpha * alpha, 0.0f, 255.0f));
    return IM_COL32((color >> 16) & 0xff, (color >> 8) & 0xff,
                    color & 0xff, adjustedAlpha);
}

UiTexture* eventTextureForSprite(const EventVm& event, int cacheIndex,
                                 int32_t source)
{
    if(cacheIndex < 0 || cacheIndex >= EVENT_VM_TEXTURE_SLOT_COUNT)
        return NULL;
    const EventVmTextureState* textureState = event.textureState(source);
    if(textureState == NULL || !textureState->loaded ||
       textureState->resourceId < 0 ||
       static_cast<size_t>(textureState->resourceId) >=
           g_ui.graphEntries.size())
        return NULL;
    const XafEntryInfo& entry =
        g_ui.graphEntries[textureState->resourceId];
    if(!hasExtension(entry.name, ".STX"))
        return NULL;

    EventTextureCacheEntry& cached = g_ui.eventTextures[cacheIndex];
    if(cached.resourceId != textureState->resourceId)
    {
        releaseUiTexture(&cached.asset);
        cached.resourceId = textureState->resourceId;
        loadUiTexture(g_graphPath, entry.name.c_str(), &cached.asset);
    }
    return cached.asset.texture != NULL ? &cached.asset : NULL;
}

void drawEventOverlay(void)
{
    const EventVm& event = g_game.eventScript();
    if(!event.loaded())
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float scaleX = sk::globals.width / UI_VIRTUAL_WIDTH;
    const float scaleY = sk::globals.height / UI_VIRTUAL_HEIGHT;
    for(int i = 0; i < EVENT_VM_SPRITE_SLOT_COUNT; ++i)
    {
        const EventVmSpriteState& sprite = event.sprite(i);
        if(!sprite.active || !sprite.visible)
            continue;
        UiTexture* texture =
            eventTextureForSprite(event, i, sprite.textureSource);
        const float sourceWidth = sprite.width != 0 ? fabsf(sprite.width) :
            (texture != NULL ? texture->width : 32.0f);
        const float sourceHeight = sprite.height != 0 ? fabsf(sprite.height) :
            (texture != NULL ? texture->height : 32.0f);
        const float width = sourceWidth * fabsf(sprite.scaleX) * scaleX;
        const float height = sourceHeight * fabsf(sprite.scaleY) * scaleY;
        const ImVec2 center(origin.x + sprite.x * scaleX + width * 0.5f,
                            origin.y + sprite.y * scaleY + height * 0.5f);
        const float radians = sprite.rotation * 0.017453292519943295f;
        const float cosine = cosf(radians);
        const float sine = sinf(radians);
        const float halfWidth = width * 0.5f;
        const float halfHeight = height * 0.5f;
        const float corners[4][2] =
        {
            { -halfWidth, -halfHeight }, { halfWidth, -halfHeight },
            { halfWidth, halfHeight }, { -halfWidth, halfHeight }
        };
        ImVec2 points[4];
        for(int corner = 0; corner < 4; corner++)
        {
            points[corner] = ImVec2(
                center.x + corners[corner][0] * cosine -
                    corners[corner][1] * sine,
                center.y + corners[corner][0] * sine +
                    corners[corner][1] * cosine);
        }
        const ImU32 color = eventColorWithAlpha(
            sprite.color, sprite.transitioning ? 0.65f : 1.0f);
        if(texture != NULL)
        {
            const float leftU = sprite.scaleX < 0.0f ? 1.0f : 0.0f;
            const float rightU = sprite.scaleX < 0.0f ? 0.0f : 1.0f;
            const float topV = sprite.scaleY < 0.0f ? 1.0f : 0.0f;
            const float bottomV = sprite.scaleY < 0.0f ? 0.0f : 1.0f;
            drawList->AddImageQuad(
                uiTextureRef(*texture), points[0], points[1],
                points[2], points[3], ImVec2(leftU, topV),
                ImVec2(rightU, topV), ImVec2(rightU, bottomV),
                ImVec2(leftU, bottomV), color);
        }
        else
        {
            drawList->AddQuadFilled(points[0], points[1],
                                    points[2], points[3], color);
            drawList->AddQuad(points[0], points[1], points[2], points[3],
                              UI_COLOR_INK, 1.0f);
        }
    }

    for(int i = 0; i < EVENT_VM_TEXT_SLOT_COUNT; ++i)
    {
        const EventVmTextState& textState = event.text(i);
        const char* text = event.activeText(i);
        if(text == NULL || !textState.visible)
            continue;
        const float textScaleX = textState.scaleX > 0.0f ?
            textState.scaleX : 1.0f;
        const float textScaleY = textState.scaleY > 0.0f ?
            textState.scaleY : 1.0f;

        std::vector<std::string> lines;
        std::string line;
        const auto lineWidth = [&](const std::string& value)
        {
            return measureRetainedMenuText(
                value.c_str(), textScaleX, 1.0f).width;
        };
        const auto flushLine = [&]()
        {
            lines.push_back(line);
            line.clear();
        };
        const unsigned char* cursor =
            reinterpret_cast<const unsigned char*>(text);
        while(*cursor != 0)
        {
            if(*cursor == '\n' ||
               (cursor[0] == '\\' && cursor[1] == 'n'))
            {
                if(*cursor == '\\')
                    cursor += 2;
                else
                    ++cursor;
                flushLine();
                continue;
            }
            const unsigned char* tokenStart = cursor;
            retainedFontNextCodepoint(cursor);
            line.append(reinterpret_cast<const char*>(tokenStart),
                        static_cast<size_t>(cursor - tokenStart));
        }
        if(!line.empty() || lines.empty())
            flushLine();

        float widest = 0.0f;
        for(size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            const float width = lineWidth(lines[lineIndex]);
            if(width > widest)
                widest = width;
        }
        float x = textState.x;
        if(textState.alignment == 1)
            x -= widest * 0.5f;
        else if(textState.alignment >= 2)
            x -= widest;
        const float lineAdvance = textState.lineAdvance >= 20 ?
            static_cast<float>(textState.lineAdvance) :
            RETAINED_FONT_LINE_PITCH;
        const ImU32 colour = eventColorWithAlpha(
            textState.color, textState.transitioning ? 0.65f : 1.0f);
        const uint8_t red = static_cast<uint8_t>(colour & 0xff);
        const uint8_t green = static_cast<uint8_t>((colour >> 8) & 0xff);
        const uint8_t blue = static_cast<uint8_t>((colour >> 16) & 0xff);
        const uint8_t alpha = static_cast<uint8_t>((colour >> 24) & 0xff);
        for(size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
            drawRetainedMenuText(
                lines[lineIndex].c_str(), x,
                textState.y + static_cast<float>(lineIndex) * lineAdvance,
                textScaleX, textScaleY, red, green, blue, alpha, true);
    }

    if(g_game.pendingEventCall() != EVENT_CALLFUNC_NONE)
    {
        const char* confirm = presentationText(
            GAME_TEXT_SELECT, "CONFIRM");
        const char* cancel = presentationText(GAME_TEXT_BACK, "CANCEL");
        char prompt[256];
        snprintf(prompt, sizeof(prompt),
                 "ENTER / A: %s    ESC / B: %s", confirm, cancel);
        const ImVec2 size = ImGui::CalcTextSize(prompt);
        drawList->AddText(
            ImVec2(origin.x + (sk::globals.width - size.x) * 0.5f,
                   origin.y + sk::globals.height - 60.0f),
            UI_COLOR_ACCENT, prompt);
    }
}

bool projectWorldPoint(const rw::V3d& world, ImVec2* screen)
{
    if(screen == NULL || g_scene.camera == NULL ||
       g_scene.camera->getFrame() == NULL)
        return false;
    const rw::Matrix& matrix = g_scene.camera->getFrame()->matrix;
    const rw::V3d relative =
    {
        world.x - matrix.pos.x,
        world.y - matrix.pos.y,
        world.z - matrix.pos.z
    };

    const float cameraX = -(relative.x * matrix.right.x +
        relative.y * matrix.right.y + relative.z * matrix.right.z);
    const float cameraY = relative.x * matrix.up.x +
        relative.y * matrix.up.y + relative.z * matrix.up.z;
    const float cameraZ = relative.x * matrix.at.x +
        relative.y * matrix.at.y + relative.z * matrix.at.z;
    if(cameraZ <= g_scene.camera->nearPlane ||
       g_scene.camera->viewWindow.x == 0.0f ||
       g_scene.camera->viewWindow.y == 0.0f)
        return false;
    screen->x = sk::globals.width *
        (0.5f + cameraX / (cameraZ * g_scene.camera->viewWindow.x) * 0.5f);
    screen->y = sk::globals.height *
        (0.5f - cameraY / (cameraZ * g_scene.camera->viewWindow.y) * 0.5f);
    return screen->x >= -UI_SPACE_8 &&
           screen->x <= sk::globals.width + UI_SPACE_8 &&
           screen->y >= -UI_SPACE_8 &&
           screen->y <= sk::globals.height + UI_SPACE_8;
}

void drawGameplayPresentation(void)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();

    if(g_combatPresentation.damageFlash[0] > 0.0f)
    {
        const uint32_t alpha = static_cast<uint32_t>(
            clampPresentation(
                g_combatPresentation.damageFlash[0] * 95.0f,
                0.0f, 95.0f));
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + sk::globals.width,
                   origin.y + sk::globals.height),
            IM_COL32(155, 18, 22, alpha));
    }

    if(g_game.interactionFocusSlot() >= 0)
    {
        const char* prompt = presentationText(
            GAME_TEXT_SELECT, "INTERACT");
        char text[128];
        snprintf(text, sizeof(text), "ENTER / A: %s", prompt);
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 minimum(
            origin.x + (sk::globals.width - size.x) * 0.5f - UI_SPACE_3,
            origin.y + sk::globals.height - 82.0f);
        const ImVec2 maximum(minimum.x + size.x + UI_SPACE_6,
                             minimum.y + size.y + UI_SPACE_2);
        drawList->AddRectFilled(minimum, maximum, UI_COLOR_SURFACE);
        drawList->AddText(ImVec2(minimum.x + UI_SPACE_3,
                                 minimum.y + UI_SPACE_1),
                          UI_COLOR_ACCENT, text);
    }
}

bool drawSystemFontText(float x, float y, const char* text,
                        bool alignCentre, const ImVec4& color, float scale)
{
    const UiTexture& atlas = g_ui.systemFont;
    if(atlas.texture == NULL || atlas.width == 0 || atlas.height == 0 ||
       text == NULL)
        return false;

    const MenuTextLayout layout = menuTextLayoutDefaults();
    const ImVec2 origin = ImGui::GetWindowPos();
    float penX = origin.x + x;
    const float penY = origin.y + y;
    if(alignCentre)
        penX -= menuTextWidth(text, layout) * 0.5f * scale;

    const float atlasWidth = MENU_ATLAS_WIDTH;
    const float atlasHeight = MENU_ATLAS_HEIGHT;
    const ImU32 packed = ImGui::GetColorU32(color);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushTextureID(uiTextureRef(atlas));
    for(const unsigned char* cursor = (const unsigned char*)text;
        *cursor != 0; ++cursor)
    {
        float cellX = 0.0f;
        float cellY = 0.0f;
        if(menuGlyphCell(*cursor, layout, &cellX, &cellY) &&
           cellX + MENU_CELL_WIDTH <= atlasWidth &&
           cellY + MENU_CELL_HEIGHT <= atlasHeight)
        {
            const ImVec2 uv0(cellX / atlasWidth, cellY / atlasHeight);
            const ImVec2 uv1((cellX + MENU_CELL_WIDTH) / atlasWidth,
                             (cellY + MENU_CELL_HEIGHT) / atlasHeight);
            drawList->AddImage(
                uiTextureRef(atlas), ImVec2(penX, penY),
                ImVec2(penX + MENU_CELL_WIDTH * scale,
                       penY + MENU_CELL_HEIGHT * scale),
                uv0, uv1, packed);
        }
        penX += menuGlyphAdvance(*cursor, layout) * scale;
    }
    drawList->PopTextureID();
    return true;
}

bool drawMenuLayoutItem(int itemIndex, int colourIndex, float alphaScale)
{
    if(itemIndex < 0 || itemIndex >= MENU_ITEM_COUNT ||
       colourIndex < 0 || colourIndex >= MENU_COLOUR_COUNT)
        return false;
    const MenuItemLayout& item = MENU_ITEM_LAYOUT[itemIndex];
    const MenuItemColour& colour = MENU_ITEM_COLOURS[colourIndex];
    const char* text = item.textId != 0 ?
        presentationText(item.textId, item.literal) : item.literal;

    const float scaleX = sk::globals.width / MENU_SCREEN_WIDTH;
    const float scaleY = sk::globals.height / MENU_SCREEN_HEIGHT;
    const ImVec4 tint(colour.r / 255.0f, colour.g / 255.0f,
                      colour.b / 255.0f, colour.a / 255.0f * alphaScale);
    return drawSystemFontText(item.x * scaleX, item.y * scaleY, text, true,
                              tint, item.scale * scaleX);
}

void drawCenteredAsset(const UiTexture& asset, float width, float y)
{
    if(asset.texture == NULL || asset.width == 0 || asset.height == 0)
        return;
    const float height = width *
        static_cast<float>(asset.height) / static_cast<float>(asset.width);
    const ImVec2 origin = ImGui::GetWindowPos();
    const float x = origin.x + (sk::globals.width - width) * 0.5f;
    ImGui::GetWindowDrawList()->AddImage(
        uiTextureRef(asset), ImVec2(x, origin.y + y),
        ImVec2(x + width, origin.y + y + height));
}

float sceneCaptionBattleCameraHeight(char mode)
{

    return mode == 0 ? 68.0f : 400.0f;
}

void drawSceneCaption(void)
{
    if(g_sceneCaption.empty())
        return;

    const float firstLineTop =
        sceneCaptionBattleCameraHeight(1) + 2.0f;
    const float rowPitch = 24.0f;

    size_t start = 0;
    int row = 0;
    while(start <= g_sceneCaption.size() && row < 2)
    {
        const size_t split = g_sceneCaption.find('\n', start);
        const size_t end = split == std::string::npos ?
            g_sceneCaption.size() : split;
        const std::string line = g_sceneCaption.substr(start, end - start);
        if(!line.empty())
        {

            const float y = firstLineTop + row * rowPitch;
            const MenuTextExtent extent =
                measureRetainedMenuText(line.c_str(), 1.0f, 1.0f);
            drawRetainedMenuText(
                line.c_str(),
                MENU_SCREEN_WIDTH * 0.5f - extent.width * 0.5f, y,
                1.0f, 1.0f, 200, 200, 202, 200, false);
            ++row;
        }
        if(split == std::string::npos)
            break;
        start = split + 1;
    }
}

void drawSceneConfirmationPrompt(void)
{
    const SceneSequencePromptState state =
        g_game.sceneSequence().promptState();
    if(state != SCENE_SEQUENCE_PROMPT_OPEN &&
       state != SCENE_SEQUENCE_PROMPT_ADVANCING)
        return;

    hudPromptClear();
    hudPromptAddTextId(
        195.0f, 452.0f, HUD_PROMPT_CONFIRM, 0x28cd);
    hudPromptAddTextId(
        327.0f, 452.0f, HUD_PROMPT_CANCEL, 0x28c2);
    hudPromptSetGate(0, 1);
    hudPromptDraw();
}

bool markerEventFlagQuery(uint16_t eventFlag, void* userData)
{
    const StoryState* const story =
        static_cast<const StoryState*>(userData);
    return story != NULL && getStorySaveFlag(*story, eventFlag);
}

void drawStageInteractionMarker(void)
{
    FstageMarkerQueryState state;
    state.stageId = g_scene.stageOriginalId;
    state.userData =
        const_cast<StoryState*>(&g_game.runtimeState().story);
    state.eventFlag = markerEventFlagQuery;
    const GameSlotRecord& player = g_game.runtimeState().player;
    const rw::V3d position = {
        player.position.x, player.position.y, player.position.z};
    FstageMarkerPrompt prompt;
    const FstageMarkerResolution resolution =
        resolveFstageInteractionMarker(g_scene.stage, position, state, &prompt);
    if(resolution == FSTAGE_MARKER_TRANSITION && prompt.side > 0)
    {
        if(g_game.runtimeState().story.stageTransitionPending == 0)
            g_game.requestFstageMarkerTransition(
                static_cast<uint16_t>(prompt.textId),
                prompt.commandParameter);
        return;
    }
    if(resolution != FSTAGE_MARKER_PROMPT &&
       resolution != FSTAGE_MARKER_CONSUMED_TEXT)
        return;
    const char* text = presentationText(
        static_cast<uint16_t>(prompt.displayTextId), "");
    drawSystemFontText(
        588.0f * sk::globals.width / 640.0f,
        450.0f * sk::globals.height / 480.0f,
        text, true, ImVec4(236.0f / 255.0f, 232.0f / 255.0f,
                          218.0f / 255.0f, 1.0f),
        sk::globals.width / 640.0f);
}

void drawIntroCardSequence(void)
{
    const ImVec2 origin = ImGui::GetWindowPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        origin,
        ImVec2(origin.x + static_cast<float>(sk::globals.width),
               origin.y + static_cast<float>(sk::globals.height)),
        IM_COL32(0, 0, 0, 255));

    const IntroCardPresentation& card = g_game.introCardPresentation();
    const uint16_t textId = g_game.introCardTextId();
    const char* text = textId != 0 ? g_game.localizedText(textId) : NULL;
    if(text != NULL && card.textAlpha != 0 && loadRetainedMenuFont())
    {

        const MenuTextExtent extent =
            measureRetainedMenuText(text, 1.0f, 1.0f);
        drawRetainedMenuText(
            text,
            320.0f - extent.width * 0.5f,
            240.0f - extent.height * 0.5f,
            1.0f, 1.0f, 200, 200, 200, card.textAlpha, false);
    }

    if(card.maskAlpha != 0)
    {
        const uint8_t alpha = static_cast<uint8_t>(
            card.maskAlpha * 255 / GAME_INTRO_CARD_MASK_ALPHA_MAX);
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + static_cast<float>(sk::globals.width),
                   origin.y + static_cast<float>(sk::globals.height)),
            IM_COL32(0, 0, 0, alpha));
    }
}

void drawRecoveredLoadingScreen(void)
{

    const char* localized = presentationText(
        GAME_TEXT_NOW_LOADING, "NOW LOADING...");
    char text[64];
    snprintf(text, sizeof(text), "%s", localized);
    size_t length = strlen(text);
    while(length > 0 && text[length - 1] == '.')
        --length;
    if(length + 3 < sizeof(text))
    {
        text[length++] = '.';
        text[length++] = '.';
        text[length++] = '.';
        text[length] = '\0';
    }

    if(!loadMenuFont())
    {
        centerText(text);
        return;
    }

    static uint8_t pulseCounter;
    ++pulseCounter;

    const uint8_t phase = pulseCounter & 0x1f;
    const uint8_t level = phase < 0x10 ?
        static_cast<uint8_t>(phase << 4) :
        static_cast<uint8_t>(0xff - ((phase - 0x10) << 4));
    const float scale = 0.7f;
    const float x = 593.0f - measureMenuText(text, scale);
    drawMenuText(text, x, 439.0f, scale,
                 level, level, level, 255, true);
}

void drawHostDebugOverlay(void)
{
    if(!g_hostDebugOverlayEnabled)
        return;

    const bool sceneActive =
        g_sceneRuntime.status() == SceneStatus::Playing;
    const SceneDefinition* definition = g_sceneRuntime.definition();
    const ImVec2 origin = ImGui::GetWindowPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float scaleX = sk::globals.width / UI_VIRTUAL_WIDTH;
    const float scaleY = sk::globals.height / 480.0f;
    const float left = origin.x + 8.0f * scaleX;
    const float top = origin.y + 8.0f * scaleY;
    const float lineHeight = 16.0f * scaleY;
    char lines[16][512];
    int lineCount = 0;

    snprintf(lines[lineCount++], sizeof(lines[0]),
             "HOST DEBUG OVERLAY (development tooling)");
    if(sceneActive)
    {
        char sceneName[192];
        const char* entry = g_activeSceneEntry != NULL ?
            g_activeSceneEntry : "(unknown)";
        const bool hasExtension =
            strstr(entry, ".SCN") != NULL || strstr(entry, ".scn") != NULL;
        snprintf(sceneName, sizeof(sceneName), "%s%s", entry,
                 hasExtension ? "" : ".SCN");
        const float sceneTime = g_sceneRuntime.time();
        const int sceneFrame = sceneTime > 0.0f ?
            static_cast<int>(sceneTime * 60.0f + 0.5f) : 0;
        snprintf(lines[lineCount++], sizeof(lines[0]), "SCENE: %s",
                 sceneName);
        snprintf(lines[lineCount++], sizeof(lines[0]),
                 "SCENE FRAME: %d  SCENE TIME: %.3fs",
                 sceneFrame, sceneTime);
        snprintf(lines[lineCount++], sizeof(lines[0]),
                 "STAGE ID: %d  DURATION: %.3fs",
                 definition != NULL ? definition->stageId : -1,
                 definition != NULL ? definition->duration : 0.0f);
        char actors[512];
        int actorLength = snprintf(actors, sizeof(actors),
                                   "SCENE ACTORS:");
        for(int i = 0; i < SCENE_ACTOR_CAPACITY &&
                    actorLength < static_cast<int>(sizeof(actors)) - 1; ++i)
        {
            if(g_scene.sceneActorTracks[i].empty())
                continue;
            const int remaining =
                static_cast<int>(sizeof(actors)) - actorLength;
            const int appended = snprintf(
                actors + actorLength, static_cast<size_t>(remaining),
                " %s", g_scene.sceneActorTracks[i].c_str());
            actorLength += appended >= remaining ? remaining - 1 : appended;
        }
        snprintf(lines[lineCount++], sizeof(lines[0]), "%s", actors);
    }
    else
    {
        const GameRuntimeState& state = g_game.runtimeState();
        snprintf(lines[lineCount++], sizeof(lines[0]), "STATE: %s",
                 Game::screenName(g_game.screen()));
        snprintf(lines[lineCount++], sizeof(lines[0]),
                 "STAGE ID: %u  GAME TIME: %.3fs",
                 static_cast<unsigned>(g_scene.stageOriginalId),
                 g_game.battleTime());
        snprintf(lines[lineCount++], sizeof(lines[0]),
                 "SCENE FRAME: N/A (no active .SCN)");
        const GameSlotRecord& player = state.player;
        const float speed = sqrtf(
            player.velocity.x * player.velocity.x +
            player.velocity.z * player.velocity.z);

        const bool locomotion =
            player.action == COMBAT_ACTION_IDLE ||
            player.action == COMBAT_ACTION_MOVE;
        const int motionId = locomotion ?
            player.locomotionMoveId :
            (player.scheduledMotionValid ?
                player.scheduledMotion.motionId :
                player.recoveredMoveId);
        snprintf(lines[lineCount++], sizeof(lines[0]),
                 "RETAIL DEBUG P0 MIRROR: FRAME %.0f MOT %d MOVE %d MSPEED %.1f",
                 player.playingAnimationFrame, motionId,
                 player.recoveredMoveId, speed);
        snprintf(lines[lineCount++], sizeof(lines[0]),
                 "HP %d/%d POS %.1f,%.1f,%.1f DIR %.1f AI %u PWORK %d",
                 player.currentHealth, player.maxHealth,
                 player.position.x, player.position.y, player.position.z,
                 player.headingDegrees,
                 static_cast<unsigned>(player.actorBehaviorFlags),
                 player.resourceProfileId);

        const int opponents = g_game.opponentCount();
        for(int i = 0; i < opponents && lineCount < 16; ++i)
        {
            const GameSlotRecord& foe = g_game.opponent(i);
            const bool foeLocomotion =
                foe.action == COMBAT_ACTION_IDLE ||
                foe.action == COMBAT_ACTION_MOVE;
            const int foeMotion = foeLocomotion ?
                foe.locomotionMoveId :
                (foe.scheduledMotionValid ?
                    foe.scheduledMotion.motionId :
                    foe.recoveredMoveId);
            snprintf(lines[lineCount++], sizeof(lines[0]),
                     "E%d ACT %d MOT %d MOVE %d FRAME %.0f HP %d "
                     "POS %.1f,%.1f,%.1f AI %u",
                     i, static_cast<int>(foe.action), foeMotion,
                     foe.recoveredMoveId, foe.playingAnimationFrame,
                     foe.currentHealth, foe.position.x, foe.position.y,
                     foe.position.z,
                     static_cast<unsigned>(foe.actorBehaviorFlags));
        }
        if(lineCount < 16 && g_game.stageAiLoaded())
        {
            snprintf(lines[lineCount++], sizeof(lines[0]), "AI: %s",
                     g_game.stageAiDiagnostic());
        }
    }

    const float width = 8.0f * scaleX + 360.0f * scaleX;
    const float height = 8.0f * scaleY +
        static_cast<float>(lineCount) * lineHeight;
    drawList->AddRectFilled(
        ImVec2(left - 4.0f * scaleX, top - 4.0f * scaleY),
        ImVec2(left + width, top + height),
        IM_COL32(0, 0, 0, 205));
    for(int i = 0; i < lineCount; ++i)
    {
        drawList->AddText(
            ImVec2(left, top + static_cast<float>(i) * lineHeight),
            IM_COL32(255, 255, 255, 255), lines[i]);
    }
}

void drawGame(void)
{
    const GameRuntimeState& state = g_game.runtimeState();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(sk::globals.width),
                                    static_cast<float>(sk::globals.height)));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("RWFightTribe", NULL,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    drawSceneMask();
    drawSceneCharacterSelectOverlay();
    drawSceneOverlay();

    drawSequencerCaption();
    if(g_sceneRuntime.status() == SceneStatus::Playing)
    {
        drawSceneCaption();
        drawHostDebugOverlay();
        ImGui::End();
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 38.0f));
    switch(g_game.screen())
    {
    case GAME_SCREEN_LOADING:
        if(g_game.introCardSequenceActive())
            drawIntroCardSequence();
        else
            drawRecoveredLoadingScreen();
        break;

    case GAME_SCREEN_ATTRACT_MOVIE:
        break;

    case GAME_SCREEN_LANGUAGE_SELECT:
    {

        const ImVec2 origin = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            origin,
            ImVec2(origin.x + static_cast<float>(sk::globals.width),
                   origin.y + static_cast<float>(sk::globals.height)),
            IM_COL32(0, 0, 0, 255));

        if(loadMenuFont())
        {
            const auto drawCenteredMenuText =
                [](const char* text, float y, float scale,
                   uint8_t r, uint8_t g, uint8_t b)
                {
                    const float x =
                        (640.0f - measureMenuText(text, scale)) * 0.5f;
                    drawMenuText(text, x, y, scale, r, g, b, 255, true);
                };

            drawCenteredMenuText("SELECT LANGUAGE", 92.0f, 0.8f,
                                 236, 232, 218);

            const float entryScale = 0.75f;
            const float firstEntryCenterY = 165.0f;
            for(int i = 0; i < 5; ++i)
            {
                char label[128];
                const bool selected = i == g_game.languageSelection();
                if(selected)
                    snprintf(label, sizeof(label), "> %s <", APP_UI_LANGUAGES[i]);
                else
                    snprintf(label, sizeof(label), "%s", APP_UI_LANGUAGES[i]);
                const float y = firstEntryCenterY + i * 32.0f -
                    MENU_CELL_HEIGHT * entryScale * 0.5f;
                drawCenteredMenuText(label, y, entryScale,
                                     selected ? 240 : 236,
                                     selected ? 199 : 232,
                                     selected ? 56 : 218);
            }

            const char* select =
                presentationText(GAME_TEXT_SELECT, "SELECT");
            char prompt[512];
            snprintf(prompt, sizeof(prompt), "ENTER / A: %s", select);
            drawCenteredMenuText(prompt, 392.0f, 0.55f, 189, 194, 204);
        }
        break;
    }

    case GAME_SCREEN_VIDEO_MODE_SELECT:

        drawBootImageStretched(PALMODE_IMAGE_VSCRN, 320.0f, 256.0f,
                               640.0f, 448.0f);
        drawVideoModeScreen();
        break;

    case GAME_SCREEN_NO_SAVE_DATA:

        drawNoSaveDataScreen();
        break;

    case GAME_SCREEN_TITLE:
    {

        const float sceneScale = titleAttractSceneScale(g_titleAttract);
        drawTitleArtwork(
            sceneScale,
            titleAttractSceneVisible(g_titleAttract) ?
                titleAttractSceneAlpha(g_titleAttract) : 0);
        ImGui::Dummy(ImVec2(0.0f, 320.0f));

        if(titleAttractPromptVisible(g_titleAttract))
        {
            const float grey = TITLE_ATTRACT_PROMPT_GREY / 255.0f;
            const float pulse =
                titleAttractPromptAlpha(g_titleAttract) / 255.0f;
            const ImVec4 promptColor(grey, grey, grey, pulse);
            const char* prompt = presentationText(
                GAME_TEXT_PRESS_START, "PRESS START BUTTON");

            const MenuItemLayout& promptItem =
                MENU_ITEM_LAYOUT[MENU_ITEM_PRESS_START];
            const float promptScaleX = sk::globals.width / MENU_SCREEN_WIDTH;
            if(!drawSystemFontText(
                   promptItem.x * promptScaleX,
                   promptItem.y * (sk::globals.height / MENU_SCREEN_HEIGHT),
                   prompt, true, promptColor,
                   promptItem.scale * promptScaleX))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, promptColor);
                centerText(prompt);
                ImGui::PopStyleColor();
            }
        }
        break;
    }

    case GAME_SCREEN_MAIN_MENU:
        drawTitleMenuScreen();
        break;

    case GAME_SCREEN_VERSUS_TYPE_SELECT:
        drawVersusTypeScreen();
        break;

    case GAME_SCREEN_CHARACTER_SELECT:
    {
        if(g_game.mode() == GAME_MODE_STORY)
        {
            drawCharacterSelectScreen();
            break;
        }

        if(g_ui.systemFont.texture != NULL)
        {
            const float scaleX = sk::globals.width / MENU_SCREEN_WIDTH;
            const float scaleY = sk::globals.height / MENU_SCREEN_HEIGHT;
            drawSystemFontText(
                320.0f * scaleX, 112.0f * scaleY, "CHARACTER SELECT", true,
                ImVec4(0x80 / 255.0f, 0xc0 / 255.0f, 1.0f,
                       0x80 / 255.0f),
                0.7f * scaleX);

            const uint8_t pulse = menuPulseHighlight(g_menuPulse.value);
            const float activeChannel =
                (pulse * 0xe0 / 0xff) / 255.0f;
            const ImVec4 rowTint(0xc0 / 255.0f, 0xc0 / 255.0f,
                                 0xc0 / 255.0f, 0x80 / 255.0f);
            const ImVec4 activeTint(activeChannel, activeChannel,
                                    activeChannel, 1.0f);
            for(int side = 0; side < 2; ++side)
            {
                const int selected =
                    g_game.versusCharacterSelection(side);
                const float x = (side == 0 ? 160.0f : 480.0f) * scaleX;
                for(int row = -2; row <= 2; ++row)
                {
                    const int index =
                        (selected + row + GAME_VERSUS_CHARACTER_COUNT) %
                        GAME_VERSUS_CHARACTER_COUNT;
                    const bool highlighted =
                        row == 0 && g_game.versusSideActive(side) &&
                        !g_game.versusSideReady(side);
                    drawSystemFontText(
                        x, (240.0f + row * 26.0f) * scaleY,
                        g_game.versusCharacterDisplayName(index), true,
                        highlighted ? activeTint : rowTint,
                        0.7f * scaleX);
                }
                drawSystemFontText(
                    x, 344.0f * scaleY,
                    g_game.versusCharacterStyleName(side), true,
                    ImVec4(1.0f, 1.0f, 0x5f / 255.0f, 0xc0 / 255.0f),
                    0.7f * scaleX);
            }
        }
        drawCommandPrompt(true);
        break;
    }

    case GAME_SCREEN_STAGE_SELECT:
    {
        const int selectedStage = g_game.stageSelection();

        if(g_ui.systemFont.texture != NULL)
        {
            const float scaleX = sk::globals.width / MENU_SCREEN_WIDTH;
            const float scaleY = sk::globals.height / MENU_SCREEN_HEIGHT;
            const MenuItemColour& heading = STAGE_SELECT_TITLE_COLOUR;
            drawSystemFontText(
                STAGE_SELECT_TITLE_X * scaleX, STAGE_SELECT_TITLE_Y * scaleY,
                "STAGE SELECT",
                true,
                ImVec4(heading.r / 255.0f, heading.g / 255.0f,
                       heading.b / 255.0f, heading.a / 255.0f),
                STAGE_SELECT_TITLE_SCALE_X * scaleX);

            const uint8_t highlight =
                menuPulseHighlight(g_menuPulse.value);
            for(int row = STAGE_SELECT_ROW_FIRST;
                row <= STAGE_SELECT_ROW_LAST; ++row)
            {
                const int index = stageSelectRowIndex(
                    selectedStage, row, GAME_VERSUS_STAGE_COUNT);
                const char* name = stageSelectionDisplayName(index);
                ImVec4 tint;
                if(row == 0)
                    tint = ImVec4(highlight / 255.0f, highlight / 255.0f,
                                  highlight / 255.0f, 1.0f);
                else
                    tint = ImVec4(STAGE_SELECT_ROW_COLOUR.r / 255.0f,
                                  STAGE_SELECT_ROW_COLOUR.g / 255.0f,
                                  STAGE_SELECT_ROW_COLOUR.b / 255.0f,
                                  STAGE_SELECT_ROW_COLOUR.a / 255.0f);
                drawSystemFontText(STAGE_SELECT_ITEM_X * scaleX,
                                   stageSelectRowY(row) * scaleY, name, true,
                                   tint,
                                   STAGE_SELECT_ITEM_SCALE_X * scaleX);
            }
        }
        else
        {
            int firstStage = selectedStage - 4;
            if(firstStage < 0)
                firstStage = 0;
            if(firstStage > GAME_VERSUS_STAGE_COUNT - 9)
                firstStage = GAME_VERSUS_STAGE_COUNT - 9;
            centerText("STAGE SELECT");
            ImGui::Dummy(ImVec2(0.0f, UI_SPACE_4));
            for(int i = firstStage; i < firstStage + 9; ++i)
            {
                drawMenuItem(stageSelectionDisplayName(i),
                             i == selectedStage);
            }
            ImGui::Dummy(ImVec2(0.0f, UI_SPACE_2));
        }
        drawCommandPrompt(true);
        break;
    }

    case GAME_SCREEN_BRIEFING:
    {

        if(g_game.mode() != GAME_MODE_STORY &&
           g_versusBannerMotion.active &&
           g_ui.systemFont.texture != NULL)
        {
            const uint8_t language = state.options.language;
            const char* separator = MENU_VERSUS_SEPARATOR[
                language < MENU_VERSUS_LANGUAGE_COUNT ? language : 0];
            const float scaleX = sk::globals.width / MENU_SCREEN_WIDTH;
            const float scaleY = sk::globals.height / MENU_SCREEN_HEIGHT;
            const MenuItemColour& banner =
                MENU_ITEM_COLOURS[MENU_COLOUR_SELECTED];
            const ImVec4 tint(banner.r / 255.0f, banner.g / 255.0f,
                              banner.b / 255.0f, banner.a / 255.0f);
            drawSystemFontText(
                g_versusBannerMotion.x[0] * scaleX,
                (g_versusBannerMotion.y[0] + 99.0f) * scaleY,
                versusBannerNameForSide(0),
                true, tint, scaleX);
            drawSystemFontText(
                320.0f * scaleX, g_versusBannerMotion.separatorY * scaleY,
                separator, true, tint, scaleX);
            drawSystemFontText(
                g_versusBannerMotion.x[1] * scaleX,
                (g_versusBannerMotion.y[1] + 99.0f) * scaleY,
                versusBannerNameForSide(1),
                true, tint, scaleX);
        }
        break;
    }

    case GAME_SCREEN_GAMEPLAY:
    {

        g_game.runtimeState().hudRadar.globalSuppress =
            sceneHudOverrideActive() && !sceneHudVisible() ? 1 : 0;
        if(!sceneHudOverrideActive() || sceneHudVisible())
        {

            drawLocalizedInfoPresentation(g_game);
            drawBattleHud(g_game, g_scene.stageOriginalId);
            drawBattleEndNotice(g_game);

            const bool sequenceCaptionActive =
                g_game.sceneSequence().caption().activeTask;
            if(hudAdvicePanelActive())
            {
                if(!sequenceCaptionActive)
                    hudDrawAdvice();
            }
            else
            {
                drawGameplayPresentation();
                if(!sequenceCaptionActive)
                {
                    if(hudAdviceBottomLineActive())
                        hudDrawAdvice();
                    else
                        hudDrawBattlePromptLine();
                }
            }
        }
        if(!hudAdvicePanelActive())
            drawStageInteractionMarker();
        break;
    }

    case GAME_SCREEN_PAUSE:
        drawPauseScreen();
        break;

    case GAME_SCREEN_INVENTORY:
        drawInventoryScreen();
        break;

    case GAME_SCREEN_ADVICE:
        drawAdviceScreen();
        break;

    case GAME_SCREEN_SHOP:
        drawShopScreen();
        break;

    case GAME_SCREEN_OPTIONS:
        drawOptionsScreen();
        break;

    case GAME_SCREEN_RESULTS:

        prepareLocalizedInfoFamily(g_game.options().language, 1);
        setResultScreenSelection(g_game.resultSelection());
        drawRecoveredResultsScreen();
        break;

    case GAME_SCREEN_CREDITS:
        drawCreditsSequenceScreen(g_game.creditsSequence());
        break;

    case GAME_SCREEN_EXIT:
        break;
    }

    drawSceneConfirmationPrompt();
    drawEventOverlay();
    drawHostDebugOverlay();
    ImGui::End();
}

}

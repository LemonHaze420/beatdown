#include "app/internal.h"

namespace app_detail
{

const char* versusBannerNameForSide(int side)
{
    int index = g_game.versusCharacterSelection(side);
    if(index < 0 || index >= GAME_VERSUS_CHARACTER_COUNT)
    {
        const uint16_t characterId =
            side == 0 ? g_game.selectedCharacterId() :
            g_game.opponent().characterId;
        index = g_game.versusCharacterIndex(characterId);
    }
    return g_game.versusCharacterDisplayName(index);
}

void updateVersusBannerMotion(float deltaSeconds)
{
    const GameScreen screen = g_game.screen();
    const bool versusBriefing =
        screen == GAME_SCREEN_BRIEFING &&
        g_game.mode() != GAME_MODE_STORY;
    if(versusBriefing && !g_versusBannerWasActive)
    {
        initializeVersusBannerMotion(g_versusBannerMotion);
        g_versusBannerWasActive = true;
        g_versusBannerFrames = 0.0f;
    }
    else if(!versusBriefing && g_versusBannerWasActive)
    {
        requestVersusBannerExit(g_versusBannerMotion);
        g_versusBannerWasActive = false;
    }

    if(g_versusBannerMotion.active &&
       (versusBriefing ||
        g_versusBannerMotion.phase != VERSUS_BANNER_ENTRY))
    {
        const float frameRate = g_game.nominalFrameRateHz();
        g_versusBannerFrames += deltaSeconds * frameRate;
        const int wholeFrames = (int)(g_versusBannerFrames + 0.00001f);
        g_versusBannerFrames -= wholeFrames;
        if(g_versusBannerFrames < 0.0f)
            g_versusBannerFrames = 0.0f;
        for(int i = 0; i < wholeFrames && g_versusBannerMotion.active; ++i)
        {
            advanceVersusBannerMotion(
                g_versusBannerMotion,
                g_game.scaleFrameDelta(1.0f / frameRate));
        }
        if(versusBriefing && !g_versusBannerMotion.active)
            g_game.completeStageBriefingPresentation();
    }
}

const char* presentationText(uint16_t textId, const char* hostLiteral)
{
    const char* text = g_game.localizedText(textId);
    return text != NULL ? text : hostLiteral;
}

const char* stageSelectionDisplayName(int versusIndex)
{
    const int wrappedIndex = stageSelectRowIndex(
        0, versusIndex, GAME_VERSUS_STAGE_COUNT);
    const uint16_t stageId = Game::versusStageId(wrappedIndex);
    if(stageId >= SCENE_SEQUENCE_STAGE_NAME_COUNT)
        return "";
    return presentationText(
        SCENE_SEQUENCE_STAGE_NAME_TEXT_IDS[stageId], "");
}

uint16_t characterResourceId(CharacterId characterId)
{

    const int resource =
        characterRecordResourceId(static_cast<uint16_t>(characterId));
    if(resource > 0)
        return static_cast<uint16_t>(resource);
    return 153;
}

bool hasExtension(const std::string& path, const char* extension)
{
    if(extension == NULL || path.size() < strlen(extension))
        return false;
    const size_t offset = path.size() - strlen(extension);
    for(size_t i = 0; extension[i] != '\0'; i++)
    {
        char left = path[offset + i];
        char right = extension[i];
        if(left >= 'a' && left <= 'z')
            left = static_cast<char>(left - 'a' + 'A');
        if(right >= 'a' && right <= 'z')
            right = static_cast<char>(right - 'a' + 'A');
        if(left != right)
            return false;
    }
    return true;
}

bool loadUiTexture(const char* archivePath, const char* entryName,
                   UiTexture* asset)
{
    if(asset == NULL)
        return false;
    if(asset->texture != NULL)
        return true;

    rw::Texture* texture = loadStxTexture(archivePath, entryName);
    if(texture == NULL || texture->raster == NULL)
    {
        if(texture != NULL)
            texture->destroy();
        return false;
    }
    asset->texture = texture;
    asset->width = static_cast<uint16_t>(texture->raster->width);
    asset->height = static_cast<uint16_t>(texture->raster->height);
    return true;
}

void releaseUiTexture(UiTexture* asset)
{
    if(asset == NULL)
        return;
    if(asset->texture != NULL)
        asset->texture->destroy();
    asset->texture = NULL;
    asset->width = 0;
    asset->height = 0;
}

void initializeUiAssets(void)
{
    listXafEntries(g_graphPath, g_ui.graphEntries);
    listXafEntries(g_fbgmPath, g_ui.audioEntries);

    if(g_ui.graphEntries.size() > (size_t)MENU_FONT_ATLAS_ENTRY)
        loadUiTexture(g_graphPath,
                      g_ui.graphEntries[MENU_FONT_ATLAS_ENTRY].name.c_str(),
                      &g_ui.systemFont);

    if(g_ui.systemFont.texture != NULL &&
       g_ui.graphEntries.size() > (size_t)MENU_FONT_ATLAS_ENTRY)
        printf("system font %s (%ux%u) %d cols x %d rows of %.0fx%.0f\n",
               g_ui.graphEntries[MENU_FONT_ATLAS_ENTRY].name.c_str(),
               static_cast<unsigned>(g_ui.systemFont.width),
               static_cast<unsigned>(g_ui.systemFont.height),
               static_cast<int>(g_ui.systemFont.width / MENU_CELL_WIDTH),
               static_cast<int>(g_ui.systemFont.height / MENU_CELL_HEIGHT),
               MENU_CELL_WIDTH, MENU_CELL_HEIGHT);
    else
        printf("system font unavailable\n");
}

void releaseUiAssets(void)
{
    for(int i = 0; i < PALMODE_IMAGE_COUNT; ++i)
        releaseUiTexture(&g_palModeTextures[i]);
    releaseUiTexture(&g_ui.systemFont);
    releaseMenuSystemArtwork();
    releaseMenuPauseResources();
    for(int i = 0; i < EVENT_VM_TEXTURE_SLOT_COUNT; i++)
    {
        releaseUiTexture(&g_ui.eventTextures[i].asset);
        g_ui.eventTextures[i].resourceId = -1;
    }
    g_ui.graphEntries.clear();
    g_ui.audioEntries.clear();
}

ImTextureRef uiTextureRef(const UiTexture& asset)
{
    return ImTextureRef(static_cast<ImTextureID>(
        reinterpret_cast<uintptr_t>(asset.texture)));
}

bool
drawBootImage(int imageId, float maxWidth)
{
    if(imageId < 0 || imageId >= PALMODE_IMAGE_COUNT)
        return false;
    const UiTexture& asset = g_palModeTextures[imageId];
    if(asset.texture == NULL || asset.width == 0 || asset.height == 0)
        return false;

    float width = static_cast<float>(asset.width);
    float height = static_cast<float>(asset.height);
    if(width > maxWidth && width > 0.0f)
    {
        height *= maxWidth / width;
        width = maxWidth;
    }
    const float indent = (ImGui::GetContentRegionAvail().x - width) * 0.5f;
    if(indent > 0.0f)
        ImGui::Indent(indent);
    ImGui::Image(uiTextureRef(asset), ImVec2(width, height));
    if(indent > 0.0f)
        ImGui::Unindent(indent);
    return true;
}

bool
drawBootImageStretched(int imageId, float centreX, float centreY,
                       float width, float height)
{
    if(imageId < 0 || imageId >= PALMODE_IMAGE_COUNT)
        return false;
    const UiTexture& asset = g_palModeTextures[imageId];
    if(asset.texture == NULL || asset.width == 0 || asset.height == 0)
        return false;

    const float scaleX = sk::globals.width / MENU_SCREEN_WIDTH;
    const float scaleY = sk::globals.height / MENU_SCREEN_HEIGHT;
    const ImVec2 min((centreX - width * 0.5f) * scaleX,
                     (centreY - height * 0.5f) * scaleY);
    const ImVec2 max((centreX + width * 0.5f) * scaleX,
                     (centreY + height * 0.5f) * scaleY);
    ImGui::GetWindowDrawList()->AddImage(uiTextureRef(asset), min, max);
    return true;
}

void updatePalModePackage(void)
{

    const int language =
        static_cast<int>(g_game.runtimeState().options.language);
    if(language == g_palModeLanguage)
        return;
    g_palModeLanguage = language;

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", g_palModeRoot,
             palModeFileForLanguage(language));
    for(int i = 0; i < PALMODE_IMAGE_COUNT; ++i)
        releaseUiTexture(&g_palModeTextures[i]);

    std::string error;
    if(loadPalModePackage(path, g_palMode, &error))
    {
        for(int i = 0; i < PALMODE_IMAGE_COUNT; ++i)
        {
            rw::Texture* texture = createPalModeTexture(g_palMode, i);
            if(texture == NULL || texture->raster == NULL)
            {
                if(texture != NULL)
                    texture->destroy();
                continue;
            }
            g_palModeTextures[i].texture = texture;
            g_palModeTextures[i].width =
                static_cast<uint16_t>(texture->raster->width);
            g_palModeTextures[i].height =
                static_cast<uint16_t>(texture->raster->height);
        }
        int ready = 0;
        for(int i = 0; i < PALMODE_IMAGE_COUNT; ++i)
        {
            if(g_palModeTextures[i].texture != NULL)
                ++ready;
        }
        printf("menu artwork %s textures=%d/%d (%ux%u, %ux%u, %ux%u)\n",
               path, ready, PALMODE_IMAGE_COUNT,
               g_palMode.images[PALMODE_IMAGE_MESS_E].width,
               g_palMode.images[PALMODE_IMAGE_MESS_E].height,
               g_palMode.images[PALMODE_IMAGE_VSCRN].width,
               g_palMode.images[PALMODE_IMAGE_VSCRN].height,
               g_palMode.images[PALMODE_IMAGE_TEST].width,
               g_palMode.images[PALMODE_IMAGE_TEST].height);
    }
    else
        printf("menu artwork %s unavailable: %s\n", path, error.c_str());
}

}

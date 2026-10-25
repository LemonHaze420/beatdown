#include "frame_capture.h"

#include <rw.h>

#include <stdio.h>
#include <string.h>
#include <filesystem>

namespace
{

struct FrameCaptureState
{
    char directory[512];
    char pendingPath[640];
    char frameShotPath[640];
    long frameShotIndex;
    long frameIndex;
    bool haveDirectory;
    bool havePending;
};

FrameCaptureState g_capture =
{
    { 0 }, { 0 }, { 0 }, -1, 0, false, false
};

const int CAPTURE_NAME_LIMIT = 64;
char g_captureNames[CAPTURE_NAME_LIMIT][64];
int g_captureNameCounts[CAPTURE_NAME_LIMIT];
int g_captureNameCount = 0;

int nameOccurrence(const char* name)
{
    for(int i = 0; i < g_captureNameCount; ++i)
    {
        if(strcmp(g_captureNames[i], name) == 0)
            return ++g_captureNameCounts[i];
    }
    if(g_captureNameCount >= CAPTURE_NAME_LIMIT)
        return 1;
    snprintf(g_captureNames[g_captureNameCount],
             sizeof(g_captureNames[0]), "%s", name);
    g_captureNameCounts[g_captureNameCount] = 1;
    ++g_captureNameCount;
    return 1;
}

rw::Raster* g_captureRaster = NULL;

bool ensureCaptureRaster(const rw::Raster& frameBuffer)
{
    if(g_captureRaster != NULL)
    {
        if(g_captureRaster->width == frameBuffer.width &&
           g_captureRaster->height == frameBuffer.height &&
           g_captureRaster->depth == frameBuffer.depth)
        {
            return true;
        }
        g_captureRaster->destroy();
        g_captureRaster = NULL;
    }
    g_captureRaster = rw::Raster::create(
        frameBuffer.width, frameBuffer.height, frameBuffer.depth,
        rw::Raster::CAMERATEXTURE);
    return g_captureRaster != NULL;
}

}

bool
captureFrameToPng(const char* path)
{
    if(path == NULL || path[0] == '\0')
        return false;
    if(rw::engine == NULL || rw::engine->currentCamera == NULL)
    {
        printf("frame capture: no camera, '%s' not written\n", path);
        return false;
    }

    rw::Raster* frameBuffer = rw::engine->currentCamera->frameBuffer;
    if(frameBuffer == NULL || !ensureCaptureRaster(*frameBuffer))
    {
        printf("frame capture: no readable frame buffer, '%s' not written\n",
               path);
        return false;
    }

    if(rw::Raster::pushContext(g_captureRaster) == NULL)
    {
        printf("frame capture: could not bind the capture raster\n");
        return false;
    }
    const bool copied = frameBuffer->renderFast(0, 0) != 0;
    rw::Raster::popContext();
    if(!copied)
    {
        printf("frame capture: frame buffer blit failed, '%s' not written\n",
               path);
        return false;
    }

    rw::Image* image = g_captureRaster->toImage();
    if(image == NULL)
    {
        printf("frame capture: raster read-back failed, '%s' not written\n",
               path);
        return false;
    }

    image->removeMask();

    rw::writePNG(image, path);
    image->destroy();
    FILE* output = fopen(path, "rb");
    if(output == NULL)
    {
        printf("frame capture: PNG encode failed, '%s' not written\n", path);
        return false;
    }
    fseek(output, 0, SEEK_END);
    const long outputSize = ftell(output);
    fclose(output);
    if(outputSize <= 0)
    {
        printf("frame capture: PNG encode produced an empty file, '%s'\n",
               path);
        return false;
    }
    printf("frame capture: wrote %s (%dx%d)\n", path,
           g_captureRaster->width, g_captureRaster->height);
    return true;
}

void
frameCaptureSetDirectory(const char* directory)
{
    if(directory == NULL || directory[0] == '\0')
    {
        g_capture.haveDirectory = false;
        g_capture.directory[0] = '\0';
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if(error || !std::filesystem::is_directory(directory))
    {
        printf("frame capture: could not create directory '%s'\n", directory);
        g_capture.haveDirectory = false;
        g_capture.directory[0] = '\0';
        return;
    }
    snprintf(g_capture.directory, sizeof(g_capture.directory), "%s",
             directory);
    g_capture.haveDirectory = true;
}

bool
frameCaptureEnabled(void)
{
    return g_capture.haveDirectory || g_capture.frameShotIndex >= 0;
}

void
frameCaptureRequest(const char* name)
{
    if(!g_capture.haveDirectory || name == NULL || name[0] == '\0')
        return;
    const int occurrence = nameOccurrence(name);
    if(occurrence <= 1)
    {
        snprintf(g_capture.pendingPath, sizeof(g_capture.pendingPath),
                 "%s/%s.png", g_capture.directory, name);
    }
    else
    {
        snprintf(g_capture.pendingPath, sizeof(g_capture.pendingPath),
                 "%s/%s_%d.png", g_capture.directory, name, occurrence);
    }
    g_capture.havePending = true;
}
void
frameCaptureRequestImmediate(const char* name)
{
    if(!g_capture.haveDirectory || name == NULL || name[0] == '\0')
        return;
    const int occurrence = nameOccurrence(name);
    char path[640];
    if(occurrence <= 1)
        snprintf(path, sizeof(path), "%s/%s.png",
                 g_capture.directory, name);
    else
        snprintf(path, sizeof(path), "%s/%s_%d.png",
                 g_capture.directory, name, occurrence);
    captureFrameToPng(path);
}

void
frameCaptureRequestFrame(long frame, const char* path)
{
    if(path == NULL || path[0] == '\0' || frame < 0)
        return;
    snprintf(g_capture.frameShotPath, sizeof(g_capture.frameShotPath), "%s",
             path);
    g_capture.frameShotIndex = frame;
}

void
frameCaptureService(void)
{
    const long frame = g_capture.frameIndex++;

    if(g_capture.frameShotIndex >= 0 && frame >= g_capture.frameShotIndex)
    {
        captureFrameToPng(g_capture.frameShotPath);
        g_capture.frameShotIndex = -1;
    }

    if(g_capture.havePending)
    {

        static bool armedLastFrame = false;
        if(armedLastFrame)
        {
            captureFrameToPng(g_capture.pendingPath);
            g_capture.havePending = false;
            armedLastFrame = false;
        }
        else
            armedLastFrame = true;
    }
}

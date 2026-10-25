#ifndef FRAME_CAPTURE_H
#define FRAME_CAPTURE_H

bool captureFrameToPng(const char* path);

void frameCaptureSetDirectory(const char* directory);

bool frameCaptureEnabled(void);

void frameCaptureRequest(const char* name);

void frameCaptureRequestImmediate(const char* name);

void frameCaptureRequestFrame(long frame, const char* path);

void frameCaptureService(void);

#endif

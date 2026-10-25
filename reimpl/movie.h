#ifndef MOVIE_H
#define MOVIE_H
#include <stddef.h>
#include <stdint.h>

bool startMoviePlayback(const char* path, void* nativeWindow);
void updateMoviePlayback(void);
bool moviePlaybackActive(void);
uint64_t moviePlaybackFrameNumber(void);
void stopMoviePlayback(void);
void terminateMoviePlayback(void);

#ifdef MOVIE_TEST
struct MovieTestInfo
{
    uint32_t videoPacketCount;
    uint32_t audioPacketCount;
    uint64_t firstVideoPts90k;
    uint32_t audioCodec;
    uint32_t audioSampleRate;
    uint16_t audioChannels;
    uint32_t audioInterleave;
};

bool movieTestInspectPss(const char* path, MovieTestInfo* info);
bool movieTestDecodeSs2Adpcm(const uint8_t* block, size_t blockSize,
                             int16_t* samples, size_t sampleCount);
#endif
#endif

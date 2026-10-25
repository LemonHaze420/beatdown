#ifndef SSL_H
#define SSL_H
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

const uint32_t SSL_FRAME_BYTES = 16;
const uint32_t SSL_FRAME_SAMPLES = 28;

const uint8_t SSL_FLAG_LOOP_END = 0x01;
const uint8_t SSL_FLAG_LOOP_BODY = 0x02;
const uint8_t SSL_FLAG_LOOP_START = 0x04;

constexpr char SSL_CHUNK_MIDI[8] = {'I', 'E', 'C', 'S', 'i', 'd', 'i', 'M'};
constexpr char SSL_CHUNK_PROG[8] = {'I', 'E', 'C', 'S', 'g', 'o', 'r', 'P'};
constexpr char SSL_CHUNK_SSET[8] = {'I', 'E', 'C', 'S', 't', 'e', 's', 'S'};
constexpr char SSL_CHUNK_SMPL[8] = {'I', 'E', 'C', 'S', 'l', 'p', 'm', 'S'};
constexpr char SSL_CHUNK_VAGI[8] = {'I', 'E', 'C', 'S', 'i', 'g', 'a', 'V'};
constexpr uint32_t SSL_HEADER_BYTES = 0x20;
constexpr uint32_t SSL_CHUNK_HEADER_BYTES = 0x0c;

constexpr int32_t PS_ADPCM_FILTER_A[5] = {0, 60, 115, 98, 122};
constexpr int32_t PS_ADPCM_FILTER_B[5] = {0, 0, -52, -55, -60};
struct SslDirectoryView
{
    const uint8_t* data;
    uint32_t size;
    uint32_t count;
};
struct SslStream
{

    uint32_t payloadOffset;

    uint32_t payloadLength;
    uint16_t sampleRate;

    uint8_t channelFlag;

    uint8_t reserved;
};

struct SslCueLayer
{
    uint16_t streamIndex;
    uint8_t programField0x10;
    uint8_t programField0x11;
    uint8_t sampleField0x04;
    uint8_t sampleField0x0d;
    uint8_t sampleField0x0b;
};
struct SslBank
{
    uint32_t version;
    uint32_t metadataStart;
    uint32_t audioStart;
    std::vector<SslStream> streams;

    std::vector<uint8_t> audio;

    std::vector<std::vector<SslCueLayer> > cues;
};

bool parseSslBank(const uint8_t* data, size_t length, SslBank& bank);
bool loadSslBank(const char* archivePath, const char* entryName,
                 SslBank& bank);

bool decodeSslStream(const SslBank& bank, size_t streamIndex,
                     std::vector<int16_t>& samples);

bool resolveSslCue(const SslBank& bank, uint8_t cue,
                   const std::vector<SslCueLayer>*& layers);

void decodePsAdpcm(const uint8_t* data, size_t length,
                   std::vector<int16_t>& samples);
#endif

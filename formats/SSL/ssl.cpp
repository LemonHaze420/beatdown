#include "ssl.h"
#include "../common.h"
#include "../XAF/xaf_runtime.h"
#include <string.h>
namespace
{
bool findChunk(const uint8_t* data, uint32_t start, uint32_t end,
               const char id[8], SslDirectoryView& view)
{
    uint32_t cursor = start;
    while(cursor + SSL_CHUNK_HEADER_BYTES <= end)
    {
        const uint32_t chunkSize = read32(data + cursor + 8);
        if(chunkSize < SSL_CHUNK_HEADER_BYTES || cursor + chunkSize > end)
            return false;
        if(memcmp(data + cursor, id, 8) == 0)
        {
            view.data = data + cursor;
            view.size = chunkSize;
            view.count = read32(view.data + 0x0c);
            return (uint64_t)0x10 + (uint64_t)view.count * 4 <= view.size;
        }
        cursor += chunkSize;
    }
    return false;
}
bool directoryPointer(const SslDirectoryView& view, uint32_t index,
                      uint32_t minimumBytes, uint32_t& pointer)
{
    if(index >= view.count)
        return false;
    pointer = read32(view.data + 0x10 + index * 4);
    return (uint64_t)pointer + minimumBytes <= view.size;
}
int16_t clampToSample(int32_t value)
{
    if(value > 32767)
        return 32767;
    if(value < -32768)
        return -32768;
    return (int16_t)value;
}
}
void
decodePsAdpcm(const uint8_t* data, size_t length, std::vector<int16_t>& samples)
{
    const size_t frames = length / SSL_FRAME_BYTES;
    samples.clear();
    samples.reserve(frames * SSL_FRAME_SAMPLES);
    int32_t history1 = 0;
    int32_t history2 = 0;
    for(size_t frame = 0; frame < frames; ++frame)
    {
        const uint8_t* block = data + frame * SSL_FRAME_BYTES;
        int32_t predictor = (block[0] >> 4) & 0x0f;
        const int32_t shift = block[0] & 0x0f;

        if(predictor > 4)
            predictor = 0;
        for(uint32_t i = 0; i < SSL_FRAME_SAMPLES; ++i)
        {
            const uint8_t packed = block[2 + i / 2];
            const uint8_t nibble = (i & 1) != 0 ? (uint8_t)(packed >> 4)
                                                : (uint8_t)(packed & 0x0f);

            int32_t value = (int32_t)(int16_t)((uint16_t)nibble << 12);
            value >>= shift;
            value += (history1 * PS_ADPCM_FILTER_A[predictor] +
                      history2 * PS_ADPCM_FILTER_B[predictor] + 32) >> 6;
            const int16_t sample = clampToSample(value);
            samples.push_back(sample);
            history2 = history1;
            history1 = sample;
        }
    }
}
bool
parseSslBank(const uint8_t* data, size_t length, SslBank& bank)
{
    if(data == NULL || length < SSL_HEADER_BYTES)
        return false;
    if(memcmp(data, "SSLDPS2 ", 8) != 0)
        return false;
    bank.version = read32(data + 0x08);
    const uint32_t headerSize = read32(data + 0x0c);
    bank.metadataStart = read32(data + 0x10);
    bank.audioStart = read32(data + 0x14);
    bank.streams.clear();
    bank.audio.clear();
    bank.cues.clear();
    if(headerSize != SSL_HEADER_BYTES)
        return false;
    if(bank.metadataStart < headerSize || bank.audioStart < bank.metadataStart)
        return false;
    if(bank.audioStart > length)
        return false;
    bank.audio.assign(data + bank.audioStart, data + length);
    const uint32_t payloadBytes = (uint32_t)bank.audio.size();

    const uint8_t* vagi = NULL;
    uint32_t vagiSize = 0;
    uint32_t cursor = bank.metadataStart;
    while(cursor + SSL_CHUNK_HEADER_BYTES <= bank.audioStart)
    {
        const uint32_t chunkSize = read32(data + cursor + 8);
        if(chunkSize < SSL_CHUNK_HEADER_BYTES ||
           cursor + chunkSize > bank.audioStart)
            return false;
        if(memcmp(data + cursor, SSL_CHUNK_VAGI, 8) == 0)
        {
            vagi = data + cursor;
            vagiSize = chunkSize;
        }
        cursor += chunkSize;
    }
    if(vagi == NULL)
        return false;

    const uint32_t count = read32(vagi + 0x0c);
    if((uint64_t)0x10 + (uint64_t)count * 4 > vagiSize)
        return false;
    bank.streams.reserve(count);
    for(uint32_t i = 0; i < count; ++i)
    {
        const uint32_t pointer = read32(vagi + 0x10 + i * 4);
        if((uint64_t)pointer + 8 > vagiSize)
            return false;
        SslStream stream;
        stream.payloadOffset = read32(vagi + pointer);
        stream.sampleRate = read16(vagi + pointer + 4);
        stream.channelFlag = vagi[pointer + 6];
        stream.reserved = vagi[pointer + 7];
        stream.payloadLength = 0;
        if(stream.payloadOffset > payloadBytes)
            return false;
        bank.streams.push_back(stream);
    }

    for(size_t i = 0; i < bank.streams.size(); ++i)
    {
        uint32_t end = payloadBytes;
        for(size_t j = 0; j < bank.streams.size(); ++j)
        {
            const uint32_t other = bank.streams[j].payloadOffset;
            if(other > bank.streams[i].payloadOffset && other < end)
                end = other;
        }
        bank.streams[i].payloadLength = end - bank.streams[i].payloadOffset;
    }

    SslDirectoryView midi;
    SslDirectoryView prog;
    SslDirectoryView sset;
    SslDirectoryView smpl;
    if(findChunk(data, headerSize, bank.metadataStart, SSL_CHUNK_MIDI, midi) &&
       findChunk(data, bank.metadataStart, bank.audioStart, SSL_CHUNK_PROG, prog) &&
       findChunk(data, bank.metadataStart, bank.audioStart, SSL_CHUNK_SSET, sset) &&
       findChunk(data, bank.metadataStart, bank.audioStart, SSL_CHUNK_SMPL, smpl))
    {
        bank.cues.resize(midi.count);
        for(uint32_t cue = 0; cue < midi.count; ++cue)
        {
            uint32_t midiPointer;
            if(!directoryPointer(midi, cue, 0x10, midiPointer) ||
               midi.data[midiPointer + 0x0e] != 0xc0 ||
               midi.data[midiPointer + 0x0f] < 0x80)
                continue;
            const uint32_t programIndex =
                midi.data[midiPointer + 0x0f] - 0x80;
            uint32_t programPointer;
            if(!directoryPointer(prog, programIndex, 0x24, programPointer))
                continue;
            uint32_t programEnd = prog.size;
            if(programIndex + 1 < prog.count)
            {
                if(!directoryPointer(prog, programIndex + 1, 0,
                                     programEnd) ||
                   programEnd < programPointer + 0x24)
                    continue;
            }
            for(uint32_t zone = programPointer + 0x24;
                zone + 0x14 <= programEnd; zone += 0x14)
            {
                const uint32_t setIndex = read16(prog.data + zone);
                uint32_t setPointer;
                if(!directoryPointer(sset, setIndex, 4, setPointer))
                    continue;
                const uint32_t layerCount = sset.data[setPointer + 3];
                if((uint64_t)setPointer + 4 + (uint64_t)layerCount * 2 >
                   sset.size)
                    continue;
                for(uint32_t layer = 0; layer < layerCount; ++layer)
                {
                    const uint32_t sampleIndex =
                        read16(sset.data + setPointer + 4 + layer * 2);
                    uint32_t samplePointer;
                    if(!directoryPointer(smpl, sampleIndex, 0x0e,
                                         samplePointer))
                        continue;
                    SslCueLayer resolved;
                    resolved.streamIndex = read16(smpl.data + samplePointer);
                    if(resolved.streamIndex >= bank.streams.size())
                        continue;
                    resolved.programField0x10 = prog.data[zone + 0x10];
                    resolved.programField0x11 = prog.data[zone + 0x11];
                    resolved.sampleField0x04 = smpl.data[samplePointer + 4];
                    resolved.sampleField0x0d = smpl.data[samplePointer + 0x0d];
                    resolved.sampleField0x0b = smpl.data[samplePointer + 0x0b];
                    bank.cues[cue].push_back(resolved);
                }
            }
        }
    }
    return true;
}
bool
loadSslBank(const char* archivePath, const char* entryName, SslBank& bank)
{
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, entryName, data) || data.empty())
        return false;
    return parseSslBank(&data[0], data.size(), bank);
}
bool
decodeSslStream(const SslBank& bank, size_t streamIndex,
                std::vector<int16_t>& samples)
{
    samples.clear();
    if(streamIndex >= bank.streams.size())
        return false;
    const SslStream& stream = bank.streams[streamIndex];
    if((size_t)stream.payloadOffset + stream.payloadLength > bank.audio.size())
        return false;
    if(stream.payloadLength < SSL_FRAME_BYTES)
        return false;
    decodePsAdpcm(&bank.audio[stream.payloadOffset], stream.payloadLength,
                  samples);
    return !samples.empty();
}
bool
resolveSslCue(const SslBank& bank, uint8_t cue,
              const std::vector<SslCueLayer>*& layers)
{
    layers = NULL;
    if(cue >= bank.cues.size() || bank.cues[cue].empty())
        return false;
    layers = &bank.cues[cue];
    return true;
}

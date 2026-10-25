#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif

#define WIN32_LEAN_AND_MEAN

#include "movie.h"
#include "audio.h"

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>

#define WITH_D3D
#include <rw.h>
#include <skeleton.h>
#ifdef MOVIE_TEST
namespace sk
{
Globals globals = {};
}
#endif

#include <algorithm>
#include <chrono>
#include <deque>
#include <utility>
#include <limits>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{
const GUID MOVIE_MPEG2_SUBTYPE =
{
    0xe06d8026, 0xdb46, 0x11cf,
    { 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea }
};

const int32_t MOVIE_ADPCM_COEFFICIENTS[5][2] =
{
    { 0, 0 }, { 60, 0 }, { 115, -52 }, { 98, -55 }, { 122, -60 }
};

const uint64_t NO_PTS = std::numeric_limits<uint64_t>::max();
const uint64_t CLOCK_HZ = 90000;
const uint32_t MAX_SEQUENCE_PROBE = 1024 * 1024;
const uint32_t MAX_PUMP_PACKETS = 192;

uint16_t readBig16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) << 8 | data[1];
}

uint32_t readLittle32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) |
           static_cast<uint32_t>(data[1]) << 8 |
           static_cast<uint32_t>(data[2]) << 16 |
           static_cast<uint32_t>(data[3]) << 24;
}

uint64_t readPts(const uint8_t* data)
{
    return (static_cast<uint64_t>((data[0] >> 1) & 7) << 30) |
           (static_cast<uint64_t>(data[1]) << 22) |
           (static_cast<uint64_t>((data[2] >> 1) & 0x7f) << 15) |
           (static_cast<uint64_t>(data[3]) << 7) |
           ((data[4] >> 1) & 0x7f);
}

bool sameGuid(const GUID& left, const GUID& right)
{
    return IsEqualGUID(left, right) != 0;
}

template <typename T>
class ComRef
{
public:
    ComRef() : value(NULL)
    {
    }

    ~ComRef()
    {
        reset();
    }

    ComRef(const ComRef&) = delete;
    ComRef& operator=(const ComRef&) = delete;

    T* get(void) const
    {
        return value;
    }

    T** put(void)
    {
        reset();
        return &value;
    }

    void adopt(T* object)
    {
        reset();
        value = object;
    }
    T* detach(void)
    {
        T* object = value;
        value = NULL;
        return object;
    }

    void reset(void)
    {
        if(value != NULL)
        {
            value->Release();
            value = NULL;
        }
    }

    T* operator->(void) const
    {
        return value;
    }

    operator bool(void) const
    {
        return value != NULL;
    }

private:
    T* value;
};

bool seekFile(FILE* file, uint64_t offset)
{
    return file != NULL && _fseeki64(file, static_cast<__int64>(offset),
                                     SEEK_SET) == 0;
}

bool readFile(FILE* file, uint64_t offset, void* destination, size_t size)
{
    if(size == 0)
        return true;
    if(file == NULL || destination == NULL || !seekFile(file, offset))
        return false;
    return fread(destination, 1, size, file) == size;
}

uint64_t findStartCode(FILE* file, uint64_t start, uint64_t end)
{
    if(file == NULL || start >= end)
        return end;

    std::vector<uint8_t> buffer(64 * 1024);
    uint64_t cursor = start;
    while(cursor < end)
    {
        const uint64_t chunkStart = cursor > start ? cursor - 2 : cursor;
        const size_t chunkSize = static_cast<size_t>(
            std::min<uint64_t>(buffer.size(), end - chunkStart));
        if(!readFile(file, chunkStart, buffer.data(), chunkSize))
            return end;
        for(size_t i = 0; i + 3 <= chunkSize; i++)
        {
            const uint64_t candidate = chunkStart + i;
            if(candidate < start)
                continue;
            if(buffer[i] == 0 && buffer[i + 1] == 0 && buffer[i + 2] == 1)
                return candidate;
        }
        if(chunkStart + chunkSize >= end)
            break;
        cursor = chunkStart + chunkSize;
    }
    return end;
}

struct PssPacket
{
    uint8_t streamId;
    uint64_t payloadOffset;
    uint32_t payloadSize;
    uint64_t pts90k;
};

bool parsePesHeader(const uint8_t* data, size_t size,
                   size_t* payloadOffset, uint64_t* pts90k)
{
    if(payloadOffset == NULL || pts90k == NULL)
        return false;
    *payloadOffset = 0;
    *pts90k = NO_PTS;
    if(data == NULL || size < 3)
        return false;

    if((data[0] & 0xc0) == 0x80)
    {
        const size_t headerLength = data[2];
        if(3 + headerLength > size)
            return false;
        const uint8_t ptsDtsFlags = static_cast<uint8_t>(data[1] >> 6);
        if(ptsDtsFlags >= 2 && headerLength >= 5)
            *pts90k = readPts(data + 3);
        *payloadOffset = 3 + headerLength;
        return true;
    }

    size_t offset = 0;
    while(offset < size && data[offset] == 0xff)
        offset++;
    if(offset >= size)
        return false;
    if((data[offset] & 0xf0) == 0x20 && offset + 5 <= size)
    {
        *pts90k = readPts(data + offset);
        offset += 5;
    }
    else if((data[offset] & 0xf0) == 0x30 && offset + 10 <= size)
    {
        *pts90k = readPts(data + offset);
        offset += 10;
    }
    else if(data[offset] == 0x0f)
    {
        offset++;
    }
    *payloadOffset = offset;
    return true;
}

class PssDemuxer
{
public:
    PssDemuxer() : file(NULL), size(0)
    {
    }

    ~PssDemuxer()
    {
        close();
    }

    bool open(const char* path)
    {
        close();
        if(path == NULL || path[0] == '\0')
            return false;
        file = fopen(path, "rb");
        if(file == NULL)
            return false;
        if(_fseeki64(file, 0, SEEK_END) != 0)
        {
            close();
            return false;
        }
        const __int64 end = _ftelli64(file);
        if(end <= 0)
        {
            close();
            return false;
        }
        size = static_cast<uint64_t>(end);
        if(!parse())
        {
            close();
            return false;
        }
        return hasVideo();
    }

    void close(void)
    {
        if(file != NULL)
            fclose(file);
        file = NULL;
        size = 0;
        packets.clear();
    }

    bool readPayload(const PssPacket& packet, std::vector<uint8_t>* output) const
    {
        if(output == NULL || packet.payloadSize == 0)
            return false;
        output->resize(packet.payloadSize);
        return readFile(file, packet.payloadOffset, output->data(),
                        packet.payloadSize);
    }

    bool readPayloadPrefix(const PssPacket& packet, size_t limit,
                           std::vector<uint8_t>* output) const
    {
        if(output == NULL || packet.payloadSize == 0)
            return false;
        const size_t amount = std::min<size_t>(packet.payloadSize, limit);
        output->resize(amount);
        return readFile(file, packet.payloadOffset, output->data(), amount);
    }

    bool hasVideo(void) const
    {
        for(size_t i = 0; i < packets.size(); i++)
        {
            if(packets[i].streamId >= 0xe0 && packets[i].streamId <= 0xef)
                return true;
        }
        return false;
    }

    FILE* stream(void) const
    {
        return file;
    }

    const std::vector<PssPacket>& streamPackets(void) const
    {
        return packets;
    }

private:
    bool parse(void)
    {
        uint64_t position = 0;
        while(position + 4 <= size)
        {
            uint8_t start[4];
            if(!readFile(file, position, start, sizeof(start)))
                return false;
            if(start[0] != 0 || start[1] != 0 || start[2] != 1)
            {
                position = findStartCode(file, position + 1, size);
                if(position >= size)
                    break;
                continue;
            }

            const uint8_t code = start[3];
            if(code == 0xba)
            {
                uint8_t pack[5];
                if(!readFile(file, position + 4, pack, sizeof(pack)))
                    return false;
                const uint64_t packSize = (pack[0] & 0xc0) == 0x40 ? 14 : 12;
                if(position + packSize > size)
                    return false;
                position += packSize;
                continue;
            }
            if(code == 0xbb)
            {
                uint8_t length[2];
                if(!readFile(file, position + 4, length, sizeof(length)))
                    return false;
                const uint64_t sectionSize = 6 + readBig16(length);
                if(position + sectionSize > size)
                    return false;
                position += sectionSize;
                continue;
            }
            if(code == 0xb9)
                break;
            if(position + 6 > size)
                return false;

            uint8_t lengthBytes[2];
            if(!readFile(file, position + 4, lengthBytes, sizeof(lengthBytes)))
                return false;
            const uint32_t packetLength = readBig16(lengthBytes);
            uint64_t packetEnd;
            if(packetLength == 0)
                packetEnd = findStartCode(file, position + 6, size);
            else
                packetEnd = position + 6 + packetLength;
            if(packetEnd <= position || packetEnd > size)
                return false;

            if((code >= 0xe0 && code <= 0xef) || code == 0xbd)
            {
                const size_t probeSize = std::min<uint32_t>(packetLength, 64);
                std::vector<uint8_t> probe(probeSize);
                if(probeSize != 0 && !readFile(file, position + 6,
                                                 probe.data(), probeSize))
                    return false;
                size_t payloadOffset;
                uint64_t pts90k;
                if(parsePesHeader(probe.data(), probe.size(),
                                  &payloadOffset, &pts90k) &&
                   payloadOffset < packetLength)
                {
                    PssPacket packet;
                    packet.streamId = code;
                    packet.payloadOffset = position + 6 + payloadOffset;
                    packet.payloadSize = packetLength == 0 ?
                        static_cast<uint32_t>(packetEnd - packet.payloadOffset) :
                        packetLength - static_cast<uint32_t>(payloadOffset);
                    packet.pts90k = pts90k;
                    if(packet.payloadSize != 0)
                        packets.push_back(packet);
                }
            }
            position = packetEnd;
        }
        return !packets.empty();
    }

    FILE* file;
    uint64_t size;
    std::vector<PssPacket> packets;
};

struct VideoFormat
{
    uint32_t width;
    uint32_t height;
    uint32_t frameRateNumerator;
    uint32_t frameRateDenominator;
    float frameRate;
    float displayAspect;
    uint32_t pixelAspectNumerator;
    uint32_t pixelAspectDenominator;
};

bool parseVideoFormat(const std::vector<uint8_t>& data, VideoFormat* format)
{
    if(format == NULL)
        return false;
    memset(format, 0, sizeof(*format));
    for(size_t i = 0; i + 8 <= data.size(); i++)
    {
        if(data[i] != 0 || data[i + 1] != 0 || data[i + 2] != 1 ||
           data[i + 3] != 0xb3)
            continue;
        const uint32_t width = static_cast<uint32_t>(data[i + 4]) << 4 |
                               (data[i + 5] >> 4);
        const uint32_t height = static_cast<uint32_t>(data[i + 5] & 0x0f) << 8 |
                                data[i + 6];
        const uint32_t aspectCode = data[i + 7] >> 4;
        const uint32_t rateCode = data[i + 7] & 0x0f;
        if(width == 0 || height == 0 || width > 4096 || height > 4096 ||
           rateCode < 1 || rateCode > 8)
            continue;

        static const uint32_t rateNumerator[9] =
        {
            0, 24000, 24, 25, 30000, 30, 50, 60000, 60
        };
        static const uint32_t rateDenominator[9] =
        {
            1, 1001, 1, 1, 1001, 1, 1, 1001, 1
        };
        float displayAspect;
        switch(aspectCode)
        {
        case 2: displayAspect = 4.0f / 3.0f; break;
        case 3: displayAspect = 16.0f / 9.0f; break;
        case 4: displayAspect = 2.21f; break;
        default: displayAspect = static_cast<float>(width) /
                                  static_cast<float>(height); break;
        }
        const float pixelAspect = displayAspect * static_cast<float>(height) /
                                   static_cast<float>(width);
        uint32_t pixelAspectNumerator = 1;
        uint32_t pixelAspectDenominator = 1;
        if(pixelAspect > 1.03f && pixelAspect < 1.10f)
        {
            pixelAspectNumerator = 16;
            pixelAspectDenominator = 15;
        }
        else if(pixelAspect > 0.86f && pixelAspect < 0.92f)
        {
            pixelAspectNumerator = 8;
            pixelAspectDenominator = 9;
        }

        format->width = width;
        format->height = height;
        format->frameRateNumerator = rateNumerator[rateCode];
        format->frameRateDenominator = rateDenominator[rateCode];
        format->frameRate = static_cast<float>(rateNumerator[rateCode]) /
                           static_cast<float>(rateDenominator[rateCode]);
        format->displayAspect = displayAspect;
        format->pixelAspectNumerator = pixelAspectNumerator;
        format->pixelAspectDenominator = pixelAspectDenominator;
        return true;
    }
    return false;
}

bool readVideoProbe(const PssDemuxer& demuxer, std::vector<uint8_t>* probe)
{
    if(probe == NULL)
        return false;
    probe->clear();
    const std::vector<PssPacket>& packets = demuxer.streamPackets();
    for(size_t i = 0; i < packets.size() && probe->size() < MAX_SEQUENCE_PROBE;
        i++)
    {
        if(packets[i].streamId < 0xe0 || packets[i].streamId > 0xef)
            continue;
        std::vector<uint8_t> payload;
        if(!demuxer.readPayload(packets[i], &payload))
            return false;
        const size_t amount = std::min<size_t>(payload.size(),
            MAX_SEQUENCE_PROBE - probe->size());
        probe->insert(probe->end(), payload.begin(), payload.begin() + amount);
    }
    return !probe->empty();
}

struct VideoFrame
{
    std::vector<uint8_t> pixels;
    uint64_t number;
    uint64_t pts90k;
};

class Mpeg2VideoDecoder
{
public:
    Mpeg2VideoDecoder() : width(0), height(0), stride(0), outputProvidesSamples(false),
        frameRateNumerator(0), frameRateDenominator(1), frameDuration90k(0),
        frameNumber(0), lastPts90k(NO_PTS), inputPts90k(NO_PTS),
        outputSubtype(MFVideoFormat_Base), draining(false)
    {
    }

    bool open(const VideoFormat& format)
    {
        close();
        width = format.width;
        height = format.height;
        frameRateNumerator = format.frameRateNumerator;
        frameRateDenominator = format.frameRateDenominator;
        frameDuration90k = static_cast<uint64_t>(CLOCK_HZ) *
                           frameRateDenominator / frameRateNumerator;

        MFT_REGISTER_TYPE_INFO inputInfo;
        inputInfo.guidMajorType = MFMediaType_Video;
        inputInfo.guidSubtype = MOVIE_MPEG2_SUBTYPE;
        IMFActivate** activates = NULL;
        UINT32 activateCount = 0;
        HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                               MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT,
                               &inputInfo, NULL, &activates, &activateCount);
        if(FAILED(hr) || activateCount == 0)
            return false;

        for(UINT32 i = 0; i < activateCount && !transform; i++)
        {
            IMFTransform* candidate = NULL;
            if(SUCCEEDED(activates[i]->ActivateObject(IID_PPV_ARGS(&candidate))) &&
               configure(candidate, format))
            {
                transform.adopt(candidate);
                candidate = NULL;
            }
            if(candidate != NULL)
                candidate->Release();
        }
        for(UINT32 i = 0; i < activateCount; i++)
            activates[i]->Release();
        CoTaskMemFree(activates);
        if(!transform)
            return false;

        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        return true;
    }

    void close(void)
    {
        frames.clear();
        transform.reset();
        inputType.reset();
        outputType.reset();
        width = 0;
        height = 0;
        stride = 0;
        frameNumber = 0;
        lastPts90k = NO_PTS;
        inputPts90k = NO_PTS;
        draining = false;
    }

    bool feed(const std::vector<uint8_t>& payload, uint64_t pts90k)
    {
        if(!transform || payload.empty())
            return false;
        inputPts90k = pts90k;
        ComRef<IMFMediaBuffer> buffer;
        if(FAILED(MFCreateMemoryBuffer(static_cast<DWORD>(payload.size()),
                                       buffer.put())))
            return false;
        BYTE* destination = NULL;
        DWORD maximumLength = 0;
        DWORD currentLength = 0;
        if(FAILED(buffer->Lock(&destination, &maximumLength, &currentLength)))
            return false;
        memcpy(destination, payload.data(), payload.size());
        buffer->Unlock();
        if(FAILED(buffer->SetCurrentLength(static_cast<DWORD>(payload.size()))))
            return false;

        ComRef<IMFSample> sample;
        if(FAILED(MFCreateSample(sample.put())) ||
           FAILED(sample->AddBuffer(buffer.get())))
            return false;
        if(pts90k != NO_PTS)
        {
            const LONGLONG time = static_cast<LONGLONG>(
                (pts90k * 10000000ULL) / CLOCK_HZ);
            sample->SetSampleTime(time);
            sample->SetSampleDuration(static_cast<LONGLONG>(
                (frameDuration90k * 10000000ULL) / CLOCK_HZ));
        }

        HRESULT hr = transform->ProcessInput(0, sample.get(), 0);
        if(hr == MF_E_NOTACCEPTING)
        {
            if(!drainOutput())
                return false;
            hr = transform->ProcessInput(0, sample.get(), 0);
        }
        if(FAILED(hr))
            return false;
        return drainOutput();
    }

    bool endOfStream(void)
    {
        if(!transform || draining)
            return !transform || !frames.empty();
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        draining = true;
        return drainOutput();
    }
    const VideoFrame& back(void) const
    {
        return frames.back();
    }

    bool empty(void) const
    {
        return frames.empty();
    }

    const VideoFrame& front(void) const
    {
        return frames.front();
    }

    void pop(void)
    {
        if(!frames.empty())
            frames.pop_front();
    }

    size_t frameCount(void) const
    {
        return frames.size();
    }

    uint32_t videoWidth(void) const
    {
        return width;
    }

    uint32_t videoHeight(void) const
    {
        return height;
    }

private:
    bool configure(IMFTransform* candidate, const VideoFormat& format)
    {
        ComRef<IMFMediaType> input;
        if(FAILED(MFCreateMediaType(input.put())))
            return false;
        if(FAILED(input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
           FAILED(input->SetGUID(MF_MT_SUBTYPE, MOVIE_MPEG2_SUBTYPE)) ||
           FAILED(MFSetAttributeSize(input.get(), MF_MT_FRAME_SIZE,
                                     format.width, format.height)) ||
           FAILED(MFSetAttributeRatio(input.get(), MF_MT_FRAME_RATE,
                                      format.frameRateNumerator,
                                      format.frameRateDenominator)) ||
           FAILED(MFSetAttributeRatio(input.get(), MF_MT_PIXEL_ASPECT_RATIO,
                                      format.pixelAspectNumerator,
                                      format.pixelAspectDenominator)))
            return false;
        if(FAILED(candidate->SetInputType(0, input.get(), 0)))
            return false;
        if(!chooseOutput(candidate, format))
            return false;
        inputType.adopt(input.detach());
        return true;
    }

    bool chooseOutput(IMFTransform* candidate, const VideoFormat& format)
    {
        for(DWORD index = 0; index < 64; index++)
        {
            IMFMediaType* available = NULL;
            HRESULT hr = candidate->GetOutputAvailableType(0, index, &available);
            if(hr == MF_E_NO_MORE_TYPES)
                break;
            if(FAILED(hr) || available == NULL)
                continue;
            GUID major;
            GUID subtype;
            const bool usable = SUCCEEDED(available->GetGUID(MF_MT_MAJOR_TYPE,
                                                               &major)) &&
                                SUCCEEDED(available->GetGUID(MF_MT_SUBTYPE,
                                                              &subtype)) &&
                                sameGuid(major, MFMediaType_Video) &&
                                isSupportedSubtype(subtype);
            if(usable && SUCCEEDED(candidate->SetOutputType(0, available, 0)))
            {
                outputType.adopt(available);
                updateOutputDescription(format);
                return true;
            }
            available->Release();
        }

        static const GUID subtypes[] =
        {
            MFVideoFormat_RGB32, MFVideoFormat_ARGB32, MFVideoFormat_NV12,
            MFVideoFormat_YUY2, MFVideoFormat_I420
        };
        for(size_t i = 0; i < sizeof(subtypes) / sizeof(subtypes[0]); i++)
        {
            ComRef<IMFMediaType> requested;
            if(FAILED(MFCreateMediaType(requested.put())) ||
               FAILED(requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)) ||
               FAILED(requested->SetGUID(MF_MT_SUBTYPE, subtypes[i])) ||
               FAILED(MFSetAttributeSize(requested.get(), MF_MT_FRAME_SIZE,
                                         format.width, format.height)) ||
               FAILED(MFSetAttributeRatio(requested.get(), MF_MT_FRAME_RATE,
                                          format.frameRateNumerator,
                                          format.frameRateDenominator)) ||
               FAILED(candidate->SetOutputType(0, requested.get(), 0)))
                continue;
            outputType.adopt(requested.detach());
            updateOutputDescription(format);
            return true;
        }
        return false;
    }

    bool updateOutputDescription(const VideoFormat& format)
    {
        if(!outputType)
            return false;
        if(FAILED(outputType->GetGUID(MF_MT_SUBTYPE, &outputSubtype)))
            return false;
        UINT32 value = 0;
        if(SUCCEEDED(outputType->GetUINT32(MF_MT_DEFAULT_STRIDE, &value)))
            stride = static_cast<int32_t>(value);
        else if(sameGuid(outputSubtype, MFVideoFormat_RGB32) ||
                sameGuid(outputSubtype, MFVideoFormat_ARGB32))
            stride = static_cast<int32_t>(format.width * 4);
        else if(sameGuid(outputSubtype, MFVideoFormat_YUY2))
            stride = static_cast<int32_t>(format.width * 2);
        else
            stride = static_cast<int32_t>(format.width);
        return true;
    }

    static bool isSupportedSubtype(const GUID& subtype)
    {
        return sameGuid(subtype, MFVideoFormat_RGB32) ||
               sameGuid(subtype, MFVideoFormat_ARGB32) ||
               sameGuid(subtype, MFVideoFormat_NV12) ||
               sameGuid(subtype, MFVideoFormat_YUY2) ||
               sameGuid(subtype, MFVideoFormat_I420) ||
               sameGuid(subtype, MFVideoFormat_IYUV);
    }

    bool drainOutput(void)
    {
        if(!transform)
            return false;
        for(uint32_t attempt = 0; attempt < 32; attempt++)
        {
            MFT_OUTPUT_STREAM_INFO streamInfo;
            memset(&streamInfo, 0, sizeof(streamInfo));
            if(FAILED(transform->GetOutputStreamInfo(0, &streamInfo)))
                return false;
            ComRef<IMFSample> providedSample;
            MFT_OUTPUT_DATA_BUFFER output;
            memset(&output, 0, sizeof(output));
            output.dwStreamID = 0;
            if((streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0)
            {
                DWORD bufferSize = streamInfo.cbSize;
                const DWORD minimumSize = sameGuid(outputSubtype,
                    MFVideoFormat_RGB32) || sameGuid(outputSubtype,
                    MFVideoFormat_ARGB32) ? width * height * 4 :
                    width * height * 2;
                if(bufferSize < minimumSize)
                    bufferSize = minimumSize;
                ComRef<IMFMediaBuffer> buffer;
                if(FAILED(MFCreateMemoryBuffer(bufferSize, buffer.put())) ||
                   FAILED(MFCreateSample(providedSample.put())) ||
                   FAILED(providedSample->AddBuffer(buffer.get())))
                    return false;
                output.pSample = providedSample.get();
            }

            DWORD status = 0;
            HRESULT hr = transform->ProcessOutput(0, 1, &output, &status);
            if(hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
            {
                if(output.pEvents != NULL)
                    output.pEvents->Release();
                if(output.pSample != NULL &&
                   output.pSample != providedSample.get())
                    output.pSample->Release();
                return true;
            }
            if(hr == MF_E_TRANSFORM_STREAM_CHANGE)
            {
                if(output.pEvents != NULL)
                    output.pEvents->Release();
                if(output.pSample != NULL &&
                   output.pSample != providedSample.get())
                    output.pSample->Release();
                return chooseOutput(transform.get(), VideoFormat{
                    width, height, frameRateNumerator, frameRateDenominator,
                    0.0f, 0.0f, 1, 1});
            }
            if(FAILED(hr))
            {
                if(output.pEvents != NULL)
                    output.pEvents->Release();
                if(output.pSample != NULL &&
                   output.pSample != providedSample.get())
                    output.pSample->Release();
                return false;
            }

            IMFSample* sample = output.pSample != NULL ? output.pSample :
                                providedSample.get();
            if(sample != NULL)
            {
                if(!copyOutput(sample))
                {
                    if(output.pEvents != NULL)
                        output.pEvents->Release();
                    if(output.pSample != NULL &&
                       output.pSample != providedSample.get())
                        output.pSample->Release();
                    return false;
                }
            }
            if(output.pEvents != NULL)
                output.pEvents->Release();
            if(output.pSample != NULL && output.pSample != providedSample.get())
                output.pSample->Release();
            if((status & MFT_OUTPUT_DATA_BUFFER_INCOMPLETE) == 0 &&
               sample == NULL)
                return true;
        }
        return true;
    }

    bool copyOutput(IMFSample* sample)
    {
        ComRef<IMFMediaBuffer> buffer;
        if(FAILED(sample->ConvertToContiguousBuffer(buffer.put())))
            return false;
        BYTE* source = NULL;
        DWORD maximumLength = 0;
        DWORD currentLength = 0;
        if(FAILED(buffer->Lock(&source, &maximumLength, &currentLength)))
            return false;

        VideoFrame frame;
        frame.number = frameNumber++;
        frame.pts90k = NO_PTS;
        LONGLONG sampleTime = 0;
        if(SUCCEEDED(sample->GetSampleTime(&sampleTime)) && sampleTime >= 0)
            frame.pts90k = static_cast<uint64_t>(
                (static_cast<uint64_t>(sampleTime) * CLOCK_HZ) / 10000000ULL);
        if(frame.pts90k == NO_PTS ||
           (lastPts90k != NO_PTS && frame.pts90k <= lastPts90k))
            frame.pts90k = lastPts90k == NO_PTS ? 0 : lastPts90k + frameDuration90k;
        lastPts90k = frame.pts90k;
        frame.pixels.resize(static_cast<size_t>(width) * height * 4);

        const int32_t sourceStride = stride == 0 ? static_cast<int32_t>(width) :
                                      stride;
        const int32_t absoluteStride = sourceStride < 0 ? -sourceStride :
                                        sourceStride;
        bool copied = false;
        if(sameGuid(outputSubtype, MFVideoFormat_RGB32) ||
           sameGuid(outputSubtype, MFVideoFormat_ARGB32))
        {
            if(currentLength >= static_cast<DWORD>(absoluteStride * height))
            {
                for(uint32_t y = 0; y < height; y++)
                {
                    const uint32_t sourceY = sourceStride < 0 ? height - 1 - y : y;
                    memcpy(frame.pixels.data() + static_cast<size_t>(y) * width * 4,
                           source + static_cast<size_t>(sourceY) * absoluteStride,
                           width * 4);
                }
                copied = true;
            }
        }
        else if(sameGuid(outputSubtype, MFVideoFormat_NV12))
        {
            const size_t required = static_cast<size_t>(absoluteStride) * height +
                static_cast<size_t>(absoluteStride) * ((height + 1) / 2);
            if(currentLength >= required)
            {
                const uint8_t* yPlane = source;
                const uint8_t* uvPlane = source +
                    static_cast<size_t>(absoluteStride) * height;
                for(uint32_t y = 0; y < height; y++)
                {
                    for(uint32_t x = 0; x < width; x++)
                    {
                        const int32_t luminance = yPlane[
                            static_cast<size_t>(y) * absoluteStride + x];
                        const size_t uvIndex = static_cast<size_t>(y / 2) *
                            absoluteStride + (x & ~1U);
                        const int32_t u = uvPlane[uvIndex];
                        const int32_t v = uvPlane[uvIndex + 1];
                        writeYuv(frame.pixels.data() +
                                 (static_cast<size_t>(y) * width + x) * 4,
                                 luminance, u, v);
                    }
                }
                copied = true;
            }
        }
        else if(sameGuid(outputSubtype, MFVideoFormat_YUY2))
        {
            const size_t required = static_cast<size_t>(absoluteStride) * height;
            if(currentLength >= required)
            {
                for(uint32_t y = 0; y < height; y++)
                {
                    const uint8_t* row = source + static_cast<size_t>(y) *
                                         absoluteStride;
                    for(uint32_t x = 0; x < width; x += 2)
                    {
                        const size_t sourceIndex = static_cast<size_t>(x) * 2;
                        const int32_t y0 = row[sourceIndex];
                        const int32_t u = row[sourceIndex + 1];
                        const int32_t y1 = row[sourceIndex + 2];
                        const int32_t v = row[sourceIndex + 3];
                        writeYuv(frame.pixels.data() +
                                 (static_cast<size_t>(y) * width + x) * 4,
                                 y0, u, v);
                        if(x + 1 < width)
                            writeYuv(frame.pixels.data() +
                                     (static_cast<size_t>(y) * width + x + 1) * 4,
                                     y1, u, v);
                    }
                }
                copied = true;
            }
        }
        else if(sameGuid(outputSubtype, MFVideoFormat_I420) ||
                sameGuid(outputSubtype, MFVideoFormat_IYUV))
        {
            const size_t planeSize = static_cast<size_t>(width) * height;
            const size_t chromaSize = static_cast<size_t>((width + 1) / 2) *
                                       ((height + 1) / 2);
            if(currentLength >= planeSize + chromaSize * 2)
            {
                const uint8_t* yPlane = source;
                const uint8_t* uPlane = source + planeSize;
                const uint8_t* vPlane = uPlane + chromaSize;
                for(uint32_t y = 0; y < height; y++)
                {
                    for(uint32_t x = 0; x < width; x++)
                    {
                        const uint32_t chromaX = x / 2;
                        const uint32_t chromaY = y / 2;
                        const size_t chromaIndex = static_cast<size_t>(chromaY) *
                            ((width + 1) / 2) + chromaX;
                        writeYuv(frame.pixels.data() +
                                 (static_cast<size_t>(y) * width + x) * 4,
                                 yPlane[static_cast<size_t>(y) * width + x],
                                 uPlane[chromaIndex], vPlane[chromaIndex]);
                    }
                }
                copied = true;
            }
        }
        buffer->Unlock();
        if(!copied)
            return false;
        frames.push_back(std::move(frame));
        while(frames.size() > 6)
            frames.pop_front();
        return true;
    }

    static void writeYuv(uint8_t* destination, int32_t y, int32_t u, int32_t v)
    {
        const int32_t c = std::max(0, y - 16);
        const int32_t d = u - 128;
        const int32_t e = v - 128;
        destination[0] = static_cast<uint8_t>(std::max(0, std::min(255,
            (298 * c + 516 * d + 128) >> 8)));
        destination[1] = static_cast<uint8_t>(std::max(0, std::min(255,
            (298 * c - 100 * d - 208 * e + 128) >> 8)));
        destination[2] = static_cast<uint8_t>(std::max(0, std::min(255,
            (298 * c + 409 * e + 128) >> 8)));
        destination[3] = 255;
    }

    uint32_t width;
    uint32_t height;
    int32_t stride;
    bool outputProvidesSamples;
    uint32_t frameRateNumerator;
    uint32_t frameRateDenominator;
    uint64_t frameDuration90k;
    uint64_t frameNumber;
    uint64_t lastPts90k;
    uint64_t inputPts90k;
    GUID outputSubtype;
    bool draining;
    ComRef<IMFTransform> transform;
    ComRef<IMFMediaType> inputType;
    ComRef<IMFMediaType> outputType;
    std::deque<VideoFrame> frames;
};

bool decodePsxAdpcmBlock(const uint8_t* source, int32_t* previous1,
                         int32_t* previous2, int16_t* destination,
                         uint32_t stride)
{
    if(source == NULL || previous1 == NULL || previous2 == NULL ||
       destination == NULL || stride == 0)
        return false;
    const uint8_t predictor = source[0] >> 4;
    const uint8_t shift = source[0] & 0x0f;
    if(predictor >= 5 || shift > 12)
        return false;
    for(uint32_t sample = 0; sample < 28; sample++)
    {
        const uint8_t packed = source[2 + sample / 2];
        int32_t value = sample & 1 ? packed >> 4 : packed & 0x0f;
        if(value >= 8)
            value -= 16;
        value = (value << 12) >> shift;
        value += (*previous1 * MOVIE_ADPCM_COEFFICIENTS[predictor][0] +
                  *previous2 * MOVIE_ADPCM_COEFFICIENTS[predictor][1] + 32) >> 6;
        value = std::max(-32768, std::min(32767, value));
        destination[static_cast<size_t>(sample) * stride] =
            static_cast<int16_t>(value);
        *previous2 = *previous1;
        *previous1 = value;
    }
    return true;
}

class MovieAudioDecoder
{
public:
    MovieAudioDecoder() : valid(false), enabled(false), headerSeen(false),
        bodyStarted(false), codec(0), sampleRate(0), channels(0), interleave(0),
        bodySize(0), bodyRead(0), pcmRead(0), previous1{0, 0}, previous2{0, 0}
    {
    }

    bool open(const PssDemuxer& demuxer)
    {
        close();
        const std::vector<PssPacket>& packets = demuxer.streamPackets();
        for(size_t i = 0; i < packets.size(); i++)
        {
            if(packets[i].streamId != 0xbd)
                continue;
            std::vector<uint8_t> payload;
            if(!demuxer.readPayloadPrefix(packets[i], 1024, &payload))
                return false;
            const size_t header = findSignature(payload, "SShd");
            const size_t body = findSignature(payload, "SSbd");
            if(header == payload.size() || body == payload.size() ||
               header + 28 > payload.size() || body + 8 > payload.size())
                continue;
            const uint8_t* h = payload.data() + header;
            if(readLittle32(h + 4) != 0x18 && readLittle32(h + 4) != 0x20)
                continue;
            codec = readLittle32(h + 8);
            sampleRate = readLittle32(h + 12);
            channels = static_cast<uint16_t>(readLittle32(h + 16));
            interleave = readLittle32(h + 20);
            bodySize = readLittle32(payload.data() + body + 4);
            if((codec != 1 && codec != 2 && codec != 0x10) ||
               (channels != 1 && channels != 2) || sampleRate < 4000 ||
               sampleRate > 192000 || interleave < 16 || interleave > 0x10000 ||
               (interleave & 0x0f) != 0 || bodySize == 0)
            {
                close();
                return false;
            }
            valid = true;
            return true;
        }
        return false;
    }

    void close(void)
    {
        if(enabled)
            endMoviePcmStream();
        valid = false;
        enabled = false;
        headerSeen = false;
        bodyStarted = false;
        codec = 0;
        sampleRate = 0;
        channels = 0;
        interleave = 0;
        bodySize = 0;
        bodyRead = 0;
        compressed.clear();
        pcm.clear();
        pcmRead = 0;
        previous1[0] = previous1[1] = 0;
        previous2[0] = previous2[1] = 0;
    }

    bool begin(void)
    {
        if(!valid)
            return true;
        enabled = beginMoviePcmStream(sampleRate, channels);
        return true;
    }

    bool feed(const PssDemuxer& demuxer, const PssPacket& packet)
    {
        if(!valid)
            return true;
        std::vector<uint8_t> payload;
        if(!demuxer.readPayload(packet, &payload))
            return false;
        size_t offset = 0;
        while(offset < payload.size() && payload[offset] == 0xff)
            offset++;
        if(offset < payload.size() && payload[offset] == 0xa0)
        {
            offset++;
            if(offset + 2 <= payload.size() && payload[offset] == 0 &&
               payload[offset + 1] == 0)
                offset += 2;
        }
        if(!bodyStarted)
        {
            const size_t body = findSignature(payload, "SSbd");
            if(body == payload.size() || body + 8 > payload.size())
                return true;
            offset = body + 8;
            bodyStarted = true;
            headerSeen = true;
        }
        if(offset < payload.size() && bodyRead < bodySize)
        {
            const size_t amount = std::min<size_t>(payload.size() - offset,
                static_cast<size_t>(bodySize - bodyRead));
            compressed.insert(compressed.end(), payload.begin() + offset,
                              payload.begin() + offset + amount);
            bodyRead += static_cast<uint32_t>(amount);
        }
        if(!decodeAvailable())
            return false;
        return flushPcm();
    }

    bool flush(void)
    {
        return flushPcm();
    }

    bool finish(void)
    {
        if(!flushPcm())
            return false;
        if(!valid)
            return true;
        return bodyRead == bodySize && compressed.empty();
    }

    bool isBacklogged(void) const
    {
        return pcmRead < pcm.size();
    }

private:
    static size_t findSignature(const std::vector<uint8_t>& data,
                                const char signature[5])
    {
        if(data.size() < 4)
            return data.size();
        for(size_t i = 0; i + 4 <= data.size(); i++)
            if(memcmp(data.data() + i, signature, 4) == 0)
                return i;
        return data.size();
    }

    bool decodeAvailable(void)
    {
        const size_t groupSize = static_cast<size_t>(interleave) * channels;
        if(groupSize == 0)
            return false;
        while(compressed.size() >= groupSize)
        {
            if(pcm.size() - pcmRead > 32768 * channels)
                break;
            const uint32_t framesPerChannel = codec == 1 ? interleave / 2 :
                                              interleave / 16 * 28;
            std::vector<int16_t> decoded(static_cast<size_t>(framesPerChannel) *
                                          channels);
            if(codec == 1)
            {
                for(uint16_t channel = 0; channel < channels; channel++)
                {
                    const uint8_t* source = compressed.data() +
                        static_cast<size_t>(channel) * interleave;
                    for(uint32_t frame = 0; frame < framesPerChannel; frame++)
                    {
                        const size_t offset = static_cast<size_t>(frame) * 2;
                        decoded[static_cast<size_t>(frame) * channels + channel] =
                            static_cast<int16_t>(source[offset] |
                                static_cast<uint16_t>(source[offset + 1]) << 8);
                    }
                }
            }
            else
            {
                for(uint16_t channel = 0; channel < channels; channel++)
                {
                    const uint8_t* source = compressed.data() +
                        static_cast<size_t>(channel) * interleave;
                    int32_t previous1Value = previous1[channel];
                    int32_t previous2Value = previous2[channel];
                    uint32_t output = 0;
                    for(uint32_t offset = 0; offset < interleave; offset += 16)
                    {
                        if(!decodePsxAdpcmBlock(
                               source + offset, &previous1Value, &previous2Value,
                               decoded.data() + channel +
                                   static_cast<size_t>(output) * channels,
                               channels))
                            return false;
                        output += 28;
                    }
                    previous1[channel] = previous1Value;
                    previous2[channel] = previous2Value;
                }
            }
            pcm.insert(pcm.end(), decoded.begin(), decoded.end());
            compressed.erase(compressed.begin(), compressed.begin() + groupSize);
        }
        return true;
    }

    bool flushPcm(void)
    {
        if(!enabled)
        {
            pcm.clear();
            pcmRead = 0;
            return true;
        }
        while(pcmRead < pcm.size())
        {
            const size_t availableSamples = pcm.size() - pcmRead;
            const uint32_t frameCount = static_cast<uint32_t>(std::min<size_t>(
                8192, availableSamples / channels));
            if(frameCount == 0)
                break;
            if(!queueMoviePcm(pcm.data() + pcmRead * channels, frameCount))
                break;
            pcmRead += static_cast<size_t>(frameCount) * channels;
        }
        if(pcmRead == pcm.size())
        {
            pcm.clear();
            pcmRead = 0;
        }
        return true;
    }

    bool valid;
    bool enabled;
    bool headerSeen;
    bool bodyStarted;
    uint32_t codec;
    uint32_t sampleRate;
    uint16_t channels;
    uint32_t interleave;
    uint32_t bodySize;
    uint32_t bodyRead;
    std::vector<uint8_t> compressed;
    std::vector<int16_t> pcm;
    size_t pcmRead;
    int32_t previous1[2];
    int32_t previous2[2];
};

bool g_mediaFoundationStarted = false;
bool g_comInitialized = false;

bool initializeMediaFoundation(void)
{
    if(g_mediaFoundationStarted)
        return true;
    const HRESULT comResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if(comResult == S_OK || comResult == S_FALSE)
        g_comInitialized = true;
    else if(comResult != RPC_E_CHANGED_MODE)
        return false;
    if(FAILED(MFStartup(MF_VERSION, MFSTARTUP_FULL)))
    {
        if(g_comInitialized)
        {
            CoUninitialize();
            g_comInitialized = false;
        }
        return false;
    }
    g_mediaFoundationStarted = true;
    return true;
}

void shutdownMediaFoundation(void)
{
    if(g_mediaFoundationStarted)
    {
        MFShutdown();
        g_mediaFoundationStarted = false;
    }
    if(g_comInitialized)
    {
        CoUninitialize();
        g_comInitialized = false;
    }
}

struct MovieState
{
    MovieState() : active(false), decoderDrained(false), packetIndex(0),
        firstVideoPts(NO_PTS), texture(NULL), width(0), height(0),
        displayAspect(4.0f / 3.0f), frameRate(30.0f)
    {
    }

    void resetResources(void)
    {
        audio.close();
        decoder.close();
        if(texture != NULL)
        {
            texture->destroy();
            texture = NULL;
        }
        demuxer.close();
        active = false;
        decoderDrained = false;
        packetIndex = 0;
        firstVideoPts = NO_PTS;
        current = VideoFrame();
        width = 0;
        height = 0;
        displayAspect = 4.0f / 3.0f;
        frameRate = 30.0f;
    }

    bool createTexture(void)
    {
        rw::Image* image = rw::Image::create(static_cast<int32_t>(width),
                                              static_cast<int32_t>(height), 32);
        if(image == NULL)
            return false;
        image->allocate();
        if(image->pixels == NULL)
        {
            image->destroy();
            return false;
        }
        memset(image->pixels, 0, static_cast<size_t>(image->stride) * height);
        rw::Raster* raster = rw::Raster::createFromImage(image);
        image->destroy();
        if(raster == NULL)
            return false;
        texture = rw::Texture::create(raster);
        if(texture == NULL)
        {
            raster->destroy();
            return false;
        }
        texture->setFilter(rw::Texture::LINEAR);
        texture->setAddressU(rw::Texture::CLAMP);
        texture->setAddressV(rw::Texture::CLAMP);
        return true;
    }

    bool updateTexture(void)
    {
        if(texture == NULL || current.pixels.empty())
            return false;
        rw::Raster* raster = texture->raster;
        if(raster == NULL)
            return false;
        uint8_t* destination = raster->lock(0, rw::Raster::LOCKWRITE |
                                                rw::Raster::LOCKNOFETCH);
        if(destination == NULL)
            return false;
        const int32_t stride = raster->stride > 0 ? raster->stride :
                               static_cast<int32_t>(width * 4);
        for(uint32_t y = 0; y < height; y++)
            memcpy(destination + static_cast<size_t>(y) * stride,
                   current.pixels.data() + static_cast<size_t>(y) * width * 4,
                   width * 4);
        raster->unlock(0);
        return true;
    }

    PssDemuxer demuxer;
    Mpeg2VideoDecoder decoder;
    MovieAudioDecoder audio;
    bool active;
    bool decoderDrained;
    size_t packetIndex;
    uint64_t firstVideoPts;
    std::chrono::steady_clock::time_point clockStart;
    rw::Texture* texture;
    VideoFrame current;
    uint32_t width;
    uint32_t height;
    float displayAspect;
    float frameRate;
};

MovieState g_movie;

bool renderMovieFrame(void)
{
    if(g_movie.texture == NULL || g_movie.current.pixels.empty() ||
       rw::engine == NULL || rw::engine->currentCamera == NULL)
        return false;

    rw::Camera* camera = static_cast<rw::Camera*>(rw::engine->currentCamera);
    const float windowWidth = static_cast<float>(std::max(1, sk::globals.width));
    const float windowHeight = static_cast<float>(std::max(1, sk::globals.height));
    const float windowAspect = windowWidth / windowHeight;
    float drawWidth = windowWidth;
    float drawHeight = windowHeight;

    if(g_movie.displayAspect > 0.0f)
    {
        if(windowAspect > g_movie.displayAspect)
            drawWidth = drawHeight * g_movie.displayAspect;
        else
            drawHeight = drawWidth / g_movie.displayAspect;
    }
    const float left = (windowWidth - drawWidth) * 0.5f;
    const float top = (windowHeight - drawHeight) * 0.5f;
    const float right = left + drawWidth;
    const float bottom = top + drawHeight;

    using namespace rw::RWDEVICE;
    Im2DVertex vertices[4];
    float xOffset = 0.0f;
    float yOffset = 0.0f;
#ifdef RWHALFPIXEL
    xOffset = -0.5f;
    yOffset = 0.5f;
#endif
    const float reciprocalZ = 1.0f / camera->nearPlane;
    const float z = rw::im2d::GetNearZ();

    const float xs[4] = { left, right, right, left };
    const float ys[4] = { top, top, bottom, bottom };
    const float us[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float vs[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    for(int i = 0; i < 4; i++)
    {
        vertices[i].setScreenX(xs[i] + xOffset);
        vertices[i].setScreenY(ys[i] + yOffset);
        vertices[i].setScreenZ(z);
        vertices[i].setCameraZ(camera->nearPlane);
        vertices[i].setRecipCameraZ(reciprocalZ);
        vertices[i].setColor(255, 255, 255, 255);
        vertices[i].setU(us[i], reciprocalZ);
        vertices[i].setV(vs[i], reciprocalZ);
    }
    static rw::uint16 indices[6] = { 0, 1, 2, 0, 2, 3 };

    const void* oldTexture = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    const uint32_t oldFilter = rw::GetRenderState(rw::TEXTUREFILTER);
    const uint32_t oldAddressU = rw::GetRenderState(rw::TEXTUREADDRESSU);
    const uint32_t oldAddressV = rw::GetRenderState(rw::TEXTUREADDRESSV);
    const uint32_t oldVertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
    const uint32_t oldZTest = rw::GetRenderState(rw::ZTESTENABLE);
    const uint32_t oldZWrite = rw::GetRenderState(rw::ZWRITEENABLE);
    const uint32_t oldCull = rw::GetRenderState(rw::CULLMODE);
    rw::SetRenderStatePtr(rw::TEXTURERASTER, g_movie.texture->raster);
    rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::LINEAR);
    rw::SetRenderState(rw::TEXTUREADDRESSU, rw::Texture::CLAMP);
    rw::SetRenderState(rw::TEXTUREADDRESSV, rw::Texture::CLAMP);
    rw::SetRenderState(rw::VERTEXALPHA, 0);
    rw::SetRenderState(rw::ZTESTENABLE, 0);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);
    rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);
    rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, vertices, 4,
                                     indices, 6);
    rw::SetRenderStatePtr(rw::TEXTURERASTER, const_cast<void*>(oldTexture));
    rw::SetRenderState(rw::TEXTUREFILTER, oldFilter);
    rw::SetRenderState(rw::TEXTUREADDRESSU, oldAddressU);
    rw::SetRenderState(rw::TEXTUREADDRESSV, oldAddressV);
    rw::SetRenderState(rw::VERTEXALPHA, oldVertexAlpha);
    rw::SetRenderState(rw::ZTESTENABLE, oldZTest);
    rw::SetRenderState(rw::ZWRITEENABLE, oldZWrite);
    rw::SetRenderState(rw::CULLMODE, oldCull);
    return true;
}

void failMovie(void)
{
    g_movie.resetResources();
}

bool pumpMovie(uint64_t targetPts90k, uint64_t targetFrame)
{
    const std::vector<PssPacket>& packets = g_movie.demuxer.streamPackets();
    uint32_t processed = 0;
    while(g_movie.packetIndex < packets.size() && processed < MAX_PUMP_PACKETS)
    {
        if(!g_movie.audio.flush())
            return false;
        if(!g_movie.decoder.empty())
        {
            if(g_movie.decoder.frameCount() >= 3)
                break;
            const VideoFrame& newest = g_movie.decoder.back();
            if(newest.pts90k != NO_PTS && newest.pts90k > targetPts90k)
                break;
            if(newest.pts90k == NO_PTS && newest.number > targetFrame)
                break;
        }

        const PssPacket& packet = packets[g_movie.packetIndex++];
        processed++;
        if(packet.streamId == 0xbd)
        {
            if(!g_movie.audio.feed(g_movie.demuxer, packet))
                return false;
            continue;
        }
        if(packet.streamId < 0xe0 || packet.streamId > 0xef)
            continue;
        if(packet.pts90k != NO_PTS && g_movie.firstVideoPts == NO_PTS)
            g_movie.firstVideoPts = packet.pts90k;
        std::vector<uint8_t> payload;
        if(!g_movie.demuxer.readPayload(packet, &payload) ||
           !g_movie.decoder.feed(payload, packet.pts90k))
            return false;
    }

    if(g_movie.packetIndex == packets.size())
    {
        if(!g_movie.decoderDrained)
        {
            if(!g_movie.decoder.endOfStream())
                return false;
            g_movie.decoderDrained = true;
        }
        return g_movie.audio.finish();
    }
    return g_movie.audio.flush();
}

void selectMovieFrame(uint64_t targetPts90k, uint64_t targetFrame)
{
    while(!g_movie.decoder.empty())
    {
        const VideoFrame& frame = g_movie.decoder.front();
        const bool due = frame.pts90k != NO_PTS ? frame.pts90k <= targetPts90k :
                         frame.number <= targetFrame;
        if(!due)
            break;
        g_movie.current = frame;
        g_movie.decoder.pop();
    }
}

}
#ifdef MOVIE_TEST
bool movieTestInspectPss(const char* path, MovieTestInfo* info)
{
    if(info == NULL)
        return false;
    memset(info, 0, sizeof(*info));
    info->firstVideoPts90k = NO_PTS;
    PssDemuxer demuxer;
    if(!demuxer.open(path))
        return false;
    const std::vector<PssPacket>& packets = demuxer.streamPackets();
    for(size_t i = 0; i < packets.size(); i++)
    {
        const PssPacket& packet = packets[i];
        if(packet.streamId >= 0xe0 && packet.streamId <= 0xef)
        {
            info->videoPacketCount++;
            if(info->firstVideoPts90k == NO_PTS)
                info->firstVideoPts90k = packet.pts90k;
        }
        else if(packet.streamId == 0xbd)
        {
            info->audioPacketCount++;
            if(info->audioCodec != 0)
                continue;
            std::vector<uint8_t> payload;
            if(!demuxer.readPayloadPrefix(packet, 1024, &payload))
                return false;
            size_t header = payload.size();
            for(size_t j = 0; j + 4 <= payload.size(); j++)
            {
                if(memcmp(payload.data() + j, "SShd", 4) == 0)
                {
                    header = j;
                    break;
                }
            }
            if(header == payload.size() || header + 28 > payload.size())
                continue;
            const uint8_t* data = payload.data() + header;
            info->audioCodec = readLittle32(data + 8);
            info->audioSampleRate = readLittle32(data + 12);
            info->audioChannels = static_cast<uint16_t>(readLittle32(data + 16));
            info->audioInterleave = readLittle32(data + 20);
        }
    }
    return info->videoPacketCount != 0;
}

bool movieTestDecodeSs2Adpcm(const uint8_t* block, size_t blockSize,
                             int16_t* samples, size_t sampleCount)
{
    if(block == NULL || samples == NULL || blockSize < 16 || sampleCount < 28)
        return false;
    int32_t previous1 = 0;
    int32_t previous2 = 0;
    return decodePsxAdpcmBlock(block, &previous1, &previous2, samples, 1);
}
#endif

bool startMoviePlayback(const char* path, void* nativeWindow)
{
    (void)nativeWindow;
    stopMoviePlayback();
    if(path == NULL || !initializeMediaFoundation())
        return false;
    if(!g_movie.demuxer.open(path))
    {
        shutdownMediaFoundation();
        return false;
    }

    std::vector<uint8_t> probe;
    VideoFormat format;
    if(!readVideoProbe(g_movie.demuxer, &probe) ||
       !parseVideoFormat(probe, &format) ||
       !g_movie.decoder.open(format))
    {
        g_movie.resetResources();
        shutdownMediaFoundation();
        return false;
    }
    g_movie.width = format.width;
    g_movie.height = format.height;
    g_movie.displayAspect = format.displayAspect;
    g_movie.frameRate = format.frameRate;
    const std::vector<PssPacket>& packets = g_movie.demuxer.streamPackets();
    for(size_t i = 0; i < packets.size(); i++)
    {
        if(packets[i].streamId >= 0xe0 && packets[i].streamId <= 0xef)
        {
            g_movie.firstVideoPts = packets[i].pts90k;
            break;
        }
    }
    if(!g_movie.createTexture())
    {
        g_movie.resetResources();
        shutdownMediaFoundation();
        return false;
    }

    g_movie.audio.open(g_movie.demuxer);
    g_movie.audio.begin();
    stopAudioChannel(AUDIO_CHANNEL_MUSIC);
    stopAudioChannel(AUDIO_CHANNEL_EFFECT);
    g_movie.clockStart = std::chrono::steady_clock::now();
    g_movie.active = true;
    return true;
}

void updateMoviePlayback(void)
{
    if(!g_movie.active)
        return;
    updateAudio();
    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    const uint64_t elapsedMicros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now - g_movie.clockStart).count());
    const uint64_t targetFrame = static_cast<uint64_t>(
        static_cast<double>(elapsedMicros) * g_movie.frameRate / 1000000.0);
    const uint64_t elapsedPts = elapsedMicros * CLOCK_HZ / 1000000ULL;
    const uint64_t targetPts = g_movie.firstVideoPts == NO_PTS ? elapsedPts :
                               g_movie.firstVideoPts + elapsedPts;
    if(!pumpMovie(targetPts, targetFrame))
    {
        failMovie();
        return;
    }
    selectMovieFrame(targetPts, targetFrame);
    if(!g_movie.current.pixels.empty())
    {
        if(!g_movie.updateTexture())
        {
            failMovie();
            return;
        }
        renderMovieFrame();
    }

    if(g_movie.packetIndex == g_movie.demuxer.streamPackets().size() &&
       g_movie.decoderDrained && g_movie.decoder.empty() &&
       !g_movie.current.pixels.empty())
        stopMoviePlayback();
}

bool moviePlaybackActive(void)
{
    return g_movie.active;
}
uint64_t moviePlaybackFrameNumber(void)
{
    return g_movie.current.pixels.empty() ? 0 : g_movie.current.number;
}

void stopMoviePlayback(void)
{
    if(!g_movie.active && g_movie.demuxer.streamPackets().empty())
        return;
    g_movie.resetResources();
}

void terminateMoviePlayback(void)
{
    stopMoviePlayback();
    shutdownMediaFoundation();
}

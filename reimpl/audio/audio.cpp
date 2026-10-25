#include "audio/audio.h"
#include "xaf_runtime.h"
#include "audio/spatial.h"
#include "formats/SOUND_CUE/sound_cues.h"
#include "formats/SSL/ssl.h"

#include <windows.h>
#include <mmsystem.h>

#include <map>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <utility>

namespace
{

struct AudioClip
{
    std::vector<int16_t> samples;
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t loopStart;
    uint32_t loopEnd;
};

struct AudioVoice
{
    HWAVEOUT output;
    WAVEHDR headers[2];
    uint32_t headerCount;
    AudioChannel channel;
    uint32_t soundHandle;
    uint32_t sourceChannel;
    uint32_t sourceChannels;
    uint32_t loopHeader;
    float volume;
    uint8_t pan;
    bool panned;
    bool paused;
    bool looping;
};

const uint32_t MOVIE_PCM_BUFFER_COUNT = 4;
const uint32_t MOVIE_PCM_MAX_FRAMES = 8192;

struct MoviePcmBuffer
{
    WAVEHDR header;
    int16_t samples[MOVIE_PCM_MAX_FRAMES * 2];
    bool prepared;
};

struct MoviePcmStream
{
    HWAVEOUT output;
    MoviePcmBuffer buffers[MOVIE_PCM_BUFFER_COUNT];
    uint16_t channels;
    bool active;
};

struct SoundBankSlot
{
    int32_t entryIndex;
    bool loaded;
    SslBank bank;
};

const int32_t ADPCM_COEFFICIENTS[5][2] =
{
    { 0, 0 }, { 60, 0 }, { 115, -52 }, { 98, -55 }, { 122, -60 }
};

struct EventMusicTrack
{
    int16_t characterGroupId;
    uint16_t normalTrack;
    uint16_t relationshipTrack;
};

const EventMusicTrack EVENT_MUSIC_TRACKS[] =
{
    {  1, 0x0a1c, 0x0a19 }, {  2, 0x04bd, 0x04ba },
    {  3, 0x03a2, 0x039f }, {  4, 0x0595, 0x0592 },
    {  5, 0x01b0, 0x01ad }, {  6, 0x0b29, 0x0b29 },
    {  7, 0x0657, 0x0657 }, {  8, 0x046d, 0x046d },
    {  9, 0x0335, 0x0335 }, { 10, 0x0456, 0x0456 },
    { 11, 0x09fd, 0x09fd }, { 12, 0x0b3f, 0x0b3f },
    { 13, 0x0afd, 0x0afd }, { 14, 0x02c6, 0x02c6 },
    { 15, 0x0645, 0x0645 }, { 16, 0x0adc, 0x0ade },
    { 17, 0x029c, 0x029c }, { 18, 0x027d, 0x027d },
    { 19, 0x02ab, 0x02ab }, { 20, 0x0657, 0x0657 },
    { 21, 0x0679, 0x0673 }, { 22, 0x0733, 0x072d },
    { 23, 0x07ea, 0x07e4 }, { 24, 0x08ac, 0x08a6 },
    { 25, 0x094c, 0x0946 }, { 26, 0x067a, 0x0674 },
    { 27, 0x067a, 0x0674 }, { 28, 0x0734, 0x072e },
    { 29, 0x07eb, 0x07e5 }, { 30, 0x07eb, 0x07e5 },
    { 31, 0x07eb, 0x07e5 }, { 32, 0x08ad, 0x08a7 },
    { 33, 0x08ad, 0x08a7 }, { 34, 0x094d, 0x0947 },
    { 35, 0x094d, 0x0947 }, { 36, 0x0456, 0x0456 }
};
const size_t EVENT_MUSIC_TRACK_COUNT =
    sizeof(EVENT_MUSIC_TRACKS) / sizeof(EVENT_MUSIC_TRACKS[0]);

std::vector<XafEntryInfo> g_archiveEntries;

std::string g_archivePath;
std::map<std::string, AudioClip> g_clips;
std::vector<AudioVoice*> g_voices;
MoviePcmStream g_movieStream;
std::vector<XafEntryInfo> g_soundArchiveEntries;
std::string g_soundArchivePath;
SoundBankSlot g_soundBanks[SOUND_BANK_SLOT_COUNT];
std::map<uint64_t, AudioClip> g_sslClips;
uint32_t g_nextSoundHandle = 1;
int g_audioOutputMode = AUDIO_OUTPUT_STEREO;

uint32_t allocateSoundHandle(void)
{
    if(g_nextSoundHandle == 0 ||
       g_nextSoundHandle == AUDIO_SOUND_HANDLE_INVALID)
        g_nextSoundHandle = 1;
    const uint32_t first = g_nextSoundHandle;
    do
    {
        const uint32_t candidate = g_nextSoundHandle++;
        if(g_nextSoundHandle == 0 ||
           g_nextSoundHandle == AUDIO_SOUND_HANDLE_INVALID)
            g_nextSoundHandle = 1;
        bool used = false;
        for(size_t i = 0; i < g_voices.size(); ++i)
            if(g_voices[i]->soundHandle == candidate)
            {
                used = true;
                break;
            }
        if(!used)
            return candidate;
    } while(g_nextSoundHandle != first);
    return AUDIO_SOUND_HANDLE_INVALID;
}

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

uint32_t readBig32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) << 24 |
           static_cast<uint32_t>(data[1]) << 16 |
           static_cast<uint32_t>(data[2]) << 8 |
           data[3];
}

int16_t clampSample(int32_t sample)
{
    if(sample < -32768)
        return -32768;
    if(sample > 32767)
        return 32767;
    return static_cast<int16_t>(sample);
}

bool decodeXau(const std::vector<uint8_t>& data, AudioClip* clip)
{
    if(clip == NULL || data.size() < 0x800 ||
       memcmp(data.data(), "XAU\0", 4) != 0 ||
       read32(data.data() + 4) != 0x100 ||
       read32(data.data() + 8) != 0x40 ||
       memcmp(data.data() + 12, "PS2\0", 4) != 0 ||
       memcmp(data.data() + 0x40, "VAGp", 4) != 0)
        return false;

    const uint32_t channels = read32(data.data() + 0x18);
    const uint32_t bytesPerChannel = readBig32(data.data() + 0x4c);
    const uint32_t sampleRate = readBig32(data.data() + 0x50);
    if((channels != 1 && channels != 2) || bytesPerChannel == 0 ||
       (bytesPerChannel & 0x0f) != 0 ||
       sampleRate < 4000 || sampleRate > 192000 ||
       static_cast<uint64_t>(bytesPerChannel) * channels + 0x800 != data.size())
        return false;

    const uint32_t samplesPerChannel = bytesPerChannel / 16 * 28;
    clip->samples.assign(static_cast<size_t>(samplesPerChannel) * channels, 0);
    clip->sampleRate = sampleRate;
    clip->channels = channels;
    clip->loopStart = read32(data.data() + 0x10);
    clip->loopEnd = read32(data.data() + 0x14);
    if(clip->loopStart > clip->loopEnd || clip->loopEnd > samplesPerChannel)
        return false;

    for(uint32_t channel = 0; channel < channels; channel++)
    {
        const uint8_t* source =
            data.data() + 0x800 + static_cast<size_t>(channel) * bytesPerChannel;
        int32_t previous1 = 0;
        int32_t previous2 = 0;
        uint32_t output = 0;
        for(uint32_t offset = 0; offset < bytesPerChannel; offset += 16)
        {
            const uint32_t predictor = source[offset] >> 4;
            const uint32_t shift = source[offset] & 0x0f;
            if(predictor >= 5 || shift > 12)
                return false;

            for(uint32_t i = 0; i < 28; i++)
            {
                const uint8_t packed = source[offset + 2 + i / 2];
                int32_t sample = (i & 1) == 0 ? packed & 0x0f : packed >> 4;
                if(sample >= 8)
                    sample -= 16;
                sample = (sample << 12) >> shift;
                sample += (previous1 * ADPCM_COEFFICIENTS[predictor][0] +
                           previous2 * ADPCM_COEFFICIENTS[predictor][1] + 32) >> 6;
                const int16_t decoded = clampSample(sample);
                clip->samples[static_cast<size_t>(channel) *
                              samplesPerChannel + output++] = decoded;
                previous2 = previous1;
                previous1 = decoded;
            }
        }
    }
    return true;
}

const XafEntryInfo* findArchiveEntry(const char* entryName)
{
    if(entryName == NULL)
        return NULL;
    for(size_t i = 0; i < g_archiveEntries.size(); ++i)
        if(g_archiveEntries[i].name == entryName)
            return &g_archiveEntries[i];
    return NULL;
}

AudioClip* loadClip(const char* entryName)
{
    std::map<std::string, AudioClip>::iterator found = g_clips.find(entryName);
    if(found != g_clips.end())
        return &found->second;

    const XafEntryInfo* entry = findArchiveEntry(entryName);
    std::vector<uint8_t> data;
    if(entry == NULL ||
       !readXafEntry(g_archivePath.c_str(), *entry, data))
        return NULL;

    AudioClip clip;
    if(!decodeXau(data, &clip))
    {
        printf("ERROR: Could not decode XAU clip '%s'.\n", entryName);
        return NULL;
    }
    return &g_clips.insert(
        std::make_pair(entryName, std::move(clip))).first->second;
}

const XafEntryInfo* findSoundArchiveEntry(const char* entryName,
                                          uint32_t* index)
{
    if(entryName == NULL)
        return NULL;
    for(size_t i = 0; i < g_soundArchiveEntries.size(); ++i)
        if(g_soundArchiveEntries[i].name == entryName)
        {
            if(index != NULL)
                *index = static_cast<uint32_t>(i);
            return &g_soundArchiveEntries[i];
        }
    return NULL;
}

bool ensureSoundBankLoaded(uint8_t slot)
{
    if(slot >= SOUND_BANK_SLOT_COUNT || g_soundArchivePath.empty())
        return false;
    SoundBankSlot& binding = g_soundBanks[slot];
    if(binding.loaded)
        return true;
    if(binding.entryIndex < 0 ||
       static_cast<size_t>(binding.entryIndex) >= g_soundArchiveEntries.size())
        return false;
    std::vector<uint8_t> bytes;
    if(!readXafEntryByIndex(g_soundArchivePath.c_str(),
                            static_cast<uint32_t>(binding.entryIndex), bytes) ||
       bytes.empty() || !parseSslBank(bytes.data(), bytes.size(), binding.bank))
        return false;
    binding.loaded = true;
    return true;
}

AudioClip* loadSslClip(uint8_t slot, uint16_t streamIndex)
{
    if(!ensureSoundBankLoaded(slot))
        return NULL;
    SoundBankSlot& binding = g_soundBanks[slot];
    if(streamIndex >= binding.bank.streams.size())
        return NULL;
    const uint64_t key =
        static_cast<uint64_t>(static_cast<uint32_t>(binding.entryIndex)) << 32 |
        streamIndex;
    std::map<uint64_t, AudioClip>::iterator found = g_sslClips.find(key);
    if(found != g_sslClips.end())
        return &found->second;

    AudioClip clip;
    if(!decodeSslStream(binding.bank, streamIndex, clip.samples))
        return NULL;
    const SslStream& stream = binding.bank.streams[streamIndex];
    clip.sampleRate = stream.sampleRate;
    clip.channels = 1;
    clip.loopStart = 0;
    clip.loopEnd = 0;

    uint32_t playbackEnd = static_cast<uint32_t>(clip.samples.size());
    bool haveLoopStart = false;
    const uint8_t* payload = binding.bank.audio.data() + stream.payloadOffset;
    const uint32_t frameCount = stream.payloadLength / SSL_FRAME_BYTES;
    for(uint32_t frame = 0; frame < frameCount; ++frame)
    {
        const uint8_t flags = payload[frame * SSL_FRAME_BYTES + 1];
        if(!haveLoopStart && (flags & SSL_FLAG_LOOP_START) != 0)
        {
            clip.loopStart = frame * SSL_FRAME_SAMPLES;
            haveLoopStart = true;
        }
        if((flags & SSL_FLAG_LOOP_END) != 0)
        {
            playbackEnd = (frame + 1) * SSL_FRAME_SAMPLES;
            if(haveLoopStart)
                clip.loopEnd = playbackEnd;
            break;
        }
    }
    if(playbackEnd < clip.samples.size())
        clip.samples.resize(playbackEnd);
    return &g_sslClips.insert(std::make_pair(key, std::move(clip))).first->second;
}

void destroyVoice(size_t index)
{
    AudioVoice* voice = g_voices[index];
    waveOutReset(voice->output);
    for(uint32_t i = 0; i < voice->headerCount; i++)
        waveOutUnprepareHeader(voice->output, &voice->headers[i],
                               sizeof(voice->headers[i]));
    waveOutClose(voice->output);
    delete voice;
    g_voices.erase(g_voices.begin() + index);
}

DWORD outputVolume(float volume)
{
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;
    const DWORD level = static_cast<DWORD>(volume * 65535.0f);
    return level | level << 16;
}

DWORD outputPannedVolume(float volume, uint8_t pan)
{
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;
    if(pan > 127)
        pan = 127;

    const float left = volume * static_cast<float>(127 - pan) / 127.0f;
    const float right = volume * static_cast<float>(pan) / 127.0f;
    const DWORD leftLevel = static_cast<DWORD>(left * 65535.0f);
    const DWORD rightLevel = static_cast<DWORD>(right * 65535.0f);
    return leftLevel | rightLevel << 16;
}

DWORD outputGainsVolume(const AudioOutputGains& gains)
{
    float left = gains.left;
    float right = gains.right;
    if(left < 0.0f)
        left = 0.0f;
    if(left > 1.0f)
        left = 1.0f;
    if(right < 0.0f)
        right = 0.0f;
    if(right > 1.0f)
        right = 1.0f;
    const DWORD leftLevel = static_cast<DWORD>(left * 65535.0f);
    const DWORD rightLevel = static_cast<DWORD>(right * 65535.0f);
    return leftLevel | rightLevel << 16;
}

void applyVoiceVolume(AudioVoice* voice)
{
    if(voice->sourceChannels == 2)
    {
        const AudioOutputGains gains = computeXpecStereoVoiceGains(
            voice->volume, voice->sourceChannel, g_audioOutputMode);
        waveOutSetVolume(voice->output, outputGainsVolume(gains));
    }
    else
    {
        waveOutSetVolume(voice->output, voice->panned ?
                         outputPannedVolume(voice->volume, voice->pan) :
                         outputVolume(voice->volume));
    }
}

void releaseCompletedMovieBuffers(void)
{
    if(!g_movieStream.active)
        return;
    for(uint32_t i = 0; i < MOVIE_PCM_BUFFER_COUNT; i++)
    {
        MoviePcmBuffer& buffer = g_movieStream.buffers[i];
        if(buffer.prepared && (buffer.header.dwFlags & WHDR_DONE) != 0)
        {
            waveOutUnprepareHeader(g_movieStream.output, &buffer.header,
                                   sizeof(buffer.header));
            memset(&buffer.header, 0, sizeof(buffer.header));
            buffer.prepared = false;
        }
    }
}

bool prepareHeader(AudioVoice* voice, uint32_t index,
                   AudioClip* clip, uint32_t firstSample,
                   uint32_t sampleCount, bool loop)
{
    const size_t samplesPerChannel = clip->samples.size() / clip->channels;
    WAVEHDR& header = voice->headers[index];
    memset(&header, 0, sizeof(header));
    header.lpData = reinterpret_cast<char*>(
        clip->samples.data() + voice->sourceChannel * samplesPerChannel +
        firstSample);
    header.dwBufferLength = sampleCount * sizeof(int16_t);
    if(loop)
    {
        header.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
        header.dwLoops = 0xffffffff;
    }
    return waveOutPrepareHeader(voice->output, &header, sizeof(header)) ==
           MMSYSERR_NOERROR;
}

bool startClipVoice(AudioClip* clip, AudioChannel channel,
                    uint32_t sourceChannel, uint32_t soundHandle, bool loop,
                    float volume, uint8_t pan, bool panned)
{
    if(clip == NULL || clip->samples.empty() || clip->sampleRate == 0 ||
       sourceChannel >= clip->channels ||
       clip->samples.size() % clip->channels != 0)
        return false;
    const uint32_t sampleCount =
        static_cast<uint32_t>(clip->samples.size() / clip->channels);
    if(clip->loopStart > sampleCount || clip->loopEnd > sampleCount)
        return false;

    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = clip->sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    AudioVoice* voice = new AudioVoice;
    memset(voice, 0, sizeof(*voice));
    voice->channel = channel;
    voice->soundHandle = soundHandle;
    voice->sourceChannel = sourceChannel;
    voice->sourceChannels = clip->channels;
    voice->loopHeader = 0xffffffff;
    voice->volume = volume;
    voice->pan = pan;
    voice->panned = panned;
    voice->looping = loop;
    if(waveOutOpen(&voice->output, WAVE_MAPPER, &format,
                   0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        delete voice;
        return false;
    }
    applyVoiceVolume(voice);

    uint32_t loopStart = clip->loopStart;
    uint32_t loopEnd = clip->loopEnd;
    if(loop && loopEnd <= loopStart)
    {
        loopStart = 0;
        loopEnd = sampleCount;
    }
    bool prepared = false;
    if(loop && loopStart > 0)
    {
        prepared = prepareHeader(voice, 0, clip, 0, loopStart, false);
        if(prepared)
        {
            voice->headerCount = 1;
            prepared = prepareHeader(voice, 1, clip, loopStart,
                                     loopEnd - loopStart, true);
            if(prepared)
            {
                voice->headerCount = 2;
                voice->loopHeader = 1;
            }
        }
    }
    else
    {
        const uint32_t end = loop ? loopEnd : sampleCount;
        prepared = prepareHeader(voice, 0, clip, 0, end, loop);
        if(prepared)
        {
            voice->headerCount = 1;
            if(loop)
                voice->loopHeader = 0;
        }
    }
    if(!prepared)
    {
        for(uint32_t i = 0; i < voice->headerCount; ++i)
            waveOutUnprepareHeader(voice->output, &voice->headers[i],
                                   sizeof(voice->headers[i]));
        waveOutClose(voice->output);
        delete voice;
        return false;
    }
    for(uint32_t i = 0; i < voice->headerCount; ++i)
        if(waveOutWrite(voice->output, &voice->headers[i],
                        sizeof(voice->headers[i])) != MMSYSERR_NOERROR)
        {
            waveOutReset(voice->output);
            for(uint32_t j = 0; j < voice->headerCount; ++j)
                waveOutUnprepareHeader(voice->output, &voice->headers[j],
                                       sizeof(voice->headers[j]));
            waveOutClose(voice->output);
            delete voice;
            return false;
        }
    g_voices.push_back(voice);
    return true;
}

bool startEffectClip(AudioClip* clip, float volume, uint8_t pan,
                     uint32_t soundHandle)
{
    const bool loop = clip != NULL && clip->loopEnd > clip->loopStart;
    return startClipVoice(clip, AUDIO_CHANNEL_EFFECT, 0, soundHandle, loop,
                          volume, pan, true);
}

}

bool initializeAudio(const char* archivePath)
{
    terminateAudio();
    if(archivePath == NULL ||
       !listXafEntries(archivePath, g_archiveEntries) ||
       g_archiveEntries.empty())
        return false;
    g_archivePath = archivePath;
    return true;
}

bool initializeSoundEffects(const char* soundArchivePath)
{
    for(size_t i = g_voices.size(); i > 0; --i)
        if(g_voices[i - 1]->channel == AUDIO_CHANNEL_EFFECT)
            destroyVoice(i - 1);
    g_soundArchiveEntries.clear();
    g_soundArchivePath.clear();
    g_sslClips.clear();
    for(int i = 0; i < SOUND_BANK_SLOT_COUNT; ++i)
    {
        g_soundBanks[i].entryIndex = -1;
        g_soundBanks[i].loaded = false;
        g_soundBanks[i].bank = SslBank();
    }
    if(soundArchivePath == NULL ||
       !listXafEntries(soundArchivePath, g_soundArchiveEntries) ||
       g_soundArchiveEntries.empty())
        return false;
    g_soundArchivePath = soundArchivePath;
    for(uint8_t slot = 0; slot < 3; ++slot)
        if(!bindSoundBankSlot(
               slot, static_cast<uint32_t>(SOUND_FIXED_BANK_ENTRIES[slot])))
            return false;
    return true;
}

bool bindSoundBankSlot(uint8_t slot, uint32_t soundArchiveEntry)
{
    if(slot >= SOUND_BANK_SLOT_COUNT ||
       soundArchiveEntry >= g_soundArchiveEntries.size())
        return false;
    const std::string& name = g_soundArchiveEntries[soundArchiveEntry].name;
    if(name.size() < 4 || name.compare(name.size() - 4, 4, ".SSL") != 0)
        return false;
    SoundBankSlot& binding = g_soundBanks[slot];
    binding.entryIndex = static_cast<int32_t>(soundArchiveEntry);
    binding.loaded = false;
    binding.bank = SslBank();
    return true;
}

bool bindSoundBankSlotByName(uint8_t slot, const char* entryName)
{
    uint32_t entryIndex;
    return findSoundArchiveEntry(entryName, &entryIndex) != NULL &&
           bindSoundBankSlot(slot, entryIndex);
}

bool bindStageSoundBanks(int32_t sceneResourceId, int32_t audioResourceId)
{
    if(sceneResourceId < 0 || audioResourceId < 0)
        return false;

    return bindSoundBankSlot(3, static_cast<uint32_t>(audioResourceId)) &&
           bindSoundBankSlot(4, static_cast<uint32_t>(sceneResourceId));
}

bool bindStageSoundBanksForStage(int stageId)
{
    return bindStageSoundBanks(stageSceneSoundBankEntry(stageId),
                               stageAudioSoundBankEntry(stageId));
}

uint32_t playSoundCue(uint8_t bankSlot, uint8_t cue, uint8_t volume,
                      uint8_t pan)
{
    if(!ensureSoundBankLoaded(bankSlot))
        return AUDIO_SOUND_HANDLE_INVALID;
    const std::vector<SslCueLayer>* layers;
    if(!resolveSslCue(g_soundBanks[bankSlot].bank, cue, layers))
        return AUDIO_SOUND_HANDLE_INVALID;

    const uint32_t handle = allocateSoundHandle();
    if(handle == AUDIO_SOUND_HANDLE_INVALID)
        return AUDIO_SOUND_HANDLE_INVALID;
    bool played = false;
    for(size_t i = 0; i < layers->size(); ++i)
    {

        AudioClip* clip = loadSslClip(bankSlot, (*layers)[i].streamIndex);
        if(startEffectClip(clip, static_cast<float>(volume) / 127.0f,
                           pan, handle))
            played = true;
    }
    return played ? handle : AUDIO_SOUND_HANDLE_INVALID;
}

uint32_t playUnplacedSoundCue(uint8_t bankSlot, uint8_t cue,
                              uint8_t effectsVolumeSetting)
{

    return playSoundCue(bankSlot, cue,
                        scaleAudioVolume(127, effectsVolumeSetting), 64);
}

uint32_t playSpatialSoundCue(uint8_t bankSlot, uint8_t cue,
                             float listenerX, float listenerY,
                             float listenerZ, float forwardX,
                             float forwardZ, float sourceX,
                             float sourceY, float sourceZ,
                             int outputMode, uint8_t effectsVolumeSetting)
{
    const AudioPlacement placement =
        computeAudioPlacement(listenerX, listenerY, listenerZ,
                              forwardX, forwardZ, sourceX, sourceY, sourceZ,
                              outputMode);

    const uint8_t volume =
        scaleAudioVolume(placement.volume, effectsVolumeSetting);
    return playSoundCue(bankSlot, cue, volume, placement.pan);
}

uint32_t playFootstepSound(uint16_t surface, uint32_t actorFlags,
                           bool alternate, uint32_t* rotationCounter,
                           float listenerX, float listenerY,
                           float listenerZ, float forwardX,
                           float forwardZ, float sourceX,
                           float sourceY, float sourceZ,
                           int outputMode, uint8_t effectsVolumeSetting)
{
    if(rotationCounter == NULL)
        return AUDIO_SOUND_HANDLE_INVALID;
    const uint8_t cue =
        soundCueFor(surface, actorFlags, alternate, *rotationCounter);
    *rotationCounter = soundCueAdvance(*rotationCounter);
    return playSpatialSoundCue(SOUND_BANK_FOOTSTEP, cue,
                               listenerX, listenerY, listenerZ,
                               forwardX, forwardZ, sourceX, sourceY, sourceZ,
                               outputMode, effectsVolumeSetting);
}

void stopSoundHandle(uint32_t handle)
{
    if(handle == AUDIO_SOUND_HANDLE_INVALID)
        return;
    for(size_t i = g_voices.size(); i > 0; --i)
        if(g_voices[i - 1]->soundHandle == handle)
            destroyVoice(i - 1);
}

bool setSoundHandlePaused(uint32_t handle, bool paused)
{
    if(handle == AUDIO_SOUND_HANDLE_INVALID)
        return false;
    bool found = false;
    bool succeeded = true;
    for(size_t i = 0; i < g_voices.size(); ++i)
    {
        AudioVoice* voice = g_voices[i];
        if(voice->soundHandle != handle)
            continue;
        found = true;
        const MMRESULT result = paused ? waveOutPause(voice->output) :
                                        waveOutRestart(voice->output);
        if(result == MMSYSERR_NOERROR)
            voice->paused = paused;
        else
            succeeded = false;
    }
    return found && succeeded;
}

bool setSoundHandleVolume(uint32_t handle, int volume)
{
    if(handle == AUDIO_SOUND_HANDLE_INVALID)
        return false;
    const float nativeVolume =
        static_cast<float>(convertXpecSoundVolume(volume)) / 16383.0f;
    bool found = false;
    for(size_t i = 0; i < g_voices.size(); ++i)
    {
        AudioVoice* voice = g_voices[i];
        if(voice->soundHandle != handle)
            continue;
        found = true;
        voice->volume = nativeVolume;
        applyVoiceVolume(voice);
    }
    return found;
}

bool setSoundHandleLooping(uint32_t handle, bool enabled)
{
    if(handle == AUDIO_SOUND_HANDLE_INVALID)
        return false;
    bool found = false;
    bool succeeded = true;
    for(size_t i = 0; i < g_voices.size(); ++i)
    {
        AudioVoice* voice = g_voices[i];
        if(voice->soundHandle != handle)
            continue;
        found = true;
        if(enabled == voice->looping)
            continue;

        if(enabled)
        {
            succeeded = false;
            continue;
        }
        if(voice->loopHeader < voice->headerCount &&
           waveOutBreakLoop(voice->output) != MMSYSERR_NOERROR)
        {
            succeeded = false;
            continue;
        }
        voice->looping = false;
    }
    return found && succeeded;
}

bool isSoundHandlePlaying(uint32_t handle)
{
    if(handle == AUDIO_SOUND_HANDLE_INVALID)
        return false;
    for(size_t i = 0; i < g_voices.size(); ++i)
        if(g_voices[i]->soundHandle == handle)
            return true;
    return false;
}

bool playMusicTrack(int32_t entryIndex, float volume)
{
    return entryIndex >= 0 &&
           playAudioClipByIndex(static_cast<uint32_t>(entryIndex),
                                AUDIO_CHANNEL_MUSIC, true, volume);
}

bool playTitleMenuMusic(bool alternate, float volume)
{
    return playMusicTrack(titleMenuMusicTrack(alternate), volume);
}

bool playStageMusic(int stageId, int storyPromptPhase, float volume)
{
    return playMusicTrack(stageMusicTrack(stageId, storyPromptPhase), volume);
}

bool playAmbienceTrack(int32_t entryIndex, float volume)
{
    return entryIndex >= 0 &&
           playAudioClipByIndex(static_cast<uint32_t>(entryIndex),
                                AUDIO_CHANNEL_MUSIC_AMBIENT, true, volume);
}

bool playStageAmbience(int stageId, bool eventFlagSet, float volume)
{
    return playAmbienceTrack(stageAmbienceTrack(stageId, eventFlagSet),
                             volume);
}

bool playAudioClip(const char* entryName, AudioChannel channel,
                   bool loop, float volume)
{
    if(entryName == NULL || g_archivePath.empty())
        return false;
    if(channel != AUDIO_CHANNEL_EFFECT)
        stopAudioChannel(channel);

    AudioClip* clip = loadClip(entryName);
    if(clip == NULL || clip->samples.empty())
        return false;

    const size_t firstVoice = g_voices.size();
    for(uint32_t sourceChannel = 0; sourceChannel < clip->channels;
        ++sourceChannel)
    {
        if(startClipVoice(clip, channel, sourceChannel,
                          AUDIO_SOUND_HANDLE_INVALID, loop, volume, 0, false))
            continue;
        while(g_voices.size() > firstVoice)
            destroyVoice(g_voices.size() - 1);
        return false;
    }
    return true;
}

bool playAudioClipByIndex(uint32_t entryIndex, AudioChannel channel,
                          bool loop, float volume)
{
    if(entryIndex >= g_archiveEntries.size())
        return false;
    const XafEntryInfo& entry = g_archiveEntries[entryIndex];
    const size_t length = entry.name.size();
    if(length < 4 || entry.name.compare(length - 4, 4, ".XAU") != 0)
        return false;
    return playAudioClip(entry.name.c_str(), channel, loop, volume);
}

int32_t resolveEventMusicTrack(int characterGroupId, int relationshipState)
{
    for(size_t i = 0; i < EVENT_MUSIC_TRACK_COUNT; ++i)
    {
        const EventMusicTrack& track = EVENT_MUSIC_TRACKS[i];
        if(track.characterGroupId == characterGroupId)
            return relationshipState == 1 ?
                track.relationshipTrack : track.normalTrack;
    }
    return -1;
}

void updateAudio(void)
{
    for(size_t i = 0; i < g_voices.size();)
    {
        AudioVoice* voice = g_voices[i];
        if((voice->headers[voice->headerCount - 1].dwFlags & WHDR_DONE) != 0)
            destroyVoice(i);
        else
            i++;
    }
    releaseCompletedMovieBuffers();
}

void setAudioOutputMode(int outputMode)
{
    if(outputMode < AUDIO_OUTPUT_STEREO ||
       outputMode > AUDIO_OUTPUT_SURROUND ||
       outputMode == g_audioOutputMode)
        return;
    g_audioOutputMode = outputMode;
    for(size_t i = 0; i < g_voices.size(); ++i)
        if(g_voices[i]->sourceChannels == 2)
            applyVoiceVolume(g_voices[i]);
}

void stopAudioChannel(AudioChannel channel)
{
    if(channel == AUDIO_CHANNEL_MOVIE)
        endMoviePcmStream();
    for(size_t i = 0; i < g_voices.size();)
    {
        if(g_voices[i]->channel == channel)
            destroyVoice(i);
        else
            i++;
    }
}

void
setAudioChannelVolume(AudioChannel channel, float volume)
{
    for(size_t i = 0; i < g_voices.size(); i++)
    {
        AudioVoice* voice = g_voices[i];
        if(voice->channel != channel)
            continue;
        voice->volume = volume;
        applyVoiceVolume(voice);
    }
    if(channel == AUDIO_CHANNEL_MOVIE && g_movieStream.active)
        waveOutSetVolume(g_movieStream.output, outputVolume(volume));
}

void
setAudioChannelPaused(AudioChannel channel, bool paused)
{
    for(size_t i = 0; i < g_voices.size(); i++)
    {
        AudioVoice* voice = g_voices[i];
        if(voice->channel != channel)
            continue;
        const MMRESULT result = paused ? waveOutPause(voice->output) :
                                        waveOutRestart(voice->output);
        if(result == MMSYSERR_NOERROR)
            voice->paused = paused;
    }
    if(channel == AUDIO_CHANNEL_MOVIE && g_movieStream.active)
    {
        if(paused)
            waveOutPause(g_movieStream.output);
        else
            waveOutRestart(g_movieStream.output);
    }
}

bool
isAudioChannelPlaying(AudioChannel channel)
{
    if(channel != AUDIO_CHANNEL_MOVIE)
    {
        for(size_t i = 0; i < g_voices.size(); i++)
            if(g_voices[i]->channel == channel)
                return true;
        return false;
    }

    releaseCompletedMovieBuffers();
    for(uint32_t i = 0; i < MOVIE_PCM_BUFFER_COUNT; i++)
        if(g_movieStream.buffers[i].prepared)
            return true;
    return false;
}

bool
beginMoviePcmStream(uint32_t sampleRate, uint16_t channels)
{
    endMoviePcmStream();
    if((channels != 1 && channels != 2) ||
       sampleRate < 4000 || sampleRate > 192000)
        return false;

    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = channels * sizeof(int16_t);
    format.nAvgBytesPerSec = sampleRate * format.nBlockAlign;
    if(waveOutOpen(&g_movieStream.output, WAVE_MAPPER, &format,
                   0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        memset(&g_movieStream, 0, sizeof(g_movieStream));
        return false;
    }
    g_movieStream.channels = channels;
    g_movieStream.active = true;
    return true;
}

bool
queueMoviePcm(const int16_t* samples, uint32_t frameCount)
{
    if(!g_movieStream.active || samples == NULL || frameCount == 0 ||
       frameCount > MOVIE_PCM_MAX_FRAMES)
        return false;

    releaseCompletedMovieBuffers();
    MoviePcmBuffer* available = NULL;
    for(uint32_t i = 0; i < MOVIE_PCM_BUFFER_COUNT; i++)
    {
        if(!g_movieStream.buffers[i].prepared)
        {
            available = &g_movieStream.buffers[i];
            break;
        }
    }
    if(available == NULL)
        return false;

    const uint32_t sampleCount = frameCount * g_movieStream.channels;
    memcpy(available->samples, samples, sampleCount * sizeof(int16_t));
    memset(&available->header, 0, sizeof(available->header));
    available->header.lpData = reinterpret_cast<char*>(available->samples);
    available->header.dwBufferLength = sampleCount * sizeof(int16_t);
    if(waveOutPrepareHeader(g_movieStream.output, &available->header,
                           sizeof(available->header)) != MMSYSERR_NOERROR)
        return false;
    available->prepared = true;
    if(waveOutWrite(g_movieStream.output, &available->header,
                    sizeof(available->header)) != MMSYSERR_NOERROR)
    {
        waveOutUnprepareHeader(g_movieStream.output, &available->header,
                               sizeof(available->header));
        memset(&available->header, 0, sizeof(available->header));
        available->prepared = false;
        return false;
    }
    return true;
}

void
endMoviePcmStream(void)
{
    if(!g_movieStream.active)
        return;
    waveOutReset(g_movieStream.output);
    for(uint32_t i = 0; i < MOVIE_PCM_BUFFER_COUNT; i++)
    {
        MoviePcmBuffer& buffer = g_movieStream.buffers[i];
        if(buffer.prepared)
            waveOutUnprepareHeader(g_movieStream.output, &buffer.header,
                                   sizeof(buffer.header));
    }
    waveOutClose(g_movieStream.output);
    memset(&g_movieStream, 0, sizeof(g_movieStream));
}

void terminateAudio(void)
{
    endMoviePcmStream();
    while(!g_voices.empty())
        destroyVoice(g_voices.size() - 1);
    g_clips.clear();
    g_archiveEntries.clear();
    g_archivePath.clear();
    g_sslClips.clear();
    g_soundArchiveEntries.clear();
    g_soundArchivePath.clear();
    for(int i = 0; i < SOUND_BANK_SLOT_COUNT; ++i)
    {
        g_soundBanks[i].entryIndex = -1;
        g_soundBanks[i].loaded = false;
        g_soundBanks[i].bank = SslBank();
    }
}

#include "app/internal.h"

namespace app_detail
{

bool startTitleAttractMovie(int movieIndex)
{

    const GameOptions& movieOptions = g_game.runtimeState().options;
    char path[64];
    if(!buildAttractMoviePath(path, sizeof(path), movieIndex,
                              movieOptions.videoTimingMode != 0,
                              movieOptions.audioOutput != AUDIO_OUTPUT_STEREO))
        return false;
    return startMoviePlayback(path, NULL);
}

void updateTitleMovieState(float deltaSeconds)
{
    const bool active = moviePlaybackActive();
    const bool finished = g_titleMovieWasActive && !active;
    if(finished)
    {
        g_currentMusicTrack = -1;
        g_currentMusicVolume = 0xff;
        g_titleAttractMode = TITLE_ATTRACT_VIDEO;
    }
    g_titleMovieWasActive = active;

    if(g_game.screen() == GAME_SCREEN_ATTRACT_MOVIE)
    {
        g_titleAttractSeconds = 0.0f;
        g_titleMoviePending = false;
        g_titleWasVisible = false;
        if(finished)
        {

            g_titleMovieIndex = static_cast<uint8_t>(
                (g_game.titleAttractMovieIndex() + 1) %
                TITLE_ATTRACT_MOVIE_COUNT);
            g_game.completePreTitleMoviePlayback();
            return;
        }
        if(active)
            return;

        if(startTitleAttractMovie(g_game.titleAttractMovieIndex()))
            g_titleMovieWasActive = true;
        return;
    }

    if(g_game.screen() != GAME_SCREEN_TITLE)
    {
        if(active)
            stopMoviePlayback();
        g_titleAttractSeconds = 0.0f;
        g_titleMoviePending = false;
        g_titleWasVisible = false;
        return;
    }
    if(!g_titleWasVisible)
    {
        g_titleAttractSeconds = 0.0f;
        g_titleMoviePending = false;
        g_titleWasVisible = true;

        titleAttractReset(g_titleAttract, false);
        g_titleAttractFrames = 0.0f;
        cameraPathStart(g_titleCameraPath, TITLE_CAMERA_PATH);
        g_titleCameraPathStarted = true;
    }
    if(active)
        return;

    titleAttractAdvanceElapsed(
        g_titleAttract, g_titleAttractFrames,
        deltaSeconds * g_game.nominalFrameRateHz(),
        g_titleAttractMode, true);

    g_titleAttractSeconds += deltaSeconds;
    if(g_titleAttractSeconds < TITLE_ATTRACT_SECONDS)
        return;
    g_titleMoviePending = true;

    if(startTitleAttractMovie(g_titleMovieIndex))
    {
        g_titleMovieIndex ^= 1;
        g_titleMovieWasActive = true;
    }
    g_titleMoviePending = false;
    g_titleAttractSeconds = 0.0f;
}

const char* audioEntryName(int32_t index)
{
    if(index < 0 || static_cast<size_t>(index) >= g_ui.audioEntries.size())
        return NULL;
    const XafEntryInfo& entry = g_ui.audioEntries[index];
    return hasExtension(entry.name, ".XAU") ? entry.name.c_str() : NULL;
}

void updateEventAudioPresentation(void)
{
    const EventVm& event = g_game.eventScript();

    const AudioChannel channels[SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT] =
    {
        AUDIO_CHANNEL_MUSIC,
        AUDIO_CHANNEL_MUSIC_SECONDARY,
        AUDIO_CHANNEL_VOICE
    };
    if(!event.loaded())
    {
        if(g_eventAudioPresentation.valid)
        {
            stopSoundHandle(g_eventAudioPresentation.soundHandle);
            stopAudioChannel(AUDIO_CHANNEL_MUSIC_SECONDARY);
            stopAudioChannel(AUDIO_CHANNEL_MUSIC_AMBIENT);
            stopAudioChannel(AUDIO_CHANNEL_VOICE);
        }
        g_eventAudioPresentation.soundHandle = AUDIO_SOUND_HANDLE_INVALID;
        g_eventAudioPresentation.valid = false;
        int32_t messageVoiceTrack = -1;
        if(g_game.pollEventMessageVoice(&messageVoiceTrack))
        {

            stopAudioChannel(AUDIO_CHANNEL_VOICE);
            playAudioClipByIndex(
                static_cast<uint32_t>(messageVoiceTrack),
                AUDIO_CHANNEL_VOICE, false,
                g_game.runtimeState().options.effectsVolume / 15.0f);
        }
        for(int i = 0;
            i < SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT; ++i)
        {
            g_game.setSceneEventSoundChannelActive(
                i, isAudioChannelPlaying(channels[i]));
        }
        return;
    }

    const EventVmAudioState& state = event.audio();
    const float masterVolume =
        g_game.runtimeState().options.musicVolume / 15.0f;
    for(int i = 0; i < SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT; i++)
    {
        const bool changed = !g_eventAudioPresentation.valid ||
            state.bgmTrack[i] != g_eventAudioPresentation.track[i] ||
            state.bgmPlaying[i] != g_eventAudioPresentation.playing[i];
        const bool ownsChannel =
            state.bgmPlaying[i] != 0 ||
            (g_eventAudioPresentation.valid &&
             g_eventAudioPresentation.playing[i] != 0);
        if(changed && ownsChannel)
        {

            stopAudioChannel(channels[i]);
            const char* entry = audioEntryName(state.bgmTrack[i]);
            if(state.bgmPlaying[i] && entry != NULL)
            {
                playAudioClip(entry, channels[i], true,
                    masterVolume * clampPresentation(
                        state.bgmVolume[i] / 100.0f, 0.0f, 1.0f));
            }
        }
        if(state.bgmPlaying[i])
        {
            setAudioChannelVolume(
                channels[i], masterVolume * clampPresentation(
                    state.bgmVolume[i] / 100.0f, 0.0f, 1.0f));
            setAudioChannelPaused(channels[i], state.bgmPaused[i] != 0);
        }
        g_eventAudioPresentation.track[i] = state.bgmTrack[i];
        g_eventAudioPresentation.volume[i] = state.bgmVolume[i];
        g_eventAudioPresentation.playing[i] = state.bgmPlaying[i];
        g_eventAudioPresentation.paused[i] = state.bgmPaused[i];
    }

    const bool soundChanged = !g_eventAudioPresentation.valid ||
        state.lastSoundBank != g_eventAudioPresentation.soundBank ||
        state.lastSoundCue != g_eventAudioPresentation.soundCue;
    if(soundChanged)
    {
        if(g_eventAudioPresentation.valid)
            stopSoundHandle(g_eventAudioPresentation.soundHandle);
        g_eventAudioPresentation.soundHandle = AUDIO_SOUND_HANDLE_INVALID;
        if(state.lastSoundBank >= 0 && state.lastSoundBank <= 0xff &&
           state.lastSoundCue >= 0 && state.lastSoundCue <= 0xff)
        {
            g_eventAudioPresentation.soundHandle = playUnplacedSoundCue(
                static_cast<uint8_t>(state.lastSoundBank),
                static_cast<uint8_t>(state.lastSoundCue),
                g_game.runtimeState().options.effectsVolume);
        }
    }
    g_eventAudioPresentation.soundBank = state.lastSoundBank;
    g_eventAudioPresentation.soundCue = state.lastSoundCue;
    g_eventAudioPresentation.valid = true;
    int32_t messageVoiceTrack = -1;
    if(g_game.pollEventMessageVoice(&messageVoiceTrack))
    {

        stopAudioChannel(AUDIO_CHANNEL_VOICE);
        playAudioClipByIndex(
            static_cast<uint32_t>(messageVoiceTrack),
            AUDIO_CHANNEL_VOICE, false,
            g_game.runtimeState().options.effectsVolume / 15.0f);
    }
    for(int i = 0; i < SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT; ++i)
    {
        g_game.setSceneEventSoundChannelActive(
            i, isAudioChannelPlaying(channels[i]));
    }
}

AudioSpatialMatrix audioSpatialMatrix(const rw::Matrix& matrix)
{
    AudioSpatialMatrix result;
    result.right = { matrix.right.x, matrix.right.y, matrix.right.z };
    result.up = { matrix.up.x, matrix.up.y, matrix.up.z };
    result.at = { matrix.at.x, matrix.at.y, matrix.at.z };
    result.pos = { matrix.pos.x, matrix.pos.y, matrix.pos.z };
    return result;
}

AudioListener activeAudioListener(void)
{

    const rw::Camera* camera = g_scene.camera;
    const rw::Frame* frame = camera->getFrame();
    return computeAudioListener(audioSpatialMatrix(camera->viewMatrix),
                                audioSpatialMatrix(frame->matrix));
}

void updateGameAudio(uint32_t)
{
    updateAudio();
    const GameRuntimeState& persistent = g_game.runtimeState();
    setAudioOutputMode(persistent.options.audioOutput);
    const GameScreen screen = g_game.screen();
    const uint8_t musicVolume = persistent.options.musicVolume;
    const bool eventMusic = g_game.eventScript().loaded() &&
        g_game.eventScript().audio().bgmPlaying[0] != 0;

    int32_t musicTrack = -1;
    int32_t ambienceTrack = -1;
    if(!eventMusic || screen == GAME_SCREEN_CREDITS)
    {
        switch(screen)
        {
        case GAME_SCREEN_TITLE:
        case GAME_SCREEN_MAIN_MENU:
        case GAME_SCREEN_VERSUS_TYPE_SELECT:
        case GAME_SCREEN_CHARACTER_SELECT:
        case GAME_SCREEN_OPTIONS:

            musicTrack = titleMenuMusicTrack((g_titleMovieIndex & 1) != 0);
            break;
        case GAME_SCREEN_BRIEFING:
            musicTrack = SOUND_MUSIC_BATTLE_SETUP;
            break;
        case GAME_SCREEN_GAMEPLAY:
        case GAME_SCREEN_PAUSE:
        case GAME_SCREEN_INVENTORY:
        {
            const int stageId = persistent.stageId;
            musicTrack = stageMusicTrack(stageId, g_game.storyPromptPhase());
            ambienceTrack = stageAmbienceTrack(
                stageId, getStorySaveFlag(persistent.story, 0x000b));
            break;
        }
        case GAME_SCREEN_RESULTS:
            musicTrack = SOUND_MUSIC_RESULTS;
            break;
        case GAME_SCREEN_CREDITS:

            if(g_game.creditsSequence().phase == CREDITS_PHASE_SCROLL)
                musicTrack = CREDITS_MUSIC_TRACK;
            break;
        default:
            break;
        }
    }
    const bool volumeChanged = musicVolume != g_currentMusicVolume;
    if(musicTrack != g_currentMusicTrack || volumeChanged)
    {
        stopAudioChannel(AUDIO_CHANNEL_MUSIC);
        if(musicTrack >= 0 && musicVolume != 0)
            playMusicTrack(musicTrack, musicVolume / 15.0f);
        g_currentMusicTrack = musicTrack;
    }
    if(ambienceTrack != g_currentAmbienceTrack || volumeChanged)
    {
        stopAudioChannel(AUDIO_CHANNEL_MUSIC_SECONDARY);
        if(ambienceTrack >= 0 && musicVolume != 0)
            playAmbienceTrack(ambienceTrack, musicVolume / 15.0f);
        g_currentAmbienceTrack = ambienceTrack;
    }
    g_currentMusicVolume = musicVolume;

    AudioListener listener;
    bool listenerReady = false;
    GameSoundEvent sound;
    while(g_game.pollSoundEvent(&sound))
    {
        if(sound.kind == GAME_SOUND_EVENT_EVENT_CUE)
        {
            playUnplacedSoundCue(sound.bank, sound.cue,
                persistent.options.effectsVolume);
            continue;
        }

        if(!listenerReady)
        {
            listener = activeAudioListener();
            listenerReady = true;
        }
        if(sound.kind == GAME_SOUND_EVENT_FOOTSTEP)
        {
            playFootstepSound(
                sound.surface, sound.actorFlags, sound.alternate,
                &sound.footstepRotationCounter,
                listener.position.x, listener.position.y, listener.position.z,
                listener.forward.x, listener.forward.z,
                sound.position.x, sound.position.y, sound.position.z,
                persistent.options.audioOutput,
                persistent.options.effectsVolume);
        }
        else
        {
            playSpatialSoundCue(
                sound.bank, sound.cue,
                listener.position.x, listener.position.y, listener.position.z,
                listener.forward.x, listener.forward.z,
                sound.position.x, sound.position.y, sound.position.z,
                persistent.options.audioOutput,
                persistent.options.effectsVolume);
        }
    }
    updateEventAudioPresentation();
}

}

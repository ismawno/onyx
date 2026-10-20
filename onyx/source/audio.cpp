#include "pch.hpp"
#include "onyx/audio.hpp"
#include "onyx/resources.hpp"
#include "tkit/container/hive.hpp"
#define MA_IMPLEMENTATION
#include <miniaudio.h>
#include <filesystem>

#define CHECK_RESOURCE_HANDLE(handle, rtype)                                                                           \
    ONYX_CHECK_RESOURCE_IS_NOT_NULL(handle);                                                                           \
    ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(handle, rtype);                                                                \
    ONYX_CHECK_RESOURCE_IS_VALID(handle, rtype);

#define ERROR(type, err, msg) Result<type>::Error(err, msg)
#define ERROR_FMT(type, err, msg, ...) Result<type>::Error(err, TKit::TierString::Format(msg, __VA_ARGS__))
#define CHECK_ERROR(res, type, err, msg, ...)                                                                          \
    if (res != MA_SUCCESS)                                                                                             \
    return ERROR_FMT(type, err, msg, __VA_ARGS__)

#ifdef TKIT_ENABLE_ENSURE
#    define ENSURE_SUCCESS(expr, msg, ...)                                                                             \
        if (const ma_result res = (expr); res != MA_SUCCESS)                                                           \
        TKIT_PANIC(msg, res __VA_OPT__(, ) __VA_ARGS__)
#else
#    define ENSURE_SUCCESS(expr, msg, ...) expr
#endif

namespace Onyx
{
namespace fs = std::filesystem;
struct Audio
{
    ma_audio_buffer Buffer;
    ma_sound Sound;
    u32 FrameCount;
};
struct AudioResourceData
{
    ma_engine Engine;
    ma_context Context;
    TKit::TierHive<Audio *> Tracks{};
};

static TKit::Storage<AudioResourceData> s_Data{};
static bool s_Available = false;

void audio_Initialize()
{
    s_Data.Construct();
    ma_engine_config cfg = ma_engine_config_init();
    const ma_result res = ma_engine_init(&cfg, &s_Data->Engine);
    s_Available = res == MA_SUCCESS;
    if (!s_Available)
    {
        TKIT_LOG_ERROR(
            "[ONYX][AUDIO] Failed to initialize audio engine with error code {}. NULL backend will be initialized...",
            res);
        const ma_backend bk = ma_backend_null;
        ma_context_init(&bk, 1, nullptr, &s_Data->Context);
        cfg.pContext = &s_Data->Context;
        TKIT_ASSERT(ma_engine_init(&cfg, &s_Data->Engine) == MA_SUCCESS,
                    "[ONYX][AUDIO] Failed to initialize NULL backend with error code {}", res);
    }
}

void audio_Terminate()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (Audio *audio : s_Data->Tracks)
    {
        ma_sound_uninit(&audio->Sound);
        ma_audio_buffer_uninit(&audio->Buffer);
        tier->Destroy(audio);
    }
    if (!s_Available)
        ma_context_uninit(&s_Data->Context);
    ma_engine_uninit(&s_Data->Engine);
    s_Data.Destruct();
}

bool audio_IsResourceIdValid(const u32 id)
{
    return s_Data->Tracks.Contains(id);
}

bool Audio_IsAvailable()
{
    return s_Available;
}

static Audio *getAudio(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Audio);
    return s_Data->Tracks[Handle_GetResourceId(handle)];
}

f32 Audio_GetVolume()
{
    return ma_engine_get_volume(&s_Data->Engine);
}
void Audio_SetVolume(const f32 volume)
{
    ma_engine_set_volume(&s_Data->Engine, volume);
}

Resource Audio_Create(const AudioData &data, const AudioFlags flags)
{
    const ma_audio_buffer_config cfg = ma_audio_buffer_config_init(
        ma_format_f32, ma_engine_get_channels(&s_Data->Engine), data.FrameCount, data.Data, nullptr);

    TKit::TierAllocator *tier = TKit::GetTier();
    Audio *audio = tier->Create<Audio>();

    ENSURE_SUCCESS(ma_audio_buffer_init(&cfg, &audio->Buffer),
                   "[ONYX][AUDIO] Failed to initialize audio buffer with error code {}");
    ma_uint32 f = 0;
    if (flags & AudioFlag_NoSpatialization)
        f |= MA_SOUND_FLAG_NO_SPATIALIZATION;

    ENSURE_SUCCESS(ma_sound_init_from_data_source(&s_Data->Engine, &audio->Buffer, f, nullptr, &audio->Sound),
                   "[ONYX][AUDIO] Failed to initialize sound with error code {}");

    ma_sound_set_looping(&audio->Sound, bool(flags & AudioFlag_Loop));

    audio->FrameCount = data.FrameCount;
    return Handle_CreateForResource(Resource_Audio, s_Data->Tracks.Insert(audio));
}

void Audio_Destroy(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Audio);
    const u32 id = Handle_GetResourceId(handle);
    Audio *audio = s_Data->Tracks[id];

    ma_sound_uninit(&audio->Sound);
    ma_audio_buffer_uninit(&audio->Buffer);

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(audio);

    s_Data->Tracks.Remove(id);
}

void Audio_PlayFrom(const Resource handle, const u32 frame)
{
    Audio *audio = getAudio(handle);
    ENSURE_SUCCESS(ma_sound_seek_to_pcm_frame(&audio->Sound, frame),
                   "[ONYX][AUDIO] Failed to seek audio with error code {} and frame {}", frame);
    ENSURE_SUCCESS(ma_sound_start(&audio->Sound), "[ONYX][AUDIO] Failed to start sound with error code {}");
}

void Audio_Play(const Resource handle)
{
    Audio *audio = getAudio(handle);
    if (ma_sound_at_end(&audio->Sound))
    {
        ENSURE_SUCCESS(ma_sound_seek_to_pcm_frame(&audio->Sound, 0),
                       "[ONYX][AUDIO] Failed to seek audio with error code {}");
    }
    ENSURE_SUCCESS(ma_sound_start(&audio->Sound), "[ONYX][AUDIO] Failed to start sound with error code {}");
}

void Audio_Stop(const Resource handle)
{
    ENSURE_SUCCESS(ma_sound_stop(&getAudio(handle)->Sound), "[ONYX][AUDIO] Failed to stop sound with error code {}");
}

f32 Audio_GetVolume(const Resource handle)
{
    return ma_sound_get_volume(&getAudio(handle)->Sound);
}
f32 Audio_GetPitch(const Resource handle)
{
    return ma_sound_get_pitch(&getAudio(handle)->Sound);
}
f32 Audio_GetPan(const Resource handle)
{
    return ma_sound_get_pan(&getAudio(handle)->Sound);
}

void Audio_SetVolume(const Resource handle, const f32 volume)
{
    ma_sound_set_volume(&getAudio(handle)->Sound, volume);
}
void Audio_SetPitch(const Resource handle, const f32 pitch)
{
    ma_sound_set_pitch(&getAudio(handle)->Sound, pitch);
}
void Audio_SetPan(const Resource handle, const f32 pan)
{
    ma_sound_set_pan(&getAudio(handle)->Sound, pan);
}

bool Audio_IsPlaying(const Resource handle)
{
    return ma_sound_is_playing(&getAudio(handle)->Sound);
}
bool Audio_IsLooping(const Resource handle)
{
    return ma_sound_is_looping(&getAudio(handle)->Sound);
}
void Audio_EnableLooping(const Resource handle, const bool loop)
{
    ma_sound_set_looping(&getAudio(handle)->Sound, loop);
}

u32 Audio_GetFrameCount(const Resource handle)
{
    return getAudio(handle)->FrameCount;
}
void Audio_SeekFrame(const Resource handle, const u32 frame)
{
    ENSURE_SUCCESS(ma_sound_seek_to_pcm_frame(&getAudio(handle)->Sound, frame),
                   "[ONYX][AUDIO] Failed to seek audio with error code {}");
}

Result<> Audio_Play(const char *path)
{
    if (!fs::exists(path) || fs::is_directory(path))
        return Result<>::Error(
            Error_FileNotFound,
            TKit::TierString::Format("[ONYX][AUDIO] File at {} does not exist or is a directory", path));

    const ma_result res = ma_engine_play_sound(&s_Data->Engine, path, nullptr);
    CHECK_ERROR(res, , Error_PlayFailed, "[ONYX][AUDIO] Failed to play sound with error code {}", res);

    return Result<>::Ok();
}

Result<AudioData> Audio_LoadData(const char *path)
{
    if (!fs::exists(path) || fs::is_directory(path))
        return ERROR_FMT(AudioData, Error_FileNotFound, "[ONYX][AUDIO] File at {} does not exist or is a directory",
                         path);

    AudioData data;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, ma_engine_get_channels(&s_Data->Engine),
                                                   ma_engine_get_sample_rate(&s_Data->Engine));
    ma_uint64 frameCount;
    const ma_result res = ma_decode_file(path, &cfg, &frameCount, &data.Data);
    CHECK_ERROR(res, AudioData, Error_LoadFailed, "[ONYX][AUDIO] File decode failed with error code {}", res);

    data.FrameCount = u32(frameCount);
    return Result<AudioData>::Ok(data);
}

void Audio_UnloadData(const AudioData &data)
{
    ma_free(data.Data, nullptr);
}
} // namespace Onyx

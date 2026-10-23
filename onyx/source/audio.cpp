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
    return ERROR_FMT(type, err, msg, u32(res) __VA_OPT__(, ) __VA_ARGS__)

#ifdef TKIT_ENABLE_ENSURE
#    define ENSURE_SUCCESS(expr, msg, ...)                                                                             \
        if (const ma_result res = (expr); res != MA_SUCCESS)                                                           \
        TKIT_PANIC(msg, u32(res) __VA_OPT__(, ) __VA_ARGS__)
#else
#    define ENSURE_SUCCESS(expr, msg, ...) expr
#endif

namespace Onyx
{
namespace fs = std::filesystem;

enum AudioType : u8
{
    Audio_Buffer,
    Audio_Stream,
    Audio_Custom,
};

struct CustomSource
{
    ma_data_source_base Base;
    AudioSourceCallback SourceFn;
    AudioSeekCallback SeekFn;
    AudioCursorCallback CursorFn;
    AudioLengthCallback LengthFn;
    u32 Channels;
    u32 SampleRate;
};
struct ProcessorNode
{
    ma_node_base Base;
    AudioProcessorCallback Fn = nullptr;
    u32 Channels;
};
struct Audio
{
    ma_audio_buffer Buffer;
    ma_sound Sound;
    CustomSource Source;
    u64 FrameCount;
    AudioEndCallback EndCallback = nullptr;
    ProcessorNode Node;
    AudioType Type;
};
struct AudioResourceData
{
    ma_engine Engine;
    ma_context Context;
    ma_sound_group Group;
    ma_log Log;
    ProcessorNode MasterNode;
    AudioDeviceCallback DeviceCallback;
    TKit::TierHive<Audio *> Audios{};
};

#ifndef ONYX_AUDIO_SPSC_QUEUE_CAPACITY
#    define ONYX_AUDIO_SPSC_QUEUE_CAPACITY 128
#endif

template <typename T> class SpscQueue
{
  public:
    static constexpr u32 N = ONYX_AUDIO_SPSC_QUEUE_CAPACITY;
    static_assert((N & (N - 1)) == 0, "[ONYX][AUDIO] ONYX_AUDIO_SPSC_QUEUE_CAPACITY must be a power of two");

    bool Push(const T data)
    {
        const u32 head = m_Head.load(std::memory_order_relaxed);
        if ((head - m_Tail.load(std::memory_order_acquire)) == N)
            return false;

        m_Data[head & (N - 1)] = data;
        m_Head.store(head + 1, std::memory_order_release);
        return true;
    }

    TKit::Optional<T> Pop()
    {
        const u32 tail = m_Tail.load(std::memory_order_relaxed);
        if (tail == m_Head.load(std::memory_order_acquire))
            return TKit::Optional<T>::None();

        const T data = m_Data[tail & (N - 1)];
        m_Tail.store(tail + 1, std::memory_order_release);
        return data;
    }

    template <typename F> void Drain(const F func)
    {
        for (;;)
        {
            const auto elm = Pop();
            if (!elm)
                break;

            func(*elm);
        }
    }

  private:
    TKit::FixedArray<T, N> m_Data{};
    alignas(64) std::atomic<u32> m_Head{0};
    alignas(64) std::atomic<u32> m_Tail{0};
};

static TKit::Storage<AudioResourceData> s_Data{};
static TKit::Storage<SpscQueue<Audio *>> s_CallbackQueue{};
static TKit::Storage<SpscQueue<AudioDeviceEvent>> s_DeviceQueue{};

static bool s_Available = false;

static void processorOnProcess(ma_node *node, const f32 **in, ma_uint32 *frameCountIn, f32 **out,
                               ma_uint32 *frameCountOut)
{
    ProcessorNode *p = static_cast<ProcessorNode *>(node);
    const ma_uint32 frames = *frameCountOut < *frameCountIn ? *frameCountOut : *frameCountIn;
    ma_copy_pcm_frames(out[0], in[0], frames, ma_format_f32, p->Channels);
    p->Fn(out[0], frames, p->Channels);
    *frameCountIn = frames;
    *frameCountOut = frames;
}

static ma_result sourceOnRead(ma_data_source *ds, void *out, const ma_uint64 frameCount, ma_uint64 *framesRead)
{
    CustomSource *s = static_cast<CustomSource *>(ds);
    const u32 n = s->SourceFn(scast<f32 *>(out), u32(frameCount), s->Channels);
    *framesRead = n;
    return n == 0 ? MA_AT_END : MA_SUCCESS;
}

static ma_result sourceOnSeek(ma_data_source *ds, const ma_uint64 frame)
{
    CustomSource *s = static_cast<CustomSource *>(ds);
    if (s->SeekFn && s->SeekFn(frame))
        return MA_SUCCESS;
    return MA_NOT_IMPLEMENTED;
}

static ma_result sourceOnGetDataFormat(ma_data_source *ds, ma_format *format, ma_uint32 *channels,
                                       ma_uint32 *sampleRate, ma_channel *, size_t)
{
    const CustomSource *s = static_cast<CustomSource *>(ds);
    *format = ma_format_f32;
    *channels = s->Channels;
    *sampleRate = s->SampleRate;
    return MA_SUCCESS;
}

static ma_result sourceOnGetCursor(ma_data_source *ds, ma_uint64 *frames)
{
    CustomSource *s = static_cast<CustomSource *>(ds);
    if (const auto v = s->CursorFn ? s->CursorFn() : TKit::Optional<u64>::None())
    {
        *frames = *v;
        return MA_SUCCESS;
    }
    return MA_NOT_IMPLEMENTED;
}

static ma_result sourceOnGetLength(ma_data_source *ds, ma_uint64 *frames)
{
    CustomSource *s = static_cast<CustomSource *>(ds);
    if (const auto v = s->LengthFn ? s->LengthFn() : TKit::Optional<u64>::None())
    {
        *frames = *v;
        return MA_SUCCESS;
    }
    return MA_NOT_IMPLEMENTED;
}
static constexpr ma_node_vtable s_ProcessorVtable = {processorOnProcess, nullptr, 1, 1, 0};
static ma_data_source_vtable s_SourceVtable = {
    sourceOnRead, sourceOnSeek, sourceOnGetDataFormat, sourceOnGetCursor, sourceOnGetLength, nullptr, 0};

static void logCallback(void *, const ma_uint32 level, const char *message)
{
    switch (level)
    {
    case MA_LOG_LEVEL_DEBUG:
        TKIT_LOG_DEBUG("[ONYX][AUDIO] {}", message);
        break;
    case MA_LOG_LEVEL_INFO:
        TKIT_LOG_INFO("[ONYX][AUDIO] {}", message);
        break;
    case MA_LOG_LEVEL_WARNING:
        TKIT_LOG_WARNING("[ONYX][AUDIO] {}", message);
        break;
    case MA_LOG_LEVEL_ERROR:
        TKIT_LOG_ERROR("[ONYX][AUDIO] {}", message);
        break;
    }
}
static void notificationCallback(const ma_device_notification *n)
{
    switch (n->type)
    {
    case ma_device_notification_type_started:
        s_DeviceQueue->Push(AudioDeviceEvent_Started);
        break;
    case ma_device_notification_type_stopped:
        s_DeviceQueue->Push(AudioDeviceEvent_Stopped);
        break;
    case ma_device_notification_type_rerouted:
        s_DeviceQueue->Push(AudioDeviceEvent_Rerouted);
        break;
    case ma_device_notification_type_interruption_began:
        s_DeviceQueue->Push(AudioDeviceEvent_InterruptionBegan);
        break;
    case ma_device_notification_type_interruption_ended:
        s_DeviceQueue->Push(AudioDeviceEvent_InterruptionEnded);
        break;
    default:
        break;
    }
}

void audio_Initialize(const u32 listenerCount)
{
    s_Data.Construct();
    s_CallbackQueue.Construct();
    s_DeviceQueue.Construct();

    ENSURE_SUCCESS(ma_log_init(nullptr, &s_Data->Log),
                   "[ONYX][AUDIO] Audio log initialization failed with result code {}");
    ENSURE_SUCCESS(ma_log_register_callback(&s_Data->Log, ma_log_callback_init(logCallback, nullptr)),
                   "[ONYX][AUDIO] Audio log registration failed with error code {}");

    ma_engine_config cfg = ma_engine_config_init();
    cfg.listenerCount = listenerCount;
    cfg.notificationCallback = notificationCallback;
    cfg.pLog = &s_Data->Log;

    const ma_result res = ma_engine_init(&cfg, &s_Data->Engine);
    s_Available = res == MA_SUCCESS;
    if (!s_Available)
    {
        TKIT_LOG_ERROR(
            "[ONYX][AUDIO] Failed to initialize audio engine with error code {}. NULL backend will be initialized...",
            u32(res));
        const ma_backend bk = ma_backend_null;
        ma_context_config ctxCfg = ma_context_config_init();
        ctxCfg.pLog = &s_Data->Log;
        ENSURE_SUCCESS(ma_context_init(&bk, 1, &ctxCfg, &s_Data->Context),
                       "[ONYX][AUDIO] Failed to initialize NULL backend with error code {}");

        cfg.pContext = &s_Data->Context;
        ENSURE_SUCCESS(ma_engine_init(&cfg, &s_Data->Engine),
                       "[ONYX][AUDIO] Failed to initialize NULL backend with error code {}");
    }
    else
    {
        s_Available = ma_engine_get_device(&s_Data->Engine)->pContext->backend != ma_backend_null;
        TKIT_LOG_ERROR_IF(
            !s_Available,
            "[ONYX][AUDIO] Failed to initialize audio engine with a non-null backend. NULL backend will be "
            "used");
    }

    ENSURE_SUCCESS(ma_sound_group_init(&s_Data->Engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &s_Data->Group),
                   "[ONYX][AUDIO] Audio group initialization failed with error code {}");
}

void audio_Terminate()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (Audio *audio : s_Data->Audios)
    {
        ma_sound_uninit(&audio->Sound);
        if (audio->Node.Fn)
            ma_node_uninit(&audio->Node.Base, nullptr);
        if (audio->Type == Audio_Buffer)
            ma_audio_buffer_uninit(&audio->Buffer);
        else if (audio->Type == Audio_Custom)
            ma_data_source_uninit(&audio->Source.Base);
        tier->Destroy(audio);
    }
    if (s_Data->MasterNode.Fn)
        ma_node_uninit(&s_Data->MasterNode.Base, nullptr);

    ma_sound_group_uninit(&s_Data->Group);
    ma_engine_uninit(&s_Data->Engine);
    if (!s_Available)
        ma_context_uninit(&s_Data->Context);
    ma_log_uninit(&s_Data->Log);

    s_DeviceQueue.Destruct();
    s_CallbackQueue.Destruct();
    s_Data.Destruct();
}

bool audio_IsResourceIdValid(const u32 id)
{
    return s_Data->Audios.Contains(id);
}

static void audio_DrainCallbackQueue()
{
    s_CallbackQueue->Drain([](Audio *audio) {
        TKIT_ASSERT(audio->EndCallback, "[ONYX][AUDIO] Consistency mismatch. Audio claims that has no "
                                        "callback, while attempting to execute its callback");
        audio->EndCallback();
    });
}
static void audio_DrainDeviceQueue()
{
    if (s_Data->DeviceCallback)
        s_DeviceQueue->Drain([](const AudioDeviceEvent event) { s_Data->DeviceCallback(event); });
}

void audio_Update()
{
    audio_DrainCallbackQueue();
    audio_DrainDeviceQueue();
}

static bool setupNode(AudioProcessorCallback &&cbk, ProcessorNode &node, ma_node *src, ma_node *dst)
{
    if (node.Fn)
    {
        ENSURE_SUCCESS(ma_node_attach_output_bus(src, 0, dst, 0),
                       "[ONYX][AUDIO] Failed to attach sound to engine endpoint with error code {}");
        ma_node_uninit(&node.Base, nullptr);
        node.Fn = nullptr;
    }

    if (!cbk)
        return false;

    node.Fn = std::move(cbk);

    ma_node_config ncfg = ma_node_config_init();
    const ma_uint32 channels = ma_engine_get_channels(&s_Data->Engine);

    ncfg.vtable = &s_ProcessorVtable;
    ncfg.pInputChannels = &channels;
    ncfg.pOutputChannels = &channels;
    node.Channels = channels;
    ENSURE_SUCCESS(ma_node_init(ma_engine_get_node_graph(&s_Data->Engine), &ncfg, nullptr, &node.Base),
                   "[ONYX][AUDIO] Failed to initialize audio node with error code {}");
    return true;
}

u32 Audio_GetChannels()
{
    return ma_engine_get_channels(&s_Data->Engine);
}
u32 Audio_GetSampleRate()
{
    return ma_engine_get_sample_rate(&s_Data->Engine);
}

bool Audio_IsAvailable()
{
    return s_Available;
}

void Audio_SetDeviceCallback(AudioDeviceCallback &&cbk)
{
    s_Data->DeviceCallback = std::move(cbk);
}
void Audio_SetMasterProcessor(AudioProcessorCallback &&cbk)
{
    ProcessorNode &node = s_Data->MasterNode;
    ma_node *endpoint = ma_engine_get_endpoint(&s_Data->Engine);

    if (setupNode(std::move(cbk), node, &s_Data->Group, endpoint))
    {
        ENSURE_SUCCESS(ma_node_attach_output_bus(&s_Data->Group, 0, &node.Base, 0),
                       "[ONYX][AUDIO] Failed to attach group to node with error code {}");
        ENSURE_SUCCESS(ma_node_attach_output_bus(&node.Base, 0, endpoint, 0),
                       "[ONYX][AUDIO] Failed to attach node to endpoint with error code {}");
    }
}

static Audio *getAudio(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Audio);
    return s_Data->Audios[Handle_GetResourceId(handle)];
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
    TKit::TierAllocator *tier = TKit::GetTier();
    Audio *audio = tier->Create<Audio>();
    audio->Type = Audio_Buffer;

    const ma_audio_buffer_config cfg = ma_audio_buffer_config_init(
        ma_format_f32, ma_engine_get_channels(&s_Data->Engine), data.FrameCount, data.Data, nullptr);

    ENSURE_SUCCESS(ma_audio_buffer_init(&cfg, &audio->Buffer),
                   "[ONYX][AUDIO] Failed to initialize audio buffer with error code {}");
    ma_uint32 f = 0;
    if (!(flags & AudioFlag_Spatialization))
        f |= MA_SOUND_FLAG_NO_SPATIALIZATION;

    ENSURE_SUCCESS(ma_sound_init_from_data_source(&s_Data->Engine, &audio->Buffer, f, &s_Data->Group, &audio->Sound),
                   "[ONYX][AUDIO] Failed to initialize sound with error code {}");

    ma_sound_set_looping(&audio->Sound, bool(flags & AudioFlag_Loop));

    audio->FrameCount = data.FrameCount;
    return Handle_CreateForResource(Resource_Audio, s_Data->Audios.Insert(audio));
}

Result<Resource> Audio_Create(const char *path, const AudioFlags flags)
{
    if (!fs::exists(path) || fs::is_directory(path))
        return Result<>::Error(
            Error_FileNotFound,
            TKit::TierString::Format("[ONYX][AUDIO] File at {} does not exist or is a directory", path));

    TKit::TierAllocator *tier = TKit::GetTier();
    Audio *audio = tier->Create<Audio>();
    audio->Type = Audio_Stream;

    ma_uint32 f = 0;
    if (!(flags & AudioFlag_Spatialization))
        f |= MA_SOUND_FLAG_NO_SPATIALIZATION;

    ENSURE_SUCCESS(ma_sound_init_from_file(&s_Data->Engine, path, MA_SOUND_FLAG_STREAM | f, &s_Data->Group, nullptr,
                                           &audio->Sound),
                   "[ONYX][AUDIO] Sound initialization failed with error code {}");

    ma_uint64 fcount;
    if (ma_sound_get_length_in_pcm_frames(&audio->Sound, &fcount) != MA_SUCCESS)
        fcount = 0;
    audio->FrameCount = u64(fcount);

    ma_sound_set_looping(&audio->Sound, bool(flags & AudioFlag_Loop));
    return Handle_CreateForResource(Resource_Audio, s_Data->Audios.Insert(audio));
}

Resource Audio_Create(AudioSourceCallback &&sourceCallback, CustomAudioSpecs &&specs)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Audio *audio = tier->Create<Audio>();
    audio->Type = Audio_Custom;

    CustomSource &src = audio->Source;
    src.SourceFn = std::move(sourceCallback);
    src.Channels = specs.Channels ? specs.Channels : ma_engine_get_channels(&s_Data->Engine);
    src.SampleRate = specs.SampleRate ? specs.SampleRate : ma_engine_get_sample_rate(&s_Data->Engine);
    src.SeekFn = std::move(specs.SeekCallback);
    src.CursorFn = std::move(specs.CursorCallback);
    src.LengthFn = std::move(specs.LengthCallback);

    ma_data_source_config cfg = ma_data_source_config_init();
    cfg.vtable = &s_SourceVtable;

    ENSURE_SUCCESS(ma_data_source_init(&cfg, &src.Base),
                   "[ONYX][AUDIO] Failed to initialize data source with error code {}");

    ma_uint32 f = 0;
    if (!(specs.Flags & AudioFlag_Spatialization))
        f |= MA_SOUND_FLAG_NO_SPATIALIZATION;

    ENSURE_SUCCESS(ma_sound_init_from_data_source(&s_Data->Engine, &src.Base, f, &s_Data->Group, &audio->Sound),
                   "[ONYX][AUDIO] Failed to initialize sound with error code {}");

    ma_sound_set_looping(&audio->Sound, bool(specs.Flags & AudioFlag_Loop));
    ma_uint64 len;
    audio->FrameCount = ma_sound_get_length_in_pcm_frames(&audio->Sound, &len) == MA_SUCCESS ? len : 0;
    return Handle_CreateForResource(Resource_Audio, s_Data->Audios.Insert(audio));
}

void Audio_Destroy(const Resource handle)
{
    CHECK_RESOURCE_HANDLE(handle, Resource_Audio);
    const u32 id = Handle_GetResourceId(handle);
    Audio *audio = s_Data->Audios[id];

    ma_sound_uninit(&audio->Sound);
    audio_DrainCallbackQueue();

    if (audio->Node.Fn)
        ma_node_uninit(&audio->Node.Base, nullptr);
    if (audio->Type == Audio_Buffer)
        ma_audio_buffer_uninit(&audio->Buffer);
    else if (audio->Type == Audio_Custom)
        ma_data_source_uninit(&audio->Source.Base);

    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(audio);

    s_Data->Audios.Remove(id);
}

void Audio_PlayFrom(const Resource handle, const u64 frame)
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

static void endImmediateTrampoline(void *userData, ma_sound *)
{
    Audio *audio = static_cast<Audio *>(userData);
    audio->EndCallback();
}

static void endDeferredTrampoline(void *userData, ma_sound *)
{
    Audio *audio = static_cast<Audio *>(userData);
    s_CallbackQueue->Push(audio);
}

void Audio_SetAudioThreadEndCallback(const Resource handle, AudioEndCallback &&cbk)
{
    Audio *audio = getAudio(handle);
    audio->EndCallback = std::move(cbk);

    ENSURE_SUCCESS(
        ma_sound_set_end_callback(&audio->Sound, audio->EndCallback ? endImmediateTrampoline : nullptr, audio),
        "[ONYX][AUDIO] Failed to set end callback with error code {}");
}

void Audio_SetMainThreadEndCallback(const Resource handle, AudioEndCallback &&cbk)
{
    Audio *audio = getAudio(handle);
    audio->EndCallback = std::move(cbk);

    ENSURE_SUCCESS(
        ma_sound_set_end_callback(&audio->Sound, audio->EndCallback ? endDeferredTrampoline : nullptr, audio),
        "[ONYX][AUDIO] Failed to set end callback with error code {}");
}

void Audio_SetProcessorCallback(const Resource handle, AudioProcessorCallback &&cbk)
{
    Audio *audio = getAudio(handle);
    ProcessorNode &node = audio->Node;

    if (setupNode(std::move(cbk), node, &audio->Sound, &s_Data->Group))
    {
        ENSURE_SUCCESS(ma_node_attach_output_bus(&audio->Sound, 0, &node, 0),
                       "[ONYX][AUDIO] Failed to attach sound to audio node with error code {}");
        ENSURE_SUCCESS(ma_node_attach_output_bus(&node, 0, &s_Data->Group, 0),
                       "[ONYX][AUDIO] Failed to attach node to endpoint with error code {}");
    }
}

u64 Audio_GetFrameCount(const Resource handle)
{
    return getAudio(handle)->FrameCount;
}
void Audio_SeekFrame(const Resource handle, const u64 frame)
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

    const ma_result res = ma_engine_play_sound(&s_Data->Engine, path, &s_Data->Group);
    CHECK_ERROR(res, , Error_PlayFailed, "[ONYX][AUDIO] Failed to play sound with error code {}");

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
    CHECK_ERROR(res, AudioData, Error_LoadFailed, "[ONYX][AUDIO] File decode failed with error code {}");

    data.FrameCount = u64(frameCount);
    return Result<AudioData>::Ok(data);
}

void Audio_UnloadData(const AudioData &data)
{
    ma_free(data.Data, nullptr);
}

#define CHECK_LISTENER(index)                                                                                          \
    TKIT_ASSERT(index < ma_engine_get_listener_count(&s_Data->Engine),                                                 \
                "[ONYX][AUDIO] Listener index {} is out of range", index)

static Audio *getSpatialAudio(const Resource handle)
{
    Audio *audio = getAudio(handle);
    TKIT_ASSERT(ma_sound_is_spatialization_enabled(&audio->Sound),
                "[ONYX][AUDIO] Cannot use spatial properties on an audio created with AudioFlag_NoSpatialization");
    return audio;
}

template <Dimension D> static f32v3 toV3(const f32v<D> &v)
{
    if constexpr (D == D2)
        return f32v3{v[0], v[1], 0.f};
    else
        return v;
}

template <Dimension D> static f32v<D> fromMa(const ma_vec3f &v)
{
    if constexpr (D == D2)
        return f32v2{v.x, v.y};
    else
        return f32v3{v.x, v.y, v.z};
}

template <Dimension D> void Audio_SetPosition(const Resource handle, const f32v<D> &position)
{
    const f32v3 p = toV3<D>(position);
    ma_sound_set_position(&getSpatialAudio(handle)->Sound, p[0], p[1], p[2]);
}
template <Dimension D> void Audio_SetVelocity(const Resource handle, const f32v<D> &velocity)
{
    const f32v3 v = toV3<D>(velocity);
    ma_sound_set_velocity(&getSpatialAudio(handle)->Sound, v[0], v[1], v[2]);
}
template <Dimension D> f32v<D> Audio_GetPosition(const Resource handle)
{
    return fromMa<D>(ma_sound_get_position(&getSpatialAudio(handle)->Sound));
}
template <Dimension D> f32v<D> Audio_GetVelocity(const Resource handle)
{
    return fromMa<D>(ma_sound_get_velocity(&getSpatialAudio(handle)->Sound));
}

u32 Listener_GetCount()
{
    return ma_engine_get_listener_count(&s_Data->Engine);
}
void Listener_Enable(const u32 index, const bool enable)
{
    CHECK_LISTENER(index);
    ma_engine_listener_set_enabled(&s_Data->Engine, index, enable);
}
bool Listener_IsEnabled(const u32 index)
{
    CHECK_LISTENER(index);
    return ma_engine_listener_is_enabled(&s_Data->Engine, index);
}

template <Dimension D> void Listener_SetPosition(const u32 index, const f32v<D> &position)
{
    CHECK_LISTENER(index);
    const f32v3 p = toV3<D>(position);
    ma_engine_listener_set_position(&s_Data->Engine, index, p[0], p[1], p[2]);
}
template <Dimension D> void Listener_SetVelocity(const u32 index, const f32v<D> &velocity)
{
    CHECK_LISTENER(index);
    const f32v3 v = toV3<D>(velocity);
    ma_engine_listener_set_velocity(&s_Data->Engine, index, v[0], v[1], v[2]);
}
template <Dimension D> f32v<D> Listener_GetPosition(const u32 index)
{
    CHECK_LISTENER(index);
    return fromMa<D>(ma_engine_listener_get_position(&s_Data->Engine, index));
}
template <Dimension D> f32v<D> Listener_GetVelocity(const u32 index)
{
    CHECK_LISTENER(index);
    return fromMa<D>(ma_engine_listener_get_velocity(&s_Data->Engine, index));
}

void Listener_SetDirection(const u32 index, const f32v3 &direction)
{
    CHECK_LISTENER(index);
    ma_engine_listener_set_direction(&s_Data->Engine, index, direction[0], direction[1], direction[2]);
}
void Listener_SetWorldUp(const u32 index, const f32v3 &up)
{
    CHECK_LISTENER(index);
    ma_engine_listener_set_world_up(&s_Data->Engine, index, up[0], up[1], up[2]);
}
f32v3 Listener_GetDirection(const u32 index)
{
    CHECK_LISTENER(index);
    return fromMa<D3>(ma_engine_listener_get_direction(&s_Data->Engine, index));
}
f32v3 Listener_GetWorldUp(const u32 index)
{
    CHECK_LISTENER(index);
    return fromMa<D3>(ma_engine_listener_get_world_up(&s_Data->Engine, index));
}

template void Audio_SetPosition<D2>(Resource, const f32v<D2> &);
template void Audio_SetPosition<D3>(Resource, const f32v<D3> &);
template void Audio_SetVelocity<D2>(Resource, const f32v<D2> &);
template void Audio_SetVelocity<D3>(Resource, const f32v<D3> &);
template f32v<D2> Audio_GetPosition<D2>(Resource);
template f32v<D3> Audio_GetPosition<D3>(Resource);
template f32v<D2> Audio_GetVelocity<D2>(Resource);
template f32v<D3> Audio_GetVelocity<D3>(Resource);

template void Listener_SetPosition<D2>(u32, const f32v<D2> &);
template void Listener_SetPosition<D3>(u32, const f32v<D3> &);
template void Listener_SetVelocity<D2>(u32, const f32v<D2> &);
template void Listener_SetVelocity<D3>(u32, const f32v<D3> &);
template f32v<D2> Listener_GetPosition<D2>(u32);
template f32v<D3> Listener_GetPosition<D3>(u32);
template f32v<D2> Listener_GetVelocity<D2>(u32);
template f32v<D3> Listener_GetVelocity<D3>(u32);
} // namespace Onyx

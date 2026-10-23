#pragma once

#ifndef ONYX_ENABLE_AUDIO
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_AUDIO"
#endif

#include "onyx/core.hpp"
#include "onyx/handle.hpp"
#include "tkit/utils/optional.hpp"

namespace Onyx
{
struct AudioData
{
    void *Data;
    u64 FrameCount;
};

using AudioFlags = u8;
enum AudioFlagBit : AudioFlags
{
    AudioFlag_Spatialization = 1U << 0,
    AudioFlag_Loop = 1U << 1,
};

enum AudioDeviceEvent : u8
{
    AudioDeviceEvent_Started,
    AudioDeviceEvent_Stopped,
    AudioDeviceEvent_Rerouted,
    AudioDeviceEvent_InterruptionBegan,
    AudioDeviceEvent_InterruptionEnded,
};

using AudioEndCallback = TKit::TierFunction<void()>;
using AudioProcessorCallback = TKit::TierFunction<void(f32 *, u32, u32)>;
using AudioDeviceCallback = TKit::TierFunction<void(AudioDeviceEvent)>;
using AudioSourceCallback = TKit::TierFunction<u32(f32 *, u32, u32)>;
using AudioSeekCallback = TKit::TierFunction<bool(u64)>;
using AudioCursorCallback = TKit::TierFunction<TKit::Optional<u64>()>;
using AudioLengthCallback = TKit::TierFunction<TKit::Optional<u64>()>;

struct CustomAudioSpecs
{
    AudioSeekCallback SeekCallback = nullptr;
    AudioCursorCallback CursorCallback = nullptr;
    AudioLengthCallback LengthCallback = nullptr;
    // will be set to engine defaults if zero
    u32 Channels = 0;
    u32 SampleRate = 0;
    //
    AudioFlags Flags = 0;
};

u32 Audio_GetChannels();
u32 Audio_GetSampleRate();

bool Audio_IsAvailable();
void Audio_SetDeviceCallback(AudioDeviceCallback &&cbk);
void Audio_SetMasterProcessor(AudioProcessorCallback &&cbk);

f32 Audio_GetVolume();
void Audio_SetVolume(f32 volume);

Resource Audio_Create(const AudioData &data, AudioFlags flags = 0);
// audio stream path
Result<Resource> Audio_Create(const char *path, AudioFlags flags = 0);
Resource Audio_Create(AudioSourceCallback &&sourceCallback, CustomAudioSpecs &&specs = {});

void Audio_Destroy(Resource audio);

ONYX_NO_DISCARD Result<> Audio_Play(const char *path);

void Audio_Play(Resource audio);
void Audio_PlayFrom(Resource audio, u64 frame);
inline void Audio_PlayFromBeginning(const Resource audio)
{
    Audio_PlayFrom(audio, 0);
}

void Audio_Stop(Resource audio);

f32 Audio_GetVolume(Resource audio);
f32 Audio_GetPitch(Resource audio);
f32 Audio_GetPan(Resource audio);

void Audio_SetVolume(Resource audio, f32 volume);
void Audio_SetPitch(Resource audio, f32 pitch);
void Audio_SetPan(Resource audio, f32 pan);

bool Audio_IsPlaying(Resource audio);
bool Audio_IsLooping(Resource audio);
void Audio_EnableLooping(Resource audio, bool loop = true);

void Audio_SetAudioThreadEndCallback(Resource audio, AudioEndCallback &&cbk);
void Audio_SetMainThreadEndCallback(Resource audio, AudioEndCallback &&cbk);

void Audio_SetProcessorCallback(Resource audio, AudioProcessorCallback &&cbk);

u64 Audio_GetFrameCount(Resource audio);
void Audio_SeekFrame(Resource audio, u64 frame);

Result<AudioData> Audio_LoadData(const char *path);
void Audio_UnloadData(const AudioData &data);

template <Dimension D> f32v<D> Audio_GetPosition(Resource audio);
template <Dimension D> f32v<D> Audio_GetVelocity(Resource audio);

template <Dimension D> void Audio_SetPosition(Resource audio, const f32v<D> &position);
template <Dimension D> void Audio_SetVelocity(Resource audio, const f32v<D> &velocity);

bool Listener_IsEnabled(u32 index);
void Listener_Enable(u32 index, bool enable = true);

template <Dimension D> f32v<D> Listener_GetPosition(u32 index);
template <Dimension D> f32v<D> Listener_GetVelocity(u32 index);

template <Dimension D> void Listener_SetPosition(u32 index, const f32v<D> &position);
template <Dimension D> void Listener_SetVelocity(u32 index, const f32v<D> &velocity);

f32v3 Listener_GetWorldUp(u32 index);
f32v3 Listener_GetDirection(u32 index);

void Listener_SetWorldUp(u32 index, const f32v3 &up);
void Listener_SetDirection(u32 index, const f32v3 &direction);
} // namespace Onyx

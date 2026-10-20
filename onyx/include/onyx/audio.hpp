#pragma once

#ifndef ONYX_ENABLE_AUDIO
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_AUDIO"
#endif

#include "onyx/core.hpp"
#include "onyx/handle.hpp"

namespace Onyx
{
struct AudioData
{
    void *Data;
    u32 FrameCount;
};

struct AudioStream
{
};

using AudioFlags = u8;
enum AudioFlagBit : AudioFlags
{
    AudioFlag_NoSpatialization = 1U << 0,
    AudioFlag_Loop = 1U << 1,
};

bool Audio_IsAvailable();

f32 Audio_GetVolume();
void Audio_SetVolume(f32 volume);

Resource Audio_Create(const AudioData &data, AudioFlags flags = 0);
void Audio_Destroy(Resource audio);

ONYX_NO_DISCARD Result<> Audio_Play(const char *path);

void Audio_Play(Resource audio);
void Audio_PlayFrom(Resource audio, u32 frame);
inline void Audio_PlatFromBeginning(const Resource audio)
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

u32 Audio_GetFrameCount(Resource audio);
void Audio_SeekFrame(Resource audio, u32 frame);

Result<AudioData> Audio_LoadData(const char *path);
void Audio_UnloadData(const AudioData &data);
} // namespace Onyx

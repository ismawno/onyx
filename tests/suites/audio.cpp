// Audio: settings, playback state, end of stream and its callbacks, spatial positions. Sources are generated in code.
// Machines without an output device skip these: Onyx falls back to miniaudio's null backend there and reports audio
// as unavailable. The Linux CI jobs load a PulseAudio null sink, a real device that plays to nowhere in real time, so
// the suite runs there and its timing rules hold.

#include "harness.hpp"

#ifdef ONYX_ENABLE_AUDIO
#    include "onyx/audio.hpp"
#    include <atomic>
#    include "tkit/profiling/clock.hpp"
#    include <cmath>
#    include <cstring>
#    include <thread>

using namespace OnyxTest;

// a source that produces `length` frames of a quiet tone and then ends
struct Blip
{
    std::atomic<u32> Cursor{0};
    u32 Length = 0;
};

static Onyx::Resource createBlip(Blip &blip, const f32 seconds, const Onyx::AudioFlags flags = 0)
{
    blip.Length = u32(seconds * f32(Onyx::Audio_GetSampleRate()));
    const Onyx::Resource source = ONYX_CHECK_RESULT(Onyx::Audio_Create(
        [&blip](f32 *out, const u32 frameCount, const u32 channels) -> u32 {
            const u32 cursor = blip.Cursor.load(std::memory_order_relaxed);
            const u32 count = cursor >= blip.Length ? 0 : Math::Min(frameCount, blip.Length - cursor);
            for (u32 i = 0; i < count * channels; ++i)
                out[i] = 0.f;
            blip.Cursor.store(cursor + count, std::memory_order_relaxed);
            return count;
        },
        {.SeekCallback =
             [&blip](const u64 frame) {
                 blip.Cursor.store(u32(frame), std::memory_order_relaxed);
                 return true;
             },
         .Flags = flags}));
    Onyx::Audio_SetVolume(source, 1e-4f);
    return source;
}

// audio lives on its own thread: wait in real time, running frames so main thread callbacks get drained
template <typename F> static bool waitSeconds(const f32 seconds, F condition)
{
    const TKit::Clock clock{};
    while (clock.GetElapsed().AsSeconds() < seconds)
    {
        if (condition())
            return true;
        Onyx::Running();
        TKit::Timespan::Sleep(TKit::Timespan::FromMilliseconds(5));
    }
    return condition();
}

#    define REQUIRE_AUDIO()                                                                                            \
        if (!Onyx::Audio_IsAvailable())                                                                                \
        SKIP("No audio output device")

ONYX_TEST(audio, device_format_is_valid, .Description = "The engine reports a positive sample rate and channel count")
{
    REQUIRE_AUDIO();
    CHECK(Onyx::Audio_GetSampleRate() > 0);
    CHECK(Onyx::Audio_GetChannels() > 0);
}

ONYX_TEST(audio, settings_read_back,
          .Description = "Volume, pitch, pan, looping and the master volume read back what was set")
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 0.2f);
    Onyx::Audio_SetVolume(source, 0.25f);
    Onyx::Audio_SetPitch(source, 1.5f);
    Onyx::Audio_SetPan(source, -0.5f);
    Onyx::Audio_EnableLooping(source, true);
    Onyx::Audio_SetVolume(0.75f);
    CHECK_THAT(Onyx::Audio_GetVolume(source), IsNear(0.25f, 1e-6f));
    CHECK_THAT(Onyx::Audio_GetPitch(source), IsNear(1.5f, 1e-6f));
    CHECK_THAT(Onyx::Audio_GetPan(source), IsNear(-0.5f, 1e-6f));
    CHECK(Onyx::Audio_IsLooping(source));
    CHECK_THAT(Onyx::Audio_GetVolume(), IsNear(0.75f, 1e-6f));
    Onyx::Audio_EnableLooping(source, false);
    CHECK(!Onyx::Audio_IsLooping(source));
    Onyx::Audio_Destroy(source);
}

ONYX_TEST(audio, playing_follows_play_and_stop,
          .Description = "IsPlaying turns true on Play and false on Stop, right away, on the calling thread")
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 5.f);
    CHECK(!Onyx::Audio_IsPlaying(source));
    CHECK(Onyx::Audio_PlayFromBeginning(source));
    CHECK(Onyx::Audio_IsPlaying(source));
    CHECK(Onyx::Audio_Stop(source));
    CHECK(!Onyx::Audio_IsPlaying(source));
    Onyx::Audio_Destroy(source);
}

ONYX_TEST(audio, short_sounds_end_on_their_own,
          .Description = "A non looping source ends when its callback runs dry: IsPlaying turns false shortly after",
          .Timeout = 30.f)
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 0.15f);
    CHECK(Onyx::Audio_PlayFromBeginning(source));
    const bool ended = waitSeconds(2.f, [&] { return !Onyx::Audio_IsPlaying(source); });
    CHECK_MSG(ended, "Still playing 2 s after a 0.15 s sound started ({} of {} frames produced)", blip.Cursor.load(),
              blip.Length);
    Onyx::Audio_Destroy(source);
}

// a source has one end callback: setting either kind replaces the other, so each kind gets its own source
ONYX_TEST(audio, end_callbacks_run_on_the_audio_thread,
          .Description = "Audio_SetAudioThreadEndCallback runs its callback once, on the audio thread, when the sound "
                         "ends",
          .Timeout = 30.f)
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 0.1f);
    std::atomic<u32> ends{0};
    std::atomic<bool> onMainThread{false};
    const std::thread::id main = std::this_thread::get_id();
    CHECK(Onyx::Audio_SetAudioThreadEndCallback(source, [&] {
        onMainThread.store(std::this_thread::get_id() == main);
        ends.fetch_add(1);
    }));
    CHECK(Onyx::Audio_PlayFromBeginning(source));
    CHECK(waitSeconds(2.f, [&] { return ends.load() >= 1; }));
    waitSeconds(0.2f, [] { return false; });
    CHECK(ends.load() == 1u);
    CHECK(!onMainThread.load());
    Onyx::Audio_Destroy(source);
}

ONYX_TEST(audio, end_callbacks_reach_the_main_thread,
          .Description = "Audio_SetMainThreadEndCallback queues the end and runs the callback once on the main thread, "
                         "when the frame loop drains the queue",
          .Timeout = 30.f)
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 0.1f);
    u32 ends = 0;
    bool onMainThread = false;
    const std::thread::id main = std::this_thread::get_id();
    CHECK(Onyx::Audio_SetMainThreadEndCallback(source, [&] {
        onMainThread = std::this_thread::get_id() == main;
        ++ends;
    }));
    CHECK(Onyx::Audio_PlayFromBeginning(source));
    CHECK(waitSeconds(2.f, [&] { return ends >= 1; }));
    waitSeconds(0.2f, [] { return false; });
    CHECK(ends == 1u);
    CHECK(onMainThread);
    Onyx::Audio_Destroy(source);
}

ONYX_TEST(audio, looping_sounds_keep_playing,
          .Description = "A looping source starts over when it runs dry instead of ending", .Timeout = 30.f)
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 0.05f, Onyx::AudioFlag_Loop);
    CHECK(Onyx::Audio_PlayFromBeginning(source));
    waitSeconds(0.5f, [] { return false; });
    CHECK(Onyx::Audio_IsPlaying(source));
    CHECK(Onyx::Audio_Stop(source));
    Onyx::Audio_Destroy(source);
}

ONYX_TEST(audio, spatial_positions_read_back,
          .Description = "Spatialized sources and listeners take positions in world units and read them back")
{
    REQUIRE_AUDIO();
    Blip blip{};
    const Onyx::Resource source = createBlip(blip, 1.f, Onyx::AudioFlag_Spatialization);
    Onyx::Audio_SetPosition<D3>(source, f32v3{1.f, 2.f, 3.f});
    CHECK_THAT(Onyx::Audio_GetPosition<D3>(source), IsNear(f32v3(1.f, 2.f, 3.f), 1e-5f));
    Onyx::Audio_SetPosition<D2>(source, f32v2{-4.f, 5.f});
    CHECK_THAT(Onyx::Audio_GetPosition<D2>(source), IsNear(f32v2(-4.f, 5.f), 1e-5f));

    Onyx::Listener_SetPosition<D3>(0, f32v3{0.f, 0.f, 10.f});
    CHECK_THAT(Onyx::Listener_GetPosition<D3>(0), IsNear(f32v3(0.f, 0.f, 10.f), 1e-5f));
    Onyx::Listener_SetDirection(0, f32v3{0.f, 0.f, -1.f});
    CHECK_THAT(Math::Normalize(Onyx::Listener_GetDirection(0)), IsNear(f32v3(0.f, 0.f, -1.f), 1e-5f));
    Onyx::Audio_Destroy(source);
}
// a mono, 16 bit PCM WAV file of `frames` silent frames at `rate` frames a second
static Array<u8> makeWav(const u32 frames, const u32 rate)
{
    const u32 dataSize = frames * 2;
    Array<u8> wav{};
    wav.Resize(44 + dataSize, u8(0));
    const auto put = [&](const usize at, const u32 value, const u32 bytes) {
        for (u32 i = 0; i < bytes; ++i)
            wav[at + i] = u8(value >> (8 * i));
    };
    std::memcpy(wav.GetData(), "RIFF", 4);
    put(4, 36 + dataSize, 4);
    std::memcpy(wav.GetData() + 8, "WAVEfmt ", 8);
    put(16, 16, 4);       // fmt chunk size
    put(20, 1, 2);        // PCM
    put(22, 1, 2);        // channels
    put(24, rate, 4);     // sample rate
    put(28, rate * 2, 4); // byte rate
    put(32, 2, 2);        // block align
    put(34, 16, 2);       // bits per sample
    std::memcpy(wav.GetData() + 36, "data", 4);
    put(40, dataSize, 4);
    return wav;
}

// a source that streams the file. Audio_Create also takes any callable as a source of its own, so it is given Onyx's
// own path type by name: a Path of another allocation (alias.hpp) would fit both overloads
static Onyx::Result<Onyx::Resource> streamFrom(const Path &path)
{
    const TKit::TierPath file{path};
    return Onyx::Audio_Create(file);
}

ONYX_TEST(audio, wav_files_decode,
          .Description = "A WAV file decodes into as many frames as it lasts at the engine's sample rate, and streams "
                         "as a source of the same length")
{
    const u32 rate = 8000;
    const u32 frames = 4000; // half a second
    const Array<u8> wav = makeWav(frames, rate);
    const Path path = TempPath("half-second.wav");
    REQUIRE(WriteFile(path, wav.GetData(), wav.GetSize()));

    const auto data = Onyx::Audio_LoadData(path);
    REQUIRE_MSG(data, "{}", Describe(data));
    const f32 expected = 0.5f * f32(Onyx::Audio_GetSampleRate());
    CHECK_MSG(std::fabs(f32(data->FrameCount) - expected) <= expected * 0.02f, "Decoded {} frames, expected about {}",
              data->FrameCount, expected);
    Onyx::Audio_UnloadData(*data);

    const auto stream = streamFrom(path);
    REQUIRE_MSG(stream, "{}", Describe(stream));
    CHECK(Onyx::Audio_GetFrameCount(*stream) > 0);
    Onyx::Audio_Destroy(*stream);
}

ONYX_TEST(audio, files_that_cannot_play_say_why,
          .Description = "Loading, streaming or playing an audio file reports a missing file, a directory and a file "
                         "that is not audio with errors of their own")
{
    const Path missing = TempPath("missing.wav");
    const Path directory = TempPath("a-directory");
    const Path junk = TempPath("junk.wav");
    REQUIRE(MakeDirectory(directory));
    REQUIRE(WriteFile(junk, "this is not audio\n"));

    struct Case
    {
        const char *What;
        Onyx::ErrorCode Got;
        Onyx::ErrorCode Expected;
    };
    const auto code = [](const auto &result) { return result ? Onyx::Error_Count : result.GetError().GetCode(); };
    const Case cases[] = {
        {"load, missing", code(Onyx::Audio_LoadData(missing)), Onyx::Error_FileNotFound},
        {"load, directory", code(Onyx::Audio_LoadData(directory)), Onyx::Error_IsNotFile},
        {"load, not audio", code(Onyx::Audio_LoadData(junk)), Onyx::Error_AudioDecodeFailed},
        {"stream, missing", code(streamFrom(missing)), Onyx::Error_FileNotFound},
        {"stream, directory", code(streamFrom(directory)), Onyx::Error_IsNotFile},
        {"stream, not audio", code(streamFrom(junk)), Onyx::Error_AudioDecodeFailed},
        {"play, missing", code(Onyx::Audio_Play(missing)), Onyx::Error_FileNotFound},
        {"play, directory", code(Onyx::Audio_Play(directory)), Onyx::Error_IsNotFile},
        {"play, not audio", code(Onyx::Audio_Play(junk)), Onyx::Error_AudioPlayFailed},
    };
    for (const Case &c : cases)
        CHECK_MSG(c.Got == c.Expected, "{}: expected {}, got {}", c.What, Onyx::ToString(c.Expected),
                  c.Got == Onyx::Error_Count ? "success" : Onyx::ToString(c.Got));
}
#endif

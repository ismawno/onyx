// Lifecycle of the library: initialization, termination, the frame loop's running state and default resources.

#include "harness.hpp"

using namespace OnyxTest;

ONYX_TEST(core, initialize_and_terminate_twice,
          .Description = "Onyx can be initialized again after a full termination, default resources included",
          .Init = Init_Manual)
{
    for (u32 i = 0; i < 2; ++i)
    {
        Onyx::Initialize();
        Onyx::Resources_CreateDefault();
        const Onyx::DefaultResources &res = Onyx::Resources_GetDefault();
        CHECK(Onyx::IsResourceValid(res.Sampler, Onyx::Resource_Sampler));
        Onyx::Window *win = OpenTestWindow();
        Frame(2);
        Onyx::CloseWindow(win);
        Onyx::Terminate();
    }
}

ONYX_TEST(core, specs_read_back, .Description = "GetSpecs returns the specs Initialize was given", .Init = Init_Manual)
{
    Onyx::Specs specs{};
    specs.MaxImages = 123;
    specs.Specs2.MaxInstances = 4321;
    specs.Specs3.MaxDrawCommands = 77;
    Onyx::Initialize(specs);
    const Onyx::Specs &active = Onyx::GetSpecs();
    CHECK(active.MaxImages == 123u);
    CHECK(active.Specs2.MaxInstances == 4321u);
    CHECK(active.Specs3.MaxDrawCommands == 77u);
    Onyx::Terminate();
}

ONYX_TEST(core, default_resources_are_valid,
          .Description = "Resources_CreateDefault registers every default mesh, the font and the sampler")
{
    const Onyx::DefaultResources &res = Onyx::Resources_GetDefault();
    CHECK(Onyx::IsResourcePoolValid(res.FontPool));
    CHECK(Onyx::IsResourceValid(res.Sampler, Onyx::Resource_Sampler));
#ifdef ONYX_ENABLE_FONT_LOAD
    CHECK(Onyx::IsResourceValid(res.Font, Onyx::Resource_Font));
#endif
    const auto &r2 = res.Resources2;
    for (const Onyx::Resource mesh : {r2.Triangle, r2.Quad})
        CHECK(Onyx::IsResourceValid<D2>(mesh, Onyx::Resource_StaticMesh));
    for (const Onyx::Resource mesh : {r2.Stadium, r2.RoundedRect})
        CHECK(Onyx::IsResourceValid<D2>(mesh, Onyx::Resource_ParametricMesh));

    const auto &r3 = res.Resources3;
    for (const Onyx::Resource mesh : {r3.Triangle, r3.Quad, r3.Box, r3.Sphere, r3.Cylinder})
        CHECK(Onyx::IsResourceValid<D3>(mesh, Onyx::Resource_StaticMesh));
    for (const Onyx::Resource mesh : {r3.Stadium, r3.RoundedRect, r3.Capsule, r3.RoundedBox, r3.Torus})
        CHECK(Onyx::IsResourceValid<D3>(mesh, Onyx::Resource_ParametricMesh));
}

ONYX_TEST(core, running_follows_open_windows,
          .Description = "Running() is false with nothing open, true while a window is, false again once it closes")
{
    CHECK(!Onyx::Running());
    Onyx::Window *win = OpenTestWindow();
    CHECK(Onyx::Running());
    Onyx::CloseWindow(win);
    CHECK(!Onyx::Running());
}

ONYX_TEST(core, quit_ends_the_loop_once,
          .Description = "Quit() makes the next Running() return false. Without flags windows survive and the loop "
                         "can go on; QuitFlag_DestroyWindows closes them")
{
    OpenTestWindow();
    Onyx::Quit();
    CHECK(!Onyx::Running());
    CHECK(Onyx::Running());

    Onyx::Quit(Onyx::QuitFlag_DestroyWindows);
    CHECK(!Onyx::Running());
    CHECK(!Onyx::Running());
}

// Fails until fixed: ToFrequency returns u32(1 / dt) + 1, one too many whenever 1 / dt rounds to the exact rate (24,
// 75, 144, 165 Hz). Math::Round would fix it
ONYX_TEST(core, frequency_and_delta_time_round_trip,
          .Description = "ToFrequency and ToDeltaTime are inverses for usual refresh rates", .Init = Init_Manual)
{
    for (const u32 hz : {24u, 30u, 60u, 75u, 120u, 144u, 165u, 240u})
        CHECK(Onyx::ToFrequency(Onyx::ToDeltaTime(hz)) == hz);
    CHECK_THAT(Onyx::ToDeltaTime(60).AsMilliseconds(), IsNear(1000.f / 60.f, 1e-3f));
}

ONYX_TEST(core, delta_time_advances, .Description = "Each frame takes some time: the delta time is positive")
{
    Onyx::Window *win = OpenTestWindow();
    Frame(3);
    CHECK(Onyx::GetDeltaTime(win).AsSeconds() > 0.f);
    CHECK(Onyx::GetDeltaTime().AsSeconds() > 0.f);
    const TKit::Timespan before = Onyx::GetTime();
    Frame(2);
    CHECK(Onyx::GetTime() > before);
}

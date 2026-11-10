#pragma once

// The command line of onyx-tests, shared by its two roles: the supervisor (the default) and the worker (--worker).

#include "test.hpp"

namespace OnyxTest
{
enum OutputFormat : u8
{
    Output_Human,
    Output_Events,
};

// how fast the overlay test engine plays the user (engine.hpp)
enum Speed : u8
{
    Speed_Fast,
    Speed_Normal,
    Speed_Cinematic,
    Speed_Count,
};
constexpr const char *SpeedNames[Speed_Count] = {"fast", "normal", "cinematic"};

struct Options
{
    Array<String> Include{};
    Array<String> Exclude{};
    Path Self{};
    String Worker{};
    Path JUnit{};
    Path Summary{};
    String Label{};
    Speed PlaySpeed = Speed_Fast;
    OutputFormat Output = Output_Human;
    u32 Jobs = 1;
    f32 TimeoutScale = 1.f;
    bool List = false;
    bool Verbose = false;
    // every test in a worker process of its own, with an Onyx no other test has used
    bool Isolate = false;
};

// the largest allocation the allocator of each thread serves, by role (PushThreadAllocator). the supervisor keeps what
// every test reported until its reports are written. a worker only needs a little outside of Onyx (its options, the
// events it emits) and there is one worker per test: a small allocator is quicker to set up
constexpr usz SupervisorMaxAllocation = usz(1) << 20;
constexpr usz WorkerMaxAllocation = usz(1) << 16;

bool ParseOptions(int argc, char **argv, Options &options);
// the options in force, for code that runs inside a test (the overlay test engine reads the speed)
const Options &GetOptions();

int RunWorker(const Options &options);
int RunSupervisor(const Options &options);
} // namespace OnyxTest

#pragma once

// The command line of onyx-tests, shared by its two roles: the supervisor (the default) and the worker (--worker).

#include "test.hpp"
#include "tkit/container/dynamic_array.hpp"

namespace OnyxTest
{
enum OutputFormat : u8
{
    Output_Human,
    Output_Events,
};

struct Options
{
    TKit::DynamicArray<TKit::DynamicString> Include{};
    TKit::DynamicArray<TKit::DynamicString> Exclude{};
    TKit::DynamicString Self{};
    TKit::DynamicString Worker{};
    TKit::DynamicString JUnit{};
    TKit::DynamicString Summary{};
    TKit::DynamicString Label{};
    // how fast the overlay test engine plays the user: fast, normal or cinematic (overlay_engine.hpp)
    TKit::DynamicString Speed{"fast"};
    OutputFormat Output = Output_Human;
    u32 Jobs = 1;
    f32 TimeoutScale = 1.f;
    bool List = false;
    bool Verbose = false;
};

bool ParseOptions(int argc, char **argv, Options &options);
// the options in force, for code that runs inside a test (the overlay test engine reads the speed)
const Options &GetOptions();

int RunWorker(const Options &options);
int RunSupervisor(const Options &options);
} // namespace OnyxTest

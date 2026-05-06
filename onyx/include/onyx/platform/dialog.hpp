#pragma once

#ifndef ONYX_ENABLE_NFD
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_NFD"
#endif

#include "onyx/core/core.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/utils/result.hpp"
#include <filesystem>

struct GLFWwindow;

namespace Onyx::Dialog
{
namespace fs = std::filesystem;

#ifndef TKIT_OS_WINDOWS
using Char = char;
#else
using Char = wchar_t;
#endif

enum Status : u8
{
    Success = 0,
    Cancel = 1,
    Error = 2,
};

struct Filter
{
    const Char *Name = nullptr;
    const Char *Extensions = nullptr;
};

struct Options
{
    GLFWwindow *Window = nullptr;
    const Char *DefaultName = nullptr;
    const Char *DefaultPath = nullptr;
    TKit::Span<const Filter> Filters{};
};

template <typename T> using Result = TKit::Result<T, Status>;
using Path = fs::path;
using Paths = TKit::TierArray<Path>;

ONYX_NO_DISCARD Result<Path> Save(const Options &options = {});
ONYX_NO_DISCARD Result<Path> OpenFolder(const Options &options = {});
ONYX_NO_DISCARD Result<Path> OpenSingle(const Options &options = {});
ONYX_NO_DISCARD Result<Paths> OpenMultiple(const Options &options = {});
const char *GetError();
void ClearError();

} // namespace Onyx::Dialog

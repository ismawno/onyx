#pragma once

#ifndef ONYX_ENABLE_NFD
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_NFD"
#endif

#include "onyx/core/alias.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/utils/result.hpp"
#include <filesystem>

struct GLFWwindow;

namespace Onyx::Dialog
{
namespace fs = std::filesystem;

enum Status : u8
{
    Success = 0,
    Cancel = 1,
    Error = 2,
};

struct Filter
{
    const char *Name = nullptr;
    const char *Extensions = nullptr;
};

struct Options
{
    GLFWwindow *Window = nullptr;
    const char *DefaultName = nullptr;
    const char *DefaultPath = nullptr;
    TKit::Span<const Filter> Filters{};
};

template <typename T> using Result = TKit::Result<T, Status>;
using Path = fs::path;
using Paths = TKit::TierArray<Path>;

Result<Path> Save(const Options &options = {});
Result<Path> OpenFolder(const Options &options = {});
Result<Path> OpenSingle(const Options &options = {});
Result<Paths> OpenMultiple(const Options &options = {});

} // namespace Onyx::Dialog

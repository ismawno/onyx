#pragma once

#ifndef ONYX_ENABLE_NFD
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_NFD"
#endif

#include "tkit/container/tier_array.hpp"
#include <filesystem>

namespace Onyx::Dialog
{
namespace fs = std::filesystem;

enum Status : u8
{
    Success = 0,
    Cancel = 1,
    Error = 2,
};

struct Options
{
    const char *Filter = nullptr;
    const char *Default = nullptr;
};

template <typename T> using Result = TKit::Result<T, Status>;
using Path = fs::path;
using Paths = TKit::TierArray<Path>;

Result<Path> Save(const Options &options = {});
Result<Path> OpenFolder(const char *fdefault = nullptr);
Result<Path> OpenSingle(const Options &options = {});
Result<Paths> OpenMultiple(const Options &options = {});

} // namespace Onyx::Dialog

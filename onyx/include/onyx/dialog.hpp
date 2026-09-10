#pragma once

#ifndef ONYX_ENABLE_NFD
#    error "[ONYX] To include this file, the corresponding feature must be enabled in CMake with ONYX_ENABLE_NFD"
#endif

#include "onyx/window.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/utils/result.hpp"
#include <filesystem>

namespace Onyx
{
namespace fs = std::filesystem;

#ifndef TKIT_OS_WINDOWS
using DialogChar = char;
#else
using DialogChar = wchar_t;
#endif

enum DialogStatus : u8
{
    Dialog_Success = 0,
    Dialog_Cancel = 1,
    Dialog_Error = 2,
};

struct Filter
{
    const DialogChar *Name = nullptr;
    const DialogChar *Extensions = nullptr;
};

struct Options
{
    Onyx_WindowHandle *Window = nullptr;
    const DialogChar *DefaultName = nullptr;
    const DialogChar *DefaultPath = nullptr;
    TKit::Span<const Filter> Filters{};
};

template <typename T> using DialogResult = TKit::Result<T, DialogStatus>;
ONYX_NO_DISCARD DialogResult<fs::path> SaveDialog(const Options &options = {});
ONYX_NO_DISCARD DialogResult<fs::path> OpenFolderDialog(const Options &options = {});
ONYX_NO_DISCARD DialogResult<fs::path> OpenSingleDialog(const Options &options = {});
ONYX_NO_DISCARD DialogResult<TKit::TierArray<fs::path>> OpenMultipleDialog(const Options &options = {});
const char *GetDialogError();
void ClearDialogError();

} // namespace Onyx

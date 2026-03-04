#include "onyx/core/pch.hpp"
#include "onyx/platform/dialog.hpp"
#include "tkit/container/stack_array.hpp"
#include <nfd.h>
#ifdef TKIT_OS_APPLE
#    define GLFW_EXPOSE_NATIVE_COCOA
#    define USE_GLFW
#    include <nfd_glfw3.h>
#elif defined(TKIT_OS_WINDOWS)
#    define GLFW_EXPOSE_NATIVE_WIN32
#    define USE_GLFW
#    include <nfd_glfw3.h>
#endif

#undef Status
#undef Success

namespace Onyx::Dialog
{
Status toStatus(const nfdresult_t result)
{
    switch (result)
    {
    case NFD_CANCEL:
        return Cancel;
    case NFD_ERROR:
        return Error;
    case NFD_OKAY:
        return Success;
    default:
        return Error;
    }
}

struct Guard
{
    Guard()
    {
        NFD_Init();
    }
    ~Guard()
    {
        NFD_Quit();
    }
};

Result<Path> OpenFolder(const Options &options)
{
    Guard g{};
    nfdchar_t *path;
    nfdpickfolderu8args_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    args.defaultPath = options.DefaultPath;
    const Status result = toStatus(NFD_PickFolderU8_With(&path, &args));
    if (result == Success)
    {
        const fs::path p = path;
        NFD_FreePathU8(path);
        return p;
    }
    return Result<Path>::Error(result);
}
Result<Path> OpenSingle(const Options &options)
{
    Guard g{};
    nfdchar_t *path;
    nfdopendialogu8args_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    TKit::StackArray<nfdu8filteritem_t> filters{};
    if (options.Filters)
    {
        filters.Reserve(options.Filters.GetSize());
        for (const Filter &filter : options.Filters)
            filters.Append(nfdu8filteritem_t{filter.Name, filter.Extensions});
        args.filterList = filters.GetData();
        args.filterCount = filters.GetSize();
    }

    args.defaultPath = options.DefaultPath;
    Status result = toStatus(NFD_OpenDialogU8_With(&path, &args));
    if (result == Success)
    {
        const fs::path p = path;
        NFD_FreePathU8(path);
        return Result<Path>::Ok(p);
    }
    return Result<Path>::Error(result);
}
Result<Paths> OpenMultiple(const Options &options)
{
    Guard g{};
    const nfdpathset_t *set;
    nfdopendialogu8args_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    TKit::StackArray<nfdu8filteritem_t> filters{};
    if (options.Filters)
    {
        filters.Reserve(options.Filters.GetSize());
        for (const Filter &filter : options.Filters)
            filters.Append(nfdu8filteritem_t{filter.Name, filter.Extensions});
        args.filterList = filters.GetData();
        args.filterCount = filters.GetSize();
    }

    const Status result = toStatus(NFD_OpenDialogMultipleU8_With(&set, &args));
    if (result == Success)
    {
        Paths paths;
        nfdpathsetsize_t count;
        toStatus(NFD_PathSet_GetCount(set, &count));
        for (nfdpathsetsize_t i = 0; i < count; ++i)
        {
            nfdchar_t *path;
            NFD_PathSet_GetPathU8(set, i, &path);
            paths.Append(path);
            NFD_PathSet_FreePathU8(path);
        }
        NFD_PathSet_Free(set);
        return Result<Paths>::Ok(paths);
    }
    return Result<Paths>::Error(result);
}

Result<Path> Save(const Options &options)
{
    Guard g{};
    nfdchar_t *path;
    nfdsavedialognargs_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    args.defaultPath = options.DefaultPath;
    args.defaultName = options.DefaultName;
    const Status result = toStatus(NFD_SaveDialogU8_With(&path, &args));
    if (result == Success)
    {
        const fs::path p = path;
        NFD_FreePathU8(path);
        return Result<Path>::Ok(p);
    }
    return Result<Path>::Error(result);
}
} // namespace Onyx::Dialog

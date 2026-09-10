#include "pch.hpp"
#include "onyx/dialog.hpp"
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

#undef DialogStatus
#undef Success

namespace Onyx
{
DialogStatus toStatus(const nfdresult_t result)
{
    switch (result)
    {
    case NFD_CANCEL:
        return Dialog_Cancel;
    case NFD_ERROR:
        return Dialog_Error;
    case NFD_OKAY:
        return Dialog_Success;
    default:
        return Dialog_Error;
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

DialogResult<fs::path> OpenFolderDialog(const Options &options)
{
    Guard g{};
    nfdnchar_t *path;
    nfdpickfoldernargs_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    args.defaultPath = options.DefaultPath;
    const DialogStatus result = toStatus(NFD_PickFolderN_With(&path, &args));
    if (result == Dialog_Success)
    {
        const fs::path p = path;
        NFD_FreePathN(path);
        return p;
    }
    return DialogResult<fs::path>::Error(result);
}
DialogResult<fs::path> OpenSingleDialog(const Options &options)
{
    Guard g{};
    nfdnchar_t *path;
    nfdopendialognargs_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    TKit::StackArray<nfdnfilteritem_t> filters{};
    if (!options.Filters.IsEmpty())
    {
        filters.Reserve(options.Filters.GetSize());
        for (const Filter &filter : options.Filters)
            filters.Append(nfdnfilteritem_t{filter.Name, filter.Extensions});
        args.filterList = filters.GetData();
        args.filterCount = filters.GetSize();
    }

    args.defaultPath = options.DefaultPath;
    DialogStatus result = toStatus(NFD_OpenDialogN_With(&path, &args));
    if (result == Dialog_Success)
    {
        const fs::path p = path;
        NFD_FreePathN(path);
        return DialogResult<fs::path>::Ok(p);
    }
    return DialogResult<fs::path>::Error(result);
}
DialogResult<TKit::TierArray<fs::path>> OpenMultipleDialog(const Options &options)
{
    Guard g{};
    const nfdpathset_t *set;
    nfdopendialognargs_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    TKit::StackArray<nfdnfilteritem_t> filters{};
    if (!options.Filters.IsEmpty())
    {
        filters.Reserve(options.Filters.GetSize());
        for (const Filter &filter : options.Filters)
            filters.Append(nfdnfilteritem_t{filter.Name, filter.Extensions});
        args.filterList = filters.GetData();
        args.filterCount = filters.GetSize();
    }

    const DialogStatus result = toStatus(NFD_OpenDialogMultipleN_With(&set, &args));
    if (result == Dialog_Success)
    {
        TKit::TierArray<fs::path> paths;
        nfdpathsetsize_t count;
        toStatus(NFD_PathSet_GetCount(set, &count));
        for (nfdpathsetsize_t i = 0; i < count; ++i)
        {
            nfdnchar_t *path;
            NFD_PathSet_GetPathN(set, i, &path);
            paths.Append(path);
            NFD_PathSet_FreePathN(path);
        }
        NFD_PathSet_Free(set);
        return DialogResult<TKit::TierArray<fs::path>>::Ok(paths);
    }
    return DialogResult<TKit::TierArray<fs::path>>::Error(result);
}

DialogResult<fs::path> SaveDialog(const Options &options)
{
    Guard g{};
    nfdnchar_t *path;
    nfdsavedialognargs_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    args.defaultPath = options.DefaultPath;
    args.defaultName = options.DefaultName;
    const DialogStatus result = toStatus(NFD_SaveDialogN_With(&path, &args));
    if (result == Dialog_Success)
    {
        const fs::path p = path;
        NFD_FreePathN(path);
        return DialogResult<fs::path>::Ok(p);
    }
    return DialogResult<fs::path>::Error(result);
}
const char *GetDialogError()
{
    return NFD_GetError();
}
void ClearDialogError()
{
    NFD_ClearError();
}
} // namespace Onyx

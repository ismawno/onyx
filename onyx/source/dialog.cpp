#include "pch.hpp"
#include "onyx/dialog.hpp"
#include "tkit/container/stack_array.hpp"
#include <nfd.h>

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

DialogResult<TKit::TierPath> OpenFolderDialog(const Options &options)
{
    Guard g{};
    nfdu8char_t *path;
    nfdpickfoldernargs_t args{};
#ifdef USE_GLFW
    if (options.Window != Graph::NullHandle)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    args.defaultPath = options.DefaultPath;
    const DialogStatus result = toStatus(NFD_PickFolderU8_With(&path, &args));
    if (result == Dialog_Success)
    {
        const TKit::TierPath p = path;
        NFD_FreePathU8(path);
        return p;
    }
    return DialogResult<TKit::TierPath>::Error(result);
}
DialogResult<TKit::TierPath> OpenSingleDialog(const Options &options)
{
    Guard g{};
    nfdu8char_t *path;
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
    DialogStatus result = toStatus(NFD_OpenDialogU8_With(&path, &args));
    if (result == Dialog_Success)
    {
        const TKit::TierPath p = path;
        NFD_FreePathU8(path);
        return DialogResult<TKit::TierPath>::Ok(p);
    }
    return DialogResult<TKit::TierPath>::Error(result);
}
DialogResult<TKit::TierArray<TKit::TierPath>> OpenMultipleDialog(const Options &options)
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

    const DialogStatus result = toStatus(NFD_OpenDialogMultipleU8_With(&set, &args));
    if (result == Dialog_Success)
    {
        TKit::TierArray<TKit::TierPath> paths;
        nfdpathsetsize_t count;
        toStatus(NFD_PathSet_GetCount(set, &count));
        for (nfdpathsetsize_t i = 0; i < count; ++i)
        {
            nfdu8char_t *path;
            NFD_PathSet_GetPathU8(set, i, &path);
            paths.Append(path);
            NFD_PathSet_FreePathU8(path);
        }
        NFD_PathSet_Free(set);
        return DialogResult<TKit::TierArray<TKit::TierPath>>::Ok(paths);
    }
    return DialogResult<TKit::TierArray<TKit::TierPath>>::Error(result);
}

DialogResult<TKit::TierPath> SaveDialog(const Options &options)
{
    Guard g{};
    nfdu8char_t *path;
    nfdsavedialognargs_t args{};
#ifdef USE_GLFW
    if (options.Window)
        NFD_GetNativeWindowFromGLFWWindow(options.Window, &args.parentWindow);
#endif
    args.defaultPath = options.DefaultPath;
    args.defaultName = options.DefaultName;
    const DialogStatus result = toStatus(NFD_SaveDialogU8_With(&path, &args));
    if (result == Dialog_Success)
    {
        const TKit::TierPath p = path;
        NFD_FreePathU8(path);
        return DialogResult<TKit::TierPath>::Ok(p);
    }
    return DialogResult<TKit::TierPath>::Error(result);
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

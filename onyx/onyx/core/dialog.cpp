#include "onyx/core/pch.hpp"
#include "onyx/core/dialog.hpp"
#include <nfd.h>

namespace Onyx::Dialog
{

Status toStatus(const nfdresult_t p_Result)
{
    switch (p_Result)
    {
    case NFD_CANCEL:
        return Cancel;
    case NFD_ERROR:
        return Error;
    case NFD_OKAY:
        return Success;
    default:
        return Success;
    }
}

Result<Path> OpenFolder(const char *p_Default)
{
    char *path;
    const Status result = toStatus(NFD_PickFolder(p_Default, &path));
    if (result == Success)
        return Result<Path>::Ok(path);
    return Result<Path>::Error(result);
}
Result<Path> OpenSingle(const Options &p_Options)
{
    char *path;
    const Status result = toStatus(NFD_OpenDialog(p_Options.Filter, p_Options.Default, &path));
    if (result == Success)
        return Result<Path>::Ok(path);
    return Result<Path>::Error(result);
}
Result<Paths> OpenMultiple(const Options &p_Options)
{
    nfdpathset_t set;
    const Status result = toStatus(NFD_OpenDialogMultiple(p_Options.Filter, p_Options.Default, &set));
    if (result == Success)
    {
        Paths paths;
        const u32 count = static_cast<u32>(NFD_PathSet_GetCount(&set));
        for (u32 i = 0; i < count; ++i)
            paths.Append(NFD_PathSet_GetPath(&set, i));

        return Result<Paths>::Ok(paths);
    }
    return Result<Paths>::Error(result);
}

Result<Path> Save(const Options &p_Options)
{
    char *path;
    const Status result = toStatus(NFD_SaveDialog(p_Options.Filter, p_Options.Default, &path));
    if (result == Success)
        return Result<Path>::Ok(path);
    return Result<Path>::Error(result);
}
} // namespace Onyx::Dialog

#include "onyx/core/pch.hpp"
#include "onyx/core/dialog.hpp"
#include <nfd.h>

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
        return Success;
    }
}

Result<Path> OpenFolder(const char *fdefault)
{
    char *path;
    const Status result = toStatus(NFD_PickFolder(fdefault, &path));
    if (result == Success)
        return Result<Path>::Ok(path);
    return Result<Path>::Error(result);
}
Result<Path> OpenSingle(const Options &options)
{
    char *path;
    const Status result = toStatus(NFD_OpenDialog(options.Filter, options.Default, &path));
    if (result == Success)
        return Result<Path>::Ok(path);
    return Result<Path>::Error(result);
}
Result<Paths> OpenMultiple(const Options &options)
{
    nfdpathset_t set;
    const Status result = toStatus(NFD_OpenDialogMultiple(options.Filter, options.Default, &set));
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

Result<Path> Save(const Options &options)
{
    char *path;
    const Status result = toStatus(NFD_SaveDialog(options.Filter, options.Default, &path));
    if (result == Success)
        return Result<Path>::Ok(path);
    return Result<Path>::Error(result);
}
} // namespace Onyx::Dialog

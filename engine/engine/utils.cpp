#include "utils.hpp"
#include "tkit/serialization/yaml/container.hpp"

namespace Engine
{
static const char *getEnvVar(const char *name)
{
    TKIT_COMPILER_WARNING_IGNORE_PUSH()
    TKIT_MSVC_WARNING_IGNORE(4996)
    return std::getenv(name);
    TKIT_COMPILER_WARNING_IGNORE_POP()
}

fs::path GetHomePath()
{
#ifdef TKIT_OS_WINDOWS
    const char *var = "USERPROFILE";
#else
    const char *var = "HOME";
#endif

    const char *path = getEnvVar(var);
    return path ? path : "/";
}

fs::path GetConfigPath()
{
#ifdef TKIT_OS_WINDOWS
    const char *var = "APPDATA";
#elif defined(TKIT_OS_APPLE)
    const char *var = "HOME";
#else
    const char *var = "XDG_CONFIG_HOME";
#endif

    const char *root = getEnvVar(var);
#ifdef TKIT_OS_LINUX
    if (root)
        return fs::path{root} / "onyx" / "config.yaml";

    root = getEnvVar("HOME");
    return root ? (fs::path{root} / ".config" / "onyx" / "config.yaml") : fs::path{};
#elif defined(TKIT_OS_APPLE)
    return root ? (fs::path{root} / "Library" / "Application support" / "onyx" / "config.yaml") : fs::path{};
#else
    return root ? (fs::path{root} / "onyx" / "config.yaml") : fs::path{};
#endif
}

Result<TKit::StackArray<fs::path>> LoadRecentProjects()
{
    const fs::path configPath = GetConfigPath();
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(configPath, TKit::StackArray<fs::path>, "[ONYX][EDITOR] File {} not found");

    const auto res = YamlTree::FromFile(configPath);
    if (!res)
        return Result<TKit::StackArray<fs::path>>::Error(Error_DeserializationFailed, res.GetError());

    const YamlTree &tree = *res;
    const ConstYamlNode root = tree.GetRoot();

    const ConstYamlNode paths = root["Recent projects"];

    TKit::StackArray<fs::path> projects{};
    projects.Reserve(paths.GetChildCount());
    for (const ConstYamlNode path : paths)
    {
        const fs::path p = path.Read<TKit::StackString>().CString();
        if (fs::exists(p))
            projects.Append(p);
    }

    return projects;
}
Result<> UpdateRecentProjects(const fs::path &path)
{
    const fs::path configPath = GetConfigPath();
    YamlTree tree{};
    const bool exists = fs::exists(configPath);
    if (exists)
    {
        const auto res = YamlTree::FromFile(configPath);
        if (!res)
            return Result<>::Error(Error_DeserializationFailed, res.GetError());

        tree = *res;
    }
    else
        fs::create_directories(configPath.parent_path());

    YamlNode root = tree.GetRoot();
    YamlNode paths = root["Recent projects"];
    const TKit::TierString pstr = path.string();
    if (exists)
        for (u32 i = 0; i < paths.GetChildCount(); ++i)
        {
            const ConstYamlNode p = paths[i];
            if (p.Read<TKit::StackString>() == pstr)
            {
                paths.RemoveByIndex(i);
                break;
            }
        }

    YamlNode pnode = paths.Prepend(pstr);
    pnode |= TKit::YamlNodeFlag_ValSingleQuote;

    tree.ToFile(configPath);
    return Result<>::Ok();
}
} // namespace Engine

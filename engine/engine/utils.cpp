#include "utils.hpp"
#include "tkit/filesystem/filesystem.hpp"
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

TKit::TierPath GetHomePath()
{
#ifdef TKIT_OS_WINDOWS
    const char *var = "USERPROFILE";
#else
    const char *var = "HOME";
#endif

    const char *path = getEnvVar(var);
    return path ? path : "/";
}

TKit::TierPath GetConfigPath()
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
        return TKit::TierPath{root} / "onyx" / "config.yaml";

    root = getEnvVar("HOME");
    return root ? (TKit::TierPath{root} / ".config" / "onyx" / "config.yaml") : TKit::TierPath{};
#elif defined(TKIT_OS_APPLE)
    return root ? (TKit::TierPath{root} / "Library" / "Application support" / "onyx" / "config.yaml")
                : TKit::TierPath{};
#else
    return root ? (TKit::TierPath{root} / "onyx" / "config.yaml") : TKit::TierPath{};
#endif
}

Result<TKit::StackArray<TKit::TierPath>> LoadRecentProjects()
{
    const TKit::TierPath configPath = GetConfigPath();
    ONYX_GENERATE_ERROR_FILE_NOT_FOUND(configPath, "[ONYX][EDITOR] Config file '{}' does not exist",
                                       "[ONYX][EDITOR] Expected a config file at '{}', but found a directory");

    const auto res = YamlTree::FromFile(configPath);
    ONYX_RETURN_ERROR_IF(
        !res, Error_DeserializationFailed,
        "[ONYX][EDITOR] Failed to load recent projects: Could not deserialize config at '{}' with error: {}",
        configPath, res.GetError());

    const YamlTree &tree = *res;
    const ConstYamlNode root = tree.GetRoot();

    const ConstYamlNode paths = root["Recent projects"];

    TKit::StackArray<TKit::TierPath> projects{};
    projects.Reserve(paths.GetChildCount());
    for (const ConstYamlNode path : paths)
    {
        TKit::StackString entry;
        const auto pres = path.TryRead(entry);
        ONYX_RETURN_ERROR_IF(!pres, Error_DeserializationFailed,
                             "[ONYX][EDITOR] Failed to load recent projects: Could not read '{}': got {} - {}",
                             TKit::YamlGeneratePath(path), TKit::YamlDescribeNode(path), pres.GetError().Message);

        const TKit::StackPath p = entry;
        if (TKit::PathExists(p))
            projects.Append(p);
    }

    return projects;
}
Result<> UpdateRecentProjects(const TKit::TierPath &path)
{
    const TKit::TierPath configPath = GetConfigPath();
    YamlTree tree{};
    const bool exists = TKit::PathExists(configPath);
    if (exists)
    {
        const auto res = YamlTree::FromFile(configPath);
        ONYX_RETURN_ERROR_IF(
            !res, Error_DeserializationFailed,
            "[ONYX][EDITOR] Failed to update recent projects: Could not deserialize config at '{}' with error: {}",
            configPath, res.GetError());

        tree = *res;
    }
    else
    {
        const TKit::TierPath parent = configPath.GetParentPath();
        ONYX_CHECK_FILESYSTEM_ERROR(
            TKit::CreateDirectories(parent),
            "[ONYX][EDITOR] Failed to update recent projects: Could not create directories at '{}' with error: {}",
            parent, TKit::ToString(res.GetError()));
    }

    YamlNode root = tree.GetRoot();
    YamlNode paths = root["Recent projects"];
    const TKit::TierString &pstr = path.GetString();
    if (exists)
        for (u32 i = 0; i < paths.GetChildCount(); ++i)
        {
            const ConstYamlNode p = paths[i];

            TKit::StackString entry;
            const auto res = p.TryRead(entry);
            ONYX_RETURN_ERROR_IF(!res, Error_DeserializationFailed,
                                 "[ONYX][EDITOR] Failed to update recent projects: Could not read '{}': got {} - {}",
                                 TKit::YamlGeneratePath(p), TKit::YamlDescribeNode(p), res.GetError().Message);

            if (entry == pstr)
            {
                paths.RemoveByIndex(i);
                break;
            }
        }

    YamlNode pnode = paths.Prepend(pstr);
    pnode |= TKit::YamlNodeFlag_ValSingleQuote;

    ONYX_CHECK_FILESYSTEM_ERROR(tree.ToFile(configPath),
                                "[ONYX][EDITOR] Failed to update recent projects: Could not export serialized "
                                "configuration to '{}' with error: {}",
                                configPath, TKit::ToString(res.GetError()));
    return Result<>::Ok();
}
} // namespace Engine

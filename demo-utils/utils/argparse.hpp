#pragma once

namespace Onyx::Demo
{
enum class Scene
{
    None = 0,
    Setup2D = 1,
    Setup3D = 2
};

Scene ParseArguments(int argc, char **argv);
} // namespace Onyx::Demo

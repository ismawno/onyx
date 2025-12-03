#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/alias.hpp"

namespace Onyx::Demo
{
enum class ApplicationType : u8
{
    SingleWindow = 0,
    MultiWindow
};

Dimension ParseArguments(int argc, char **argv);
} // namespace Onyx::Demo

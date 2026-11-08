#pragma once

#include "onyx/core.hpp"
#include "tkit/utils/alias.hpp"
#include "tkit/utils/literals.hpp"
#include "tkit/utils/dimension.hpp"
#include "tkit/math/math.hpp"

namespace Engine
{
// Basically inherit all aliases from Toolkit
namespace Math = TKit::Math;
using namespace TKit::Alias;
using namespace TKit::Literals;
using TKit::D2;
using TKit::D3;
using TKit::D_Count;
using TKit::Dimension;
using YamlTree = TKit::YamlTree;
using ConstYamlNode = TKit::ConstYamlNode;
using YamlNode = TKit::YamlNode;

using Scene = u32;
using Viewport = u32;
using RenderView = u32;
using RenderContext = u32;

template <typename T = void> using Result = Onyx::Result<T>;
using enum Onyx::ErrorCode;
} // namespace Engine

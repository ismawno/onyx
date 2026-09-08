#pragma once

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
} // namespace Engine

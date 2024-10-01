#include "core/pch.hpp"
#include "onyx/draw/primitives/cuboid.hpp"
#include "onyx/draw/model.hpp"

namespace ONYX
{
Cuboid3D::Cuboid3D(const vec3 &p_Position, const vec3 &p_Dimension, const Color &p_Color) noexcept
    : ModelShape(Model::GetCube(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
    Transform.Position = p_Position;
    Transform.Scale = p_Dimension;
}

Cuboid3D::Cuboid3D(const Color &p_Color) noexcept
    : ModelShape(Model::GetCube(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, p_Color)
{
}
} // namespace ONYX
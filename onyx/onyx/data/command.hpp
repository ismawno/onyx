#pragma once

#include "onyx/data/state.hpp"
#include "onyx/data/options.hpp"
#include "onyx/object/mesh.hpp"
#include "onyx/object/primitives.hpp"

namespace Onyx
{
namespace Detail
{
enum class ShapeType2D : u8
{
    Triangle = 0,
    Square,
    NGon,
    Polygon,
    Circle,
    Stadium,
    RoundedSquare,
    Mesh,
};
enum class ShapeType3D : u8
{
    Triangle = 0,
    Square,
    NGon,
    Polygon,
    Circle,
    Stadium,
    RoundedSquare,
    Mesh,
    Cube,
    Cylinder,
    Sphere,
    Capsule,
    RoundedCube,
};
template <Dimension D> struct ShapeType;

template <> struct ShapeType<D2>
{
    using Type = ShapeType2D;
};
template <> struct ShapeType<D3>
{
    using Type = ShapeType3D;
};

template <typename T> struct DrawProperty
{
    DrawProperty() noexcept : DrawProperty(T{})
    {
    }
    DrawProperty(const T &p_Single) noexcept : Single(p_Single), IsSingle(true)
    {
    }
    DrawProperty(const TKit::Span<const T> p_Multi) noexcept : Multi(p_Multi), IsSingle(false)
    {
    }
    DrawProperty &operator=(const T &p_Single) noexcept
    {
        Single = p_Single;
        IsSingle = true;
    }
    DrawProperty &operator=(const TKit::Span<const T> p_Multi) noexcept
    {
        Multi = p_Multi;
        IsSingle = false;
    }

    const T &Get(const u32 p_Index) const noexcept
    {
        return IsSingle ? Single : Multi[p_Index];
    }

    union {
        T Single;
        TKit::Span<const T> Multi;
    };
    bool IsSingle;
};
} // namespace Detail

template <Dimension D> using Shape = Detail::ShapeType<D>::Type;

template <Dimension D> struct DrawCommand
{
    template <typename T> using DP = Detail::DrawProperty<T>;

    DrawCommand() noexcept = default;
    Shape<D> Shape{};

    TKit::Span<const fmat<D>> Transforms{};
    DP<RenderState<D>> State{};
    DP<fvec<D>> ShapeSize = fvec<D>{1.f};

    DP<Mesh<D>> Mesh{};
    DP<CircleOptions> CircleOptions{};
    DP<TKit::Span<const fvec2>> Vertices{};
    DP<u32> NGonSides = 3;
    DP<f32> Diameter = 1.f;
    DP<f32> Length = 1.f;
    DP<Resolution> Resolution = Onyx::Resolution::Medium;
};

} // namespace Onyx

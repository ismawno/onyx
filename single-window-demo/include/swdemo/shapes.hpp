#pragma once

#include "onyx/rendering/render_context.hpp"
#include "onyx/draw/transform.hpp"

namespace ONYX
{
template <u32 N>
    requires(IsDim<N>())
class Shape
{
  public:
    virtual ~Shape() = default;
    virtual const char *GetName() const noexcept = 0;

    virtual void Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept = 0;
    void Draw(RenderContext<N> *p_Context) noexcept;

    Transform<N> Transform;
    MaterialData<N> Material;
};

using Shape2D = Shape<2>;
using Shape3D = Shape<3>;

template <u32 N>
    requires(IsDim<N>())
class Triangle final : public Shape<N>
{
  public:
    const char *GetName() const noexcept override;
    void Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept override;
};

template <u32 N>
    requires(IsDim<N>())
class Rect final : public Shape<N>
{
  public:
    const char *GetName() const noexcept override;
    void Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept override;
};

template <u32 N>
    requires(IsDim<N>())
class Ellipse final : public Shape<N>
{
  public:
    const char *GetName() const noexcept override;
    void Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept override;
};

template <u32 N>
    requires(IsDim<N>())
class NGon final : public Shape<N>
{
  public:
    const char *GetName() const noexcept override;
    void Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept override;

    u32 Sides;
};

template <u32 N>
    requires(IsDim<N>())
class Polygon final : public Shape<N>
{
  public:
    const char *GetName() const noexcept override;
    void Draw(RenderContext<N> *p_Context, const mat<N> &p_Transform) noexcept override;

    DynamicArray<vec<N>> Vertices;
};

class Cuboid final : public Shape3D
{
    const char *GetName() const noexcept override;
    void Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept override;
};

class Ellipsoid final : public Shape3D
{
    const char *GetName() const noexcept override;
    void Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept override;
};

class Cylinder final : public Shape3D
{
    const char *GetName() const noexcept override;
    void Draw(RenderContext3D *p_Context, const mat4 &p_Transform) noexcept override;
};

} // namespace ONYX
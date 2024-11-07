#pragma once

#include "onyx/rendering/render_context.hpp"
#include "onyx/draw/transform.hpp"

namespace ONYX
{
template <Dimension D> class Shape
{
  public:
    virtual ~Shape() = default;
    virtual const char *GetName() const noexcept = 0;

    void Draw(RenderContext<D> *p_Context) noexcept;

    virtual void Edit() noexcept;

    Transform<D> Transform;
    MaterialData<D> Material;

    bool Fill = true;
    bool Outline = false;
    f32 OutlineWidth = 0.01f;
    Color OutlineColor = Color::ORANGE;

  private:
    virtual void draw(RenderContext<D> *p_Context) noexcept = 0;
};

template <Dimension D> class Triangle final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;
};

template <Dimension D> class Square final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;
};

template <Dimension D> class Circle final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;

    void Edit() noexcept override;

    f32 LowerAngle = 0.f;
    f32 UpperAngle = glm::two_pi<f32>();
    f32 Hollowness = 0.f;
};

template <Dimension D> class NGon final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;

    u32 Sides = 3;
};

template <Dimension D> class Polygon final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;

    DynamicArray<vec<D>> Vertices;
};

template <Dimension D> class Stadium final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;

    void Edit() noexcept override;

    f32 Length = 1.f;
    f32 Radius = 0.5f;
};

template <Dimension D> class RoundedSquare final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D> *p_Context) noexcept override;

    void Edit() noexcept override;

    vec2 Dimensions{1.f};
    f32 Radius = 0.5f;
};

class Cube final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D3> *p_Context) noexcept override;
};

class Sphere final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D3> *p_Context) noexcept override;
};

class Cylinder final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D3> *p_Context) noexcept override;
};

class Capsule final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D3> *p_Context) noexcept override;

    void Edit() noexcept override;

    f32 Length = 1.f;
    f32 Radius = 0.5f;
};

class RoundedCube final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void draw(RenderContext<D3> *p_Context) noexcept override;

    void Edit() noexcept override;

    vec3 Dimensions{1.f};
    f32 Radius = 0.5f;
};

} // namespace ONYX
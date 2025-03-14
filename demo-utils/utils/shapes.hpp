#pragma once

#include "onyx/rendering/render_context.hpp"
#include "onyx/draw/transform.hpp"

namespace Onyx::Demo
{
template <Dimension D> class Shape
{
  public:
    virtual ~Shape() = default;
    virtual const char *GetName() const noexcept = 0;

    void Draw(RenderContext<D> *p_Context) noexcept;

    virtual void Edit() noexcept;

    Transform<D> Transform;

  private:
    MaterialData<D> m_Material;

    bool m_Fill = true;
    bool m_Outline = false;
    f32 m_OutlineWidth = 0.01f;
    Color m_OutlineColor = Color::ORANGE;

    virtual void draw(RenderContext<D> *p_Context) noexcept = 0;
};

template <Dimension D> class Triangle final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
};

template <Dimension D> class Square final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
    fvec2 m_Dimensions{1.f};
};

template <Dimension D> class Circle final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
    fvec2 m_Dimensions{1.f};

    CircleOptions m_Options{};
};

template <Dimension D> class NGon final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

    u32 Sides = 3;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
    fvec2 m_Dimensions{1.f};
};

template <Dimension D> class Polygon final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

    TKit::StaticArray<fvec2, ONYX_MAX_POLYGON_VERTICES> Vertices;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
};

template <Dimension D> class Stadium final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
    f32 m_Length = 1.f;
    f32 m_Diameter = 1.f;
};

template <Dimension D> class RoundedSquare final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context) noexcept override;
    fvec2 m_Dimensions{1.f};
    f32 m_Diameter = 1.f;
};

class Cube final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context) noexcept override;
    fvec3 m_Dimensions{1.f};
};

class Sphere final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context) noexcept override;
    Resolution m_Res = Resolution::Medium;
    fvec3 m_Dimensions{1.f};
};

class Cylinder final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context) noexcept override;
    Resolution m_Res = Resolution::Medium;
    fvec3 m_Dimensions{1.f};
};

class Capsule final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context) noexcept override;
    Resolution m_Res = Resolution::Medium;
    f32 m_Length = 1.f;
    f32 m_Diameter = 1.f;
};

class RoundedCube final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context) noexcept override;
    Resolution m_Res = Resolution::Medium;
    fvec3 m_Dimensions{1.f};
    f32 m_Diameter = 1.f;
};

} // namespace Onyx::Demo
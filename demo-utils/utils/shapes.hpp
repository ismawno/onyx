#pragma once

#include "onyx/rendering/render_context.hpp"
#include "onyx/property/transform.hpp"
#include "onyx/object/mesh.hpp"
#include "tkit/container/static_array.hpp"
#include <string>

namespace Onyx::Demo
{
template <Dimension D> struct NamedMesh
{
    static const TKit::StaticArray16<NamedMesh<D>> &Get() noexcept;
    static TKit::StaticArray16<std::string> Query(std::string_view p_Directory) noexcept;

    static bool IsLoaded(std::string_view p_Name) noexcept;
    static VKit::FormattedResult<NamedMesh<D>> Load(std::string_view p_Name, std::string_view p_Path,
                                                    const fmat<D> &p_Transform) noexcept;

    std::string Name{};
    Mesh<D> Mesh{};
};

template <Dimension D> class Shape
{
  public:
    virtual ~Shape() = default;
    virtual const char *GetName() const noexcept = 0;

    void SetProperties(RenderContext<D> *p_Context) noexcept;

    void Draw(RenderContext<D> *p_Context) noexcept;
    void DrawRaw(RenderContext<D> *p_Context) const noexcept;

    void Draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) noexcept;
    void DrawRaw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept;

    virtual void Edit() noexcept;

    Transform<D> Transform;

  private:
    MaterialData<D> m_Material;

    bool m_Fill = true;
    bool m_Outline = false;
    f32 m_OutlineWidth = 0.01f;
    Color m_OutlineColor = Color::ORANGE;

    virtual void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept = 0;
};

template <Dimension D> class MeshShape final : public Shape<D>
{
  public:
    MeshShape(const NamedMesh<D> &p_Mesh) noexcept;

    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;

    NamedMesh<D> m_Mesh{};
    fvec<D> m_Dimensions{1.f};
};

template <Dimension D> class Triangle final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
};

template <Dimension D> class Square final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
    fvec2 m_Dimensions{1.f};
};

template <Dimension D> class Circle final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
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
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
    fvec2 m_Dimensions{1.f};
};

template <Dimension D> class Polygon final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

    PolygonVerticesArray Vertices;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
};

template <Dimension D> class Stadium final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
    f32 m_Length = 1.f;
    f32 m_Diameter = 1.f;
};

template <Dimension D> class RoundedSquare final : public Shape<D>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const noexcept override;
    fvec2 m_Dimensions{1.f};
    f32 m_Diameter = 1.f;
};

class ONYX_API Cube final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept override;
    fvec3 m_Dimensions{1.f};
};

class ONYX_API Sphere final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept override;
    Resolution m_Res = Resolution::Medium;
    fvec3 m_Dimensions{1.f};
};

class ONYX_API Cylinder final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept override;
    Resolution m_Res = Resolution::Medium;
    fvec3 m_Dimensions{1.f};
};

class ONYX_API Capsule final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept override;
    Resolution m_Res = Resolution::Medium;
    f32 m_Length = 1.f;
    f32 m_Diameter = 1.f;
};

class ONYX_API RoundedCube final : public Shape<D3>
{
  public:
    const char *GetName() const noexcept override;
    void Edit() noexcept override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const noexcept override;

    Resolution m_Res = Resolution::Medium;
    fvec3 m_Dimensions{1.f};
    f32 m_Diameter = 1.f;
};

} // namespace Onyx::Demo

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
    static const TKit::StaticArray16<NamedMesh<D>> &Get();
    static TKit::StaticArray16<std::string> Query(std::string_view p_Directory);

    static bool IsLoaded(std::string_view p_Name);
    static VKit::FormattedResult<NamedMesh<D>> Load(std::string_view p_Name, std::string_view p_Path,
                                                    const f32m<D> &p_Transform);

    std::string Name{};
    Mesh<D> Mesh{};
};

template <Dimension D> class Shape
{
  public:
    virtual ~Shape() = default;
    virtual const char *GetName() const = 0;

    void SetProperties(RenderContext<D> *p_Context);

    void Draw(RenderContext<D> *p_Context);
    void DrawRaw(RenderContext<D> *p_Context) const;

    void Draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform);
    void DrawRaw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const;

    virtual void Edit();

    Transform<D> Transform;

  private:
    MaterialData<D> m_Material;

    bool m_Fill = true;
    bool m_Outline = false;
    f32 m_OutlineWidth = 0.01f;
    Color m_OutlineColor = Color::ORANGE;

    virtual void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const = 0;
};

template <Dimension D> class MeshShape final : public Shape<D>
{
  public:
    MeshShape(const NamedMesh<D> &p_Mesh);

    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;

    NamedMesh<D> m_Mesh{};
    f32v<D> m_Dimensions{1.f};
};

template <Dimension D> class Triangle final : public Shape<D>
{
  public:
    const char *GetName() const override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
};

template <Dimension D> class Square final : public Shape<D>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
    f32v2 m_Dimensions{1.f};
};

template <Dimension D> class Circle final : public Shape<D>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
    f32v2 m_Dimensions{1.f};

    CircleOptions m_Options{};
};

template <Dimension D> class NGon final : public Shape<D>
{
  public:
    const char *GetName() const override;
    void Edit() override;

    u32 Sides = 3;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
    f32v2 m_Dimensions{1.f};
};

template <Dimension D> class Polygon final : public Shape<D>
{
  public:
    const char *GetName() const override;
    void Edit() override;

    PolygonVerticesArray Vertices;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
};

template <Dimension D> class Stadium final : public Shape<D>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
    f32 m_Length = 1.f;
    f32 m_Diameter = 1.f;
};

template <Dimension D> class RoundedSquare final : public Shape<D>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D> *p_Context, const Onyx::Transform<D> &p_Transform) const override;
    f32v2 m_Dimensions{1.f};
    f32 m_Diameter = 1.f;
};

class ONYX_API Cube final : public Shape<D3>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const override;
    f32v3 m_Dimensions{1.f};
};

class ONYX_API Sphere final : public Shape<D3>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const override;
    Resolution m_Res = Resolution::Medium;
    f32v3 m_Dimensions{1.f};
};

class ONYX_API Cylinder final : public Shape<D3>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const override;
    Resolution m_Res = Resolution::Medium;
    f32v3 m_Dimensions{1.f};
};

class ONYX_API Capsule final : public Shape<D3>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const override;
    Resolution m_Res = Resolution::Medium;
    f32 m_Length = 1.f;
    f32 m_Diameter = 1.f;
};

class ONYX_API RoundedCube final : public Shape<D3>
{
  public:
    const char *GetName() const override;
    void Edit() override;

  private:
    void draw(RenderContext<D3> *p_Context, const Onyx::Transform<D3> &p_Transform) const override;

    Resolution m_Res = Resolution::Medium;
    f32v3 m_Dimensions{1.f};
    f32 m_Diameter = 1.f;
};

} // namespace Onyx::Demo

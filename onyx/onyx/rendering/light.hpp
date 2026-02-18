#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/color.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx
{
template <Dimension D> class PointLight
{
  public:
    PointLight(const f32v<D> &pos = f32v<D>{0.f}, const f32 radius = 1.f, const f32 intensity = 1.f,
               const Color &color = Color::White)
        : m_Position(pos), m_Radius(radius), m_Intensity(intensity), m_Color(color)
    {
    }

    const f32v<D> &GetPosition() const
    {
        return m_Position;
    }
    f32 GetRadius() const
    {
        return m_Radius;
    }
    f32 GetIntensity() const
    {
        return m_Intensity;
    }
    const Color &GetColor() const
    {
        return m_Color;
    }

    void SetViewMask(const ViewMask mask)
    {
        m_ViewMask = mask;
        m_Dirty = true;
    }

    void SetPosition(const f32v<D> &pos)
    {
        m_Position = pos;
        m_Dirty = true;
    }
    void SetRadius(const f32 radius)
    {
        m_Radius = radius;
        m_Dirty = true;
    }
    void SetIntensity(const f32 intensity)
    {
        m_Intensity = intensity;
        m_Dirty = true;
    }
    void SetColor(const Color &color)
    {
        m_Color = color;
        m_Dirty = true;
    }

    bool IsDirty() const
    {
        return m_Dirty;
    }

    void MarkNonDirty()
    {
        m_Dirty = false;
    }

    PointLightData<D> CreateInstanceData() const
    {
        PointLightData<D> pdata;
        pdata.Position = m_Position;
        pdata.Intensity = m_Intensity;
        pdata.Radius = m_Radius;
        pdata.Color = m_Color.Pack();
        pdata.ViewMask = m_ViewMask;
        return pdata;
    }

  private:
    f32v<D> m_Position;
    f32 m_Radius;
    f32 m_Intensity;
    ViewMask m_ViewMask;
    Color m_Color;
    bool m_Dirty = true;
};

class DirectionalLight
{
  public:
    DirectionalLight(const f32v3 &dir = f32v3{0.f}, const f32 intensity = 1.f, const Color &color = Color::White)
        : m_Direction(dir), m_Intensity(intensity), m_Color(color)
    {
    }

    const f32v3 &GetDirection() const
    {
        return m_Direction;
    }

    f32 GetIntensity() const
    {
        return m_Intensity;
    }
    const Color &GetColor() const
    {
        return m_Color;
    }

    void SetViewMask(const ViewMask mask)
    {
        m_ViewMask = mask;
        m_Dirty = true;
    }

    void SetDirection(const f32v3 &dir)
    {
        m_Direction = dir;
        m_Dirty = true;
    }
    void SetIntensity(const f32 intensity)
    {
        m_Intensity = intensity;
        m_Dirty = true;
    }
    void SetColor(const Color &color)
    {
        m_Color = color;
        m_Dirty = true;
    }

    bool IsDirty() const
    {
        return m_Dirty;
    }

    void MarkNonDirty()
    {
        m_Dirty = false;
    }

    DirectionalLightData CreateInstanceData() const
    {
        DirectionalLightData ddata;
        ddata.Direction = m_Direction;
        ddata.Intensity = m_Intensity;
        ddata.Color = m_Color.Pack();
        ddata.ViewMask = m_ViewMask;
        return ddata;
    }

  private:
    f32v3 m_Direction;
    f32 m_Intensity;
    Color m_Color;
    ViewMask m_ViewMask;
    bool m_Dirty = true;
};
} // namespace Onyx

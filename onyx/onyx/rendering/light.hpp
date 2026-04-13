#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/color.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/property/transform.hpp"

namespace Onyx
{
struct DepthBiasParameters
{
    f32 Constant = 1.25f;
    f32 Clamp = 0.f;
    f32 Slope = 1.75f;
};

template <Dimension D> struct PointLightSpecs
{
    f32v<D> Position = f32v<D>{0.f};
    f32 Radius = 1.f;
    f32 Intensity = 0.8f;
    Color Tint = Color::White;
};

template <> struct PointLightSpecs<D3>
{
    f32v3 Position = f32v3{0.f};
    f32 Radius = 1.f;
    f32 Intensity = 0.8f;
    Color Tint = Color::White;
    DepthBiasParameters DepthBias{};
};

template <Dimension D> class PointLight
{
  public:
    using InstanceData = PointLightData<D>;
    PointLight(const PointLightSpecs<D> &specs = {}) : m_Specs{specs}
    {
    }

    const PointLightSpecs<D> &GetSpecs() const
    {
        return m_Specs;
    }
    void SetSpecs(const PointLightSpecs<D> &specs)
    {
        m_Specs = specs;
        m_Dirty = true;
    }

    ViewMask GetViewMask() const
    {
        return m_ViewMask;
    }
    void SetViewMask(const ViewMask mask)
    {
        m_ViewMask = mask;
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

    // plz user dont use this
    void SetShadowMapOffset(const u32 offset)
    {
        m_ShadowMapOffset = offset;
    }

    PointLightData<D> CreateInstanceData() const
    {
        PointLightData<D> pdata;
        pdata.Position = m_Specs.Position;
        pdata.Intensity = m_Specs.Intensity;
        pdata.Radius = m_Specs.Radius;
        pdata.Color = m_Specs.Tint.Pack();
        pdata.ViewMask = m_ViewMask;
        pdata.ShadowMapOffset = m_ShadowMapOffset;
        return pdata;
    }

  private:
    PointLightSpecs<D> m_Specs;
    ViewMask m_ViewMask;
    u32 m_ShadowMapOffset = 0;
    bool m_Dirty = true;
};

// TODO(Isma): Fix direction sign
struct DirectionalLightSpecs
{
    f32v3 Position = f32v3{5.f};
    f32v3 Direction = f32v3{1.f};
    f32 Range = 20.f;
    f32 Depth = 200.f;
    f32 Intensity = 0.8f;
    Color Tint = Color::White;
    DepthBiasParameters DepthBias{};
};

class DirectionalLight
{
  public:
    using InstanceData = DirectionalLightData;
    DirectionalLight(const DirectionalLightSpecs &specs = {}) : m_Specs(specs)
    {
    }

    const DirectionalLightSpecs &GetSpecs() const
    {
        return m_Specs;
    }

    ViewMask GetViewMask() const
    {
        return m_ViewMask;
    }

    void SetSpecs(const DirectionalLightSpecs &specs)
    {
        m_Specs = specs;
        m_Dirty = true;
    }

    void SetViewMask(const ViewMask mask)
    {
        m_ViewMask = mask;
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

    // plz user dont use this
    void SetShadowMapOffset(const u32 offset)
    {
        m_ShadowMapOffset = offset;
    }

    DirectionalLightData CreateInstanceData() const
    {
        DirectionalLightData ddata;
        const f32 r = m_Specs.Range;
        const f32 d = m_Specs.Depth;
        ddata.ProjectionView =
            CreateTransformData<D3>(Transform<D3>::Orthographic(-r, r, -r, r, 0.f, d) *
                                    Transform<D3>::LookTowards(m_Specs.Position, -m_Specs.Direction));
        ddata.Direction = m_Specs.Direction;
        ddata.Intensity = m_Specs.Intensity;
        ddata.Color = m_Specs.Tint.Pack();
        ddata.ViewMask = m_ViewMask;
        ddata.ShadowMapOffset = m_ShadowMapOffset;
        return ddata;
    }

  private:
    DirectionalLightSpecs m_Specs;
    ViewMask m_ViewMask;
    u32 m_ShadowMapOffset = 0;
    bool m_Dirty = true;
};
} // namespace Onyx

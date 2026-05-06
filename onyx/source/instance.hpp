#pragma once

#include "onyx/instance.hpp"
#include "onyx/math.hpp"
#include "onyx/handle.hpp"
#include "vkit/resource/host_buffer.hpp"

namespace Onyx
{
template <Dimension D> struct TransformData;

template <> struct TransformData<D2>
{
    f32v2 Column0;
    f32v2 Column1;
    f32v2 Column3;
};

template <> struct TransformData<D3>
{
    f32v4 Row0;
    f32v4 Row1;
    f32v4 Row2;
};

template <Dimension D> TransformData<D> CreateTransformData(const f32m<D> &transform)
{
    TransformData<D> data;
    if constexpr (D == D2)
    {
        data.Column0 = f32v2{transform[0]};
        data.Column1 = f32v2{transform[1]};
        data.Column3 = f32v2{transform[2]};
    }
    else
    {
        data.Row0 = f32v4{transform[0][0], transform[1][0], transform[2][0], transform[3][0]};
        data.Row1 = f32v4{transform[0][1], transform[1][1], transform[2][1], transform[3][1]};
        data.Row2 = f32v4{transform[0][2], transform[1][2], transform[2][2], transform[3][2]};
    }
    return data;
}

template <Dimension D> struct InstanceData
{
    TransformData<D> Transform;
    u32 FillColor;
    u32 OutlineColor;
    u32 Alignment;
    u32 MatHandle;
    f32 OutlineWidth;
};
template <> struct InstanceData<D2>
{
    TransformData<D2> Transform;
    u32 FillColor;
    u32 OutlineColor;
    u32 Alignment;
    u32 MatHandle;
    f32 OutlineWidth;
    u32 DepthCounter;
};

template <Dimension D> struct StaticInstanceData
{
    InstanceData<D> Data;
    u32 BoundsHandle;
};

template <Dimension D> struct CircleInstanceData
{
    InstanceData<D> Data;
    ArcData Arc;
    FadeData Fade;
};

template <Dimension D> struct ParametricInstanceData
{
    InstanceData<D> Data;
    u32 BoundsHandle;
    InstanceParameters Parameters;
    ParametricShape Shape;
};

template <Dimension D> struct GlyphInstanceData
{
    InstanceData<D> Data;
    u32 BoundsHandle;
    u32 SamplerHandle;
    u32 AtlasHandle;
};

template <Dimension D> u32 GetInstanceSize(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return sizeof(CircleInstanceData<D>);
    case Geometry_Static:
        return sizeof(StaticInstanceData<D>);
    case Geometry_Parametric:
        return sizeof(ParametricInstanceData<D>);
    case Geometry_Glyph:
        return sizeof(GlyphInstanceData<D>);
    default:
        TKIT_FATAL("[ONYX][INSTANCE] Unrecognized geometry type");
        return 0;
    }
}

template <Dimension D> struct PointLightData;
template <> struct PointLightData<D3>
{
    f32v3 Position;
    f32 LightSize;
    f32 Intensity;
    f32 LightRadius;
    f32 ShadowRadius;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
};

template <> struct PointLightData<D2>
{
    f32v2 Position;
    f32v2 Direction;
    f32 LightSize;
    f32 Intensity;
    f32 LightRadius;
    f32 ShadowRadius;
    f32 Angle;
    f32 Decay;
    f32 Extent;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
};

#define ONYX_MAX_CASCADES 4

template <Dimension D> struct DirectionalLightData;
template <> struct DirectionalLightData<D2>
{
    TransformData<D2> ProjectionView;
    f32v2 Direction;
    f32 LightOffset;
    f32 LightExtent;
    f32 ShadowExtent;
    f32 Decay;
    f32 TanAngleSize;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
};

struct CascadeData
{
    TransformData<D3> ProjectionView;
    f32v2 InvSize;
    f32 DepthRange;
    f32 Split;
};

template <> struct DirectionalLightData<D3>
{
    TKit::FixedArray<CascadeData, ONYX_MAX_CASCADES> Cascades;
    f32v3 Direction;
    f32v3 Offset;
    f32v2 Extent;
    f32v3 PlaneVec1;
    f32v3 PlaneVec2;
    f32 Decay;
    f32 TanAngleSize;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;
    u32 CascadeCount;
    u32 CascadeEnable;
    ViewMask ViewMask;
    LightFlags Flags;
};

struct SpotLightData
{
    f32m4 ProjectionView;
    f32v3 Position;
    f32v3 Direction;
    f32v2 ShadowInvSize;
    f32 Near;
    f32 Far;
    f32 LightSize;
    f32 CosHalfPov;
    f32 LightRange;
    f32 Decay;
    f32 Intensity;
    u32 Color;
    u32 ShadowMapOffset;
    ViewMask ViewMask;
    LightFlags Flags;
};

template <Dimension D> struct ShadedPushConstantData;

using ShadedFlags = u32;
enum ShadedFlagBit : ShadedFlags
{
    ShadedFlag_Shadows = 1 << 0,
};

template <> struct ShadedPushConstantData<D2>
{
    f32m4 ProjectionView;
    TKit::FixedArray<u32, LightTypeCount<D2>> LightRanges{};
    TKit::FixedArray<f32, LightTypeCount<D2>> TexelSizes{};
    u32 AmbientColor;
    ShadedFlags Flags;
    ViewMask ViewBit;
};

template <> struct ShadedPushConstantData<D3>
{
    f32m4 ProjectionView;
    f32v3 ViewPosition;
    f32v3 ViewForward;
    TKit::FixedArray<u32, LightTypeCount<D3>> LightRanges{};
    TKit::FixedArray<f32, LightTypeCount<D3>> TexelSizes{};
    u32 AmbientColor;
    ShadedFlags Flags;
    ViewMask ViewBit;
};

using FlatPushConstantData = f32m4;
using CompositorPushConstantData = u32;

struct PostProcessPushConstantData
{
    u32v2 Extent;
    u32 AttachmentIndex;
    u32 MaxOutlineWidth;
    u32 Flags;
};

template <Dimension D> struct ShadowPushConstantData
{
    f32m4 LightProjection;
};
template <> struct ShadowPushConstantData<D3>
{
    f32m4 LightProjection;
    f32v3 LightPos;
    f32 Far;
    f32 DepthBias;
};

struct RayMarchPushConstantData
{
    // Points
    f32 StartAngle;
    f32 EndAngle;

    u32 Mode;

    u32 OcclusionMapIndex;
    u32 OcclusionResolution;
    u32 ShadowMapIndex;
    u32 ShadowResolution;
    f32 DistanceBias;
};

struct InstanceDataBuffer
{
    VKit::HostBuffer Data{};
    u32 InstanceSize = 0;
    u32 Instances = 0;
    u32 Capacity = 0;
};

struct InstanceDataArrays
{
    InstanceDataBuffer Circles{};
    TKit::FixedArray<TKit::FixedArray<TKit::TierArray<InstanceDataBuffer>, ONYX_MAX_RESOURCE_POOLS>, Resource_MeshCount>
        Meshes{};
};

} // namespace Onyx

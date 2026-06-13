#pragma once

#include "onyx/instance.hpp"
#include "onyx/math.hpp"
#include "onyx/handle.hpp"
#include "onyx/resources.hpp"
#include "vkit/resource/host_buffer.hpp"
#include "tkit/container/bitset.hpp"

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

// having world rect per instance is a bit painful. bloats instance data a lot, but i really could not find a better
// alternative due to how contexts are designed to be independent and its data being read only once recorded
template <Dimension D> struct InstanceData
{
    TransformData<D> Transform;
    WorldRect<D> Rect;
    u32 FillColor;
    u32 OutlineColor;
    u32 MatOrSamplerTex;
    u32 TexOffset;
    u32 TexScale;
    f32 OutlineWidth;
};
template <> struct InstanceData<D2>
{
    TransformData<D2> Transform;
    WorldRect<D2> Rect;
    u32 FillColor;
    u32 OutlineColor;
    u32 MatOrSamplerTex;
    u32 TexOffset;
    u32 TexScale;
    f32 OutlineWidth;
    u32 DepthCounter;
};

template <Dimension D> using DynamicInstanceData = InstanceData<D>;

template <Dimension D> struct StaticInstanceData
{
    InstanceData<D> Data;
    u32 Alignment;
    u32 BoundsId;
};

template <Dimension D> struct CircleInstanceData
{
    InstanceData<D> Data;
    u32 Alignment;
    ArcData Arc;
    FadeData Fade;
};

template <Dimension D> struct ParametricInstanceData
{
    InstanceData<D> Data;
    u32 Alignment;
    u32 BoundsId;
    InstanceParameters Parameters;
    ParametricShape Shape;
};

template <Dimension D> struct GlyphInstanceData
{
    InstanceData<D> Data;
    u32 SamplerAtlasId;
    f32 UnitRange;
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
    case Geometry_Dynamic:
        return sizeof(DynamicInstanceData<D>);

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
    ShadedFlag_Shadows = 1U << 0,
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
using BlendPushConstantData = u32;

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

struct LocalResourceRegistry
{
    TKit::TierArray<Resource> ResourceIds{};

    // NOTE(Isma): Is this enough?
    TKit::StaticBitSet2048 UsedResourceIds{2048};

    void Clear()
    {
        ResourceIds.Clear();
        UsedResourceIds.ClearAll();
    }

    void RegisterResourceId(const u32 rid)
    {
        if (!UsedResourceIds[rid])
        {
            UsedResourceIds.Set(rid);
            ResourceIds.Append(rid);
        }
    }
};

struct InstanceResourceGroup
{
    TKit::TierArray<InstanceDataBuffer> Instances{};
    LocalResourceRegistry Registry{};
};

struct InstanceDataArrays
{
    InstanceDataBuffer Circles{};
    InstanceResourceGroup DynamicMeshes{};
    MeshInstanceGrouping<InstanceResourceGroup> Meshes{};
};

template <Dimension D, typename F> void ForEachResourceGroup(F &&func)
{
    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
            for (u32 mtype = 0; mtype < Resource_MeshPoolCount; ++mtype)
            {
                const ResourceType rtype = ResourceType(mtype);
                const TKit::Span<const u32> poolIds = Resources::GetResourcePoolIds<D>(rtype);
                for (const u32 pid : poolIds)
                    func(bpass, rmode, mtype, pid);
            }
}
template <Dimension D, typename F> void ForEachDynamicMeshResourceGroup(F &&func)
{
    for (u32 bpass = 0; bpass < BlendPass_Count; ++bpass)
        for (u32 rmode = 0; rmode < RenderMode_Count; ++rmode)
            func(bpass, rmode);
}

} // namespace Onyx

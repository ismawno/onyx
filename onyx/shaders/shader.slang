#include "qol.slang"

#if defined(ONYX_PASS_FILL) || (defined(ONYX_PASS_SHADOW) && defined(ONYX_DIMENSION_2D))
#    define ONYX_USES_MATERIAL
#endif

#if defined(ONYX_PASS_FILL) || (defined(ONYX_PASS_SHADOW) && defined(ONYX_DIMENSION_3D))
#    define ONYX_USES_WORLD_POS
#endif

#if !defined(ONYX_GEOMETRY_CIRCLE) && !defined(ONYX_GEOMETRY_GLYPH)
#   define ONYX_USES_BOUNDS
#endif

#if defined(ONYX_DIMENSION_2D) || defined(ONYX_GEOMETRY_CIRCLE)
#    define ONYX_VEC4(v2) f32v4((v2), 0.0, 1.0)
#else
#    define ONYX_VEC4(v3) f32v4((v3), 1.0)
#endif

#ifdef ONYX_DIMENSION_2D
#    define ONYX_VEC(v4) (v4).xy
#else
#    define ONYX_VEC(v4) (v4).xyz
#endif

#ifdef ONYX_USES_MATERIAL
#    include "material.slang"
#endif

#if defined(ONYX_PASS_SHADOW) && defined(ONYX_DIMENSION_3D)
#    define FragOutput f32
#    define FragSuffix : SV_Depth
#else
#    define FragOutput f32v4
#    define FragSuffix
#endif

#ifdef ONYX_DIMENSION_2D

#    define LightData LightData2D
#    define BoundsArray Bounds2D
#    define InstanceData InstanceData2D
#    define MaterialBuffer Materials2D
#    define PointLightBuffer PointLights2D
#    define ShadowSamplerResource SamplerState
#    define ResourceTable ResourceTable2D
#    define f32v f32v2

#    define ComputeTransform ComputeTransform2D
#    define ComputeAlignment ComputeAlignment2D
#    define ComputeStencilScale ComputeStencilScale2D
#    define CheckParametricFragment CheckParametricFragment2D

#    ifdef ONYX_GEOMETRY_CIRCLE
#        include "circle.slang"
#        define Instances CircleInstances2D
#        define GeometryInstanceData CircleInstanceData2D
#    elif defined(ONYX_GEOMETRY_STATIC)
#        include "static.slang"
#        define Instances StaticInstances2D
#        define GeometryInstanceData StaticInstanceData2D
#    elif defined(ONYX_GEOMETRY_PARAMETRIC)
#        include "parametric.slang"
#        define Instances ParametricInstances2D
#        define GeometryInstanceData ParametricInstanceData2D
#    elif defined(ONYX_GEOMETRY_GLYPH)
#        include "glyph.slang"
#        define Instances GlyphInstances2D
#        define GeometryInstanceData GlyphInstanceData2D
#    endif

#else

#    define LightData LightData3D
#    define BoundsArray Bounds3D
#    define InstanceData InstanceData3D
#    define MaterialBuffer Materials3D
#    define PointLightBuffer PointLights3D
#    define ShadowSamplerResource SamplerComparisonState
#    define ResourceTable ResourceTable3D
#    define f32v f32v3

#    define ComputeTransform ComputeTransform3D
#    define ComputeAlignment ComputeAlignment3D
#    define ComputeStencilScale ComputeStencilScale3D
#    define CheckParametricFragment CheckParametricFragment3D

#    ifdef ONYX_GEOMETRY_CIRCLE
#        include "circle.slang"
#        define Instances CircleInstances3D
#        define GeometryInstanceData CircleInstanceData3D
#    elif defined(ONYX_GEOMETRY_STATIC)
#        include "static.slang"
#        define Instances StaticInstances3D
#        define GeometryInstanceData StaticInstanceData3D
#    elif defined(ONYX_GEOMETRY_PARAMETRIC)
#        include "parametric.slang"
#        define Instances ParametricInstances3D
#        define GeometryInstanceData ParametricInstanceData3D
#    elif defined(ONYX_GEOMETRY_GLYPH)
#        include "glyph.slang"
#        define Instances GlyphInstances3D
#        define GeometryInstanceData GlyphInstanceData3D
#    endif

#endif

#ifndef ONYX_GEOMETRY_CIRCLE
struct VertexInput
{
    f32v Position : POSITION;

#    ifdef ONYX_GEOMETRY_GLYPH
    f32v2 AtlasCoords : TEXCOORD0;
#    endif

#    ifdef ONYX_PASS_FILL
    f32v2 TexCoords : TEXCOORD0;
#    endif

#    if defined(ONYX_DIMENSION_3D) && !defined(ONYX_GEOMETRY_GLYPH)
    f32v3 Normal : NORMAL;

#        ifdef ONYX_PASS_FILL
    f32v4 Tangent : TANGENT;
#        endif

#    endif

#    ifdef ONYX_GEOMETRY_PARAMETRIC
    ParametricRegionFlags Region;
#    endif

}
#endif

struct FragInput
{
#ifdef ONYX_GEOMETRY_CIRCLE
    [[vk::location(0)]]
    f32v2 LocalPosition;
#elif defined(ONYX_GEOMETRY_PARAMETRIC)
    [[vk::location(0)]]
    f32v LocalPosition;
#endif

#ifdef ONYX_PASS_FILL
    [[vk::location(1)]]
    f32v2 TexCoords;
#endif

#ifdef ONYX_USES_WORLD_POS
    [[vk::location(2)]]
    f32v WorldPosition;
#endif

#ifdef ONYX_USES_MATERIAL
    [[vk::location(3)]]
    nointerpolation u32 MatId;
#endif

#ifdef ONYX_GEOMETRY_GLYPH
    [[vk::location(4)]]
    f32v2 AtlasCoords;

    [[vk::location(5)]]
    nointerpolation u32 SamplerId;

    [[vk::location(6)]]
    nointerpolation u32 AtlasId;
#endif

#ifdef ONYX_GEOMETRY_PARAMETRIC
    [[vk::location(7)]]
    nointerpolation u32v2 ShapeInfo;
#endif

#ifdef ONYX_GEOMETRY_CIRCLE
    [[vk::location(8)]]
    nointerpolation ArcData Arc;

#    ifndef ONYX_PASS_SHADOW
    [[vk::location(11)]]
    nointerpolation f32v2 Fade;
#    endif

#endif

#ifndef ONYX_PASS_SHADOW
    [[vk::location(12)]]
    nointerpolation f32v4 FragColor;
#endif

#if defined(ONYX_DIMENSION_3D) && defined(ONYX_PASS_FILL)
    [[vk::location(13)]]
    f32m3 TBN;
#endif

    f32v4 Position : SV_Position;
}

struct PushConstants
{
    f32m4 ProjectionView;
#ifdef ONYX_PASS_FILL
    LightData Light;
#elif defined(ONYX_PASS_STENCIL)
    f32 OutlineMultiplier;
#elif defined(ONYX_PASS_SHADOW) && defined(ONYX_DIMENSION_3D)
    f32v3 LightPos;
    f32 Far;
    f32 DepthBias;
#endif
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING, 0)]]
Instances g_Instances;

#ifdef ONYX_USES_BOUNDS

[[vk::binding(ONYX_BOUNDS_BINDING, 0)]]
BoundsArray g_Bounds;

#endif

[shader("vertex")]
FragInput mainVS(
#ifdef ONYX_GEOMETRY_CIRCLE
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
#else
    const VertexInput input,
#endif
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const GeometryInstanceData gdata = g_Instances[instanceIndex];
    const InstanceData data = gdata.Data;

    const f32m4 transform = ComputeTransform(data.Transform);

#ifdef ONYX_GEOMETRY_CIRCLE

    const u32 vertexIndex = vertexId + baseVertex;
    const f32v2 alignment = ComputeCircleAlignment(data.Alignment);
    const f32v2 localPos = g_Positions[vertexIndex];

#else
#    ifndef ONYX_GEOMETRY_GLYPH
    const f32v alignment = ComputeAlignment(data.Alignment, gdata.BoundsHandle, g_Bounds);
#    else

    //TODO(Isma): Maybe avoid this zero sub?

    const f32v alignment = f32v(0.f);
#    endif
    const f32v localPos = input.Position;
#endif

#ifdef ONYX_GEOMETRY_PARAMETRIC

    const InstanceParameters params = gdata.Parameters;
#    ifdef ONYX_DIMENSION_2D
    const f32v2 pos = GetParametricPosition2D(localPos, params, input.Region, alignment);
#    else
    const f32v3 pos = GetParametricPosition3D(localPos, input.Normal, params, input.Region, alignment);
#    endif

    const f32v4 shiftedPos = ONYX_VEC4(pos);

#else
    const f32v4 shiftedPos = ONYX_VEC4(localPos - alignment);
#endif
    const f32v4 worldPos = mul(shiftedPos, transform);

    FragInput finput;
    finput.Position = mul(worldPos, g_PushData.ProjectionView);

#ifdef ONYX_DIMENSION_2D
    finput.Position.z = ComputeDepth(data.DepthCounter);
#elif defined(ONYX_PASS_FILL)
    const f32m3 normalMatrix = ComputeNormalMatrix(f32m3(transform));

#    if defined(ONYX_GEOMETRY_CIRCLE) || defined(ONYX_GEOMETRY_GLYPH)

    const f32v3 N = normalize(normalMatrix[2]);
    const f32v3 T = normalize(normalMatrix[0]);
    const f32v3 B = cross(N, T);

#    else

    const f32v3 N = normalize(mul(input.Normal, normalMatrix));
    const f32v3 T = normalize(mul(input.Tangent.xyz, normalMatrix));
    const f32v3 B = cross(N, T) * input.Tangent.w;

#    endif

    finput.TBN = f32m3(T, B, N);
#endif

#ifdef ONYX_PASS_FILL
#    ifdef ONYX_GEOMETRY_CIRCLE
    finput.TexCoords = g_TexCoords[vertexIndex];
#    else
    finput.TexCoords = input.TexCoords;
#    endif
#endif

#ifdef ONYX_GEOMETRY_GLYPH
    finput.AtlasCoords = input.AtlasCoords;
    finput.SamplerId = GetAssetId(gdata.SamplerHandle);
    finput.AtlasId = GetAssetId(gdata.AtlasHandle);
#endif

#if defined(ONYX_GEOMETRY_CIRCLE) || defined(ONYX_GEOMETRY_PARAMETRIC)
    finput.LocalPosition = localPos;
#endif

#ifdef ONYX_GEOMETRY_PARAMETRIC
    finput.ShapeInfo = u32v2(params.Shape, input.Region);
#endif

#ifdef ONYX_USES_WORLD_POS
    finput.WorldPosition = ONYX_VEC(worldPos);
#endif

#ifdef ONYX_USES_MATERIAL
    finput.MatId = GetAssetId(data.MatHandle);
#endif

#ifdef ONYX_GEOMETRY_CIRCLE

    finput.Arc.CosSin = f32v4(gdata.Arc.LowerCos, gdata.Arc.LowerSin, gdata.Arc.UpperCos, gdata.Arc.UpperSin);
    finput.Arc.AngleOverflow = gdata.Arc.AngleOverflow;
    finput.Arc.Hollowness = gdata.Arc.Hollowness;

#    ifndef ONYX_PASS_SHADOW
    finput.Fade = f32v2(gdata.Fade.InnerFade, gdata.Fade.OuterFade);
#    endif

#endif

#ifdef ONYX_PASS_FILL
    finput.FragColor = unpackUnorm4x8ToFloat(data.FillColor);
#elif defined(ONYX_PASS_STENCIL)
    finput.FragColor = unpackUnorm4x8ToFloat(data.OutlineColor);
#endif

    return finput;
}

[[vk::binding(ONYX_SAMPLERS_BINDING, 0)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING, 0)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

#ifdef ONYX_USES_MATERIAL
[[vk::binding(ONYX_MATERIALS_BINDING, 0)]]
MaterialBuffer g_Materials;
#endif

#ifdef ONYX_PASS_FILL
[[vk::binding(ONYX_PLIGHTS_BINDING, 0)]]
PointLightBuffer g_PointLights;

#    ifdef ONYX_DIMENSION_3D

[[vk::binding(ONYX_DLIGHTS_BINDING, 0)]]
DirectionalLights g_DirLights;

[[vk::binding(ONYX_POINT_MAPS_BINDING, 0)]]
TextureCube g_PointMaps[ONYX_MAX_TEXTURE_MAPS];

[[vk::binding(ONYX_DIRECTIONAL_MAPS_BINDING, 0)]]
Texture2DArray g_DirectionalMaps[ONYX_MAX_TEXTURE_MAPS];

#    else

[[vk::binding(ONYX_SHADOW_MAPS_BINDING, 0)]]
Texture1D g_ShadowMaps[ONYX_MAX_TEXTURE_MAPS];

#    endif

[[vk::binding(ONYX_SHADOW_SAMPLER_BINDING, 0)]]
ShadowSamplerResource g_ShadowSampler;

#endif


[shader("fragment")]
#if defined(ONYX_DIMENSION_3D) && defined(ONYX_PASS_FILL)

FragOutput mainFS(const FragInput input, const bool isFrontFacing : SV_IsFrontFace)

#else

FragOutput mainFS(const FragInput input) FragSuffix

#endif

{

#ifdef ONYX_GEOMETRY_PARAMETRIC

if (!CheckParametricFragment(input.LocalPosition, input.ShapeInfo))
    discard;

#endif

#ifdef ONYX_GEOMETRY_CIRCLE
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

#    ifndef ONYX_PASS_SHADOW
    const f32 alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
#    endif

#elif defined(ONYX_GEOMETRY_GLYPH)
    const f32 alpha = ComputeMTSDFCoverage(g_Samplers, g_Textures, input.SamplerId, input.AtlasId, input.AtlasCoords);
    if (alpha <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;


#elif !defined(ONYX_PASS_SHADOW)
    const f32 alpha = 1.f;
#endif

#ifdef ONYX_PASS_FILL

    ResourceTable resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    resources.ShadowSampler = g_ShadowSampler;

#    ifdef ONYX_DIMENSION_3D

    resources.DirLights = g_DirLights;
    resources.PointMaps = g_PointMaps;
    resources.DirectionalMaps = g_DirectionalMaps;
    return ComputeMaterialColor3D(input.FragColor, g_PushData.Light, input.MatId, input.WorldPosition, input.TexCoords, resources, alpha, input.TBN, isFrontFacing);

#    else

    resources.ShadowMaps = g_ShadowMaps;
    return ComputeMaterialColor2D(input.FragColor, g_PushData.Light, input.MatId, input.WorldPosition, input.TexCoords, resources, alpha);

#    endif

#elif defined(ONYX_PASS_SHADOW)

#    ifdef ONYX_DIMENSION_2D

    if (input.MatId == NullAsset || g_Materials[input.MatId].Occluder == 0)
        discard;

    return f32v4(1.f);

#    else

    if (g_PushData.Far > 0.f)
        return distance(input.WorldPosition, g_PushData.LightPos) / g_PushData.Far + g_PushData.DepthBias;

    return input.Position.z + g_PushData.DepthBias;

#    endif

#elif defined(ONYX_GEOMETRY_CIRCLE) || defined(ONYX_GEOMETRY_GLYPH)
    return f32v4(input.FragColor.rgb, input.FragColor.a * alpha);
#else
    return input.FragColor;
#endif
}


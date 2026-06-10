#include "qol.slang"

// 2D shadows uses materials to check if they are an occluder or not
#if defined(ONYX_PASS_SHADED) || (defined(ONYX_PASS_SHADOW) && defined(ONYX_DIMENSION_2D))
#    define ONYX_USES_MATERIALS
#endif

#if !defined(ONYX_GEOMETRY_CIRCLE) && !defined(ONYX_GEOMETRY_GLYPH) && !defined(ONYX_GEOMETRY_DYNAMIC)
#    define ONYX_USES_BOUNDS
#endif

#if defined(ONYX_DIMENSION_2D) || defined(ONYX_GEOMETRY_CIRCLE)
#    define ONYX_VEC4(v2) f32v4((v2), 0.0, 1.0)
#else
#    define ONYX_VEC4(v3) f32v4((v3), 1.0)
#endif

#if defined(ONYX_GEOMETRY_DYNAMIC) && !defined(ONYX_PASS_SHADOW)
#    define ONYX_USES_VERTEX_COLOR
#endif

#ifdef ONYX_DIMENSION_2D
#    define ONYX_VEC(v4) (v4).xy
#else
#    define ONYX_VEC(v4) (v4).xyz
#endif

#ifdef ONYX_USES_MATERIALS
#    include "material.slang"
#endif

#ifdef ONYX_DIMENSION_2D

#    define LightData LightData2D
#    define BoundsArray Bounds2D
#    define InstanceData InstanceData2D
#    define MaterialBuffer Materials2D
#    define PointLightBuffer PointLights2D
#    define DirectionalLightBuffer DirectionalLights2D
#    define ShadowSamplerResource SamplerState
#    define ResourceTable ResourceTable2D
#    define WorldRect WorldRect2D
#    define f32v f32v2

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
#    elif defined(ONYX_GEOMETRY_DYNAMIC)
#        include "dynamic.slang"
#        define Instances DynamicInstances2D
#        define GeometryInstanceData DynamicInstanceData2D
#    endif

#else

#    define LightData LightData3D
#    define BoundsArray Bounds3D
#    define InstanceData InstanceData3D
#    define MaterialBuffer Materials3D
#    define PointLightBuffer PointLights3D
#    define DirectionalLightBuffer DirectionalLights3D
#    define ShadowSamplerResource SamplerComparisonState
#    define ResourceTable ResourceTable3D
#    define WorldRect WorldRect3D
#    define f32v f32v3

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
#    elif defined(ONYX_GEOMETRY_DYNAMIC)
#        include "dynamic.slang"
#        define Instances DynamicInstances3D
#        define GeometryInstanceData DynamicInstanceData3D
#    endif

#endif

#ifndef ONYX_GEOMETRY_CIRCLE
struct VertexInput
{
    f32v Position : POSITION;

#    ifdef ONYX_GEOMETRY_GLYPH
    f32v2 AtlasCoords : TEXCOORD0;
#    endif

#    ifndef ONYX_PASS_SHADOW
    f32v2 TexCoord : TEXCOORD0;
#    endif

#    if defined(ONYX_DIMENSION_3D) && !defined(ONYX_GEOMETRY_GLYPH)
#        if defined(ONYX_GEOMETRY_PARAMETRIC) || defined(ONYX_PASS_SHADED)
    f32v3 Normal : NORMAL;
#        endif

#        ifdef ONYX_PASS_SHADED
    f32v4 Tangent : TANGENT;
#        endif
#    endif

#    ifdef ONYX_USES_VERTEX_COLOR
    u32 Color : COLOR0;
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

#ifndef ONYX_PASS_SHADOW
    [[vk::location(1)]]
    f32v2 TexCoord;
#endif

    [[vk::location(2)]]
    f32v WorldPosition;

    [[vk::location(3)]]
    nointerpolation u32 InstanceIndex;

#ifdef ONYX_GEOMETRY_GLYPH
    [[vk::location(4)]]
    f32v2 AtlasCoords;
#endif

#ifdef ONYX_GEOMETRY_PARAMETRIC
    [[vk::location(5)]]
    nointerpolation u32v2 ShapeInfo;
#endif

#ifdef ONYX_USES_VERTEX_COLOR
    [[vk::location(6)]]
    f32v4 VertexColor;
#endif

#if defined(ONYX_DIMENSION_3D) && defined(ONYX_PASS_SHADED)
    [[vk::location(7)]]
    f32m3 TBN;
#endif

    f32v4 Position : SV_Position;
}

#ifndef ONYX_PASS_SHADOW
struct FragOpaqueOutput
{
    f32v4 Fill : SV_Target0;
    f32v4 Outline : SV_Target1;
}
struct FragTransparentOutput
{
    f32v4 Fill : SV_Target0;
    f32v4 Outline : SV_Target1;
    f32 Revealage : SV_Target2;
}
#    define FragSuffix
#elif defined(ONYX_DIMENSION_3D)
#    define FragOpaqueOutput f32
#    define FragSuffix : SV_Depth
#else
#    define FragOpaqueOutput f32
#    define FragSuffix
#endif


struct PushConstants
{
    f32m4 ProjectionView;
#ifdef ONYX_PASS_SHADED
    LightData Light;
#elif defined(ONYX_PASS_SHADOW) && defined(ONYX_DIMENSION_3D)
    f32v3 LightPos;
    f32 Far;
    f32 DepthBias;
#endif
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_INSTANCES_BINDING_POINT)]]
Instances g_Instances;

#ifdef ONYX_USES_BOUNDS

[[vk::binding(ONYX_BOUNDS_BINDING_POINT)]]
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
#ifdef ONYX_GEOMETRY_DYNAMIC
    const InstanceData data = gdata;
#else
    const InstanceData data = gdata.Data;
#endif

    const f32m4 transform = ConstructTransform(data.Transform);

#ifdef ONYX_GEOMETRY_CIRCLE

    const u32 vertexIndex = vertexId + baseVertex;
    const f32v2 alignment = ComputeCircleAlignment(gdata.Alignment);
    const f32v2 localPos = g_Positions[vertexIndex];

#else
#    ifdef ONYX_USES_BOUNDS
    const f32v alignment = ComputeAlignment(gdata.Alignment, gdata.BoundsId, g_Bounds);
#    else

    // NOTE(Isma): Maybe avoid this zero sub?

    const f32v alignment = f32v(0.f);
#    endif
    const f32v localPos = input.Position;
#endif

#ifdef ONYX_GEOMETRY_PARAMETRIC

    const InstanceParameters params = gdata.Parameters;
#    ifdef ONYX_DIMENSION_2D
    const f32v2 pos = GetParametricPosition(localPos, params, input.Region, alignment);
#    else
    const f32v3 pos = GetParametricPosition(localPos, input.Normal, params, input.Region, alignment);
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
#elif defined(ONYX_PASS_SHADED)
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

#ifndef ONYX_PASS_SHADOW
#    ifdef ONYX_GEOMETRY_CIRCLE
    finput.TexCoord = g_TexCoord[vertexIndex];
#    else
    finput.TexCoord = input.TexCoord;
#    endif
#endif

#ifdef ONYX_GEOMETRY_GLYPH
    finput.AtlasCoords = input.AtlasCoords;
#endif

#if defined(ONYX_GEOMETRY_CIRCLE) || defined(ONYX_GEOMETRY_PARAMETRIC)
    finput.LocalPosition = localPos;
#endif

#ifdef ONYX_GEOMETRY_PARAMETRIC
    finput.ShapeInfo = u32v2(params.Shape, input.Region);
#endif

#ifdef ONYX_USES_VERTEX_COLOR
    finput.VertexColor = unpackUnorm4x8ToFloat(input.Color);
#endif

    finput.WorldPosition = ONYX_VEC(worldPos);
    finput.InstanceIndex = instanceIndex;

    return finput;
}

[[vk::binding(ONYX_SAMPLERS_BINDING_POINT)]]
SamplerState g_Samplers[ONYX_MAX_SAMPLERS];

[[vk::binding(ONYX_TEXTURES_BINDING_POINT)]]
Texture2D g_Textures[ONYX_MAX_TEXTURES];

#ifdef ONYX_USES_MATERIALS
[[vk::binding(ONYX_MATERIALS_BINDING_POINT)]]
MaterialBuffer g_Materials;
#endif

#ifdef ONYX_PASS_SHADED
[[vk::binding(ONYX_POINT_LIGHTS_BINDING_POINT)]]
PointLightBuffer g_PointLights;

[[vk::binding(ONYX_DIRECTIONAL_LIGHTS_BINDING_POINT)]]
DirectionalLightBuffer g_DirLights;

#    ifdef ONYX_DIMENSION_3D

[[vk::binding(ONYX_SPOT_LIGHTS_BINDING_POINT)]]
SpotLights g_SpotLights;

[[vk::binding(ONYX_POINT_MAPS_BINDING_POINT)]]
TextureCube<f32> g_PointMaps[ONYX_MAX_TEXTURE_MAPS];

[[vk::binding(ONYX_DIRECTIONAL_MAPS_BINDING_POINT)]]
Texture2DArray<f32> g_DirectionalMaps[ONYX_MAX_TEXTURE_MAPS];

[[vk::binding(ONYX_SPOT_MAPS_BINDING_POINT)]]
Texture2D<f32> g_SpotMaps[ONYX_MAX_TEXTURE_MAPS];


#    else

[[vk::binding(ONYX_SHADOW_MAPS_BINDING_POINT)]]
Texture1D<f32> g_ShadowMaps[ONYX_MAX_SHADOW_OCCLUSION_MAP_SIZE];

#    endif

[[vk::binding(ONYX_SHADOW_SAMPLER_BINDING_POINT)]]
SamplerState g_ShadowSampler;

[[vk::binding(ONYX_SHADOW_COMPARE_SAMPLER_BINDING_POINT)]]
SamplerComparisonState g_ShadowCompareSampler;

#endif

#if defined(ONYX_DIMENSION_3D) && defined(ONYX_PASS_SHADED)
#    define ONYX_DECLARE_FRAGMENT_SIGNATURE(name, output) output name(const FragInput input, const bool isFrontFacing : SV_IsFrontFace)
#    define ONYX_FRAGMENT_PARAMETERS input, isFrontFacing
#else
#    define ONYX_DECLARE_FRAGMENT_SIGNATURE(name, output) output name(const FragInput input) FragSuffix
#    define ONYX_FRAGMENT_PARAMETERS input
#endif

ONYX_DECLARE_FRAGMENT_SIGNATURE(computeFragment, FragOpaqueOutput)
{
    const GeometryInstanceData gdata = g_Instances[input.InstanceIndex];
#ifdef ONYX_GEOMETRY_DYNAMIC
    const InstanceData data = gdata;
#else
    const InstanceData data = gdata.Data;
#endif

    const WorldRect rect = data.Rect;
    if (!CheckWorldClip(input.WorldPosition, rect))
        discard;

#ifdef ONYX_GEOMETRY_PARAMETRIC

    if (!CheckParametricFragment(input.LocalPosition, input.ShapeInfo))
        discard;

#endif
#ifndef ONYX_PASS_SHADOW
#    ifdef ONYX_USES_VERTEX_COLOR
    const f32v4 fill = unpackUnorm4x8ToFloat(data.FillColor) * input.VertexColor;
#    else
    const f32v4 fill = unpackUnorm4x8ToFloat(data.FillColor);
#    endif
    const f32v4 outline = unpackUnorm4x8ToFloat(data.OutlineColor);
    const f32 outlineWidth = data.OutlineWidth;
#endif

    // in the shadow case this could conflict with flat render passes (as mtid would be the sampler/tex id). however, shadow invocations are filtered by shaded passes so we are fine
    const u32 mtid = data.MatOrTexId;

#ifdef ONYX_GEOMETRY_CIRCLE
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, gdata.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, gdata.Arc)) discard;

    #ifndef ONYX_PASS_SHADOW
        const f32 alpha = CircleFade(fill.a, bounds, gdata.Fade);
    #endif

#elif defined(ONYX_GEOMETRY_GLYPH)
    const f32 alpha = ComputeMTSDFCoverage(g_Samplers, g_Textures, gdata.SamplerId, gdata.AtlasId, gdata.UnitRange, input.AtlasCoords);
    if (alpha <= 0.01f) // so that transparent parts do not touch depth/stencil buffers
        discard;

#elif defined(ONYX_PASS_SHADED)
    const f32 alpha = 1.f;
#endif

#ifndef ONYX_PASS_SHADOW
    FragOpaqueOutput out;
    out.Outline = f32v4(outline.rgb, outlineWidth);
#endif

#ifdef ONYX_PASS_SHADED
    ResourceTable resources;
    resources.Materials = g_Materials;
    resources.Samplers = g_Samplers;
    resources.Textures = g_Textures;
    resources.PointLights = g_PointLights;
    resources.DirLights = g_DirLights;
    resources.ShadowSampler = g_ShadowSampler;
    resources.ShadowCompareSampler = g_ShadowCompareSampler;

#    ifdef ONYX_DIMENSION_3D

    resources.SpLights = g_SpotLights;
    resources.PointMaps = g_PointMaps;
    resources.DirectionalMaps = g_DirectionalMaps;
    resources.SpotMaps = g_SpotMaps;

    out.Fill = ComputeMaterialColor(fill, g_PushData.Light, mtid, input.WorldPosition, input.TexCoord, resources, alpha, input.TBN, isFrontFacing);
    return out;

#    else

    resources.ShadowMaps = g_ShadowMaps;
    out.Fill = ComputeMaterialColor(fill, g_PushData.Light, mtid, input.WorldPosition, input.TexCoord, resources, alpha);
    return out;

#    endif

#elif defined(ONYX_PASS_SHADOW)

#    ifdef ONYX_DIMENSION_2D

    if (mtid == NullResource || g_Materials[mtid].Occluder == 0)
        discard;

    return 1.f;

#    else

    if (g_PushData.Far > 0.f)
        return distance(input.WorldPosition, g_PushData.LightPos) / g_PushData.Far + g_PushData.DepthBias;

    return input.Position.z + g_PushData.DepthBias;

#    endif
#else
#    if defined(ONYX_GEOMETRY_CIRCLE) || defined(ONYX_GEOMETRY_GLYPH)
    out.Fill = ComputeFlatColor(g_Samplers, g_Textures, mtid, f32v4(fill.rgb, alpha), input.TexCoord);
#    else
    out.Fill = ComputeFlatColor(g_Samplers, g_Textures, mtid, fill, input.TexCoord);
#    endif
    return out;
#endif
}

[shader("fragment")]
ONYX_DECLARE_FRAGMENT_SIGNATURE(mainFSO, FragOpaqueOutput)
{
    return computeFragment(ONYX_FRAGMENT_PARAMETERS);
}

#ifndef ONYX_PASS_SHADOW
[shader("fragment")]
ONYX_DECLARE_FRAGMENT_SIGNATURE(mainFST, FragTransparentOutput)
{
    const FragOpaqueOutput opaque = computeFragment(ONYX_FRAGMENT_PARAMETERS);
    FragTransparentOutput out;
    const f32 a = opaque.Fill.a;
    const f32 z = input.Position.z;
    const f32 w = clamp(pow(min(1.0, a * 10.0) + 0.01, 3.0) * 1e8 *
                         pow(1.0 - z * 0.9, 3.0), 1e-2, 3e3);

    out.Fill = w * f32v4(opaque.Fill.rgb * a, a);
    out.Outline = opaque.Outline;
    out.Revealage = a;
    return out;
}
#endif


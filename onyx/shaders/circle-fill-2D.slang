#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    f32v2 WorldPosition;
    [[vk::location(3)]]
    nointerpolation u32 MatHandle;
    [[vk::location(4)]]
    nointerpolation ArcData Arc;
    [[vk::location(7)]]
    nointerpolation f32v2 Fade;
    [[vk::location(8)]]
    nointerpolation f32v4 FragColor;

    f32v4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceDataFill2D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    LightData2D Light;
};

[[vk::push_constant]]
PushConstants g_PushData;

static const f32v2 g_Positions[6] =
{
    f32v2(-0.5, -0.5),
    f32v2( 0.5,  0.5),
    f32v2(-0.5,  0.5),
    f32v2(-0.5, -0.5),
    f32v2( 0.5, -0.5),
    f32v2( 0.5,  0.5)
};

static const f32v2 g_TexCoords[6] =
{
    f32v2(0.0, 1.0),
    f32v2(1.0, 0.0),
    f32v2(0.0, 0.0),
    f32v2(0.0, 1.0),
    f32v2(1.0, 1.0),
    f32v2(1.0, 0.0)
};

[shader("vertex")]
VertexOutput mainVS(
    const u32 vertexId : SV_VertexID,
    const u32 baseVertex : SV_StartVertexLocation,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 vertexIndex = vertexId + baseVertex;
    const u32 instanceIndex = instanceId + baseInstance;

    const CircleInstanceDataFill2D data = g_Instances[instanceIndex];
    const f32m4 transform = ComputeTransform2D(data.Data.Basis);

    const f32v2 localPos = g_Positions[vertexIndex];
    const f32v4 worldPos = mul(f32v4(localPos, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.TexCoord = g_TexCoords[vertexIndex];

    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xy;
    output.MatHandle = data.Data.MatHandle;

    output.Arc.CosSin = f32v4(data.Arc.LowerCos, data.Arc.LowerSin, data.Arc.UpperCos, data.Arc.UpperSin);
    output.Arc.AngleOverflow = data.Arc.AngleOverflow;
    output.Arc.Hollowness = data.Arc.Hollowness;

    output.Fade = f32v2(data.Fade.InnerFade, data.Fade.OuterFade);
    output.FragColor = unpackUnorm4x8ToFloat(data.Data.Color);

    return output;
}

[[vk::binding(1, 0)]]
Materials2D g_Materials[255];

[[vk::binding(2, 0)]]
SamplerState g_Samplers[8];

[[vk::binding(3, 0)]]
Texture2D g_Textures[1024];

[[vk::binding(4, 0)]]
PointLights2D g_PointLights;

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;


    const f32v3 lcolor = GetLightColor2D(g_PushData.Light, input.WorldPosition, g_PointLights);
    const f32 alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
    if (input.MatHandle != u32(-1))
    {
        const u32 poolIdx = GetMaterialPoolIndex(input.MatHandle);
        const u32 matIdx = GetMaterialIndex(input.MatHandle);

        const Material2D mat = g_Materials[poolIdx][matIdx];
        if (mat.Texture == u32(-1) || mat.Sampler == u32(-1))
            return input.FragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);

        const f32v4 tex = g_Textures[mat.Texture].Sample(g_Samplers[mat.Sampler], input.TexCoord);
        return tex * input.FragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, alpha);
    }
    return input.FragColor * f32v4(lcolor, alpha);
}

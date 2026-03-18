#include "utils.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    f32v2 TexCoord : TEXCOORD0;
};

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 WorldPosition;
    [[vk::location(1)]]
    f32v2 TexCoord;
    [[vk::location(2)]]
    nointerpolation u32 MatHandle;
    [[vk::location(3)]]
    nointerpolation f32v4 FragColor;

    f32v4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<InstanceDataFill2D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    LightData2D Light;
};

[[vk::push_constant]]
PushConstants g_PushData;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const InstanceDataFill2D data = g_Instances[instanceIndex];

    const f32m4 transform = ComputeTransform2D(data.Basis);
    const f32v4 worldPos = mul(f32v4(input.Position, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.TexCoord = input.TexCoord;

    output.WorldPosition = worldPos.xy;
    output.MatHandle = data.MatHandle;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);

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
    const f32v3 lcolor = GetLightColor2D(g_PushData.Light, input.WorldPosition, g_PointLights);
    if (input.MatHandle != u32(-1))
    {
        const u32 poolIdx = GetMaterialPoolIndex(input.MatHandle);
        const u32 matIdx = GetMaterialIndex(input.MatHandle);

        const Material2D mat = g_Materials[poolIdx][matIdx];

        if (mat.Texture == u32(-1) || mat.Sampler == u32(-1))
            return input.FragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, 1.f);

        const f32v4 tex = g_Textures[mat.Texture].Sample(g_Samplers[mat.Sampler], input.TexCoord);
        return tex * input.FragColor * unpackUnorm4x8ToFloat(mat.ColorFactor) * f32v4(lcolor, 1.f);
    }
    return input.FragColor * f32v4(lcolor, 1.f);
}

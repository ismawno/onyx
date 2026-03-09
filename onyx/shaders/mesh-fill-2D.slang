#include "utils.slang"

struct VertexInput
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VertexOutput
{
    [[vk::location(0)]]
    float2 WorldPosition;
    [[vk::location(1)]]
    float2 TexCoord;
    [[vk::location(2)]]
    nointerpolation uint MatIndex;
    [[vk::location(3)]]
    nointerpolation float4 FragColor;

    float4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<InstanceDataFill2D, Std430DataLayout> g_Instances;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData2D Light;
};

[[vk::push_constant]]
PushConstants g_PushData;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const uint instanceId : SV_InstanceID,
    const uint baseInstance : SV_StartInstanceLocation)
{
    const uint instanceIndex = instanceId + baseInstance;

    const InstanceDataFill2D data = g_Instances[instanceIndex];

    const float4x4 transform = ComputeTransform2D(data.Basis);
    const float4 worldPos = mul(float4(input.Position, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.TexCoord = input.TexCoord;

    output.WorldPosition = worldPos.xy;
    output.MatIndex = data.MatIndex;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);

    return output;
}

[[vk::binding(1, 0)]]
Materials2D g_Materials;

[[vk::binding(2, 0)]]
SamplerState g_Samplers[8];

[[vk::binding(3, 0)]]
Texture2D g_Textures[1024];

[[vk::binding(4, 0)]]
PointLights2D g_PointLights;

[shader("fragment")]
float4 mainFS(const VertexOutput input)
{
    const float3 lcolor = GetLightColor2D(g_PushData.Light, input.WorldPosition, g_PointLights);
    if (input.MatIndex != uint(-1))
    {
        const Material2D mat = g_Materials[input.MatIndex];
        if (mat.Texture == uint(-1) || mat.Sampler == uint(-1))
            return input.FragColor * unpackUnorm4x8ToFloat(g_Materials[input.MatIndex].ColorFactor) * float4(lcolor, 1.f);

        const float4 tex = g_Textures[mat.Texture].Sample(g_Samplers[mat.Sampler], input.TexCoord);
        return tex * input.FragColor * unpackUnorm4x8ToFloat(g_Materials[input.MatIndex].ColorFactor) * float4(lcolor, 1.f);
    }
    return input.FragColor * float4(lcolor, 1.f);
}

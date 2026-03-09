#include "utils.slang"

struct VertexInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
};

struct VertexOutput
{
    [[vk::location(0)]]
    float3 WorldPosition;
    [[vk::location(1)]]
    float2 TexCoord;
    [[vk::location(2)]]
    nointerpolation uint MatIndex;
    [[vk::location(3)]]
    float4 FragColor;
    [[vk::location(4)]]
    float3x3 TBN;

    float4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<InstanceDataFill3D, Std430DataLayout> g_Instances;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData3D LightData;
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

    const InstanceDataFill3D data = g_Instances[instanceIndex];

    const float4x4 transform = ComputeTransform3D(data.Basis);
    const float4 worldPos = mul(float4(input.Position, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);

    const float3x3 normalMatrix = ComputeNormalMatrix(float3x3(transform));

    const float3 N = normalize(mul(input.Normal, normalMatrix));
    const float3 T = normalize(mul(input.Tangent.xyz, normalMatrix));

    const float3 B = cross(N, T) * input.Tangent.w;
    output.TBN = float3x3(T, B, N);

    output.WorldPosition = worldPos.xyz;
    output.TexCoord = input.TexCoord;
    output.MatIndex = data.MatIndex;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);

    return output;
}

[[vk::binding(1, 0)]]
Materials3D g_Materials;

[[vk::binding(2, 0)]]
SamplerState g_Samplers[8];

[[vk::binding(3, 0)]]
Texture2D g_Textures[1024];

[[vk::binding(4, 0)]]
PointLights3D g_PointLights;

[[vk::binding(5, 0)]]
DirectionalLights g_DirLights;


[shader("fragment")]
float4 mainFS(const VertexOutput input, const bool isFrontFacing : SV_IsFrontFace)
{
    if (input.MatIndex != uint(-1))
    {
        const Material3D mat = g_Materials[input.MatIndex];
        MaterialInfo3D info;
        info.Points = g_PointLights;
        info.Directionals = g_DirLights;
        info.Albedo = unpackUnorm4x8ToFloat(mat.AlbedoFactor);
        info.WorldPosition = input.WorldPosition;
        info.Emissive = float3(mat.EmissiveFactorX, mat.EmissiveFactorY, mat.EmissiveFactorZ);
        info.Roughness = mat.RoughnessFactor;
        info.Metallic = mat.MetallicFactor;
        info.Occlusion = 1.0;

        if (mat.Sampler == uint(-1))
        {
            info.Normal = input.TBN[2];
            if (!isFrontFacing)
                info.Normal = -info.Normal;
            const float3 lcolor = GetLightColor3D(g_PushData.LightData, info);
            return input.FragColor * float4(lcolor, 1.0);
        }

        const SamplerState samp = g_Samplers[mat.Sampler];

        // Albedo
        if (mat.AlbedoTex != uint(-1))
            info.Albedo *= g_Textures[mat.AlbedoTex].Sample(samp, input.TexCoord);

        // Normal map
        if (mat.NormalTex != uint(-1))
        {
            float3 sampledNormal = g_Textures[mat.NormalTex].Sample(samp, input.TexCoord).xyz;
            sampledNormal = sampledNormal * 2.0 - 1.0; // unpack from [0,1] to [-1,1]
            sampledNormal.xy *= mat.NormalScale;
            info.Normal = normalize(mul(sampledNormal, input.TBN)); // requires TBN matrix in VertexOutput
        } else
            info.Normal = input.TBN[2];

        if (!isFrontFacing)
            info.Normal = -info.Normal;

        // Metallic/Roughness (glTF convention: G = roughness, B = metallic)
        if (mat.MetallicRoughnessTex != uint(-1))
        {
            const float4 mr = g_Textures[mat.MetallicRoughnessTex].Sample(samp, input.TexCoord);
            info.Roughness *= mr.g;
            info.Metallic *= mr.b;
        }

        // Occlusion (R channel)
        if (mat.OcclusionTex != uint(-1))
        {
            const float sampledOcclusion = g_Textures[mat.OcclusionTex].Sample(samp, input.TexCoord).r;
            info.Occlusion = 1.0 + mat.OcclusionStrength * (sampledOcclusion - 1.0);
        }

        // Emissive
        if (mat.EmissiveTex != uint(-1))
            info.Emissive *= g_Textures[mat.EmissiveTex].Sample(samp, input.TexCoord).rgb;

        const float3 lcolor = GetLightColor3D(g_PushData.LightData, info);
        return input.FragColor * float4(lcolor, info.Albedo.w);
    }
    return input.FragColor;
}


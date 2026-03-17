#include "utils.slang"

struct VertexOutput
{
    [[vk::location(0)]]
    float2 LocalPosition;
    [[vk::location(1)]]
    float2 TexCoord;
    [[vk::location(2)]]
    float3 WorldPosition;
    [[vk::location(3)]]
    nointerpolation uint MatIndex;
    [[vk::location(4)]]
    nointerpolation ArcData Arc;
    [[vk::location(7)]]
    nointerpolation float2 Fade;
    [[vk::location(8)]]
    nointerpolation float4 FragColor;
    [[vk::location(9)]]
    float3x3 TBN;

    float4 Position : SV_Position;
};

[[vk::binding(0, 0)]]
StructuredBuffer<CircleInstanceDataFill3D, Std430DataLayout> g_InstanceBuffer;

struct PushConstants
{
    float4x4 ProjectionView;
    LightData3D LightData;
};

[[vk::push_constant]]
PushConstants g_PushData;


static const float2 g_Positions[6] =
{
    float2(-0.5, -0.5),
    float2( 0.5,  0.5),
    float2(-0.5,  0.5),
    float2(-0.5, -0.5),
    float2( 0.5, -0.5),
    float2( 0.5,  0.5)
};

static const float2 g_TexCoords[6] =
{
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(0.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 1.0),
    float2(1.0, 0.0)
};

[shader("vertex")]
VertexOutput mainVS(
    const uint vertexId : SV_VertexID,
    const uint baseVertex : SV_StartVertexLocation,
    const uint instanceId : SV_InstanceID,
    const uint baseInstance : SV_StartInstanceLocation)
{
    const uint vertexIndex = vertexId + baseVertex;
    const uint instanceIndex = instanceId + baseInstance;

    const CircleInstanceDataFill3D data = g_InstanceBuffer[instanceIndex];
    const float4x4 transform = ComputeTransform3D(data.Data.Basis);

    const float2 localPos = g_Positions[vertexIndex];
    const float4 worldPos = mul(float4(localPos, 0.0, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);
    output.TexCoord = g_TexCoords[vertexIndex];

    const float3x3 normalMatrix = ComputeNormalMatrix(float3x3(transform));
    const float3 N = normalize(normalMatrix[2]);
    const float3 T = normalize(normalMatrix[0]);
    const float3 B = cross(N, T);
    output.TBN = float3x3(T, B, N);

    output.LocalPosition = localPos;
    output.WorldPosition = worldPos.xyz;
    output.MatIndex = data.Data.MatIndex;

    output.Arc.CosSin = float4(data.Arc.LowerCos, data.Arc.LowerSin, data.Arc.UpperCos, data.Arc.UpperSin);
    output.Arc.AngleOverflow = data.Arc.AngleOverflow;
    output.Arc.Hollowness = data.Arc.Hollowness;

    output.Fade = float2(data.Fade.InnerFade, data.Fade.OuterFade);

    output.FragColor = unpackUnorm4x8ToFloat(data.Data.Color);

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
    const CircleBounds bounds = GetCircleBounds(input.LocalPosition, input.Arc.Hollowness);
    if (!CheckOutsideCircle(bounds, input.LocalPosition, input.Arc)) discard;

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

        // Albedo
        if (mat.AlbedoSamp != uint(-1) && mat.AlbedoTex != uint(-1))
            info.Albedo *= g_Textures[mat.AlbedoTex].Sample(g_Samplers[mat.AlbedoSamp], input.TexCoord);

        // Normal map
        if (mat.NormalSamp != uint(-1) && mat.NormalTex != uint(-1))
        {
            float3 sampledNormal = g_Textures[mat.NormalTex].Sample(g_Samplers[mat.NormalSamp], input.TexCoord).xyz;
            sampledNormal = sampledNormal * 2.0 - 1.0; // unpack from [0,1] to [-1,1]
            sampledNormal.xy *= mat.NormalScale;
            info.Normal = normalize(mul(sampledNormal, input.TBN)); // requires TBN matrix in VertexOutput
        } else
            info.Normal = input.TBN[2];

        if (!isFrontFacing)
            info.Normal = -info.Normal;

        // Metallic/Roughness (glTF convention: G = roughness, B = metallic)
        if (mat.MetallicRoughnessSamp != uint(-1) && mat.MetallicRoughnessTex != uint(-1))
        {
            const float4 mr = g_Textures[mat.MetallicRoughnessTex].Sample(g_Samplers[mat.MetallicRoughnessSamp], input.TexCoord);
            info.Roughness *= mr.g;
            info.Metallic *= mr.b;
        }

        // Occlusion (R channel)
        if (mat.OcclusionSamp != uint(-1) && mat.OcclusionTex != uint(-1))
        {
            const float sampledOcclusion = g_Textures[mat.OcclusionTex].Sample(g_Samplers[mat.OcclusionSamp], input.TexCoord).r;
            info.Occlusion = 1.0 + mat.OcclusionStrength * (sampledOcclusion - 1.0);
        }

        // Emissive
        if (mat.EmissiveSamp != uint(-1) && mat.EmissiveTex != uint(-1))
            info.Emissive *= g_Textures[mat.EmissiveTex].Sample(g_Samplers[mat.EmissiveSamp], input.TexCoord).rgb;

        const float3 lcolor = GetLightColor3D(g_PushData.LightData, info);
        const float alpha = CircleFade(input.FragColor.a, bounds, input.Fade);
        return input.FragColor * float4(lcolor, info.Albedo.w * alpha);
    }

    return input.FragColor;
}


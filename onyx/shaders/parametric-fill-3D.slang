#include "utils.slang"

struct VertexInput
{
    f32v3 Position : POSITION;
    f32v2 TexCoord : TEXCOORD0;
    f32v3 Normal : NORMAL;
    f32v4 Tangent : TANGENT;
    ParametricRegionFlags Region;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v3 WorldPosition;
    [[vk::location(1)]]
    f32v3 LocalPosition;
    [[vk::location(2)]]
    f32v2 TexCoord;
    [[vk::location(3)]]
    nointerpolation u32 MatHandle;
    [[vk::location(4)]]
    f32v4 FragColor;
    [[vk::location(5)]]
    nointerpolation u32v2 ShapeInfo;
    [[vk::location(6)]]
    f32m3 TBN;

    f32v4 Position : SV_Position;
}

[[vk::binding(0, 0)]]
StructuredBuffer<ParametricInstanceDataFill3D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
    LightData3D LightData;
}

[[vk::push_constant]]
PushConstants g_PushData;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput input,
    const u32 instanceId : SV_InstanceID,
    const u32 baseInstance : SV_StartInstanceLocation)
{
    const u32 instanceIndex = instanceId + baseInstance;

    const ParametricInstanceDataFill3D pdata = g_Instances[instanceIndex];
    const StaticInstanceDataFill3D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v3 pos = GetParametricPosition3D(input.Position, params, input.Region);

    const f32m4 transform = ComputeTransform3D(data.Basis);
    const f32v4 worldPos = mul(f32v4(pos, 1.0), transform);

    VertexOutput output;
    output.Position = mul(worldPos, g_PushData.ProjectionView);

    const f32m3 normalMatrix = ComputeNormalMatrix(f32m3(transform));

    const f32v3 N = normalize(mul(input.Normal, normalMatrix));
    const f32v3 T = normalize(mul(input.Tangent.xyz, normalMatrix));

    const f32v3 B = cross(N, T) * input.Tangent.w;
    output.TBN = f32m3(T, B, N);

    output.WorldPosition = worldPos.xyz;
    output.LocalPosition = input.Position;
    output.ShapeInfo = u32v2(params.Shape, input.Region);
    output.TexCoord = input.TexCoord;
    output.MatHandle = data.MatHandle;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);

    return output;
}

[[vk::binding(1, 0)]]
Materials3D g_Materials[255];

[[vk::binding(2, 0)]]
SamplerState g_Samplers[8];

[[vk::binding(3, 0)]]
Texture2D g_Textures[1024];

[[vk::binding(4, 0)]]
PointLights3D g_PointLights;

[[vk::binding(5, 0)]]
DirectionalLights g_DirLights;

[shader("fragment")]
f32v4 mainFS(const VertexOutput input, const bool isFrontFacing : SV_IsFrontFace)
{
    if (!CheckParametricFragment3D(input.LocalPosition, input.ShapeInfo))
        discard;

    if (input.MatHandle != u32(-1))
    {
        const u32 poolIdx = GetMaterialPoolIndex(input.MatHandle);
        const u32 matIdx = GetMaterialIndex(input.MatHandle);

        const Material3D mat = g_Materials[poolIdx][matIdx];
        MaterialInfo3D info;
        info.Points = g_PointLights;
        info.Directionals = g_DirLights;
        info.Albedo = unpackUnorm4x8ToFloat(mat.AlbedoFactor);
        info.WorldPosition = input.WorldPosition;
        info.Emissive = f32v3(mat.EmissiveFactorX, mat.EmissiveFactorY, mat.EmissiveFactorZ);
        info.Roughness = mat.RoughnessFactor;
        info.Metallic = mat.MetallicFactor;
        info.Occlusion = 1.0;

        // Albedo
        if (mat.AlbedoSamp != u32(-1) && mat.AlbedoTex != u32(-1))
            info.Albedo *= g_Textures[mat.AlbedoTex].Sample(g_Samplers[mat.AlbedoSamp], input.TexCoord);

        // Normal map
        if (mat.NormalSamp != u32(-1) && mat.NormalTex != u32(-1))
        {
            f32v3 sampledNormal = g_Textures[mat.NormalTex].Sample(g_Samplers[mat.NormalSamp], input.TexCoord).xyz;
            sampledNormal = sampledNormal * 2.0 - 1.0; // unpack from [0,1] to [-1,1]
            sampledNormal.xy *= mat.NormalScale;
            info.Normal = normalize(mul(sampledNormal, input.TBN)); // requires TBN matrix in VertexOutput
        } else
            info.Normal = input.TBN[2];

        if (!isFrontFacing)
            info.Normal = -info.Normal;

        // Metallic/Roughness (glTF convention: G = roughness, B = metallic)
        if (mat.MetallicRoughnessSamp != u32(-1) && mat.MetallicRoughnessTex != u32(-1))
        {
            const f32v4 mr = g_Textures[mat.MetallicRoughnessTex].Sample(g_Samplers[mat.MetallicRoughnessSamp], input.TexCoord);
            info.Roughness *= mr.g;
            info.Metallic *= mr.b;
        }

        // Occlusion (R channel)
        if (mat.OcclusionSamp != u32(-1) && mat.OcclusionTex != u32(-1))
        {
            const f32 sampledOcclusion = g_Textures[mat.OcclusionTex].Sample(g_Samplers[mat.OcclusionSamp], input.TexCoord).r;
            info.Occlusion = 1.0 + mat.OcclusionStrength * (sampledOcclusion - 1.0);
        }

        // Emissive
        if (mat.EmissiveSamp != u32(-1) && mat.EmissiveTex != u32(-1))
            info.Emissive *= g_Textures[mat.EmissiveTex].Sample(g_Samplers[mat.EmissiveSamp], input.TexCoord).rgb;

        const f32v3 lcolor = GetLightColor3D(g_PushData.LightData, info);
        return input.FragColor * f32v4(lcolor, info.Albedo.w);
    }
    return input.FragColor;
}


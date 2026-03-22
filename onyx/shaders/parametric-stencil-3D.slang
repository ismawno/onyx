#include "utils.slang"

struct VertexInput
{
    f32v3 Position : POSITION;
    u32 Region;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v3 LocalPosition;
    [[vk::location(1)]]
    nointerpolation f32v4 FragColor;
    [[vk::location(2)]]
    nointerpolation u32v2 ShapeInfo;

    f32v4 Position : SV_Position;
}

[[vk::binding(0, 0)]]
StructuredBuffer<ParametricInstanceDataStencil3D, Std430DataLayout> g_Instances;

struct PushConstants
{
    f32m4 ProjectionView;
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

    const ParametricInstanceDataStencil3D pdata = g_Instances[instanceIndex];
    const StaticInstanceDataStencil3D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v3 pos = GetParametricPosition3D(input.Position, params, input.Region);

    const f32m4 transform = ComputeTransform3D(data.Basis);

    const f32v3 scale = ComputeStencilScale3D(transform);
    const f32v3 extruded = pos + (normalize(input.Position) * data.OutlineWidth) / scale;

    VertexOutput output;

    output.Position = mul(mul(f32v4(extruded, 1.0), transform), g_PushData.ProjectionView);
    output.LocalPosition = input.Position;
    output.ShapeInfo = u32v2(params.Shape, input.Region);

    output.FragColor = unpackUnorm4x8ToFloat(data.Color);
    return output;
}

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    if (!CheckParametricFragment3D(input.LocalPosition, input.ShapeInfo))
        discard;

    return input.FragColor;
}

#include "utils.slang"

struct VertexInput
{
    f32v2 Position : POSITION;
    u32 Region;
}

struct VertexOutput
{
    [[vk::location(0)]]
    f32v2 LocalPosition;
    [[vk::location(1)]]
    nointerpolation f32v4 FragColor;
    [[vk::location(2)]]
    nointerpolation u32v2 ShapeInfo;

    f32v4 Position : SV_Position;
}

[[vk::binding(0, 0)]]
StructuredBuffer<ParametricInstanceDataStencil2D, Std430DataLayout> g_Instances;

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

    const ParametricInstanceDataStencil2D pdata = g_Instances[instanceIndex];
    const StaticInstanceDataStencil2D data = pdata.Data;
    const InstanceParameters params = pdata.Parameters;

    const f32v2 pos = GetParametricPosition2D(input.Position, params, input.Region);

    const f32m4 transform = ComputeTransform2D(data.Basis);

    const f32v2 scale = ComputeStencilScale2D(transform);
    const f32v2 extruded = pos + (normalize(input.Position) * data.OutlineWidth) / scale;

    VertexOutput output;
    output.Position = mul(mul(f32v4(extruded, 0.0, 1.0), transform), g_PushData.ProjectionView);
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);
    output.LocalPosition = input.Position;
    output.ShapeInfo = u32v2(params.Shape, input.Region);

    return output;
}

[shader("fragment")]
f32v4 mainFS(const VertexOutput input)
{
    if (!CheckParametricFragment2D(input.LocalPosition, input.ShapeInfo))
        discard;

    return input.FragColor;
}

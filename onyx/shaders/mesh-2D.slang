#include "utils.slang"

struct VertexInput
{
    float2 Position : POSITION;
};

struct VertexOutput
{
    [[vk::location(0)]]
    nointerpolation float4 FragColor;

    float4 Position : SV_Position;
    float  PointSize : SV_PointSize;
};

[[vk::binding(0, 0)]]
StructuredBuffer<InstanceData2D, Std430DataLayout> instanceBuffer;

struct PushConstants
{
    float4x4 ProjectionView;
};

[[vk::push_constant]]
PushConstants pdata;

[shader("vertex")]
VertexOutput mainVS(
    const VertexInput p_Input,
    const uint instanceId : SV_InstanceID,
    const uint baseInstance : SV_StartInstanceLocation)
{
    const uint instanceIndex = instanceId + baseInstance;

    const InstanceData2D data = instanceBuffer[instanceIndex];

    const float4x4 transform = ComputeTransform2D(data.Basis);
    VertexOutput output;
    output.Position =  mul(mul(transform, float4(p_Input.Position, 0.0, 1.0)), pdata.ProjectionView);
    output.PointSize = 1.0;
    output.FragColor = unpackUnorm4x8ToFloat(data.Color);
    return output;
}

[shader("fragment")]
float4 mainFS(const VertexOutput p_Input)
{
    return p_Input.FragColor;
}

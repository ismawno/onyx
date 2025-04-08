#version 460

layout(location = 0) in vec2 i_Position;
layout(location = 0) out flat vec4 o_FragColor;

struct InstanceData
{
    mat4 Transform;
    vec4 Color;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceBuffer
{
    InstanceData Instances[];
}
instanceBuffer;

layout(push_constant) uniform ProjectionViewData
{
    mat4 ProjectionView;
}
projectionView;

void main()
{
    gl_Position = projectionView.ProjectionView * instanceBuffer.Instances[gl_InstanceIndex].Transform *
                  vec4(i_Position, 0.0, 1.0);
    gl_PointSize = 1.0;
    o_FragColor = instanceBuffer.Instances[gl_InstanceIndex].Color;
}
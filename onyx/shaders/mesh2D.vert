#version 460

layout(location = 0) in vec2 i_Position;
layout(location = 0) out vec4 o_FragColor;

struct ObjectData
{
    mat4 Transform;
    vec4 Color;
};

layout(set = 0, binding = 0) readonly buffer ObjectBuffer
{
    ObjectData Objects[];
}
objectBuffer;

void main()
{
    gl_Position = objectBuffer.Objects[gl_InstanceIndex].Transform * vec4(i_Position, 0.0, 1.0);
    gl_PointSize = 1.0;
    o_FragColor = objectBuffer.Objects[gl_InstanceIndex].Color;
}
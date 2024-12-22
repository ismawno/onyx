#version 460

layout(location = 0) out vec2 o_FragUV;

void main()
{
    // A full-screen triangle can be made from these three vertices:
    // gl_VertexIndex = 0 -> (-1, -1)
    // gl_VertexIndex = 1 -> ( 3, -1)
    // gl_VertexIndex = 2 -> (-1,  3)
    const vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

    // UV coordinates can simply map the entire image from [0,0] to [1,1]
    // We'll map the triangle vertices to cover the [0,0]-[1,1] UV space:
    const vec2 uvs[3] = vec2[](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    o_FragUV = uvs[gl_VertexIndex];
}
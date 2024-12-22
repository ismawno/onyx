#version 460

void main()
{
    // A full-screen triangle can be made from these three vertices:
    // gl_VertexIndex = 0 -> (-1, -1)
    // gl_VertexIndex = 1 -> ( 3, -1)
    // gl_VertexIndex = 2 -> (-1,  3)
    const vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}

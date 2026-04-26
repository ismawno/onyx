#include "qol.slang"

static const f32v2 g_Positions[6] =
{
    f32v2(-1.0, -1.0),
    f32v2( 1.0, -1.0),
    f32v2(-1.0,  1.0),
    f32v2( 1.0, -1.0),
    f32v2(-1.0,  1.0),
    f32v2( 1.0,  1.0),
};

struct FragInput
{
    [[vk::location(0)]]
    f32v2 TexCoord;
    f32v4 Position : SV_Position;
};

[shader("vertex")]
FragInput mainVS(const u32 vertexId : SV_VertexID)
{
    FragInput finput;
    const f32v2 pos = g_Positions[vertexId];
    finput.Position = f32v4(pos, 0.f, 1.f);
    finput.TexCoord = pos * 0.5f + 0.5f;

    return finput;
}

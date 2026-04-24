#include "qol.slang"

// TODO(Isma): switch vertex shaders with pp
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

struct PushConstants
{
    u32 AttachmentIndex;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_COMPOSITOR_COLOR_ATTACHMENTS_BINDING)]]
Sampler2D g_Attachments[ONYX_MAX_ATTACHMENTS];

[shader("fragment")]
f32v4 mainFS(const FragInput input)
{
    // TODO(Isma): Use raw load here
    return g_Attachments[g_PushData.AttachmentIndex].Sample(input.TexCoord);
}


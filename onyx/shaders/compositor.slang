#include "full-vertex.slang"

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
    return g_Attachments[g_PushData.AttachmentIndex].Sample(input.TexCoord);
}


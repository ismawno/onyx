#include "full-vertex.slang"

struct PushConstants
{
    u32 AttachmentIndex;
}

[[vk::push_constant]]
PushConstants g_PushData;

[[vk::binding(ONYX_BLEND_TRANSPARENT_ATTACHMENTS_BINDING)]]
Sampler2D g_Transparent[ONYX_MAX_ATTACHMENTS];

[[vk::binding(ONYX_BLEND_REVEALAGE_ATTACHMENTS_BINDING)]]
Sampler2D<f32> g_Revealage[ONYX_MAX_ATTACHMENTS];

[shader("fragment")]
f32v4 mainFS(const FragInput input)
{
    const u32 idx = g_PushData.AttachmentIndex;
    const f32v2 uv = input.TexCoord;
    const f32 revealage = g_Revealage[idx].Sample(uv);

    // NOTE(Isma): Subject to float precision errors
    if (revealage == 1.f)
        discard;

    const f32v4 accum = g_Transparent[idx].Sample(uv);

    // NOTE(Isma): Should also handle overflow
    const f32v3 avg = accum.rgb / max(accum.a, 1e-5f);
    return f32v4(avg, 1.f - revealage);
}

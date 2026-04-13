#include "qol.slang"

struct PushConstants
{
    u32 OcclusionMapIndex;
    u32 ShadowMapIndex;
    u32 OcclusionResolution;
    u32 ShadowResolution;
}

[[vk::push_constant]]
PushConstants g_Push;

[[vk::binding(ONYX_OCCLUSION_MAP_BINDING, 0)]]
RWTexture2D<f32> g_OcclusionMaps[ONYX_MAX_TEXTURE_MAPS];

[[vk::binding(ONYX_DISTANCE_MAP_BINDING, 0)]]
RWTexture1D<f32> g_ShadowMaps[ONYX_MAX_TEXTURE_MAPS];

[shader("compute")]
[numthreads(64, 1, 1)]
void main(const u32v3 id : SV_DispatchThreadID)
{
    if (id.x >= g_Push.ShadowResolution)
        return;

    const f32 angle = (f32(id.x) + 0.5) / f32(g_Push.ShadowResolution) * 2.0 * 3.14159265;
    const f32v2 dir = f32v2(cos(angle), sin(angle));

    const f32 center = f32(g_Push.OcclusionResolution) * 0.5;
    const f32 maxSteps = center;

    f32 dist = 1.0;
    for (f32 t = 0.0; t < maxSteps; t += 1.0)
    {
        const f32v2 pos = center + dir * t;
        const i32v2 texel = i32v2(pos);

        if (texel.x < 0 || texel.y < 0 ||
            texel.x >= i32(g_Push.OcclusionResolution) ||
            texel.y >= i32(g_Push.OcclusionResolution))
            break;

        if (g_OcclusionMaps[g_Push.OcclusionMapIndex][texel] > 0.0)
        {
            dist = t / maxSteps;
            break;
        }
    }

    g_ShadowMaps[g_Push.ShadowMapIndex][i32(id.x)] = dist;
}

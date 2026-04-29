#include "qol.slang"

struct PushConstants
{
    // Points
    f32 StartAngle;
    f32 EndAngle;

    u32 Mode;

    u32 OcclusionMapIndex;
    u32 OcclusionResolution;
    u32 ShadowMapIndex;
    u32 ShadowResolution;
    f32 DistanceBias;
}

[[vk::push_constant]]
PushConstants g_Push;

[[vk::binding(ONYX_OCCLUSION_MAP_BINDING_POINT)]]
RWTexture2D<f32> g_OcclusionMaps[ONYX_MAX_TEXTURE_MAPS];

[[vk::binding(ONYX_DISTANCE_MAP_BINDING_POINT)]]
RWTexture1D<f32> g_ShadowMaps[ONYX_MAX_TEXTURE_MAPS];

[shader("compute")]
[numthreads(64, 1, 1)]
void main(const u32v3 id : SV_DispatchThreadID)
{
    if (id.x >= g_Push.ShadowResolution)
        return;


    f32 maxSteps;
    f32v2 dir;
    f32v2 origin;
    if (g_Push.Mode == ONYX_MARCH_MODE_POINT_LIGHT)
    {
        const f32 tt = f32(id.x) / f32(g_Push.ShadowResolution);

        const f32 angle = lerp(g_Push.StartAngle, g_Push.EndAngle, tt);
        dir = f32v2(cos(angle), sin(angle));
        maxSteps = g_Push.OcclusionResolution * 0.5f;
        origin = f32v2(maxSteps);
    }
    else
    {
        dir = f32v2(1.f, 0.f);
        maxSteps = g_Push.OcclusionResolution;
        origin = f32v2(0.f, f32(id.x));
    }

    f32 dist = 1.0;
    for (f32 t = 0.0; t < maxSteps; t += 1.0)
    {
        const f32v2 pos = origin + dir * t;
        const i32v2 texel = i32v2(pos);

        if (texel.x < 0 || texel.y < 0 ||
            texel.x >= i32(g_Push.OcclusionResolution) ||
            texel.y >= i32(g_Push.OcclusionResolution))
            break;

        if (g_OcclusionMaps[g_Push.OcclusionMapIndex][texel] > 0.0)
        {
            dist = max(0.f, t / maxSteps - g_Push.DistanceBias);
            break;
        }
    }

    g_ShadowMaps[g_Push.ShadowMapIndex][i32(id.x)] = dist;
}

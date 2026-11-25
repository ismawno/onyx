// Fragment hook that reshades the material: toon lighting. Works in 2D and 3D, no varyings.
//
// Onyx resolves the material (textures included) into input.Material, then lights it. The hook takes that material,
// lights it again with ComputeLighting, which applies onyx's lights to any material at any position, and snaps the
// result to a few bands. Objects without a material get lit too: their Material holds defaults, and ComputeLighting
// lights whatever it is given.

static const float Bands = 4.0;

export FragmentHookOutput FragmentHook(const FragmentHookInput input)
{
    FragmentHookOutput output;

    // flat passes have no lights, and their texture is only in Color
    if (input.Pass != HookPass_Shaded)
    {
        output.Color = input.Color;
        output.Outputs.Mask = float4(1.0);
        return output;
    }

    HookMaterial material = input.Material;
#ifdef ONYX_DIMENSION_3D
    // a white, rough, non-metallic surface: what comes back is the light that reaches it, not the surface's color
    material.Albedo = float4(1.0);
    material.Metallic = 0.0;
    material.Roughness = 1.0;
    material.Emissive = float3(0.0);
#endif
    const float3 light = ComputeLighting(material, input.WorldPosition);

    // keep the light's hue, step its intensity
    const float intensity = dot(light, float3(0.2126, 0.7152, 0.0722));
    const float band = ceil(saturate(intensity) * Bands) / Bands;
    const float3 hue = intensity > 0.0 ? light / max(max(light.r, light.g), light.b) : float3(1.0);

    const float4 surface = input.Material.FillColor * input.Material.Albedo;
    float3 color = surface.rgb * hue * band;
#ifdef ONYX_DIMENSION_3D
    color += input.Material.Emissive;
#endif

    output.Color = float4(color, surface.a * input.Coverage);
    output.Outputs.Mask = float4(band, band, band, 1.0);
    return output;
}

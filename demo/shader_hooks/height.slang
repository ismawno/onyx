// Fragment hook for the control center. Varyings: Height (float, smooth). Works in 2D and 3D.
//
// Tints each pixel by the Height varying wave.slang writes: blue where the wave is low, gold where it is high. The
// control center only applies it together with a vertex hook declared with the same varying.

export FragmentHookOutput FragmentHook(const FragmentHookInput input)
{
    // wave.slang moves vertices between -0.3 and 0.3: this maps that range to 0 to 1
    const float t = saturate(input.Varyings.Height / 0.6 + 0.5);
    const float3 low = float3(0.25, 0.35, 1.0);
    const float3 high = float3(1.0, 0.8, 0.3);

    FragmentHookOutput output;
    output.Color = float4(input.Color.rgb * lerp(low, high, t), input.Color.a);
    output.Outputs.Mask = float4(t, t, t, 1.0);
    return output;
}

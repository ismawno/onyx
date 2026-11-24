// Fragment hook for the control center. Varyings: none. Works in 2D and 3D.
//
// Moving diagonal stripes that invert the color Onyx computed for the pixel, and the same stripes written to the
// control center's Mask hook output, which its Shaders panel reads back.

export FragmentHookOutput FragmentHook(const FragmentHookInput input)
{
    const float time = input.Globals.LoadF32(0);
    const float stripe = step(0.5, frac(6.0 * (input.TexCoord.x + input.TexCoord.y) - time));

    FragmentHookOutput output;
    output.Color = float4(lerp(input.Color.rgb, 1.0 - input.Color.rgb, stripe), input.Color.a);
    output.Outputs.Mask = float4(stripe, stripe, stripe, 1.0);
    return output;
}

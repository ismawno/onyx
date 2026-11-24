// Vertex and fragment hook in one file, for the control center. Varyings: Ripple (float, smooth). Works in 2D and 3D.
//
// Load this same file as both the vertex and the fragment hook of a dimension, with the varying declared on both. Each
// compile only links the function of its own stage, but the whole file must compile for both, so both need to see the
// varying.
//
// Rings travel out from the world origin: the vertex hook pushes vertices in and out along them, and the fragment hook
// shades by the same wave, which reaches it as the Ripple varying.

export VertexHookOutput VertexHook(const VertexHookInput input)
{
    const float time = input.Globals.LoadF32(0);
#ifdef ONYX_DIMENSION_3D
    // rings on the floor plane, so they read the same from any height
    const float dist = length(input.WorldPosition.xz);
#else
    const float dist = length(input.WorldPosition);
#endif
    const float ripple = sin(3.0 * dist - 3.0 * time);

    VertexHookOutput output;
    output.WorldPosition = input.WorldPosition;
#ifdef ONYX_DIMENSION_3D
    output.WorldPosition.y += 0.15 * ripple;
#else
    output.WorldPosition *= 1.0 + 0.04 * ripple;
#endif
    output.Varyings.Ripple = ripple;
    return output;
}

export FragmentHookOutput FragmentHook(const FragmentHookInput input)
{
    const float t = 0.5 + 0.5 * input.Varyings.Ripple;

    FragmentHookOutput output;
    output.Color = float4(input.Color.rgb * lerp(0.4, 1.2, t), input.Color.a);
    output.Outputs.Mask = float4(t, t, t, 1.0);
    return output;
}

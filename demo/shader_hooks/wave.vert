// Vertex hook for the control center. Varyings: Height (float, smooth). Works in 2D and 3D.
//
// Moves every vertex up and down with a wave that travels along x, and hands the offset to the fragment hook as the
// Height varying. Pair it with height.frag, declared with the same varying.

export VertexHookOutput VertexHook(const VertexHookInput input)
{
    const float time = input.Globals.LoadF32(0);
    const float height = 0.3 * sin(2.0 * time + 1.5 * input.WorldPosition.x);

    VertexHookOutput output;
    output.WorldPosition = input.WorldPosition;
    output.WorldPosition.y += height;
    output.Varyings.Height = height;
    return output;
}

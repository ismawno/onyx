// Fragment hook for the control center that does not compile, on purpose: precompile it to see how the Shaders panel
// reports errors. The message should point at line 10 of this file.

export FragmentHookOutput FragmentHook(const FragmentHookInput input)
{
    FragmentHookOutput output;
    output.Color = input.Color;
    output.Outputs.Mask = float4(1.0);
    // Brightness is not a field of FragmentHookInput
    output.Color.rgb *= input.Brightness;
    return output;
}

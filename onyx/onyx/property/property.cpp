#include "onyx/core/pch.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/asset/material.hpp"

namespace Onyx
{
const char *ToString(const Geometry geo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return "Geometry_Circle";
    case Geometry_Static:
        return "Geometry_Static";
    case Geometry_Parametric:
        return "Geometry_Parametric";
    default:
        return "Unknown";
    }
}

const char *ToString(const LightType light)
{
    switch (light)
    {
    case Light_Point:
        return "Light_Point";
    case Light_Directional:
        return "Light_Directional";
    default:
        return "Unknown";
    }
}

const char *ToString(const StencilPass pass)
{
    switch (pass)
    {
    case StencilPass_NoStencilWriteDoFill:
        return "StencilPass_NoStencilWriteDoFill";
    case StencilPass_DoStencilWriteDoFill:
        return "StencilPass_DoStencilWriteDoFill";
    case StencilPass_DoStencilWriteNoFill:
        return "StencilPass_DoStencilWriteNoFill";
    case StencilPass_DoStencilTestNoFill:
        return "StencilPass_DoStencilTestNoFill";
    default:
        return "Unknown";
    }
}

const char *ToString(const DrawPass pass)
{
    switch (pass)
    {
    case DrawPass_Fill:
        return "DrawPass_Fill";
    case DrawPass_Stencil:
        return "DrawPass_Stencil";
    default:
        return "Unknown";
    }
}

const char *ToString(const Shading shading)
{
    switch (shading)
    {
    case Shading_Unlit:
        return "Shading_Unlit";
    case Shading_Lit:
        return "Shading_Lit";
    default:
        return "Unknown";
    }
}
const char *ToString(const AssetType atype)
{
    switch (atype)
    {
    case Asset_StaticMesh:
        return "Asset_StaticMesh";
    case Asset_ParametricMesh:
        return "Asset_ParametricMesh";
    case Asset_GlyphMesh:
        return "Asset_GlyphMesh";
    case Asset_Material:
        return "Asset_Material";
    case Asset_Font:
        return "Asset_Font";
    case Asset_Sampler:
        return "Asset_Sampler";
    case Asset_Texture:
        return "Asset_Texture";
    case Asset_Count:
        return "Asset_Count";
    default:
        return "Unknown";
    }
}

const char *ToString(const TextureSlot slot)
{
    switch (slot)
    {
    case TextureSlot_Albedo:
        return "TextureSlot_Albedo";
    case TextureSlot_MetallicRoughness:
        return "TextureSlot_MetallicRoughness";
    case TextureSlot_Normal:
        return "TextureSlot_Normal";
    case TextureSlot_Occlusion:
        return "TextureSlot_Occlusion";
    case TextureSlot_Emissive:
        return "TextureSlot_Emissive";
    case TextureSlot_Count:
        return "TextureSlot_Count";
    default:
        return "Unknown";
    }
}
} // namespace Onyx

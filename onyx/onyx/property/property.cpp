#include "onyx/core/pch.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/rendering/pass.hpp"
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
    case Geometry_Glyph:
        return "Geometry_Glyph";
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

const char *ToString(const RenderPass pass)
{
    switch (pass)
    {
    case RenderPass_Shaded:
        return "RenderPass_Shaded";
    case RenderPass_Flat:
        return "RenderPass_Flat";
    case RenderPass_Shadow:
        return "RenderPass_Shadow";
    default:
        return "Unknown";
    }
}

const char *ToString(const RenderMode mode)
{
    switch (mode)
    {
    case RenderMode_Shaded:
        return "RenderMode_Shaded";
    case RenderMode_Flat:
        return "RenderMode_Flat";
    case RenderMode_Outlined:
        return "RenderMode_Outlined";
    case RenderMode_ShadedOutlined:
        return "RenderMode_ShadedOutlined";
    case RenderMode_FlatOutlined:
        return "RenderMode_FlatOutlined";
    case RenderMode_None:
        return "RenderMode_None";
    default:
        return "Unknown";
    }
}
const char *ToString(const PipelinePass pass)
{
    switch (pass)
    {
    case PipelinePass_Shaded:
        return "PipelinePass_Shaded";
    case PipelinePass_Flat:
        return "PipelinePass_Outlined";
    case PipelinePass_Outlined:
        return "PipelinePass_Outlined";
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
    case Asset_Bounds:
        return "Asset_Bounds";
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

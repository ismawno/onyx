#include "onyx/core/pch.hpp"
#include "onyx/property/instance.hpp"
#include "onyx/rendering/pass.hpp"
#include "onyx/resource/material.hpp"

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
    case Light_Spot:
        return "Light_Spot";
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

const char *ToString(const ResourceType atype)
{
    switch (atype)
    {
    case Resource_StaticMesh:
        return "Resource_StaticMesh";
    case Resource_ParametricMesh:
        return "Resource_ParametricMesh";
    case Resource_GlyphMesh:
        return "Resource_GlyphMesh";
    case Resource_Material:
        return "Resource_Material";
    case Resource_Font:
        return "Resource_Font";
    case Resource_Sampler:
        return "Resource_Sampler";
    case Resource_Texture:
        return "Resource_Texture";
    case Resource_Bounds:
        return "Resource_Bounds";
    case Resource_Buffer:
        return "Resource_Buffer";
    case Resource_Image:
        return "Resource_Image";
    case Resource_Count:
        return "Resource_Count";
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

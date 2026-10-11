#include "pch.hpp"
#include "onyx/material.hpp"
#include "conversion.hpp"
#include "pass.hpp"

namespace Onyx
{
Graph::Format GetFormat(const u32 components, const ImageComponentType type, const bool rgb)
{
    switch (type)
    {
    case ImageComponent_UnsignedByte:
    case ImageComponent_SignedByte:
        switch (components)
        {
        case 1:
            return rgb ? Graph::Format_R8_SRGB : Graph::Format_R8_UNORM;
        case 2:
            return rgb ? Graph::Format_R8G8_SRGB : Graph::Format_R8G8_UNORM;
        case 3:
            return rgb ? Graph::Format_R8G8B8_SRGB : Graph::Format_R8G8B8_UNORM;
        case 4:
            return rgb ? Graph::Format_R8G8B8A8_SRGB : Graph::Format_R8G8B8A8_UNORM;
        }
    case ImageComponent_UnsignedShort:
    case ImageComponent_SignedShort:
        switch (components)
        {
        case 1:
            return Graph::Format_R16_UNORM;
        case 2:
            return Graph::Format_R16G16_UNORM;
        case 3:
            return Graph::Format_R16G16B16_UNORM;
        case 4:
            return Graph::Format_R16G16B16A16_UNORM;
        }
    case ImageComponent_UnsignedInteger:
    case ImageComponent_SignedInteger:
        switch (components)
        {
        case 1:
            return Graph::Format_R32_UINT;
        case 2:
            return Graph::Format_R32G32_UINT;
        case 3:
            return Graph::Format_R32G32B32_UINT;
        case 4:
            return Graph::Format_R32G32B32A32_UINT;
        }
    case ImageComponent_Float:
        switch (components)
        {
        case 1:
            return Graph::Format_R32_SFLOAT;
        case 2:
            return Graph::Format_R32G32_SFLOAT;
        case 3:
            return Graph::Format_R32G32B32_SFLOAT;
        case 4:
            return Graph::Format_R32G32B32A32_SFLOAT;
        }
    default:
        return Graph::Format_Undefined;
    }
}

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
    case Geometry_Dynamic:
        return "Geometry_Dynamic";
    default:
        return "Unknown";
    }
}

const char *ToString(const BlendPass bpass)
{
    switch (bpass)
    {
    case BlendPass_Opaque:
        return "BlendPass_Opaque";
    case BlendPass_Transparent:
        return "BlendPass_Transparent";
    case BlendPass_All:
        return "BlendPass_All";
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

const char *ToString(const ResourceType rtype)
{
    switch (rtype)
    {
    case Resource_StaticMesh:
        return "Resource_StaticMesh";
    case Resource_ParametricMesh:
        return "Resource_ParametricMesh";
    case Resource_GlyphMesh:
        return "Resource_GlyphMesh";
    case Resource_DynamicMesh:
        return "Resource_DynamicMesh";
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
const char *ToString(const ErrorCode code)
{
    switch (code)
    {
    case Error_LoadFailed:
        return "LoadFailed";
    case Error_FileNotFound:
        return "FileNotFound";
    case Error_EntryPointNotFound:
        return "EntryPointNotFound";
    case Error_ShaderCompilationFailed:
        return "ShaderCompilationFailed";
    default:
        return "Unknown";
    }
}
} // namespace Onyx

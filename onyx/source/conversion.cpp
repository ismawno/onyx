#include "pch.hpp"
#include "onyx/material.hpp"
#include "onyx/input.hpp"
#include "onyx/window.hpp"
#include "conversion.hpp"
#include "pass.hpp"

namespace Onyx
{
Format GetFormat(const u32 components, const ImageComponentType type, const bool rgb)
{
    switch (type)
    {
    case ImageComponent_UnsignedByte:
    case ImageComponent_SignedByte:
        switch (components)
        {
        case 1:
            return rgb ? Format_R8_SRGB : Format_R8_UNORM;
        case 2:
            return rgb ? Format_R8G8_SRGB : Format_R8G8_UNORM;
        case 3:
            return rgb ? Format_R8G8B8_SRGB : Format_R8G8B8_UNORM;
        case 4:
            return rgb ? Format_R8G8B8A8_SRGB : Format_R8G8B8A8_UNORM;
        }
    case ImageComponent_UnsignedShort:
    case ImageComponent_SignedShort:
        switch (components)
        {
        case 1:
            return Format_R16_UNORM;
        case 2:
            return Format_R16G16_UNORM;
        case 3:
            return Format_R16G16B16_UNORM;
        case 4:
            return Format_R16G16B16A16_UNORM;
        }
    case ImageComponent_UnsignedInteger:
    case ImageComponent_SignedInteger:
        switch (components)
        {
        case 1:
            return Format_R32_UINT;
        case 2:
            return Format_R32G32_UINT;
        case 3:
            return Format_R32G32B32_UINT;
        case 4:
            return Format_R32G32B32A32_UINT;
        }
    case ImageComponent_Float:
        switch (components)
        {
        case 1:
            return Format_R32_SFLOAT;
        case 2:
            return Format_R32G32_SFLOAT;
        case 3:
            return Format_R32G32B32_SFLOAT;
        case 4:
            return Format_R32G32B32A32_SFLOAT;
        }
    default:
        return Format_Undefined;
    }
}

VkFormat AsVulkanFormat(const Format format)
{
    switch (format)
    {
    case Format_Undefined:
        return VK_FORMAT_UNDEFINED;

    case Format_R8_UNORM:
        return VK_FORMAT_R8_UNORM;
    case Format_R8_SNORM:
        return VK_FORMAT_R8_SNORM;
    case Format_R8_UINT:
        return VK_FORMAT_R8_UINT;
    case Format_R8_SINT:
        return VK_FORMAT_R8_SINT;
    case Format_R8_SRGB:
        return VK_FORMAT_R8_SRGB;

    case Format_R8G8_UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case Format_R8G8_SNORM:
        return VK_FORMAT_R8G8_SNORM;
    case Format_R8G8_UINT:
        return VK_FORMAT_R8G8_UINT;
    case Format_R8G8_SINT:
        return VK_FORMAT_R8G8_SINT;
    case Format_R8G8_SRGB:
        return VK_FORMAT_R8G8_SRGB;

    case Format_R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_UNORM;
    case Format_R8G8B8_SNORM:
        return VK_FORMAT_R8G8B8_SNORM;
    case Format_R8G8B8_UINT:
        return VK_FORMAT_R8G8B8_UINT;
    case Format_R8G8B8_SINT:
        return VK_FORMAT_R8G8B8_SINT;
    case Format_R8G8B8_SRGB:
        return VK_FORMAT_R8G8B8_SRGB;

    case Format_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Format_R8G8B8A8_SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case Format_R8G8B8A8_UINT:
        return VK_FORMAT_R8G8B8A8_UINT;
    case Format_R8G8B8A8_SINT:
        return VK_FORMAT_R8G8B8A8_SINT;
    case Format_R8G8B8A8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;

    case Format_B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case Format_B8G8R8A8_SNORM:
        return VK_FORMAT_B8G8R8A8_SNORM;
    case Format_B8G8R8A8_UINT:
        return VK_FORMAT_B8G8R8A8_UINT;
    case Format_B8G8R8A8_SINT:
        return VK_FORMAT_B8G8R8A8_SINT;
    case Format_B8G8R8A8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;

    case Format_R16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case Format_R16_SNORM:
        return VK_FORMAT_R16_SNORM;
    case Format_R16_UINT:
        return VK_FORMAT_R16_UINT;
    case Format_R16_SINT:
        return VK_FORMAT_R16_SINT;
    case Format_R16_SFLOAT:
        return VK_FORMAT_R16_SFLOAT;

    case Format_R16G16_UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case Format_R16G16_SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case Format_R16G16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case Format_R16G16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case Format_R16G16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;

    case Format_R16G16B16_UNORM:
        return VK_FORMAT_R16G16B16_UNORM;
    case Format_R16G16B16_SNORM:
        return VK_FORMAT_R16G16B16_SNORM;
    case Format_R16G16B16_UINT:
        return VK_FORMAT_R16G16B16_UINT;
    case Format_R16G16B16_SINT:
        return VK_FORMAT_R16G16B16_SINT;
    case Format_R16G16B16_SFLOAT:
        return VK_FORMAT_R16G16B16_SFLOAT;

    case Format_R16G16B16A16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case Format_R16G16B16A16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case Format_R16G16B16A16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case Format_R16G16B16A16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case Format_R16G16B16A16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;

    case Format_R32_UINT:
        return VK_FORMAT_R32_UINT;
    case Format_R32_SINT:
        return VK_FORMAT_R32_SINT;
    case Format_R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;

    case Format_R32G32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case Format_R32G32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case Format_R32G32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;

    case Format_R32G32B32_UINT:
        return VK_FORMAT_R32G32B32_UINT;
    case Format_R32G32B32_SINT:
        return VK_FORMAT_R32G32B32_SINT;
    case Format_R32G32B32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;

    case Format_R32G32B32A32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case Format_R32G32B32A32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case Format_R32G32B32A32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    case Format_D16_UNORM:
        return VK_FORMAT_D16_UNORM;
    case Format_D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;

    case Format_D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case Format_D32_SFLOAT_S8_UINT:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;

    case Format_BC1_RGBA_UNORM:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case Format_BC1_RGBA_SRGB:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case Format_BC5_UNORM:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case Format_BC5_SNORM:
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case Format_BC7_UNORM:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case Format_BC7_SRGB:
        return VK_FORMAT_BC7_SRGB_BLOCK;

    default:
        return VK_FORMAT_UNDEFINED;
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
const char *ToString(const PresentMode mode)
{
    switch (mode)
    {
    case PresentMode_Immediate:
        return "PresentMode_Albedo";
    case PresentMode_Mailbox:
        return "PresentMode_MetallicRoughness";
    case PresentMode_VSync:
        return "PresentMode_Normal";
    case PresentMode_Count:
        return "PresentMode_Count";
    default:
        return "Unknown";
    }
}
const char *ToString(const Key key)
{
    switch (key)
    {
    case Key_Space:
        return "Space";
    case Key_Apostrophe:
        return "Apostrophe";
    case Key_Comma:
        return "Comma";
    case Key_Minus:
        return "Minus";
    case Key_Period:
        return "Period";
    case Key_Slash:
        return "Slash";
    case Key_N0:
        return "N0";
    case Key_N1:
        return "N1";
    case Key_N2:
        return "N2";
    case Key_N3:
        return "N3";
    case Key_N4:
        return "N4";
    case Key_N5:
        return "N5";
    case Key_N6:
        return "N6";
    case Key_N7:
        return "N7";
    case Key_N8:
        return "N8";
    case Key_N9:
        return "N9";
    case Key_Semicolon:
        return "Semicolon";
    case Key_Equal:
        return "Equal";
    case Key_A:
        return "A";
    case Key_B:
        return "B";
    case Key_C:
        return "C";
    case Key_D:
        return "D";
    case Key_E:
        return "E";
    case Key_F:
        return "F";
    case Key_G:
        return "G";
    case Key_H:
        return "H";
    case Key_I:
        return "I";
    case Key_J:
        return "J";
    case Key_K:
        return "K";
    case Key_L:
        return "L";
    case Key_M:
        return "M";
    case Key_N:
        return "N";
    case Key_O:
        return "O";
    case Key_P:
        return "P";
    case Key_Q:
        return "Q";
    case Key_R:
        return "R";
    case Key_S:
        return "S";
    case Key_T:
        return "T";
    case Key_U:
        return "U";
    case Key_V:
        return "V";
    case Key_W:
        return "W";
    case Key_X:
        return "X";
    case Key_Y:
        return "Y";
    case Key_Z:
        return "Z";
    case Key_LeftBracket:
        return "LeftBracket";
    case Key_Backslash:
        return "Backslash";
    case Key_RightBracket:
        return "RightBracket";
    case Key_GraveAccent:
        return "GraveAccent";
    case Key_World_1:
        return "World_1";
    case Key_World_2:
        return "World_2";
    case Key_Escape:
        return "Escape";
    case Key_Enter:
        return "Enter";
    case Key_Tab:
        return "Tab";
    case Key_Backspace:
        return "Backspace";
    case Key_Insert:
        return "Insert";
    case Key_Delete:
        return "Delete";
    case Key_Right:
        return "Right";
    case Key_Left:
        return "Left";
    case Key_Down:
        return "Down";
    case Key_Up:
        return "Up";
    case Key_PageUp:
        return "PageUp";
    case Key_PageDown:
        return "PageDown";
    case Key_Home:
        return "Home";
    case Key_End:
        return "End";
    case Key_CapsLock:
        return "CapsLock";
    case Key_ScrollLock:
        return "ScrollLock";
    case Key_NumLock:
        return "NumLock";
    case Key_PrintScreen:
        return "PrintScreen";
    case Key_Pause:
        return "Pause";
    case Key_F1:
        return "F1";
    case Key_F2:
        return "F2";
    case Key_F3:
        return "F3";
    case Key_F4:
        return "F4";
    case Key_F5:
        return "F5";
    case Key_F6:
        return "F6";
    case Key_F7:
        return "F7";
    case Key_F8:
        return "F8";
    case Key_F9:
        return "F9";
    case Key_F10:
        return "F10";
    case Key_F11:
        return "F11";
    case Key_F12:
        return "F12";
    case Key_F13:
        return "F13";
    case Key_F14:
        return "F14";
    case Key_F15:
        return "F15";
    case Key_F16:
        return "F16";
    case Key_F17:
        return "F17";
    case Key_F18:
        return "F18";
    case Key_F19:
        return "F19";
    case Key_F20:
        return "F20";
    case Key_F21:
        return "F21";
    case Key_F22:
        return "F22";
    case Key_F23:
        return "F23";
    case Key_F24:
        return "F24";
    case Key_F25:
        return "F25";
    case Key_KP_0:
        return "KP_0";
    case Key_KP_1:
        return "KP_1";
    case Key_KP_2:
        return "KP_2";
    case Key_KP_3:
        return "KP_3";
    case Key_KP_4:
        return "KP_4";
    case Key_KP_5:
        return "KP_5";
    case Key_KP_6:
        return "KP_6";
    case Key_KP_7:
        return "KP_7";
    case Key_KP_8:
        return "KP_8";
    case Key_KP_9:
        return "KP_9";
    case Key_KPDecimal:
        return "KPDecimal";
    case Key_KPDivide:
        return "KPDivide";
    case Key_KPMultiply:
        return "KPMultiply";
    case Key_KPSubtract:
        return "KPSubtract";
    case Key_KPAdd:
        return "KPAdd";
    case Key_KPEnter:
        return "KPEnter";
    case Key_KPEqual:
        return "KPEqual";
    case Key_LeftShift:
        return "LeftShift";
    case Key_LeftControl:
        return "LeftControl";
    case Key_LeftAlt:
        return "LeftAlt";
    case Key_LeftSuper:
        return "LeftSuper";
    case Key_RightShift:
        return "RightShift";
    case Key_RightControl:
        return "RightControl";
    case Key_RightAlt:
        return "RightAlt";
    case Key_RightSuper:
        return "RightSuper";
    case Key_Menu:
        return "Menu";
    case Key_None:
        return "None";
    case Key_Count:
        return "Unknown";
    }
    return "Unknown";
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

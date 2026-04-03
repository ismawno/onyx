#include "onyx/core/pch.hpp"
#include "onyx/asset/texture.hpp"
#include <stb_image.h>

namespace Onyx
{
Result<TextureData> LoadTextureDataFromImageFile(const char *path, const ImageComponentFormat requiredComponents,
                                                 const LoadTextureDataFlags flags)
{
    TextureData data{};
    i32 w;
    i32 h;
    i32 c;

    ImageComponentType type;
    void *img = nullptr;

    if (stbi_is_hdr(path))
    {
        type = ImageComponent_Float;
        img = stbi_loadf(path, &w, &h, &c, i32(requiredComponents));
    }
    else if (stbi_is_16_bit(path))
    {
        type = ImageComponent_UnsignedShort;
        img = stbi_load_16(path, &w, &h, &c, i32(requiredComponents));
    }
    else
    {
        type = ImageComponent_UnsignedByte;
        img = stbi_load(path, &w, &h, &c, i32(requiredComponents));
    }

    if (!img)
        return Result<TextureData>::Error(
            Error_LoadFailed, TKit::Format("[ONYX][TEXTURE] Failed to load image data: {}", stbi_failure_reason()));

    data.Width = u32(w);
    data.Height = u32(h);
    data.Components = requiredComponents > 0 ? requiredComponents : u32(c);
    data.Format = Detail::GetFormat(i32(data.Components), type, !(flags & LoadTextureDataFlag_AsLinearImage));
    const usz size = data.ComputeSize();

    data.Data = scast<std::byte *>(TKit::Allocate(size));
    TKIT_ASSERT(data.Data, "[ONYX][TEXTURE] Failed to allocate image data with size {:L} bytes", size);
    TKit::ForwardCopy(data.Data, img, size);
    stbi_image_free(img);
    return data;
}
} // namespace Onyx

namespace Onyx::Detail
{
VkFormat GetFormat(const u32 components, const ImageComponentType type, const bool rgb)
{
    switch (type)
    {
    case ImageComponent_UnsignedByte:
    case ImageComponent_SignedByte:
        switch (components)
        {
        case 1:
            return rgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
        case 2:
            return rgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
        case 3:
            return rgb ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
        case 4:
            return rgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        }
    case ImageComponent_UnsignedShort:
    case ImageComponent_SignedShort:
        switch (components)
        {
        case 1:
            return VK_FORMAT_R16_UNORM;
        case 2:
            return VK_FORMAT_R16G16_UNORM;
        case 3:
            return VK_FORMAT_R16G16B16_UNORM;
        case 4:
            return VK_FORMAT_R16G16B16A16_UNORM;
        }
    case ImageComponent_UnsignedInteger:
    case ImageComponent_SignedInteger:
        switch (components)
        {
        case 1:
            return VK_FORMAT_R32_UINT;
        case 2:
            return VK_FORMAT_R32G32_UINT;
        case 3:
            return VK_FORMAT_R32G32B32_UINT;
        case 4:
            return VK_FORMAT_R32G32B32A32_UINT;
        }
    case ImageComponent_Float:
        switch (components)
        {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
    default:
        return VK_FORMAT_UNDEFINED;
    }
}
} // namespace Onyx::Detail

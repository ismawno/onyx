#pragma once

#include "onyx/core/core.hpp"
#include "vkit/resource/device_image.hpp"

namespace Onyx
{
struct TextureData
{
    std::byte *Data = nullptr;
    u32 Width = 0;
    u32 Height = 0;
    u32 Components = 0;
    VkFormat Format = VK_FORMAT_UNDEFINED;

    usz ComputeSize() const
    {
        return Width * Height * VKit::DeviceImage::GetBytesPerPixel(Format);
    }
};

enum ImageComponentType : u8
{
    ImageComponent_UnsignedByte,
    ImageComponent_UnsignedShort,
    ImageComponent_UnsignedInteger,
    ImageComponent_SignedByte,
    ImageComponent_SignedShort,
    ImageComponent_SignedInteger,
    ImageComponent_Float
};

enum ImageComponentFormat : u8
{
    ImageComponent_Auto = 0,
    ImageComponent_Grey = 1,
    ImageComponent_GreyAlpha = 2,
    ImageComponent_RGB = 3,
    ImageComponent_RGBA = 4,
};

#ifdef ONYX_ENABLE_IMAGE_LOAD
using LoadTextureDataFlags = u8;
enum LoadTextureDataFlagBit : LoadTextureDataFlags
{
    LoadTextureDataFlag_AsLinearImage = 1 << 0,
};

ONYX_NO_DISCARD Result<TextureData> LoadTextureDataFromImageFile(
    const char *path, const ImageComponentFormat requiredComponents = ImageComponent_Auto,
    LoadTextureDataFlags flags = 0);
#endif

} // namespace Onyx

namespace Onyx::Detail
{
VkFormat GetFormat(const u32 components, ImageComponentType type, bool rgb);
}

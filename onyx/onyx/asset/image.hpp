#pragma once

#include "onyx/core/core.hpp"
#include "vkit/resource/device_image.hpp"

namespace Onyx
{
enum ImageOperation : u8
{
    ImageOperation_FlipX,
    ImageOperation_FlipY,
    ImageOperation_Rotate90CW,
    ImageOperation_Rotate90CCW,
    ImageOperation_Rotate180
};

struct ImageData
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

    void Manipulate(ImageOperation op);
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
using LoadImageDataFlags = u8;
enum LoadImageDataFlagBit : LoadImageDataFlags
{
    LoadImageDataFlag_AsLinearImage = 1 << 0,
};

ONYX_NO_DISCARD Result<ImageData> LoadImageDataFromFile(
    const char *path, const ImageComponentFormat requiredComponents = ImageComponent_Auto,
    LoadImageDataFlags flags = 0);
#endif

} // namespace Onyx

namespace Onyx::Detail
{
VkFormat GetFormat(const u32 components, ImageComponentType type, bool rgb);
}

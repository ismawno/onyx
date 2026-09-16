#pragma once

#include "onyx/core.hpp"

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

enum ImageFormat : u8
{
    ImageFormat_Undefined,
    // 8-bit single channel
    ImageFormat_R8_UNORM,
    ImageFormat_R8_SNORM,
    ImageFormat_R8_UINT,
    ImageFormat_R8_SINT,
    ImageFormat_R8_SRGB,

    // 8-bit dual channel
    ImageFormat_R8G8_UNORM,
    ImageFormat_R8G8_SNORM,
    ImageFormat_R8G8_UINT,
    ImageFormat_R8G8_SINT,
    ImageFormat_R8G8_SRGB,

    // 8-bit RGB
    ImageFormat_R8G8B8_UNORM,
    ImageFormat_R8G8B8_SNORM,
    ImageFormat_R8G8B8_UINT,
    ImageFormat_R8G8B8_SINT,
    ImageFormat_R8G8B8_SRGB,

    // 8-bit RGBA
    ImageFormat_R8G8B8A8_UNORM,
    ImageFormat_R8G8B8A8_SNORM,
    ImageFormat_R8G8B8A8_UINT,
    ImageFormat_R8G8B8A8_SINT,
    ImageFormat_R8G8B8A8_SRGB,

    // 8-bit BGRA
    ImageFormat_B8G8R8A8_UNORM,
    ImageFormat_B8G8R8A8_SNORM,
    ImageFormat_B8G8R8A8_UINT,
    ImageFormat_B8G8R8A8_SINT,
    ImageFormat_B8G8R8A8_SRGB,

    // 16-bit single channel
    ImageFormat_R16_UNORM,
    ImageFormat_R16_SNORM,
    ImageFormat_R16_UINT,
    ImageFormat_R16_SINT,
    ImageFormat_R16_SFLOAT,

    // 16-bit dual channel
    ImageFormat_R16G16_UNORM,
    ImageFormat_R16G16_SNORM,
    ImageFormat_R16G16_UINT,
    ImageFormat_R16G16_SINT,
    ImageFormat_R16G16_SFLOAT,

    // 16-bit RGB
    ImageFormat_R16G16B16_UNORM,
    ImageFormat_R16G16B16_SNORM,
    ImageFormat_R16G16B16_UINT,
    ImageFormat_R16G16B16_SINT,
    ImageFormat_R16G16B16_SFLOAT,

    // 16-bit RGBA
    ImageFormat_R16G16B16A16_UNORM,
    ImageFormat_R16G16B16A16_SNORM,
    ImageFormat_R16G16B16A16_UINT,
    ImageFormat_R16G16B16A16_SINT,
    ImageFormat_R16G16B16A16_SFLOAT,

    // 32-bit single channel
    ImageFormat_R32_UINT,
    ImageFormat_R32_SINT,
    ImageFormat_R32_SFLOAT,

    // 32-bit dual channel
    ImageFormat_R32G32_UINT,
    ImageFormat_R32G32_SINT,
    ImageFormat_R32G32_SFLOAT,

    // 32-bit RGB
    ImageFormat_R32G32B32_UINT,
    ImageFormat_R32G32B32_SINT,
    ImageFormat_R32G32B32_SFLOAT,

    // 32-bit RGBA
    ImageFormat_R32G32B32A32_UINT,
    ImageFormat_R32G32B32A32_SINT,
    ImageFormat_R32G32B32A32_SFLOAT,

    // Packed HDR
    ImageFormat_B10G11R11_UnsignedFloat,

    // Depth
    ImageFormat_D16_UNORM,
    ImageFormat_D32_SFLOAT,

    // Depth + Stencil
    ImageFormat_D24_UNORM_S8_UINT,
    ImageFormat_D32_SFLOAT_S8_UINT,

    // Compressed
    ImageFormat_BC1_RGBA_UNORM,
    ImageFormat_BC1_RGBA_SRGB,
    ImageFormat_BC5_UNORM,
    ImageFormat_BC5_SNORM,
    ImageFormat_BC7_UNORM,
    ImageFormat_BC7_SRGB,

    ImageFormat_Count
};

struct ImageData
{
    std::byte *Data = nullptr;
    u32 Width = 0;
    u32 Height = 0;
    u32 Components = 0;
    ImageFormat Format = ImageFormat_Undefined;

    usz ComputeSize() const;
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
    LoadImageDataFlag_AsLinearImage = 1U << 0,
};

ONYX_NO_DISCARD Result<ImageData> Image_LoadDataFromFile(const char *path,
                                                         ImageComponentFormat requiredComponents = ImageComponent_Auto,
                                                         LoadImageDataFlags flags = 0);
ONYX_NO_DISCARD Result<ImageData> Image_LoadDataFromMemory(
    const std::byte *memory, u32 size, ImageComponentFormat requiredComponents = ImageComponent_Auto,
    LoadImageDataFlags flags = 0);
void Image_UnloadData(const ImageData &data);
#endif
} // namespace Onyx

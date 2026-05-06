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

enum Format : u8
{
    Format_Undefined,
    // 8-bit single channel
    Format_R8_UNORM,
    Format_R8_SNORM,
    Format_R8_UINT,
    Format_R8_SINT,
    Format_R8_SRGB,

    // 8-bit dual channel
    Format_R8G8_UNORM,
    Format_R8G8_SNORM,
    Format_R8G8_UINT,
    Format_R8G8_SINT,
    Format_R8G8_SRGB,

    // 8-bit RGB
    Format_R8G8B8_UNORM,
    Format_R8G8B8_SNORM,
    Format_R8G8B8_UINT,
    Format_R8G8B8_SINT,
    Format_R8G8B8_SRGB,

    // 8-bit RGBA
    Format_R8G8B8A8_UNORM,
    Format_R8G8B8A8_SNORM,
    Format_R8G8B8A8_UINT,
    Format_R8G8B8A8_SINT,
    Format_R8G8B8A8_SRGB,

    // 8-bit BGRA
    Format_B8G8R8A8_UNORM,
    Format_B8G8R8A8_SNORM,
    Format_B8G8R8A8_UINT,
    Format_B8G8R8A8_SINT,
    Format_B8G8R8A8_SRGB,

    // 16-bit single channel
    Format_R16_UNORM,
    Format_R16_SNORM,
    Format_R16_UINT,
    Format_R16_SINT,
    Format_R16_SFLOAT,

    // 16-bit dual channel
    Format_R16G16_UNORM,
    Format_R16G16_SNORM,
    Format_R16G16_UINT,
    Format_R16G16_SINT,
    Format_R16G16_SFLOAT,

    // 16-bit RGB
    Format_R16G16B16_UNORM,
    Format_R16G16B16_SNORM,
    Format_R16G16B16_UINT,
    Format_R16G16B16_SINT,
    Format_R16G16B16_SFLOAT,

    // 16-bit RGBA
    Format_R16G16B16A16_UNORM,
    Format_R16G16B16A16_SNORM,
    Format_R16G16B16A16_UINT,
    Format_R16G16B16A16_SINT,
    Format_R16G16B16A16_SFLOAT,

    // 32-bit single channel
    Format_R32_UINT,
    Format_R32_SINT,
    Format_R32_SFLOAT,

    // 32-bit dual channel
    Format_R32G32_UINT,
    Format_R32G32_SINT,
    Format_R32G32_SFLOAT,

    // 32-bit RGB
    Format_R32G32B32_UINT,
    Format_R32G32B32_SINT,
    Format_R32G32B32_SFLOAT,

    // 32-bit RGBA
    Format_R32G32B32A32_UINT,
    Format_R32G32B32A32_SINT,
    Format_R32G32B32A32_SFLOAT,

    // Packed HDR
    Format_B10G11R11_UnsignedFloat,

    // Depth
    Format_D16_UNORM,
    Format_D32_SFLOAT,

    // Depth + Stencil
    Format_D24_UNORM_S8_UINT,
    Format_D32_SFLOAT_S8_UINT,

    // Compressed
    Format_BC1_RGBA_UNORM,
    Format_BC1_RGBA_SRGB,
    Format_BC5_UNORM,
    Format_BC5_SNORM,
    Format_BC7_UNORM,
    Format_BC7_SRGB,

    Format_Count
};

struct ImageData
{
    std::byte *Data = nullptr;
    u32 Width = 0;
    u32 Height = 0;
    u32 Components = 0;
    Format Format = Format_Undefined;

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
    LoadImageDataFlag_AsLinearImage = 1 << 0,
};

ONYX_NO_DISCARD Result<ImageData> LoadImageDataFromFile(const char *path,
                                                        ImageComponentFormat requiredComponents = ImageComponent_Auto,
                                                        LoadImageDataFlags flags = 0);
ONYX_NO_DISCARD Result<ImageData> LoadImageDataFromMemory(const std::byte *memory, u32 size,
                                                          ImageComponentFormat requiredComponents = ImageComponent_Auto,
                                                          LoadImageDataFlags flags = 0);
#endif
} // namespace Onyx

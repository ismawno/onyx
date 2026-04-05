#include "onyx/core/pch.hpp"
#include "onyx/asset/image.hpp"
#ifdef ONYX_ENABLE_IMAGE_LOAD
#    include <stb_image.h>
#endif

namespace Onyx
{
static void flipX(std::byte *data, const u32 width, const u32 height, const u32 components)
{
    const u32 stride = width * components;
    for (u32 y = 0; y < height; ++y)
    {
        std::byte *row = data + y * stride;
        for (u32 x = 0; x < width / 2; ++x)
        {
            std::byte *a = row + x * components;
            std::byte *b = row + (width - 1 - x) * components;
            for (u32 c = 0; c < components; ++c)
                std::swap(a[c], b[c]);
        }
    }
}
static void flipY(std::byte *data, const u32 width, const u32 height, const u32 components)
{
    const u32 stride = width * components;
    for (u32 y = 0; y < height / 2; ++y)
    {
        std::byte *a = data + y * stride;
        std::byte *b = data + (height - 1 - y) * stride;
        for (u32 x = 0; x < stride; ++x)
            std::swap(a[x], b[x]);
    }
}

static void rotate90CW(std::byte *&data, u32 &width, u32 &height, const u32 components, const usz size)
{
    const u32 nw = height;
    const u32 nh = width;

    std::byte *tmp = scast<std::byte *>(TKit::Allocate(size));

    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x)
        {
            const std::byte *src = data + (y * width + x) * components;
            std::byte *dst = tmp + (x * nw + (nw - 1 - y)) * components;
            for (u32 c = 0; c < components; ++c)
                dst[c] = src[c];
        }

    TKit::Deallocate(data);
    data = tmp;
    width = nw;
    height = nh;
}

static void rotate90CCW(std::byte *&data, u32 &width, u32 &height, const u32 components, const usz size)
{
    const u32 nw = height;
    const u32 nh = width;

    std::byte *tmp = scast<std::byte *>(TKit::Allocate(size));

    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x)
        {
            const std::byte *src = data + (y * width + x) * components;
            std::byte *dst = tmp + ((nh - 1 - x) * nw + y) * components;
            for (u32 c = 0; c < components; ++c)
                dst[c] = src[c];
        }

    TKit::Deallocate(data);
    data = tmp;
    width = nw;
    height = nh;
}

void ImageData::Manipulate(const ImageOperation op)
{
    switch (op)
    {
    case ImageOperation_FlipX:
        flipX(Data, Width, Height, Components);
        break;
    case ImageOperation_FlipY:
        flipY(Data, Width, Height, Components);
        break;
    case ImageOperation_Rotate90CW:
        rotate90CW(Data, Width, Height, Components, ComputeSize());
        break;
    case ImageOperation_Rotate90CCW:
        rotate90CCW(Data, Width, Height, Components, ComputeSize());
        break;
    case ImageOperation_Rotate180:
        flipX(Data, Width, Height, Components);
        flipY(Data, Width, Height, Components);
        break;
    }
}

#ifdef ONYX_ENABLE_IMAGE_LOAD
Result<ImageData> LoadImageDataFromFile(const char *path, const ImageComponentFormat requiredComponents,
                                        const LoadImageDataFlags flags)
{
    ImageData data{};
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
        return Result<ImageData>::Error(
            Error_LoadFailed, TKit::Format("[ONYX][IMAGE] Failed to load image data: {}", stbi_failure_reason()));

    data.Width = u32(w);
    data.Height = u32(h);
    data.Components = requiredComponents > 0 ? requiredComponents : u32(c);
    data.Format = Detail::GetFormat(i32(data.Components), type, !(flags & LoadImageDataFlag_AsLinearImage));
    const usz size = data.ComputeSize();

    data.Data = scast<std::byte *>(TKit::Allocate(size));
    TKIT_ASSERT(data.Data, "[ONYX][IMAGE] Failed to allocate image data with size {:L} bytes", size);
    TKit::ForwardCopy(data.Data, img, size);
    stbi_image_free(img);
    return data;
}
#endif
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

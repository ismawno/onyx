#include "pch.hpp"
#include "onyx/resource/image.hpp"
#include "conversion.hpp"
#include "vkit/resource/device_image.hpp"
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

usz ImageData::ComputeSize() const
{
    return Width * Height * VKit::DeviceImage::GetBytesPerPixel(AsVulkanFormat(Format));
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
struct ImageArgs
{
    ImageComponentFormat RequiredComponents;
    LoadImageDataFlags Flags;
    const char *Path = nullptr;
    const std::byte *Memory = nullptr;
    const u32 Size = 0;
};

ONYX_NO_DISCARD static Result<ImageData> load(const ImageArgs &args)
{
    ImageData data{};
    i32 w;
    i32 h;
    i32 c;

    ImageComponentType type;
    void *img = nullptr;

    const ImageComponentFormat requiredComponents = args.RequiredComponents;
    const LoadImageDataFlags flags = args.Flags;
    if (args.Path)
    {
        const char *path = args.Path;
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
    }
    else
    {
        const stbi_uc *mem = rcast<const stbi_uc *>(args.Memory);
        const i32 len = i32(args.Size);
        if (stbi_is_hdr_from_memory(mem, len))
        {
            type = ImageComponent_Float;
            img = stbi_loadf_from_memory(mem, len, &w, &h, &c, i32(requiredComponents));
        }
        else if (stbi_is_16_bit_from_memory(mem, len))
        {
            type = ImageComponent_UnsignedShort;
            img = stbi_load_16_from_memory(mem, len, &w, &h, &c, i32(requiredComponents));
        }
        else
        {
            type = ImageComponent_UnsignedByte;
            img = stbi_load_from_memory(mem, len, &w, &h, &c, i32(requiredComponents));
        }
    }

    if (!img)
        return Result<ImageData>::Error(
            Error_LoadFailed, TKit::Format("[ONYX][IMAGE] Failed to load image data: {}", stbi_failure_reason()));

    data.Width = u32(w);
    data.Height = u32(h);
    data.Components = requiredComponents > 0 ? requiredComponents : u32(c);
    data.Format = GetFormat(i32(data.Components), type, !(flags & LoadImageDataFlag_AsLinearImage));
    const usz sz = data.ComputeSize();

    data.Data = scast<std::byte *>(TKit::Allocate(sz));
    TKIT_ASSERT(data.Data, "[ONYX][IMAGE] Failed to allocate image data with size {:L} bytes", sz);
    TKit::ForwardCopy(data.Data, img, sz);
    stbi_image_free(img);
    return data;
}

Result<ImageData> LoadImageDataFromFile(const char *path, const ImageComponentFormat requiredComponents,
                                        const LoadImageDataFlags flags)
{
    return load({.RequiredComponents = requiredComponents, .Flags = flags, .Path = path});
}
Result<ImageData> LoadImageDataFromMemory(const std::byte *memory, const u32 size,
                                          const ImageComponentFormat requiredComponents, const LoadImageDataFlags flags)
{
    return load({.RequiredComponents = requiredComponents, .Flags = flags, .Memory = memory, .Size = size});
}
#endif
} // namespace Onyx

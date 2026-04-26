#include "onyx/core/pch.hpp"
#include "onyx/asset/gltf.hpp"
TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_CLANG_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_CLANG_WARNING_IGNORE("-Wmissing-field-initializers")
TKIT_GCC_WARNING_IGNORE("-Wdeprecated-literal-operator")
TKIT_GCC_WARNING_IGNORE("-Wmissing-field-initializers")
#include <tiny_gltf.h>
TKIT_COMPILER_WARNING_IGNORE_POP()

namespace Onyx
{
static ImageComponentType getComponentType(const i32 pixelType)
{
    switch (pixelType)
    {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        return ImageComponent_SignedByte;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return ImageComponent_UnsignedByte;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        return ImageComponent_SignedShort;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return ImageComponent_UnsignedShort;
    case TINYGLTF_COMPONENT_TYPE_INT:
        return ImageComponent_SignedInteger;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return ImageComponent_UnsignedInteger;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return ImageComponent_Float;
    default:
        TKIT_FATAL("[ONYX][GLTF] Unrecognized component type {}", pixelType);
        return ImageComponent_UnsignedByte;
    }
}

template <Dimension D> Result<GltfData<D>> LoadGltfDataFromFile(const std::string &path, const LoadGltfDataFlags flags)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string warn, err;

    loader.SetPreserveImageChannels(!(flags & LoadGltfDataFlag_ForceRGBA));
    const bool ok = path.ends_with(".bin") || path.ends_with(".glb")
                        ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
                        : loader.LoadASCIIFromFile(&model, &err, &warn, path);
    TKIT_LOG_WARNING_IF(!warn.empty(), "[ONYX][GLTF] {}", warn);
    if (!ok)
        return Result<GltfData<D>>::Error(Error_LoadFailed, TKit::Format("[ONYX][GLTF] Failed to load gltf: {}", err));
    GltfData<D> data{};
    for (const auto &mesh : model.meshes)
    {
        std::unordered_map<i32, u32> primToMeshIndex;
        for (const auto &prim : mesh.primitives)
        {
            const i32 posAccessorIdx = prim.attributes.at("POSITION");
            if (primToMeshIndex.contains(posAccessorIdx))
                continue;

            StatMeshData<D> meshData{};

            u32 posStride;
            u32 normStride;
            u32 uvStride;
            u32 tanStride;

            const auto &posAccessor = model.accessors[posAccessorIdx];
            const auto &posView = model.bufferViews[posAccessor.bufferView];
            const auto &posBuf = model.buffers[posView.buffer];
            const f32 *positions = rcast<const f32 *>(posBuf.data.data() + posView.byteOffset + posAccessor.byteOffset);

            posStride = posView.byteStride ? posView.byteStride / sizeof(f32) : 3;
            const f32 *normals = nullptr;
            if (prim.attributes.contains("NORMAL"))
            {
                const auto &normAccessor = model.accessors[prim.attributes.at("NORMAL")];
                const auto &normView = model.bufferViews[normAccessor.bufferView];
                normals = rcast<const f32 *>(model.buffers[normView.buffer].data.data() + normView.byteOffset +
                                             normAccessor.byteOffset);
                normStride = normView.byteStride ? normView.byteStride / sizeof(f32) : 3;
            }

            const f32 *TexCoord = nullptr;
            if (prim.attributes.contains("TEXCOORD_0"))
            {
                const auto &uvAccessor = model.accessors[prim.attributes.at("TEXCOORD_0")];
                const auto &uvView = model.bufferViews[uvAccessor.bufferView];
                TexCoord = rcast<const f32 *>(model.buffers[uvView.buffer].data.data() + uvView.byteOffset +
                                               uvAccessor.byteOffset);
                uvStride = uvView.byteStride ? uvView.byteStride / sizeof(f32) : 2;
            }

            const f32 *tangents = nullptr;
            if constexpr (D == D3)
            {
                if (prim.attributes.contains("TANGENT"))
                {
                    const auto &tanAccessor = model.accessors[prim.attributes.at("TANGENT")];
                    const auto &tanView = model.bufferViews[tanAccessor.bufferView];
                    tangents = rcast<const f32 *>(model.buffers[tanView.buffer].data.data() + tanView.byteOffset +
                                                  tanAccessor.byteOffset);
                    tanStride = tanView.byteStride ? tanView.byteStride / sizeof(f32) : 4;
                }
            }

            const u32 vertexCount = u32(posAccessor.count);
            for (u32 i = 0; i < vertexCount; ++i)
            {
                StatVertex<D> vertex{};
                for (u32 j = 0; j < D; ++j)
                    vertex.Position[j] = positions[posStride * i + j];
                if constexpr (D == D3)
                {
                    if (normals)
                        for (u32 j = 0; j < 3; ++j)
                            vertex.Normal[j] = normals[normStride * i + j];
                    if (tangents)
                        for (u32 j = 0; j < 4; ++j)
                            vertex.Tangent[j] = tangents[tanStride * i + j];
                }
                if (TexCoord)
                    for (u32 j = 0; j < 2; ++j)
                        vertex.TexCoord[j] = TexCoord[uvStride * i + j];

                meshData.Vertices.Append(vertex);
            }

            const auto &idxAccessor = model.accessors[prim.indices];
            const auto &idxView = model.bufferViews[idxAccessor.bufferView];
            const u8 *idxBuf = model.buffers[idxView.buffer].data.data() + idxView.byteOffset + idxAccessor.byteOffset;
            const u32 indexCount = u32(idxAccessor.count);
            for (u32 i = 0; i < indexCount; ++i)
            {
                switch (idxAccessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    meshData.Indices.Append(Index(idxBuf[i]));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    meshData.Indices.Append(Index(rcast<const u16 *>(idxBuf)[i]));
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    meshData.Indices.Append(Index(rcast<const u32 *>(idxBuf)[i]));
                    break;
                }
            }
            data.StaticMeshes.Append(meshData);
        }
    }

    std::unordered_set<u32> srgbTexs;
    for (const auto &mat : model.materials)
    {
        const i32 albedoIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        const i32 emissiveIdx = mat.emissiveTexture.index;
        if (albedoIdx >= 0)
            srgbTexs.insert(u32(model.textures[albedoIdx].source));
        if (emissiveIdx >= 0)
            srgbTexs.insert(u32(model.textures[emissiveIdx].source));

        if constexpr (D == D2)
        {
            MaterialData<D2> matData{};
            const auto &pbr = mat.pbrMetallicRoughness;
            const auto &color = pbr.baseColorFactor;
            const u8 r = u8(color[0] * 255.f);
            const u8 g = u8(color[1] * 255.f);
            const u8 b = u8(color[2] * 255.f);
            const u8 a = u8(color[3] * 255.f);
            matData.ColorFactor = (a << 24) | (b << 16) | (g << 8) | r; // ABGR packed

            const i32 texIdx = pbr.baseColorTexture.index;
            if (texIdx >= 0)
            {
                matData.Sampler = Asset(model.textures[texIdx].sampler);
                matData.Texture = Asset(model.textures[texIdx].source);
            }

            data.Materials.Append(matData);
        }
        else
        {
            MaterialData<D3> matData{};
            const auto &pbr = mat.pbrMetallicRoughness;

            const auto &color = pbr.baseColorFactor;
            const u8 r = u8(color[0] * 255.f);
            const u8 g = u8(color[1] * 255.f);
            const u8 b = u8(color[2] * 255.f);
            const u8 a = u8(color[3] * 255.f);
            matData.AlbedoFactor = (a << 24) | (b << 16) | (g << 8) | r;

            matData.MetallicFactor = f32(pbr.metallicFactor);
            matData.RoughnessFactor = f32(pbr.roughnessFactor);
            matData.NormalScale = f32(mat.normalTexture.scale);
            matData.OcclusionStrength = f32(mat.occlusionTexture.strength);

            const auto &emissive = mat.emissiveFactor;
            matData.EmissiveFactor = f32v3{f32(emissive[0]), f32(emissive[1]), f32(emissive[2])};

            const i32 albedoIdx = pbr.baseColorTexture.index;
            if (albedoIdx >= 0)
            {
                matData.Samplers[TextureSlot_Albedo] = Asset(model.textures[albedoIdx].sampler);
                matData.Textures[TextureSlot_Albedo] = Asset(model.textures[albedoIdx].source);
            }

            const i32 mrIdx = pbr.metallicRoughnessTexture.index;
            if (mrIdx >= 0)
            {
                matData.Samplers[TextureSlot_MetallicRoughness] = Asset(model.textures[mrIdx].sampler);
                matData.Textures[TextureSlot_MetallicRoughness] = Asset(model.textures[mrIdx].source);
            }

            const i32 normalIdx = mat.normalTexture.index;
            if (normalIdx >= 0)
            {
                matData.Samplers[TextureSlot_Normal] = Asset(model.textures[normalIdx].sampler);
                matData.Textures[TextureSlot_Normal] = Asset(model.textures[normalIdx].source);
            }

            const i32 occlusionIdx = mat.occlusionTexture.index;
            if (occlusionIdx >= 0)
            {
                matData.Samplers[TextureSlot_Occlusion] = Asset(model.textures[occlusionIdx].sampler);
                matData.Textures[TextureSlot_Occlusion] = Asset(model.textures[occlusionIdx].source);
            }

            const i32 emissiveIdx = mat.emissiveTexture.index;
            if (emissiveIdx >= 0)
            {
                matData.Samplers[TextureSlot_Emissive] = Asset(model.textures[emissiveIdx].sampler);
                matData.Textures[TextureSlot_Emissive] = Asset(model.textures[emissiveIdx].source);
            }

            data.Materials.Append(matData);
        }
    }

    for (const auto &sampler : model.samplers)
    {
        SamplerData samplerData{};

        switch (sampler.minFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            samplerData.MinFilter = SamplerFilter_Nearest;
            samplerData.Mode = SamplerMode_Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            samplerData.MinFilter = SamplerFilter_Linear;
            samplerData.Mode = SamplerMode_Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            samplerData.MinFilter = SamplerFilter_Nearest;
            samplerData.Mode = SamplerMode_Linear;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        default:
            samplerData.MinFilter = SamplerFilter_Linear;
            samplerData.Mode = SamplerMode_Linear;
            break;
        }

        switch (sampler.magFilter)
        {
        case TINYGLTF_TEXTURE_FILTER_NEAREST:
            samplerData.MagFilter = SamplerFilter_Nearest;
            break;
        case TINYGLTF_TEXTURE_FILTER_LINEAR:
        default:
            samplerData.MagFilter = SamplerFilter_Linear;
            break;
        }

        const auto toWrap = [](const i32 wrap) -> SamplerWrap {
            switch (wrap)
            {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return SamplerWrap_ClampToEdge;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                return SamplerWrap_MirroredRepeat;
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
            default:
                return SamplerWrap_Repeat;
            }
        };

        samplerData.WrapU = toWrap(sampler.wrapS);
        samplerData.WrapV = toWrap(sampler.wrapT);
        data.Samplers.Append(samplerData);
    }

    for (u32 i = 0; i < u32(model.images.size()); ++i)
    {
        const auto &image = model.images[i];
        ImageData img{};
        img.Width = u32(image.width);
        img.Height = u32(image.height);
        img.Components = u32(image.component);
        img.Format = Detail::GetFormat(image.component, getComponentType(image.pixel_type), srgbTexs.contains(i));

        const usz size = img.ComputeSize();
        img.Data = scast<std::byte *>(TKit::Allocate(size));
        TKIT_ASSERT(size == image.image.size(),
                    "[ONYX][GLTF] Image size mismatch between gltf ({:L}) and computed size based "
                    "on components and "
                    "format ({:L})",
                    image.image.size(), size);
        TKIT_ASSERT(img.Data, "[ONYX][GLTF] Failed to allocate image data of size {:L} bytes", img.ComputeSize());

        TKit::ForwardCopy(img.Data, image.image.data(), size);
        data.Images.Append(img);
    }

    return data;
}

template Result<GltfData<D2>> LoadGltfDataFromFile(const std::string &path, LoadGltfDataFlags flags);
template Result<GltfData<D3>> LoadGltfDataFromFile(const std::string &path, LoadGltfDataFlags flags);
} // namespace Onyx

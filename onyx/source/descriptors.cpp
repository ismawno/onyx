#include "pch.hpp"
#include "onyx/core/definitions.hpp"
#include "onyx/core/specs.hpp"
#include "descriptors.hpp"
#include "conversion.hpp"
#include "core.hpp"

namespace Onyx::Descriptors
{
struct DescriptorData
{
    VKit::DescriptorPool Pool{};
    TKit::FixedArray<TKit::FixedArray<VKit::DescriptorSetLayout, RenderPass_Count>, D_Count> Layouts{};
    VKit::DescriptorSetLayout RayMarchLayout{};
    VKit::DescriptorSetLayout PostProcessLayout{};
    VKit::DescriptorSetLayout CompositorLayout{};
};

static TKit::Storage<DescriptorData> s_DescriptorData{};

constexpr u32 Dim2 = 0;
constexpr u32 Dim3 = 1;

static void createDescriptorData(const Specs &specs)
{
    // TODO(Isma): There are several flags with the unused while pending bit. revisit if those are really necessary

    const VKit::LogicalDevice &device = GetDevice();

    constexpr VkDescriptorType buffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    constexpr VkDescriptorType sampler = VK_DESCRIPTOR_TYPE_SAMPLER;
    constexpr VkDescriptorType sampledImage = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    constexpr VkDescriptorType combined = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    constexpr VkDescriptorType storageImage = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    constexpr VkShaderStageFlagBits compute = VK_SHADER_STAGE_COMPUTE_BIT;
    constexpr VkShaderStageFlagBits vertex = VK_SHADER_STAGE_VERTEX_BIT;
    constexpr VkShaderStageFlagBits fragment = VK_SHADER_STAGE_FRAGMENT_BIT;

    constexpr VkDescriptorBindingFlagBitsEXT pbound = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
    constexpr VkDescriptorBindingFlagBitsEXT bindUnused = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

    s_DescriptorData->Pool = ONYX_CHECK_VKIT_RESULT(VKit::DescriptorPool::Builder(device)
                                                        .SetMaxSets(specs.MaxSets)
                                                        .AddPoolSize(buffer, specs.StorageBufferPoolSize)
                                                        .AddPoolSize(sampler, specs.SamplerPoolSize)
                                                        .AddPoolSize(sampledImage, specs.SampledImagePoolSize)
                                                        .AddPoolSize(combined, specs.CombinedImageSamplerPoolSize)
                                                        .AddPoolSize(storageImage, specs.StorageImagePoolSize)
                                                        .Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Flat] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(buffer, vertex) // instance
                                   .AddBinding2(sampler, fragment, ONYX_MAX_SAMPLERS,
                                                pbound | bindUnused) // samplers
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURES,
                                                pbound | bindUnused) // textures
                                   .AddBinding2(buffer, vertex)      // bounds2
                                   .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Flat] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(buffer, vertex) // instance
                                   .AddBinding2(sampler, fragment, ONYX_MAX_SAMPLERS,
                                                pbound | bindUnused) // samplers
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURES,
                                                pbound | bindUnused) // textures
                                   .AddBinding2(buffer, vertex)      // bounds3
                                   .Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Shaded] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(buffer, vertex) // instance
                                   .AddBinding2(sampler, fragment, ONYX_MAX_SAMPLERS,
                                                pbound | bindUnused) // samplers
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURES,
                                                pbound | bindUnused) // textures
                                   .AddBinding2(buffer, vertex)      // bounds2
                                   .AddBinding2(buffer, fragment)    // materials
                                   .AddBinding2(buffer, fragment)    // point lights
                                   .AddBinding2(buffer, fragment)    // dir lights
                                   .AddBinding2(sampler, fragment)   // shadow sampler
                                   .AddBinding2(sampler, fragment)   // shadow compare sampler
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_SHADOW_OCCLUSION_MAP_SIZE,
                                                pbound | bindUnused) // shadow maps
                                   .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Shaded] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(buffer, vertex) // instance
                                   .AddBinding2(sampler, fragment, ONYX_MAX_SAMPLERS,
                                                pbound | bindUnused) // samplers
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURES,
                                                pbound | bindUnused) // textures
                                   .AddBinding2(buffer, vertex)      // bounds3
                                   .AddBinding2(buffer, fragment)    // materials
                                   .AddBinding2(buffer, fragment)    // point lights
                                   .AddBinding2(buffer, fragment)    // dir lights
                                   .AddBinding2(sampler, fragment)   // shadow sampler
                                   .AddBinding2(sampler, fragment)   // shadow compare sampler
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS,
                                                pbound | bindUnused) // point maps
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS,
                                                pbound | bindUnused) // directional maps
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS,
                                                pbound | bindUnused) // spot maps
                                   .AddBinding2(buffer, fragment)    // spot lights
                                   .Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Shadow] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(buffer, vertex) // instance
                                   .AddBinding2(sampler, fragment, ONYX_MAX_SAMPLERS,
                                                pbound | bindUnused) // samplers
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURES,
                                                pbound | bindUnused) // textures
                                   .AddBinding2(buffer, vertex)      // bounds2
                                   .AddBinding2(buffer, fragment)    // materials
                                   .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Shadow] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(buffer, vertex) // instance
                                   .AddBinding2(sampler, fragment, ONYX_MAX_SAMPLERS,
                                                pbound | bindUnused) // samplers
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_TEXTURES,
                                                pbound | bindUnused) // textures
                                   .AddBinding2(buffer, vertex)      // bounds3
                                   .Build());

    s_DescriptorData->RayMarchLayout =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(storageImage, compute, ONYX_MAX_SHADOW_OCCLUSION_MAP_SIZE,
                                                pbound | bindUnused) // occlusion maps
                                   .AddBinding2(storageImage, compute, ONYX_MAX_SHADOW_OCCLUSION_MAP_SIZE,
                                                pbound | bindUnused) // shadow maps
                                   .Build());

    s_DescriptorData->PostProcessLayout =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(combined, fragment, ONYX_MAX_ATTACHMENTS,
                                                pbound | bindUnused) // color attachments
                                   .AddBinding2(combined, fragment, ONYX_MAX_ATTACHMENTS,
                                                pbound | bindUnused) // outline attachments
                                   .AddBinding2(sampledImage, fragment, ONYX_MAX_ATTACHMENTS,
                                                pbound | bindUnused) // stencil attachments
                                   .Build());

    s_DescriptorData->CompositorLayout =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder(device)
                                   .AddBinding2(combined, fragment, ONYX_MAX_ATTACHMENTS,
                                                pbound | bindUnused) // color attachments
                                   .Build());

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->Pool.SetName("onyx-descriptor-pool"));
        u32 i = 2;
        for (auto &dims : s_DescriptorData->Layouts)
        {
            u32 j = 0;
            for (auto &renders : dims)
            {
                ONYX_CHECK_VKIT_RESULT(renders.SetName(
                    TKit::Format("onyx-descriptor-set-layout-{}D-{}", i, ToString(RenderPass(j++))).c_str()));
            }
            ++i;
        }
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->RayMarchLayout.SetName("onyx-ray-march-descriptor-set-layout"));
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->PostProcessLayout.SetName("onyx-post-process-descriptor-set-layout"));
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->CompositorLayout.SetName("onyx-compositor-descriptor-set-layout"));
    }
}

void Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorData.Construct();
    return createDescriptorData(specs);
}
void Terminate()
{
    s_DescriptorData->Pool.Destroy();
    for (auto &dims : s_DescriptorData->Layouts)
        for (auto &renders : dims)
            renders.Destroy();

    s_DescriptorData->RayMarchLayout.Destroy();
    s_DescriptorData->PostProcessLayout.Destroy();
    s_DescriptorData->CompositorLayout.Destroy();
    s_DescriptorData.Destruct();
}

const VKit::DescriptorPool &GetDescriptorPool()
{
    return s_DescriptorData->Pool;
}

template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorLayout(const RenderPass pass)
{
    return s_DescriptorData->Layouts[D - 2][pass];
}
const VKit::DescriptorSetLayout &GetRayMarchDescriptorLayout()
{
    return s_DescriptorData->RayMarchLayout;
}
const VKit::DescriptorSetLayout &GetPostProcessDescriptorLayout()
{
    return s_DescriptorData->PostProcessLayout;
}
const VKit::DescriptorSetLayout &GetCompositorDescriptorLayout()
{
    return s_DescriptorData->CompositorLayout;
}

template <Dimension D>
void BindBuffer(const u32 binding, const TKit::Span<const VkDescriptorSet> sets,
                TKit::Span<const VkDescriptorBufferInfo> info, const RenderPass pass, const u32 dstElement)
{
    VKit::DescriptorSet::Writer writer{GetDevice(), &GetDescriptorLayout<D>(pass)};
    writer.WriteBuffer(binding, info, dstElement);
    for (const VkDescriptorSet set : sets)
        writer.Overwrite(set);
}
template <Dimension D>
void BindImage(const u32 binding, const TKit::Span<const VkDescriptorSet> sets,
               TKit::Span<const VkDescriptorImageInfo> info, const RenderPass pass, const u32 dstElement)
{
    VKit::DescriptorSet::Writer writer{GetDevice(), &GetDescriptorLayout<D>(pass)};
    writer.WriteImage(binding, info, dstElement);
    for (const VkDescriptorSet set : sets)
        writer.Overwrite(set);
}

template const VKit::DescriptorSetLayout &GetDescriptorLayout<D2>(RenderPass pass);
template const VKit::DescriptorSetLayout &GetDescriptorLayout<D3>(RenderPass pass);

template void BindBuffer<D2>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                             TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement);
template void BindBuffer<D3>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                             TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement);

template void BindImage<D2>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                            TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement);
template void BindImage<D3>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                            TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement);
} // namespace Onyx::Descriptors

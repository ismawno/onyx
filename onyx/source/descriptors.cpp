#include "pch.hpp"
#include "onyx/definitions.hpp"
#include "onyx/specs.hpp"
#include "descriptors.hpp"
#include "core.hpp"
#include "conversion.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx::Descriptors
{
struct DescriptorData
{
    VKit::DescriptorPool Pool{};
    TKit::FixedArray<TKit::FixedArray<VKit::DescriptorSetLayout, RenderPass_Count>, D_Count> Layouts{};
    TKit::FixedArray<VKit::DescriptorSetLayout, StandalonePass_Count> StandaloneLayouts{};
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

    constexpr u32 instances = ONYX_INSTANCES_BINDING_POINT;
    constexpr u32 samplers = ONYX_SAMPLERS_BINDING_POINT;
    constexpr u32 textures = ONYX_TEXTURES_BINDING_POINT;
    constexpr u32 textureOffsets = ONYX_TEXTURE_OFFSETS_BINDING_POINT;
    constexpr u32 bounds = ONYX_BOUNDS_BINDING_POINT;
    constexpr u32 materials = ONYX_MATERIALS_BINDING_POINT;
    constexpr u32 pointLights = ONYX_POINT_LIGHTS_BINDING_POINT;
    constexpr u32 directionalLights = ONYX_DIRECTIONAL_LIGHTS_BINDING_POINT;
    constexpr u32 shadowSampler = ONYX_SHADOW_SAMPLER_BINDING_POINT;
    constexpr u32 shadowCompareSampler = ONYX_SHADOW_COMPARE_SAMPLER_BINDING_POINT;
    constexpr u32 shadowMaps = ONYX_SHADOW_MAPS_BINDING_POINT;
    constexpr u32 pointMaps = ONYX_POINT_MAPS_BINDING_POINT;
    constexpr u32 directionalMaps = ONYX_DIRECTIONAL_MAPS_BINDING_POINT;
    constexpr u32 spotMaps = ONYX_SPOT_MAPS_BINDING_POINT;
    constexpr u32 spotLights = ONYX_SPOT_LIGHTS_BINDING_POINT;
    constexpr u32 occlusionMap = ONYX_OCCLUSION_MAP_BINDING_POINT;
    constexpr u32 rayMarchMap = ONYX_RAY_MARCH_MAP_BINDING_POINT;
    constexpr u32 blendTransparentAttachments = ONYX_BLEND_TRANSPARENT_ATTACHMENTS_BINDING;
    constexpr u32 blendRevealageAttachments = ONYX_BLEND_REVEALAGE_ATTACHMENTS_BINDING;
    constexpr u32 postProcessColorAttachments = ONYX_POST_PROCESS_COLOR_ATTACHMENTS_BINDING;
    constexpr u32 postProcessOutlineAttachments = ONYX_POST_PROCESS_OUTLINE_ATTACHMENTS_BINDING;
    constexpr u32 postProcessStencilAttachments = ONYX_POST_PROCESS_STENCIL_ATTACHMENTS_BINDING;
    constexpr u32 compositorColorAttachments = ONYX_COMPOSITOR_COLOR_ATTACHMENTS_BINDING;

    s_DescriptorData->Pool = ONYX_CHECK_VKIT_RESULT(VKit::DescriptorPool::Builder(device)
                                                        .SetMaxSets(specs.MaxSets)
                                                        .AddPoolSize(buffer, specs.StorageBufferPoolSize)
                                                        .AddPoolSize(sampler, specs.SamplerPoolSize)
                                                        .AddPoolSize(sampledImage, specs.SampledImagePoolSize)
                                                        .AddPoolSize(combined, specs.CombinedImageSamplerPoolSize)
                                                        .AddPoolSize(storageImage, specs.StorageImagePoolSize)
                                                        .Build());

    VKit::DescriptorSetLayout::Builder flatLayout{device};
    flatLayout.AddBinding2(instances, buffer, vertex | fragment)
        .AddBinding2(samplers, sampler, fragment, ONYX_MAX_SAMPLERS, pbound | bindUnused)
        .AddBinding2(textures, sampledImage, fragment, ONYX_MAX_TEXTURES, pbound | bindUnused)
        .AddBinding2(textureOffsets, buffer, fragment)
        .AddBinding2(bounds, buffer, vertex);

    s_DescriptorData->Layouts[Dim2][RenderPass_Flat] = ONYX_CHECK_VKIT_RESULT(flatLayout.Build());
    s_DescriptorData->Layouts[Dim3][RenderPass_Flat] = ONYX_CHECK_VKIT_RESULT(flatLayout.Build());

    VKit::DescriptorSetLayout::Builder shadedLayout{device};
    shadedLayout.AddBinding2(instances, buffer, vertex | fragment)
        .AddBinding2(samplers, sampler, fragment, ONYX_MAX_SAMPLERS, pbound | bindUnused)
        .AddBinding2(textures, sampledImage, fragment, ONYX_MAX_TEXTURES, pbound | bindUnused)
        .AddBinding2(textureOffsets, buffer, fragment)
        .AddBinding2(bounds, buffer, vertex)
        .AddBinding2(materials, buffer, fragment)
        .AddBinding2(pointLights, buffer, fragment)
        .AddBinding2(directionalLights, buffer, fragment)
        .AddBinding2(shadowSampler, sampler, fragment)
        .AddBinding2(shadowCompareSampler, sampler, fragment);

    // we perform a copy to not modify shaded layout
    s_DescriptorData->Layouts[Dim2][RenderPass_Shaded] =
        ONYX_CHECK_VKIT_RESULT(VKit::DescriptorSetLayout::Builder{shadedLayout}
                                   .AddBinding2(shadowMaps, sampledImage, fragment,
                                                ONYX_MAX_RAY_MARCH_AND_OCCLUSION_MAP_SIZE, pbound | bindUnused)
                                   .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Shaded] = ONYX_CHECK_VKIT_RESULT(
        shadedLayout.AddBinding2(pointMaps, sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS, pbound | bindUnused)
            .AddBinding2(directionalMaps, sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS, pbound | bindUnused)
            .AddBinding2(spotMaps, sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS, pbound | bindUnused)
            .AddBinding2(spotLights, buffer, fragment)
            .Build());

    VKit::DescriptorSetLayout::Builder shadowLayout{device};
    shadowLayout.AddBinding2(instances, buffer, vertex | fragment)
        .AddBinding2(samplers, sampler, fragment, ONYX_MAX_SAMPLERS, pbound | bindUnused)
        .AddBinding2(textures, sampledImage, fragment, ONYX_MAX_TEXTURES, pbound | bindUnused)
        .AddBinding2(bounds, buffer, vertex);

    s_DescriptorData->Layouts[Dim3][RenderPass_Shadow] = ONYX_CHECK_VKIT_RESULT(shadowLayout.Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Shadow] =
        ONYX_CHECK_VKIT_RESULT(shadowLayout.AddBinding2(materials, buffer, fragment).Build());

    // NOTE(Isma): Removed bind unused here
    s_DescriptorData->StandaloneLayouts[StandalonePass_RayMarch] = ONYX_CHECK_VKIT_RESULT(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(occlusionMap, storageImage, compute, ONYX_MAX_RAY_MARCH_AND_OCCLUSION_MAP_SIZE, pbound)
            .AddBinding2(rayMarchMap, storageImage, compute, ONYX_MAX_RAY_MARCH_AND_OCCLUSION_MAP_SIZE, pbound)
            .Build());

    s_DescriptorData->StandaloneLayouts[StandalonePass_Blend] = ONYX_CHECK_VKIT_RESULT(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(blendTransparentAttachments, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound)
            .AddBinding2(blendRevealageAttachments, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound)
            .Build());

    s_DescriptorData->StandaloneLayouts[StandalonePass_PostProcess] = ONYX_CHECK_VKIT_RESULT(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(postProcessColorAttachments, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound)
            .AddBinding2(postProcessOutlineAttachments, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound)
            .AddBinding2(postProcessStencilAttachments, sampledImage, fragment, ONYX_MAX_ATTACHMENTS, pbound)
            .Build());

    s_DescriptorData->StandaloneLayouts[StandalonePass_Compositor] = ONYX_CHECK_VKIT_RESULT(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(compositorColorAttachments, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound | bindUnused)
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
                    TKit::StackString::Format("onyx-descriptor-set-layout-{}D-{}", i, ToString(RenderPass(j++)))
                        .CString()));
            }
            ++i;
        }
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->StandaloneLayouts[StandalonePass_RayMarch].SetName(
            "onyx-ray-march-descriptor-set-layout"));
        ONYX_CHECK_VKIT_RESULT(
            s_DescriptorData->StandaloneLayouts[StandalonePass_Blend].SetName("onyx-blend-descriptor-set-layout"));
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->StandaloneLayouts[StandalonePass_PostProcess].SetName(
            "onyx-post-process-descriptor-set-layout"));
        ONYX_CHECK_VKIT_RESULT(s_DescriptorData->StandaloneLayouts[StandalonePass_Compositor].SetName(
            "onyx-compositor-descriptor-set-layout"));
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

    for (auto &layout : s_DescriptorData->StandaloneLayouts)
        layout.Destroy();
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
const VKit::DescriptorSetLayout &GetDescriptorLayout(const StandalonePass pass)
{
    return s_DescriptorData->StandaloneLayouts[pass];
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

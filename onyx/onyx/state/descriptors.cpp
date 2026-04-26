#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"

namespace Onyx::Descriptors
{
struct DescriptorData
{
    VKit::DescriptorPool Pool{};
    TKit::FixedArray<TKit::FixedArray<VKit::DescriptorSetLayout, RenderPass_Count>, D_Count> Layouts{};
    VKit::DescriptorSetLayout DistanceLayout{};
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
    s_DescriptorData->Pool = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorPool::Builder(device)
            .SetMaxSets(specs.MaxSets)
            .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, specs.StorageBufferPoolSize)
            .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, specs.SamplerPoolSize)
            .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, specs.SampledImagePoolSize)
            .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, specs.CombinedImageSamplerPoolSize)
            .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, specs.StorageImagePoolSize)
            .Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Flat] = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                 // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds2
            .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Flat] = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                 // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds3
            .Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Shaded] = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                   // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)   // bounds2
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // materials
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)        // shadow sampler
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURE_MAPS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // shadow maps
            .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Shaded] = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                   // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)   // bounds3
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // materials
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)        // shadow sampler
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURE_MAPS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // point maps
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURE_MAPS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT)   // directional maps
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // dir lights
            .Build());

    s_DescriptorData->Layouts[Dim2][RenderPass_Shadow] = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                   // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)   // bounds2
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // materials
            .Build());

    s_DescriptorData->Layouts[Dim3][RenderPass_Shadow] = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                 // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds3
            .Build());

    s_DescriptorData->DistanceLayout = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, ONYX_MAX_TEXTURE_MAPS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // occlusion maps
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, ONYX_MAX_TEXTURE_MAPS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // shadow maps
            .Build());

    s_DescriptorData->PostProcessLayout = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_ATTACHMENTS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // color attachments
            .AddBinding2(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_ATTACHMENTS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // outline attachments
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_ATTACHMENTS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // stencil attachments
            .Build());

    s_DescriptorData->CompositorLayout = ONYX_CHECK_EXPRESSION(
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_ATTACHMENTS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT) // color attachments
            .Build());

    if (IsDebugUtilsEnabled())
    {
        ONYX_CHECK_EXPRESSION(s_DescriptorData->Pool.SetName("onyx-descriptor-pool"));
        u32 i = 2;
        for (auto &dims : s_DescriptorData->Layouts)
        {
            u32 j = 0;
            for (auto &renders : dims)
            {
                ONYX_CHECK_EXPRESSION(renders.SetName(
                    TKit::Format("onyx-descriptor-set-layout-{}D-{}", i, ToString(RenderPass(j++))).c_str()));
            }
            ++i;
        }
        ONYX_CHECK_EXPRESSION(s_DescriptorData->DistanceLayout.SetName("onyx-distance-descriptor-set-layout"));
        ONYX_CHECK_EXPRESSION(s_DescriptorData->PostProcessLayout.SetName("onyx-post-process-descriptor-set-layout"));
        ONYX_CHECK_EXPRESSION(s_DescriptorData->CompositorLayout.SetName("onyx-compositor-descriptor-set-layout"));
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

    s_DescriptorData->DistanceLayout.Destroy();
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
const VKit::DescriptorSetLayout &GetDistanceDescriptorLayout()
{
    return s_DescriptorData->DistanceLayout;
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
                             TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement = 0);
template void BindBuffer<D3>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                             TKit::Span<const VkDescriptorBufferInfo> info, RenderPass pass, u32 dstElement = 0);

template void BindImage<D2>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                            TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement = 0);
template void BindImage<D3>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                            TKit::Span<const VkDescriptorImageInfo> info, RenderPass pass, u32 dstElement = 0);
} // namespace Onyx::Descriptors

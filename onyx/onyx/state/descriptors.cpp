#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"

namespace Onyx::Descriptors
{
static TKit::Storage<VKit::DescriptorPool> s_DescriptorPool{};
static TKit::Storage<VKit::DescriptorSetLayout> s_StencilDescLayout2{};
static TKit::Storage<VKit::DescriptorSetLayout> s_StencilDescLayout3{};
static TKit::Storage<VKit::DescriptorSetLayout> s_FillDescLayout2{};
static TKit::Storage<VKit::DescriptorSetLayout> s_FillDescLayout3{};

ONYX_NO_DISCARD static Result<> createDescriptorData(const Specs &specs)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto poolResult = VKit::DescriptorPool::Builder(device)
                                .SetMaxSets(specs.MaxSets)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, specs.StorageBufferPoolSize)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, specs.SamplerPoolSize)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, specs.SampledImagePoolSize)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, specs.SampledImagePoolSize)
                                .Build();

    TKIT_RETURN_ON_ERROR(poolResult);
    *s_DescriptorPool = poolResult.GetValue();

    auto layoutResult =
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds2
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // textures
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_StencilDescLayout2 = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds2
                       .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
                       .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                 // textures
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds3
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_StencilDescLayout3 = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds2
                       .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
                       .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                   // textures
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // materials
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_FillDescLayout2 = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // bounds2
                       .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
                       .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)                   // textures
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)   // bounds3
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // materials
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
                       .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // dir lights
                       .Build();

    *s_FillDescLayout3 = layoutResult.GetValue();

    if (Core::CanNameObjects())
    {
        TKIT_RETURN_IF_FAILED(s_DescriptorPool->SetName("onyx-descriptor-pool"));
        TKIT_RETURN_IF_FAILED(s_StencilDescLayout2->SetName("onyx-stencil-descriptor-set-layout-2D"));
        TKIT_RETURN_IF_FAILED(s_StencilDescLayout3->SetName("onyx-stencil-descriptor-set-layout-3D"));
        TKIT_RETURN_IF_FAILED(s_FillDescLayout2->SetName("onyx-fill-descriptor-set-layout-2D"));
        return s_FillDescLayout3->SetName("onyx-fill-descriptor-set-layout-3D");
    }

    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorPool.Construct();
    s_StencilDescLayout2.Construct();
    s_StencilDescLayout3.Construct();
    s_FillDescLayout2.Construct();
    s_FillDescLayout3.Construct();
    return createDescriptorData(specs);
}
void Terminate()
{
    s_DescriptorPool->Destroy();
    s_StencilDescLayout2->Destroy();
    s_StencilDescLayout3->Destroy();
    s_FillDescLayout2->Destroy();
    s_FillDescLayout3->Destroy();

    s_DescriptorPool.Destruct();
    s_StencilDescLayout2.Destruct();
    s_StencilDescLayout3.Destruct();
    s_FillDescLayout2.Destruct();
    s_FillDescLayout3.Destruct();
}

const VKit::DescriptorPool &GetDescriptorPool()
{
    return *s_DescriptorPool;
}

template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(const DrawPass pass)
{
    if constexpr (D == D2)
        return pass == DrawPass_Fill ? *s_FillDescLayout2 : *s_StencilDescLayout2;
    else
        return pass == DrawPass_Fill ? *s_FillDescLayout3 : *s_StencilDescLayout3;
}

template <Dimension D>
void WriteBuffer(const u32 binding, const TKit::Span<const VkDescriptorSet> sets,
                 TKit::Span<const VkDescriptorBufferInfo> info, const DrawPass pass, const u32 dstElement)
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &GetDescriptorSetLayout<D>(pass)};
    writer.WriteBuffer(binding, info, dstElement);
    for (const VkDescriptorSet set : sets)
        writer.Overwrite(set);
}
template <Dimension D>
void WriteImage(const u32 binding, const TKit::Span<const VkDescriptorSet> sets,
                TKit::Span<const VkDescriptorImageInfo> info, const DrawPass pass, const u32 dstElement)
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &GetDescriptorSetLayout<D>(pass)};
    writer.WriteImage(binding, info, dstElement);
    for (const VkDescriptorSet set : sets)
        writer.Overwrite(set);
}

template const VKit::DescriptorSetLayout &GetDescriptorSetLayout<D2>(DrawPass pass);
template const VKit::DescriptorSetLayout &GetDescriptorSetLayout<D3>(DrawPass pass);

template void WriteBuffer<D2>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                              TKit::Span<const VkDescriptorBufferInfo> info, DrawPass pass, u32 dstElement = 0);
template void WriteBuffer<D3>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                              TKit::Span<const VkDescriptorBufferInfo> info, DrawPass pass, u32 dstElement = 0);

template void WriteImage<D2>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                             TKit::Span<const VkDescriptorImageInfo> info, DrawPass pass, u32 dstElement = 0);
template void WriteImage<D3>(u32 binding, TKit::Span<const VkDescriptorSet> sets,
                             TKit::Span<const VkDescriptorImageInfo> info, DrawPass pass, u32 dstElement = 0);
} // namespace Onyx::Descriptors

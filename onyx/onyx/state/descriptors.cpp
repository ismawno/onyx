#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/asset/handle.hpp"

namespace Onyx::Descriptors
{
static TKit::Storage<VKit::DescriptorPool> s_DescriptorPool{};
static TKit::Storage<VKit::DescriptorSetLayout> s_StencilDescLayout{};
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
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // textures
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_StencilDescLayout = layoutResult.GetValue();

    layoutResult =
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_ASSET_POOLS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT)       // materials
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_FillDescLayout2 = layoutResult.GetValue();

    layoutResult =
        VKit::DescriptorSetLayout::Builder(device)
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_SAMPLERS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // samplers
            .AddBinding2(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_TEXTURES,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT) // textures
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, ONYX_MAX_ASSET_POOLS,
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT)       // materials
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
            .AddBinding2(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // dir lights
            .Build();

    *s_FillDescLayout3 = layoutResult.GetValue();

    if (Core::CanNameObjects())
    {
        TKIT_RETURN_IF_FAILED(s_DescriptorPool->SetName("onyx-descriptor-pool"));
        TKIT_RETURN_IF_FAILED(s_StencilDescLayout->SetName("onyx-stencil-descriptor-set-layout"));
        TKIT_RETURN_IF_FAILED(s_FillDescLayout2->SetName("onyx-fill-descriptor-set-layout-2D"));
        return s_FillDescLayout3->SetName("onyx-fill-descriptor-set-layout-3D");
    }

    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorPool.Construct();
    s_StencilDescLayout.Construct();
    s_FillDescLayout2.Construct();
    s_FillDescLayout3.Construct();
    return createDescriptorData(specs);
}
void Terminate()
{
    s_DescriptorPool->Destroy();
    s_StencilDescLayout->Destroy();
    s_FillDescLayout2->Destroy();
    s_FillDescLayout3->Destroy();

    s_DescriptorPool.Destruct();
    s_StencilDescLayout.Destruct();
    s_FillDescLayout2.Destruct();
    s_FillDescLayout3.Destruct();
}

const VKit::DescriptorPool &GetDescriptorPool()
{
    return *s_DescriptorPool;
}

const VKit::DescriptorSetLayout &GetStencilDescriptorSetLayout()
{
    return *s_StencilDescLayout;
}

template <Dimension D> const VKit::DescriptorSetLayout &GetFillDescriptorSetLayout()
{
    if constexpr (D == D2)
        return *s_FillDescLayout2;
    else
        return *s_FillDescLayout3;
}

template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(const DrawPass pass)
{
    if (pass == DrawPass_Stencil)
        return *s_StencilDescLayout;
    if constexpr (D == D2)
        return *s_FillDescLayout2;
    else
        return *s_FillDescLayout3;
}

template const VKit::DescriptorSetLayout &GetFillDescriptorSetLayout<D2>();
template const VKit::DescriptorSetLayout &GetFillDescriptorSetLayout<D3>();

template const VKit::DescriptorSetLayout &GetDescriptorSetLayout<D2>(DrawPass pass);
template const VKit::DescriptorSetLayout &GetDescriptorSetLayout<D3>(DrawPass pass);
} // namespace Onyx::Descriptors

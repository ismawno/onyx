#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"

namespace Onyx::Descriptors
{
static TKit::Storage<VKit::DescriptorPool> s_DescriptorPool{};
static TKit::Storage<VKit::DescriptorSetLayout> s_UnlitDescLayout{};
static TKit::Storage<VKit::DescriptorSetLayout> s_LitDescLayout2{};
static TKit::Storage<VKit::DescriptorSetLayout> s_LitDescLayout3{};

ONYX_NO_DISCARD static Result<> createDescriptorData(const Specs &specs)
{
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto poolResult = VKit::DescriptorPool::Builder(device)
                                .SetMaxSets(specs.MaxSets)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, specs.PoolSize)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, specs.PoolSize)
                                .Build();
    TKIT_RETURN_ON_ERROR(poolResult);
    *s_DescriptorPool = poolResult.GetValue();

    auto layoutResult = VKit::DescriptorSetLayout::Builder(device)
                            .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // instance
                            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_UnlitDescLayout = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)   // instance
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // ambient lights
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_LitDescLayout2 = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)   // instance
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // point lights
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // ambient lights
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // dir lights
                       .Build();

    *s_LitDescLayout3 = layoutResult.GetValue();

    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorPool.Construct();
    s_UnlitDescLayout.Construct();
    s_LitDescLayout2.Construct();
    s_LitDescLayout3.Construct();
    return createDescriptorData(specs);
}
void Terminate()
{
    s_DescriptorPool->Destroy();
    s_UnlitDescLayout->Destroy();
    s_LitDescLayout2->Destroy();
    s_LitDescLayout3->Destroy();

    s_DescriptorPool.Destruct();
    s_UnlitDescLayout.Destruct();
    s_LitDescLayout2.Destruct();
    s_LitDescLayout3.Destruct();
}

const VKit::DescriptorPool &GetDescriptorPool()
{
    return *s_DescriptorPool;
}

const VKit::DescriptorSetLayout &GetUnlitDescriptorSetLayout()
{
    return *s_UnlitDescLayout;
}

template <Dimension D> const VKit::DescriptorSetLayout &GetLitDescriptorSetLayout()
{
    if constexpr (D == D2)
        return *s_LitDescLayout2;
    else
        return *s_LitDescLayout3;
}

template <Dimension D> const VKit::DescriptorSetLayout &GetDescriptorSetLayout(const Shading shading)
{
    if (shading == Shading_Unlit)
        return *s_UnlitDescLayout;
    if constexpr (D == D2)
        return *s_LitDescLayout2;
    else
        return *s_LitDescLayout3;
}

template const VKit::DescriptorSetLayout &GetLitDescriptorSetLayout<D2>();
template const VKit::DescriptorSetLayout &GetLitDescriptorSetLayout<D3>();

template const VKit::DescriptorSetLayout &GetDescriptorSetLayout<D2>(Shading shading);
template const VKit::DescriptorSetLayout &GetDescriptorSetLayout<D3>(Shading shading);
} // namespace Onyx::Descriptors

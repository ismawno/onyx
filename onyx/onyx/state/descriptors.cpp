#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/core/core.hpp"
#include "vkit/resource/device_buffer.hpp"

namespace Onyx::Descriptors
{

static VKit::DescriptorPool s_DescriptorPool{};
static VKit::DescriptorSetLayout s_InstanceDataStorageLayout{};
static VKit::DescriptorSetLayout s_LightStorageLayout{};
static TKit::ArenaArray<DescriptorSet> s_Sets{};

static void createDescriptorData(const Specs &p_Specs)
{
    TKIT_LOG_INFO("[ONYX] Creating assets descriptor data");
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto poolResult = VKit::DescriptorPool::Builder(device)
                                .SetMaxSets(p_Specs.MaxSets)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, p_Specs.PoolSize)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, p_Specs.PoolSize)
                                .Build();

    VKIT_CHECK_RESULT(poolResult);
    s_DescriptorPool = poolResult.GetValue();

    auto layoutResult = VKit::DescriptorSetLayout::Builder(device)
                            .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    VKIT_CHECK_RESULT(layoutResult);
    s_InstanceDataStorageLayout = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    VKIT_CHECK_RESULT(layoutResult);
    s_LightStorageLayout = layoutResult.GetValue();
}

void Initialize(const Specs &p_Specs)
{
    s_Sets.Reserve(p_Specs.MaxSets);
    createDescriptorData(p_Specs);
}
void Terminate()
{
    s_DescriptorPool.Destroy();
    s_InstanceDataStorageLayout.Destroy();
    s_LightStorageLayout.Destroy();
}

DescriptorSet &FindSuitableDescriptorSet(const VKit::DeviceBuffer &p_Buffer)
{
    for (DescriptorSet &set : s_Sets)
        if (!set.InUse())
        {
            if (set.Buffer == p_Buffer)
                return set;
            const VkDescriptorBufferInfo info = p_Buffer.CreateDescriptorInfo();
            set.Set = WriteStorageBufferDescriptorSet(info, set.Set);
            set.Buffer = p_Buffer;
        }

    DescriptorSet &set = s_Sets.Append();
    const VkDescriptorBufferInfo info = p_Buffer.CreateDescriptorInfo();
    set.Set = WriteStorageBufferDescriptorSet(info);
    set.Buffer = p_Buffer;
    return set;
}

const VKit::DescriptorPool &GetDescriptorPool()
{
    return s_DescriptorPool;
}
const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout()
{
    return s_InstanceDataStorageLayout;
}
const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout()
{
    return s_LightStorageLayout;
}

VkDescriptorSet WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &p_Info, VkDescriptorSet p_OldSet)
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &s_InstanceDataStorageLayout};
    writer.WriteBuffer(0, p_Info);

    if (!p_OldSet)
    {
        const auto result = s_DescriptorPool.Allocate(s_InstanceDataStorageLayout);
        VKIT_CHECK_RESULT(result);
        p_OldSet = result.GetValue();
    }
    writer.Overwrite(p_OldSet);
    return p_OldSet;
}
} // namespace Onyx::Descriptors

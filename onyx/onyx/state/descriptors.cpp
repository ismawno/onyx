#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"
#include "vkit/resource/device_buffer.hpp"

namespace Onyx::Descriptors
{

static VKit::DescriptorPool s_DescriptorPool{};
static VKit::DescriptorSetLayout s_InstanceDataStorageLayout{};
static VKit::DescriptorSetLayout s_LightStorageLayout{};
static TKit::ArenaArray<DescriptorSet> s_Sets{};

ONYX_NO_DISCARD static Result<> createDescriptorData(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Creating assets descriptor data");
    const VKit::LogicalDevice &device = Core::GetDevice();
    const auto poolResult = VKit::DescriptorPool::Builder(device)
                                .SetMaxSets(specs.MaxSets)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, specs.PoolSize)
                                .AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, specs.PoolSize)
                                .Build();
    TKIT_RETURN_ON_ERROR(poolResult);
    s_DescriptorPool = poolResult.GetValue();

    auto layoutResult = VKit::DescriptorSetLayout::Builder(device)
                            .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_InstanceDataStorageLayout = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    s_LightStorageLayout = layoutResult.GetValue();
    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    s_Sets.Reserve(specs.MaxSets);
    return createDescriptorData(specs);
}
void Terminate()
{
    s_DescriptorPool.Destroy();
    s_InstanceDataStorageLayout.Destroy();
    s_LightStorageLayout.Destroy();
}

Result<DescriptorSet *> FindSuitableDescriptorSet(const VKit::DeviceBuffer &buffer)
{
    for (DescriptorSet &set : s_Sets)
        if (!set.InUse())
        {
            if (set.Buffer == buffer.GetHandle())
                return &set;
            const VkDescriptorBufferInfo info = buffer.CreateDescriptorInfo();
            const auto result = WriteStorageBufferDescriptorSet(info, set.Set);
            TKIT_RETURN_ON_ERROR(result);
            set.Set = result.GetValue();
            set.Buffer = buffer;
        }

    DescriptorSet &set = s_Sets.Append();
    const VkDescriptorBufferInfo info = buffer.CreateDescriptorInfo();

    const auto result = WriteStorageBufferDescriptorSet(info, set.Set);
    TKIT_RETURN_ON_ERROR(result);
    set.Set = result.GetValue();
    set.Buffer = buffer;
    return &set;
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

Result<VkDescriptorSet> WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &info, VkDescriptorSet oldSet)
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), &s_InstanceDataStorageLayout};
    writer.WriteBuffer(0, info);

    if (!oldSet)
    {
        const auto result = s_DescriptorPool.Allocate(s_InstanceDataStorageLayout);
        TKIT_RETURN_ON_ERROR(result);
        oldSet = result.GetValue();
    }
    writer.Overwrite(oldSet);
    return oldSet;
}
} // namespace Onyx::Descriptors

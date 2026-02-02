#include "onyx/core/pch.hpp"
#include "onyx/state/descriptors.hpp"
#include "onyx/execution/execution.hpp"
#include "vkit/resource/device_buffer.hpp"

namespace Onyx
{
struct DescriptorSet
{
    Execution::Tracker Tracker{};
    VkDescriptorSet Set = VK_NULL_HANDLE;
    VkBuffer Buffer = VK_NULL_HANDLE; // will have to generalize to any handle
};
} // namespace Onyx

namespace Onyx::Descriptors
{
static TKit::Storage<VKit::DescriptorPool> s_DescriptorPool{};
static TKit::Storage<VKit::DescriptorSetLayout> s_InstanceDataStorageLayout{};
static TKit::Storage<VKit::DescriptorSetLayout> s_LightStorageLayout{};
static TKit::Storage<TKit::ArenaArray<DescriptorSet>> s_Sets{};

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
                            .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                            .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_InstanceDataStorageLayout = layoutResult.GetValue();

    layoutResult = VKit::DescriptorSetLayout::Builder(device)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                       .Build();

    TKIT_RETURN_ON_ERROR(layoutResult);
    *s_LightStorageLayout = layoutResult.GetValue();
    return Result<>::Ok();
}

Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorPool.Construct();
    s_InstanceDataStorageLayout.Construct();
    s_LightStorageLayout.Construct();
    s_Sets.Construct();

    s_Sets->Reserve(specs.MaxSets);
    return createDescriptorData(specs);
}
void Terminate()
{
    s_DescriptorPool->Destroy();
    s_InstanceDataStorageLayout->Destroy();
    s_LightStorageLayout->Destroy();

    s_DescriptorPool.Destruct();
    s_InstanceDataStorageLayout.Destruct();
    s_LightStorageLayout.Destruct();
    s_Sets.Destruct();
}

void MarkInUse(DescriptorSet *set, const VKit::Queue *queue, u64 inFlightValue)
{
    set->Tracker.MarkInUse(queue, inFlightValue);
}
VkDescriptorSet GetSet(const DescriptorSet *set)
{
    return set->Set;
}

Result<DescriptorSet *> FindSuitableDescriptorSet(const VKit::DeviceBuffer &buffer)
{
    for (DescriptorSet &set : *s_Sets)
        if (!set.Tracker.InUse())
        {
            if (set.Buffer == buffer.GetHandle())
                return &set;
            const VkDescriptorBufferInfo info = buffer.CreateDescriptorInfo();
            const auto result = WriteStorageBufferDescriptorSet(info, set.Set);
            TKIT_RETURN_ON_ERROR(result);
            set.Set = result.GetValue();
            set.Buffer = buffer;
        }

    DescriptorSet &set = s_Sets->Append();
    const VkDescriptorBufferInfo info = buffer.CreateDescriptorInfo();

    const auto result = WriteStorageBufferDescriptorSet(info, set.Set);
    TKIT_RETURN_ON_ERROR(result);
    set.Set = result.GetValue();
    set.Buffer = buffer;
    return &set;
}

const VKit::DescriptorPool &GetDescriptorPool()
{
    return *s_DescriptorPool;
}
const VKit::DescriptorSetLayout &GetInstanceDataStorageDescriptorSetLayout()
{
    return *s_InstanceDataStorageLayout;
}
const VKit::DescriptorSetLayout &GetLightStorageDescriptorSetLayout()
{
    return *s_LightStorageLayout;
}

Result<VkDescriptorSet> WriteStorageBufferDescriptorSet(const VkDescriptorBufferInfo &info, VkDescriptorSet oldSet)
{
    VKit::DescriptorSet::Writer writer{Core::GetDevice(), s_InstanceDataStorageLayout.Get()};
    writer.WriteBuffer(0, info);

    if (!oldSet)
    {
        const auto result = s_DescriptorPool->Allocate(*s_InstanceDataStorageLayout);
        TKIT_RETURN_ON_ERROR(result);
        oldSet = result.GetValue();
    }
    writer.Overwrite(oldSet);
    return oldSet;
}
} // namespace Onyx::Descriptors

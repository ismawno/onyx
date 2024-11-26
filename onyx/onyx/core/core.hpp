#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/instance.hpp"
#include "onyx/core/device.hpp"
#include "onyx/core/vma.hpp"

#ifndef ONYX_MAX_DESCRIPTOR_SETS
#    define ONYX_MAX_DESCRIPTOR_SETS 1000
#endif

#ifndef ONYX_MAX_STORAGE_BUFFER_DESCRIPTORS
#    define ONYX_MAX_STORAGE_BUFFER_DESCRIPTORS 1000
#endif

namespace TKit
{
class StackAllocator;
class ITaskManager;
} // namespace TKit

// This file handles the lifetime of global data the ONYX library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure proper destruction ordering and avoid the extremely annoying static memory
// deallocation randomness, I use reference counting. In the terminate method, I just set the global references to
// nullptr to ensure the reference count goes to 0 just before the program ends, avoiding static mess

namespace Onyx
{
class Window;
class DescriptorPool;
class DescriptorSetLayout;

struct ONYX_API Core
{
    static void Initialize(TKit::StackAllocator *p_Allocator, TKit::ITaskManager *p_Manager) noexcept;
    static void Terminate() noexcept;

    static TKit::StackAllocator *GetStackAllocator() noexcept;
    static TKit::ITaskManager *GetTaskManager() noexcept;

    static const TKit::Ref<Instance> &GetInstance() noexcept;
    static const TKit::Ref<Device> &GetDevice() noexcept;

    static VmaAllocator GetVulkanAllocator() noexcept;

    static const DescriptorPool *GetDescriptorPool() noexcept;
    static const DescriptorSetLayout *GetTransformStorageDescriptorSetLayout() noexcept;
    static const DescriptorSetLayout *GetLightStorageDescriptorSetLayout() noexcept;

    template <Dimension D> static VkPipelineLayout GetGraphicsPipelineLayout() noexcept;

  private:
    // Should ony be called by window constructor (I should look for a way to better hide this)
    static const TKit::Ref<Device> &tryCreateDevice(VkSurfaceKHR p_Surface) noexcept;

    friend class Window;
};

} // namespace Onyx

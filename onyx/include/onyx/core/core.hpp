#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/instance.hpp"
#include "onyx/core/device.hpp"
#include "onyx/core/vma.hpp"

#ifndef ONYX_MAX_DESCRIPTOR_SETS
#    define ONYX_MAX_DESCRIPTOR_SETS 100
#endif

#ifndef ONYX_MAX_STORAGE_BUFFER_DESCRIPTORS
#    define ONYX_MAX_STORAGE_BUFFER_DESCRIPTORS 100
#endif

namespace KIT
{
class StackAllocator;
class ITaskManager;
} // namespace KIT

// This file handles the lifetime of global data the ONYX library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure proper destruction ordering and avoid the extremely annoting static memory
// deallocation randomness, I use reference counting. In the terminate method, I just set the global references to
// nullptr to ensure the reference count goes to 0 just before the program ends, avoiding static mess

namespace ONYX
{
class Window;
class DescriptorPool;
class DescriptorSetLayout;

struct ONYX_API Core
{
    static void Initialize(KIT::StackAllocator *p_Allocator, KIT::ITaskManager *p_Manager) noexcept;
    static void Terminate() noexcept;

    static KIT::StackAllocator *GetStackAllocator() noexcept;
    static KIT::ITaskManager *GetTaskManager() noexcept;

    static const KIT::Ref<Instance> &GetInstance() noexcept;
    static const KIT::Ref<Device> &GetDevice() noexcept;

    static VmaAllocator GetVulkanAllocator() noexcept;

    static const KIT::Ref<DescriptorPool> &GetDescriptorPool() noexcept;
    static const KIT::Ref<DescriptorSetLayout> &GetStorageBufferDescriptorSetLayout() noexcept;

  private:
    // Should ony be called by window constructor (I should look for a way to better hide this)
    static const KIT::Ref<Device> &tryCreateDevice(VkSurfaceKHR p_Surface) noexcept;

    friend class Window;
};

} // namespace ONYX

#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/instance.hpp"
#include "onyx/core/device.hpp"
#include "onyx/core/vma.hpp"

namespace KIT
{
class StackAllocator;
class TaskManager;
} // namespace KIT

// This file handles the lifetime of global data the ONYX library needs, such as the Vulkan instance and device. To
// properly cleanup resources, ensure proper destruction ordering and avoid the extremely annoting static memory
// deallocation randomness, I use reference counting. In the terminate method, I just set the global references to
// nullptr to ensure the reference count goes to 0 just before the program ends, avoiding static mess

namespace ONYX
{
class Window;
struct ONYX_API Core
{
    static void Initialize(KIT::StackAllocator *p_Allocator, KIT::TaskManager *p_Manager) noexcept;
    static void Terminate() noexcept;

    static KIT::StackAllocator *GetStackAllocator() noexcept;
    static KIT::TaskManager *GetTaskManager() noexcept;

    static const KIT::Ref<ONYX::Instance> &GetInstance() noexcept;
    static const KIT::Ref<ONYX::Device> &GetDevice() noexcept;

    ONYX_DIMENSION_TEMPLATE static const char *GetVertexShaderPath() noexcept;
    ONYX_DIMENSION_TEMPLATE static const char *GetFragmentShaderPath() noexcept;

    VmaAllocator GetVulkanAllocator() const noexcept;

  private:
    // Should ony be called by window constructor (I should look for a way to better hide this)
    static const KIT::Ref<ONYX::Device> &tryCreateDevice(VkSurfaceKHR p_Surface) noexcept;

    friend class Window;
};

} // namespace ONYX

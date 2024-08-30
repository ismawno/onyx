#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/instance.hpp"
#include "onyx/core/device.hpp"

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
ONYX_DIMENSION_TEMPLATE class Window;
struct ONYX_API Core
{
    static void Initialize(KIT::StackAllocator *p_Allocator, KIT::TaskManager *p_Manager) noexcept;
    static void Terminate() noexcept;

    static KIT::StackAllocator *StackAllocator() noexcept;
    static KIT::TaskManager *TaskManager() noexcept;

    static const KIT::Ref<Instance> &GetInstance() noexcept;
    static const KIT::Ref<Device> &GetDevice() noexcept;

  private:
    // Should ony be called by window constructor (I should look for a way to better hide this)
    static const KIT::Ref<Device> &getOrCreateDevice(VkSurfaceKHR p_Surface) noexcept;

    friend class Window<2>;
    friend class Window<3>;
};

} // namespace ONYX

#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/instance.hpp"
#include "onyx/core/device.hpp"
#include "kit/container/buffered_array.hpp"
#include "kit/memory/stack_allocator.hpp"
#include "kit/multiprocessing/task_manager.hpp"

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

struct ONYX_API Allocator
{
    // using Allocate/Deallocate pair instead of Push/Pop so that when passing the pointer when deallocating, the
    // allocator
    // can ensure that the order of deallocation is correct (only in assert mode)

    template <typename T> static T *Push() noexcept
    {
        return Core::StackAllocator()->Allocate<T>();
    }
    template <typename T, typename... Args>
    static KIT::BufferedArray<T> Push(const usize p_Capacity, Args &&...args) noexcept
    {
        return KIT::BufferedArray<T>(Core::StackAllocator()->Allocate<T>(p_Capacity), p_Capacity,
                                     std::forward<Args>(args)...);
    }

    static void Pop(const void *p_Ptr) noexcept;

    template <typename T> static void Pop(const KIT::BufferedArray<T> &p_BufferedArray) noexcept
    {
        Core::StackAllocator()->Deallocate(p_BufferedArray.data());
    }
};

} // namespace ONYX

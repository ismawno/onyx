#pragma once

#include "onyx/core/api.hpp"
#include "kit/memory/ptr.hpp"
#include <vulkan/vulkan.hpp>

namespace ONYX
{
class ONYX_API Instance : public KIT::RefCounted<Instance>
{
    KIT_NON_COPYABLE(Instance)
  public:
    Instance() noexcept;
    ~Instance() noexcept;

#ifdef KIT_ENABLE_ASSERTS
    static const char *ValidationLayer() noexcept;
#endif

    VkInstance VulkanInstance() const noexcept;

  private:
    void initialize() noexcept;

    VkInstance m_Instance;

    friend struct Core;
};
} // namespace ONYX
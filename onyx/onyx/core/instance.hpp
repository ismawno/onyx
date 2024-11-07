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
    static const char *GetValidationLayer() noexcept;
#endif

    VkInstance GetInstance() const noexcept;

  private:
    void createInstance() noexcept;

    VkInstance m_Instance;

    friend struct Core;
};
} // namespace ONYX
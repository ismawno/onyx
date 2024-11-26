#pragma once

#include "onyx/core/api.hpp"
#include "tkit/memory/ptr.hpp"
#include <vulkan/vulkan.hpp>

namespace Onyx
{
class ONYX_API Instance : public TKit::RefCounted<Instance>
{
    TKIT_NON_COPYABLE(Instance)
  public:
    Instance() noexcept;
    ~Instance() noexcept;

#ifdef TKIT_ENABLE_ASSERTS
    static const char *GetValidationLayer() noexcept;
#endif

    VkInstance GetInstance() const noexcept;

  private:
    void createInstance() noexcept;

    VkInstance m_Instance;

    friend struct Core;
};
} // namespace Onyx
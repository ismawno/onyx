#include "onyx/core/pch.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/platform/glfw.hpp"
#include "tkit/container/static_array.hpp"

namespace Onyx::Platform
{
static TKit::Storage<TKit::StaticArray<Window *, ONYX_MAX_VIEWS>> s_Windows{};

#ifdef TKIT_ENABLE_ERROR_LOGS
static void glfwErrorCallback(const i32 errorCode, const char *description)
{
    TKIT_LOG_ERROR("[ONYX][GLFW] An error ocurred with code {} and the following description: {}", errorCode,
                   description);
}
#endif

static VkSurfaceFormatKHR s_SurfaceFormat;
static VkFormat s_ColorFormat;
static VkFormat s_DepthStencilFormat;

void Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][PLATFORM] Initializing");
    s_Windows.Construct();
    s_SurfaceFormat = specs.SurfaceFormat;
    s_ColorFormat = specs.ColorFormat;
    s_DepthStencilFormat = specs.DepthStencilFormat;
#ifdef TKIT_ENABLE_ERROR_LOGS
    glfwSetErrorCallback(glfwErrorCallback);
#endif
    glfwInitHint(GLFW_PLATFORM, specs.Platform);
    TKIT_CHECK_RETURNS(glfwInit(), GLFW_TRUE, "[ONYX][PLATFORM] GLFW failed to initialize");

    TKIT_LOG_WARNING_IF(!glfwVulkanSupported(), "[ONYX][PLATFORM] Vulkan is not supported, according to GLFW");
}

void Terminate()
{
    DestroyWindows();
    glfwTerminate();
    s_Windows.Destruct();
}

VkSurfaceFormatKHR GetSurfaceFormat()
{
    return s_SurfaceFormat;
}
VkFormat GetColorFormat()
{
    return s_ColorFormat;
}
VkFormat GetDepthStencilFormat()
{
    return s_DepthStencilFormat;
}

Window *CreateWindow(const WindowSpecs &specs)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    Window *window = tier->Create<Window>(specs);
    s_Windows->Append(window);
    return window;
}

static void removeWindow(const Window *window)
{
    for (u32 i = 0; i < s_Windows->GetSize(); ++i)
        if (s_Windows->At(i) == window)
        {
            s_Windows->RemoveUnordered(s_Windows->begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][PLATFORM] Window '{}' not found", window->GetTitle());
}

void DestroyWindow(Window *window)
{
    removeWindow(window);
    TKit::TierAllocator *tier = TKit::GetTier();
    tier->Destroy(window);
}

void DestroyWindows()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (Window *window : *s_Windows)
        tier->Destroy(window);
    s_Windows->Clear();
}

void PollEvents()
{
    glfwPollEvents();
}

} // namespace Onyx::Platform

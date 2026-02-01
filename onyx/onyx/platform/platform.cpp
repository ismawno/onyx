#include "onyx/core/pch.hpp"
#include "onyx/platform/platform.hpp"
#include "onyx/platform/window.hpp"
#include "onyx/platform/glfw.hpp"

namespace Onyx::Platform
{
static TKit::Storage<TKit::ArenaArray<Window *>> s_Windows{};

#ifdef TKIT_ENABLE_ERROR_LOGS
static void glfwErrorCallback(const i32 errorCode, const char *description)
{
    TKIT_LOG_ERROR("[ONYX][GLFW] An error ocurred with code {} and the following description: {}", errorCode,
                   description);
}
#endif
Result<> Initialize(const Specs &specs)
{
    TKIT_LOG_INFO("[ONYX][PLATFORM] Initializing");
    s_Windows.Construct();
#ifdef TKIT_ENABLE_ERROR_LOGS
    glfwSetErrorCallback(glfwErrorCallback);
#endif
    glfwInitHint(GLFW_PLATFORM, specs.Platform);
    if (glfwInit() != GLFW_TRUE)
        return Result<>::Error(Error_InitializationFailed, "[ONYX][PLATFORM] GLFW failed to initialize");

    TKIT_LOG_WARNING_IF(!glfwVulkanSupported(), "[ONYX][PLATFORM] Vulkan is not supported, according to GLFW");

    s_Windows->Reserve(64);
    return Result<>::Ok();
}

void Terminate()
{
    for (Window *window : *s_Windows)
        DestroyWindow(window);
    glfwTerminate();
    s_Windows.Destruct();
}

static u64 s_ViewCache = TKIT_U64_MAX;
static u64 allocateViewBit()
{
    const u32 index = static_cast<u32>(std::countr_zero(s_ViewCache));
    const u64 viewBit = 1 << index;
    s_ViewCache &= ~viewBit;
    return viewBit;
}
static void deallocateViewBit(const u64 viewBit)
{
    s_ViewCache |= viewBit;
}

Result<Window *> CreateWindow(const WindowSpecs &specs)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, specs.Flags & WindowFlag_Resizable);
    glfwWindowHint(GLFW_VISIBLE, specs.Flags & WindowFlag_Visible);
    glfwWindowHint(GLFW_DECORATED, specs.Flags & WindowFlag_Decorated);
    glfwWindowHint(GLFW_FOCUSED, specs.Flags & WindowFlag_Focused);
    glfwWindowHint(GLFW_FLOATING, specs.Flags & WindowFlag_Floating);

    GLFWwindow *handle = glfwCreateWindow(static_cast<i32>(specs.Dimensions[0]), static_cast<i32>(specs.Dimensions[1]),
                                          specs.Name, nullptr, nullptr);
    if (!handle)
        return Result<>::Error(Error_RejectedWindow);

    VKit::DeletionQueue cleanup{};
    cleanup.Push([handle] { glfwDestroyWindow(handle); });

    u32v2 pos;
    if (specs.Position != u32v2{TKIT_U32_MAX})
    {
        glfwSetWindowPos(handle, static_cast<i32>(specs.Position[0]), static_cast<i32>(specs.Position[1]));
        pos = specs.Position;
    }
    else
    {
        i32 x, y;
        glfwGetWindowPos(handle, &x, &y);
        pos = u32v2{x, y};
    }

    VkSurfaceKHR surface;
    const VkResult result = glfwCreateWindowSurface(Core::GetInstance(), handle, nullptr, &surface);
    if (result != VK_SUCCESS)
        return Result<>::Error(Error_NoSurfaceCapabilities);

    cleanup.Push([surface] { Core::GetInstanceTable()->DestroySurfaceKHR(Core::GetInstance(), surface, nullptr); });
    if (specs.Flags & WindowFlag_InstallCallbacks)
        Input::InstallCallbacks(handle);

    const VkExtent2D extent = Window::waitGlfwEvents(specs.Dimensions[0], specs.Dimensions[1]);
    auto sresult = Window::createSwapChain(specs.PresentMode, surface, extent);
    TKIT_RETURN_ON_ERROR(sresult);

    VKit::SwapChain &schain = sresult.GetValue();
    cleanup.Push([&schain] { schain.Destroy(); });

    auto syresult = Execution::CreateSyncData(schain.GetImageCount());
    TKIT_RETURN_ON_ERROR(syresult);
    TKit::TierArray<Execution::SyncData> &syncData = syresult.GetValue();
    cleanup.Push([&syncData] { Execution::DestroySyncData(syncData); });

    if (s_ViewCache == 0)
        return Result<>::Error(
            Error_InitializationFailed,
            "[ONYX][WINDOW] Maximum amount of windows exceeded. There is a hard limit of 64 windows");

    TKit::TierAllocator *alloc = TKit::GetTier();
    Window *window = alloc->Create<Window>();

    window->m_Window = handle;
    window->m_Name = specs.Name;
    window->m_Flags = specs.Flags;
    window->m_Surface = surface;
    window->m_SwapChain = schain;
    window->m_Position = pos;
    window->m_Dimensions = specs.Dimensions;

    auto iresult = Window::createImageData(window->m_SwapChain);
    TKIT_RETURN_ON_ERROR(iresult);
    TKit::TierArray<Window::ImageData> &imageData = iresult.GetValue();
    cleanup.Push([&imageData] { Window::destroyImageData(imageData); });
    window->m_Images = std::move(imageData);

    window->m_PresentMode = specs.PresentMode;
    window->m_SyncData = std::move(syncData);
    window->m_ViewBit = allocateViewBit();
    window->m_Present = Execution::FindSuitableQueue(VKit::Queue_Present);
    window->UpdateMonitorDeltaTime();
    glfwSetWindowUserPointer(handle, window);

    cleanup.Dismiss();
    return window;
}
void DestroyWindow(Window *window)
{
    deallocateViewBit(window->GetViewBit());
    TKit::TierAllocator *alloc = TKit::GetTier();
    alloc->Destroy(window);
}

} // namespace Onyx::Platform

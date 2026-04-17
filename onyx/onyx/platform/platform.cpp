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

    return Result<>::Ok();
}

void Terminate()
{
    DestroyWindows();
    glfwTerminate();
    s_Windows.Destruct();
}

Result<Window *> CreateWindow(const WindowSpecs &specs)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, specs.Flags & WindowFlag_Resizable);
    glfwWindowHint(GLFW_VISIBLE, specs.Flags & WindowFlag_Visible);
    glfwWindowHint(GLFW_DECORATED, specs.Flags & WindowFlag_Decorated);
    glfwWindowHint(GLFW_FOCUSED, specs.Flags & WindowFlag_Focused);
    glfwWindowHint(GLFW_FLOATING, specs.Flags & WindowFlag_Floating);
    // glfwWindowHint(GLFW_ICONIFIED, specs.Flags & WindowFlag_Iconified);
#ifdef ONYX_GLFW_FOCUS_ON_SHOW
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, specs.Flags & WindowFlag_FocusOnShow);
#endif

    GLFWwindow *handle =
        glfwCreateWindow(i32(specs.Dimensions[0]), i32(specs.Dimensions[1]), specs.Title, nullptr, nullptr);
    if (!handle)
        return Result<>::Error(Error_RejectedWindow);

    VKit::DeletionQueue cleanup{};
    cleanup.Push([handle] { glfwDestroyWindow(handle); });

    u32v2 pos;
    if (specs.Position != i32v2{TKIT_I32_MAX})
    {
        TKIT_ASSERT(specs.Position[0] < TKIT_I32_MAX,
                    "[ONYX][PLATFORM] If component y of the window position is not "
                    "TKIT_I32_MAX, component x must not be either. Passed position is ({}, {})",
                    specs.Position[0], specs.Position[1]);
        TKIT_ASSERT(specs.Position[1] < TKIT_I32_MAX,
                    "[ONYX][PLATFORM] If component x of the window position is not "
                    "TKIT_I32_MAX, component y must not be either. Passed position is ({}, {})",
                    specs.Position[0], specs.Position[1]);

        glfwSetWindowPos(handle, specs.Position[0], specs.Position[1]);
        pos = specs.Position;
    }
    else
    {
        i32 x, y;
        glfwGetWindowPos(handle, &x, &y);
        pos = u32v2{x, y};
    }

    VkSurfaceKHR surface;
    VKIT_RETURN_IF_FAILED(glfwCreateWindowSurface(GetInstance(), handle, nullptr, &surface), Result<Window *>);
    cleanup.Push([surface] { GetInstanceTable()->DestroySurfaceKHR(GetInstance(), surface, nullptr); });

    const VkExtent2D extent = Window::getNewExtent(handle);
    auto sresult = Window::createSwapChain(specs.PresentMode, surface, extent);
    TKIT_RETURN_ON_ERROR(sresult);

    VKit::SwapChain &schain = sresult.GetValue();
    cleanup.Push([&schain] { schain.Destroy(); });

    auto syresult = Execution::CreateViewSyncData(schain.GetImageCount());
    TKIT_RETURN_ON_ERROR(syresult);
    TKit::TierArray<Execution::ViewSyncData> &syncData = syresult.GetValue();

    cleanup.Push([&syncData] { Execution::DestroyViewSyncData(syncData); });

    TKit::TierAllocator *tier = TKit::GetTier();
    Window *window = tier->Create<Window>();

    window->m_Window = handle;
    window->m_Surface = surface;
    window->m_SwapChain = schain;

    auto iresult = Window::createImageData(window->m_SwapChain);
    TKIT_RETURN_ON_ERROR(iresult);
    TKit::TierArray<Window::ImageData> &imageData = iresult.GetValue();
    cleanup.Push([&imageData] { Window::destroyImageData(imageData); });
    window->m_Images = std::move(imageData);
    window->m_PresentMode = specs.PresentMode;
    window->m_SyncData = std::move(syncData);
    window->m_Present = Execution::FindSuitableQueue(VKit::Queue_Present);
    window->UpdateMonitorDeltaTime();
    if (IsDebugUtilsEnabled())
    {
        TKIT_RETURN_IF_FAILED(window->nameSurface());
        TKIT_RETURN_IF_FAILED(window->nameSwapChain());
        TKIT_RETURN_IF_FAILED(window->nameSyncData());
        TKIT_RETURN_IF_FAILED(window->nameImageData());
    }
    glfwSetWindowUserPointer(handle, window);
    if (specs.Flags & WindowFlag_InstallCallbacks)
        window->InstallCallbacks();

    cleanup.Dismiss();
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
    ViewMask vmask = 0;
    for (const RenderView<D2> *rv : window->GetRenderViews<D2>())
        vmask |= rv->GetViewBit();
    for (const RenderView<D3> *rv : window->GetRenderViews<D3>())
        vmask |= rv->GetViewBit();

    Renderer::RemoveTarget(vmask);
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

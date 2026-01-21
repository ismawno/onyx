#include "onyx/core/pch.hpp"
#include "onyx/imgui/backend.hpp"
#include "onyx/app/window.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/core/core.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

namespace Onyx
{
void InitializeImGui(Window *p_Window)
{
    TKIT_CHECK_RETURNS(ImGui_ImplGlfw_InitForVulkan(p_Window->GetWindowHandle(), true), true,
                       "[ONYX] Failed to initialize ImGui GLFW for window '{}'", p_Window->GetName());

    const VKit::Instance &instance = Core::GetInstance();
    const VKit::LogicalDevice &device = Core::GetDevice();

    ImGuiIO &io = ImGui::GetIO();
    TKIT_LOG_WARNING_IF(
        (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) &&
            instance.GetInfo().Flags & VKit::InstanceFlag_HasValidationLayers,
        "[ONYX] Vulkan validation layers have become stricter regarding semaphore and fence usage when submitting to "
        "Execution. ImGui may not have caught up to this and may trigger validation errors when the "
        "ImGuiConfigFlags_ViewportsEnable flag is set. If this is the case, either disable the flag or the vulkan "
        "validation layers. If the application runs well, you may safely ignore this warning");

    ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
    pipelineInfo.PipelineRenderingCreateInfo = Renderer::CreatePipelineRenderingCreateInfo();
    pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    const VkSurfaceCapabilitiesKHR &sc = p_Window->GetSwapChain().GetInfo().SupportDetails.Capabilities;

    const u32 imageCount = sc.minImageCount != sc.maxImageCount ? sc.minImageCount + 1 : sc.minImageCount;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = instance.GetInfo().ApiVersion;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = *device.GetInfo().PhysicalDevice;
    initInfo.Device = device;
    initInfo.Queue = Execution::FindSuitableQueue(VKit::Queue_Graphics)->GetHandle();
    initInfo.QueueFamily = Execution::GetFamilyIndex(VKit::Queue_Graphics);
    initInfo.DescriptorPoolSize = 100;
    initInfo.MinImageCount = sc.minImageCount;
    initInfo.ImageCount = imageCount;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain = pipelineInfo;

    TKIT_CHECK_RETURNS(ImGui_ImplVulkan_LoadFunctions(instance.GetInfo().ApiVersion,
                                                      [](const char *p_Name, void *) -> PFN_vkVoidFunction {
                                                          return VKit::Vulkan::GetInstanceProcAddr(Core::GetInstance(),
                                                                                                   p_Name);
                                                      }),
                       true, "[ONYX] Failed to load ImGui Vulkan functions");

    TKIT_CHECK_RETURNS(ImGui_ImplVulkan_Init(&initInfo), true,
                       "[ONYX] Failed to initialize ImGui Vulkan for window '{}'", p_Window->GetName());
}

void NewImGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void RenderImGuiData(ImDrawData *p_Data, const VkCommandBuffer p_CommandBuffer)
{
    ImGui_ImplVulkan_RenderDrawData(p_Data, p_CommandBuffer);
}
void RenderImGuiWindows()
{
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

void ShutdownImGui()
{
    Core::DeviceWaitIdle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyPlatformWindows();
}

} // namespace Onyx

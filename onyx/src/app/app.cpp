#include "core/pch.hpp"
#include "onyx/app/app.hpp"
#include "onyx/app/input.hpp"
#include "onyx/core/core.hpp"
#include "kit/multiprocessing/task_manager.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE Application<N>::~Application() noexcept
{
    if (!m_Terminated && m_Started)
        Shutdown();
}

ONYX_DIMENSION_TEMPLATE Window<N> *Application<N>::OpenWindow(const Window<N>::Specs &p_Specs) noexcept
{
    m_Windows.push_back(KIT::Scope<Window<N>>::Create(p_Specs));
    m_Tasks.emplace_back();
    return m_Windows.back().Get();
}

ONYX_DIMENSION_TEMPLATE Window<N> *Application<N>::OpenWindow() noexcept
{
    return OpenWindow(typename Window<N>::Specs{});
}

ONYX_DIMENSION_TEMPLATE void Application<N>::CloseWindow(usize p_Index) noexcept
{
    KIT_ASSERT(p_Index < m_Windows.size(), "Index out of bounds");
    if (m_Tasks[p_Index])
        m_Tasks[p_Index]->WaitUntilFinished();

    m_Windows.erase(m_Windows.begin() + p_Index);
    m_Tasks.erase(m_Tasks.begin() + p_Index);

    // Check if the main window got removed. If so, the main thread will handle the next window
    if (p_Index == 0 && !m_Tasks.empty())
    {
        m_Tasks[0]->WaitUntilFinished();
        m_Tasks[0] = nullptr;
    }
}

ONYX_DIMENSION_TEMPLATE void Application<N>::CloseWindow(const Window<N> *p_Window) noexcept
{
    for (usize i = 0; i < m_Windows.size(); ++i)
        if (m_Windows[i].Get() == p_Window)
        {
            CloseWindow(i);
            return;
        }
    KIT_ERROR("Window was not found");
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Start() noexcept
{
    KIT_ASSERT(!m_Terminated && !m_Started, "Application already started");
    KIT_ASSERT(!m_Windows.empty(), "Must first open a window");
    m_Device = Core::Device();
    initializeImGui();
    m_Started = true;
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Shutdown() noexcept
{
    KIT_ASSERT(!m_Terminated && m_Started, "Application not started");
    shutdownImGui();
    m_Terminated = true;
}

ONYX_DIMENSION_TEMPLATE bool Application<N>::NextFrame() noexcept
{
    if (m_Windows.empty())
        return false;

    Input::PollEvents();
    runAndManageWindows();
    return !m_Windows.empty();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::Run() noexcept
{
    Start();
    while (NextFrame())
        ;
    Shutdown();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::runAndManageWindows() noexcept
{
    for (usize i = 1; i < m_Windows.size(); ++i)
    {
        KIT::TaskManager *taskManager = Core::TaskManager();
        auto &task = m_Tasks[i];
        if (!task)
        {
            Window<N> *window = m_Windows[i].Get();
            task = taskManager->CreateAndSubmit([window](usize) { runFrame(*window); });
        }
        else
        {
            task->WaitUntilFinished();
            task->Reset();
            taskManager->SubmitTask(task);
        }
    }
    beginRenderImGui();
    ImGui::Begin("Windows");
    ImGui::End();
    // Main thread always handles the first window. First element of tasks is always nullptr
    runFrame(*m_Windows[0], [this](const VkCommandBuffer p_CommandBuffer) { endRenderImGui(p_CommandBuffer); });

    for (usize i = m_Windows.size() - 1; i < m_Windows.size(); --i)
        if (m_Windows[i]->ShouldClose())
            CloseWindow(i);
}

ONYX_DIMENSION_TEMPLATE void Application<N>::runFrame(Window<N> &p_Window) noexcept
{
    runFrame(p_Window, [](const VkCommandBuffer) {});
}

ONYX_DIMENSION_TEMPLATE void Application<N>::beginRenderImGui() noexcept
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::endRenderImGui(VkCommandBuffer p_CommandBuffer) noexcept
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_CommandBuffer);
    ImGui::UpdatePlatformWindows();

    // Lock the queues, imgui docking will call vkQueueSubmit
    m_Device->LockQueues();
    ImGui::RenderPlatformWindowsDefault();
    m_Device->UnlockQueues();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::initializeImGui() noexcept
{
    constexpr std::uint32_t poolSize = 1000;
    VkDescriptorPoolSize poolSizes[11] = {{VK_DESCRIPTOR_TYPE_SAMPLER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolSize},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolSize},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolSize},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolSize},
                                          {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolSize}};

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = 11;
    poolInfo.pPoolSizes = poolSizes;

    const auto &instance = Core::Instance();
    KIT_ASSERT_RETURNS(vkCreateDescriptorPool(m_Device->VulkanDevice(), &poolInfo, nullptr, &m_ImGuiPool), VK_SUCCESS,
                       "Failed to create descriptor pool");

    ImGui::CreateContext();

    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    auto &window = m_Windows[0];
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window->GLFWWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance->VulkanInstance();
    initInfo.PhysicalDevice = m_Device->PhysicalDevice();
    initInfo.Device = m_Device->VulkanDevice();
    initInfo.Queue = m_Device->GraphicsQueue();
    initInfo.DescriptorPool = m_ImGuiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.RenderPass = window->GetRenderer().GetSwapChain().RenderPass();

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
    // ImGui_ImplVulkan_DestroyFontsTexture();
}

ONYX_DIMENSION_TEMPLATE void Application<N>::shutdownImGui() noexcept
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_Device->VulkanDevice(), m_ImGuiPool, nullptr);
}

template class Application<2>;
template class Application<3>;
} // namespace ONYX
#include "onyx/core/pch.hpp"
#include "onyx/application/layer.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/theme.hpp"
#    include "onyx/imgui/backend.hpp"
#endif

namespace Onyx
{
WindowLayer::WindowLayer(ApplicationLayer *appLayer, Window *window, const TKit::Timespan targetDeltaTime,
                         const WindowLayerFlags flags)
    : m_AppLayer(appLayer), m_Window(window)
{
#ifdef ONYX_ENABLE_IMGUI
    if (flags & WindowLayerFlag_ImGuiEnabled)
        initializeImGui();
#endif
    m_Delta.Target = window->IsVSync() ? window->GetMonitorDeltaTime() : targetDeltaTime;
    m_Flags = flags;
}
WindowLayer::WindowLayer(ApplicationLayer *appLayer, Window *window, const WindowLayerFlags flags)
    : WindowLayer(appLayer, window, window->GetMonitorDeltaTime(), flags)
{
}
Result<Renderer::RenderSubmitInfo> WindowLayer::OnRender(const ExecutionInfo &info)
{
    OnRender(info.DeltaTime);
    return Render(info);
}

Result<Renderer::RenderSubmitInfo> WindowLayer::Render(const ExecutionInfo &info)
{
    m_Window->BeginRendering(info.CommandBuffer);
    const auto result = Renderer::Render(info.Queue, info.CommandBuffer, m_Window);
#ifdef ONYX_ENABLE_IMGUI
    if (checkFlags(WindowLayerFlag_ImGuiEnabled))
    {
        ImGui::Render();
        RenderImGuiData(ImGui::GetDrawData(), info.CommandBuffer);
        RenderImGuiWindows();
    }
#endif
    m_Window->EndRendering(info.CommandBuffer);
    return result;
}

#ifdef ONYX_ENABLE_IMGUI
void WindowLayer::initializeImGui()
{
    TKIT_ASSERT(!checkFlags(WindowLayerFlag_ImGuiEnabled),
                "[ONYX][APPLICATION] Trying to initialize ImGui for window '{}' when it is already enabled. If you "
                "meant to reload ImGui, use ReloadImGui()",
                m_Window->GetName());

    IMGUI_CHECKVERSION();

    m_ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    m_ImPlotContext = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(m_ImPlotContext);
#    endif

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags = m_ImGuiConfigFlags;
    InitializeImGui(m_Window);
    ImFont *font = io.Fonts->AddFontFromFileTTF(ONYX_ROOT_PATH "/onyx/fonts/OpenSans-Regular.ttf", 16.f);
    io.FontDefault = font;
    ApplyTheme(Theme_Baby);

    setFlags(WindowLayerFlag_ImGuiEnabled);
}
void WindowLayer::shutdownImGui()
{
    TKIT_ASSERT(
        checkFlags(WindowLayerFlag_ImGuiEnabled),
        "[ONYX][APPLICATION] Trying to shut down ImGui for window '{}' when it is not initialized to begin with",
        m_Window->GetName());

    clearFlags(WindowLayerFlag_ImGuiEnabled);

    ImGui::SetCurrentContext(m_ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(m_ImPlotContext);
#    endif

    ShutdownImGui();

    ImGui::DestroyContext(m_ImGuiContext);
    m_ImGuiContext = nullptr;
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::DestroyContext(m_ImPlotContext);
    m_ImPlotContext = nullptr;
#    endif
}
#endif

Result<Renderer::TransferSubmitInfo> ApplicationLayer::OnTransfer(const ExecutionInfo &info)
{
    OnTransfer(info.DeltaTime);
    return Transfer(info);
}
Result<Renderer::TransferSubmitInfo> ApplicationLayer::Transfer(const ExecutionInfo &info)
{
    return Renderer::Transfer(info.Queue, info.CommandBuffer);
}

} // namespace Onyx

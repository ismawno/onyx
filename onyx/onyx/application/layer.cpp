#include "onyx/core/pch.hpp"
#include "onyx/application/layer.hpp"
#ifdef ONYX_ENABLE_IMGUI
#    include "onyx/imgui/theme.hpp"
#    include "onyx/imgui/backend.hpp"
#endif

namespace Onyx
{
WindowLayer::WindowLayer(ApplicationLayer *appLayer, Window *window, const TKit::Timespan targetDeltaTime,
                         const WindowLayerSpecs &specs)
    : m_AppLayer(appLayer), m_Window(window)
{
#ifdef ONYX_ENABLE_IMGUI
    m_ImGuiConfigFlags = specs.ImGuiConfigFlags;
    if (specs.Flags & WindowLayerFlag_ImGuiEnabled)
    {
        VKIT_CHECK_EXPRESSION(initializeImGui());
    }
#endif
    m_Delta.Target = window->IsVSync() ? window->GetMonitorDeltaTime() : targetDeltaTime;
    m_Flags = specs.Flags;
}
WindowLayer::WindowLayer(ApplicationLayer *appLayer, Window *window, const WindowLayerSpecs &specs)
    : WindowLayer(appLayer, window, window->GetMonitorDeltaTime(), specs)
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
        TKIT_RETURN_IF_FAILED(ImGuiBackend::RenderData(ImGui::GetDrawData(), info.CommandBuffer),
                              m_Window->EndRendering(info.CommandBuffer));
        TKIT_RETURN_IF_FAILED(ImGuiBackend::UpdatePlatformWindows(), m_Window->EndRendering(info.CommandBuffer));
    }
#endif
    m_Window->EndRendering(info.CommandBuffer);
    return result;
}

#ifdef ONYX_ENABLE_IMGUI
Result<> WindowLayer::initializeImGui()
{
    TKIT_ASSERT(!checkFlags(WindowLayerFlag_ImGuiEnabled),
                "[ONYX][APPLICATION] Trying to initialize ImGui for window '{}' when it is already enabled. If you "
                "meant to reload ImGui, use ReloadImGui()",
                m_Window->GetTitle());

    IMGUI_CHECKVERSION();

    if (m_ImGuiContext)
        ImGui::DestroyContext(m_ImGuiContext);

#    ifdef ONYX_ENABLE_IMPLOT
    if (m_ImPlotContext)
        ImPlot::DestroyContext(m_ImPlotContext);
#    endif

    m_ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    m_ImPlotContext = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(m_ImPlotContext);
#    endif

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags = m_ImGuiConfigFlags;
    TKIT_RETURN_IF_FAILED(ImGuiBackend::Create(m_Window));
    ImFont *font = io.Fonts->AddFontFromFileTTF(ONYX_ROOT_PATH "/onyx/fonts/OpenSans-Regular.ttf", 16.f);
    io.FontDefault = font;
    ApplyTheme(Theme_Baby);

    setFlags(WindowLayerFlag_ImGuiEnabled);
    return Result<>::Ok();
}
void WindowLayer::shutdownImGui()
{
    TKIT_ASSERT(
        checkFlags(WindowLayerFlag_ImGuiEnabled),
        "[ONYX][APPLICATION] Trying to shut down ImGui for window '{}' when it is not initialized to begin with",
        m_Window->GetTitle());

    clearFlags(WindowLayerFlag_ImGuiEnabled);

    ImGui::SetCurrentContext(m_ImGuiContext);
#    ifdef ONYX_ENABLE_IMPLOT
    ImPlot::SetCurrentContext(m_ImPlotContext);
#    endif

    ImGuiBackend::Destroy();

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

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
    if (specs.Flags & WindowLayerFlag_ImGuiEnabled)
        initializeImGui();
#endif
    m_Delta.Target = window->IsVSync() ? window->GetMonitorDeltaTime() : targetDeltaTime;
    m_Flags = specs.Flags;
}
WindowLayer::WindowLayer(ApplicationLayer *appLayer, Window *window, const WindowLayerSpecs &specs)
    : WindowLayer(appLayer, window, window->GetMonitorDeltaTime(), specs)
{
}
RenderSubmitInfo WindowLayer::OnRender(const ExecutionInfo &info)
{
    OnRender(info.DeltaTime);
    return Render(info);
}

RenderSubmitInfo WindowLayer::Render(const ExecutionInfo &info)
{
    // TODO(Isma): User must be able to set this. Stop using window layer flag for this maybe, and rely on render flags
    // for imgui?
    RenderFlags flags = 0;
#ifdef ONYX_ENABLE_IMGUI
    flags |= RenderFlag_ImGui * checkFlags(WindowLayerFlag_ImGuiEnabled);
#endif
    return Renderer::Render(info.Queue, info.CommandBuffer, m_Window, flags);
}

#ifdef ONYX_ENABLE_IMGUI
void WindowLayer::initializeImGui()
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

    ImGuiBackend::Create(m_Window);
    ImGuiIO &io = ImGui::GetIO();
    ImFont *font = io.Fonts->AddFontFromFileTTF(ONYX_ROOT_PATH "/onyx/fonts/OpenSans-Regular.ttf", 16.f);
    io.FontDefault = font;
    ApplyImGuiTheme(ImGuiTheme_Baby);

    setFlags(WindowLayerFlag_ImGuiEnabled);
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

TransferSubmitInfo ApplicationLayer::OnTransfer(const ExecutionInfo &info)
{
    OnTransfer(info.DeltaTime);
    return Transfer(info);
}
TransferSubmitInfo ApplicationLayer::Transfer(const ExecutionInfo &info)
{
    return Renderer::Transfer(info.Queue, info.CommandBuffer);
}

} // namespace Onyx

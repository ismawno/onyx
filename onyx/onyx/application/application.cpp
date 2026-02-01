#include "onyx/core/pch.hpp"
#include "onyx/application/application.hpp"
#include "onyx/imgui/backend.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
Application::~Application()
{
    for (WindowLayer *wlayer : m_WindowLayers)
    {
        destroyWindowLayer(wlayer);
        Platform::DestroyWindow(wlayer->m_Window);
    }
    destroyAppLayer();
}

Result<bool> Application::NextTick(TKit::Clock &clock)
{
    TKIT_PROFILE_NSCOPE("Onyx::Application::NextTick");
    Input::PollEvents();

    constexpr auto revoke = Execution::RevokeUnsubmittedQueueTimelines;
    if (m_AppLayer->isUpdateDue())
    {
        m_AppLayer->markUpdateTick();
        m_AppLayer->OnUpdate(m_AppLayer->m_UpdateDelta);
    }
    if (m_AppLayer->isTransferDue())
    {
        m_AppLayer->markTransferTick();
        Renderer::Coalesce();
        VKit::Queue *tqueue = Execution::FindSuitableQueue(VKit::Queue_Transfer);
        TKIT_RETURN_IF_FAILED(tqueue->UpdateCompletedTimeline());

        const auto tresult = Execution::FindSuitableCommandPool(VKit::Queue_Transfer);
        TKIT_RETURN_ON_ERROR(tresult);
        CommandPool *tpool = tresult.GetValue();

        const auto cmdres = Execution::Allocate(tpool);

        TKIT_RETURN_ON_ERROR(cmdres);
        const VkCommandBuffer cmd = cmdres.GetValue();
        TKIT_RETURN_IF_FAILED(Execution::BeginCommandBuffer(cmd));
        const auto result =
            m_AppLayer->OnTransfer({.Queue = tqueue, .CommandBuffer = cmd, .DeltaTime = m_AppLayer->m_TransferDelta});

        TKIT_RETURN_IF_FAILED(Execution::EndCommandBuffer(cmd), revoke());
        TKIT_RETURN_IF_FAILED(result, revoke());

        const Renderer::TransferSubmitInfo &info = result.GetValue();
        if (info)
        {
            TKIT_RETURN_IF_FAILED(Renderer::SubmitTransfer(tqueue, tpool, info), revoke());
        }
    }

    TKit::StackArray<WindowLayer *> dueWindows{};
    dueWindows.Reserve(m_WindowLayers.GetSize());

    for (WindowLayer *wlayer : m_WindowLayers)
    {
        Window *win = wlayer->m_Window;
        for (const Event &event : win->GetNewEvents())
        {
            if (win->IsVSync() && (event.Type == Event_SwapChainRecreated || event.Type == Event_WindowMoved))
            {
                wlayer->m_Delta.Target = win->GetMonitorDeltaTime();
#ifdef ONYX_ENABLE_IMGUI
                wlayer->m_DeltaInfo.LimitHertz = true;
#endif
            }
            wlayer->OnEvent(event);
        }
        win->FlushEvents();
        if (wlayer->isDue())
        {
            const auto result = win->AcquireNextImage();
            TKIT_RETURN_ON_ERROR(result);
            if (result.GetValue())
            {
                wlayer->markTick();
                dueWindows.Append(wlayer);
            }
        }
    }

    if (!dueWindows.IsEmpty())
    {
        VKit::Queue *gqueue = Execution::FindSuitableQueue(VKit::Queue_Graphics);
        TKIT_RETURN_IF_FAILED(gqueue->UpdateCompletedTimeline());

        const auto gresult = Execution::FindSuitableCommandPool(VKit::Queue_Graphics);
        TKIT_RETURN_ON_ERROR(gresult);
        CommandPool *gpool = gresult.GetValue();

        TKit::StackArray<Renderer::RenderSubmitInfo> rinfos{};
        rinfos.Reserve(m_WindowLayers.GetSize());

        for (WindowLayer *wlayer : dueWindows)
        {
            const auto cmdres = Execution::Allocate(gpool);
            TKIT_RETURN_ON_ERROR(cmdres, revoke());
            const VkCommandBuffer cmd = cmdres.GetValue();

            TKIT_RETURN_IF_FAILED(Execution::BeginCommandBuffer(cmd), revoke());
            Renderer::ApplyAcquireBarriers(cmd);
#ifdef ONYX_ENABLE_IMGUI
            if (wlayer->checkFlags(WindowLayerFlag_ImGuiEnabled))
            {
                ImGui::SetCurrentContext(wlayer->m_ImGuiContext);
                NewImGuiFrame();
            }
#endif
            const auto rnres = wlayer->OnRender({.Queue = gqueue, .CommandBuffer = cmd, .DeltaTime = wlayer->m_Delta});
            TKIT_RETURN_IF_FAILED(Execution::EndCommandBuffer(cmd), revoke());

            TKIT_RETURN_ON_ERROR(rnres, revoke());
            rinfos.Append(rnres.GetValue());
        }
        TKIT_RETURN_IF_FAILED(Renderer::SubmitRender(gqueue, gpool, rinfos), revoke());
        for (u32 i = 0; i < rinfos.GetSize(); ++i)
        {
            TKIT_RETURN_IF_FAILED(dueWindows[i]->m_Window->Present(rinfos[i]), revoke())
        }

        revoke();
    }
    const auto endFrame = [&] {
        m_AppLayer->m_Flags = 0;
        TKIT_PROFILE_MARK_FRAME();
    };

    if (m_AppLayer->checkFlags(ApplicationLayerFlag_RequestQuitApplication))
    {
        endFrame();
        return false;
    }

    for (const OpenWindowRequest &request : m_AppLayer->m_WindowRequests)
    {
        const auto result = Platform::CreateWindow(request.Specs);
        TKIT_RETURN_ON_ERROR(result);
        WindowLayer *wlayer = request.LayerCreation(m_AppLayer, result.GetValue());
        m_WindowLayers.Append(wlayer);
    }
    m_AppLayer->m_WindowRequests.Clear();

    if (m_AppLayer->m_Replacement)
    {
        ApplicationLayer *layer = m_AppLayer->m_Replacement();
        destroyAppLayer();
        m_AppLayer = layer;
        m_AppLayer->m_Replacement = nullptr;
        updateWindowLayers();
    }
    for (u32 i = 0; i < m_WindowLayers.GetSize(); ++i)
    {
        WindowLayer *layer = m_WindowLayers[i];
        Window *window = layer->m_Window;
        if (layer->checkFlags(WindowLayerFlag_RequestCloseWindow) || window->ShouldClose())
        {
            destroyWindowLayer(layer);
            Platform::DestroyWindow(window);
            m_WindowLayers.RemoveUnordered(m_WindowLayers.begin() + i);
            continue;
        }
#ifdef ONYX_ENABLE_IMGUI
        if (layer->checkFlags(WindowLayerFlag_RequestDisableImGui))
            layer->shutdownImGui();
        if (layer->checkFlags(WindowLayerFlag_RequestEnableImGui))
            layer->initializeImGui();
#endif
        layer->clearFlags(WindowLayerFlag_RequestEnableImGui | WindowLayerFlag_RequestDisableImGui);
        if (layer->m_Replacement)
        {
            WindowLayer *nlayer = layer->m_Replacement(m_AppLayer, layer->m_Window);
            destroyWindowLayer(nlayer);
            m_WindowLayers[i] = nlayer;
        }
    }

    if (m_WindowLayers.IsEmpty())
    {
        endFrame();
        return false;
    }

    const auto computeSleep = [](WindowLayer *wlayer) { return wlayer->m_Delta.Target - wlayer->m_Clock.GetElapsed(); };

    TKit::Timespan sleep = computeSleep(m_WindowLayers[0]);
    for (u32 i = 1; i < m_WindowLayers.GetSize(); ++i)
    {
        const TKit::Timespan s = computeSleep(m_WindowLayers[i]);
        if (s < sleep)
            sleep = s;
    }
    {
        TKIT_PROFILE_NSCOPE("Onyx::Application::Sleep");
        TKit::Timespan::Sleep(sleep);
    }

    TKIT_PROFILE_MARK_FRAME();
    m_DeltaTime = clock.Restart();
    return true;
}

Result<> Application::Run()
{
    TKit::Clock clock{};
    for (;;)
    {
        const auto result = NextTick(clock);
        TKIT_RETURN_ON_ERROR(result);
        if (!result.GetValue())
            return Result<>::Ok();
    }
}

void Application::updateWindowLayers()
{
    for (WindowLayer *wlayer : m_WindowLayers)
        wlayer->m_AppLayer = m_AppLayer;
}
WindowLayer **Application::getLayerFromWindow(const Window *window)
{
    for (WindowLayer *&wlayer : m_WindowLayers)
        if (wlayer->m_Window == window)
            return &wlayer;
    TKIT_FATAL("[ONYX][APPLICATION] Failed to find a window layer with the window named '{}' attached",
               window->GetName());
    return nullptr;
}

void Application::destroyWindowLayer(WindowLayer *layer)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    const u32 size = layer->m_Size;
    layer->~WindowLayer();
    tier->Deallocate(static_cast<void *>(layer), size);
}
void Application::destroyAppLayer()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    const u32 size = m_AppLayer->m_Size;
    m_AppLayer->~ApplicationLayer();
    tier->Deallocate(static_cast<void *>(m_AppLayer), size);
}
} // namespace Onyx

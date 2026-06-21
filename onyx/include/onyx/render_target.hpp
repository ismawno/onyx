#pragma once

#include "onyx/view.hpp"
#include "tkit/container/static_array.hpp"

namespace Onyx
{
struct LayerAssign
{
    u64 LayerIncrease = TKIT_U64_MAX / 2;
    u64 LayerDecrease = TKIT_U64_MAX / 2;

    u64 ToTop()
    {
        return LayerIncrease++;
    }
    u64 ToBottom()
    {
        return --LayerDecrease;
    }
};
struct RenderTargetInfo
{
    TKit::TierArray<RenderView<D2> *> Views2;
    TKit::TierArray<RenderView<D3> *> Views3;

    // only used in windows
    Onyx_Semaphore ImageAvailableSemaphore;
    Onyx_Semaphore RenderFinishedSemaphore;
};

class RenderTarget
{
    TKIT_NON_COPYABLE(RenderTarget)
  public:
    RenderTarget() = default;
    virtual ~RenderTarget();

    template <Dimension D> RenderView<D> *CreateRenderView(Camera<D> *camera, RenderViewFlags flags = 0);

    template <Dimension D> void DestroyRenderView(RenderView<D> *rv);

    template <Dimension D> void BringToTop(RenderView<D> *rv)
    {
        rv->Layer = m_LayerAssign.ToTop();
    }
    template <Dimension D> void BringToBottom(RenderView<D> *rv)
    {
        rv->Layer = m_LayerAssign.ToBottom();
    }
    template <Dimension D> const TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &GetRenderViews() const
    {
        if constexpr (D == D2)
            return m_RenderViews2;
        else
            return m_RenderViews3;
    }

    virtual RenderTargetInfo CreateRenderTargetInfo()
    {
        RenderTargetInfo info;
        info.Views2 = getSortedViews<D2>();
        info.Views3 = getSortedViews<D3>();
        info.ImageAvailableSemaphore = 0;
        info.RenderFinishedSemaphore = 0;
        return info;
    }

    Color ClearColor = Color_Black;

  protected:
    void updateRenderViews();
    void findAvailableFramebuffers();

    // TODO(Isma): Implement sort here somehow. rename this to get sorted views
    template <Dimension D> TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> getSortedViews() const
    {
        TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> views = GetRenderViews<D>();
        std::sort(views.begin(), views.end(),
                  [](const RenderView<D> *rv1, const RenderView<D> *rv2) { return rv1->Layer < rv2->Layer; });
        return views;
    }
    template <Dimension D> TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &getRenderViews()
    {
        if constexpr (D == D2)
            return m_RenderViews2;
        else
            return m_RenderViews3;
    }

  private:
    virtual u32v2 getExtent() const = 0;

    TKit::StaticArray<RenderView<D2> *, ONYX_MAX_VIEWS> m_RenderViews2{};
    TKit::StaticArray<RenderView<D3> *, ONYX_MAX_VIEWS> m_RenderViews3{};
    LayerAssign m_LayerAssign{};
};
} // namespace Onyx

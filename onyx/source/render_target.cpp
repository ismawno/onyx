#include "pch.hpp"
#include "onyx/render_target.hpp"

namespace Onyx
{
RenderTarget::~RenderTarget()
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (const RenderView<D2> *rv : m_RenderViews2)
        tier->Destroy(rv);
    for (const RenderView<D3> *rv : m_RenderViews3)
        tier->Destroy(rv);
}
template <Dimension D> RenderView<D> *RenderTarget::CreateRenderView(Camera<D> *camera, const RenderViewSpecs &specs)
{
    TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &views = getRenderViews<D>();

    TKit::TierAllocator *tier = TKit::GetTier();
    const u32v2 size = getSize();
    const f32v2 scale = getContentScale();
    RenderView<D> *rv = tier->Create<RenderView<D>>(size, scale, camera, specs);
    views.Append(rv);

    rv->Layer = m_LayerAssign.ToTop();
    return rv;
}

template <Dimension D> void RenderTarget::DestroyRenderView(const RenderView<D> *rv)
{
    TKit::StaticArray<RenderView<D> *, ONYX_MAX_VIEWS> &views = getRenderViews<D>();
    for (u32 i = 0; i < views.GetSize(); ++i)
        if (views[i] == rv)
        {
            TKit::TierAllocator *tier = TKit::GetTier();
            tier->Destroy(rv);
            views.RemoveUnordered(views.begin() + i);
            return;
        }
    TKIT_FATAL("[ONYX][WINDOW] Render view to destroy not found");
}
void RenderTarget::updateRenderViews()
{
    const u32v2 size = getSize();
    const f32v2 scale = getContentScale();
    for (RenderView<D2> *rv : m_RenderViews2)
        rv->update(size, scale);
    for (RenderView<D3> *rv : m_RenderViews3)
        rv->update(size, scale);
}
void RenderTarget::findAvailableFramebuffers()
{
    for (RenderView<D2> *rv : m_RenderViews2)
        rv->findAvailableFramebuffer();
    for (RenderView<D3> *rv : m_RenderViews3)
        rv->findAvailableFramebuffer();
}

template RenderView<D2> *RenderTarget::CreateRenderView<D2>(Camera<D2> *camera, const RenderViewSpecs &specs);
template RenderView<D3> *RenderTarget::CreateRenderView<D3>(Camera<D3> *camera, const RenderViewSpecs &specs);

template void RenderTarget::DestroyRenderView<D2>(const RenderView<D2> *view);
template void RenderTarget::DestroyRenderView<D3>(const RenderView<D3> *view);
} // namespace Onyx

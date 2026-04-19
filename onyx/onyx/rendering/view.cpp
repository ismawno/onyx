#include "onyx/core/pch.hpp"
#include "onyx/rendering/view.hpp"
#include "onyx/property/transform.hpp"
#include "onyx/rendering/renderer.hpp"

namespace Onyx
{
// TODO(Isma): Consider having 2D and 3D view sets
static ViewMask s_ViewCache = TKit::Limits<ViewMask>::Max();
static ViewMask allocateViewBit()
{
    TKIT_ASSERT(s_ViewCache != 0,
                "[ONYX][WINDOW] Maximum amount of windows exceeded. There is a hard limit of {} windows",
                8 * sizeof(ViewMask));

    const u32 index = u32(std::countr_zero(s_ViewCache));
    const ViewMask viewBit = 1 << index;
    s_ViewCache &= ~viewBit;
    return viewBit;
}
static void deallocateViewBit(const ViewMask viewBit)
{
    s_ViewCache |= viewBit;
}

template <Dimension D> static void applyCoordinateSystemExtrinsic(f32m<D> &transform)
{
    // Essentially, a rotation around the x axis
    for (u32 i = 0; i < D + 1; ++i)
        for (u32 j = 1; j < D; ++j)
            transform[i][j] = -transform[i][j];
}

VkRect2D ScreenScissor::AsVulkanScissor(const VkExtent2D &extent, const ScreenViewport &viewport) const
{
    const f32v2 size = viewport.Max - viewport.Min;
    const f32v2 min = viewport.Min + 0.5f * (1.f + Min) * size;
    const f32v2 max = viewport.Min + 0.5f * (1.f + Max) * size;

    VkRect2D scissor;
    scissor.offset.x = i32(0.5f * (1.f + min[0]) * extent.width);
    scissor.offset.y = i32(0.5f * (1.f - max[1]) * extent.height);
    scissor.extent.width = u32(0.5f * (1.f + max[0]) * extent.width) - u32(scissor.offset.x);
    scissor.extent.height = u32(0.5f * (1.f - min[1]) * extent.height) - u32(scissor.offset.y);

    return scissor;
}
VkViewport ScreenViewport::AsVulkanViewport(const VkExtent2D &extent) const
{
    VkViewport viewport;
    viewport.x = 0.5f * (1.f + Min[0]) * extent.width;
    viewport.y = 0.5f * (1.f - Max[1]) * extent.height;
    viewport.width = 0.5f * (1.f + Max[0]) * extent.width - viewport.x;
    viewport.height = 0.5f * (1.f - Min[1]) * extent.height - viewport.y;
    viewport.minDepth = DepthBounds[0];
    viewport.maxDepth = DepthBounds[1];

    return viewport;
}
template <Dimension D>
RenderView<D>::RenderView(Camera<D> *camera, const ScreenViewport &viewport, const ScreenScissor &scissor)
    : m_Camera(camera), m_Viewport(viewport), m_Scissor(scissor)
{
    m_ViewBit = allocateViewBit();
}
template <Dimension D> RenderView<D>::~RenderView()
{
    Renderer::RemoveTarget(m_ViewBit);
    deallocateViewBit(m_ViewBit);
}

template <Dimension D> f32v2 RenderView<D>::ScreenToViewport(const f32v2 &screenPos) const
{
    const f32v2 size = m_Viewport.Max - m_Viewport.Min;
    return -1.f + 2.f * (screenPos - m_Viewport.Min) / size;
}

template <Dimension D> f32v<D> RenderView<D>::ViewportToWorld(f32v<D> viewportPos) const
{
    viewportPos[1] = -viewportPos[1]; // Invert y axis to undo onyx's inversion to GLFW
    const f32m<D> pv = Math::Inverse(m_ProjectionView);
    if constexpr (D == D2)
        return pv * f32v3{viewportPos, 1.f};
    else
    {
        const f32v4 clip = pv * f32v4{viewportPos, 1.f};
        return f32v3{clip} / clip[3];
    }
}

template <Dimension D> f32v2 RenderView<D>::WorldToViewport(const f32v<D> &worldPos) const
{
    if constexpr (D == D2)
    {
        f32v2 viewportPos = f32v2{m_ProjectionView * f32v3{worldPos, 1.f}};
        viewportPos[1] = -viewportPos[1];
        return viewportPos;
    }
    else
    {
        f32v4 clip = m_ProjectionView * f32v4{worldPos, 1.f};
        clip[1] = -clip[1];
        return f32v3{clip} / clip[3];
    }
}

template <Dimension D> f32v2 RenderView<D>::ViewportToScreen(const f32v2 &viewportPos) const
{
    const f32v2 size = m_Viewport.Max - m_Viewport.Min;
    return m_Viewport.Min + 0.5f * (1.f + viewportPos) * size;
}

template <Dimension D> f32v<D> RenderView<D>::ScreenToWorld(const f32v<D> &screenPos) const
{
    if constexpr (D == D2)
        return ViewportToWorld(ScreenToViewport(screenPos));
    else
    {
        const f32 z = screenPos[2];
        return ViewportToWorld(f32v3{ScreenToViewport(f32v2{screenPos}), z});
    }
}

template <Dimension D> f32m<D> RenderView<D>::ComputeView() const
{
    if (m_Mode == ViewMode_Manual)
        return m_View;

    f32m<D> view = m_Camera->View.ComputeInverseTransform();
    applyCoordinateSystemExtrinsic<D>(view);
    return view;
}
template <Dimension D> f32m<D> RenderView<D>::ComputeProjection() const
{
    if (m_Mode == ViewMode_Manual)
        return m_View;

    const VkViewport viewport = m_Viewport.AsVulkanViewport(m_Extent);
    const f32 aspect = viewport.width / viewport.height;
    if constexpr (D == D2)
        return Transform<D2>::Orthographic(m_Camera->OrthoParameters.Size, aspect);
    else
    {
        const OrthographicParameters<D3> &oparams = m_Camera->OrthoParameters;
        const PerspectiveParameters &pparams = m_Camera->PerspParameters;
        return m_Camera->Mode == CameraMode_Perspective
                   ? Transform<D3>::Perspective(pparams.FieldOfView, pparams.Near, pparams.Far, aspect)
                   : Transform<D3>::Orthographic(oparams.Size, aspect, oparams.Near, oparams.Far);
    }
}

template <Dimension D> void RenderView<D>::ZoomScroll(const f32v<D> &screenPos, const f32 step)
{
    const f32 factor = 1.f - step;
    const auto doZoom = [&] {
        m_Camera->OrthoParameters.Size *= factor;
        const f32v<D> wb = ScreenToWorld(screenPos);
        CacheMatrices();
        const f32v<D> wa = ScreenToWorld(screenPos);
        m_Camera->View.Translation += wb - wa;
    };
    if constexpr (D == D2)
        doZoom();
    else
    {
        if (m_Camera->Mode == CameraMode_Orthographic)
            doZoom();
        else
            m_Camera->PerspParameters.FieldOfView *= factor;
    }
}

template class RenderView<D2>;
template class RenderView<D3>;

} // namespace Onyx

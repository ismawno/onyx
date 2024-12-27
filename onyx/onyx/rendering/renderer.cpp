#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/app/window.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/utilities/math.hpp"

namespace Onyx
{
template <Dimension D, template <Dimension, PipelineMode> typename R>
RenderSystem<D, R>::RenderSystem(const VkRenderPass p_RenderPass) noexcept
    : NoStencilWriteDoFill(p_RenderPass), DoStencilWriteDoFill(p_RenderPass), DoStencilWriteNoFill(p_RenderPass),
      DoStencilTestNoFill(p_RenderPass)
{
}

template <Dimension D, template <Dimension, PipelineMode> typename R> void RenderSystem<D, R>::Flush() noexcept
{
    NoStencilWriteDoFill.Flush();
    DoStencilWriteDoFill.Flush();
    DoStencilWriteNoFill.Flush();
    DoStencilTestNoFill.Flush();
}

static VkDescriptorSet resetLightBufferDescriptorSet(const VkDescriptorBufferInfo &p_DirectionalInfo,
                                                     const VkDescriptorBufferInfo &p_PointInfo,
                                                     VkDescriptorSet p_OldSet = VK_NULL_HANDLE) noexcept
{
    const VKit::DescriptorSetLayout &layout = Core::GetLightStorageDescriptorSetLayout();
    const VKit::DescriptorPool &pool = Core::GetDescriptorPool();

    VKit::DescriptorSet::Writer writer(Core::GetDevice(), &layout);
    writer.WriteBuffer(0, p_DirectionalInfo);
    writer.WriteBuffer(1, p_PointInfo);

    if (!p_OldSet)
    {
        const auto result = pool.Allocate(layout);
        VKIT_ASSERT_RESULT(result);
        p_OldSet = result.GetValue();
    }
    writer.Overwrite(p_OldSet);
    return p_OldSet;
}

template <Dimension D>
IRenderer<D>::IRenderer(const VkRenderPass p_RenderPass, const TKit::StaticArray8<RenderState<D>> *p_State,
                        const ProjectionViewData<D> *p_ProjectionView) noexcept
    : m_MeshRenderer(p_RenderPass), m_PrimitiveRenderer(p_RenderPass), m_PolygonRenderer(p_RenderPass),
      m_CircleRenderer(p_RenderPass), m_ProjectionView(p_ProjectionView), m_State(p_State)
{
}

Renderer<D3>::Renderer(const VkRenderPass p_RenderPass, const TKit::StaticArray8<RenderState<D3>> *p_State,
                       const ProjectionViewData<D3> *p_ProjectionView) noexcept
    : IRenderer<D3>(p_RenderPass, p_State, p_ProjectionView)
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkDescriptorBufferInfo dirInfo = m_DeviceLightData.DirectionalLightBuffers[i].GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.PointLightBuffers[i].GetDescriptorInfo();
        m_DeviceLightData.DescriptorSets[i] = resetLightBufferDescriptorSet(dirInfo, pointInfo);
    }
}

DeviceLightData::DeviceLightData(const usize p_Capacity) noexcept
{
    for (usize i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DirectionalLightBuffers[i] = Core::CreateMutableStorageBuffer<DirectionalLight>(p_Capacity);
        PointLightBuffers[i] = Core::CreateMutableStorageBuffer<PointLight>(p_Capacity);
    }
}
DeviceLightData::~DeviceLightData() noexcept
{
    for (usize i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DirectionalLightBuffers[i].Destroy();
        PointLightBuffers[i].Destroy();
    }
}

static mat4 transform3ToTransform4(const mat3 &p_Transform) noexcept
{
    mat4 t4{1.f};
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

template <Dimension D, typename IData>
static IData createFullDrawInstanceData(const mat<D> &p_Transform, const MaterialData<D> &p_Material,
                                        [[maybe_unused]] u32 &p_ZOffset) noexcept
{
    IData instanceData{};
    if constexpr (D == D2)
    {
        instanceData.Transform = transform3ToTransform4(p_Transform);
        ApplyCoordinateSystemExtrinsic(instanceData.Transform);
        instanceData.Transform[3][2] = 1.f - ++p_ZOffset * glm::epsilon<f32>();
    }
    else
    {
        instanceData.Transform = p_Transform;
        instanceData.NormalMatrix = mat4(glm::transpose(glm::inverse(mat3(instanceData.Transform))));
    }
    instanceData.Material = p_Material;

    return instanceData;
}
// ProjectionView is needed here, because stencil uses 2D shaders, where they require the transform to be complete.
template <Dimension D, typename IData>
static IData createStencilInstanceData(const mat<D> &p_Transform, const RenderState<D> &p_State, const u8 p_Flags,
                                       [[maybe_unused]] const mat<D> &p_ProjectionView,
                                       [[maybe_unused]] u32 &p_ZOffset) noexcept
{
    IData instanceData{};
    if constexpr (D == D2)
    {
        mat3 transform = p_Transform;
        if (p_Flags & DrawFlags_DoStencilScale)
            Transform<D2>::ScaleIntrinsic(transform, vec2{1.f + p_State.OutlineWidth});
        instanceData.Transform = transform3ToTransform4(transform);
        ApplyCoordinateSystemExtrinsic(instanceData.Transform);
        instanceData.Transform[3][2] = 1.f - ++p_ZOffset * glm::epsilon<f32>();
    }
    else
    {
        instanceData.Transform = p_ProjectionView * p_Transform;
        if (p_Flags & DrawFlags_DoStencilScale)
            Transform<D3>::ScaleIntrinsic(instanceData.Transform, vec3{1.f + p_State.OutlineWidth});
    }

    instanceData.Material.Color = p_State.OutlineColor;
    return instanceData;
}

template <Dimension D>
template <typename Renderer, typename... DrawArgs>
void IRenderer<D>::draw(Renderer &p_Renderer, const mat<D> &p_Transform, u8 p_Flags, DrawArgs &&...p_Args) noexcept
{
    const RenderState<D> &state = m_State->back();
    TKIT_ASSERT(state.OutlineWidth >= 0.f, "[ONYX] Outline width must be non-negative");

    if (p_Flags & DrawFlags_Auto)
    {
        p_Flags = DrawFlags_DoStencilScale;
        if (state.Fill)
        {
            if (state.Outline)
                p_Flags |= DrawFlags_DoStencilWriteDoFill | DrawFlags_DoStencilTestNoFill;
            else
                p_Flags |= DrawFlags_NoStencilWriteDoFill;
        }
        else if (state.Outline)
            p_Flags |= DrawFlags_DoStencilWriteNoFill | DrawFlags_DoStencilTestNoFill;
    }

    using FillIData = InstanceData<D, DrawMode::Fill>;
    using StencilIData = InstanceData<D, DrawMode::Stencil>;
    if (p_Flags & DrawFlags_NoStencilWriteDoFill)
    {
        const FillIData instanceData = createFullDrawInstanceData<D, FillIData>(p_Transform, state.Material, m_ZOffset);
        p_Renderer.NoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArgs>(p_Args)...);
        ++m_DrawCount;
    }
    if (p_Flags & DrawFlags_DoStencilWriteDoFill)
    {
        const FillIData instanceData = createFullDrawInstanceData<D, FillIData>(p_Transform, state.Material, m_ZOffset);
        p_Renderer.DoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArgs>(p_Args)...);
        ++m_DrawCount;
    }
    if (p_Flags & DrawFlags_DoStencilWriteNoFill)
    {
        const StencilIData instanceData = createStencilInstanceData<D, StencilIData>(
            p_Transform, state, 0, m_ProjectionView->ProjectionView, m_ZOffset);
        p_Renderer.DoStencilWriteNoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArgs>(p_Args)...);
        ++m_DrawCount;
    }
    if (p_Flags & DrawFlags_DoStencilTestNoFill)
    {
        const StencilIData instanceData = createStencilInstanceData<D, StencilIData>(
            p_Transform, state, p_Flags, m_ProjectionView->ProjectionView, m_ZOffset);
        p_Renderer.DoStencilTestNoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArgs>(p_Args)...);
        ++m_DrawCount;
    }
}

template <Dimension D>
static mat<D> finalTransform(const mat<D> &p_Transform, const RenderState<D> &p_State,
                             [[maybe_unused]] const mat<D> &p_ProjectionView) noexcept
{
    if constexpr (D == D2)
        return p_ProjectionView * p_State.Axes * p_Transform;
    else
        return p_State.Axes * p_Transform;
}

template <Dimension D>
void IRenderer<D>::DrawMesh(const mat<D> &p_Transform, const Model<D> &p_Model, const u8 p_Flags) noexcept
{
    const mat<D> transform = finalTransform<D>(p_Transform, m_State->back(), m_ProjectionView->ProjectionView);
    draw(m_MeshRenderer, transform, p_Flags, p_Model);
}

template <Dimension D>
void IRenderer<D>::DrawPrimitive(const mat<D> &p_Transform, const usize p_PrimitiveIndex, const u8 p_Flags) noexcept
{
    const mat<D> transform = finalTransform<D>(p_Transform, m_State->back(), m_ProjectionView->ProjectionView);
    draw(m_PrimitiveRenderer, transform, p_Flags, p_PrimitiveIndex);
}

template <Dimension D>
void IRenderer<D>::DrawPolygon(const mat<D> &p_Transform, const std::span<const vec<D>> p_Vertices,
                               const u8 p_Flags) noexcept
{
    const mat<D> transform = finalTransform<D>(p_Transform, m_State->back(), m_ProjectionView->ProjectionView);
    draw(m_PolygonRenderer, transform, p_Flags, p_Vertices);
}

template <Dimension D>
void IRenderer<D>::DrawCircleOrArc(const mat<D> &p_Transform, const f32 p_InnerFade, const f32 p_OuterFade,
                                   const f32 p_Hollowness, const f32 p_LowerAngle, const f32 p_UpperAngle,
                                   const u8 p_Flags) noexcept
{
    const mat<D> transform = finalTransform<D>(p_Transform, m_State->back(), m_ProjectionView->ProjectionView);
    draw(m_CircleRenderer, transform, p_Flags, p_InnerFade, p_OuterFade, p_Hollowness, p_LowerAngle, p_UpperAngle);
}
template <Dimension D>
void IRenderer<D>::DrawCircleOrArc(const mat<D> &p_Transform, const u8 p_Flags, const f32 p_InnerFade,
                                   const f32 p_OuterFade, const f32 p_Hollowness, const f32 p_LowerAngle,
                                   const f32 p_UpperAngle) noexcept
{
    const mat<D> transform = finalTransform<D>(p_Transform, m_State->back(), m_ProjectionView->ProjectionView);
    draw(m_CircleRenderer, transform, p_Flags, p_InnerFade, p_OuterFade, p_Hollowness, p_LowerAngle, p_UpperAngle);
}

void Renderer<D2>::Flush() noexcept
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();
    m_DrawCount = 0;
}
void Renderer<D3>::Flush() noexcept
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();

    m_DirectionalLights.clear();
    m_PointLights.clear();
    m_DrawCount = 0;
}

void Renderer<D2>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_DrawCount == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D2>::Render");
    RenderInfo<D2, DrawMode::Fill> fillDrawInfo;
    fillDrawInfo.CommandBuffer = p_CommandBuffer;
    fillDrawInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);
    m_PrimitiveRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);
    m_PolygonRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);
    m_CircleRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);

    m_MeshRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);
    m_PrimitiveRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);
    m_PolygonRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);
    m_CircleRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);

    RenderInfo<D2, DrawMode::Stencil> stencilInfo;
    stencilInfo.CommandBuffer = p_CommandBuffer;
    stencilInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.DoStencilWriteNoFill.Render(stencilInfo);
    m_PrimitiveRenderer.DoStencilWriteNoFill.Render(stencilInfo);
    m_PolygonRenderer.DoStencilWriteNoFill.Render(stencilInfo);
    m_CircleRenderer.DoStencilWriteNoFill.Render(stencilInfo);

    m_MeshRenderer.DoStencilTestNoFill.Render(stencilInfo);
    m_PrimitiveRenderer.DoStencilTestNoFill.Render(stencilInfo);
    m_PolygonRenderer.DoStencilTestNoFill.Render(stencilInfo);
    m_CircleRenderer.DoStencilTestNoFill.Render(stencilInfo);

    m_FrameIndex = (m_FrameIndex + 1) % ONYX_MAX_FRAMES_IN_FLIGHT;
    m_ZOffset = 0;
}

void Renderer<D3>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    if (m_DrawCount == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D3>::Render");
    RenderInfo<D3, DrawMode::Fill> fillDrawInfo;
    fillDrawInfo.CommandBuffer = p_CommandBuffer;
    fillDrawInfo.FrameIndex = m_FrameIndex;
    fillDrawInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[m_FrameIndex];
    fillDrawInfo.DirectionalLightCount = static_cast<u32>(m_DirectionalLights.size());
    fillDrawInfo.PointLightCount = static_cast<u32>(m_PointLights.size());
    fillDrawInfo.ProjectionView = &m_ProjectionView->ProjectionView;
    fillDrawInfo.ViewPosition = &m_ProjectionView->View.Translation;
    fillDrawInfo.AmbientColor = &AmbientColor;

    for (usize i = 0; i < m_DirectionalLights.size(); ++i)
        m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex].WriteAt(i, &m_DirectionalLights[i]);
    for (usize i = 0; i < m_PointLights.size(); ++i)
        m_DeviceLightData.PointLightBuffers[m_FrameIndex].WriteAt(i, &m_PointLights[i]);

    m_MeshRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);
    m_PrimitiveRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);
    m_PolygonRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);
    m_CircleRenderer.NoStencilWriteDoFill.Render(fillDrawInfo);

    m_MeshRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);
    m_PrimitiveRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);
    m_PolygonRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);
    m_CircleRenderer.DoStencilWriteDoFill.Render(fillDrawInfo);

    RenderInfo<D3, DrawMode::Stencil> stencilInfo;
    stencilInfo.CommandBuffer = p_CommandBuffer;
    stencilInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.DoStencilWriteNoFill.Render(stencilInfo);
    m_PrimitiveRenderer.DoStencilWriteNoFill.Render(stencilInfo);
    m_PolygonRenderer.DoStencilWriteNoFill.Render(stencilInfo);
    m_CircleRenderer.DoStencilWriteNoFill.Render(stencilInfo);

    m_MeshRenderer.DoStencilTestNoFill.Render(stencilInfo);
    m_PrimitiveRenderer.DoStencilTestNoFill.Render(stencilInfo);
    m_PolygonRenderer.DoStencilTestNoFill.Render(stencilInfo);
    m_CircleRenderer.DoStencilTestNoFill.Render(stencilInfo);

    m_FrameIndex = (m_FrameIndex + 1) % ONYX_MAX_FRAMES_IN_FLIGHT;
}

void Renderer<D3>::AddDirectionalLight(const DirectionalLight &p_Light) noexcept
{
    const usize size = m_DirectionalLights.size();
    auto &buffer = m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex];
    if (buffer.GetInfo().InstanceCount == size)
    {
        buffer.Destroy();
        buffer = Core::CreateMutableStorageBuffer<DirectionalLight>(size * 2);
        const VkDescriptorBufferInfo dirInfo = buffer.GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.PointLightBuffers[m_FrameIndex].GetDescriptorInfo();

        m_DeviceLightData.DescriptorSets[m_FrameIndex] =
            resetLightBufferDescriptorSet(dirInfo, pointInfo, m_DeviceLightData.DescriptorSets[m_FrameIndex]);
    }

    m_DirectionalLights.push_back(p_Light);
}

void Renderer<D3>::AddPointLight(const PointLight &p_Light) noexcept
{
    const usize size = m_PointLights.size();
    MutableStorageBuffer<PointLight> &buffer = m_DeviceLightData.PointLightBuffers[m_FrameIndex];
    if (buffer.GetInfo().InstanceCount == size)
    {
        buffer.Destroy();
        buffer = Core::CreateMutableStorageBuffer<PointLight>(size * 2);
        const VkDescriptorBufferInfo dirInfo =
            m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex].GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = buffer.GetDescriptorInfo();

        m_DeviceLightData.DescriptorSets[m_FrameIndex] =
            resetLightBufferDescriptorSet(dirInfo, pointInfo, m_DeviceLightData.DescriptorSets[m_FrameIndex]);
    }

    m_PointLights.push_back(p_Light);
}

template class IRenderer<D2>;
template class IRenderer<D3>;

} // namespace Onyx
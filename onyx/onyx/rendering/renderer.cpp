#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/app/window.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/utils/math.hpp"

namespace Onyx::Detail
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
IRenderer<D>::IRenderer(const VkRenderPass p_RenderPass, const ProjectionViewData<D> *p_ProjectionView) noexcept
    : m_MeshRenderer(p_RenderPass), m_PrimitiveRenderer(p_RenderPass), m_PolygonRenderer(p_RenderPass),
      m_CircleRenderer(p_RenderPass), m_ProjectionView(p_ProjectionView)
{
}

Renderer<D3>::Renderer(const VkRenderPass p_RenderPass, const ProjectionViewData<D3> *p_ProjectionView) noexcept
    : IRenderer<D3>(p_RenderPass, p_ProjectionView)
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkDescriptorBufferInfo dirInfo = m_DeviceLightData.DirectionalLightBuffers[i].GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.PointLightBuffers[i].GetDescriptorInfo();
        m_DeviceLightData.DescriptorSets[i] = resetLightBufferDescriptorSet(dirInfo, pointInfo);
    }
}

DeviceLightData::DeviceLightData(const u32 p_Capacity) noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DirectionalLightBuffers[i] = Core::CreateMutableStorageBuffer<DirectionalLight>(p_Capacity);
        PointLightBuffers[i] = Core::CreateMutableStorageBuffer<PointLight>(p_Capacity);
    }
}
DeviceLightData::~DeviceLightData() noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DirectionalLightBuffers[i].Destroy();
        PointLightBuffers[i].Destroy();
    }
}

static fmat4 transform3ToTransform4(const fmat3 &p_Transform) noexcept
{
    fmat4 t4{1.f};
    t4[0][0] = p_Transform[0][0];
    t4[0][1] = p_Transform[0][1];
    t4[1][0] = p_Transform[1][0];
    t4[1][1] = p_Transform[1][1];

    t4[3][0] = p_Transform[2][0];
    t4[3][1] = p_Transform[2][1];
    return t4;
}

template <Dimension D, typename IData>
static IData createFullDrawInstanceData(const fmat<D> &p_Transform, const MaterialData<D> &p_Material,
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
        instanceData.NormalMatrix = fmat4(glm::transpose(glm::inverse(fmat3(instanceData.Transform))));
    }
    instanceData.Material = p_Material;

    return instanceData;
}
// ProjectionView is needed here, because stencil uses 2D shaders, where they require the transform to be complete.
template <Dimension D, typename IData>
static IData createStencilInstanceData(const fmat<D> &p_Transform, const Color &p_OutlineColor,
                                       [[maybe_unused]] const fmat<D> &p_ProjectionView,
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
        instanceData.Transform = p_ProjectionView * p_Transform;

    instanceData.Material.Color = p_OutlineColor;
    return instanceData;
}

template <Dimension D>
template <typename Renderer, typename DrawArg>
void IRenderer<D>::draw(Renderer &p_Renderer, const RenderState<D> *p_State, const fmat<D> &p_Transform,
                        DrawArg &&p_Arg, DrawFlags p_Flags) noexcept
{
    TKIT_ASSERT(p_State->OutlineWidth >= 0.f, "[ONYX] Outline width must be non-negative");

    using FillIData = InstanceData<D, DrawMode::Fill>;
    using StencilIData = InstanceData<D, DrawMode::Stencil>;
    if (p_Flags & DrawFlags_NoStencilWriteDoFill)
    {
        const FillIData instanceData =
            createFullDrawInstanceData<D, FillIData>(p_Transform, p_State->Material, m_ZOffset);
        p_Renderer.NoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlags_DoStencilWriteDoFill)
    {
        const FillIData instanceData =
            createFullDrawInstanceData<D, FillIData>(p_Transform, p_State->Material, m_ZOffset);
        p_Renderer.DoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlags_DoStencilWriteNoFill)
    {
        const StencilIData instanceData = createStencilInstanceData<D, StencilIData>(
            p_Transform, p_State->OutlineColor, m_ProjectionView->ProjectionView, m_ZOffset);
        if constexpr (!std::is_same_v<Renderer, RenderSystem<D, CircleRenderer>>)
            p_Renderer.DoStencilWriteNoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArg>(p_Arg));
        else
        {
            CircleOptions options = p_Arg;
            options.InnerFade = 0.f;
            options.OuterFade = 0.f;
            p_Renderer.DoStencilWriteNoFill.Draw(m_FrameIndex, instanceData, options);
        }
    }
    if (p_Flags & DrawFlags_DoStencilTestNoFill)
    {
        const StencilIData instanceData = createStencilInstanceData<D, StencilIData>(
            p_Transform, p_State->OutlineColor, m_ProjectionView->ProjectionView, m_ZOffset);
        if constexpr (!std::is_same_v<Renderer, RenderSystem<D, CircleRenderer>>)
            p_Renderer.DoStencilTestNoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArg>(p_Arg));
        else
        {
            CircleOptions options = p_Arg;
            options.InnerFade = 0.f;
            options.OuterFade = 0.f;
            p_Renderer.DoStencilTestNoFill.Draw(m_FrameIndex, instanceData, options);
        }
    }
}

template <Dimension D>
void IRenderer<D>::DrawMesh(const RenderState<D> *p_State, const fmat<D> &p_Transform, const Model<D> &p_Model,
                            const DrawFlags p_Flags) noexcept
{
    draw(m_MeshRenderer, p_State, p_Transform, p_Model, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawPrimitive(const RenderState<D> *p_State, const fmat<D> &p_Transform, const u32 p_PrimitiveIndex,
                                 const DrawFlags p_Flags) noexcept
{
    draw(m_PrimitiveRenderer, p_State, p_Transform, p_PrimitiveIndex, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawPolygon(const RenderState<D> *p_State, const fmat<D> &p_Transform,
                               const TKit::Span<const fvec2> p_Vertices, const DrawFlags p_Flags) noexcept
{
    draw(m_PolygonRenderer, p_State, p_Transform, p_Vertices, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawCircle(const RenderState<D> *p_State, const fmat<D> &p_Transform, const CircleOptions &p_Options,
                              const DrawFlags p_Flags) noexcept
{
    draw(m_CircleRenderer, p_State, p_Transform, p_Options, p_Flags);
}

void Renderer<D2>::Flush() noexcept
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();
}
void Renderer<D3>::Flush() noexcept
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();

    m_DirectionalLights.clear();
    m_PointLights.clear();
}

void Renderer<D2>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
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
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D3>::Render");
    RenderInfo<D3, DrawMode::Fill> fillDrawInfo;
    fillDrawInfo.CommandBuffer = p_CommandBuffer;
    fillDrawInfo.FrameIndex = m_FrameIndex;
    fillDrawInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[m_FrameIndex];
    fillDrawInfo.DirectionalLightCount = m_DirectionalLights.size();
    fillDrawInfo.PointLightCount = m_PointLights.size();
    fillDrawInfo.ProjectionView = &m_ProjectionView->ProjectionView;
    fillDrawInfo.ViewPosition = &m_ProjectionView->View.Translation;
    fillDrawInfo.AmbientColor = &AmbientColor;

    for (u32 i = 0; i < m_DirectionalLights.size(); ++i)
        m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex].WriteAt(i, &m_DirectionalLights[i]);
    m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex].Flush();

    for (u32 i = 0; i < m_PointLights.size(); ++i)
        m_DeviceLightData.PointLightBuffers[m_FrameIndex].WriteAt(i, &m_PointLights[i]);
    m_DeviceLightData.PointLightBuffers[m_FrameIndex].Flush();

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
    const u32 size = m_DirectionalLights.size();
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
    const u32 size = m_PointLights.size();
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

template class ONYX_API IRenderer<D2>;
template class ONYX_API IRenderer<D3>;

} // namespace Onyx::Detail
#include "core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/window.hpp"
#include "kit/utilities/math.hpp"

namespace ONYX
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
                                                     const DescriptorSetLayout *p_Layout, const DescriptorPool *p_Pool,
                                                     const VkDescriptorSet p_OldSet = VK_NULL_HANDLE) noexcept
{
    if (p_OldSet)
        p_Pool->Deallocate(p_OldSet);

    DescriptorWriter writer(p_Layout, p_Pool);
    writer.WriteBuffer(0, &p_DirectionalInfo);
    writer.WriteBuffer(1, &p_PointInfo);
    return writer.Build();
}

template <Dimension D>
IRenderer<D>::IRenderer(const VkRenderPass p_RenderPass, const DynamicArray<RenderState<D>> *p_State) noexcept
    : m_MeshRenderer(p_RenderPass), m_PrimitiveRenderer(p_RenderPass), m_PolygonRenderer(p_RenderPass),
      m_CircleRenderer(p_RenderPass), m_State(p_State)
{
}

Renderer<D3>::Renderer(const VkRenderPass p_RenderPass, const DynamicArray<RenderState<D3>> *p_State) noexcept
    : IRenderer<D3>(p_RenderPass, p_State)
{
    m_DescriptorPool = Core::GetDescriptorPool();
    m_DescriptorSetLayout = Core::GetLightStorageDescriptorSetLayout();
    for (u32 i = 0; i < SwapChain::MFIF; ++i)
    {
        const VkDescriptorBufferInfo dirInfo = m_DeviceLightData.DirectionalLightBuffers[i]->GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.PointLightBuffers[i]->GetDescriptorInfo();
        m_DeviceLightData.DescriptorSets[i] =
            resetLightBufferDescriptorSet(dirInfo, pointInfo, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get());
    }
}

DeviceLightData::DeviceLightData(const usize p_Capacity) noexcept
{
    for (usize i = 0; i < SwapChain::MFIF; ++i)
    {
        DirectionalLightBuffers[i].Create(p_Capacity);
        PointLightBuffers[i].Create(p_Capacity);
    }
}
DeviceLightData::~DeviceLightData() noexcept
{
    for (usize i = 0; i < SwapChain::MFIF; ++i)
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

static IData createFullDrawInstanceData(const mat<D> &p_Transform, const RenderState<D> &p_State,
                                        [[maybe_unused]] u32 &p_ZOffset) noexcept
{
    IData instanceData{};
    if constexpr (D == D2)
    {
        instanceData.Transform = transform3ToTransform4(p_State.Axes * p_Transform);
        // And now, we apply the coordinate system for 2D cases, which might seem that it is applied after the
        // projection, but it is cool because I use no projection for 2D
        ApplyCoordinateSystem(instanceData.Transform);
        instanceData.Transform[3][2] = 1.f - ++p_ZOffset * glm::epsilon<f32>();
    }
    else
    {
        instanceData.Transform = p_Transform;
        instanceData.ProjectionView = p_State.HasProjection ? p_State.Projection * p_State.Axes : p_State.Axes;
        instanceData.NormalMatrix = mat4(glm::transpose(glm::inverse(mat3(p_Transform))));
        instanceData.ViewPosition = p_State.InverseAxes[3];
    }
    instanceData.Material = p_State.Material;

    return instanceData;
}

template <Dimension D> static void computeOutlineScale(mat<D> &p_Transform, const f32 p_OutlineWidth) noexcept
{
    Transform<D>::ScaleIntrinsic(p_Transform, vec<D>{1.f + p_OutlineWidth});
}

template <Dimension D, typename IData, bool Scaled = true>
static IData createStencilInstanceData(const mat<D> &p_Transform, const RenderState<D> &p_State,
                                       [[maybe_unused]] u32 &p_ZOffset) noexcept
{
    IData instanceData{};
    if constexpr (D == D2)
    {
        mat3 transform = p_State.Axes * p_Transform;
        if constexpr (Scaled)
            Transform<D2>::ScaleIntrinsic(transform, vec2{1.f + p_State.OutlineWidth});
        instanceData.Transform = transform3ToTransform4(transform);
        // And now, we apply the coordinate system for 2D cases, which might seem that it is applied after the
        // projection, but it is cool because I use no projection for 2D
        ApplyCoordinateSystem(instanceData.Transform);
        instanceData.Transform[3][2] = 1.f - ++p_ZOffset * glm::epsilon<f32>();
    }
    else
    {
        mat4 transform = (p_State.HasProjection ? p_State.Projection * p_State.Axes : p_State.Axes) * p_Transform;
        if constexpr (Scaled)
            Transform<D3>::ScaleIntrinsic(transform, vec3{1.f + p_State.OutlineWidth});
        instanceData.Transform = transform;
    }

    instanceData.Material.Color = p_State.OutlineColor;
    return instanceData;
}

template <Dimension D>
template <typename Renderer, typename... DrawArgs>
void IRenderer<D>::draw(Renderer &p_Renderer, const mat<D> &p_Transform, DrawArgs &&...p_Args) noexcept
{
    const RenderState<D> &state = m_State->back();
    KIT_ASSERT(state.OutlineWidth >= 0.f, "Outline width must be non-negative");
    const bool hasOutline = state.Outline && !KIT::ApproachesZero(state.OutlineWidth);

    if (!state.Fill && !hasOutline)
        return;

    using FillIData = InstanceData<D, DrawMode::Fill>;
    using StencilIData = InstanceData<D, DrawMode::Stencil>;
    if (state.Fill)
    {
        const FillIData instanceData = createFullDrawInstanceData<D, FillIData>(p_Transform, state, m_ZOffset);
        if (!hasOutline)
            p_Renderer.NoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArgs>(p_Args)...);
        else
        {
            p_Renderer.DoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArgs>(p_Args)...);

            const StencilIData stencilData = createStencilInstanceData<D, StencilIData>(p_Transform, state, m_ZOffset);
            p_Renderer.DoStencilTestNoFill.Draw(m_FrameIndex, stencilData, std::forward<DrawArgs>(p_Args)...);
        }
    }
    else
    {
        const StencilIData ghostData = createStencilInstanceData<D, StencilIData, false>(p_Transform, state, m_ZOffset);
        const StencilIData stencilData = createStencilInstanceData<D, StencilIData>(p_Transform, state, m_ZOffset);
        p_Renderer.DoStencilWriteNoFill.Draw(m_FrameIndex, ghostData, std::forward<DrawArgs>(p_Args)...);
        p_Renderer.DoStencilTestNoFill.Draw(m_FrameIndex, stencilData, std::forward<DrawArgs>(p_Args)...);
    }
}

template <Dimension D>
void IRenderer<D>::DrawMesh(const KIT::Ref<const Model<D>> &p_Model, const mat<D> &p_Transform) noexcept
{
    draw(m_MeshRenderer, p_Transform, p_Model);
}

template <Dimension D>
void IRenderer<D>::DrawPrimitive(const usize p_PrimitiveIndex, const mat<D> &p_Transform) noexcept
{
    draw(m_PrimitiveRenderer, p_Transform, p_PrimitiveIndex);
}

template <Dimension D>
void IRenderer<D>::DrawPolygon(const std::span<const vec<D>> p_Vertices, const mat<D> &p_Transform) noexcept
{
    draw(m_PolygonRenderer, p_Transform, p_Vertices);
}

// This is a kind of annoying exception
template <Dimension D>
void IRenderer<D>::DrawCircle(const mat<D> &p_Transform, const f32 p_LowerAngle, const f32 p_UpperAngle,
                              const f32 p_Hollowness) noexcept
{
    const RenderState<D> &state = m_State->back();
    KIT_ASSERT(state.OutlineWidth >= 0.f, "Outline width must be non-negative");
    const bool hasOutline = state.Outline && !KIT::ApproachesZero(state.OutlineWidth);
    if (!state.Fill && !hasOutline)
        return;

    const vec4 arcInfo =
        vec4{glm::cos(p_LowerAngle), glm::sin(p_LowerAngle), glm::cos(p_UpperAngle), glm::sin(p_UpperAngle)};
    const u32 angleOverflow = glm::abs(p_UpperAngle - p_LowerAngle) > glm::pi<f32>() ? 1 : 0;

    using FillIData = CircleInstanceData<D, DrawMode::Fill>;
    using StencilIData = CircleInstanceData<D, DrawMode::Stencil>;
    if (state.Fill)
    {
        FillIData instanceData = createFullDrawInstanceData<D, FillIData>(p_Transform, state, m_ZOffset);
        instanceData.ArcInfo = arcInfo;
        instanceData.AngleOverflow = angleOverflow;
        instanceData.Hollowness = p_Hollowness;
        if (!hasOutline)
            m_CircleRenderer.NoStencilWriteDoFill.Draw(m_FrameIndex, instanceData);
        else
        {
            m_CircleRenderer.DoStencilWriteDoFill.Draw(m_FrameIndex, instanceData);

            StencilIData stencilData = createStencilInstanceData<D, StencilIData>(p_Transform, state, m_ZOffset);
            stencilData.ArcInfo = arcInfo;
            stencilData.AngleOverflow = angleOverflow;
            stencilData.Hollowness = p_Hollowness;
            m_CircleRenderer.DoStencilTestNoFill.Draw(m_FrameIndex, stencilData);
        }
    }
    else
    {
        StencilIData ghostData = createStencilInstanceData<D, StencilIData, false>(p_Transform, state, m_ZOffset);
        ghostData.ArcInfo = arcInfo;
        ghostData.AngleOverflow = angleOverflow;
        ghostData.Hollowness = p_Hollowness;
        StencilIData stencilData = createStencilInstanceData<D, StencilIData>(p_Transform, state, m_ZOffset);
        stencilData.ArcInfo = arcInfo;
        stencilData.AngleOverflow = angleOverflow;
        stencilData.Hollowness = p_Hollowness;
        m_CircleRenderer.DoStencilWriteNoFill.Draw(m_FrameIndex, ghostData);
        m_CircleRenderer.DoStencilTestNoFill.Draw(m_FrameIndex, stencilData);
    }
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

    m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MFIF;
    m_ZOffset = 0;
}

void Renderer<D3>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<D3, DrawMode::Fill> fillDrawInfo;
    fillDrawInfo.CommandBuffer = p_CommandBuffer;
    fillDrawInfo.FrameIndex = m_FrameIndex;
    fillDrawInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[m_FrameIndex];
    fillDrawInfo.DirectionalLightCount = static_cast<u32>(m_DirectionalLights.size());
    fillDrawInfo.PointLightCount = static_cast<u32>(m_PointLights.size());
    fillDrawInfo.AmbientColor = AmbientColor;

    for (usize i = 0; i < m_DirectionalLights.size(); ++i)
        m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex]->WriteAt(i, &m_DirectionalLights[i]);
    for (usize i = 0; i < m_PointLights.size(); ++i)
        m_DeviceLightData.PointLightBuffers[m_FrameIndex]->WriteAt(i, &m_PointLights[i]);

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

    m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MFIF;
}

void Renderer<D3>::AddDirectionalLight(const DirectionalLight &p_Light) noexcept
{
    const usize size = m_DirectionalLights.size();
    auto &buffer = m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex];
    if (buffer->GetInstanceCount() == size)
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo dirInfo = buffer->GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.PointLightBuffers[m_FrameIndex]->GetDescriptorInfo();

        m_DeviceLightData.DescriptorSets[m_FrameIndex] =
            resetLightBufferDescriptorSet(dirInfo, pointInfo, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(),
                                          m_DeviceLightData.DescriptorSets[m_FrameIndex]);
    }

    m_DirectionalLights.push_back(p_Light);
}

void Renderer<D3>::AddPointLight(const PointLight &p_Light) noexcept
{
    const usize size = m_PointLights.size();
    auto &buffer = m_DeviceLightData.PointLightBuffers[m_FrameIndex];
    if (buffer->GetInstanceCount() == size)
    {
        buffer.Destroy();
        buffer.Create(size * 2);
        const VkDescriptorBufferInfo dirInfo =
            m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex]->GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = buffer->GetDescriptorInfo();

        m_DeviceLightData.DescriptorSets[m_FrameIndex] =
            resetLightBufferDescriptorSet(dirInfo, pointInfo, m_DescriptorSetLayout.Get(), m_DescriptorPool.Get(),
                                          m_DeviceLightData.DescriptorSets[m_FrameIndex]);
    }

    m_PointLights.push_back(p_Light);
}

template class IRenderer<D2>;
template class IRenderer<D3>;

} // namespace ONYX
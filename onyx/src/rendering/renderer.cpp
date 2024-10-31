#include "core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/window.hpp"
#include "kit/utilities/math.hpp"

namespace ONYX
{
void ApplyCoordinateSystem(mat4 &p_Axes, mat4 *p_InverseAxes) noexcept
{
    // Essentially, a rotation around the x axis
    for (glm::length_t i = 0; i < 4; ++i)
        for (glm::length_t j = 1; j <= 2; ++j)
            p_Axes[i][j] = -p_Axes[i][j];
    if (p_InverseAxes)
    {
        mat4 &iaxes = *p_InverseAxes;
        iaxes[1] = -iaxes[1];
        iaxes[2] = -iaxes[2];
    }
}

template <template <typename> typename R, template <u32, StencilMode> typename Specs, u32 N>
RenderSystem<R, Specs, N>::RenderSystem(const VkRenderPass p_RenderPass) noexcept
    : NoStencilWriteFill(p_RenderPass), StencilWriteFill(p_RenderPass), StencilWriteNoFill(p_RenderPass),
      StencilTest(p_RenderPass)
{
}

template <template <typename> typename R, template <u32, StencilMode> typename Specs, u32 N>
void RenderSystem<R, Specs, N>::Flush() noexcept
{
    NoStencilWriteFill.Flush();
    StencilWriteFill.Flush();
    StencilWriteNoFill.Flush();
    StencilTest.Flush();
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

template <u32 N>
    requires(IsDim<N>())
IRenderer<N>::IRenderer(const VkRenderPass p_RenderPass, const DynamicArray<RenderState<N>> *p_State) noexcept
    : m_MeshRenderer(p_RenderPass), m_PrimitiveRenderer(p_RenderPass), m_PolygonRenderer(p_RenderPass),
      m_CircleRenderer(p_RenderPass), m_State(p_State)
{
}

Renderer<3>::Renderer(const VkRenderPass p_RenderPass, const DynamicArray<RenderState3D> *p_State) noexcept
    : IRenderer<3>(p_RenderPass, p_State)
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

template <u32 N, typename IData>
    requires(IsDim<N>())
static IData createFullDrawInstanceData(const mat<N> &p_Transform, const RenderState<N> &p_State,
                                        [[maybe_unused]] u32 &p_ZOffset) noexcept
{
    IData instanceData{};
    if constexpr (N == 2)
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

template <u32 N>
    requires(IsDim<N>())
static void computeOutlineScale(mat<N> &p_Transform, const f32 p_OutlineWidth) noexcept
{
    Transform<N>::ScaleIntrinsic(p_Transform, vec<N>{1.f + p_OutlineWidth});
}

template <u32 N, typename IData, bool Scaled = true>
static IData createStencilInstanceData(const mat<N> &p_Transform, const RenderState<N> &p_State,
                                       [[maybe_unused]] u32 &p_ZOffset) noexcept
{
    IData instanceData{};
    if constexpr (N == 2)
    {
        mat3 transform = p_State.Axes * p_Transform;
        if constexpr (Scaled)
            Transform2D::ScaleIntrinsic(transform, vec2{1.f + p_State.OutlineWidth});
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
            Transform3D::ScaleIntrinsic(transform, vec3{1.f + p_State.OutlineWidth});
        instanceData.Transform = transform;
    }

    instanceData.Material.Color = p_State.OutlineColor;
    return instanceData;
}

template <u32 N>
    requires(IsDim<N>())
template <typename Renderer, typename... DrawArgs>
void IRenderer<N>::draw(Renderer &p_Renderer, const mat<N> &p_Transform, DrawArgs &&...p_Args) noexcept
{
    const RenderState<N> &state = m_State->back();
    KIT_ASSERT(state.OutlineWidth >= 0.f, "Outline width must be non-negative");
    const bool hasOutline = state.Outline && !KIT::ApproachesZero(state.OutlineWidth);

    if (!state.Fill && !hasOutline)
        return;

    using FullIData = InstanceData<N, true>;
    using StencilIData = InstanceData<N, false>;
    if (state.Fill)
    {
        const FullIData instanceData = createFullDrawInstanceData<N, FullIData>(p_Transform, state, m_ZOffset);
        if (!hasOutline)
            p_Renderer.NoStencilWriteFill.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., instanceData);
        else
        {
            p_Renderer.StencilWriteFill.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., instanceData);

            const StencilIData stencilData = createStencilInstanceData<N, StencilIData>(p_Transform, state, m_ZOffset);
            p_Renderer.StencilTest.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., stencilData);
        }
    }
    else
    {
        const StencilIData ghostData = createStencilInstanceData<N, StencilIData, false>(p_Transform, state, m_ZOffset);
        const StencilIData stencilData = createStencilInstanceData<N, StencilIData>(p_Transform, state, m_ZOffset);
        p_Renderer.StencilWriteNoFill.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., ghostData);
        p_Renderer.StencilTest.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., stencilData);
    }
}

template <u32 N>
    requires(IsDim<N>())
void IRenderer<N>::DrawMesh(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform) noexcept
{
    draw(m_MeshRenderer, p_Transform, p_Model);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderer<N>::DrawPrimitive(const usize p_PrimitiveIndex, const mat<N> &p_Transform) noexcept
{
    draw(m_PrimitiveRenderer, p_Transform, p_PrimitiveIndex);
}

template <u32 N>
    requires(IsDim<N>())
void IRenderer<N>::DrawPolygon(const std::span<const vec<N>> p_Vertices, const mat<N> &p_Transform) noexcept
{
    draw(m_PolygonRenderer, p_Transform, p_Vertices);
}

// This is a kind of annoying exception
template <u32 N>
    requires(IsDim<N>())
void IRenderer<N>::DrawCircle(const mat<N> &p_Transform, const f32 p_LowerAngle, const f32 p_UpperAngle) noexcept
{
    const RenderState<N> &state = m_State->back();
    KIT_ASSERT(state.OutlineWidth >= 0.f, "Outline width must be non-negative");
    const bool hasOutline = state.Outline && !KIT::ApproachesZero(state.OutlineWidth);
    if (!state.Fill && !hasOutline)
        return;

    const vec4 arcInfo =
        vec4{glm::cos(p_LowerAngle), glm::sin(p_LowerAngle), glm::cos(p_UpperAngle), glm::sin(p_UpperAngle)};
    const u32 angleOverflow = glm::abs(p_UpperAngle - p_LowerAngle) > glm::pi<f32>() ? 1 : 0;

    using FullIData = CircleInstanceData<N, true>;
    using StencilIData = CircleInstanceData<N, false>;
    if (state.Fill)
    {
        FullIData instanceData = createFullDrawInstanceData<N, FullIData>(p_Transform, state, m_ZOffset);
        instanceData.ArcInfo = arcInfo;
        instanceData.AngleOverflow = angleOverflow;
        if (!hasOutline)
            m_CircleRenderer.NoStencilWriteFill.Draw(m_FrameIndex, instanceData);
        else
        {
            m_CircleRenderer.StencilWriteFill.Draw(m_FrameIndex, instanceData);

            StencilIData stencilData = createStencilInstanceData<N, StencilIData>(p_Transform, state, m_ZOffset);
            stencilData.ArcInfo = arcInfo;
            stencilData.AngleOverflow = angleOverflow;
            m_CircleRenderer.StencilTest.Draw(m_FrameIndex, stencilData);
        }
    }
    else
    {
        StencilIData ghostData = createStencilInstanceData<N, StencilIData, false>(p_Transform, state, m_ZOffset);
        ghostData.ArcInfo = arcInfo;
        ghostData.AngleOverflow = angleOverflow;
        StencilIData stencilData = createStencilInstanceData<N, StencilIData>(p_Transform, state, m_ZOffset);
        stencilData.ArcInfo = arcInfo;
        stencilData.AngleOverflow = angleOverflow;
        m_CircleRenderer.StencilWriteNoFill.Draw(m_FrameIndex, ghostData);
        m_CircleRenderer.StencilTest.Draw(m_FrameIndex, stencilData);
    }
}

void Renderer<2>::Flush() noexcept
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();
}
void Renderer<3>::Flush() noexcept
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();

    m_DirectionalLights.clear();
    m_PointLights.clear();
}

void Renderer<2>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<2, true> fullDrawInfo;
    fullDrawInfo.CommandBuffer = p_CommandBuffer;
    fullDrawInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.NoStencilWriteFill.Render(fullDrawInfo);
    m_PrimitiveRenderer.NoStencilWriteFill.Render(fullDrawInfo);
    m_PolygonRenderer.NoStencilWriteFill.Render(fullDrawInfo);
    m_CircleRenderer.NoStencilWriteFill.Render(fullDrawInfo);

    m_MeshRenderer.StencilWriteFill.Render(fullDrawInfo);
    m_PrimitiveRenderer.StencilWriteFill.Render(fullDrawInfo);
    m_PolygonRenderer.StencilWriteFill.Render(fullDrawInfo);
    m_CircleRenderer.StencilWriteFill.Render(fullDrawInfo);

    RenderInfo<2, false> stencilInfo;
    stencilInfo.CommandBuffer = p_CommandBuffer;
    stencilInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.StencilWriteNoFill.Render(stencilInfo);
    m_PrimitiveRenderer.StencilWriteNoFill.Render(stencilInfo);
    m_PolygonRenderer.StencilWriteNoFill.Render(stencilInfo);
    m_CircleRenderer.StencilWriteNoFill.Render(stencilInfo);

    m_MeshRenderer.StencilTest.Render(stencilInfo);
    m_PrimitiveRenderer.StencilTest.Render(stencilInfo);
    m_PolygonRenderer.StencilTest.Render(stencilInfo);
    m_CircleRenderer.StencilTest.Render(stencilInfo);

    m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MFIF;
    m_ZOffset = 0;
}

void Renderer<3>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<3, true> fullDrawInfo;
    fullDrawInfo.CommandBuffer = p_CommandBuffer;
    fullDrawInfo.FrameIndex = m_FrameIndex;
    fullDrawInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[m_FrameIndex];
    fullDrawInfo.DirectionalLightCount = static_cast<u32>(m_DirectionalLights.size());
    fullDrawInfo.PointLightCount = static_cast<u32>(m_PointLights.size());
    fullDrawInfo.AmbientColor = AmbientColor;

    for (usize i = 0; i < m_DirectionalLights.size(); ++i)
        m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex]->WriteAt(i, &m_DirectionalLights[i]);
    for (usize i = 0; i < m_PointLights.size(); ++i)
        m_DeviceLightData.PointLightBuffers[m_FrameIndex]->WriteAt(i, &m_PointLights[i]);

    m_MeshRenderer.NoStencilWriteFill.Render(fullDrawInfo);
    m_PrimitiveRenderer.NoStencilWriteFill.Render(fullDrawInfo);
    m_PolygonRenderer.NoStencilWriteFill.Render(fullDrawInfo);
    m_CircleRenderer.NoStencilWriteFill.Render(fullDrawInfo);

    m_MeshRenderer.StencilWriteFill.Render(fullDrawInfo);
    m_PrimitiveRenderer.StencilWriteFill.Render(fullDrawInfo);
    m_PolygonRenderer.StencilWriteFill.Render(fullDrawInfo);
    m_CircleRenderer.StencilWriteFill.Render(fullDrawInfo);

    RenderInfo<3, false> stencilInfo;
    stencilInfo.CommandBuffer = p_CommandBuffer;
    stencilInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.StencilWriteNoFill.Render(stencilInfo);
    m_PrimitiveRenderer.StencilWriteNoFill.Render(stencilInfo);
    m_PolygonRenderer.StencilWriteNoFill.Render(stencilInfo);
    m_CircleRenderer.StencilWriteNoFill.Render(stencilInfo);

    m_MeshRenderer.StencilTest.Render(stencilInfo);
    m_PrimitiveRenderer.StencilTest.Render(stencilInfo);
    m_PolygonRenderer.StencilTest.Render(stencilInfo);
    m_CircleRenderer.StencilTest.Render(stencilInfo);

    m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MFIF;
}

void Renderer<3>::AddDirectionalLight(const DirectionalLight &p_Light) noexcept
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

void Renderer<3>::AddPointLight(const PointLight &p_Light) noexcept
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

template class IRenderer<2>;
template class IRenderer<3>;

} // namespace ONYX
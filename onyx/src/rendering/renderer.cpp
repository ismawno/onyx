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
IRenderer<N>::IRenderer(Window *p_Window, const VkRenderPass p_RenderPass,
                        const DynamicArray<RenderState<N>> *p_State) noexcept
    : m_MeshRenderer(p_RenderPass), m_PrimitiveRenderer(p_RenderPass), m_PolygonRenderer(p_RenderPass),
      m_CircleRenderer(p_RenderPass), m_Window(p_Window), m_State(p_State)
{
}

Renderer<3>::Renderer(Window *p_Window, const VkRenderPass p_RenderPass,
                      const DynamicArray<RenderState3D> *p_State) noexcept
    : IRenderer<3>(p_Window, p_RenderPass, p_State)
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

template <typename T = InstanceData2D>
static T createInstanceData2D(const mat3 &p_Transform, const vec4 &p_Color) noexcept
{
    T instanceData;
    instanceData.Transform = transform3ToTransform4(p_Transform);
    // And now, we apply the coordinate system for 2D cases, which might seem that it is applied after the projection,
    // but it is cool because I use no projection for 2D
    ApplyCoordinateSystem(instanceData.Transform);
    instanceData.Material.Color = p_Color;
    return instanceData;
}

template <typename T = InstanceData3D>
static T createInstanceData3D(const mat4 &p_Transform, const RenderState3D &p_State) noexcept
{
    T instanceData;
    instanceData.Transform = p_Transform;
    instanceData.ProjectionView = p_State.HasProjection ? p_State.Projection * p_State.Axes : p_State.Axes;
    instanceData.NormalMatrix = mat4(glm::transpose(glm::inverse(mat3(p_Transform))));

    instanceData.ViewPosition = p_State.InverseAxes[3];
    instanceData.Material = p_State.Material;
    return instanceData;
}

static mat3 computeStrokeTransform(const mat3 &p_Transform, const f32 p_StrokeWidth) noexcept
{
    const vec2 scale = Transform2D::ExtractScale(p_Transform);
    const vec2 stroke = (scale + p_StrokeWidth) / scale;
    mat3 transform = p_Transform;
    Transform2D::ScaleIntrinsic(transform, stroke);
    return transform;
}

template <u32 N>
    requires(IsDim<N>())
template <typename Renderer, typename... DrawArgs>
void IRenderer<N>::draw(Renderer &p_Renderer, const mat<N> &p_Transform, DrawArgs &&...p_Args) noexcept
{
    auto &state = m_State->back();
    if constexpr (N == 2)
    {
        if (!state.NoStroke && !KIT::ApproachesZero(state.StrokeWidth))
        {
            const mat3 strokeTransform = state.Axes * computeStrokeTransform(p_Transform, state.StrokeWidth);
            const InstanceData2D strokeData = createInstanceData2D(strokeTransform, state.StrokeColor);
            p_Renderer.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., strokeData);
        }
        const Color &color = state.NoFill ? m_Window->BackgroundColor : state.Material.Color;
        const InstanceData2D data = createInstanceData2D(state.Axes * p_Transform, color);
        p_Renderer.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., data);
    }
    else
    {
        const InstanceData3D data = createInstanceData3D(p_Transform, state);
        p_Renderer.Draw(m_FrameIndex, std::forward<DrawArgs>(p_Args)..., data);
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
    using IData = typename CircleRenderer<N>::CircleInstanceData;
    auto &state = m_State->back();
    const vec4 arcInfo =
        vec4{glm::cos(p_LowerAngle), glm::sin(p_LowerAngle), glm::cos(p_UpperAngle), glm::sin(p_UpperAngle)};
    const u32 angleOverflow = glm::abs(p_UpperAngle - p_LowerAngle) > glm::pi<f32>() ? 1 : 0;
    if constexpr (N == 2)
    {
        if (!state.NoStroke && !KIT::ApproachesZero(state.StrokeWidth))
        {
            const mat3 strokeTransform = state.Axes * computeStrokeTransform(p_Transform, state.StrokeWidth);
            IData strokeData = createInstanceData2D<IData>(strokeTransform, state.StrokeColor);
            strokeData.ArcInfo = arcInfo;
            strokeData.AngleOverflow = angleOverflow;
            m_CircleRenderer.Draw(m_FrameIndex, strokeData);
        }
        const Color &color = state.NoFill ? m_Window->BackgroundColor : state.Material.Color;
        IData data = createInstanceData2D<IData>(state.Axes * p_Transform, color);
        data.ArcInfo = arcInfo;
        data.AngleOverflow = angleOverflow;
        m_CircleRenderer.Draw(m_FrameIndex, data);
    }
    else
    {
        IData data = createInstanceData3D<IData>(p_Transform, state);
        data.ArcInfo = arcInfo;
        data.AngleOverflow = angleOverflow;
        m_CircleRenderer.Draw(m_FrameIndex, data);
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
    RenderInfo<2> renderInfo;
    renderInfo.CommandBuffer = p_CommandBuffer;
    renderInfo.FrameIndex = m_FrameIndex;

    m_MeshRenderer.Render(renderInfo);
    m_PrimitiveRenderer.Render(renderInfo);
    m_PolygonRenderer.Render(renderInfo);
    m_CircleRenderer.Render(renderInfo);

    m_FrameIndex = (m_FrameIndex + 1) % SwapChain::MFIF;
}

void Renderer<3>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<3> renderInfo;
    renderInfo.CommandBuffer = p_CommandBuffer;
    renderInfo.FrameIndex = m_FrameIndex;
    renderInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[m_FrameIndex];
    renderInfo.DirectionalLightCount = static_cast<u32>(m_DirectionalLights.size());
    renderInfo.PointLightCount = static_cast<u32>(m_PointLights.size());
    renderInfo.AmbientColor = AmbientColor;

    for (usize i = 0; i < m_DirectionalLights.size(); ++i)
        m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex]->WriteAt(i, &m_DirectionalLights[i]);
    for (usize i = 0; i < m_PointLights.size(); ++i)
        m_DeviceLightData.PointLightBuffers[m_FrameIndex]->WriteAt(i, &m_PointLights[i]);

    m_MeshRenderer.Render(renderInfo);
    m_PrimitiveRenderer.Render(renderInfo);
    m_PolygonRenderer.Render(renderInfo);
    m_CircleRenderer.Render(renderInfo);

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
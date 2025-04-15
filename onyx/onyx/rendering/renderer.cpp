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

template <Dimension D, template <Dimension, PipelineMode> typename R>
void RenderSystem<D, R>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    NoStencilWriteDoFill.SendToDevice(p_FrameIndex);
    DoStencilWriteDoFill.SendToDevice(p_FrameIndex);
    DoStencilWriteNoFill.SendToDevice(p_FrameIndex);
    DoStencilTestNoFill.SendToDevice(p_FrameIndex);
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
IRenderer<D>::IRenderer(const VkRenderPass p_RenderPass) noexcept
    : m_MeshRenderer(p_RenderPass), m_PrimitiveRenderer(p_RenderPass), m_PolygonRenderer(p_RenderPass),
      m_CircleRenderer(p_RenderPass)
{
}

Renderer<D3>::Renderer(const VkRenderPass p_RenderPass) noexcept : IRenderer<D3>(p_RenderPass)
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
        DirectionalLightBuffers[i] = CreateHostVisibleStorageBuffer<DirectionalLight>(p_Capacity);
        PointLightBuffers[i] = CreateHostVisibleStorageBuffer<PointLight>(p_Capacity);
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

template <DrawLevel DLevel> static constexpr Dimension getMaterialDimension() noexcept
{
    if constexpr (DLevel == DrawLevel::Simple)
        return D2;
    else
        return D3;
}

template <DrawLevel DLevel>
static InstanceData<DLevel> createInstanceData(const fmat4 &p_Transform,
                                               const MaterialData<getMaterialDimension<DLevel>()> &p_Material) noexcept
{
    InstanceData<DLevel> instanceData{};
    instanceData.Transform = p_Transform;
    instanceData.Material = p_Material;
    if constexpr (DLevel == DrawLevel::Complex)
        instanceData.NormalMatrix = fmat4(glm::transpose(glm::inverse(fmat3(instanceData.Transform))));

    return instanceData;
}

template <Dimension D>
template <typename Renderer, typename DrawArg>
void IRenderer<D>::draw(Renderer &p_Renderer, const RenderState<D> *p_State, const fmat4 &p_Transform, DrawArg &&p_Arg,
                        DrawFlags p_Flags) noexcept
{
    TKIT_ASSERT(p_State->OutlineWidth >= 0.f, "[ONYX] Outline width must be non-negative");
    constexpr DrawLevel dlevel = D == D2 ? DrawLevel::Simple : DrawLevel::Complex;

    if (p_Flags & DrawFlags_NoStencilWriteDoFill)
    {
        const auto instanceData = createInstanceData<dlevel>(p_Transform, p_State->Material);
        p_Renderer.NoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlags_DoStencilWriteDoFill)
    {
        const auto instanceData = createInstanceData<dlevel>(p_Transform, p_State->Material);
        p_Renderer.DoStencilWriteDoFill.Draw(m_FrameIndex, instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlags_DoStencilWriteNoFill)
    {
        const MaterialData<D2> material{p_State->OutlineColor};
        const auto instanceData = createInstanceData<DrawLevel::Simple>(p_Transform, material);

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
        const MaterialData<D2> material{p_State->OutlineColor};
        const auto instanceData = createInstanceData<DrawLevel::Simple>(p_Transform, material);

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
void IRenderer<D>::DrawMesh(const RenderState<D> *p_State, const fmat4 &p_Transform, const Model<D> &p_Model,
                            const DrawFlags p_Flags) noexcept
{
    draw(m_MeshRenderer, p_State, p_Transform, p_Model, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawPrimitive(const RenderState<D> *p_State, const fmat4 &p_Transform, const u32 p_PrimitiveIndex,
                                 const DrawFlags p_Flags) noexcept
{
    draw(m_PrimitiveRenderer, p_State, p_Transform, p_PrimitiveIndex, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawPolygon(const RenderState<D> *p_State, const fmat4 &p_Transform,
                               const TKit::Span<const fvec2> p_Vertices, const DrawFlags p_Flags) noexcept
{
    draw(m_PolygonRenderer, p_State, p_Transform, p_Vertices, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawCircle(const RenderState<D> *p_State, const fmat4 &p_Transform, const CircleOptions &p_Options,
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

void Renderer<D2>::SendToDevice() noexcept
{
    m_MeshRenderer.SendToDevice(m_FrameIndex);
    m_PrimitiveRenderer.SendToDevice(m_FrameIndex);
    m_PolygonRenderer.SendToDevice(m_FrameIndex);
    m_CircleRenderer.SendToDevice(m_FrameIndex);
}

void Renderer<D3>::SendToDevice() noexcept
{
    m_MeshRenderer.SendToDevice(m_FrameIndex);
    m_PrimitiveRenderer.SendToDevice(m_FrameIndex);
    m_PolygonRenderer.SendToDevice(m_FrameIndex);
    m_CircleRenderer.SendToDevice(m_FrameIndex);

    for (u32 i = 0; i < m_DirectionalLights.size(); ++i)
        m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex].WriteAt(i, &m_DirectionalLights[i]);
    for (u32 i = 0; i < m_PointLights.size(); ++i)
        m_DeviceLightData.PointLightBuffers[m_FrameIndex].WriteAt(i, &m_PointLights[i]);

    m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex].Flush();
    m_DeviceLightData.PointLightBuffers[m_FrameIndex].Flush();
}

template <DrawLevel DLevel, typename... Renderers>
static void noStencilWriteDoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers) noexcept
{
    (p_Renderers.NoStencilWriteDoFill.Render(p_RenderInfo), ...);
}

template <DrawLevel DLevel, typename... Renderers>
static void doStencilWriteDoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers) noexcept
{
    (p_Renderers.DoStencilWriteDoFill.Render(p_RenderInfo), ...);
}

template <DrawLevel DLevel, typename... Renderers>
static void doStencilWriteNoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers) noexcept
{
    (p_Renderers.DoStencilWriteNoFill.Render(p_RenderInfo), ...);
}

template <DrawLevel DLevel, typename... Renderers>
static void doStencilTestNoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers) noexcept
{
    (p_Renderers.DoStencilTestNoFill.Render(p_RenderInfo), ...);
}

template <Dimension D>
static void setCameraViewport(const VkCommandBuffer p_CommandBuffer, const CameraInfo &p_Camera) noexcept
{
    if (!p_Camera.Transparent)
    {
        const Color &bg = p_Camera.BackgroundColor;
        TKit::Array<VkClearAttachment, D - 1> clearAttachments{};
        clearAttachments[0].colorAttachment = 0;
        clearAttachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachments[0].clearValue.color = {{bg.RGBA.r, bg.RGBA.g, bg.RGBA.b, bg.RGBA.a}};

        if constexpr (D == D3)
        {
            clearAttachments[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            clearAttachments[1].clearValue.depthStencil = {1.f, 0};
        }

        VkClearRect clearRect{};
        clearRect.rect.offset = {static_cast<i32>(p_Camera.Viewport.x), static_cast<i32>(p_Camera.Viewport.y)};
        clearRect.rect.extent = {static_cast<u32>(p_Camera.Viewport.width), static_cast<u32>(p_Camera.Viewport.height)};
        clearRect.layerCount = 1;
        clearRect.baseArrayLayer = 0;

        vkCmdClearAttachments(p_CommandBuffer, D - 1, clearAttachments.data(), 1, &clearRect);
    }
    vkCmdSetViewport(p_CommandBuffer, 0, 1, &p_Camera.Viewport);
    vkCmdSetScissor(p_CommandBuffer, 0, 1, &p_Camera.Scissor);
}

void Renderer<D2>::Render(const VkCommandBuffer p_CommandBuffer, const TKit::Span<const CameraInfo> p_Cameras) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D2>::Render");
    RenderInfo<DrawLevel::Simple> simpleDrawInfo;
    simpleDrawInfo.CommandBuffer = p_CommandBuffer;
    simpleDrawInfo.FrameIndex = m_FrameIndex;

    for (const CameraInfo &camera : p_Cameras)
    {
        setCameraViewport<D2>(p_CommandBuffer, camera);

        simpleDrawInfo.Camera = &camera;
        noStencilWriteDoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilWriteDoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilWriteNoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilTestNoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
    }

    m_FrameIndex = (m_FrameIndex + 1) % ONYX_MAX_FRAMES_IN_FLIGHT;
}

void Renderer<D3>::Render(const VkCommandBuffer p_CommandBuffer, const TKit::Span<const CameraInfo> p_Cameras) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D3>::Render");
    RenderInfo<DrawLevel::Complex> complexDrawInfo;
    complexDrawInfo.CommandBuffer = p_CommandBuffer;
    complexDrawInfo.FrameIndex = m_FrameIndex;
    complexDrawInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[m_FrameIndex];
    complexDrawInfo.DirectionalLightCount = m_DirectionalLights.size();
    complexDrawInfo.PointLightCount = m_PointLights.size();
    complexDrawInfo.AmbientColor = &AmbientColor;

    RenderInfo<DrawLevel::Simple> simpleDrawInfo;
    simpleDrawInfo.CommandBuffer = p_CommandBuffer;
    simpleDrawInfo.FrameIndex = m_FrameIndex;

    for (const CameraInfo &camera : p_Cameras)
    {
        setCameraViewport<D3>(p_CommandBuffer, camera);

        complexDrawInfo.Camera = &camera;
        noStencilWriteDoFill(complexDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilWriteDoFill(complexDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);

        simpleDrawInfo.Camera = &camera;
        doStencilWriteNoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilTestNoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
    }

    m_FrameIndex = (m_FrameIndex + 1) % ONYX_MAX_FRAMES_IN_FLIGHT;
}

void Renderer<D3>::AddDirectionalLight(const DirectionalLight &p_Light) noexcept
{
    const u32 size = m_DirectionalLights.size();
    auto &buffer = m_DeviceLightData.DirectionalLightBuffers[m_FrameIndex];
    if (buffer.GetInfo().InstanceCount == size)
    {
        buffer.Destroy();
        buffer = CreateHostVisibleStorageBuffer<DirectionalLight>(size * 2);
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
    HostVisibleStorageBuffer<PointLight> &buffer = m_DeviceLightData.PointLightBuffers[m_FrameIndex];
    if (buffer.GetInfo().InstanceCount == size)
    {
        buffer.Destroy();
        buffer = CreateHostVisibleStorageBuffer<PointLight>(size * 2);
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
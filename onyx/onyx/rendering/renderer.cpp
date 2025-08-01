#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx::Detail
{
template <Dimension D, template <Dimension, PipelineMode> typename R>
RenderSystem<D, R>::RenderSystem(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
    : NoStencilWriteDoFill(p_RenderInfo), DoStencilWriteDoFill(p_RenderInfo), DoStencilWriteNoFill(p_RenderInfo),
      DoStencilTestNoFill(p_RenderInfo)
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
void RenderSystem<D, R>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    NoStencilWriteDoFill.GrowToFit(p_FrameIndex);
    DoStencilWriteDoFill.GrowToFit(p_FrameIndex);
    DoStencilWriteNoFill.GrowToFit(p_FrameIndex);
    DoStencilTestNoFill.GrowToFit(p_FrameIndex);
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
IRenderer<D>::IRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept
    : m_MeshRenderer(p_RenderInfo), m_PrimitiveRenderer(p_RenderInfo), m_PolygonRenderer(p_RenderInfo),
      m_CircleRenderer(p_RenderInfo)
{
}

Renderer<D3>::Renderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept : IRenderer<D3>(p_RenderInfo)
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkDescriptorBufferInfo dirInfo = m_DeviceLightData.DirectionalLightBuffers[i].GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.PointLightBuffers[i].GetDescriptorInfo();
        m_DeviceLightData.DescriptorSets[i] = resetLightBufferDescriptorSet(dirInfo, pointInfo);
    }
}

DeviceLightData::DeviceLightData() noexcept
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DirectionalLightBuffers[i] = CreateHostVisibleStorageBuffer<DirectionalLight>(ONYX_BUFFER_INITIAL_CAPACITY);
        PointLightBuffers[i] = CreateHostVisibleStorageBuffer<PointLight>(ONYX_BUFFER_INITIAL_CAPACITY);
    }
}
DeviceLightData::~DeviceLightData() noexcept
{
    Core::DeviceWaitIdle();
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DirectionalLightBuffers[i].Destroy();
        PointLightBuffers[i].Destroy();
    }
}

template <typename T> void DeviceLightData::Grow(const u32 p_FrameIndex, const u32 p_Instances) noexcept
{
    HostVisibleStorageBuffer<T> *buffer;
    if constexpr (std::is_same_v<T, DirectionalLight>)
        buffer = &DirectionalLightBuffers[p_FrameIndex];
    else
        buffer = &PointLightBuffers[p_FrameIndex];

    buffer->Destroy();
    *buffer = CreateHostVisibleStorageBuffer<T>(1 + p_Instances + p_Instances / 2);

    const VkDescriptorBufferInfo dirInfo = DirectionalLightBuffers[p_FrameIndex].GetDescriptorInfo();
    const VkDescriptorBufferInfo pointInfo = PointLightBuffers[p_FrameIndex].GetDescriptorInfo();

    DescriptorSets[p_FrameIndex] = resetLightBufferDescriptorSet(dirInfo, pointInfo, DescriptorSets[p_FrameIndex]);
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
        p_Renderer.NoStencilWriteDoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlags_DoStencilWriteDoFill)
    {
        const auto instanceData = createInstanceData<dlevel>(p_Transform, p_State->Material);
        p_Renderer.DoStencilWriteDoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlags_DoStencilWriteNoFill)
    {
        const MaterialData<D2> material{p_State->OutlineColor};
        const auto instanceData = createInstanceData<DrawLevel::Simple>(p_Transform, material);

        if constexpr (!std::is_same_v<Renderer, RenderSystem<D, CircleRenderer>>)
            p_Renderer.DoStencilWriteNoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
        else
        {
            CircleOptions options = p_Arg;
            options.InnerFade = 0.f;
            options.OuterFade = 0.f;
            p_Renderer.DoStencilWriteNoFill.Draw(instanceData, options);
        }
    }
    if (p_Flags & DrawFlags_DoStencilTestNoFill)
    {
        const MaterialData<D2> material{p_State->OutlineColor};
        const auto instanceData = createInstanceData<DrawLevel::Simple>(p_Transform, material);

        if constexpr (!std::is_same_v<Renderer, RenderSystem<D, CircleRenderer>>)
            p_Renderer.DoStencilTestNoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
        else
        {
            CircleOptions options = p_Arg;
            options.InnerFade = 0.f;
            options.OuterFade = 0.f;
            p_Renderer.DoStencilTestNoFill.Draw(instanceData, options);
        }
    }
}

template <Dimension D>
void IRenderer<D>::DrawMesh(const RenderState<D> *p_State, const fmat4 &p_Transform, const Mesh<D> &p_Mesh,
                            const DrawFlags p_Flags) noexcept
{
    draw(m_MeshRenderer, p_State, p_Transform, p_Mesh, p_Flags);
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

    m_HostLightData.DirectionalLights.Clear();
    m_HostLightData.PointLights.Clear();
}

void Renderer<D2>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_MeshRenderer.GrowToFit(p_FrameIndex);
    m_PrimitiveRenderer.GrowToFit(p_FrameIndex);
    m_PolygonRenderer.GrowToFit(p_FrameIndex);
    m_CircleRenderer.GrowToFit(p_FrameIndex);
}
void Renderer<D2>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    m_MeshRenderer.SendToDevice(p_FrameIndex);
    m_PrimitiveRenderer.SendToDevice(p_FrameIndex);
    m_PolygonRenderer.SendToDevice(p_FrameIndex);
    m_CircleRenderer.SendToDevice(p_FrameIndex);
}

void Renderer<D3>::GrowToFit(const u32 p_FrameIndex) noexcept
{
    m_MeshRenderer.GrowToFit(p_FrameIndex);
    m_PrimitiveRenderer.GrowToFit(p_FrameIndex);
    m_PolygonRenderer.GrowToFit(p_FrameIndex);
    m_CircleRenderer.GrowToFit(p_FrameIndex);

    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    auto &devDirBuffer = m_DeviceLightData.DirectionalLightBuffers[p_FrameIndex];
    if (dcount >= devDirBuffer.GetInfo().InstanceCount)
        m_DeviceLightData.Grow<DirectionalLight>(p_FrameIndex, dcount);

    const u32 pcount = m_HostLightData.PointLights.GetSize();
    auto &devPointBuffer = m_DeviceLightData.PointLightBuffers[p_FrameIndex];
    if (pcount >= devPointBuffer.GetInfo().InstanceCount)
        m_DeviceLightData.Grow<PointLight>(p_FrameIndex, pcount);
}

void Renderer<D3>::SendToDevice(const u32 p_FrameIndex) noexcept
{
    m_MeshRenderer.SendToDevice(p_FrameIndex);
    m_PrimitiveRenderer.SendToDevice(p_FrameIndex);
    m_PolygonRenderer.SendToDevice(p_FrameIndex);
    m_CircleRenderer.SendToDevice(p_FrameIndex);

    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    if (dcount > 0)
    {
        auto &devDirBuffer = m_DeviceLightData.DirectionalLightBuffers[p_FrameIndex];
        const auto &hostDirBuffer = m_HostLightData.DirectionalLights;
        devDirBuffer.Write(hostDirBuffer);
    }

    const u32 pcount = m_HostLightData.PointLights.GetSize();
    if (pcount > 0)
    {
        auto &devPointBuffer = m_DeviceLightData.PointLightBuffers[p_FrameIndex];
        const auto &hostPointBuffer = m_HostLightData.PointLights;
        devPointBuffer.Write(hostPointBuffer);
    }
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
    const auto &table = Core::GetDeviceTable();
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

        table.CmdClearAttachments(p_CommandBuffer, D - 1, clearAttachments.GetData(), 1, &clearRect);
    }
    table.CmdSetViewport(p_CommandBuffer, 0, 1, &p_Camera.Viewport);
    table.CmdSetScissor(p_CommandBuffer, 0, 1, &p_Camera.Scissor);
}

void Renderer<D2>::Render(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer,
                          const TKit::Span<const CameraInfo> p_Cameras) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D2>::Render");
    RenderInfo<DrawLevel::Simple> simpleDrawInfo;
    simpleDrawInfo.CommandBuffer = p_CommandBuffer;
    simpleDrawInfo.FrameIndex = p_FrameIndex;

    for (const CameraInfo &camera : p_Cameras)
    {
        setCameraViewport<D2>(p_CommandBuffer, camera);

        simpleDrawInfo.Camera = &camera;
        noStencilWriteDoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilWriteDoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilWriteNoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
        doStencilTestNoFill(simpleDrawInfo, m_MeshRenderer, m_PrimitiveRenderer, m_PolygonRenderer, m_CircleRenderer);
    }
}

void Renderer<D3>::Render(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer,
                          const TKit::Span<const CameraInfo> p_Cameras) noexcept
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D3>::Render");
    RenderInfo<DrawLevel::Complex> complexDrawInfo;
    complexDrawInfo.CommandBuffer = p_CommandBuffer;
    complexDrawInfo.FrameIndex = p_FrameIndex;
    complexDrawInfo.LightStorageBuffers = m_DeviceLightData.DescriptorSets[p_FrameIndex];
    complexDrawInfo.DirectionalLightCount = m_HostLightData.DirectionalLights.GetSize();
    complexDrawInfo.PointLightCount = m_HostLightData.PointLights.GetSize();
    complexDrawInfo.AmbientColor = &AmbientColor;

    RenderInfo<DrawLevel::Simple> simpleDrawInfo;
    simpleDrawInfo.CommandBuffer = p_CommandBuffer;
    simpleDrawInfo.FrameIndex = p_FrameIndex;

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
}

void Renderer<D3>::AddDirectionalLight(const DirectionalLight &p_Light) noexcept
{
    m_HostLightData.DirectionalLights.Append(p_Light);
}

void Renderer<D3>::AddPointLight(const PointLight &p_Light) noexcept
{
    m_HostLightData.PointLights.Append(p_Light);
}

template class ONYX_API IRenderer<D2>;
template class ONYX_API IRenderer<D3>;

} // namespace Onyx::Detail

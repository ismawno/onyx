#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "vkit/descriptors/descriptor_set.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

namespace Onyx::Detail
{
template <Dimension D, template <Dimension, PipelineMode> typename R>
RenderGroup<D, R>::RenderGroup(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
    : NoStencilWriteDoFill(p_RenderInfo), DoStencilWriteDoFill(p_RenderInfo), DoStencilWriteNoFill(p_RenderInfo),
      DoStencilTestNoFill(p_RenderInfo)
{
}

template <Dimension D, template <Dimension, PipelineMode> typename R> void RenderGroup<D, R>::Flush()
{
    NoStencilWriteDoFill.Flush();
    DoStencilWriteDoFill.Flush();
    DoStencilWriteNoFill.Flush();
    DoStencilTestNoFill.Flush();
}

template <Dimension D, template <Dimension, PipelineMode> typename R>
void RenderGroup<D, R>::GrowToFit(const u32 p_FrameIndex)
{
    NoStencilWriteDoFill.GrowToFit(p_FrameIndex);
    DoStencilWriteDoFill.GrowToFit(p_FrameIndex);
    DoStencilWriteNoFill.GrowToFit(p_FrameIndex);
    DoStencilTestNoFill.GrowToFit(p_FrameIndex);
}
template <Dimension D, template <Dimension, PipelineMode> typename R>
void RenderGroup<D, R>::SendToDevice(const u32 p_FrameIndex, TaskArray &p_Tasks)
{
    const TKit::ITaskManager *tm = Core::GetTaskManager();
    if (NoStencilWriteDoFill.HasInstances(p_FrameIndex))
        p_Tasks.Append(tm->CreateTask([this, p_FrameIndex] { NoStencilWriteDoFill.SendToDevice(p_FrameIndex); }));
    if (DoStencilWriteDoFill.HasInstances(p_FrameIndex))
        p_Tasks.Append(tm->CreateTask([this, p_FrameIndex] { DoStencilWriteDoFill.SendToDevice(p_FrameIndex); }));
    if (DoStencilWriteNoFill.HasInstances(p_FrameIndex))
        p_Tasks.Append(tm->CreateTask([this, p_FrameIndex] { DoStencilWriteNoFill.SendToDevice(p_FrameIndex); }));
    if (DoStencilTestNoFill.HasInstances(p_FrameIndex))
        p_Tasks.Append(tm->CreateTask([this, p_FrameIndex] { DoStencilTestNoFill.SendToDevice(p_FrameIndex); }));
}

template <Dimension D, template <Dimension, PipelineMode> typename R>
void RenderGroup<D, R>::RecordCopyCommands(const CopyInfo &p_Info)
{
    if (NoStencilWriteDoFill.HasInstances(p_Info.FrameIndex))
        NoStencilWriteDoFill.RecordCopyCommands(p_Info);
    if (DoStencilWriteDoFill.HasInstances(p_Info.FrameIndex))
        DoStencilWriteDoFill.RecordCopyCommands(p_Info);
    if (DoStencilWriteNoFill.HasInstances(p_Info.FrameIndex))
        DoStencilWriteNoFill.RecordCopyCommands(p_Info);
    if (DoStencilTestNoFill.HasInstances(p_Info.FrameIndex))
        DoStencilTestNoFill.RecordCopyCommands(p_Info);
}

static VkDescriptorSet resetLightBufferDescriptorSet(const VkDescriptorBufferInfo &p_DirectionalInfo,
                                                     const VkDescriptorBufferInfo &p_PointInfo,
                                                     VkDescriptorSet p_OldSet = VK_NULL_HANDLE)
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
IRenderer<D>::IRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
    : m_MeshRenderer(p_RenderInfo), m_PrimitiveRenderer(p_RenderInfo), m_PolygonRenderer(p_RenderInfo),
      m_CircleRenderer(p_RenderInfo)
{
}

Renderer<D3>::Renderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) : IRenderer<D3>(p_RenderInfo)
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkDescriptorBufferInfo dirInfo = m_DeviceLightData.DeviceLocalDirectionals[i].GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.DeviceLocalPoints[i].GetDescriptorInfo();
        m_DeviceLightData.DescriptorSets[i] = resetLightBufferDescriptorSet(dirInfo, pointInfo);
    }
}

DeviceLightData::DeviceLightData()
{
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DeviceLocalDirectionals[i] = CreateDeviceLocalStorageBuffer<DirectionalLight>(ONYX_BUFFER_INITIAL_CAPACITY);
        DeviceLocalPoints[i] = CreateDeviceLocalStorageBuffer<PointLight>(ONYX_BUFFER_INITIAL_CAPACITY);
        StagingDirectionals[i] = CreateHostVisibleStorageBuffer<DirectionalLight>(ONYX_BUFFER_INITIAL_CAPACITY);
        StagingPoints[i] = CreateHostVisibleStorageBuffer<PointLight>(ONYX_BUFFER_INITIAL_CAPACITY);
    }
}
DeviceLightData::~DeviceLightData()
{
    Core::DeviceWaitIdle();
    for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        DeviceLocalDirectionals[i].Destroy();
        DeviceLocalPoints[i].Destroy();
        StagingDirectionals[i].Destroy();
        StagingPoints[i].Destroy();
    }
}

void DeviceLightData::GrowToFit(const u32 p_FrameIndex, const u32 p_Directionals, const u32 p_Points)
{
    auto &ldbuffer = DeviceLocalDirectionals[p_FrameIndex];
    if (ldbuffer.GetInfo().InstanceCount < p_Directionals)
    {
        ldbuffer.Destroy();
        ldbuffer = CreateDeviceLocalStorageBuffer<DirectionalLight>(1 + p_Directionals + p_Directionals / 2);
    }

    auto &sdbuffer = StagingDirectionals[p_FrameIndex];
    if (sdbuffer.GetInfo().InstanceCount < p_Directionals)
    {
        sdbuffer.Destroy();
        sdbuffer = CreateHostVisibleStorageBuffer<DirectionalLight>(1 + p_Directionals + p_Directionals / 2);
    }
    auto &lpbuffer = DeviceLocalPoints[p_FrameIndex];
    if (lpbuffer.GetInfo().InstanceCount < p_Points)
    {
        lpbuffer.Destroy();
        lpbuffer = CreateDeviceLocalStorageBuffer<PointLight>(1 + p_Points + p_Points / 2);
    }

    auto &spbuffer = StagingPoints[p_FrameIndex];
    if (spbuffer.GetInfo().InstanceCount < p_Points)
    {
        spbuffer.Destroy();
        spbuffer = CreateHostVisibleStorageBuffer<PointLight>(1 + p_Points + p_Points / 2);
    }

    const VkDescriptorBufferInfo dirInfo = ldbuffer.GetDescriptorInfo();
    const VkDescriptorBufferInfo pointInfo = lpbuffer.GetDescriptorInfo();

    DescriptorSets[p_FrameIndex] = resetLightBufferDescriptorSet(dirInfo, pointInfo, DescriptorSets[p_FrameIndex]);
}

struct Material
{
    u32 Color;
    f32 DiffuseContribution;
    f32 SpecularContribution;
    f32 SpecularSharpness;
};

template <Dimension D, DrawMode DMode>
static InstanceData<D, DMode> createInstanceData(const fmat<D> &p_Transform, const Material &p_Material)
{
    if constexpr (D == D2)
    {
        InstanceData<D2, DMode> instanceData{};
        instanceData.Basis1 = fvec2{p_Transform[0]};
        instanceData.Basis2 = fvec2{p_Transform[1]};
        instanceData.Basis3 = fvec2{p_Transform[2]};
        instanceData.Color = p_Material.Color;
        return instanceData;
    }
    else if constexpr (DMode == DrawMode::Fill)
    {
        InstanceData<D3, DrawMode::Fill> instanceData{};
        instanceData.Basis1 = fvec4{p_Transform[0].x, p_Transform[1].x, p_Transform[2].x, p_Transform[3].x};
        instanceData.Basis2 = fvec4{p_Transform[0].y, p_Transform[1].y, p_Transform[2].y, p_Transform[3].y};
        instanceData.Basis3 = fvec4{p_Transform[0].z, p_Transform[1].z, p_Transform[2].z, p_Transform[3].z};
        instanceData.Color = p_Material.Color;
        instanceData.DiffuseContribution = p_Material.DiffuseContribution;
        instanceData.SpecularContribution = p_Material.SpecularContribution;
        instanceData.SpecularSharpness = p_Material.SpecularSharpness;
        return instanceData;
    }
    else
    {
        InstanceData<D3, DrawMode::Stencil> instanceData{};
        instanceData.Basis1 = fvec4{p_Transform[0].x, p_Transform[1].x, p_Transform[2].x, p_Transform[3].x};
        instanceData.Basis2 = fvec4{p_Transform[0].y, p_Transform[1].y, p_Transform[2].y, p_Transform[3].y};
        instanceData.Basis3 = fvec4{p_Transform[0].z, p_Transform[1].z, p_Transform[2].z, p_Transform[3].z};
        instanceData.Color = p_Material.Color;
        return instanceData;
    }
}

template <Dimension D>
template <typename Renderer, typename DrawArg>
void IRenderer<D>::draw(Renderer &p_Renderer, const RenderState<D> &p_State, const fmat<D> &p_Transform,
                        DrawArg &&p_Arg, DrawFlags p_Flags)
{
    TKIT_ASSERT(p_State.OutlineWidth >= 0.f, "[ONYX] Outline width must be non-negative");

    Material material;
    if constexpr (D == D3)
    {
        material.DiffuseContribution = p_State.Material.DiffuseContribution;
        material.SpecularContribution = p_State.Material.SpecularContribution;
        material.SpecularSharpness = p_State.Material.SpecularSharpness;
    }

    if (p_Flags & DrawFlag_NoStencilWriteDoFill)
    {
        material.Color = p_State.Material.Color.Pack();
        const auto instanceData = createInstanceData<D, DrawMode::Fill>(p_Transform, material);
        p_Renderer.NoStencilWriteDoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlag_DoStencilWriteDoFill)
    {
        material.Color = p_State.Material.Color.Pack();
        const auto instanceData = createInstanceData<D, DrawMode::Fill>(p_Transform, material);
        p_Renderer.DoStencilWriteDoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlag_DoStencilWriteNoFill)
    {
        material.Color = p_State.OutlineColor.Pack();
        const auto instanceData = createInstanceData<D, DrawMode::Stencil>(p_Transform, material);

        if constexpr (!std::is_same_v<Renderer, RenderGroup<D, CircleRenderer>>)
            p_Renderer.DoStencilWriteNoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
        else
        {
            CircleOptions options = p_Arg;
            options.InnerFade = 0.f;
            options.OuterFade = 0.f;
            p_Renderer.DoStencilWriteNoFill.Draw(instanceData, options);
        }
    }
    if (p_Flags & DrawFlag_DoStencilTestNoFill)
    {
        material.Color = p_State.OutlineColor.Pack();
        const auto instanceData = createInstanceData<D, DrawMode::Stencil>(p_Transform, material);

        if constexpr (!std::is_same_v<Renderer, RenderGroup<D, CircleRenderer>>)
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
void IRenderer<D>::DrawMesh(const RenderState<D> &p_State, const fmat<D> &p_Transform, const Mesh<D> &p_Mesh,
                            const DrawFlags p_Flags)
{
    draw(m_MeshRenderer, p_State, p_Transform, p_Mesh, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawPrimitive(const RenderState<D> &p_State, const fmat<D> &p_Transform, const u32 p_PrimitiveIndex,
                                 const DrawFlags p_Flags)
{
    draw(m_PrimitiveRenderer, p_State, p_Transform, p_PrimitiveIndex, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawPolygon(const RenderState<D> &p_State, const fmat<D> &p_Transform,
                               const TKit::Span<const fvec2> p_Vertices, const DrawFlags p_Flags)
{
    draw(m_PolygonRenderer, p_State, p_Transform, p_Vertices, p_Flags);
}

template <Dimension D>
void IRenderer<D>::DrawCircle(const RenderState<D> &p_State, const fmat<D> &p_Transform, const CircleOptions &p_Options,
                              const DrawFlags p_Flags)
{
    draw(m_CircleRenderer, p_State, p_Transform, p_Options, p_Flags);
}

void Renderer<D2>::Flush()
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();
}
void Renderer<D3>::Flush()
{
    m_MeshRenderer.Flush();
    m_PrimitiveRenderer.Flush();
    m_PolygonRenderer.Flush();
    m_CircleRenderer.Flush();

    m_HostLightData.DirectionalLights.Clear();
    m_HostLightData.PointLights.Clear();
}

void Renderer<D2>::GrowToFit(const u32 p_FrameIndex)
{
    m_MeshRenderer.GrowToFit(p_FrameIndex);
    m_PrimitiveRenderer.GrowToFit(p_FrameIndex);
    m_PolygonRenderer.GrowToFit(p_FrameIndex);
    m_CircleRenderer.GrowToFit(p_FrameIndex);
}

void Renderer<D2>::SendToDevice(const u32 p_FrameIndex)
{
    TaskArray tasks{};
    m_MeshRenderer.SendToDevice(p_FrameIndex, tasks);
    m_PrimitiveRenderer.SendToDevice(p_FrameIndex, tasks);
    m_PolygonRenderer.SendToDevice(p_FrameIndex, tasks);
    m_CircleRenderer.SendToDevice(p_FrameIndex, tasks);
    if (tasks.IsEmpty())
        return;

    TKit::ITaskManager *tm = Core::GetTaskManager();
    const Task task = tasks.GetBack();
    tasks.Pop();

    if (tasks.IsEmpty())
    {
        (*task)();
        tm->DestroyTask(task);
        return;
    }

    tm->SubmitTasks(TKit::Span<const Task>{tasks});

    (*task)();
    tm->DestroyTask(task);

    for (const Task t : tasks)
    {
        tm->WaitUntilFinished(t);
        tm->DestroyTask(t);
    }
}
VkPipelineStageFlags Renderer<D2>::RecordCopyCommands(const u32 p_FrameIndex, const VkCommandBuffer p_GraphicsCommand,
                                                      const VkCommandBuffer p_TransferCommand)
{
    const bool separate = Core::GetTransferMode() == TransferMode::Separate;
    TKit::StaticArray16<VkBufferMemoryBarrier> sacquires{};
    TKit::StaticArray4<VkBufferMemoryBarrier> vacquires{};
    TKit::StaticArray32<VkBufferMemoryBarrier> releases{};

    CopyInfo info{};
    info.CommandBuffer = p_TransferCommand;
    info.FrameIndex = p_FrameIndex;
    info.AcquireShaderBarriers = &sacquires;
    info.ReleaseBarriers = separate ? &releases : nullptr;
    info.AcquireVertexBarriers = &vacquires;

    m_MeshRenderer.RecordCopyCommands(info);
    m_PrimitiveRenderer.RecordCopyCommands(info);
    m_PolygonRenderer.RecordCopyCommands(info);
    m_CircleRenderer.RecordCopyCommands(info);

    VkPipelineStageFlags flags = 0;
    if (!releases.IsEmpty())
        ApplyReleaseBarrier(p_TransferCommand, releases);

    if (!sacquires.IsEmpty())
    {
        ApplyAcquireBarrier(p_GraphicsCommand, sacquires, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
        flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }
    if (!vacquires.IsEmpty())
    {
        ApplyAcquireBarrier(p_GraphicsCommand, vacquires, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
        flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    return flags;
}

void Renderer<D3>::GrowToFit(const u32 p_FrameIndex)
{
    m_MeshRenderer.GrowToFit(p_FrameIndex);
    m_PrimitiveRenderer.GrowToFit(p_FrameIndex);
    m_PolygonRenderer.GrowToFit(p_FrameIndex);
    m_CircleRenderer.GrowToFit(p_FrameIndex);

    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    const u32 pcount = m_HostLightData.PointLights.GetSize();
    m_DeviceLightData.GrowToFit(p_FrameIndex, dcount, pcount);
}

void Renderer<D3>::SendToDevice(const u32 p_FrameIndex)
{
    TaskArray tasks{};
    m_MeshRenderer.SendToDevice(p_FrameIndex, tasks);
    m_PrimitiveRenderer.SendToDevice(p_FrameIndex, tasks);
    m_PolygonRenderer.SendToDevice(p_FrameIndex, tasks);
    m_CircleRenderer.SendToDevice(p_FrameIndex, tasks);

    Task task = nullptr;
    if (!tasks.IsEmpty())
    {
        task = tasks.GetBack();
        tasks.Pop();
    }

    TKit::ITaskManager *tm = Core::GetTaskManager();

    if (!tasks.IsEmpty())
        tm->SubmitTasks(TKit::Span<const Task>{tasks});

    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    if (dcount > 0)
    {
        auto &devDirBuffer = m_DeviceLightData.StagingDirectionals[p_FrameIndex];
        const auto &hostDirBuffer = m_HostLightData.DirectionalLights;
        devDirBuffer.Write(hostDirBuffer);
    }

    const u32 pcount = m_HostLightData.PointLights.GetSize();
    if (pcount > 0)
    {
        auto &devPointBuffer = m_DeviceLightData.StagingPoints[p_FrameIndex];
        const auto &hostPointBuffer = m_HostLightData.PointLights;
        devPointBuffer.Write(hostPointBuffer);
    }
    if (!task)
        return;

    (*task)();
    tm->DestroyTask(task);
    for (const Task t : tasks)
    {
        tm->WaitUntilFinished(t);
        tm->DestroyTask(t);
    }
}
VkPipelineStageFlags Renderer<D3>::RecordCopyCommands(const u32 p_FrameIndex, const VkCommandBuffer p_GraphicsCommand,
                                                      const VkCommandBuffer p_TransferCommand)
{
    const bool separate = Core::GetTransferMode() == TransferMode::Separate;
    TKit::StaticArray16<VkBufferMemoryBarrier> sacquires{};
    TKit::StaticArray4<VkBufferMemoryBarrier> vacquires{};
    TKit::StaticArray32<VkBufferMemoryBarrier> releases{};

    CopyInfo info{};
    info.CommandBuffer = p_TransferCommand;
    info.FrameIndex = p_FrameIndex;
    info.AcquireShaderBarriers = &sacquires;
    info.ReleaseBarriers = separate ? &releases : nullptr;
    info.AcquireVertexBarriers = &vacquires;

    m_MeshRenderer.RecordCopyCommands(info);
    m_PrimitiveRenderer.RecordCopyCommands(info);
    m_PolygonRenderer.RecordCopyCommands(info);
    m_CircleRenderer.RecordCopyCommands(info);

    const u32 dsize = m_HostLightData.DirectionalLights.GetSize() * sizeof(DirectionalLight);
    if (dsize > 0)
    {
        const auto &ldbuffer = m_DeviceLightData.DeviceLocalDirectionals[p_FrameIndex];
        const auto &sdbuffer = m_DeviceLightData.StagingDirectionals[p_FrameIndex];
        RecordCopy(p_TransferCommand, ldbuffer, sdbuffer, dsize);
        sacquires.Append(CreateAcquireBarrier(ldbuffer, dsize));
        if (separate)
            releases.Append(CreateReleaseBarrier(ldbuffer, dsize));
    }

    const u32 psize = m_HostLightData.PointLights.GetSize() * sizeof(PointLight);
    if (psize > 0)
    {
        const auto &lpbuffer = m_DeviceLightData.DeviceLocalPoints[p_FrameIndex];
        const auto &spbuffer = m_DeviceLightData.StagingPoints[p_FrameIndex];
        RecordCopy(p_TransferCommand, lpbuffer, spbuffer, psize);
        sacquires.Append(CreateAcquireBarrier(lpbuffer, dsize));
        if (separate)
            releases.Append(CreateReleaseBarrier(lpbuffer, dsize));
    }

    VkPipelineStageFlags flags = 0;
    if (!releases.IsEmpty())
        ApplyReleaseBarrier(p_TransferCommand, releases);

    if (!sacquires.IsEmpty())
    {
        ApplyAcquireBarrier(p_GraphicsCommand, sacquires, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
        flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }
    if (!vacquires.IsEmpty())
    {
        ApplyAcquireBarrier(p_GraphicsCommand, vacquires, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
        flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    return flags;
}

template <DrawLevel DLevel, typename... Renderers>
static void noStencilWriteDoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers)
{
    (p_Renderers.NoStencilWriteDoFill.Render(p_RenderInfo), ...);
}

template <DrawLevel DLevel, typename... Renderers>
static void doStencilWriteDoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers)
{
    (p_Renderers.DoStencilWriteDoFill.Render(p_RenderInfo), ...);
}

template <DrawLevel DLevel, typename... Renderers>
static void doStencilWriteNoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers)
{
    (p_Renderers.DoStencilWriteNoFill.Render(p_RenderInfo), ...);
}

template <DrawLevel DLevel, typename... Renderers>
static void doStencilTestNoFill(const RenderInfo<DLevel> &p_RenderInfo, Renderers &...p_Renderers)
{
    (p_Renderers.DoStencilTestNoFill.Render(p_RenderInfo), ...);
}

template <Dimension D> static void setCameraViewport(const VkCommandBuffer p_CommandBuffer, const CameraInfo &p_Camera)
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
                          const TKit::Span<const CameraInfo> p_Cameras)
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
                          const TKit::Span<const CameraInfo> p_Cameras)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D3>::Render");
    RenderInfo<DrawLevel::Complex> complexDrawInfo;
    complexDrawInfo.CommandBuffer = p_CommandBuffer;
    complexDrawInfo.FrameIndex = p_FrameIndex;
    LightData light;

    light.DescriptorSet = m_DeviceLightData.DescriptorSets[p_FrameIndex];
    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    const u32 pcount = m_HostLightData.PointLights.GetSize();

    light.AmbientColor = &AmbientColor;

    light.DirectionalCount = dcount;
    light.PointCount = pcount;

    complexDrawInfo.Light = light;

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

void Renderer<D3>::AddDirectionalLight(const DirectionalLight &p_Light)
{
    m_HostLightData.DirectionalLights.Append(p_Light);
}

void Renderer<D3>::AddPointLight(const PointLight &p_Light)
{
    m_HostLightData.PointLights.Append(p_Light);
}

template class ONYX_API IRenderer<D2>;
template class ONYX_API IRenderer<D3>;

} // namespace Onyx::Detail

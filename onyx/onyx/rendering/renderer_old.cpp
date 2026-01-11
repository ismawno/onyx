#include "onyx/core/pch.hpp"
#include "onyx/rendering/renderer.hpp"
#include "onyx/asset/assets.hpp"
#include "tkit/profiling/macros.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

namespace Onyx::Detail
{
static VkDescriptorSet resetLightBufferDescriptorSet(const VkDescriptorBufferInfo &p_DirectionalInfo,
                                                     const VkDescriptorBufferInfo &p_PointInfo,
                                                     VkDescriptorSet p_OldSet = VK_NULL_HANDLE)
{
    const VKit::DescriptorSetLayout &layout = Assets::GetLightStorageDescriptorSetLayout();
    const VKit::DescriptorPool &pool = Assets::GetDescriptorPool();

    VKit::DescriptorSet::Writer writer(Core::GetDevice(), &layout);
    writer.WriteBuffer(0, p_DirectionalInfo);
    writer.WriteBuffer(1, p_PointInfo);

    if (!p_OldSet)
    {
        const auto result = pool.Allocate(layout);
        VKIT_CHECK_RESULT(result);
        p_OldSet = result.GetValue();
    }
    writer.Overwrite(p_OldSet);
    return p_OldSet;
}

template <Dimension D>
IRenderer<D>::IRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
    : m_StatMeshSystem(p_RenderInfo), m_CircleSystem(p_RenderInfo)
{
}

Renderer<D3>::Renderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) : IRenderer<D3>(p_RenderInfo)
{
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
        const VkDescriptorBufferInfo dirInfo = m_DeviceLightData.DeviceLocalDirectionals[i].GetDescriptorInfo();
        const VkDescriptorBufferInfo pointInfo = m_DeviceLightData.DeviceLocalPoints[i].GetDescriptorInfo();
        m_DeviceLightData.DescriptorSets[i] = resetLightBufferDescriptorSet(dirInfo, pointInfo);
    }
}

DeviceLightData::DeviceLightData()
{
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
        DeviceLocalDirectionals[i] = CreateBuffer<DirectionalLight>(Buffer_DeviceStorage);
        DeviceLocalPoints[i] = CreateBuffer<PointLight>(Buffer_DeviceStorage);
        StagingDirectionals[i] = CreateBuffer<DirectionalLight>(Buffer_Staging);
        StagingPoints[i] = CreateBuffer<PointLight>(Buffer_Staging);
    }
}
DeviceLightData::~DeviceLightData()
{
    Core::DeviceWaitIdle();
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
    {
        DeviceLocalDirectionals[i].Destroy();
        DeviceLocalPoints[i].Destroy();
        StagingDirectionals[i].Destroy();
        StagingPoints[i].Destroy();
    }
}

void DeviceLightData::GrowDeviceBuffers(const u32 p_FrameIndex, const u32 p_Directionals, const u32 p_Points)
{
    auto &ldbuffer = DeviceLocalDirectionals[p_FrameIndex];
    auto &sdbuffer = StagingDirectionals[p_FrameIndex];
    auto &lpbuffer = DeviceLocalPoints[p_FrameIndex];
    auto &spbuffer = StagingPoints[p_FrameIndex];

    GrowBufferIfNeeded<DirectionalLight>(ldbuffer, p_Directionals, Buffer_DeviceStorage);
    GrowBufferIfNeeded<DirectionalLight>(sdbuffer, p_Directionals, Buffer_Staging);
    GrowBufferIfNeeded<PointLight>(lpbuffer, p_Points, Buffer_DeviceStorage);
    GrowBufferIfNeeded<PointLight>(spbuffer, p_Points, Buffer_Staging);

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
static InstanceData<D, DMode> createInstanceData(const f32m<D> &p_Transform, const Material &p_Material)
{
    if constexpr (D == D2)
    {
        InstanceData<D2, DMode> instanceData{};
        instanceData.Basis1 = f32v2{p_Transform[0]};
        instanceData.Basis2 = f32v2{p_Transform[1]};
        instanceData.Basis3 = f32v2{p_Transform[2]};
        instanceData.Color = p_Material.Color;
        return instanceData;
    }
    else if constexpr (DMode == Draw_Fill)
    {
        InstanceData<D3, Draw_Fill> instanceData{};
        instanceData.Basis1 = f32v4{p_Transform[0][0], p_Transform[1][0], p_Transform[2][0], p_Transform[3][0]};
        instanceData.Basis2 = f32v4{p_Transform[0][1], p_Transform[1][1], p_Transform[2][1], p_Transform[3][1]};
        instanceData.Basis3 = f32v4{p_Transform[0][2], p_Transform[1][2], p_Transform[2][2], p_Transform[3][2]};
        instanceData.Color = p_Material.Color;
        instanceData.DiffuseContribution = p_Material.DiffuseContribution;
        instanceData.SpecularContribution = p_Material.SpecularContribution;
        instanceData.SpecularSharpness = p_Material.SpecularSharpness;
        return instanceData;
    }
    else
    {
        InstanceData<D3, Draw_Outline> instanceData{};
        instanceData.Basis1 = f32v4{p_Transform[0][0], p_Transform[1][0], p_Transform[2][0], p_Transform[3][0]};
        instanceData.Basis2 = f32v4{p_Transform[0][1], p_Transform[1][1], p_Transform[2][1], p_Transform[3][1]};
        instanceData.Basis3 = f32v4{p_Transform[0][2], p_Transform[1][2], p_Transform[2][2], p_Transform[3][2]};
        instanceData.Color = p_Material.Color;
        return instanceData;
    }
}

template <Dimension D>
template <typename Renderer, typename DrawArg>
void IRenderer<D>::draw(Renderer &p_Renderer, const RenderState<D> &p_State, const f32m<D> &p_Transform,
                        DrawArg &&p_Arg, DrawFlags p_Flags)
{
    TKIT_ASSERT(p_State.OutlineWidth >= 0.f, "[ONYX] Outline width ({}) must be non-negative", p_State.OutlineWidth);

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
        const auto instanceData = createInstanceData<D, Draw_Fill>(p_Transform, material);
        p_Renderer.NoStencilWriteDoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlag_DoStencilWriteDoFill)
    {
        material.Color = p_State.Material.Color.Pack();
        const auto instanceData = createInstanceData<D, Draw_Fill>(p_Transform, material);
        p_Renderer.DoStencilWriteDoFill.Draw(instanceData, std::forward<DrawArg>(p_Arg));
    }
    if (p_Flags & DrawFlag_DoStencilWriteNoFill)
    {
        material.Color = p_State.OutlineColor.Pack();
        const auto instanceData = createInstanceData<D, Draw_Outline>(p_Transform, material);

        if constexpr (!std::is_same_v<Renderer, Expand<CircleSystem>>)
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
        const auto instanceData = createInstanceData<D, Draw_Outline>(p_Transform, material);

        if constexpr (!std::is_same_v<Renderer, Expand<CircleSystem>>)
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

void Renderer<D2>::Flush()
{
    m_StatMeshSystem.Flush();
    m_CircleSystem.Flush();
}
void Renderer<D3>::Flush()
{
    m_StatMeshSystem.Flush();
    m_CircleSystem.Flush();

    m_HostLightData.DirectionalLights.Clear();
    m_HostLightData.PointLights.Clear();
}

void Renderer<D2>::GrowDeviceBuffers(const u32 p_FrameIndex)
{
    m_StatMeshSystem.GrowDeviceBuffers(p_FrameIndex);
    m_CircleSystem.GrowDeviceBuffers(p_FrameIndex);
}

void Renderer<D2>::SendToDevice(const u32 p_FrameIndex)
{
    Task mainTask{};
    TKit::Array16<Task> tasks{};
    SendInfo info{};
    info.Tasks = &tasks;
    info.MainTask = &mainTask;
    info.SubmissionIndex = 0;

    m_StatMeshSystem.SendToDevice(p_FrameIndex, info);
    m_CircleSystem.SendToDevice(p_FrameIndex, info);

    if (!mainTask)
        return;

    mainTask();
    TKit::ITaskManager *tm = Core::GetTaskManager();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}
VkPipelineStageFlags Renderer<D2>::RecordCopyCommands(const u32 p_FrameIndex, const VkCommandBuffer p_GraphicsCommand,
                                                      const VkCommandBuffer p_TransferCommand)
{
    const bool separate = Queues::IsSeparateTransferMode();
    TKit::Array16<VkBufferMemoryBarrier> sacquires{};
    TKit::Array4<VkBufferMemoryBarrier> vacquires{};
    TKit::Array32<VkBufferMemoryBarrier> releases{};

    CopyInfo info{};
    info.CommandBuffer = p_TransferCommand;
    info.FrameIndex = p_FrameIndex;
    info.AcquireShaderBarriers = &sacquires;
    info.ReleaseBarriers = separate ? &releases : nullptr;
    info.AcquireVertexBarriers = &vacquires;

    m_StatMeshSystem.RecordCopyCommands(info);
    m_CircleSystem.RecordCopyCommands(info);

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

void Renderer<D3>::GrowDeviceBuffers(const u32 p_FrameIndex)
{
    m_StatMeshSystem.GrowDeviceBuffers(p_FrameIndex);
    m_CircleSystem.GrowDeviceBuffers(p_FrameIndex);

    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    const u32 pcount = m_HostLightData.PointLights.GetSize();
    m_DeviceLightData.GrowDeviceBuffers(p_FrameIndex, dcount, pcount);
}

void Renderer<D3>::SendToDevice(const u32 p_FrameIndex)
{
    Task mainTask{};
    TKit::Array16<Task> tasks{};
    SendInfo info{};
    info.Tasks = &tasks;
    info.MainTask = &mainTask;
    info.SubmissionIndex = 0;

    m_StatMeshSystem.SendToDevice(p_FrameIndex, info);
    m_CircleSystem.SendToDevice(p_FrameIndex, info);

    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    if (dcount > 0)
    {
        VKit::DeviceBuffer &devDirBuffer = m_DeviceLightData.StagingDirectionals[p_FrameIndex];
        const auto &hostDirBuffer = m_HostLightData.DirectionalLights;
        devDirBuffer.Write<DirectionalLight>(hostDirBuffer);
        VKIT_CHECK_EXPRESSION(devDirBuffer.Flush());
    }

    const u32 pcount = m_HostLightData.PointLights.GetSize();
    if (pcount > 0)
    {
        VKit::DeviceBuffer &devPointBuffer = m_DeviceLightData.StagingPoints[p_FrameIndex];
        const auto &hostPointBuffer = m_HostLightData.PointLights;
        devPointBuffer.Write<PointLight>(hostPointBuffer);
        VKIT_CHECK_EXPRESSION(devPointBuffer.Flush());
    }
    if (!mainTask)
        return;

    mainTask();

    TKit::ITaskManager *tm = Core::GetTaskManager();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
}
VkPipelineStageFlags Renderer<D3>::RecordCopyCommands(const u32 p_FrameIndex, const VkCommandBuffer p_GraphicsCommand,
                                                      const VkCommandBuffer p_TransferCommand)
{
    const bool separate = Queues::IsSeparateTransferMode();
    TKit::Array16<VkBufferMemoryBarrier> sacquires{};
    TKit::Array4<VkBufferMemoryBarrier> vacquires{};
    TKit::Array32<VkBufferMemoryBarrier> releases{};

    CopyInfo info{};
    info.CommandBuffer = p_TransferCommand;
    info.FrameIndex = p_FrameIndex;
    info.AcquireShaderBarriers = &sacquires;
    info.ReleaseBarriers = separate ? &releases : nullptr;
    info.AcquireVertexBarriers = &vacquires;

    m_StatMeshSystem.RecordCopyCommands(info);
    m_CircleSystem.RecordCopyCommands(info);

    const u32 dsize = m_HostLightData.DirectionalLights.GetSize() * sizeof(DirectionalLight);
    if (dsize > 0)
    {
        VKit::DeviceBuffer &ldbuffer = m_DeviceLightData.DeviceLocalDirectionals[p_FrameIndex];
        VKit::DeviceBuffer &sdbuffer = m_DeviceLightData.StagingDirectionals[p_FrameIndex];
        ldbuffer.CopyFromBuffer(p_TransferCommand, sdbuffer, {.Size = dsize});
        sacquires.Append(CreateAcquireBarrier(ldbuffer, dsize));
        if (separate)
            releases.Append(CreateReleaseBarrier(ldbuffer, dsize));
    }

    const u32 psize = m_HostLightData.PointLights.GetSize() * sizeof(PointLight);
    if (psize > 0)
    {
        VKit::DeviceBuffer &lpbuffer = m_DeviceLightData.DeviceLocalPoints[p_FrameIndex];
        VKit::DeviceBuffer &spbuffer = m_DeviceLightData.StagingPoints[p_FrameIndex];
        lpbuffer.CopyFromBuffer(p_TransferCommand, spbuffer, {.Size = psize});
        sacquires.Append(CreateAcquireBarrier(lpbuffer, psize));
        if (separate)
            releases.Append(CreateReleaseBarrier(lpbuffer, psize));
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

template <Dimension D> static void setCameraViewport(const VkCommandBuffer p_CommandBuffer, const CameraInfo &p_Camera)
{
    const auto &table = Core::GetDeviceTable();
    if (!p_Camera.Transparent)
    {
        const Color &bg = p_Camera.BackgroundColor;
        TKit::FixedArray<VkClearAttachment, D - 1> clearAttachments{};
        clearAttachments[0].colorAttachment = 0;
        clearAttachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearAttachments[0].clearValue.color = {{bg.RGBA[0], bg.RGBA[1], bg.RGBA[2], bg.RGBA[3]}};

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
    RenderInfo<Shading_Unlit> unlitRenderInfo;
    unlitRenderInfo.CommandBuffer = p_CommandBuffer;
    unlitRenderInfo.FrameIndex = p_FrameIndex;

    for (const CameraInfo &camera : p_Cameras)
    {
        setCameraViewport<D2>(p_CommandBuffer, camera);

        unlitRenderInfo.Camera = &camera;
        renderFill(unlitRenderInfo);
        renderOutline(unlitRenderInfo);
    }
}

void Renderer<D3>::Render(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer,
                          const TKit::Span<const CameraInfo> p_Cameras)
{
    TKIT_PROFILE_NSCOPE("Onyx::Renderer<D3>::Render");
    RenderInfo<Shading_Lit> litRenderInfo;
    litRenderInfo.CommandBuffer = p_CommandBuffer;
    litRenderInfo.FrameIndex = p_FrameIndex;
    LightData light;

    light.DescriptorSet = m_DeviceLightData.DescriptorSets[p_FrameIndex];
    const u32 dcount = m_HostLightData.DirectionalLights.GetSize();
    const u32 pcount = m_HostLightData.PointLights.GetSize();

    light.AmbientColor = &AmbientColor;

    light.DirectionalCount = dcount;
    light.PointCount = pcount;

    litRenderInfo.Light = light;

    RenderInfo<Shading_Unlit> unlitRenderInfo;
    unlitRenderInfo.CommandBuffer = p_CommandBuffer;
    unlitRenderInfo.FrameIndex = p_FrameIndex;

    for (const CameraInfo &camera : p_Cameras)
    {
        setCameraViewport<D3>(p_CommandBuffer, camera);

        litRenderInfo.Camera = &camera;
        renderFill(litRenderInfo);

        unlitRenderInfo.Camera = &camera;
        renderOutline(unlitRenderInfo);
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

template class IRenderer<D2>;
template class IRenderer<D3>;

} // namespace Onyx::Detail

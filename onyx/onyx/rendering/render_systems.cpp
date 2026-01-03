#include "onyx/core/pch.hpp"
#include "onyx/rendering/render_systems.hpp"
#include "onyx/state/pipelines.hpp"
#include "vkit/state/descriptor_set.hpp"
#include "tkit/multiprocessing/topology.hpp"
#include "tkit/multiprocessing/task_manager.hpp"
#include "tkit/profiling/macros.hpp"

#ifdef TKIT_ENABLE_INSTRUMENTATION
#    include <atomic>
#    define INCREASE_DRAW_CALL_COUNT() s_DrawCallCount.fetch_add(1, std::memory_order_relaxed)
#else
#    define INCREASE_DRAW_CALL_COUNT()
#endif

namespace Onyx::Detail
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
static std::atomic<u32> s_DrawCallCount = 0;

u32 GetDrawCallCount()
{
    return s_DrawCallCount.load(std::memory_order_relaxed);
}
void ResetDrawCallCount()
{
    s_DrawCallCount.store(0, std::memory_order_relaxed);
}
#endif

RenderSystem::RenderSystem()
{
    for (u32 i = 0; i < MaxFramesInFlight; ++i)
        m_DeviceSubmissionId[i] = 0;
}
RenderSystem::~RenderSystem()
{
    Core::DeviceWaitIdle();
    m_Pipeline.Destroy();
}

template <Shading S> static void pushConstantData(const RenderInfo<S> &p_Info)
{
    PushConstantData<S> pdata{};
    pdata.ProjectionView = p_Info.Camera->ProjectionView;

    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
    if constexpr (S == Shading_Lit)
    {
        pdata.ViewPosition = f32v4(p_Info.Camera->ViewPosition, 1.f);
        pdata.AmbientColor = p_Info.Light.AmbientColor->RGBA;
        pdata.DirectionalLightCount = p_Info.Light.DirectionalCount;
        pdata.PointLightCount = p_Info.Light.PointCount;
        stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    const auto &table = Core::GetDeviceTable();
    table.CmdPushConstants(p_Info.CommandBuffer, Pipelines::GetGraphicsPipelineLayout(S), stages, 0,
                           sizeof(PushConstantData<S>), &pdata);
}

template <Shading S> static void bindDescriptorSets(const RenderInfo<S> &p_Info, const VkDescriptorSet p_InstanceData)
{
    if constexpr (S == Shading_Unlit)
        VKit::DescriptorSet::Bind(Core::GetDevice(), p_Info.CommandBuffer, p_InstanceData,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS, Pipelines::GetGraphicsPipelineLayout(Shading_Unlit));
    else
    {
        const TKit::FixedArray<VkDescriptorSet, 2> sets = {p_InstanceData, p_Info.Light.DescriptorSet};
        VKit::DescriptorSet::Bind(Core::GetDevice(), p_Info.CommandBuffer, sets, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Pipelines::GetGraphicsPipelineLayout(Shading_Lit));
    }
}

template <Dimension D, DrawMode DMode>
StatMeshSystem<D, DMode>::StatMeshSystem(const PipelineMode p_Mode,
                                         const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    TKIT_ASSERT(GetDrawMode(p_Mode) == DMode, "[ONYX] Draw and pipeline modes mismatch!");
    m_Pipeline = Pipelines::CreateStaticMeshPipeline<D>(p_Mode, p_RenderInfo);
}

template <Dimension D, DrawMode DMode>
void StatMeshSystem<D, DMode>::Draw(const InstanceData &p_InstanceData, const Mesh p_Mesh)
{
    thread_local const u32 threadIndex = TKit::Topology::GetThreadIndex();
    auto &hostData = m_ThreadHostData[threadIndex];
    hostData.Data[p_Mesh].Append(p_InstanceData);
    ++hostData.Instances;
}

template <Dimension D, DrawMode DMode> void StatMeshSystem<D, DMode>::GrowDeviceBuffers(const u32 p_FrameIndex)
{
    m_DeviceInstances = 0;
    for (const auto &hostData : m_ThreadHostData)
        m_DeviceInstances += hostData.Instances;

    m_DeviceData.GrowDeviceBuffers(p_FrameIndex, m_DeviceInstances);
}

template <Dimension D, DrawMode DMode> void StatMeshSystem<D, DMode>::SendToDevice(const u32 p_FrameIndex)
{
    VKit::DeviceBuffer &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    const u32 pcount = Assets::GetStaticMeshCount<D>();

    Task mainTask{};
    u32 sindex = 0;
    for (u32 i = 0; i < pcount; ++i)
        for (u32 j = 0; j < MaxThreads; ++j)
        {
            const auto &hostData = m_ThreadHostData[j];
            const auto &data = hostData.Data[i];
            if (!data.IsEmpty())
            {
                const auto copy = [&, offset] {
                    TKIT_PROFILE_NSCOPE("Onyx::StatMeshSystem::SendToDevice");
                    storageBuffer.Write<InstanceData>(data, {.DstOffset = offset});
                };
                if (!mainTask)
                    mainTask = copy;
                else
                {
                    Task &task = tasks.Append(copy);
                    sindex = tm->SubmitTask(&task, sindex);
                }
                offset += data.GetSize();
            }
        }

    mainTask();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
    VKIT_ASSERT_EXPRESSION(storageBuffer.Flush());
}

template <Dimension D, DrawMode DMode> void StatMeshSystem<D, DMode>::RecordCopyCommands(const CopyInfo &p_Info)
{
    if (!HasInstances(p_Info.FrameIndex))
        return;
    RenderSystem::AcknowledgeSubmission(p_Info.FrameIndex);
    const u32 size = m_DeviceInstances * sizeof(InstanceData);

    VKit::DeviceBuffer &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    VKit::DeviceBuffer &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    buffer.CopyFromBuffer(p_Info.CommandBuffer, staging, {.Size = size});

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}
template <Dimension D, DrawMode DMode> void StatMeshSystem<D, DMode>::Render(const RenderInfo &p_Info)
{
    if (m_DeviceInstances == 0)
        return;

    TKIT_PROFILE_NSCOPE("Onyx::StatMeshSystem::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);

    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr Shading shading = GetShading<D>(DMode);
    bindDescriptorSets<shading>(p_Info, instanceDescriptor);

    Assets::BindStaticMeshes<D>(p_Info.CommandBuffer);

    pushConstantData<shading>(p_Info);
    u32 firstInstance = 0;

    const u32 pcount = Assets::GetStaticMeshCount<D>();
    for (u32 i = 0; i < pcount; ++i)
    {
        u32 instanceCount = 0;
        for (const auto &hostData : m_ThreadHostData)
            instanceCount += hostData.Data[i].GetSize();
        if (instanceCount == 0)
            continue;

        Assets::DrawStaticMesh<D>(p_Info.CommandBuffer, i, firstInstance, instanceCount);
        INCREASE_DRAW_CALL_COUNT();
        firstInstance += instanceCount;
    }
}

template <Dimension D, DrawMode DMode> void StatMeshSystem<D, DMode>::Flush()
{
    RenderSystem::Flush();
    for (auto &hostData : m_ThreadHostData)
    {
        for (auto &data : hostData.Data)
            data.Clear();
        hostData.Data.Resize(Onyx::Assets::GetStaticMeshCount<D>());
        hostData.Instances = 0;
    }
}

template <Dimension D, DrawMode DMode>
CircleSystem<D, DMode>::CircleSystem(const PipelineMode p_Mode, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
{
    TKIT_ASSERT(GetDrawMode(p_Mode) == DMode, "[ONYX] Draw and pipeline modes mismatch!");
    m_Pipeline = Pipelines::CreateCirclePipeline<D>(p_Mode, p_RenderInfo);
}

template <Dimension D, DrawMode DMode>
void CircleSystem<D, DMode>::Draw(const InstanceData &p_InstanceData, const CircleOptions &p_Options)
{
    // if (TKit::Approximately(p_Options.LowerAngle, p_Options.UpperAngle) ||
    //     TKit::Approximately(p_Options.Hollowness, 1.f))
    //     return;

    thread_local const u32 threadIndex = TKit::Topology::GetThreadIndex();
    auto &hostData = m_ThreadHostData[threadIndex];

    CircleInstanceData instanceData;
    instanceData.Base = p_InstanceData;

    instanceData.LowerCos = Math::Cosine(p_Options.LowerAngle);
    instanceData.LowerSin = Math::Sine(p_Options.LowerAngle);
    instanceData.UpperCos = Math::Cosine(p_Options.UpperAngle);
    instanceData.UpperSin = Math::Sine(p_Options.UpperAngle);

    instanceData.AngleOverflow = Math::Absolute(p_Options.UpperAngle - p_Options.LowerAngle) > Math::Pi<f32>() ? 1 : 0;
    instanceData.Hollowness = p_Options.Hollowness;
    instanceData.InnerFade = p_Options.InnerFade;
    instanceData.OuterFade = p_Options.OuterFade;

    hostData.Data.Append(instanceData);
}

template <Dimension D, DrawMode DMode> void CircleSystem<D, DMode>::GrowDeviceBuffers(const u32 p_FrameIndex)
{
    m_DeviceInstances = 0;
    for (const auto &hostData : m_ThreadHostData)
        m_DeviceInstances += hostData.Data.GetSize();

    m_DeviceData.GrowDeviceBuffers(p_FrameIndex, m_DeviceInstances);
}
template <Dimension D, DrawMode DMode> void CircleSystem<D, DMode>::SendToDevice(const u32 p_FrameIndex)
{
    VKit::DeviceBuffer &storageBuffer = m_DeviceData.StagingStorage[p_FrameIndex];
    u32 offset = 0;

    TaskArray tasks{};
    TKit::ITaskManager *tm = Core::GetTaskManager();

    u32 sindex = 0;
    Task mainTask{};
    for (u32 i = 0; i < MaxThreads; ++i)
    {
        const auto &hostData = m_ThreadHostData[i];
        if (!hostData.Data.IsEmpty())
        {
            const auto copy = [&, offset] {
                TKIT_PROFILE_NSCOPE("Onyx::CircleSystem::SendToDevice");
                storageBuffer.Write<CircleInstanceData>(hostData.Data, {.DstOffset = offset});
            };
            if (!mainTask)
                mainTask = copy;
            else
            {
                Task &task = tasks.Append(copy);
                sindex = tm->SubmitTask(&task, sindex);
            }
            offset += hostData.Data.GetSize();
        }
    }

    mainTask();
    for (const Task &task : tasks)
        tm->WaitUntilFinished(task);
    VKIT_ASSERT_EXPRESSION(storageBuffer.Flush());
}

template <Dimension D, DrawMode DMode> void CircleSystem<D, DMode>::RecordCopyCommands(const CopyInfo &p_Info)
{
    if (!HasInstances(p_Info.FrameIndex))
        return;
    RenderSystem::AcknowledgeSubmission(p_Info.FrameIndex);

    const u32 size = m_DeviceInstances * sizeof(CircleInstanceData);

    VKit::DeviceBuffer &buffer = m_DeviceData.DeviceLocalStorage[p_Info.FrameIndex];
    VKit::DeviceBuffer &staging = m_DeviceData.StagingStorage[p_Info.FrameIndex];
    buffer.CopyFromBuffer(p_Info.CommandBuffer, staging, {.Size = size});

    p_Info.AcquireShaderBarriers->Append(CreateAcquireBarrier(buffer, size));
    if (p_Info.ReleaseBarriers)
        p_Info.ReleaseBarriers->Append(CreateReleaseBarrier(buffer, size));
}

template <Dimension D, DrawMode DMode> void CircleSystem<D, DMode>::Render(const RenderInfo &p_Info)
{
    if (m_DeviceInstances == 0)
        return;
    TKIT_PROFILE_NSCOPE("Onyx::CircleSystem::Render");

    m_Pipeline.Bind(p_Info.CommandBuffer);
    const VkDescriptorSet instanceDescriptor = m_DeviceData.DescriptorSets[p_Info.FrameIndex];

    constexpr Shading shading = GetShading<D>(DMode);
    bindDescriptorSets<shading>(p_Info, instanceDescriptor);
    pushConstantData<shading>(p_Info);

    const auto &table = Core::GetDeviceTable();

    u32 instanceCount = 0;
    for (const auto &hostData : m_ThreadHostData)
        instanceCount += hostData.Data.GetSize();
    TKIT_ASSERT(instanceCount > 0,
                "[ONYX] Circle renderer instance count is zero, which should have trigger an early return earlier");

    table.CmdDraw(p_Info.CommandBuffer, 6, instanceCount, 0, 0);
    INCREASE_DRAW_CALL_COUNT();
}

template <Dimension D, DrawMode DMode> void CircleSystem<D, DMode>::Flush()
{
    RenderSystem::Flush();
    for (auto &hostData : m_ThreadHostData)
        hostData.Data.Clear();
}

// This is crazy
template class ONYX_API StatMeshSystem<D2, Draw_Fill>;
template class ONYX_API StatMeshSystem<D2, Draw_Outline>;

template class ONYX_API StatMeshSystem<D3, Draw_Fill>;
template class ONYX_API StatMeshSystem<D3, Draw_Outline>;

template class ONYX_API CircleSystem<D2, Draw_Fill>;
template class ONYX_API CircleSystem<D2, Draw_Outline>;

template class ONYX_API CircleSystem<D3, Draw_Fill>;
template class ONYX_API CircleSystem<D3, Draw_Outline>;

} // namespace Onyx::Detail

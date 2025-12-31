#pragma once

#include "onyx/resource/state.hpp"
#include "onyx/resource/assets.hpp"
#include "onyx/resource/options.hpp"
#include "vkit/pipeline/graphics_pipeline.hpp"
#include "tkit/container/fixed_array.hpp"

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324)

namespace Onyx::Detail
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
u32 GetDrawCallCount();
void ResetDrawCallCount();
#endif

class RenderSystem
{
  public:
    RenderSystem();
    ~RenderSystem();

    bool HasInstances(const u32 p_FrameIndex) const
    {
        return m_DeviceInstances != 0 && m_DeviceSubmissionId[p_FrameIndex] != m_HostSubmissionId;
    }
    void Flush()
    {
        ++m_HostSubmissionId;
    }
    void AcknowledgeSubmission(const u32 p_FrameIndex)
    {
        m_DeviceSubmissionId[p_FrameIndex] = m_HostSubmissionId;
    }

  protected:
    VKit::GraphicsPipeline m_Pipeline{};

    PerFrameData<u64> m_DeviceSubmissionId{};
    u64 m_HostSubmissionId = 0;
    u32 m_DeviceInstances = 0;
};

template <Dimension D, DrawMode DMode> class StatMeshSystem final : public RenderSystem
{
    TKIT_NON_COPYABLE(StatMeshSystem)

    using RenderInfo = RenderInfo<GetShading<D>(DMode)>;
    using InstanceData = InstanceData<D, DMode>;

  public:
    StatMeshSystem(PipelineMode p_Mode, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

    void Draw(const InstanceData &p_InstanceData, Assets::Mesh p_Mesh);

    void GrowDeviceBuffers(u32 p_FrameIndex);
    void SendToDevice(u32 p_FrameIndex);

    void RecordCopyCommands(const CopyInfo &p_Info);
    void Render(const RenderInfo &p_Info);
    void Flush();

  private:
    struct alignas(TKIT_CACHE_LINE_SIZE) MeshHostData
    {
        TKit::Array<HostBuffer<InstanceData>, MaxStatMeshes> Data{};
        u32 Instances = 0;
    };

    TKit::FixedArray<MeshHostData, MaxThreads> m_ThreadHostData{};
    DeviceData<InstanceData> m_DeviceData{};
};

template <Dimension D, DrawMode DMode> class CircleSystem final : public RenderSystem
{
    TKIT_NON_COPYABLE(CircleSystem)

    using RenderInfo = RenderInfo<GetShading<D>(DMode)>;
    using InstanceData = InstanceData<D, DMode>;
    using CircleInstanceData = CircleInstanceData<D, DMode>;

  public:
    CircleSystem(PipelineMode p_Mode, const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

    void Draw(const InstanceData &p_InstanceData, const CircleOptions &p_Properties);

    void GrowDeviceBuffers(u32 p_FrameIndex);
    void SendToDevice(u32 p_FrameIndex);

    void RecordCopyCommands(const CopyInfo &p_Info);
    void Render(const RenderInfo &p_Info);
    void Flush();

  private:
    struct alignas(TKIT_CACHE_LINE_SIZE) CircleHostData
    {
        HostBuffer<CircleInstanceData> Data;
    };

    TKit::FixedArray<CircleHostData, MaxThreads> m_ThreadHostData{};
    DeviceData<CircleInstanceData> m_DeviceData{};
};
TKIT_COMPILER_WARNING_IGNORE_POP()
} // namespace Onyx::Detail

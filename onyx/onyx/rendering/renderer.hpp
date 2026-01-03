#pragma once

#include "onyx/rendering/render_systems.hpp"
#include "tkit/multiprocessing/task_manager.hpp"

namespace Onyx::Detail
{
struct SendInfo
{
    TKit::Array16<Task> *Tasks;
    Task *MainTask;
    u32 SubmissionIndex;
};

using DrawFlags = u8;
enum DrawFlagBit : DrawFlags
{
    /// Do not write to stencil buffer, perform fill operation.
    DrawFlag_NoStencilWriteDoFill = 1 << 0,

    /// Write to stencil buffer, perform fill operation.
    DrawFlag_DoStencilWriteDoFill = 1 << 1,

    /// Write to stencil buffer, do not perform fill operation.
    DrawFlag_DoStencilWriteNoFill = 1 << 2,

    /// Test stencil buffer, do not perform fill operation.
    DrawFlag_DoStencilTestNoFill = 1 << 3,
};

template <Dimension D> class IRenderer
{
    TKIT_NON_COPYABLE(IRenderer)
    template <template <Dimension, DrawMode> typename System> struct Expand
    {
        Expand(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo)
            : NoStencilWriteDoFill(Pipeline_NoStencilWriteDoFill, p_RenderInfo),
              DoStencilWriteDoFill(Pipeline_DoStencilWriteDoFill, p_RenderInfo),
              DoStencilWriteNoFill(Pipeline_DoStencilWriteNoFill, p_RenderInfo),
              DoStencilTestNoFill(Pipeline_DoStencilTestNoFill, p_RenderInfo)
        {
        }

        void GrowDeviceBuffers(const u32 p_FrameIndex)
        {
            NoStencilWriteDoFill.GrowDeviceBuffers(p_FrameIndex);
            DoStencilWriteDoFill.GrowDeviceBuffers(p_FrameIndex);
            DoStencilWriteNoFill.GrowDeviceBuffers(p_FrameIndex);
            DoStencilTestNoFill.GrowDeviceBuffers(p_FrameIndex);
        }

        void SendToDevice(const u32 p_FrameIndex, SendInfo &p_Info)
        {
            TKit::ITaskManager *tm = Core::GetTaskManager();
            const auto send = [&p_Info, p_FrameIndex, tm](auto &p_System) {
                if (!p_System.HasInstances(p_FrameIndex))
                    return;

                const auto fn = [&p_System, p_FrameIndex]() { p_System.SendToDevice(p_FrameIndex); };
                Task &mainTask = *p_Info.MainTask;
                if (!mainTask)
                    mainTask = fn;
                else
                {
                    Task &task = p_Info.Tasks->Append(fn);
                    p_Info.SubmissionIndex = tm->SubmitTask(&task, p_Info.SubmissionIndex);
                }
            };

            send(NoStencilWriteDoFill);
            send(DoStencilWriteDoFill);
            send(DoStencilWriteNoFill);
            send(DoStencilTestNoFill);
        }

        void RecordCopyCommands(const CopyInfo &p_Info)
        {
            NoStencilWriteDoFill.RecordCopyCommands(p_Info);
            DoStencilWriteDoFill.RecordCopyCommands(p_Info);
            DoStencilWriteNoFill.RecordCopyCommands(p_Info);
            DoStencilTestNoFill.RecordCopyCommands(p_Info);
        }

        void Flush()
        {
            NoStencilWriteDoFill.Flush();
            DoStencilWriteDoFill.Flush();
            DoStencilWriteNoFill.Flush();
            DoStencilTestNoFill.Flush();
        }
        System<D, Draw_Fill> NoStencilWriteDoFill;
        System<D, Draw_Fill> DoStencilWriteDoFill;
        System<D, Draw_Outline> DoStencilWriteNoFill;
        System<D, Draw_Outline> DoStencilTestNoFill;
    };

  public:
    IRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

    void DrawStaticMesh(const RenderState<D> &p_State, const f32m<D> &p_Transform, Mesh p_Mesh,
                        DrawFlags p_Flags)
    {
        draw(m_StatMeshSystem, p_State, p_Transform, p_Mesh, p_Flags);
    }
    void DrawCircle(const RenderState<D> &p_State, const f32m<D> &p_Transform, const CircleOptions &p_Options,
                    DrawFlags p_Flags)
    {
        draw(m_CircleSystem, p_State, p_Transform, p_Options, p_Flags);
    }

  protected:
    Expand<StatMeshSystem> m_StatMeshSystem;
    Expand<CircleSystem> m_CircleSystem;

    template <Shading S> void renderFill(const RenderInfo<S> &p_Info)
    {
        m_StatMeshSystem.NoStencilWriteDoFill.Render(p_Info);
        m_CircleSystem.NoStencilWriteDoFill.Render(p_Info);
        m_StatMeshSystem.DoStencilWriteDoFill.Render(p_Info);
        m_CircleSystem.DoStencilWriteDoFill.Render(p_Info);
    }
    void renderOutline(const RenderInfo<Shading_Unlit> &p_Info)
    {
        m_StatMeshSystem.DoStencilWriteNoFill.Render(p_Info);
        m_CircleSystem.DoStencilWriteNoFill.Render(p_Info);
        m_StatMeshSystem.DoStencilTestNoFill.Render(p_Info);
        m_CircleSystem.DoStencilTestNoFill.Render(p_Info);
    }

  private:
    template <typename Renderer, typename DrawArg>
    void draw(Renderer &p_Renderer, const RenderState<D> &p_State, const f32m<D> &p_Transform, DrawArg &&p_Arg,
              DrawFlags p_Flags);
};

template <Dimension D> class Renderer;

template <> class ONYX_API Renderer<D2> final : public IRenderer<D2>
{
  public:
    using IRenderer<D2>::IRenderer;

    void GrowDeviceBuffers(u32 p_FrameIndex);
    void SendToDevice(u32 p_FrameIndex);

    VkPipelineStageFlags RecordCopyCommands(u32 p_FrameIndex, VkCommandBuffer p_GraphicsCommand,
                                            VkCommandBuffer p_TransferCommand);

    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer, TKit::Span<const CameraInfo> p_Cameras);
    void Flush();
};
} // namespace Onyx::Detail

namespace Onyx
{
struct DirectionalLight
{
    f32v3 Direction;
    f32 Intensity;
    u32 Color;
};

struct PointLight
{
    f32v3 Position;
    f32 Intensity;
    f32 Radius;
    u32 Color;
};
} // namespace Onyx

namespace Onyx::Detail
{
struct ONYX_API DeviceLightData
{
    TKIT_NON_COPYABLE(DeviceLightData)

    DeviceLightData();
    ~DeviceLightData();

    void GrowDeviceBuffers(u32 p_FrameIndex, u32 p_Directionals, u32 p_Points);

    PerFrameData<VKit::DeviceBuffer> DeviceLocalDirectionals;
    PerFrameData<VKit::DeviceBuffer> DeviceLocalPoints;
    PerFrameData<VKit::DeviceBuffer> StagingDirectionals;
    PerFrameData<VKit::DeviceBuffer> StagingPoints;
    PerFrameData<VkDescriptorSet> DescriptorSets;
};

struct ONYX_API HostLightData
{
    HostBuffer<DirectionalLight> DirectionalLights;
    HostBuffer<PointLight> PointLights;
};

template <> class ONYX_API Renderer<D3> final : public IRenderer<D3>
{
  public:
    Renderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo);

    void GrowDeviceBuffers(u32 p_FrameIndex);

    void SendToDevice(u32 p_FrameIndex);

    VkPipelineStageFlags RecordCopyCommands(u32 p_FrameIndex, VkCommandBuffer p_GraphicsCommand,
                                            VkCommandBuffer p_TransferCommand);

    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer, TKit::Span<const CameraInfo> p_Cameras);

    void AddDirectionalLight(const DirectionalLight &p_Light);
    void AddPointLight(const PointLight &p_Light);
    void Flush();

    Color AmbientColor = Color{Color::WHITE, 0.4f};

  private:
    HostLightData m_HostLightData{};
    DeviceLightData m_DeviceLightData{};
};
} // namespace Onyx::Detail

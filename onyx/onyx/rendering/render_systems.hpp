#pragma once

#include "onyx/data/state.hpp"
#include "onyx/object/mesh.hpp"
#include "onyx/object/primitives.hpp"
#include "onyx/data/options.hpp"
#include "tkit/container/array.hpp"

namespace Onyx::Detail
{
#ifdef TKIT_ENABLE_INSTRUMENTATION
u32 GetDrawCallCount() noexcept;
void ResetDrawCallCount() noexcept;
#endif

/**
 * @brief Responsible for handling all user draw calls that involve meshes built from a `Mesh` instance.
 *
 * User draw calls will be stored and then recorded in a command buffer when the render step begins.
 * This renderer uses instanced rendering to draw multiple instances of the same mesh in a single draw call.
 *
 */
template <Dimension D, PipelineMode PMode> class MeshRenderer
{
    TKIT_NON_COPYABLE(MeshRenderer)

    using RenderInfo = RenderInfo<GetDrawLevel<D, PMode>()>;
    using InstanceData = InstanceData<GetDrawLevel<D, PMode>()>;

  public:
    MeshRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;
    ~MeshRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a mesh instance. This is an onyx draw call.
     *
     * This method does not record any vulkan commands.
     *
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_Mesh The mesh to draw.
     */
    void Draw(const InstanceData &p_InstanceData, const Mesh<D> &p_Mesh) noexcept;

    /**
     * @brief Grow all device buffers to fit host data.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void GrowToFit(u32 p_FrameIndex) noexcept;

    /**
     * @brief Send all host data to the device through storage, vertex or index buffers.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void SendToDevice(u32 p_FrameIndex) noexcept;

    /**
     * @brief Record vulkan copy commands to send data to a device local buffer.
     *
     * @param p_Info The necessary information to record the copies.
     */
    void RecordCopyCommands(const CopyInfo &p_Info) noexcept;

    /**
     * @brief Record the current command buffer with the stored onyx draw calls.
     *
     * It will also copy all of the host data (stored in c++ standars data structures) to the device data (stored in
     * storage buffers).
     *
     * @param p_Info The rendering info of the current frame.
     */
    void Render(const RenderInfo &p_Info) noexcept;

    /**
     * @brief Clear all of the stored onyx draw calls.
     *
     * This method can be called optionally. Not doing so will persist the onyx draw calls until the next frame.
     *
     */
    void Flush() noexcept;

    bool HasInstances() const noexcept;

  private:
    struct alignas(TKIT_CACHE_LINE_SIZE) MeshHostData
    {
        TKit::HashMap<Mesh<D>, HostStorageBuffer<InstanceData>> Data{};
        u32 Instances = 0;
        std::byte _Pad[TKIT_CACHE_LINE_SIZE];
    };

    VKit::GraphicsPipeline m_Pipeline{};

    TKit::Array<MeshHostData, ONYX_MAX_THREADS> m_HostData{};

    DeviceData<InstanceData> m_DeviceData{};

    u32 m_DeviceInstances = 0;
};

/**
 * @brief Responsible for handling all user draw calls that involve primitives, such as triangles, squares, capsules
 * etc.
 *
 * User draw calls will be stored and then recorded in a command buffer when the render step begins.
 * This renderer uses instanced rendering to draw multiple instances of the same primitive in a single draw call.
 * All of the primitives geometry data is stored in two unique index and vertex buffers created at the beginning of the
 * program.
 *
 */
template <Dimension D, PipelineMode PMode> class PrimitiveRenderer
{
    TKIT_NON_COPYABLE(PrimitiveRenderer)

    using RenderInfo = RenderInfo<GetDrawLevel<D, PMode>()>;
    using InstanceData = InstanceData<GetDrawLevel<D, PMode>()>;

  public:
    PrimitiveRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;
    ~PrimitiveRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a primitive instance. This is an onyx draw call.
     *
     * This method does not record any vulkan commands.
     *
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_PrimitiveIndex The index of the primitive to draw. Can be queried from `Primitive<D>::Get...Index()`
     */
    void Draw(const InstanceData &p_InstanceData, u32 p_PrimitiveIndex) noexcept;

    /**
     * @brief Grow all device buffers to fit host data.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void GrowToFit(u32 p_FrameIndex) noexcept;

    /**
     * @brief Send all host data to the device through storage, vertex or index buffers.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void SendToDevice(u32 p_FrameIndex) noexcept;

    /**
     * @brief Record vulkan copy commands to send data to a device local buffer.
     *
     * @param p_Info The necessary information to record the copies.
     */
    void RecordCopyCommands(const CopyInfo &p_Info) noexcept;

    /**
     * @brief Record the current command buffer with the stored onyx draw calls.
     *
     * It will also copy all of the host data (stored in c++ standars data structures) to the device data (stored in
     * storage buffers).
     *
     * @param p_Info The rendering info of the current frame.
     */
    void Render(const RenderInfo &p_Info) noexcept;

    /**
     * @brief Clear all of the stored onyx draw calls.
     *
     * This method can be called optionally. Not doing do will persist the onyx draw calls until the next frame.
     *
     */
    void Flush() noexcept;

    bool HasInstances() const noexcept;

  private:
    struct alignas(TKIT_CACHE_LINE_SIZE) PrimitiveHostData
    {
        TKit::Array<HostStorageBuffer<InstanceData>, Primitives<D>::AMOUNT> Data{};
        u32 Instances = 0;
        std::byte _Pad[TKIT_CACHE_LINE_SIZE];
    };

    VKit::GraphicsPipeline m_Pipeline{};

    TKit::Array<PrimitiveHostData, ONYX_MAX_THREADS> m_HostData{};

    DeviceData<InstanceData> m_DeviceData{};

    u32 m_DeviceInstances = 0;
};

/**
 * @brief Responsible for handling all user draw calls that involve polygons polygons of arbitrary geometry.
 *
 * The polygons vertices must be sorted consistently, either clockwise or counter-clockwise. Failing to do so will
 * result in the polygon not being rendered correctly.
 *
 * User draw calls will be stored and then recorded in a command buffer when the render step begins.
 * This renderer does not use instanced rendering, as each polygon is unique and has its own geometry.
 * All of the polygons geometry data is stored in two uniques index and vertex buffers that grow progressively as more
 * polygons are drawn.
 *
 */
template <Dimension D, PipelineMode PMode> class PolygonRenderer
{
    TKIT_NON_COPYABLE(PolygonRenderer)

    using RenderInfo = RenderInfo<GetDrawLevel<D, PMode>()>;
    using InstanceData = InstanceData<GetDrawLevel<D, PMode>()>;

  public:
    PolygonRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;
    ~PolygonRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a polygon instance. This is an onyx draw call.
     *
     * This method does not record any vulkan commands.
     *
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_Vertices The vertices of the polygon to draw. Must be sorted consistently.
     */
    void Draw(const InstanceData &p_InstanceData, TKit::Span<const fvec2> p_Vertices) noexcept;

    /**
     * @brief Grow all device buffers to fit host data.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void GrowToFit(u32 p_FrameIndex) noexcept;

    /**
     * @brief Send all host data to the device through storage, vertex or index buffers.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void SendToDevice(u32 p_FrameIndex) noexcept;

    /**
     * @brief Record vulkan copy commands to send data to a device local buffer.
     *
     * @param p_Info The necessary information to record the copies.
     */
    void RecordCopyCommands(const CopyInfo &p_Info) noexcept;

    /**
     * @brief Record the current command buffer with the stored onyx draw calls.
     *
     * It will also copy all of the host data (stored in c++ standars data structures) to the device data (stored in
     * storage buffers).
     *
     * @param p_Info The rendering info of the current frame.
     */
    void Render(const RenderInfo &p_Info) noexcept;

    /**
     * @brief Clear all of the stored onyx draw calls.
     *
     * This method can be called optionally. Not doing do will persist the onyx draw calls until the next frame.
     *
     */
    void Flush() noexcept;

    bool HasInstances() const noexcept;

  private:
    struct alignas(TKIT_CACHE_LINE_SIZE) PolygonHostData
    {
        HostStorageBuffer<InstanceData> Data;
        HostStorageBuffer<PrimitiveDataLayout> Layouts;
        HostVertexBuffer<D> Vertices;
        HostIndexBuffer Indices;
        std::byte _Pad[TKIT_CACHE_LINE_SIZE];
    };

    VKit::GraphicsPipeline m_Pipeline{};

    TKit::Array<PolygonHostData, ONYX_MAX_THREADS> m_HostData{};

    PolygonDeviceData<D, GetDrawLevel<D, PMode>()> m_DeviceData{};

    u32 m_DeviceInstances = 0;
    u32 m_DeviceVertices = 0;
    u32 m_DeviceIndices = 0;
};

/**
 * @brief Responsible for handling all user draw calls that involve circles.
 *
 * This renderer uses a special shader with no input vertices to draw a square, and then discards fragments that are
 * outside the circle or the user-defined arc.
 *
 * User draw calls will be stored and then recorded in a command buffer when the render step begins.
 * This renderer uses instanced rendering for all of its draw calls, as all circles share the same geometry.
 *
 */
template <Dimension D, PipelineMode PMode> class CircleRenderer
{
    TKIT_NON_COPYABLE(CircleRenderer)

    using InstanceData = InstanceData<GetDrawLevel<D, PMode>()>;
    using RenderInfo = RenderInfo<GetDrawLevel<D, PMode>()>;

    using CircleInstanceData = CircleInstanceData<GetDrawLevel<D, PMode>()>;

  public:
    CircleRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;
    ~CircleRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a circle instance. This is an onyx draw call.
     *
     * This draw call would actually correspond to an arc, as lower and upper angles can be specified. To draw
     * a full circle, set the lower angle to 0 and the upper angle to 2 * PI (or any combination that results in p_Upper
     * - p_Lower == 2 * PI).
     *
     * Nothing will be drawn if p_LowerAngle == p_UpperAngle or if p_Hollowness approaches 1.
     *
     * This method does not record any vulkan commands.
     *
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1, denoting how much the circle fades from the center to the
     * edge.
     * @param p_OuterFade A value between 0 and 1, denoting how much the circle fades from the edge to the
     * center.
     * @param p_Hollowness A value between 0 and 1, denoting how hollow the circle is. 0 is a full circle and
     * 1 would correspond to not having a circle at all.
     * @param p_LowerAngle The angle from which the arc starts.
     * @param p_UpperAngle The angle at which the arc ends.
     */
    void Draw(const InstanceData &p_InstanceData, const CircleOptions &p_Properties) noexcept;

    /**
     * @brief Grow all device buffers to fit host data.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void GrowToFit(u32 p_FrameIndex) noexcept;

    /**
     * @brief Send all host data to the device through storage, vertex or index buffers.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void SendToDevice(u32 p_FrameIndex) noexcept;

    /**
     * @brief Record vulkan copy commands to send data to a device local buffer.
     *
     * @param p_Info The necessary information to record the copies.
     */
    void RecordCopyCommands(const CopyInfo &p_Info) noexcept;

    /**
     * @brief Record the current command buffer with the stored onyx draw calls.
     *
     * It will also copy all of the host data (stored in c++ standars data structures) to the device data (stored in
     * storage buffers).
     *
     * @param p_Info The rendering info of the current frame.
     */
    void Render(const RenderInfo &p_Info) noexcept;

    /**
     * @brief Clear all of the stored onyx draw calls.
     *
     * This method can be called optionally. Not doing do will persist the onyx draw calls until the next frame.
     *
     */
    void Flush() noexcept;

    bool HasInstances() const noexcept;

  private:
    struct alignas(TKIT_CACHE_LINE_SIZE) CircleHostData
    {
        HostStorageBuffer<CircleInstanceData> Data;
        std::byte _Pad[TKIT_CACHE_LINE_SIZE];
    };

    VKit::GraphicsPipeline m_Pipeline{};

    TKit::Array<CircleHostData, ONYX_MAX_THREADS> m_HostData{};

    DeviceData<CircleInstanceData> m_DeviceData{};

    u32 m_DeviceInstances = 0;
};

} // namespace Onyx::Detail

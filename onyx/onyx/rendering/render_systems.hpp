#pragma once

#include "onyx/rendering/render_specs.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#ifndef ONYX_BUFFER_INITIAL_CAPACITY
#    define ONYX_BUFFER_INITIAL_CAPACITY 4
#endif

namespace ONYX
{
/**
 * @brief Responsible for handling all user draw calls that involve meshes built from a Model instance.
 *
 * User draw calls will be stored and then recorded in a command buffer when the render step begins.
 * This renderer uses instanced rendering to draw multiple instances of the same model in a single draw call.
 *
 */
template <Dimension D, PipelineMode PMode> class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)

    using InstanceData = ONYX::InstanceData<D, GetDrawMode<PMode>()>;
    using RenderInfo = ONYX::RenderInfo<D, GetDrawMode<PMode>()>;

  public:
    MeshRenderer(VkRenderPass p_RenderPass) noexcept;
    ~MeshRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a model instance. This is an onyx draw call.
     *
     * This method does not perform any vulkan commands.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_Model The model to draw.
     */
    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, const KIT::Ref<const Model<D>> &p_Model) noexcept;

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

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    // Could actually use a pointer to the model instead of a reference and take extra care the model still lives
    // while drawing
    using HostInstanceData = HashMap<KIT::Ref<const Model<D>>, DynamicArray<InstanceData>>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

/**
 * @brief Responsible for handling all user draw calls that involve primitives, such as triangles, squares, capsules
 * etc.
 *
 * User draw calls will be stored and then recorded in a command buffer when the render step begins.
 * This renderer uses instanced rendering to draw multiple instances of the same primitive in a single draw call.
 * All of the primitives geometry data is stored in two uniques index and vertex buffers created at the beginning of the
 * program.
 *
 */
template <Dimension D, PipelineMode PMode> class ONYX_API PrimitiveRenderer
{
    KIT_NON_COPYABLE(PrimitiveRenderer)

    using InstanceData = ONYX::InstanceData<D, GetDrawMode<PMode>()>;
    using RenderInfo = ONYX::RenderInfo<D, GetDrawMode<PMode>()>;

  public:
    PrimitiveRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PrimitiveRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a primitive instance. This is an onyx draw call.
     *
     * This method does not perform any vulkan commands.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_PrimitiveIndex The index of the primitive to draw. Can be queried from Primitive<D>::Get...Index()
     */
    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, usize p_PrimitiveIndex) noexcept;

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

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    using HostInstanceData = std::array<DynamicArray<InstanceData>, Primitives<D>::AMOUNT>;

    HostInstanceData m_HostInstanceData;
    DeviceInstanceData<InstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
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
template <Dimension D, PipelineMode PMode> class ONYX_API PolygonRenderer
{
    KIT_NON_COPYABLE(PolygonRenderer)

    using InstanceData = ONYX::InstanceData<D, GetDrawMode<PMode>()>;
    using RenderInfo = ONYX::RenderInfo<D, GetDrawMode<PMode>()>;

    using PolygonInstanceData = ONYX::PolygonInstanceData<D, GetDrawMode<PMode>()>;
    using PolygonDeviceInstanceData = ONYX::PolygonDeviceInstanceData<D, GetDrawMode<PMode>()>;

  public:
    PolygonRenderer(VkRenderPass p_RenderPass) noexcept;
    ~PolygonRenderer() noexcept;

    /**
     * @brief Record and store the data needed to draw a polygon instance. This is an onyx draw call.
     *
     * This method does not perform any vulkan commands.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_Vertices The vertices of the polygon to draw. Must be sorted consistently.
     */
    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, std::span<const vec<D>> p_Vertices) noexcept;

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

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of polygons to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<PolygonInstanceData> m_HostInstanceData;
    PolygonDeviceInstanceData m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
    DynamicArray<Vertex<D>> m_Vertices;
    DynamicArray<Index> m_Indices;
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
template <Dimension D, PipelineMode PMode> class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)

    using InstanceData = ONYX::InstanceData<D, GetDrawMode<PMode>()>;
    using RenderInfo = ONYX::RenderInfo<D, GetDrawMode<PMode>()>;

    using CircleInstanceData = ONYX::CircleInstanceData<D, GetDrawMode<PMode>()>;

  public:
    CircleRenderer(VkRenderPass p_RenderPass) noexcept;
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
     * This method does not perform any vulkan commands.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_InstanceData The data needed to draw the instance (transforms, material data, etc.).
     * @param p_LowerAngle The angle from which the arc starts.
     * @param p_UpperAngle The angle at which the arc ends.
     * @param p_Hollowness A normalized value between 0 and 1, denoting how hollow the circle is. 0 is a full circle and
     * 1 would correspond to not having a circle at all.
     */
    void Draw(u32 p_FrameIndex, const InstanceData &p_InstanceData, f32 p_LowerAngle, f32 p_UpperAngle,
              f32 p_Hollowness) noexcept;

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

  private:
    KIT::Storage<Pipeline> m_Pipeline;

    // Batch data maps perfectly to the number of circles to be drawn i.e number of entries in storage buffer.
    // StorageSizes is not needed
    DynamicArray<CircleInstanceData> m_HostInstanceData;
    DeviceInstanceData<CircleInstanceData> m_DeviceInstanceData{ONYX_BUFFER_INITIAL_CAPACITY};
};

} // namespace ONYX
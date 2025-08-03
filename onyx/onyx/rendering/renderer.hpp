#pragma once

#include "onyx/rendering/render_systems.hpp"

namespace Onyx::Detail
{
/**
 * @brief The RenderSystem struct manages multiple renderers with different pipeline modes.
 *
 * This template struct holds instances of a renderer R with different PipelineModes,
 * allowing for different rendering passes such as filling and outlining shapes.
 *
 * @tparam D The dimensionality (`D2`or `D3`).
 * @tparam R The renderer type template (e.g., `MeshRenderer`, `PrimitiveRenderer`).
 */
template <Dimension D, template <Dimension, PipelineMode> typename Renderer> struct RenderSystem
{
    RenderSystem(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;

    /**
     * @brief Clear all host data in each renderer.
     *
     * This method should be called to reset the renderers' state, typically at the beginning or end of a frame.
     */
    void Flush() noexcept;

    /**
     * @brief Grow all device buffers to fit host data.
     *
     * @param p_FrameIndex The index of the current frame.
     */
    void GrowToFit(u32 p_FrameIndex) noexcept;

    /**
     * @brief Send all host data to the device through storage, vertex or index buffers.
     *
     */
    void SendToDevice(u32 p_FrameIndex) noexcept;

    /// Renderer without stencil write, performs fill operation.
    Renderer<D, PipelineMode::NoStencilWriteDoFill> NoStencilWriteDoFill;

    /// Renderer with stencil write, performs fill operation.
    Renderer<D, PipelineMode::DoStencilWriteDoFill> DoStencilWriteDoFill;

    /// Renderer with stencil write, does not perform fill operation.
    Renderer<D, PipelineMode::DoStencilWriteNoFill> DoStencilWriteNoFill;

    /// Renderer with stencil test, does not perform fill operation.
    Renderer<D, PipelineMode::DoStencilTestNoFill> DoStencilTestNoFill;
};

using DrawFlags = u8;
/**
 * @brief Enumeration of flags used to control drawing behavior.
 *
 * These flags determine how shapes are rendered, particularly regarding stencil operations and scaling.
 */
enum DrawFlagBit : DrawFlags
{
    /// Do not write to stencil buffer, perform fill operation.
    DrawFlags_NoStencilWriteDoFill = 1 << 0,

    /// Write to stencil buffer, perform fill operation.
    DrawFlags_DoStencilWriteDoFill = 1 << 1,

    /// Write to stencil buffer, do not perform fill operation.
    DrawFlags_DoStencilWriteNoFill = 1 << 2,

    /// Test stencil buffer, do not perform fill operation.
    DrawFlags_DoStencilTestNoFill = 1 << 3,
};

/**
 * @brief The IRenderer class is a base class for renderers in the Onyx API.
 *
 * This template class provides common functionality for 2D and 3D renderers, handling draw calls and managing render
 * systems.
 *
 * @tparam D The dimensionality (`D2`or `D3`).
 */
template <Dimension D> class IRenderer
{
    TKIT_NON_COPYABLE(IRenderer)

  public:
    IRenderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;

    /**
     * @brief Record a draw call for a mesh model.
     *
     * @param p_State The render state used when drawing.
     * @param p_Transform The transformation matrix to apply to the mesh.
     * @param p_Mesh The mesh model to draw.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawMesh(const RenderState<D> &p_State, const fmat4 &p_Transform, const Mesh<D> &p_Mesh,
                  DrawFlags p_Flags) noexcept;

    /**
     * @brief Record a draw call for a primitive shape.
     *
     * @param p_State The render state used when drawing.
     * @param p_Transform The transformation matrix to apply to the primitive.
     * @param p_PrimitiveIndex Index of the primitive shape to draw.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawPrimitive(const RenderState<D> &p_State, const fmat4 &p_Transform, u32 p_PrimitiveIndex,
                       DrawFlags p_Flags) noexcept;

    /**
     * @brief Record a draw call for a polygon defined by a set of vertices.
     *
     * @param p_State The render state used when drawing.
     * @param p_Transform The transformation matrix to apply to the polygon.
     * @param p_Vertices Span of vertices defining the polygon.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawPolygon(const RenderState<D> &p_State, const fmat4 &p_Transform, TKit::Span<const fvec2> p_Vertices,
                     DrawFlags p_Flags) noexcept;

    /**
     * @brief Record a draw call for a circle or arc.
     *
     * @param p_State The render state used when drawing.
     * @param p_Transform The transformation matrix to apply to the circle.
     *
     * The following is encoded in the `CircleOptions` struct:
     *
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     *
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawCircle(const RenderState<D> &p_State, const fmat4 &p_Transform, const CircleOptions &p_Options,
                    DrawFlags p_Flags) noexcept;

  protected:
    RenderSystem<D, MeshRenderer> m_MeshRenderer;
    RenderSystem<D, PrimitiveRenderer> m_PrimitiveRenderer;
    RenderSystem<D, PolygonRenderer> m_PolygonRenderer;
    RenderSystem<D, CircleRenderer> m_CircleRenderer;

  private:
    /**
     * @brief Internal template method to handle drawing with different renderers.
     *
     * @tparam Renderer The type of the renderer.
     * @tparam DrawArgs Variadic template for additional drawing arguments.
     * @param p_Renderer Reference to the renderer.
     * @param p_State The render state used when drawing.
     * @param p_Transform The transformation matrix to apply.
     * @param p_Arg An additional argument specific to the renderer.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    template <typename Renderer, typename DrawArg>
    void draw(Renderer &p_Renderer, const RenderState<D> &p_State, const fmat4 &p_Transform, DrawArg &&p_Arg,
              DrawFlags p_Flags) noexcept;
};

/**
 * @brief Forward declaration of the Renderer class template.
 *
 * Specialized implementations are provided for 2D and 3D rendering.
 *
 * @tparam D The dimensionality (`D2`or `D3`).
 */
template <Dimension D> class Renderer;

/**
 * @brief The Renderer class specialized for 2D rendering.
 *
 * Handles the rendering of 2D shapes and manages the rendering pipeline.
 */
template <> class ONYX_API Renderer<D2> final : public IRenderer<D2>
{
  public:
    using IRenderer<D2>::IRenderer;

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
     * @brief Record all stored draw calls into the command buffer for execution.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The Vulkan command buffer to record commands into.
     * @param p_Cameras The cameras from which to render the scene.
     */
    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer, TKit::Span<const CameraInfo> p_Cameras) noexcept;

    /**
     * @brief Clear all host data and resets the renderer's state.
     *
     * Should be called to prepare the renderer for a new frame.
     */
    void Flush() noexcept;
};
} // namespace Onyx::Detail

namespace Onyx
{
/**
 * @brief Structure representing a directional light in the scene.
 *
 * Contains the light's direction, intensity, and color.
 */
struct ONYX_API DirectionalLight
{
    /// Direction of the light and its intensity (w component).
    fvec4 DirectionAndIntensity;

    /// Color of the light.
    Color Color;
};

/**
 * @brief Structure representing a point light in the scene.
 *
 * Contains the light's position, intensity, color, and radius of influence.
 */
struct ONYX_API PointLight
{
    /// Position of the light and its intensity (w component).
    fvec4 PositionAndIntensity;

    /// Color of the light.
    Color Color;

    /// Radius within which the light has an effect.
    f32 Radius;
};
} // namespace Onyx

namespace Onyx::Detail
{
/**
 * @brief Manages device-side storage and descriptors for lighting data.
 *
 * Contains storage buffers for directional and point lights, and their associated descriptor sets.
 */
struct ONYX_API DeviceLightData
{
    TKIT_NON_COPYABLE(DeviceLightData)

    DeviceLightData() noexcept;
    ~DeviceLightData() noexcept;

    template <typename T> void Grow(u32 p_FrameIndex, u32 p_Instances) noexcept;

    PerFrameData<HostVisibleStorageBuffer<DirectionalLight>> DirectionalLightBuffers;
    PerFrameData<HostVisibleStorageBuffer<PointLight>> PointLightBuffers;
    PerFrameData<VkDescriptorSet> DescriptorSets;
};

struct ONYX_API HostLightData
{
    HostStorageBuffer<DirectionalLight> DirectionalLights;
    HostStorageBuffer<PointLight> PointLights;
};

/**
 * @brief The Renderer class specialized for 3D rendering.
 *
 * Handles the rendering of 3D shapes, manages lighting, and controls the rendering pipeline.
 */
template <> class ONYX_API Renderer<D3> final : public IRenderer<D3>
{
  public:
    Renderer(const VkPipelineRenderingCreateInfoKHR &p_RenderInfo) noexcept;

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
     * @brief Record all stored draw calls into the command buffer for execution.
     *
     * @param p_FrameIndex The index of the current frame.
     * @param p_CommandBuffer The Vulkan command buffer to record commands into.
     * @param p_Cameras The cameras from which to render the scene.
     */
    void Render(u32 p_FrameIndex, VkCommandBuffer p_CommandBuffer, TKit::Span<const CameraInfo> p_Cameras) noexcept;

    /**
     * @brief Add a directional light to the scene.
     *
     * @param p_Light The directional light to add.
     */
    void AddDirectionalLight(const DirectionalLight &p_Light) noexcept;

    /**
     * @brief Add a point light to the scene.
     *
     * @param p_Light The point light to add.
     */
    void AddPointLight(const PointLight &p_Light) noexcept;

    /**
     * @brief Clear all host data, lights, and resets the renderer's state.
     *
     * Should be called to prepare the renderer for a new frame.
     */
    void Flush() noexcept;

    Color AmbientColor = Color{Color::WHITE, 0.4f};

  private:
    HostLightData m_HostLightData{};
    DeviceLightData m_DeviceLightData{};
};
} // namespace Onyx::Detail

#pragma once

#include "onyx/rendering/render_systems.hpp"
#include "vkit/descriptors/descriptor_set_layout.hpp"
#include "vkit/descriptors/descriptor_pool.hpp"

#ifndef ONYX_MAX_DIRECTIONAL_LIGHTS
#    define ONYX_MAX_DIRECTIONAL_LIGHTS 8
#endif

#ifndef ONYX_MAX_POINT_LIGHTS
#    define ONYX_MAX_POINT_LIGHTS 64
#endif

namespace Onyx::Detail
{
/**
 * @brief The RenderSystem struct manages multiple renderers with different pipeline modes.
 *
 * This template struct holds instances of a renderer R with different PipelineModes,
 * allowing for different rendering passes such as filling and outlining shapes.
 *
 * @tparam D The dimensionality (2D or 3D).
 * @tparam R The renderer type template (e.g., MeshRenderer, PrimitiveRenderer).
 */
template <Dimension D, template <Dimension, PipelineMode> typename R> struct RenderSystem
{
    /**
     * @brief Construct a RenderSystem with the specified render pass.
     *
     * @param p_RenderPass The Vulkan render pass to be used by the renderers.
     */
    RenderSystem(VkRenderPass p_RenderPass) noexcept;

    /**
     * @brief Clear all stored draw calls in each renderer.
     *
     * This method should be called to reset the renderers' state, typically at the beginning or end of a frame.
     */
    void Flush() noexcept;

    /// Renderer without stencil write, performs fill operation.
    R<D, PipelineMode::NoStencilWriteDoFill> NoStencilWriteDoFill;

    /// Renderer with stencil write, performs fill operation.
    R<D, PipelineMode::DoStencilWriteDoFill> DoStencilWriteDoFill;

    /// Renderer with stencil write, does not perform fill operation.
    R<D, PipelineMode::DoStencilWriteNoFill> DoStencilWriteNoFill;

    /// Renderer with stencil test, does not perform fill operation.
    R<D, PipelineMode::DoStencilTestNoFill> DoStencilTestNoFill;
};

/**
 * @brief Enumeration of flags used to control drawing behavior.
 *
 * These flags determine how shapes are rendered, particularly regarding stencil operations and scaling.
 */
enum DrawFlags : u8
{
    /// Automatic selection of draw flags based on the current state.
    DrawFlags_Auto = 1 << 0,

    /// Do not write to stencil buffer, perform fill operation.
    DrawFlags_NoStencilWriteDoFill = 1 << 1,

    /// Write to stencil buffer, perform fill operation.
    DrawFlags_DoStencilWriteDoFill = 1 << 2,

    /// Write to stencil buffer, do not perform fill operation.
    DrawFlags_DoStencilWriteNoFill = 1 << 3,

    /// Test stencil buffer, do not perform fill operation.
    DrawFlags_DoStencilTestNoFill = 1 << 4,

    /// Apply scaling during stencil operations.
    DrawFlags_DoStencilScale = 1 << 5,
};

/**
 * @brief The IRenderer class is an abstract base class for renderers in the Onyx API.
 *
 * This template class provides common functionality for 2D and 3D renderers, handling draw calls and managing render
 * systems.
 *
 * @tparam D The dimensionality (2D or 3D).
 */
template <Dimension D> class ONYX_API IRenderer
{
    TKIT_NON_COPYABLE(IRenderer)

  public:
    /**
     * @brief Construct an IRenderer with the specified render pass and render state.
     *
     * @param p_RenderPass The Vulkan render pass to be used.
     * @param p_State Pointer to the current render state stack.
     */
    IRenderer(VkRenderPass p_RenderPass, const TKit::StaticArray8<RenderState<D>> *p_State,
              const ProjectionViewData<D> *p_ProjectionView) noexcept; // Passing the state like this is a bit dodgy

    /**
     * @brief Record a draw call for a mesh model.
     *
     * @param p_Transform The transformation matrix to apply to the mesh.
     * @param p_Model The mesh model to draw.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawMesh(const fmat<D> &p_Transform, const Model<D> &p_Model, u8 p_Flags = DrawFlags_Auto) noexcept;

    /**
     * @brief Record a draw call for a primitive shape.
     *
     * @param p_Transform The transformation matrix to apply to the primitive.
     * @param p_PrimitiveIndex Index of the primitive shape to draw.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawPrimitive(const fmat<D> &p_Transform, u32 p_PrimitiveIndex, u8 p_Flags = DrawFlags_Auto) noexcept;

    /**
     * @brief Record a draw call for a polygon defined by a set of vertices.
     *
     * @param p_Transform The transformation matrix to apply to the polygon.
     * @param p_Vertices Span of vertices defining the polygon.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawPolygon(const fmat<D> &p_Transform, std::span<const fvec<D>> p_Vertices,
                     u8 p_Flags = DrawFlags_Auto) noexcept;

    /**
     * @brief Record a draw call for a circle or arc.
     *
     * @param p_Transform The transformation matrix to apply to the circle.
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     * @param p_Flags Drawing flags to control rendering behavior.
     */
    void DrawCircleOrArc(const fmat<D> &p_Transform, f32 p_InnerFade = 0.f, f32 p_OuterFade = 0.f,
                         f32 p_Hollowness = 0.f, f32 p_LowerAngle = 0.f, f32 p_UpperAngle = glm::two_pi<f32>(),
                         u8 p_Flags = DrawFlags_Auto) noexcept;

    /**
     * @brief Record a draw call for a circle or arc with flags specified before angles.
     *
     * @param p_Transform The transformation matrix to apply to the circle.
     * @param p_Flags Drawing flags to control rendering behavior.
     * @param p_InnerFade A value between 0 and 1 indicating how much the circle fades from the center to the edge.
     * @param p_OuterFade A value between 0 and 1 indicating how much the circle fades from the edge to the center.
     * @param p_LowerAngle Starting angle of the arc in radians.
     * @param p_UpperAngle Ending angle of the arc in radians.
     * @param p_Hollowness A value between 0 and 1 indicating how hollow the circle is.
     */
    void DrawCircleOrArc(const fmat<D> &p_Transform, u8 p_Flags, f32 p_InnerFade = 0.f, f32 p_OuterFade = 0.f,
                         f32 p_Hollowness = 0.f, f32 p_LowerAngle = 0.f,
                         f32 p_UpperAngle = glm::two_pi<f32>()) noexcept;

  protected:
    /// Current frame index for synchronization purposes.
    u32 m_FrameIndex = 0;

    /// Z-offset used in 2D rendering to manage draw order; only used in 2D.
    u32 m_ZOffset = 0;

    /// Number of draw calls recorded in the current frame.
    u32 m_DrawCount = 0;

    /// Render system for mesh models.
    RenderSystem<D, MeshRenderer> m_MeshRenderer;

    /// Render system for primitive shapes.
    RenderSystem<D, PrimitiveRenderer> m_PrimitiveRenderer;

    /// Render system for polygons.
    RenderSystem<D, PolygonRenderer> m_PolygonRenderer;

    /// Render system for circles and arcs.
    RenderSystem<D, CircleRenderer> m_CircleRenderer;

    /// Pointer to the global projection view data.
    const ProjectionViewData<D> *m_ProjectionView;

  private:
    /**
     * @brief Internal template method to handle drawing with different renderers.
     *
     * @tparam Renderer The type of the renderer.
     * @tparam DrawArgs Variadic template for additional drawing arguments.
     * @param p_Renderer Reference to the renderer.
     * @param p_Transform The transformation matrix to apply.
     * @param p_Flags Drawing flags to control rendering behavior.
     * @param p_Args Additional arguments specific to the renderer.
     */
    template <typename Renderer, typename... DrawArgs>
    void draw(Renderer &p_Renderer, const fmat<D> &p_Transform, u8 p_Flags, DrawArgs &&...p_Args) noexcept;

    /// Pointer to the current render state stack.
    const TKit::StaticArray8<RenderState<D>> *m_State;
};

/**
 * @brief Forward declaration of the Renderer class template.
 *
 * Specialized implementations are provided for 2D and 3D rendering.
 *
 * @tparam D The dimensionality (2D or 3D).
 */
template <Dimension D> class Renderer;

/**
 * @brief The Renderer class specialized for 2D rendering.
 *
 * Handles the rendering of 2D shapes and manages the rendering pipeline.
 */
template <> class Renderer<D2> final : public IRenderer<D2>
{
  public:
    using IRenderer<D2>::IRenderer;

    /**
     * @brief Record all stored draw calls into the command buffer for execution.
     *
     * @param p_CommandBuffer The Vulkan command buffer to record commands into.
     */
    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

    /**
     * @brief Clear all stored draw calls and resets the renderer's state.
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

    /**
     * @brief Construct DeviceLightData with a specified initial capacity.
     *
     * @param p_Capacity The initial capacity for the light buffers.
     */
    DeviceLightData(u32 p_Capacity) noexcept;

    /**
     * @brief Destroy the DeviceLightData, releasing resources.
     */
    ~DeviceLightData() noexcept;

    /// Storage buffers for directional lights, one per frame in flight.
    PerFrameData<MutableStorageBuffer<DirectionalLight>> DirectionalLightBuffers;

    /// Storage buffers for point lights, one per frame in flight.
    PerFrameData<MutableStorageBuffer<PointLight>> PointLightBuffers;

    /// Descriptor sets associated with the light buffers, one per frame in flight.
    PerFrameData<VkDescriptorSet> DescriptorSets;
};

/**
 * @brief The Renderer class specialized for 3D rendering.
 *
 * Handles the rendering of 3D shapes, manages lighting, and controls the rendering pipeline.
 */
template <> class Renderer<D3> final : public IRenderer<D3>
{
  public:
    /**
     * @brief Construct a Renderer for 3D rendering with the specified render pass and render state.
     *
     * @param p_RenderPass The Vulkan render pass to be used.
     * @param p_State Pointer to the current render state stack.
     */
    Renderer(VkRenderPass p_RenderPass, const TKit::StaticArray8<RenderState<D3>> *p_State,
             const ProjectionViewData<D3> *p_ProjectionView) noexcept;

    /**
     * @brief Record all stored draw calls into the command buffer for execution.
     *
     * @param p_CommandBuffer The Vulkan command buffer to record commands into.
     */
    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

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
     * @brief Clear all stored draw calls, lights, and resets the renderer's state.
     *
     * Should be called to prepare the renderer for a new frame.
     */
    void Flush() noexcept;

    /// Ambient light color and intensity in the scene.
    Color AmbientColor = Color{Color::WHITE, 0.4f};

  private:
    /// Collection of directional lights in the scene.
    TKit::StaticArray<DirectionalLight, ONYX_MAX_DIRECTIONAL_LIGHTS> m_DirectionalLights;

    /// Collection of point lights in the scene.
    TKit::StaticArray<PointLight, ONYX_MAX_POINT_LIGHTS> m_PointLights;

    /// Device-side storage and descriptors for light data.
    DeviceLightData m_DeviceLightData{ONYX_BUFFER_INITIAL_CAPACITY};
};
} // namespace Onyx::Detail

#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/buffer/storage_buffer.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/draw/model.hpp"
#include <vulkan/vulkan.hpp>

namespace ONYX
{

// VERY CLUNKY: 3 out of 4 possible instantiations of MaterialData and RenderInfo are identical

/**
 * @brief The MaterialData struct is a simple collection of data that represents the material of a shape.
 *
 * The material is a simple color in 2D, and a color with additional properties in 3D, mostly used for lighting. The
 * simple 2D material data is also used for stencil passes in 3D.
 *
 * @tparam D The dimension (D2 or D3).
 */
template <Dimension D> struct MaterialData;

template <> struct MaterialData<D2>
{
    Color Color = ONYX::Color::WHITE;
};

template <> struct MaterialData<D3>
{
    Color Color = ONYX::Color::WHITE;
    f32 DiffuseContribution = 0.8f;
    f32 SpecularContribution = 0.2f;
    f32 SpecularSharpness = 32.f;
};

/**
 * @brief The PipelineMode enum represents a grouping of pipelines with slightly different settings that all renderers
 * use.
 *
 * To support nice outlines, especially in 3D, the stencil buffer can be used to re-render the same shape
 * slightly scaled only in places where the stencil buffer has not been set. Generally, only two passes would be
 * necessary, but in this implementation four are used.
 *
 * - NoStencilWriteDoFill: This pass will render the shape normally and corresponds to a shape being rendered without
 * an outline, thus not writing to the stencil buffer. This is important so that other shapes having outlines can have
 * theirs drawn on top of objects that do not have an outline. This way, an object's outline will always be visible and
 * on top of non-outlined shapes. The corresponding DrawMode is Fill.
 *
 * - DoStencilWriteDoFill: This pass will render the shape normally and write to the stencil buffer, which corresponds
 * to a shape being rendered both filled and with an outline. The corresponding DrawMode is Fill.
 *
 * - DoStencilWriteNoFill: This pass will only write to the stencil buffer and will not render the shape. This step is
 * necessary in case the user wants to render an outline only, without the shape being filled. The corresponding
 * DrawMode is Stencil.
 *
 * - DoStencilTestNoFill: This pass will test the stencil buffer and render the shape only where the stencil buffer is
 * not set. The corresponding DrawMode is Stencil.
 *
 */
enum class ONYX_API PipelineMode : u8
{
    NoStencilWriteDoFill,
    DoStencilWriteDoFill,
    DoStencilWriteNoFill,
    DoStencilTestNoFill
};

/**
 * @brief The DrawMode is related to the data each PipelineMode needs to render correctly.
 *
 * To render a filled shape in, say, 3D, the renderer must know information about the lights in the environment, have
 * access to normals, etc. When writing/testing to the stencil buffer, however, the renderer only needs the shape's
 * geometry and an outline color.
 *
 * The first two modes are used for rendering filled shapes (DrawMode::Fill), and the last two are used for rendering
 * outlines (DrawMode::Stencil).
 *
 */
enum class ONYX_API DrawMode : u8
{
    Fill,
    Stencil
};

/**
 * @brief Get the DrawMode corresponding to a PipelineMode.
 *
 * @tparam PMode The pipeline mode.
 * @return The corresponding DrawMode.
 */
template <PipelineMode PMode> constexpr DrawMode GetDrawMode() noexcept
{
    if constexpr (PMode == PipelineMode::NoStencilWriteDoFill || PMode == PipelineMode::DoStencilWriteDoFill)
        return DrawMode::Fill;
    else
        return DrawMode::Stencil;
}

/**
 * @brief The RenderInfo is a small struct containing information the renderers need to draw their shapes.
 *
 * It contains the current command buffer, the current frame index, different descriptor sets to bind to (storage
 * buffers containing light information in the 3D case, for example), and some other global information.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam DMode The draw mode (Fill or Stencil).
 */
template <Dimension D, DrawMode DMode> struct RenderInfo;

template <DrawMode DMode> struct ONYX_API RenderInfo<D2, DMode>
{
    VkCommandBuffer CommandBuffer;
    u32 FrameIndex;
};

template <> struct ONYX_API RenderInfo<D3, DrawMode::Fill>
{
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet LightStorageBuffers;
    u32 FrameIndex;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    vec4 AmbientColor;
};

template <> struct ONYX_API RenderInfo<D3, DrawMode::Stencil>
{
    VkCommandBuffer CommandBuffer;
    u32 FrameIndex;
};

/**
 * @brief The InstanceData struct is the collection of all the data needed to render a shape.
 *
 * It is stored and sent to the GPU in a storage buffer, and the renderer will use this data to render the shape.
 * The InstanceData varies between dimensions and draw modes, as the data needed to render a 2D shape is different from
 * the data needed to render a 3D shape, and the data needed to render a filled shape is different from the data needed
 * to render an outline.
 *
 * The most notable data this struct contains is the transform matrix, responsible for positioning, rotating, and
 * scaling the shape; the material data, which contains the color of the shape and some other properties; and the view
 * matrix, which in this library is used as the transform of the coordinate system.
 *
 * The view (or axes) matrix is still stored per instance because of the immediate mode. This way, the user can change
 * the view matrix between shapes, and the renderer will use the correct one.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam DMode The draw mode (Fill or Stencil).
 */
template <Dimension D, DrawMode DMode> struct InstanceData
{
    mat4 Transform;
    MaterialData<D> Material;
};

// Could actually save some space by using smaller matrices in the 2D case and removing the last row, as it always is 0
// 0 1 but I don't want to deal with the alignment management, to be honest.

template <> struct ONYX_API InstanceData<D3, DrawMode::Fill>
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView; // The projection view may vary between shapes
    vec4 ViewPosition;
    MaterialData<D3> Material;
};

/**
 * @brief The DeviceInstanceData is a convenience struct that helps organize the data that is sent to the GPU so that
 * each frame contains a dedicated set of storage buffers and descriptors.
 *
 * @tparam T The type of the data that is sent to the GPU.
 */
template <typename T> struct ONYX_API DeviceInstanceData
{
    KIT_NON_COPYABLE(DeviceInstanceData)
    DeviceInstanceData(usize p_Capacity) noexcept
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
        {
            StorageBuffers[i].Create(p_Capacity);
            StorageSizes[i] = 0;
        }
    }
    ~DeviceInstanceData() noexcept
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
            StorageBuffers[i].Destroy();
    }

    std::array<KIT::Storage<StorageBuffer<T>>, SwapChain::MFIF> StorageBuffers;
    std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
    std::array<usize, SwapChain::MFIF> StorageSizes;
};

/**
 * @brief An extension of the DeviceInstanceData for polygons.
 *
 * This struct contains additional mutable vertex and index buffers that are used to store the geometry of arbitrary
 * polygons.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam DMode The draw mode (Fill or Stencil).
 */
template <Dimension D, DrawMode DMode> struct PolygonDeviceInstanceData : DeviceInstanceData<InstanceData<D, DMode>>
{
    PolygonDeviceInstanceData(const usize p_Capacity) noexcept : DeviceInstanceData<InstanceData<D, DMode>>(p_Capacity)
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
        {
            VertexBuffers[i].Create(p_Capacity);
            IndexBuffers[i].Create(p_Capacity);
        }
    }
    ~PolygonDeviceInstanceData() noexcept
    {
        for (usize i = 0; i < SwapChain::MFIF; ++i)
        {
            VertexBuffers[i].Destroy();
            IndexBuffers[i].Destroy();
        }
    }

    std::array<KIT::Storage<MutableVertexBuffer<D>>, SwapChain::MFIF> VertexBuffers;
    std::array<KIT::Storage<MutableIndexBuffer>, SwapChain::MFIF> IndexBuffers;
};

/**
 * @brief Specific InstanceData for polygons.
 *
 * The Layout field is actually not sent to the GPU. It is used on the CPU side to know which parts of the index and
 * vertex buffers to use when issuing Vulkan draw commands.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam DMode The draw mode (Fill or Stencil).
 */
template <Dimension D, DrawMode DMode> struct PolygonInstanceData
{
    InstanceData<D, DMode> BaseData;
    PrimitiveDataLayout Layout;
};

KIT_WARNING_IGNORE_PUSH
KIT_MSVC_WARNING_IGNORE(4324)

/**
 * @brief Specific InstanceData for circles.
 *
 * The additional data is used in the fragment shaders to discard fragments that are outside the circle or the
 * user-defined arc.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam DMode The draw mode (Fill or Stencil).
 */
template <Dimension D, DrawMode DMode> struct CircleInstanceData
{
    InstanceData<D, DMode> BaseData;
    alignas(16) vec4 ArcInfo;
    u32 AngleOverflow;
    f32 Hollowness;
};
KIT_WARNING_IGNORE_POP

/**
 * @brief Some push constant data that is used in the 3D case, containing the ambient color and the number of lights.
 *
 */
struct PushConstantData3D
{
    vec4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

/**
 * @brief Create the pipeline specifications for a meshed shape.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam PMode The pipeline mode.
 * @param p_RenderPass The render pass to use.
 * @return The pipeline specifications.
 */
template <Dimension D, PipelineMode PMode>
ONYX_API Pipeline::Specs CreateMeshedPipelineSpecs(VkRenderPass p_RenderPass) noexcept;

/**
 * @brief Create the pipeline specifications for a circle shape.
 *
 * @tparam D The dimension (D2 or D3).
 * @tparam PMode The pipeline mode.
 * @param p_RenderPass The render pass to use.
 * @return The pipeline specifications.
 */
template <Dimension D, PipelineMode PMode>
ONYX_API Pipeline::Specs CreateCirclePipelineSpecs(VkRenderPass p_RenderPass) noexcept;

/**
 * @brief Modify the axes transform (which corresponds to a view matrix, but with
 * scaling included as well) to comply with a specific coordinate system.
 *
 * The current coordinate system used by this library is right-handed, with the center of the screen being at the
 * middle. The X-axis points to the right, the Y-axis points upwards, and the Z-axis points out of the screen.
 *
 * @param p_Axes The axes to modify.
 * @param p_InverseAxes The inverse axes to modify. If nullptr, the inverse axes will not be modified.
 */
ONYX_API void ApplyCoordinateSystem(mat4 &p_Axes, mat4 *p_InverseAxes = nullptr) noexcept;

/**
 * @brief The RenderState struct is used by the RenderContext class to track the current object and axes
 * transformations, the current material, outline color and width, and some other rendering settings.
 *
 * It holds all of the state that RenderContext's immediate mode API needs and allows it to easily push/pop states to
 * quickly modify and restore the rendering state.
 *
 * @tparam D The dimension (D2 or D3).
 */
template <Dimension D> struct RenderState;

template <> struct RenderState<D2>
{
    mat3 Transform{1.f};
    mat3 Axes{1.f};
    Color OutlineColor = Color::WHITE;
    f32 OutlineWidth = 0.f;
    MaterialData<D2> Material{};
    bool Fill = true;
    bool Outline = false;
};

template <> struct RenderState<D3>
{
    mat4 Transform{1.f};
    mat4 Axes{1.f};
    mat4 InverseAxes{1.f}; // Just for caching
    mat4 Projection{1.f};
    Color LightColor = Color::WHITE;
    Color OutlineColor = Color::WHITE;
    f32 OutlineWidth = 0.f;
    MaterialData<D3> Material{};
    bool Fill = true;
    bool Outline = false;
    bool HasProjection = false;
};

} // namespace ONYX
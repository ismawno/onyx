#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/draw/primitives.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/draw/transform.hpp"
#include "onyx/core/core.hpp"
#include "vkit/pipeline/graphics_pipeline.hpp"
#include <vulkan/vulkan.h>

namespace Onyx
{
/**
 * @brief Modify the transform to comply with a specific coordinate system extrinsically.
 *
 * The current coordinate system used by this library is right-handed, with the center of the screen being at the
 * middle. The X-axis points to the right, the Y-axis points upwards, and the Z-axis points out of the screen.
 *
 * @param p_Transform The transform to modify.
 */
ONYX_API void ApplyCoordinateSystemExtrinsic(fmat4 &p_Transform) noexcept;

/**
 * @brief Modify the transform to comply with a specific coordinate system intrinsically.
 *
 * The current coordinate system used by this library is right-handed, with the center of the screen being at the
 * middle. The X-axis points to the right, the Y-axis points upwards, and the Z-axis points out of the screen.
 *
 * This version of the function is used to apply such coordinate system to the corresponding inverse transform.
 *
 * @param p_Transform The transform to modify.
 */
ONYX_API void ApplyCoordinateSystemIntrinsic(fmat4 &p_Transform) noexcept;

/**
 * @brief Promote a 2D transform to an equivalent 3D transform.
 *
 * This function takes a 2D transform and promotes it to a 3D transform by adding a Z-axis component set to the
 * identity.
 *
 * @param p_Transform The 2D transform to promote.
 * @return The promoted 3D transform.
 */
ONYX_API fmat4 PromoteTransform(const fmat3 &p_Transform) noexcept;

/**
 * @brief The `MaterialData` struct is a simple collection of data that represents the material of a shape.
 *
 * The material is a simple color in 2D, and a color with additional properties in 3D, mostly used for lighting. The
 * simple 2D material data is also used for stencil passes in 3D (`DrawLevel::Simple`).
 *
 * @tparam D The dimension (`D2` or `D3`).
 */
template <Dimension D> struct MaterialData;

template <> struct ONYX_API MaterialData<D2>
{
    Color Color = Onyx::Color::WHITE;
};

template <> struct ONYX_API MaterialData<D3>
{
    Color Color = Onyx::Color::WHITE;
    f32 DiffuseContribution = 0.8f;
    f32 SpecularContribution = 0.2f;
    f32 SpecularSharpness = 32.f;
};

/**
 * @brief The `RenderState` struct is used by the `RenderContext` class to track the current object and axes
 * transformations, the current material, outline color and width, and some other rendering settings.
 *
 * It holds all of the state that `RenderContext`'s immediate mode API needs and allows it to easily push/pop states to
 * quickly modify and restore the rendering state.
 *
 * @tparam D The dimension (`D2` or `D3`).
 */
template <Dimension D> struct RenderState;

template <> struct ONYX_API RenderState<D2>
{
    fmat3 Transform{1.f};
    fmat3 Axes{1.f};
    Color OutlineColor = Color::WHITE;
    MaterialData<D2> Material{};
    f32 OutlineWidth = 0.1f;
    bool Fill = true;
    bool Outline = false;
};

template <> struct ONYX_API RenderState<D3>
{
    fmat4 Transform{1.f};
    fmat4 Axes{1.f};
    Color OutlineColor = Color::WHITE;
    Color LightColor = Color::WHITE;
    MaterialData<D3> Material{};
    f32 OutlineWidth = 0.1f;
    bool Fill = true;
    bool Outline = false;
};

/**
 * @brief The `ProjectionViewData` struct is a simple struct that holds the view and projection matrices.
 *
 * 2D shapes only need a view matrix, as the projection matrix is always an orthographic projection matrix. The view can
 * also include scaling.
 *
 * In 2D, the projection view matrix is the "raw" inverse of the view's transform. Then, just before sending the data to
 * the gpu as a `fmat4`, the renderer applies the extrinsic coordinate system.
 *
 * In 3D, the projection view matrix is the projection matrix multiplied by the view matrix. As the view matrix is
 * already a `fmat4`, the renderer can directly apply the extrinsic coordinate system.
 *
 * @tparam D The dimension (`D2` or `D3`).
 */
template <Dimension D> struct ProjectionViewData;

template <> struct ONYX_API ProjectionViewData<D2>
{
    Transform<D2> View{};
    fmat3 ProjectionView{1.f};
};
template <> struct ONYX_API ProjectionViewData<D3>
{
    Transform<D3> View{};
    fmat4 Projection{1.f};
    fmat4 ProjectionView{1.f};
};

struct CircleOptions
{
    f32 InnerFade = 0.f;
    f32 OuterFade = 0.f;
    f32 Hollowness = 0.f;
    f32 LowerAngle = 0.f;
    f32 UpperAngle = glm::two_pi<f32>();
};
} // namespace Onyx

namespace Onyx::Detail
{
/**
 * @brief The `PipelineMode` enum represents a grouping of pipelines with slightly different settings that all renderers
 * use.
 *
 * To support nice outlines, especially in 3D, the stencil buffer can be used to re-render the same shape
 * slightly scaled only in places where the stencil buffer has not been set. Generally, only two passes would be
 * necessary, but in this implementation four are used.
 *
 * - `NoStencilWriteDoFill`: This pass will render the shape normally and corresponds to a shape being rendered without
 * an outline, thus not writing to the stencil buffer. This is important so that other shapes having outlines can have
 * theirs drawn on top of objects that do not have an outline. This way, an object's outline will always be visible and
 * on top of non-outlined shapes. The corresponding `DrawMode` is `Fill`.
 *
 * - `DoStencilWriteDoFill`: This pass will render the shape normally and write to the stencil buffer, which corresponds
 * to a shape being rendered both filled and with an outline. The corresponding `DrawMode` is `Fill`.
 *
 * - `DoStencilWriteNoFill`: This pass will only write to the stencil buffer and will not render the shape. This step is
 * necessary in case the user wants to render an outline only, without the shape being filled. The corresponding
 * `DrawMode` is `Stencil`.
 *
 * - `DoStencilTestNoFill`: This pass will test the stencil buffer and render the shape only where the stencil buffer is
 * not set. The corresponding `DrawMode` is St`encil`.
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
 * @brief The `DrawMode` is related to the data each `PipelineMode` needs to render correctly.
 *
 * To render a filled shape in, say, 3D, the renderer must know information about the lights in the environment, have
 * access to normals, etc. When writing/testing to the stencil buffer, however, the renderer only needs the shape's
 * geometry and an outline color.
 *
 * The first two modes are used for rendering filled shapes (`DrawMode::Fill`), and the last two are used for rendering
 * outlines (`DrawMode::Stencil`).
 *
 */
enum class ONYX_API DrawMode : u8
{
    Fill,
    Stencil
};

/**
 * @brief The `DrawLevel` combines resource requirements when requiring combinations of `Dimension` and `DrawMode`.
 *
 * Turns out that when rendering things in 2D, (either with `DrawMode::Fill` or `DrawMode::Stencil`), the resource
 * requirements are very similar when rendering things in 3D with `DrawMode::Stencil`. This means some resources (such
 * as pipeline layouts) can be shared between these two cases.
 *
 * In summary, `DrawLevel::Simple` is used for 2D rendering and 3D stencil rendering, while `DrawLevel::Complex` is used
 * exclusively for 3D filled rendering.
 *
 */
enum class ONYX_API DrawLevel : u8
{
    Simple,
    Complex
};

/**
 * @brief Get the `DrawMode` corresponding to a `PipelineMode`.
 *
 * @tparam PMode The pipeline mode.
 * @return The corresponding `DrawMode`.
 */
template <PipelineMode PMode> constexpr DrawMode GetDrawMode() noexcept
{
    if constexpr (PMode == PipelineMode::NoStencilWriteDoFill || PMode == PipelineMode::DoStencilWriteDoFill)
        return DrawMode::Fill;
    else
        return DrawMode::Stencil;
}

/**
 * @brief Get the `DrawLevel` corresponding to a `Dimension` and `DrawMode`.
 *
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam DMode The draw mode (`Fill` or `Stencil`).
 * @return The corresponding `DrawLevel`.
 */
template <Dimension D, DrawMode DMode> constexpr DrawLevel GetDrawLevel() noexcept
{
    if constexpr (D == D2 || DMode == DrawMode::Stencil)
        return DrawLevel::Simple;
    else
        return DrawLevel::Complex;
}

/**
 * @brief Get the `DrawLevel` corresponding to a `Dimension` and `PipelineMode`.
 *
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam PMode The pipeline mode.
 * @return The corresponding `DrawLevel`.
 */
template <Dimension D, PipelineMode PMode> constexpr DrawLevel GetDrawLevel() noexcept
{
    return GetDrawLevel<D, GetDrawMode<PMode>()>();
}

/**
 * @brief The `RenderInfo` is a small struct containing information the renderers need to draw their shapes.
 *
 * It contains the current command buffer, the current frame index, different descriptor sets to bind to (storage
 * buffers containing light information in the 3D case, for example), and some other global information.
 *
 * @tparam DLevel The draw level (`Simple` or `Complex`).
 */
template <DrawLevel DLevel> struct RenderInfo;

template <> struct ONYX_API RenderInfo<DrawLevel::Simple>
{
    VkCommandBuffer CommandBuffer;
    const fmat4 *ProjectionView;
    u32 FrameIndex;
};

template <> struct ONYX_API RenderInfo<DrawLevel::Complex>
{
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet LightStorageBuffers;
    const fmat4 *ProjectionView;
    const Color *AmbientColor;
    const fvec3 *ViewPosition;
    u32 FrameIndex;
    u32 DirectionalLightCount;
    u32 PointLightCount;
};

/**
 * @brief The `InstanceData` struct is the collection of all the data needed to render a shape.
 *
 * It is stored and sent to the GPU in a storage buffer, and the renderer will use this data to render the shape.
 * The `InstanceData` varies between dimensions and draw modes, as the data needed to render a 2D shape is different
 * from the data needed to render a 3D shape, and the data needed to render a filled shape is different from the data
 * needed to render an outline.
 *
 * The most notable data this struct contains is the transform matrix, responsible for positioning, rotating, and
 * scaling the shape; the material data, which contains the color of the shape and some other properties; and the view
 * matrix, which in this library is used as the transform of the coordinate system.
 *
 * The view (or axes) matrix is still stored per instance because of the immediate mode. This way, the user can change
 * the view matrix between shapes, and the renderer will use the correct one.
 *
 * @tparam DLevel The draw level (`Simple` or `Complex`).
 */
template <DrawLevel DLevel> struct InstanceData;

// Could actually save some space by using smaller matrices in the 2D case and removing the last row, as it always is 0
// 0 1 but I don't want to deal with the alignment management, to be honest.

template <> struct ONYX_API InstanceData<DrawLevel::Simple>
{
    fmat4 Transform;
    MaterialData<D2> Material;
};
template <> struct ONYX_API InstanceData<DrawLevel::Complex>
{
    fmat4 Transform;
    fmat4 NormalMatrix;
    MaterialData<D3> Material;
};

/**
 * @brief The `DeviceInstanceData` is a convenience struct that helps organize the data that is sent to the GPU so that
 * each frame contains a dedicated set of storage buffers and descriptors.
 *
 * @tparam T The type of the data that is sent to the GPU.
 */
template <typename T> struct DeviceInstanceData
{
    TKIT_NON_COPYABLE(DeviceInstanceData)
    DeviceInstanceData(u32 p_Capacity) noexcept
    {
        for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
        {
            StorageBuffers[i] = Core::CreateMutableStorageBuffer<T>(p_Capacity);
            StorageSizes[i] = 0;
        }
    }
    ~DeviceInstanceData() noexcept
    {
        for (u32 i = 0; i < ONYX_MAX_FRAMES_IN_FLIGHT; ++i)
            StorageBuffers[i].Destroy();
    }

    PerFrameData<MutableStorageBuffer<T>> StorageBuffers;
    PerFrameData<VkDescriptorSet> DescriptorSets;
    PerFrameData<u32> StorageSizes;
};

/**
 * @brief An extension of the `DeviceInstanceData` for polygons.
 *
 * This struct contains additional mutable vertex and index buffers that are used to store the geometry of arbitrary
 * polygons.
 *
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam DLevel The draw level (`Simple` or `Complex`).
 */
template <Dimension D, DrawLevel DLevel> struct PolygonDeviceInstanceData : DeviceInstanceData<InstanceData<DLevel>>
{
    PolygonDeviceInstanceData(const u32 p_Capacity) noexcept;
    ~PolygonDeviceInstanceData() noexcept;

    PerFrameData<MutableVertexBuffer<D>> VertexBuffers;
    PerFrameData<MutableIndexBuffer> IndexBuffers;
};

/**
 * @brief Specific `InstanceData` for polygons.
 *
 * The Layout field is actually not sent to the GPU. It is used on the CPU side to know which parts of the index and
 * vertex buffers to use when issuing Vulkan draw commands.
 *
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam DMode The draw mode (`Fill` or `Stencil`).
 */
template <DrawLevel DLevel> struct PolygonInstanceData
{
    InstanceData<DLevel> BaseData;
    PrimitiveDataLayout Layout;
};

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324)

/**
 * @brief Specific `InstanceData` for circles.
 *
 * The additional data is used in the fragment shaders to discard fragments that are outside the circle or the
 * user-defined arc.
 *
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam DMode The draw mode (`Fill` or `Stencil`).
 */
template <DrawLevel DLevel> struct CircleInstanceData
{
    InstanceData<DLevel> BaseData;
    alignas(16) fvec4 ArcInfo;
    u32 AngleOverflow;
    f32 Hollowness;
    f32 InnerFade;
    f32 OuterFade;
};
TKIT_COMPILER_WARNING_IGNORE_POP()

/**
 * @brief Some global push constant data that is used by the shaders.
 */
template <DrawLevel DLevel> struct PushConstantData;

template <> struct ONYX_API PushConstantData<DrawLevel::Simple>
{
    fmat4 ProjectionView;
};

template <> struct ONYX_API PushConstantData<DrawLevel::Complex>
{
    fmat4 ProjectionView;
    fvec4 ViewPosition;
    fvec4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

template <Dimension D, PipelineMode PMode> struct PipelineGenerator
{
    /**
     * @brief Create a pipeline for meshed shapes.
     *
     * @tparam D The dimension (`D2` or `D3`).
     * @tparam PMode The pipeline mode.
     * @param p_RenderPass The render pass to use.
     * @return The pipeline handle.
     */
    static VKit::GraphicsPipeline CreateMeshPipeline(VkRenderPass p_RenderPass) noexcept;

    /**
     * @brief Create a pipeline for circle shapes.
     *
     * @tparam D The dimension (`D2` or `D3`).
     * @tparam PMode The pipeline mode.
     * @param p_RenderPass The render pass to use.
     * @return The pipeline handle.
     */
    static VKit::GraphicsPipeline CreateCirclePipeline(VkRenderPass p_RenderPass) noexcept;
};

} // namespace Onyx::Detail

#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/color.hpp"
#include "onyx/core/limits.hpp"
#include "onyx/resource/buffer.hpp"
#include "onyx/asset/assets.hpp"
#include <vulkan/vulkan.h>

TKIT_COMPILER_WARNING_IGNORE_PUSH()
TKIT_MSVC_WARNING_IGNORE(4324)

namespace Onyx
{
template <Dimension D> struct MaterialData;

template <> struct MaterialData<D2>
{
    TKIT_REFLECT_DECLARE(MaterialData)
    TKIT_YAML_SERIALIZE_DECLARE(MaterialData)
    Color Color = Onyx::Color::WHITE;
};

template <> struct MaterialData<D3>
{
    TKIT_REFLECT_DECLARE(MaterialData)
    TKIT_YAML_SERIALIZE_DECLARE(MaterialData)
    Color Color = Onyx::Color::WHITE;
    f32 DiffuseContribution = 0.8f;
    f32 SpecularContribution = 0.2f;
    f32 SpecularSharpness = 32.f;
};

using RenderStateFlags = u8;
enum RenderStateFlagBit : RenderStateFlags
{
    RenderStateFlag_Fill = 1 << 0,
    RenderStateFlag_Outline = 1 << 1,
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

template <> struct RenderState<D2>
{
    TKIT_REFLECT_DECLARE(RenderState)
    TKIT_YAML_SERIALIZE_DECLARE(RenderState)
    f32m3 Transform = f32m3::Identity();
    Color OutlineColor = Color::WHITE;
    MaterialData<D2> Material{};
    f32 OutlineWidth = 0.1f;
    RenderStateFlags Flags = RenderStateFlag_Fill;
};

template <> struct RenderState<D3>
{
    TKIT_REFLECT_DECLARE(RenderState)
    TKIT_YAML_SERIALIZE_DECLARE(RenderState)
    f32m4 Transform = f32m4::Identity();
    Color OutlineColor = Color::WHITE;
    Color LightColor = Color::WHITE;
    MaterialData<D3> Material{};
    f32 OutlineWidth = 0.1f;
    RenderStateFlags Flags = RenderStateFlag_Fill;
};

/**
 * @brief The `PipelineMode` enum represents a grouping of pipelines with slightly different settings that all renderers
 * use.
 *
 * To support nice outlines, especially in 3D, the stencil buffer can be used to re-render the same shape
 * slightly scaled only in places where the stencil buffer has not been set. Generally, only two passes would be
 * necessary, but in this implementation four are used.
 *
 * - `Pipeline_NoStencilWriteDoFill`: This pass will render the shape normally and corresponds to a shape being rendered
 * without an outline, thus not writing to the stencil buffer. This is important so that other shapes having outlines
 * can have theirs drawn on top of objects that do not have an outline. This way, an object's outline will always be
 * visible and on top of non-outlined shapes. The corresponding `DrawMode` is `Draw_Fill`.
 *
 * - `Pipeline_DoStencilWriteDoFill`: This pass will render the shape normally and write to the stencil buffer, which
 * corresponds to a shape being rendered both filled and with an outline. The corresponding `DrawMode` is `Draw_Fill`.
 *
 * - `Pipeline_DoStencilWriteNoFill`: This pass will only write to the stencil buffer and will not render the shape.
 * This step is necessary in case the user wants to render an outline only, without the shape being filled. The
 * corresponding `DrawMode` is `Draw_Outline`.
 *
 * - `Pipeline_DoStencilTestNoFill`: This pass will test the stencil buffer and render the shape only where the stencil
 * buffer is not set. The corresponding `DrawMode` is `Draw_Outline`.
 *
 */
enum PipelineMode : u8
{
    Pipeline_NoStencilWriteDoFill,
    Pipeline_DoStencilWriteDoFill,
    Pipeline_DoStencilWriteNoFill,
    Pipeline_DoStencilTestNoFill
};

/**
 * @brief The `DrawMode` is related to the data each `PipelineMode` needs to render correctly.
 *
 * To render a filled shape in, say, 3D, the renderer must know information about the lights in the environment, have
 * access to normals, etc. When writing/testing to the stencil buffer, however, the renderer only needs the shape's
 * geometry and an outline color.
 *
 * The first two modes are used for rendering filled shapes (`Draw_Fill`), and the last two are used for rendering
 * outlines (`Draw_Outline`).
 *
 */
enum DrawMode : u8
{
    Draw_Fill,
    Draw_Outline
};

enum Shading : u8
{
    Shading_Unlit,
    Shading_Lit
};

constexpr DrawMode GetDrawMode(const PipelineMode p_Mode)
{
    if (p_Mode == Pipeline_NoStencilWriteDoFill || p_Mode == Pipeline_DoStencilWriteDoFill)
        return Draw_Fill;
    return Draw_Outline;
}

template <Dimension D> constexpr Shading GetShading(const DrawMode p_Mode)
{
    if constexpr (D == D2)
        return Shading_Unlit;
    else
        return p_Mode == Draw_Fill ? Shading_Lit : Shading_Unlit;
}
template <Dimension D> constexpr Shading GetShading(const PipelineMode p_Mode)
{
    return GetShading<D>(GetDrawMode(p_Mode));
}

} // namespace Onyx

namespace Onyx::Detail
{

struct CameraInfo
{
    f32m4 ProjectionView;
    Color BackgroundColor;
    f32v3 ViewPosition; // Unused in 2D... not ideal
    VkViewport Viewport;
    VkRect2D Scissor;
    bool Transparent;
};

struct LightData
{
    VkDescriptorSet DescriptorSet;
    const Color *AmbientColor;
    u32 DirectionalCount;
    u32 PointCount;
};

/**
 * @brief The `RenderInfo` is a small struct containing information the renderers need to draw their shapes.
 *
 * It contains the current command buffer, the current frame index, different descriptor sets to bind to (storage
 * buffers containing light information in the 3D case, for example), and some other global information.
 *
 * @tparam S The shading (`Shading_Unlit` or `Shading_Lit`).
 */
template <Shading S> struct RenderInfo;

template <> struct RenderInfo<Shading_Unlit>
{
    VkCommandBuffer CommandBuffer;
    const CameraInfo *Camera;
    u32 FrameIndex;
};

template <> struct RenderInfo<Shading_Lit>
{
    VkCommandBuffer CommandBuffer;
    const CameraInfo *Camera;
    const f32v3 *ViewPosition;
    u32 FrameIndex;
    LightData Light;
};

struct CopyInfo
{
    u32 FrameIndex;
    VkCommandBuffer CommandBuffer;
    TKit::Array16<VkBufferMemoryBarrier> *AcquireShaderBarriers;
    TKit::Array4<VkBufferMemoryBarrier> *AcquireVertexBarriers;
    TKit::Array32<VkBufferMemoryBarrier> *ReleaseBarriers;
};

/**
 * @brief The `InstanceData` struct is the collection of all the data needed to render a shape.
 *
 * It is stored and sent to the device in a storage buffer, and the renderer will use this data to render the shape.
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
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam DMode The draw mode (`Draw_Fill` or `Draw_Outline`).
 */
template <Dimension D, DrawMode DMode> struct InstanceData;

template <DrawMode DMode> struct InstanceData<D2, DMode>
{
    f32v2 Basis1;
    f32v2 Basis2;
    f32v2 Basis3;
    u32 Color;
};

template <> struct InstanceData<D3, Draw_Fill>
{
    f32v4 Basis1;
    f32v4 Basis2;
    f32v4 Basis3;
    u32 Color;
    f32 DiffuseContribution;
    f32 SpecularContribution;
    f32 SpecularSharpness;
};
template <> struct InstanceData<D3, Draw_Outline>
{
    f32v4 Basis1;
    f32v4 Basis2;
    f32v4 Basis3;
    u32 Color;
};

/**
 * @brief Specific `InstanceData` for circles.
 *
 * The additional data is used in the fragment shaders to discard fragments that are outside the circle or the
 * user-defined arc.
 *
 * @tparam D The dimension (`D2` or `D3`).
 * @tparam DMode The draw mode (`Draw_Fill` or `Draw_Outline`).
 */
template <Dimension D, DrawMode DMode> struct CircleInstanceData
{
    InstanceData<D, DMode> Base;

    f32 LowerCos;
    f32 LowerSin;
    f32 UpperCos;
    f32 UpperSin;

    u32 AngleOverflow;
    f32 Hollowness;
    f32 InnerFade;
    f32 OuterFade;
};

/**
 * @brief The `DeviceData` is a convenience struct that helps organize the data that is sent to the device so
 * that each frame contains a dedicated set of storage buffers and descriptors.
 *
 * @tparam T The type of the data that is sent to the device.
 */
template <typename T> struct DeviceData
{
    TKIT_NON_COPYABLE(DeviceData)
    DeviceData()
    {
        for (u32 i = 0; i < MaxFramesInFlight; ++i)
        {
            DeviceLocalStorage[i] = CreateBuffer<T>(Buffer_DeviceStorage);
            StagingStorage[i] = CreateBuffer<T>(Buffer_Staging);

            const VkDescriptorBufferInfo info = DeviceLocalStorage[i].GetDescriptorInfo();
            DescriptorSets[i] = Assets::WriteStorageBufferDescriptorSet(info);
        }
    }
    ~DeviceData()
    {
        for (u32 i = 0; i < MaxFramesInFlight; ++i)
        {
            DeviceLocalStorage[i].Destroy();
            StagingStorage[i].Destroy();
        }
    }

    void GrowDeviceBuffers(const u32 p_FrameIndex, const u32 p_Instances)
    {
        auto &lbuffer = DeviceLocalStorage[p_FrameIndex];
        auto &sbuffer = StagingStorage[p_FrameIndex];
        if (GrowBufferIfNeeded<T>(lbuffer, p_Instances, Buffer_DeviceStorage))
        {
            const VkDescriptorBufferInfo info = lbuffer.GetDescriptorInfo();
            DescriptorSets[p_FrameIndex] = Assets::WriteStorageBufferDescriptorSet(info, DescriptorSets[p_FrameIndex]);
        }
        GrowBufferIfNeeded<T>(sbuffer, p_Instances, Buffer_Staging);
    }

    PerFrameData<VKit::DeviceBuffer> DeviceLocalStorage;
    PerFrameData<VKit::DeviceBuffer> StagingStorage;
    PerFrameData<VkDescriptorSet> DescriptorSets;
};

/**
 * @brief Some global push constant data that is used by the shaders.
 */
template <Shading Sh> struct PushConstantData;

template <> struct PushConstantData<Shading_Unlit>
{
    f32m4 ProjectionView;
};

template <> struct PushConstantData<Shading_Lit>
{
    f32m4 ProjectionView;
    f32v4 ViewPosition;
    f32v4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

} // namespace Onyx::Detail

TKIT_COMPILER_WARNING_IGNORE_POP()

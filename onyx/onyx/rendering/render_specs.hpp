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

enum class ONYX_API PipelineMode : u8
{
    NoStencilWriteDoFill,
    DoStencilWriteDoFill,
    DoStencilWriteNoFill,
    DoStencilTestNoFill
};

enum class ONYX_API DrawMode : u8
{
    Fill,
    Stencil
};

template <PipelineMode PMode> constexpr DrawMode GetDrawMode() noexcept
{
    if constexpr (PMode == PipelineMode::NoStencilWriteDoFill || PMode == PipelineMode::DoStencilWriteDoFill)
        return DrawMode::Fill;
    else
        return DrawMode::Stencil;
}

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

template <Dimension D, DrawMode DMode> struct InstanceData
{
    mat4 Transform;
    MaterialData<D2> Material;
};

// Could actually save some space by using smaller matrices in the 2D case and removing the last row, as it always is 0
// 0 1 but i dont want to deal with the alignment management tbh

template <> struct ONYX_API InstanceData<D3, DrawMode::Fill>
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView; // The projection view may vary between shapes
    vec4 ViewPosition;
    MaterialData<D3> Material;
};

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

template <Dimension D, DrawMode DMode> struct PolygonInstanceData
{
    InstanceData<D, DMode> BaseData;
    PrimitiveDataLayout Layout;
};

template <Dimension D, DrawMode DMode> struct CircleInstanceData
{
    InstanceData<D, DMode> BaseData;
    alignas(16) vec4 ArcInfo;
    u32 AngleOverflow;
    f32 Hollowness;
};

struct PushConstantData3D
{
    vec4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

template <Dimension D, PipelineMode PMode>
ONYX_API Pipeline::Specs CreateMeshedPipelineSpecs(VkRenderPass p_RenderPass, const VkDescriptorSetLayout *p_Layouts,
                                                   u32 p_LayoutCount) noexcept;

template <Dimension D, PipelineMode PMode>
ONYX_API Pipeline::Specs CreateCirclePipelineSpecs(VkRenderPass p_RenderPass, const VkDescriptorSetLayout *p_Layouts,
                                                   u32 p_LayoutCount) noexcept;

template <Dimension D, DrawMode DMode> struct ONYX_API MeshRendererSpecs
{
    using InstanceData = ONYX::InstanceData<D, DMode>;
    using RenderInfo = ONYX::RenderInfo<D, DMode>;
};
template <Dimension D, DrawMode DMode> struct ONYX_API PrimitiveRendererSpecs
{
    using InstanceData = ONYX::InstanceData<D, DMode>;
    using RenderInfo = ONYX::RenderInfo<D, DMode>;
};
template <Dimension D, DrawMode DMode> struct ONYX_API PolygonRendererSpecs
{
    using InstanceData = ONYX::InstanceData<D, DMode>;
    using RenderInfo = ONYX::RenderInfo<D, DMode>;
    using DeviceInstanceData = PolygonDeviceInstanceData<D, DMode>;
    using HostInstanceData = PolygonInstanceData<D, DMode>;
};
template <Dimension D, DrawMode DMode> struct ONYX_API CircleRendererSpecs
{
    using InstanceData = CircleInstanceData<D, DMode>;
    using RenderInfo = ONYX::RenderInfo<D, DMode>;
};

// This function modifies the axes to support different coordinate systems
ONYX_API void ApplyCoordinateSystem(mat4 &p_Axes, mat4 *p_InverseAxes = nullptr) noexcept;

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
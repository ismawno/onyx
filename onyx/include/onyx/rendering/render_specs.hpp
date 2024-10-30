#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/draw/color.hpp"
#include "onyx/buffer/storage_buffer.hpp"
#include "onyx/rendering/swap_chain.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/draw/primitives.hpp"
#include <vulkan/vulkan.hpp>

namespace ONYX
{

// VERY CLUNKY: 3 out of 4 possible instantiations of MaterialData and RenderInfo are identical

template <u32 N>
    requires(IsDim<N>())
struct MaterialData;

template <> struct MaterialData<2>
{
    Color Color = ONYX::Color::WHITE;
};

template <> struct MaterialData<3>
{
    Color Color = ONYX::Color::WHITE;
    f32 DiffuseContribution = 0.8f;
    f32 SpecularContribution = 0.2f;
    f32 SpecularSharpness = 32.f;
};

using MaterialData2D = MaterialData<2>;
using MaterialData3D = MaterialData<3>;

enum class ONYX_API StencilMode
{
    NoStencilWriteFill,
    StencilWriteFill,
    StencilWriteNoFill,
    StencilTest
};

template <StencilMode Mode> constexpr bool IsFullDrawPass() noexcept
{
    return Mode != StencilMode::StencilTest && Mode != StencilMode::StencilWriteNoFill;
}

template <u32 N, bool FullDrawPass>
    requires(IsDim<N>())
struct RenderInfo;

template <bool FullDrawPass> struct ONYX_API RenderInfo<2, FullDrawPass>
{
    VkCommandBuffer CommandBuffer;
    u32 FrameIndex;
};

template <> struct ONYX_API RenderInfo<3, true>
{
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet LightStorageBuffers;
    u32 FrameIndex;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    vec4 AmbientColor;
};
template <> struct ONYX_API RenderInfo<3, false>
{
    VkCommandBuffer CommandBuffer;
    u32 FrameIndex;
};

template <u32 N, bool FullDrawPass>
    requires(IsDim<N>())
struct InstanceData;

// Could actually save some space by using smaller matrices in the 2D case and removing the last row, as it always is 0
// 0 1 but i dont want to deal with the alignment management tbh

template <bool FullDrawPass> struct ONYX_API InstanceData<2, FullDrawPass>
{
    mat4 Transform;
    MaterialData2D Material;
};
template <> struct ONYX_API InstanceData<3, true>
{
    mat4 Transform;
    mat4 NormalMatrix;
    mat4 ProjectionView; // The projection view may vary between shapes
    vec4 ViewPosition;
    MaterialData3D Material;
};
template <> struct ONYX_API InstanceData<3, false>
{
    mat4 Transform;
    MaterialData2D Material;
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

template <u32 N, bool FullDrawPass>
    requires(IsDim<N>())
struct PolygonDeviceInstanceData : DeviceInstanceData<InstanceData<N, FullDrawPass>>
{
    PolygonDeviceInstanceData(const usize p_Capacity) noexcept
        : DeviceInstanceData<InstanceData<N, FullDrawPass>>(p_Capacity)
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

    std::array<KIT::Storage<MutableVertexBuffer<N>>, SwapChain::MFIF> VertexBuffers;
    std::array<KIT::Storage<MutableIndexBuffer>, SwapChain::MFIF> IndexBuffers;
};

template <u32 N, bool FullDrawPass>
    requires(IsDim<N>())
struct PolygonInstanceData : InstanceData<N, FullDrawPass>
{
    PrimitiveDataLayout Layout;
};

template <u32 N, bool FullDrawPass>
    requires(IsDim<N>())
struct CircleInstanceData : InstanceData<N, FullDrawPass>
{
    alignas(16) vec4 ArcInfo;
    u32 AngleOverflow;
};

struct PushConstantData3D
{
    vec4 AmbientColor;
    u32 DirectionalLightCount;
    u32 PointLightCount;
    u32 _Padding[2];
};

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
struct ONYX_API MeshRendererSpecs
{
    static constexpr u32 Dimension = N;
    static constexpr StencilMode STMode = Mode;
    using InstanceData = ONYX::InstanceData<N, IsFullDrawPass<Mode>()>;
    using RenderInfo = ONYX::RenderInfo<N, IsFullDrawPass<Mode>()>;

    static Pipeline::Specs CreatePipelineSpecs(VkRenderPass p_RenderPass, const VkDescriptorSetLayout *p_Layouts,
                                               u32 p_LayoutCount) noexcept;
};

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
struct ONYX_API PrimitiveRendererSpecs
{
    static constexpr u32 Dimension = N;
    static constexpr StencilMode STMode = Mode;
    using InstanceData = ONYX::InstanceData<N, IsFullDrawPass<Mode>()>;
    using RenderInfo = ONYX::RenderInfo<N, IsFullDrawPass<Mode>()>;

    static Pipeline::Specs CreatePipelineSpecs(VkRenderPass p_RenderPass, const VkDescriptorSetLayout *p_Layouts,
                                               u32 p_LayoutCount) noexcept;
};

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
struct ONYX_API PolygonRendererSpecs
{
    static constexpr u32 Dimension = N;
    static constexpr StencilMode STMode = Mode;
    using InstanceData = ONYX::InstanceData<N, IsFullDrawPass<Mode>()>;
    using DeviceInstanceData = PolygonDeviceInstanceData<N, IsFullDrawPass<Mode>()>;
    using HostInstanceData = PolygonInstanceData<N, IsFullDrawPass<Mode>()>;

    using RenderInfo = ONYX::RenderInfo<N, IsFullDrawPass<Mode>()>;

    static Pipeline::Specs CreatePipelineSpecs(VkRenderPass p_RenderPass, const VkDescriptorSetLayout *p_Layouts,
                                               u32 p_LayoutCount) noexcept;
};

template <u32 N, StencilMode Mode>
    requires(IsDim<N>())
struct ONYX_API CircleRendererSpecs
{
    static constexpr u32 Dimension = N;
    static constexpr StencilMode STMode = Mode;
    using InstanceData = CircleInstanceData<N, IsFullDrawPass<Mode>()>;
    using RenderInfo = ONYX::RenderInfo<N, IsFullDrawPass<Mode>()>;

    static Pipeline::Specs CreatePipelineSpecs(VkRenderPass p_RenderPass, const VkDescriptorSetLayout *p_Layouts,
                                               u32 p_LayoutCount) noexcept;
};

} // namespace ONYX
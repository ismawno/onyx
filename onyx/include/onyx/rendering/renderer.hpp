#pragma once

#include "onyx/rendering/render_systems.hpp"

namespace ONYX
{
// This function modifies the axes to support different coordinate systems
ONYX_API void ApplyCoordinateSystem(mat4 &p_Axes) noexcept;

ONYX_DIMENSION_TEMPLATE class ONYX_API IRenderer
{
    KIT_NON_COPYABLE(IRenderer)
  public:
    IRenderer(Window *p_Window, VkRenderPass p_RenderPass) noexcept;

    void DrawMesh(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform) noexcept;
    void DrawPrimitive(usize p_PrimitiveIndex, const mat<N> &p_Transform) noexcept;
    void DrawPolygon(std::span<const vec<N>> p_Vertices, const mat<N> &p_Transform) noexcept;
    void DrawCircle(const mat<N> &p_Transform) noexcept;

  protected:
    u32 m_FrameIndex = 0;
    MeshRenderer<N> m_MeshRenderer;
    PrimitiveRenderer<N> m_PrimitiveRenderer;
    PolygonRenderer<N> m_PolygonRenderer;
    CircleRenderer<N> m_CircleRenderer;

  private:
    template <typename Renderer, typename... DrawArgs>
    void draw(Renderer &p_Renderer, const mat<N> &p_Transform, DrawArgs &&...p_Args) noexcept;

    Window *m_Window;
};

ONYX_DIMENSION_TEMPLATE class Renderer;

template <> class Renderer<2> final : public IRenderer<2>
{
  public:
    using IRenderer<2>::IRenderer;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;
    void Flush() noexcept;
};

struct ONYX_API DirectionalLight
{
    vec4 DirectionAndIntensity;
    vec4 Color;
};

struct ONYX_API PointLight
{
    vec4 PositionAndIntensity;
    vec4 Color;
    f32 Radius;
};

struct ONYX_API DeviceLightData
{
    KIT_NON_COPYABLE(DeviceLightData)

    DeviceLightData() noexcept;
    ~DeviceLightData() noexcept;

    std::array<KIT::Storage<StorageBuffer<DirectionalLight>>, SwapChain::MFIF> DirectionalLightBuffers;
    std::array<KIT::Storage<StorageBuffer<PointLight>>, SwapChain::MFIF> PointLightBuffers;
    std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
};

template <> class Renderer<3> final : public IRenderer<3>
{
  public:
    Renderer(Window *p_Window, VkRenderPass p_RenderPass) noexcept;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

    void AddDirectionalLight(const DirectionalLight &p_Light) noexcept;
    void AddPointLight(const PointLight &p_Light) noexcept;
    void Flush() noexcept;

    f32 AmbientIntensity = 0.4f;
    Color AmbientColor = Color::WHITE;

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool; // A bit dodgy bc these go out of scope before pipelines
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;

    DynamicArray<DirectionalLight> m_DirectionalLights;
    DynamicArray<PointLight> m_PointLights;
    DeviceLightData m_DeviceLightData;
};

using Renderer2D = Renderer<2>;
using Renderer3D = Renderer<3>;

} // namespace ONYX
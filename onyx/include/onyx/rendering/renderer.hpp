#pragma once

#include "onyx/rendering/render_systems.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"

namespace ONYX
{

template <Dimension D, template <Dimension, PipelineMode> typename R> struct RenderSystem
{
    RenderSystem(VkRenderPass p_RenderPass) noexcept;
    void Flush() noexcept;

    R<D, PipelineMode::NoStencilWriteDoFill> NoStencilWriteDoFill;
    R<D, PipelineMode::DoStencilWriteDoFill> DoStencilWriteDoFill;
    R<D, PipelineMode::DoStencilWriteNoFill> DoStencilWriteNoFill;
    R<D, PipelineMode::DoStencilTestNoFill> DoStencilTestNoFill;
};

template <Dimension D> class ONYX_API IRenderer
{
    KIT_NON_COPYABLE(IRenderer)
  public:
    IRenderer(VkRenderPass p_RenderPass,
              const DynamicArray<RenderState<D>> *p_State) noexcept; // Passing the state like this is a bit dodgy

    void DrawMesh(const KIT::Ref<const Model<D>> &p_Model, const mat<D> &p_Transform) noexcept;
    void DrawPrimitive(usize p_PrimitiveIndex, const mat<D> &p_Transform) noexcept;
    void DrawPolygon(std::span<const vec<D>> p_Vertices, const mat<D> &p_Transform) noexcept;
    void DrawCircle(const mat<D> &p_Transform, f32 p_LowerAngle = 0.f, f32 p_UpperAngle = glm::two_pi<f32>(),
                    f32 p_Hollowness = 0.f) noexcept;

  protected:
    u32 m_FrameIndex = 0;
    // Only used for 2D, but it's easier to just have it here
    u32 m_ZOffset = 0;

    RenderSystem<D, MeshRenderer> m_MeshRenderer;
    RenderSystem<D, PrimitiveRenderer> m_PrimitiveRenderer;
    RenderSystem<D, PolygonRenderer> m_PolygonRenderer;
    RenderSystem<D, CircleRenderer> m_CircleRenderer;

  private:
    template <typename Renderer, typename... DrawArgs>
    void draw(Renderer &p_Renderer, const mat<D> &p_Transform, DrawArgs &&...p_Args) noexcept;

    const DynamicArray<RenderState<D>> *m_State;
};

template <Dimension D> class Renderer;

template <> class Renderer<D2> final : public IRenderer<D2>
{
  public:
    using IRenderer<D2>::IRenderer;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;
    void Flush() noexcept;
};

struct ONYX_API DirectionalLight
{
    vec4 DirectionAndIntensity;
    Color Color;
};

struct ONYX_API PointLight
{
    vec4 PositionAndIntensity;
    Color Color;
    f32 Radius;
};

struct ONYX_API DeviceLightData
{
    KIT_NON_COPYABLE(DeviceLightData)

    DeviceLightData(usize p_Capacity) noexcept;
    ~DeviceLightData() noexcept;

    std::array<KIT::Storage<StorageBuffer<DirectionalLight>>, SwapChain::MFIF> DirectionalLightBuffers;
    std::array<KIT::Storage<StorageBuffer<PointLight>>, SwapChain::MFIF> PointLightBuffers;
    std::array<VkDescriptorSet, SwapChain::MFIF> DescriptorSets;
};

template <> class Renderer<D3> final : public IRenderer<D3>
{
  public:
    Renderer(VkRenderPass p_RenderPass, const DynamicArray<RenderState<D3>> *p_State) noexcept;

    void Render(VkCommandBuffer p_CommandBuffer) noexcept;

    void AddDirectionalLight(const DirectionalLight &p_Light) noexcept;
    void AddPointLight(const PointLight &p_Light) noexcept;
    void Flush() noexcept;

    Color AmbientColor = Color{Color::WHITE, 0.4f};

  private:
    KIT::Ref<DescriptorPool> m_DescriptorPool; // A bit dodgy bc these go out of scope before pipelines
    KIT::Ref<DescriptorSetLayout> m_DescriptorSetLayout;

    DynamicArray<DirectionalLight> m_DirectionalLights;
    DynamicArray<PointLight> m_PointLights;
    DeviceLightData m_DeviceLightData{ONYX_BUFFER_INITIAL_CAPACITY};
};

} // namespace ONYX
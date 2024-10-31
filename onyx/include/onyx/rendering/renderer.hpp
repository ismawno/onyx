#pragma once

#include "onyx/rendering/render_systems.hpp"
#include "onyx/descriptors/descriptor_set_layout.hpp"
#include "onyx/descriptors/descriptor_pool.hpp"

namespace ONYX
{
// This function modifies the axes to support different coordinate systems
ONYX_API void ApplyCoordinateSystem(mat4 &p_Axes, mat4 *p_InverseAxes = nullptr) noexcept;

template <u32 N>
    requires(IsDim<N>())
struct RenderState;
template <> struct RenderState<2>
{
    mat3 Transform{1.f};
    mat3 Axes{1.f};
    Color OutlineColor = Color::WHITE;
    f32 OutlineWidth = 0.f;
    MaterialData2D Material{};
    bool Fill = true;
    bool Outline = false;
};

template <> struct RenderState<3>
{
    mat4 Transform{1.f};
    mat4 Axes{1.f};
    mat4 InverseAxes{1.f}; // Just for caching
    mat4 Projection{1.f};
    Color LightColor = Color::WHITE;
    Color OutlineColor = Color::WHITE;
    f32 OutlineWidth = 0.f;
    MaterialData3D Material{};
    bool Fill = true;
    bool Outline = false;
    bool HasProjection = false;
};

using RenderState2D = RenderState<2>;
using RenderState3D = RenderState<3>;

template <template <typename> typename R, template <u32, StencilMode> typename RSpecs, u32 N> struct RenderSystem
{
    template <StencilMode Mode> using Specs = RSpecs<N, Mode>;

    RenderSystem(VkRenderPass p_RenderPass) noexcept;
    void Flush() noexcept;

    R<RSpecs<N, StencilMode::NoStencilWriteFill>> NoStencilWriteFill;
    R<RSpecs<N, StencilMode::StencilWriteFill>> StencilWriteFill;
    R<RSpecs<N, StencilMode::StencilWriteNoFill>> StencilWriteNoFill;
    R<RSpecs<N, StencilMode::StencilTest>> StencilTest;
};

template <u32 N>
    requires(IsDim<N>())
class ONYX_API IRenderer
{
    KIT_NON_COPYABLE(IRenderer)
  public:
    IRenderer(VkRenderPass p_RenderPass,
              const DynamicArray<RenderState<N>> *p_State) noexcept; // Passing the state like this is a bit dodgy

    void DrawMesh(const KIT::Ref<const Model<N>> &p_Model, const mat<N> &p_Transform) noexcept;
    void DrawPrimitive(usize p_PrimitiveIndex, const mat<N> &p_Transform) noexcept;
    void DrawPolygon(std::span<const vec<N>> p_Vertices, const mat<N> &p_Transform) noexcept;
    void DrawCircle(const mat<N> &p_Transform, f32 p_LowerAngle = 0.f, f32 p_UpperAngle = glm::two_pi<f32>(),
                    f32 p_Hollowness = 0.f) noexcept;

  protected:
    u32 m_FrameIndex = 0;
    // Only used for 2D, but it's easier to just have it here
    u32 m_ZOffset = 0;

    RenderSystem<MeshRenderer, MeshRendererSpecs, N> m_MeshRenderer;
    RenderSystem<PrimitiveRenderer, PrimitiveRendererSpecs, N> m_PrimitiveRenderer;
    RenderSystem<PolygonRenderer, PolygonRendererSpecs, N> m_PolygonRenderer;
    RenderSystem<CircleRenderer, CircleRendererSpecs, N> m_CircleRenderer;

  private:
    template <typename Renderer, typename... DrawArgs>
    void draw(Renderer &p_Renderer, const mat<N> &p_Transform, DrawArgs &&...p_Args) noexcept;

    const DynamicArray<RenderState<N>> *m_State;
};

template <u32 N>
    requires(IsDim<N>())
class Renderer;

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

template <> class Renderer<3> final : public IRenderer<3>
{
  public:
    Renderer(VkRenderPass p_RenderPass, const DynamicArray<RenderState3D> *p_State) noexcept;

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

using Renderer2D = Renderer<2>;
using Renderer3D = Renderer<3>;

} // namespace ONYX
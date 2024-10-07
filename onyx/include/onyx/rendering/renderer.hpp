#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/rendering/pipeline.hpp"
#include "onyx/draw/model.hpp"
#include "onyx/core/core.hpp"

#include <vulkan/vulkan.hpp>

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct RenderInfo;

template <> struct ONYX_API RenderInfo<2>
{
    VkCommandBuffer CommandBuffer;
};

template <> struct ONYX_API RenderInfo<3>
{
    VkCommandBuffer CommandBuffer;
    VkDescriptorSet GlobalDescriptorSet;
    mat4 *Projection;
};

ONYX_DIMENSION_TEMPLATE class ONYX_API MeshRenderer
{
    KIT_NON_COPYABLE(MeshRenderer)
  public:
    MeshRenderer(VkRenderPass p_RenderPass, VkDescriptorSetLayout p_Layout) noexcept;
    ~MeshRenderer() noexcept;

    void Draw(const Model *p_Model, const mat<N> &p_Transform, const vec4 &p_Color) noexcept;
    void Render(const RenderInfo<N> &p_Info) const noexcept;

    void Flush() noexcept;

  private:
    struct DrawData
    {
        DrawData(const ONYX::Model *p_Model, const mat<N> &p_Transform, const vec4 &p_Color) noexcept
            : Model(p_Model), Transform(p_Transform), Color(p_Color)
        {
        }
        const Model *Model;
        mat<N> Transform;
        vec4 Color;
    };

    KIT::Storage<Pipeline> m_Pipeline;
    DynamicArray<DrawData> m_DrawData;
};

using MeshRenderer2D = MeshRenderer<2>;
using MeshRenderer3D = MeshRenderer<3>;

ONYX_DIMENSION_TEMPLATE class ONYX_API CircleRenderer
{
    KIT_NON_COPYABLE(CircleRenderer)
  public:
    CircleRenderer(VkRenderPass p_RenderPass, VkDescriptorSetLayout p_Layout) noexcept;
    ~CircleRenderer() noexcept;

    void Draw(const mat<N> &p_Transform, const vec4 &p_Color) noexcept;
    void Render(const RenderInfo<N> &p_Info) const noexcept;

    void Flush() noexcept;

  private:
    struct DrawData
    {
        DrawData(const mat<N> &p_Transform, const vec4 &p_Color) noexcept : Transform(p_Transform), Color(p_Color)
        {
        }
        mat<N> Transform;
        vec4 Color;
    };

    KIT::Storage<Pipeline> m_Pipeline;
    DynamicArray<DrawData> m_DrawData;
};

using CircleRenderer2D = CircleRenderer<2>;
using CircleRenderer3D = CircleRenderer<3>;
} // namespace ONYX
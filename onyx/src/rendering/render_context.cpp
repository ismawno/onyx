#include "core/pch.hpp"
#include "onyx/rendering/render_context.hpp"
#include "onyx/descriptors/descriptor_writer.hpp"
#include "onyx/app/input.hpp"
#include "onyx/app/window.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/utilities/math.hpp"

namespace ONYX
{

ONYX_DIMENSION_TEMPLATE IRenderContext<N>::IRenderContext(Window *p_Window) noexcept : m_Window(p_Window)
{
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::initializeRenderers(const VkRenderPass p_RenderPass,
                                                                    const VkDescriptorSetLayout p_Layout) noexcept
{
    m_MeshRenderer.Create(p_RenderPass, p_Layout);
    m_CircleRenderer.Create(p_RenderPass, p_Layout);
    m_RenderState.push_back(RenderState<N>{});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Background(const Color &p_Color) noexcept
{
    m_MeshRenderer->Flush();
    m_CircleRenderer->Flush();
    m_Window->BackgroundColor = p_Color;
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Translate(const vec<N> &p_Translation) noexcept
{
    mat<N> &transform = m_RenderState.back().Transform;
    transform = Transform<N>::ComputeTranslationMatrix(p_Translation) * transform;
}
void RenderContext<2>::Translate(const f32 p_X, const f32 p_Y) noexcept
{
    Translate(vec2{p_X, p_Y});
}
void RenderContext<3>::Translate(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Translate(vec3{p_X, p_Y, p_Z});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Scale(const vec<N> &p_Scale) noexcept
{
    mat<N> &transform = m_RenderState.back().Transform;
    transform = Transform<N>::ComputeScaleMatrix(p_Scale) * transform;
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Scale(const f32 p_Scale) noexcept
{
    mat<N> &transform = m_RenderState.back().Transform;
    transform = Transform<N>::ComputeScaleMatrix(p_Scale) * transform;
}
void RenderContext<2>::Scale(const f32 p_X, const f32 p_Y) noexcept
{
    Scale(vec2{p_X, p_Y});
}
void RenderContext<3>::Scale(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Scale(vec3{p_X, p_Y, p_Z});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::KeepWindowAspect() noexcept
{
    const f32 aspect = 1.f / m_Window->GetScreenAspect();
    mat<N> &view = m_RenderState.back().View;
    if constexpr (N == 2)
        view *= Transform2D::ComputeScaleMatrix(vec2{aspect, 1.f});
    else
        view *= Transform3D::ComputeScaleMatrix(vec3{aspect, 1.f, aspect});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::TranslateView(const vec<N> &p_Translation) noexcept
{
    mat<N> &view = m_RenderState.back().View;
    view *= Transform<N>::ComputeTranslationMatrix(-p_Translation);
}
void RenderContext<2>::TranslateView(const f32 p_X, const f32 p_Y) noexcept
{
    TranslateView(vec2{p_X, p_Y});
}
void RenderContext<3>::TranslateView(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    TranslateView(vec3{p_X, p_Y, p_Z});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::ScaleView(const vec<N> &p_Scale) noexcept
{
    mat<N> &view = m_RenderState.back().View;
    view = Transform<N>::ComputeScaleMatrix(1.f / p_Scale) * view;
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::ScaleView(const f32 p_Scale) noexcept
{
    mat<N> &view = m_RenderState.back().View;
    view = Transform<N>::ComputeScaleMatrix(1.f / p_Scale) * view;
}
void RenderContext<2>::ScaleView(const f32 p_X, const f32 p_Y) noexcept
{
    ScaleView(vec2{p_X, p_Y});
}
void RenderContext<3>::ScaleView(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    ScaleView(vec3{p_X, p_Y, p_Z});
}

void RenderContext<2>::Rotate(const f32 p_Angle) noexcept
{
    mat3 &transform = m_RenderState.back().Transform;
    transform = Transform2D::ComputeRotationMatrix(p_Angle) * transform;
}

void RenderContext<3>::Rotate(const quat &p_Quaternion) noexcept
{
    mat4 &transform = m_RenderState.back().Transform;
    transform = Transform3D::ComputeRotationMatrix(p_Quaternion) * transform;
}

void RenderContext<3>::Rotate(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    Rotate(glm::angleAxis(p_Angle, p_Axis));
}

void RenderContext<3>::Rotate(const vec3 &p_Angles) noexcept
{
    Rotate(glm::quat(p_Angles));
}

void RenderContext<2>::RotateView(const f32 p_Angle) noexcept
{
    mat3 &view = m_RenderState.back().View;
    view *= Transform2D::ComputeRotationMatrix(-p_Angle);
}

void RenderContext<3>::RotateView(const quat &p_Quaternion) noexcept
{
    mat4 &view = m_RenderState.back().View;
    view *= Transform3D::ComputeRotationMatrix(glm::conjugate(p_Quaternion));
}

void RenderContext<3>::RotateView(const f32 p_Angle, const vec3 &p_Axis) noexcept
{
    RotateView(glm::angleAxis(p_Angle, p_Axis));
}

void RenderContext<3>::RotateView(const vec3 &p_Angles) noexcept
{
    RotateView(glm::quat(p_Angles));
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square() noexcept
{
    const Model *model = Model::GetSquare<N>();
    Mesh(model);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const f32 p_Size) noexcept
{
    const mat<N> transform = Transform<N>::ComputeScaleMatrix(p_Size) * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Square(const vec<N> &p_Position, const f32 p_Size) noexcept
{
    const mat<N> transform =
        Transform<N>::ComputeTranslationScaleMatrix(p_Position, p_Size) * m_RenderState.back().Transform;
    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y) noexcept
{
    Square(vec2{p_X, p_Y});
}
void RenderContext<2>::Square(const f32 p_X, const f32 p_Y, const f32 p_Size) noexcept
{
    Square(vec2{p_X, p_Y}, p_Size);
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Square(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Square(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Size) noexcept
{
    Square(vec3{p_X, p_Y, p_Z}, p_Size);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform = Transform2D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeScaleMatrix(vec3{p_Dimensions, 1.f}) * m_RenderState.back().Transform;

    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform =
            Transform2D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Dimensions, 1.f}) *
                    m_RenderState.back().Transform;

    const Model *model = Model::GetSquare<N>();
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Rect(const f32 p_XDim, const f32 p_YDim) noexcept
{
    Rect(vec2{p_XDim, p_YDim});
}
void RenderContext<2>::Rect(const f32 p_X, const f32 p_Y, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Rect(vec2{p_X, p_Y}, vec2{p_XDim, p_YDim});
}
void RenderContext<3>::Rect(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Rect(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides) noexcept
{
    const Model *model = Model::GetRegularPolygon<N>(p_Sides);
    Mesh(model);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position) noexcept
{
    const mat<N> transform = Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    const Model *model = Model::GetRegularPolygon<N>(p_Sides);
    Mesh(model, transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::NGon(const u32 p_Sides, const vec<N> &p_Position,
                                                     const f32 p_Radius) noexcept
{
    const mat<N> transform =
        Transform<N>::ComputeTranslationScaleMatrix(p_Position, p_Radius) * m_RenderState.back().Transform;
    const Model *model = Model::GetRegularPolygon<N>(p_Sides);
    Mesh(model, transform);
}
void RenderContext<2>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y) noexcept
{
    NGon(p_Sides, vec2{p_X, p_Y});
}
void RenderContext<2>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Radius) noexcept
{
    NGon(p_Sides, vec2{p_X, p_Y}, p_Radius);
}
void RenderContext<3>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    NGon(p_Sides, vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::NGon(const u32 p_Sides, const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius) noexcept
{
    NGon(p_Sides, vec3{p_X, p_Y, p_Z}, p_Radius);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle() noexcept
{
    circleMesh(m_RenderState.back().Transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const vec<N> &p_Position) noexcept
{
    const mat<N> transform = Transform<N>::ComputeTranslationMatrix(p_Position) * m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Circle(const vec<N> &p_Position, const f32 p_Radius) noexcept
{
    const mat<N> transform =
        Transform<N>::ComputeTranslationScaleMatrix(p_Position, p_Radius) * m_RenderState.back().Transform;
    circleMesh(transform);
}
void RenderContext<2>::Circle(const f32 p_X, const f32 p_Y) noexcept
{
    Circle(vec2{p_X, p_Y});
}
void RenderContext<2>::Circle(const f32 p_X, const f32 p_Y, const f32 p_Radius) noexcept
{
    Circle(vec2{p_X, p_Y}, p_Radius);
}
void RenderContext<3>::Circle(const f32 p_X, const f32 p_Y, const f32 p_Z) noexcept
{
    Circle(vec3{p_X, p_Y, p_Z});
}
void RenderContext<3>::Circle(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_Radius) noexcept
{
    Circle(vec3{p_X, p_Y, p_Z}, p_Radius);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform = Transform2D::ComputeScaleMatrix(p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeScaleMatrix(vec3{p_Dimensions, 1.f}) * m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const vec<N> &p_Position, const vec2 &p_Dimensions) noexcept
{
    mat<N> transform;
    if constexpr (N == 2)
        transform =
            Transform2D::ComputeTranslationScaleMatrix(p_Position, p_Dimensions) * m_RenderState.back().Transform;
    else
        transform = Transform3D::ComputeTranslationScaleMatrix(p_Position, vec3{p_Dimensions, 1.f}) *
                    m_RenderState.back().Transform;
    circleMesh(transform);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Ellipse(const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec2{p_XDim, p_YDim});
}
void RenderContext<2>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec2{p_X, p_Y}, vec2{p_XDim, p_YDim});
}
void RenderContext<3>::Ellipse(const f32 p_X, const f32 p_Y, const f32 p_Z, const f32 p_XDim, const f32 p_YDim) noexcept
{
    Ellipse(vec3{p_X, p_Y, p_Z}, vec2{p_XDim, p_YDim});
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const Model *p_Model, const mat<N> &p_Transform) noexcept
{
    const RenderState<N> &state = m_RenderState.back();
    if constexpr (N == 2)
    {
        if (!state.NoStroke && !KIT::ApproachesZero(state.StrokeWidth))
        {
            const vec2 scale = Transform2D::ExtractScaleTransform(p_Transform);
            const vec2 stroke = (scale + state.StrokeWidth) / scale;
            const mat<N> transform = state.View * Transform2D::ComputeScaleMatrix(stroke) * p_Transform;
            m_MeshRenderer->Draw(p_Model, transform, state.StrokeColor);
        }
    }
    m_MeshRenderer->Draw(p_Model, state.View * p_Transform, state.FillColor);
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Mesh(const Model *p_Model) noexcept
{
    Mesh(p_Model, m_RenderState.back().Transform);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Push() noexcept
{
    m_RenderState.push_back(m_RenderState.back());
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::PushAndClear() noexcept
{
    m_RenderState.push_back(RenderState<N>{});
}
ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Pop() noexcept
{
    KIT_ASSERT(m_RenderState.size() > 1, "For every push, there must be a pop");
    m_RenderState.pop_back();
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::Fill(const Color &p_Color) noexcept
{
    m_RenderState.back().FillColor = p_Color;
}
void RenderContext<2>::Stroke() noexcept
{
    m_RenderState.back().NoStroke = false;
}
void RenderContext<2>::Stroke(const Color &p_Color) noexcept
{
    m_RenderState.back().StrokeColor = p_Color;
    m_RenderState.back().NoStroke = false;
}
void RenderContext<2>::StrokeWidth(const f32 p_Width) noexcept
{
    m_RenderState.back().StrokeWidth = p_Width;
    m_RenderState.back().NoStroke = false;
}
void RenderContext<2>::NoStroke() noexcept
{
    m_RenderState.back().NoStroke = true;
}

vec2 RenderContext<2>::GetViewMousePosition() const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
    return glm::inverse(m_RenderState.back().View) * vec3{mpos, 1.f};
}
vec3 RenderContext<3>::GetViewMousePosition(const f32 p_Depth) const noexcept
{
    const vec2 mpos = Input::GetMousePosition(m_Window);
    return glm::inverse(m_RenderState.back().View) * vec4{mpos, p_Depth, 1.f};
}

void RenderContext<3>::SetPerspectiveProjection(const f32 p_FieldOfView, const f32 p_Aspect, const f32 p_Near,
                                                const f32 p_Far) noexcept
{
    const f32 halfPov = glm::tan(0.5f * p_FieldOfView);

    m_Projection = mat4{0.f};
    m_Projection[0][0] = 1.f / (p_Aspect * halfPov);
    m_Projection[1][1] = 1.f / halfPov;
    m_Projection[2][2] = p_Far / (p_Far - p_Near);
    m_Projection[2][3] = 1.f;
    m_Projection[3][2] = p_Far * p_Near / (p_Near - p_Far);
}
void RenderContext<3>::SetOrthographicProjection() noexcept
{
    // Already included in the view matrix
    m_Projection = mat4{1.f};
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::circleMesh(const mat<N> &p_Transform) noexcept
{
    const RenderState<N> &state = m_RenderState.back();
    if constexpr (N == 2)
    {
        if (!state.NoStroke && !KIT::ApproachesZero(state.StrokeWidth))
        {
            const vec2 scale = Transform2D::ExtractScaleTransform(p_Transform);
            const vec2 stroke = (scale + state.StrokeWidth) / scale;
            const mat<N> transform = state.View * Transform2D::ComputeScaleMatrix(stroke) * p_Transform;
            m_CircleRenderer->Draw(transform, state.StrokeColor);
        }
    }
    m_CircleRenderer->Draw(state.View * p_Transform, state.FillColor);
}

ONYX_DIMENSION_TEMPLATE void IRenderContext<N>::resetRenderState() noexcept
{
    KIT_ASSERT(m_RenderState.size() == 1, "For every push, there must be a pop");
    m_RenderState[0] = RenderState<N>{};
}

RenderContext<2>::RenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : IRenderContext<2>(p_Window)
{
    initializeRenderers(p_RenderPass, nullptr);
}

void RenderContext<2>::Render(const VkCommandBuffer p_CommandBuffer) noexcept
{
    RenderInfo<2> info{};
    info.CommandBuffer = p_CommandBuffer;

    m_MeshRenderer->Render(info);
    m_CircleRenderer->Render(info);
    resetRenderState();
}

RenderContext<3>::RenderContext(Window *p_Window, const VkRenderPass p_RenderPass) noexcept
    : IRenderContext<3>(p_Window)
{
    m_Device = Core::GetDevice();
    createGlobalUniformHelper();
    initializeRenderers(p_RenderPass, m_GlobalUniformHelper->Layout.GetLayout());
}

RenderContext<3>::~RenderContext() noexcept
{
    m_GlobalUniformHelper.Destroy();
}

struct GlobalUBO
{
    vec4 LightDirection;
    f32 LightIntensity;
    f32 AmbientIntensity;
};

void RenderContext<3>::Render(const u32 p_FrameIndex, const VkCommandBuffer p_CommandBuffer) noexcept
{
    GlobalUBO ubo{};
    ubo.LightDirection = vec4{glm::normalize(LightDirection), 0.f};
    ubo.LightIntensity = LightIntensity;
    ubo.AmbientIntensity = AmbientIntensity;

    m_GlobalUniformHelper->UniformBuffer.WriteAt(p_FrameIndex, &ubo);
    m_GlobalUniformHelper->UniformBuffer.FlushAt(p_FrameIndex);

    RenderInfo<3> info{};
    info.CommandBuffer = p_CommandBuffer;
    info.GlobalDescriptorSet = m_GlobalUniformHelper->DescriptorSets[p_FrameIndex];

    m_MeshRenderer->Render(info);
    m_CircleRenderer->Render(info);
    resetRenderState();
}

void RenderContext<3>::createGlobalUniformHelper() noexcept
{
    DescriptorPool::Specs poolSpecs{};
    poolSpecs.MaxSets = SwapChain::MAX_FRAMES_IN_FLIGHT;
    poolSpecs.PoolSizes.push_back(
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT});

    static constexpr std::array<VkDescriptorSetLayoutBinding, 1> bindings = {
        {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};

    const auto &props = m_Device->GetProperties();
    Buffer::Specs bufferSpecs{};
    bufferSpecs.InstanceCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
    bufferSpecs.InstanceSize = sizeof(GlobalUBO);
    bufferSpecs.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferSpecs.AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    bufferSpecs.AllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    bufferSpecs.MinimumAlignment =
        glm::max(props.limits.minUniformBufferOffsetAlignment, props.limits.nonCoherentAtomSize);

    m_GlobalUniformHelper.Create(poolSpecs, bindings, bufferSpecs);
    m_GlobalUniformHelper->UniformBuffer.Map();

    for (usize i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const auto info = m_GlobalUniformHelper->UniformBuffer.DescriptorInfoAt(i);
        DescriptorWriter writer{&m_GlobalUniformHelper->Layout, &m_GlobalUniformHelper->Pool};
        writer.WriteBuffer(0, &info);
        m_GlobalUniformHelper->DescriptorSets[i] = writer.Build();
    }
}

template class IRenderContext<2>;
template class IRenderContext<3>;

} // namespace ONYX
#include "core/pch.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/core/logging.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE struct Trigonometry;

template <> struct Trigonometry<2>
{
    Trigonometry(f32 p_Rotation) noexcept : c(glm::cos(p_Rotation)), s(glm::sin(p_Rotation))
    {
    }

    mat2 RotationMatrix() const noexcept
    {
        return mat2(c, -s, s, c);
    }

    f32 c;
    f32 s;
};

// Potentially unused
template <> struct Trigonometry<3>
{
    Trigonometry(const vec3 &p_Rotation) noexcept
        : c1(glm::cos(p_Rotation.x)), s1(glm::sin(p_Rotation.x)), c2(glm::cos(p_Rotation.y)),
          s2(glm::sin(p_Rotation.y)), c3(glm::cos(p_Rotation.z)), s3(glm::sin(p_Rotation.z))
    {
    }

    f32 c1;
    f32 s1;
    f32 c2;
    f32 s2;
    f32 c3;
    f32 s3;
};

ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeModelTransform() const noexcept
{
    mat4 result;
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({Scale.x, 0.f}, {0.f, Scale.y});
        const mat2 rotScale = Trigonometry<2>(Rotation).RotationMatrix() * scale;
        const vec2 translation = Position - rotScale * Origin;

        result = mat4{vec4(rotScale[0], 0.f, 0.f), vec4(rotScale[1], 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                      vec4(translation, 0.f, 1.f)};
    }
    else
    {
        const mat3 scale = mat3({Scale.x, 0.f, 0.f}, {0.f, Scale.y, 0.f}, {0.f, 0.f, Scale.z});
        const mat3 rotScale = glm::toMat3(Rotation) * scale;
        const vec3 translation = Position - rotScale * Origin;

        result = mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4(translation, 1.f)};
    }
    if (Parent)
        return Parent->ComputeModelTransform() * result;
    return result;
}

ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeViewTransform() const noexcept
{
    mat4 result;
    vec<N> sc = Scale;
    sc.y = -sc.y; // Flip the y-axis. I want a Y-up coordinate system
    if constexpr (N == 2)
    {
        const mat2 rotation = Trigonometry<2>(-Rotation).RotationMatrix();
        const vec2 translation = (Origin - rotation * Position) / sc;

        result = mat4{vec4(rotation[0] / sc, 0.f, 0.f), vec4(rotation[1] / sc, 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                      vec4(translation, 0.f, 1.f)};
    }
    else
    {
        const mat3 rotation = glm::toMat3(glm::conjugate(Rotation));
        const vec3 translation = (Origin - rotation * Position) / sc;
        result = mat4{vec4(rotation[0] / sc, 0.f), vec4(rotation[1] / sc, 0.f), vec4(rotation[2] / sc, 0.f),
                      vec4(translation, 1.f)};
    }
    if (Parent)
        return Parent->ComputeViewTransform() * result;
    return result;
}

ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeInverseModelTransform() const noexcept
{
    mat4 result;
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({1.f / Scale.x, 0.f}, {0.f, 1.f / Scale.y});
        const mat2 rotScale = scale * Trigonometry<2>(-Rotation).RotationMatrix();
        const vec2 translation = Origin - rotScale * Position;

        result = mat4{vec4(rotScale[0], 0.f, 0.f), vec4(rotScale[1], 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                      vec4(translation, 0.f, 1.f)};
    }
    else
    {
        const mat3 scale = mat3({1.f / Scale.x, 0.f, 0.f}, {0.f, 1.f / Scale.y, 0.f}, {0.f, 0.f, 1.f / Scale.z});
        const mat3 rotScale = scale * glm::toMat3(glm::conjugate(Rotation));
        const vec3 translation = Origin - rotScale * Position;

        result = mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4(translation, 1.f)};
    }
    if (Parent)
        return result * Parent->ComputeInverseModelTransform();
    return result;
}

ONYX_DIMENSION_TEMPLATE mat4 ITransform<N>::ComputeInverseViewTransform() const noexcept
{
    mat4 result;
    if constexpr (N == 2)
    {
        const mat2 rotation = Trigonometry<2>(-Rotation).RotationMatrix();
        const vec2 translation = Position - rotation * Origin;

        return mat4{vec4(rotation[0] * Scale.x, 0.f, 0.f), vec4(-rotation[1] * Scale.y, 0.f, 0.f),
                    vec4(0.f, 0.f, 1.f, 0.f), vec4(translation, 0.f, 1.f)};
    }
    else
    {
        const mat3 rotation = glm::toMat3(Rotation);
        const vec3 translation = Position - rotation * Origin;
        return mat4{vec4(rotation[0] * Scale.x, 0.f), vec4(-rotation[1] * Scale.y, 0.f),
                    vec4(rotation[2] * Scale.z, 0.f), vec4(translation, 1.f)};
    }
    if (Parent)
        return result * Parent->ComputeInverseViewTransform();
    return result;
}

ONYX_DIMENSION_TEMPLATE void ITransform<N>::UpdateFromModelTransform(const mat4 &p_ModelTransform) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 rotScale = mat2{p_ModelTransform[0], p_ModelTransform[1]};
        Scale = vec2{glm::length(rotScale[0]), glm::length(rotScale[1])};

        // No need to normalize: scale cancels out
        Rotation = glm::atan(p_ModelTransform[0].y, p_ModelTransform[0].x);
        Position = vec2{p_ModelTransform[3]} + rotScale * Origin;
    }
    else
    {
        const mat3 rotScale = mat3{p_ModelTransform[0], p_ModelTransform[1], p_ModelTransform[2]};
        Scale = vec3{glm::length(rotScale[0]), glm::length(rotScale[1]), glm::length(rotScale[2])};

        vec3 angles;
        angles.x = glm::atan(rotScale[1][2], rotScale[2][2]);
        angles.y =
            glm::atan(-rotScale[0][2], glm::sqrt(rotScale[1][2] * rotScale[1][2] + rotScale[2][2] * rotScale[2][2]));
        angles.z = glm::atan(rotScale[0][1], rotScale[0][0]);

        Rotation = quat{angles};
        Position = vec3{p_ModelTransform[3]} + rotScale * Origin;
    }
}

ONYX_DIMENSION_TEMPLATE vec<N> ITransform<N>::LocalOffset(const vec<N> &p_Offset) const noexcept
{
    if constexpr (N == 2)
        return Trigonometry<2>(Rotation).RotationMatrix() * p_Offset;
    else
        return Rotation * p_Offset;
}

ONYX_DIMENSION_TEMPLATE vec<N> ITransform<N>::LocalOffsetX(const f32 p_Offset) const noexcept
{
    if constexpr (N == 2)
    {
        const auto [c, s] = Trigonometry<2>(Rotation);
        return vec<N>(c * p_Offset, s * p_Offset);
    }
    else
        return LocalOffset(vec3{p_Offset, 0.f, 0.f});
}

ONYX_DIMENSION_TEMPLATE vec<N> ITransform<N>::LocalOffsetY(const f32 p_Offset) const noexcept
{
    if constexpr (N == 2)
    {
        const auto [c, s] = Trigonometry<2>(Rotation);
        return vec<N>(-s * p_Offset, c * p_Offset);
    }
    else
        return LocalOffset(vec3{0.f, p_Offset, 0.f});
}

Transform<2> Transform<2>::FromModelTransform(const mat4 &p_ModelTransform) noexcept
{
    Transform<2> result{};
    result.UpdateFromModelTransform(p_ModelTransform);
    return result;
}

vec3 Transform<3>::GetEulerAngles() const noexcept
{
    return glm::eulerAngles(Rotation);
}
void Transform<3>::SetEulerAngles(const vec3 &p_Angles) noexcept
{
    Rotation = glm::quat(p_Angles);
    Rotation = glm::normalize(Rotation);
}

vec3 Transform<3>::LocalOffsetZ(const f32 p_Offset) const noexcept
{
    return LocalOffset(vec3{0.f, 0.f, p_Offset});
}

void Transform<3>::RotateLocalAxis(const quat &p_Rotation) noexcept
{
    Rotation = glm::normalize(Rotation * p_Rotation);
}
void Transform<3>::RotateLocalAxis(const vec3 &p_Angles) noexcept
{
    RotateLocalAxis(glm::quat(p_Angles));
}
void Transform<3>::RotateLocalAxisX(const f32 p_Angle) noexcept
{
    RotateLocalAxis(glm::angleAxis(p_Angle, vec3{1.f, 0.f, 0.f}));
}
void Transform<3>::RotateLocalAxisY(const f32 p_Angle) noexcept
{
    RotateLocalAxis(glm::angleAxis(p_Angle, vec3{0.f, 1.f, 0.f}));
}
void Transform<3>::RotateLocalAxisZ(const f32 p_Angle) noexcept
{
    RotateLocalAxis(glm::angleAxis(p_Angle, vec3{0.f, 0.f, 1.f}));
}

void Transform<3>::RotateGlobalAxis(const quat &p_Rotation) noexcept
{
    Rotation = glm::normalize(p_Rotation * Rotation);
}
void Transform<3>::RotateGlobalAxis(const vec3 &p_Angles) noexcept
{
    RotateGlobalAxis(glm::quat(p_Angles));
}
void Transform<3>::RotateGlobalAxisX(const f32 p_Angle) noexcept
{
    RotateGlobalAxis(glm::angleAxis(p_Angle, vec3{1.f, 0.f, 0.f}));
}
void Transform<3>::RotateGlobalAxisY(const f32 p_Angle) noexcept
{
    RotateGlobalAxis(glm::angleAxis(p_Angle, vec3{0.f, 1.f, 0.f}));
}
void Transform<3>::RotateGlobalAxisZ(const f32 p_Angle) noexcept
{
    RotateGlobalAxis(glm::angleAxis(p_Angle, vec3{0.f, 0.f, 1.f}));
}

Transform<3> Transform<3>::FromModelTransform(const mat4 &p_ModelTransform) noexcept
{
    Transform<3> result{};
    result.UpdateFromModelTransform(p_ModelTransform);
    return result;
}

template struct ITransform<2>;
template struct ITransform<3>;

} // namespace ONYX
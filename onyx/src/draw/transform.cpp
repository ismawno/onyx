#include "core/pch.hpp"
#include "onyx/draw/transform.hpp"

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

mat4 Transform<2>::ModelTransform() const noexcept
{
    const mat2 scale = mat2({Scale.x, 0.f}, {0.f, Scale.y});
    const mat2 linear = Trigonometry<2>(Rotation).RotationMatrix() * scale;
    const vec2 translation = Position - linear * Origin;

    const mat4 result = mat4{vec4(linear[0], 0.f, 0.f), vec4(linear[1], 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                             vec4(translation, 0.f, 1.f)};
    return Parent ? Parent->ModelTransform() * result : result;
}

mat4 Transform<2>::InverseModelTransform() const noexcept
{
    const mat2 scale = mat2({1.f / Scale.x, 0.f}, {0.f, 1.f / Scale.y});
    const mat2 linear = scale * Trigonometry<2>(-Rotation).RotationMatrix();
    const vec2 translation = Origin - linear * Position;

    const mat4 result = mat4{vec4(linear[0], 0.f, 0.f), vec4(linear[1], 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                             vec4(translation, 0.f, 1.f)};
    return Parent ? result * Parent->InverseModelTransform() : result;
}

mat4 Transform<2>::CameraTransform() const noexcept
{
    const mat2 rotation = Trigonometry<2>(-Rotation).RotationMatrix();
    const vec2 translation = (Origin - rotation * Position) / Scale;

    const mat4 result = mat4{vec4(rotation[0] / Scale, 0.f, 0.f), vec4(rotation[1] / Scale, 0.f, 0.f),
                             vec4(0.f, 0.f, 1.f, 0.f), vec4(translation, 0.f, 1.f)};
    return Parent ? result * Parent->CameraTransform() : result;
}

mat4 Transform<2>::InverseCameraTransform() const noexcept
{
    const mat2 rotation = Trigonometry<2>(-Rotation).RotationMatrix();
    const vec2 translation = Position - rotation * Origin;

    const mat4 result = mat4{vec4(rotation[0] * Scale.x, 0.f, 0.f), vec4(rotation[1] * Scale.y, 0.f, 0.f),
                             vec4(0.f, 0.f, 1.f, 0.f), vec4(translation, 0.f, 1.f)};
    return Parent ? Parent->InverseCameraTransform() * result : result;
}

vec2 Transform<2>::LocalOffset(const vec2 &p_Offset) const noexcept
{
    return Trigonometry<2>(Rotation).RotationMatrix() * p_Offset;
}

vec2 Transform<2>::LocalOffsetX(const f32 p_Offset) const noexcept
{
    const auto [c, s] = Trigonometry<2>(Rotation);
    return vec2(c * p_Offset, s * p_Offset);
}

vec2 Transform<2>::LocalOffsetY(const f32 p_Offset) const noexcept
{
    const auto [c, s] = Trigonometry<2>(Rotation);
    return vec2(-s * p_Offset, c * p_Offset);
}

mat4 Transform<3>::ModelTransform() const noexcept
{
    const mat3 scale = mat3({Scale.x, 0.f, 0.f}, {0.f, Scale.y, 0.f}, {0.f, 0.f, Scale.z});
    const mat3 linear = glm::toMat3(Rotation) * scale;
    const vec3 translation = Position - linear * Origin;

    const mat4 result = mat4{vec4(linear[0], 0.f), vec4(linear[1], 0.f), vec4(linear[2], 0.f), vec4(translation, 1.f)};
    return Parent ? Parent->ModelTransform() * result : result;
}

mat4 Transform<3>::InverseModelTransform() const noexcept
{
    const mat3 scale = mat3({1.f / Scale.x, 0.f, 0.f}, {0.f, 1.f / Scale.y, 0.f}, {0.f, 0.f, 1.f / Scale.z});
    const mat3 linear = scale * glm::toMat3(glm::conjugate(Rotation));
    const vec3 translation = Origin - linear * Position;

    const mat4 result = mat4{vec4(linear[0], 0.f), vec4(linear[1], 0.f), vec4(linear[2], 0.f), vec4(translation, 1.f)};
    return Parent ? result * Parent->InverseModelTransform() : result;
}

mat4 Transform<3>::CameraTransform() const noexcept
{
    const mat3 rotation = glm::toMat3(glm::conjugate(Rotation));
    const vec3 translation = (Origin - rotation * Position) / Scale;
    const mat4 result = mat4{vec4(rotation[0] / Scale, 0.f), vec4(rotation[1] / Scale, 0.f),
                             vec4(rotation[2] / Scale, 0.f), vec4(translation, 1.f)};
    return Parent ? result * Parent->CameraTransform() : result;
}

mat4 Transform<3>::InverseCameraTransform() const noexcept
{
    const mat3 rotation = glm::toMat3(Rotation);
    const vec3 translation = Position - rotation * Origin;
    const mat4 result = mat4{vec4(rotation[0] * Scale.x, 0.f), vec4(rotation[1] * Scale.y, 0.f),
                             vec4(rotation[2] * Scale.z, 0.f), vec4(translation, 1.f)};
    return Parent ? Parent->InverseCameraTransform() * result : result;
}

vec3 Transform<3>::LocalOffset(const vec3 &p_Offset) const noexcept
{
    return Rotation * p_Offset;
}

vec3 Transform<3>::LocalOffsetX(const f32 p_Offset) const noexcept
{
    return LocalOffset(vec3{p_Offset, 0.f, 0.f});
}

vec3 Transform<3>::LocalOffsetY(const f32 p_Offset) const noexcept
{
    return LocalOffset(vec3{0.f, p_Offset, 0.f});
}

vec3 Transform<3>::LocalOffsetZ(const f32 p_Offset) const noexcept
{
    return LocalOffset(vec3{0.f, 0.f, p_Offset});
}

void Transform<3>::RotateLocal(const quat &p_Rotation) noexcept
{
    Rotation = glm::normalize(Rotation * p_Rotation);
}
void Transform<3>::RotateLocal(const vec3 &p_Angles) noexcept
{
    RotateLocal(glm::quat(p_Angles));
}
void Transform<3>::RotateLocalX(const f32 p_Angle) noexcept
{
    RotateLocal(glm::angleAxis(p_Angle, vec3{1.f, 0.f, 0.f}));
}
void Transform<3>::RotateLocalY(const f32 p_Angle) noexcept
{
    RotateLocal(glm::angleAxis(p_Angle, vec3{0.f, 1.f, 0.f}));
}
void Transform<3>::RotateLocalZ(const f32 p_Angle) noexcept
{
    RotateLocal(glm::angleAxis(p_Angle, vec3{0.f, 0.f, 1.f}));
}

void Transform<3>::RotateGlobal(const quat &p_Rotation) noexcept
{
    Rotation = glm::normalize(p_Rotation * Rotation);
}
void Transform<3>::RotateGlobal(const vec3 &p_Angles) noexcept
{
    RotateGlobal(glm::quat(p_Angles));
}
void Transform<3>::RotateGlobalX(const f32 p_Angle) noexcept
{
    RotateGlobal(glm::angleAxis(p_Angle, vec3{1.f, 0.f, 0.f}));
}
void Transform<3>::RotateGlobalY(const f32 p_Angle) noexcept
{
    RotateGlobal(glm::angleAxis(p_Angle, vec3{0.f, 1.f, 0.f}));
}
void Transform<3>::RotateGlobalZ(const f32 p_Angle) noexcept
{
    RotateGlobal(glm::angleAxis(p_Angle, vec3{0.f, 0.f, 1.f}));
}

} // namespace ONYX
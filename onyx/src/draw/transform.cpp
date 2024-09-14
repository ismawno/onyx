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

vec2 Transform<2>::LocalOffsetX(f32 p_Offset) const noexcept
{
    const auto [c, s] = Trigonometry<2>(Rotation);
    return vec2(c * p_Offset, s * p_Offset);
}

vec2 Transform<2>::LocalOffsetY(f32 p_Offset) const noexcept
{
    const auto [c, s] = Trigonometry<2>(Rotation);
    return vec2(-s * p_Offset, c * p_Offset);
}

mat4 Transform<3>::ModelTransform() const noexcept
{
    const mat3 scale = mat3({Scale.x, 0.f, 0.f}, {0.f, Scale.y, 0.f}, {0.f, 0.f, Scale.z});
    const mat3 linear = Rotation * scale;
    const vec3 translation = Position - linear * Origin;

    const mat4 result = mat4{vec4(linear[0], 0.f), vec4(linear[1], 0.f), vec4(linear[2], 0.f), vec4(translation, 1.f)};
    return Parent ? Parent->ModelTransform() * result : result;
}

mat4 Transform<3>::InverseModelTransform() const noexcept
{
    const mat3 scale = mat3({1.f / Scale.x, 0.f, 0.f}, {0.f, 1.f / Scale.y, 0.f}, {0.f, 0.f, 1.f / Scale.z});
    const mat3 linear = scale * InverseRotation();
    const vec3 translation = Origin - linear * Position;

    const mat4 result = mat4{vec4(linear[0], 0.f), vec4(linear[1], 0.f), vec4(linear[2], 0.f), vec4(translation, 1.f)};
    return Parent ? result * Parent->InverseModelTransform() : result;
}

mat4 Transform<3>::CameraTransform() const noexcept
{
    const mat3 rotation = InverseRotation();
    const vec3 translation = (Origin - rotation * Position) / Scale;
    const mat4 result = mat4{vec4(rotation[0] / Scale, 0.f), vec4(rotation[1] / Scale, 0.f),
                             vec4(rotation[2] / Scale, 0.f), vec4(translation, 1.f)};
    return Parent ? result * Parent->CameraTransform() : result;
}

mat4 Transform<3>::InverseCameraTransform() const noexcept
{
    const vec3 translation = Position - Rotation * Origin;
    const mat4 result = mat4{vec4(Rotation[0] * Scale.x, 0.f), vec4(Rotation[1] * Scale.y, 0.f),
                             vec4(Rotation[2] * Scale.z, 0.f), vec4(translation, 1.f)};
    return Parent ? Parent->InverseCameraTransform() * result : result;
}

vec3 Transform<3>::LocalOffset(const vec3 &p_Offset) const noexcept
{
    return Rotation * p_Offset;
}

vec3 Transform<3>::LocalOffsetX(f32 p_Offset) const noexcept
{
    return Rotation[0] * p_Offset;
}

vec3 Transform<3>::LocalOffsetY(f32 p_Offset) const noexcept
{
    return Rotation[1] * p_Offset;
}

vec3 Transform<3>::LocalOffsetZ(f32 p_Offset) const noexcept
{
    return Rotation[2] * p_Offset;
}

void Transform<3>::RotateLocal(const mat3 &p_Rotation) noexcept
{
    Rotation *= p_Rotation;
}

void Transform<3>::RotateGlobal(const mat3 &p_Rotation) noexcept
{
    Rotation = p_Rotation * Rotation;
}

mat3 Transform<3>::InverseRotation() const noexcept
{
    return glm::transpose(Rotation);
}

mat3 Transform<3>::RotXYZ(const vec3 &p_Rotation) noexcept
{
    const auto [c1, s1, c2, s2, c3, s3] = Trigonometry<3>(p_Rotation);
    return {{(c2 * c3), (c1 * s3 + c3 * s1 * s2), (s1 * s3 - c1 * c3 * s2)},
            {(-c2 * s3), (c1 * c3 + s1 * s2 * s3), (c3 * s1 + c1 * s2 * s3)},
            {(s2), (-c2 * s1), (c1 * c2)}};
}

mat3 Transform<3>::RotXZY(const vec3 &p_Rotation) noexcept
{
    const auto [c1, s1, c3, s3, c2, s2] = Trigonometry<3>(p_Rotation);
    return {{(c2 * c3), (s1 * s3 + c1 * c3 * s2), (c3 * s1 * s2 - c1 * s3)},
            {(-s2), (c1 * c2), (c2 * s1)},
            {(c2 * s3), (c1 * s2 * s3 - c3 * s1), (c1 * c3 + s1 * s2 * s3)}};
}

mat3 Transform<3>::RotYXZ(const vec3 &p_Rotation) noexcept
{
    const auto [c2, s2, c1, s1, c3, s3] = Trigonometry<3>(p_Rotation);
    return {{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)},
            {(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)},
            {(c2 * s1), (-s2), (c1 * c2)}};
}

mat3 Transform<3>::RotYZX(const vec3 &p_Rotation) noexcept
{
    const auto [c3, s3, c1, s1, c2, s2] = Trigonometry<3>(p_Rotation);
    return {{(c1 * c2), (s2), (-c2 * s1)},
            {(s1 * s3 - c1 * c3 * s2), (c2 * c3), (c1 * s3 + c3 * s1 * s2)},
            {(c3 * s1 + c1 * s2 * s3), (-c2 * s3), (c1 * c3 - s1 * s2 * s3)}};
}

mat3 Transform<3>::RotZXY(const vec3 &p_Rotation) noexcept
{
    const auto [c2, s2, c3, s3, c1, s1] = Trigonometry<3>(p_Rotation);
    return {{(c1 * c3 - s1 * s2 * s3), (c3 * s1 + c1 * s2 * s3), (-c2 * s3)},
            {(-c2 * s1), (c1 * c2), (s2)},
            {(c1 * s3 + c3 * s1 * s2), (s1 * s3 - c1 * c3 * s2), (c2 * c3)}};
}

mat3 Transform<3>::RotZYX(const vec3 &p_Rotation) noexcept
{
    const auto [c3, s3, c2, s2, c1, s1] = Trigonometry<3>(p_Rotation);
    return {{(c1 * c2), (c2 * s1), (-s2)},
            {(c1 * s2 * s3 - c3 * s1), (c1 * c3 + s1 * s2 * s3), (c2 * s3)},
            {(s1 * s3 + c1 * c3 * s2), (c3 * s1 * s2 - c1 * s3), (c2 * c3)}};
}

mat3 Transform<3>::RotXY(const f32 p_RotX, const f32 p_RotY) noexcept
{
    const auto [c1, s1] = Trigonometry<2>(p_RotX);
    const auto [c2, s2] = Trigonometry<2>(p_RotY);
    return {{c2, s1 * s2, -c1 * s2}, {0.f, c1, s1}, {s2, -c2 * s1, c1 * c2}};
}

mat3 Transform<3>::RotXZ(const f32 p_RotX, const f32 p_RotZ) noexcept
{
    const auto [c1, s1] = Trigonometry<2>(p_RotX);
    const auto [c3, s3] = Trigonometry<2>(p_RotZ);
    return {{c3, c1 * s3, s1 * s3}, {-s3, c1 * c3, c3 * s1}, {0.f, -s1, c1}};
}

mat3 Transform<3>::RotYX(const f32 p_RotY, const f32 p_RotX) noexcept
{
    const auto [c1, s1] = Trigonometry<2>(p_RotX);
    const auto [c2, s2] = Trigonometry<2>(p_RotY);
    return {{c2, 0.f, -s2}, {s1 * s2, c1, c2 * s1}, {c1 * s2, -s1, c1 * c2}};
}

mat3 Transform<3>::RotYZ(f32 p_RotY, f32 p_RotZ) noexcept
{
    const auto [c2, s2] = Trigonometry<2>(p_RotY);
    const auto [c3, s3] = Trigonometry<2>(p_RotZ);
    return {{c2 * c3, s3, -c3 * s2}, {-c2 * s3, c3, s2 * s3}, {s2, 0.f, c2}};
}

mat3 Transform<3>::RotZX(const f32 p_RotZ, const f32 p_RotX) noexcept
{
    const auto [c1, s1] = Trigonometry<2>(p_RotX);
    const auto [c3, s3] = Trigonometry<2>(p_RotZ);
    return {{c3, s3, 0.f}, {-c1 * s3, c1 * c3, s1}, {s1 * s3, -c3 * s1, c1}};
}

mat3 Transform<3>::RotZY(f32 p_RotZ, f32 p_RotY) noexcept
{
    const auto [c2, s2] = Trigonometry<2>(p_RotY);
    const auto [c3, s3] = Trigonometry<2>(p_RotZ);
    return {{c2 * c3, c2 * s3, -s2}, {-s3, c3, 0.f}, {c3 * s2, s2 * s3, c2}};
}

mat3 Transform<3>::RotX(const f32 p_RotX) noexcept
{
    const auto [c, s] = Trigonometry<2>(p_RotX);
    return {{1.f, 0.f, 0.f}, {0.f, c, s}, {0.f, -s, c}};
}

mat3 Transform<3>::RotY(const f32 p_RotY) noexcept
{
    const auto [c, s] = Trigonometry<2>(p_RotY);
    return {{c, 0.f, -s}, {0.f, 1.f, 0.f}, {s, 0.f, c}};
}

mat3 Transform<3>::RotZ(const f32 p_RotZ) noexcept
{
    const auto [c, s] = Trigonometry<2>(p_RotZ);
    return {{c, s, 0.f}, {-s, c, 0.f}, {0.f, 0.f, 1.f}};
}

} // namespace ONYX
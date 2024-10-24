#include "core/pch.hpp"
#include "onyx/draw/transform.hpp"

#include "kit/core/logging.hpp"

namespace ONYX
{
ONYX_DIMENSION_TEMPLATE mat<N - 1> rotationMatrix(const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const f32 c = glm::cos(p_Rotation);
        const f32 s = glm::sin(p_Rotation);
        return mat2{c, s, -s, c};
    }
    else
        return glm::toMat3(p_Rotation);
}

mat4 rotationMatrix4(const quat &p_Quaternion) noexcept
{
    return glm::toMat4(p_Quaternion);
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                              const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({p_Scale.x, 0.f}, {0.f, p_Scale.y});
        const mat2 rotScale = rotationMatrix<N>(p_Rotation) * scale;

        return mat3{vec3(rotScale[0], 0.f), vec3(rotScale[1], 0.f), vec3{p_Translation, 1.f}};
    }
    else
    {
        const mat3 scale = mat3({p_Scale.x, 0.f, 0.f}, {0.f, p_Scale.y, 0.f}, {0.f, 0.f, p_Scale.z});
        const mat3 rotScale = rotationMatrix<N>(p_Rotation) * scale;

        return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4{p_Translation, 1.f}};
    }
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeReverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                                     const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({p_Scale.x, 0.f}, {0.f, p_Scale.y});
        const mat2 rotScale = scale * rotationMatrix<N>(p_Rotation);
        const vec2 translation = rotScale * p_Translation;

        return mat3{vec3(rotScale[0], 0.f), vec3(rotScale[1], 0.f), vec3{translation, 1.f}};
    }
    else
    {
        const mat3 scale = mat3({p_Scale.x, 0.f, 0.f}, {0.f, p_Scale.y, 0.f}, {0.f, 0.f, p_Scale.z});
        const mat3 rotScale = scale * rotationMatrix<N>(p_Rotation);
        const vec3 translation = rotScale * p_Translation;

        return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f), vec4{translation, 1.f}};
    }
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeInverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                                     const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
        return ComputeReverseTransform(-p_Translation, 1.f / p_Scale, -p_Rotation);
    else
        return ComputeReverseTransform(-p_Translation, 1.f / p_Scale, glm::conjugate(p_Rotation));
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTransform() const noexcept
{
    return ComputeTransform(Translation, Scale, Rotation);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeReverseTransform() const noexcept
{
    return ComputeReverseTransform(Translation, Scale, Rotation);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeInverseTransform() const noexcept
{
    return ComputeInverseTransform(Translation, Scale, Rotation);
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationScaleMatrix(const vec<N> &p_Translation,
                                                                           const vec<N> &p_Scale) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(p_Scale.x, 0.f, 0.f), vec3(0.f, p_Scale.y, 0.f), vec3{p_Translation, 1.f}};
    else
        return mat4{vec4(p_Scale.x, 0.f, 0.f, 0.f), vec4(0.f, p_Scale.y, 0.f, 0.f), vec4(0.f, 0.f, p_Scale.z, 0.f),
                    vec4{p_Translation, 1.f}};
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeReverseTranslationScaleMatrix(const vec<N> &p_Translation,
                                                                                  const vec<N> &p_Scale) noexcept
{
    if constexpr (N == 2)
    {
        const vec2 translation = p_Scale * p_Translation;
        return mat3{vec3(p_Scale.x, 0.f, 0.f), vec3(0.f, p_Scale.y, 0.f), vec3{translation, 1.f}};
    }
    else
    {
        const vec3 translation = p_Scale * p_Translation;
        return mat4{vec4(p_Scale.x, 0.f, 0.f, 0.f), vec4(0.f, p_Scale.y, 0.f, 0.f), vec4(0.f, 0.f, p_Scale.z, 0.f),
                    vec4{translation, 1.f}};
    }
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeInverseTranslationScaleMatrix(const vec<N> &p_Translation,
                                                                                  const vec<N> &p_Scale) noexcept
{
    return ComputeReverseTranslationScaleMatrix(-p_Translation, 1.f / p_Scale);
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationScaleMatrix() const noexcept
{
    return ComputeTranslationScaleMatrix(Translation, Scale);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeReverseTranslationScaleMatrix() const noexcept
{
    return ComputeReverseTranslationScaleMatrix(Translation, Scale);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeInverseTranslationScaleMatrix() const noexcept
{
    return ComputeInverseTranslationScaleMatrix(Translation, Scale);
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationMatrix(const vec<N> &p_Translation) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), vec3{p_Translation, 1.f}};
    else
        return mat4{vec4(1.f, 0.f, 0.f, 0.f), vec4(0.f, 1.f, 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                    vec4{p_Translation, 1.f}};
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeScaleMatrix(const vec<N> &p_Scale) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(p_Scale.x, 0.f, 0.f), vec3(0.f, p_Scale.y, 0.f), vec3(0.f, 0.f, 1.f)};
    else
        return mat4{vec4(p_Scale.x, 0.f, 0.f, 0.f), vec4(0.f, p_Scale.y, 0.f, 0.f), vec4(0.f, 0.f, p_Scale.z, 0.f),
                    vec4(0.f, 0.f, 0.f, 1.f)};
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeRotationMatrix(const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 rot = rotationMatrix<N>(p_Rotation);
        return mat3{vec3(rot[0], 0.f), vec3(rot[1], 0.f), vec3(0.f, 0.f, 1.f)};
    }
    else
        return rotationMatrix4(p_Rotation);
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationMatrix() const noexcept
{
    return ComputeTranslationMatrix(Translation);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeScaleMatrix() const noexcept
{
    return ComputeScaleMatrix(Scale);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeRotationMatrix() const noexcept
{
    return ComputeRotationMatrix(Rotation);
}

ONYX_DIMENSION_TEMPLATE void Transform<N>::Extract(const mat<N> &p_Transform, vec<N> *p_Translation, vec<N> *p_Scale,
                                                   rot<N> *p_Rotation) noexcept
{
    *p_Translation = ExtractTranslation(p_Transform);
    *p_Scale = ExtractScale(p_Transform);
    *p_Rotation = ExtractRotation(p_Transform);
}

ONYX_DIMENSION_TEMPLATE Transform<N> Transform<N>::Extract(const mat<N> &p_Transform) noexcept
{
    Transform<N> transform;
    Extract(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

ONYX_DIMENSION_TEMPLATE vec<N> Transform<N>::ExtractTranslation(const mat<N> &p_Transform) noexcept
{
    return vec<N>{p_Transform[N]};
}
ONYX_DIMENSION_TEMPLATE vec<N> Transform<N>::ExtractScale(const mat<N> &p_Transform) noexcept
{
    if constexpr (N == 2)
        return vec2{glm::length(vec2{p_Transform[0]}), glm::length(vec2{p_Transform[1]})};
    else
        return vec3{glm::length(vec3{p_Transform[0]}), glm::length(vec3{p_Transform[1]}),
                    glm::length(vec3{p_Transform[2]})};
}
ONYX_DIMENSION_TEMPLATE rot<N> Transform<N>::ExtractRotation(const mat<N> &p_Transform) noexcept
{
    if constexpr (N == 2)
        return glm::atan(p_Transform[0][1], p_Transform[0][0]);
    else
    {
        vec3 angles;
        angles.x = glm::atan(p_Transform[1][2], p_Transform[2][2]);
        angles.y = glm::atan(-p_Transform[0][2],
                             glm::sqrt(p_Transform[1][2] * p_Transform[1][2] + p_Transform[2][2] * p_Transform[2][2]));
        angles.z = glm::atan(p_Transform[0][1], p_Transform[0][0]);
        return quat{angles};
    }
}

template struct Transform<2>;
template struct Transform<3>;

} // namespace ONYX
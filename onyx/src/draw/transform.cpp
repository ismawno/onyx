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

ONYX_DIMENSION_TEMPLATE static vec<N + 1> adaptTranslation(const vec<N> &p_Translation)
{
#if ONYX_COORDINATE_SYSTEM == ONYX_CS_CENTERED_CARTESIAN
    vec<N + 1> translation{p_Translation, 1.f};
    translation.y = -translation.y;
    return translation;
#else
    return vec<N + 1>{p_Translation, 1.f};
#endif
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                              const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({p_Scale.x, 0.f}, {0.f, p_Scale.y});
        const mat2 rotScale = Trigonometry<2>(p_Rotation).RotationMatrix() * scale;

        return mat3{vec3(rotScale[0], 0.f), vec3(rotScale[1], 0.f), adaptTranslation<N>(p_Translation)};
    }
    else
    {
        const mat3 scale = mat3({p_Scale.x, 0.f, 0.f}, {0.f, p_Scale.y, 0.f}, {0.f, 0.f, p_Scale.z});
        const mat3 rotScale = glm::toMat3(p_Rotation) * scale;

        return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f),
                    adaptTranslation<N>(p_Translation)};
    }
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeInverseTransform(const vec<N> &p_Translation, const vec<N> &p_Scale,
                                                                     const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 scale = mat2({1.f / p_Scale.x, 0.f}, {0.f, 1.f / p_Scale.y});
        const mat2 rotScale = scale * Trigonometry<2>(-p_Rotation).RotationMatrix();
        const vec2 translation = -rotScale * p_Translation;

        return mat3{vec3(rotScale[0], 0.f), vec3(rotScale[1], 0.f), adaptTranslation<N>(translation)};
    }
    else
    {
        const mat3 scale = mat3({1.f / p_Scale.x, 0.f, 0.f}, {0.f, 1.f / p_Scale.y, 0.f}, {0.f, 0.f, 1.f / p_Scale.z});
        const mat3 rotScale = scale * glm::toMat3(glm::conjugate(p_Rotation));
        const vec3 translation = -rotScale * p_Translation;

        return mat4{vec4(rotScale[0], 0.f), vec4(rotScale[1], 0.f), vec4(rotScale[2], 0.f),
                    adaptTranslation<N>(translation)};
    }
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTransform() const noexcept
{
    return ComputeTransform(Translation, Scale, Rotation);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeInverseTransform() const noexcept
{
    return ComputeInverseTransform(Translation, Scale, Rotation);
}

ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationMatrix(const vec<N> &p_Translation) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), adaptTranslation<N>(p_Translation)};
    else
        return mat4{vec4(1.f, 0.f, 0.f, 0.f), vec4(0.f, 1.f, 0.f, 0.f), vec4(0.f, 0.f, 1.f, 0.f),
                    adaptTranslation<N>(p_Translation)};
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeScaleMatrix(const vec<N> &p_Scale) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(p_Scale.x, 0.f, 0.f), vec3(0.f, p_Scale.y, 0.f), vec3(0.f, 0.f, 1.f)};
    else
        return mat4{vec4(p_Scale.x, 0.f, 0.f, 0.f), vec4(0.f, p_Scale.y, 0.f, 0.f), vec4(0.f, 0.f, p_Scale.z, 0.f),
                    vec4(0.f, 0.f, 0.f, 1.f)};
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeScaleMatrix(f32 p_Scale) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(p_Scale, 0.f, 0.f), vec3(0.f, p_Scale, 0.f), vec3(0.f, 0.f, 1.f)};
    else
        return mat4{vec4(p_Scale, 0.f, 0.f, 0.f), vec4(0.f, p_Scale, 0.f, 0.f), vec4(0.f, 0.f, p_Scale, 0.f),
                    vec4(0.f, 0.f, 0.f, 1.f)};
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeRotationMatrix(const rot<N> &p_Rotation) noexcept
{
    if constexpr (N == 2)
    {
        const mat2 rot = Trigonometry<2>(p_Rotation).RotationMatrix();
        return mat3{vec3(rot[0], 0.f), vec3(rot[1], 0.f), vec3(0.f, 0.f, 1.f)};
    }
    else
        return glm::toMat4(p_Rotation);
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationScaleMatrix(const vec<N> &p_Translation,
                                                                           const vec<N> &p_Scale) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(p_Scale.x, 0.f, 0.f), vec3(0.f, p_Scale.y, 0.f), adaptTranslation<N>(p_Translation)};
    else
        return mat4{vec4(p_Scale.x, 0.f, 0.f, 0.f), vec4(0.f, p_Scale.y, 0.f, 0.f), vec4(0.f, 0.f, p_Scale.z, 0.f),
                    adaptTranslation<N>(p_Translation)};
}
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationScaleMatrix(const vec<N> &p_Translation,
                                                                           const f32 p_Scale) noexcept
{
    if constexpr (N == 2)
        return mat3{vec3(p_Scale, 0.f, 0.f), vec3(0.f, p_Scale, 0.f), adaptTranslation<N>(p_Translation)};
    else
        return mat4{vec4(p_Scale, 0.f, 0.f, 0.f), vec4(0.f, p_Scale, 0.f, 0.f), vec4(0.f, 0.f, p_Scale, 0.f),
                    adaptTranslation<N>(p_Translation)};
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
ONYX_DIMENSION_TEMPLATE mat<N> Transform<N>::ComputeTranslationScaleMatrix() const noexcept
{
    return ComputeTranslationScaleMatrix(Translation, Scale);
}

ONYX_DIMENSION_TEMPLATE void Transform<N>::ExtractTransform(const mat<N> &p_Transform, vec<N> *p_Translation,
                                                            vec<N> *p_Scale, rot<N> *p_Rotation) noexcept
{
    *p_Translation = ExtractTranslationTransform(p_Transform);
    *p_Scale = ExtractScaleTransform(p_Transform);
    *p_Rotation = ExtractRotationTransform(p_Transform);
}

ONYX_DIMENSION_TEMPLATE Transform<N> Transform<N>::ExtractTransform(const mat<N> &p_Transform) noexcept
{
    Transform<N> transform;
    ExtractTransform(p_Transform, &transform.Translation, &transform.Scale, &transform.Rotation);
    return transform;
}

ONYX_DIMENSION_TEMPLATE vec<N> Transform<N>::ExtractTranslationTransform(const mat<N> &p_Transform) noexcept
{
#if ONYX_COORDINATE_SYSTEM == ONYX_CS_CENTERED_CARTESIAN
    vec<N> translation{p_Transform[N]};
    translation.y = -translation.y;
    return translation;
#else
    return vec<N>{p_Transform[N]};
#endif
}
ONYX_DIMENSION_TEMPLATE vec<N> Transform<N>::ExtractScaleTransform(const mat<N> &p_Transform) noexcept
{
    if constexpr (N == 2)
        return vec2{glm::length(vec2{p_Transform[0]}), glm::length(vec2{p_Transform[1]})};
    else
        return vec3{glm::length(vec3{p_Transform[0]}), glm::length(vec3{p_Transform[1]}),
                    glm::length(vec3{p_Transform[2]})};
}
ONYX_DIMENSION_TEMPLATE rot<N> Transform<N>::ExtractRotationTransform(const mat<N> &p_Transform) noexcept
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